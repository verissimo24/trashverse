#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <zmq.h>   
#include <SDL2/SDL_timer.h>
#include <pthread.h>

#include "universe-data.h"  
#include "display.h"
#include "comm.h"     
#include "physics-rules.h"

static Universe universe;
static Display display;

static pthread_mutex_t universe_mtx = PTHREAD_MUTEX_INITIALIZER;  //protect the access to the universe

//global flags
static int quit = 0;              
static int game_over = 0;   

//Thread that applies physics laws periodically(10ms)
static void* physics_thread() 
{
    while (1) {
        SDL_Delay(10); //10ms period for physics update

        pthread_mutex_lock(&universe_mtx);
        if (quit) { pthread_mutex_unlock(&universe_mtx); break; }

        physics_step(&universe);
        universe_update_ship_interactions(&universe);

        //checks for universe ending condition
        if (universe.num_trash >= universe.max_trash) {
            game_over = 1;
            quit = 1;
        }
        pthread_mutex_unlock(&universe_mtx);
    }
    return NULL;
}

//Thread that will handle client connection/move messages
void* reqrep_thread() 
{
    // ZMQ server channel 
    zmqChannel channel = create_server_channel(universe.reqrep_port);
    if (!channel.sock) {
        fprintf(stderr, "Failed to create server channel\n");
        pthread_mutex_lock(&universe_mtx);
        quit = 1;
        pthread_mutex_unlock(&universe_mtx);
        return NULL;
    }

    while (1) {

        char message_type[32];
        char client_id = '?';
        direction_t dir = 0;
        uint32_t token = 0;

        read_message(channel.sock, message_type, &client_id, &dir, &token);
        if (message_type[0] == '\0') {
            // (timeout), checks if a different thread closed
            pthread_mutex_lock(&universe_mtx);
            int q = quit;
            pthread_mutex_unlock(&universe_mtx);
            if (q) break;
            continue;
        }

        pthread_mutex_lock(&universe_mtx);
        if (quit) { pthread_mutex_unlock(&universe_mtx); break; }

        // 1) CONNECT message
        if (strcmp(message_type, "CONNECT") == 0) {

            //Choose free slot
            int free_idx = -1;

            if (!universe.ships) {
                free_idx = 0; 
            } else {
                for (int i = 0; i < MAX_SHIPS; ++i) {
                    if (!universe.ships[i].active) { free_idx = i; break; }
                }
            }

            if (free_idx < 0) {
                printf("SERVER: Reject CONNECT (no free ships)\n");
                pthread_mutex_unlock(&universe_mtx);
                send_response(channel.sock, "ERROR");
                continue;
            }

            client_id = (char)('A' + free_idx);

            uint32_t new_token = (uint32_t)rand();
            if (new_token == 0) new_token = 1;   //leave the 0 to identify unitialized tokens

            int idx = universe_add_ship(&universe, client_id, new_token);
            if (idx < 0) {
                pthread_mutex_unlock(&universe_mtx);
                send_response(channel.sock, "ERROR");
                continue;
            }

            pthread_mutex_unlock(&universe_mtx);
            send_connect_response(channel.sock, client_id, new_token);
            continue;
        }

        if (strcmp(message_type, "QUIT") == 0) {

            if (!universe.ships) {
                pthread_mutex_unlock(&universe_mtx);
                send_response(channel.sock, "ERROR");
                continue;
            }

            int ok = (universe_remove_ship(&universe, client_id, token) == 0);

            pthread_mutex_unlock(&universe_mtx);
            send_response(channel.sock, ok ? "OK" : "ERROR");
            continue;
        }


        // MOVE message
        if (strcmp(message_type, "MOVE") == 0) {

            if (!universe.ships) {
                pthread_mutex_unlock(&universe_mtx);
                send_response(channel.sock, "ERROR");
                continue;
            }

            int authorized = 0;

            for (int i = 0; i < MAX_SHIPS; ++i) {
                Ship* s = &universe.ships[i];
                if ((s->ship_id == client_id) && (s->token == token)) {  //validate client id and token
                    universe_move_ship(&universe, client_id, dir);
                    authorized = 1;
                    break;
                }
            }

            pthread_mutex_unlock(&universe_mtx);
            send_response(channel.sock, authorized ? "OK" : "ERROR");
            continue;
        }

        pthread_mutex_unlock(&universe_mtx);
        send_response(channel.sock, "ERROR");
    }

    destroy_channel(&channel);
    return NULL;
}

//Thread that broadcasts the universe state/stats to the clients/dashboard
static void* pub_thread()
{
    zmqChannel pub = create_server_pub_channel(universe.pub_port);

    if (!pub.sock) {
        fprintf(stderr, "Failed to create PUB channel\n");
        pthread_mutex_lock(&universe_mtx);
        quit = 1;
        pthread_mutex_unlock(&universe_mtx);
        return NULL;
    }

    fprintf(stdout, "PUB thread started on port %d\n", universe.pub_port);

    while (1) {
        SDL_Delay(33);  //30 updates/s

        pthread_mutex_lock(&universe_mtx);
        int local_quit = quit;
        int local_go   = game_over;

        //sends universe state to the clients
        comm_pub_send_universe_state(pub.sock, &universe, local_go);

        //sends universe stats to the dashboard
        comm_pub_send_universe_stats(pub.sock, &universe, local_go);

        pthread_mutex_unlock(&universe_mtx);

        if (local_quit) {
            //To guarantee that the client is notified about the universe ending
            for (int i = 0; i < 5; ++i) {
                SDL_Delay(50);
                pthread_mutex_lock(&universe_mtx);
                comm_pub_send_universe_state(pub.sock, &universe, 1);
                pthread_mutex_unlock(&universe_mtx);
            }
            break;
        }   
    }

    destroy_channel(&pub);
    return NULL;
}

enum {     //Isto eventualmente vai sair e vou fazer 3 threads extra para dar handle disto.
    UE_TRASH_GEN = 2,       
    UE_RECYCLE_CHANGE = 3,
    UE_DRAW = 4,
};

static Uint32 timer_callback(Uint32 interval, void *param)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_USEREVENT;
    ev.user.code = (int)(intptr_t)param;   
    ev.user.data1 = NULL;
    ev.user.data2 = NULL;
    SDL_PushEvent(&ev);
    return interval; 
}

int main(void)
{
    //seed rand using current time
    srand((unsigned int)time(NULL));   

    // Load universe specifications from config file
    if (universe_load_from_config(&universe, "server.cfg") != 0) {
        fprintf(stderr, "Failed to load universe configuration\n");
        return 1;
    }

    universe_create_planets(&universe);   
    universe_create_trash(&universe);

    // Initialize universe SDL2 window
    if (display_universe_init(&display, &universe) != 0) {
        fprintf(stderr, "Failed to initialize display\n");
        universe_destroy(&universe);
        return 1;
    }

    SDL_TimerID trash_timer   = SDL_AddTimer(10000, timer_callback, (void*)(intptr_t)UE_TRASH_GEN);
    SDL_TimerID recycle_timer = SDL_AddTimer(30000, timer_callback, (void*)(intptr_t)UE_RECYCLE_CHANGE);
    SDL_TimerID draw_timer = SDL_AddTimer(33, timer_callback, (void*)(intptr_t)UE_DRAW);
    
    pthread_t thread_id_step;
    pthread_create(&thread_id_step, NULL, physics_thread, NULL);
    
    pthread_t thread_id_rep;
    pthread_create(&thread_id_rep, NULL, reqrep_thread, NULL);

    pthread_t thread_id_pub;
    pthread_create(&thread_id_pub, NULL, pub_thread, NULL);

    // +3 threads em vez dos timers

    SDL_Delay(500);  
    fprintf(stderr, "PUB socket should be ready now\n");

    int need_draw = 1;

    while (1) {
    
        pthread_mutex_lock(&universe_mtx);
        int local_quit = quit;
        pthread_mutex_unlock(&universe_mtx);
        if (local_quit) break;

        // 1) Handle SDL events (window + our timer events)
        SDL_Event e;
        SDL_WaitEvent(&e);
        do{
            switch (e.type) {

                case SDL_QUIT:
                    pthread_mutex_lock(&universe_mtx);
                    quit = 1;
                    pthread_mutex_unlock(&universe_mtx);
                    break;

                case SDL_USEREVENT:
                    if (e.user.code == UE_TRASH_GEN) {
                        // 10s: periodic trash generation (only if ships exist)
                        pthread_mutex_lock(&universe_mtx);
                        if (universe.num_ships > 0) {
                            universe_add_trash(&universe);
                        }
                        pthread_mutex_unlock(&universe_mtx);
                    }
                    else if (e.user.code == UE_RECYCLE_CHANGE) {
                        // 30s: change recycling planet
                        pthread_mutex_lock(&universe_mtx);
                        universe_change_recycling_planet(&universe);
                        pthread_mutex_unlock(&universe_mtx);
                    }
                    else if (e.user.code == UE_DRAW) {
                        // 30Hz:draw
                        need_draw = 1;
                        //display_draw_universe(&display, &universe);
                    }
                    break;

                default:
                    break;
            } 
        }while (SDL_PollEvent(&e));

        //5) Draw only when the 30Hz draw timer allows it
        if (need_draw) {
            pthread_mutex_lock(&universe_mtx);
            display_draw_universe(&display, &universe);
            pthread_mutex_unlock(&universe_mtx);
            need_draw = 0;
        }

        // Small sleep to avoid 100% CPU spinning
        SDL_Delay(1);
    }

    // End of the world show game over
    pthread_mutex_lock(&universe_mtx);
    int go = game_over;
    pthread_mutex_unlock(&universe_mtx);

    if (go) {
        pthread_mutex_lock(&universe_mtx);
        display_show_game_over(&display, &universe);
        pthread_mutex_unlock(&universe_mtx);

        SDL_Delay(2000); 
    }


    pthread_join(thread_id_rep, NULL);
    pthread_join(thread_id_pub, NULL);
    pthread_join(thread_id_step, NULL);
    

    //destroy_channel(&channel);
    display_shutdown(&display);
    universe_destroy(&universe);

    SDL_RemoveTimer(trash_timer);
    SDL_RemoveTimer(recycle_timer);
    SDL_RemoveTimer(draw_timer);

    
    return 0;
}


