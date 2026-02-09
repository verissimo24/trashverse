#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <zmq.h>   
#include <SDL2/SDL_timer.h>
#include <pthread.h>
#include <time.h>

#include "universe-data.h"  
#include "display.h"
#include "comm.h"     
#include "physics-rules.h"

static Universe universe;
static Display display;

static pthread_mutex_t universe_mtx = PTHREAD_MUTEX_INITIALIZER;  //protect the access to the universe
static pthread_cond_t universe_cv = PTHREAD_COND_INITIALIZER;

//global flag
static int quit = 0;                

//Thread that applies physics laws periodically(10ms)
void* physics_thread() 
{
    while (!quit) {
        SDL_Delay(10); //10ms period for physics update

        pthread_mutex_lock(&universe_mtx);
        
        physics_step(&universe);
        universe_update_ship_interactions(&universe);

        //checks for universe ending condition
        if (universe.num_trash >= universe.max_trash) {
            quit = 1;
            pthread_cond_broadcast(&universe_cv);
        }
        pthread_mutex_unlock(&universe_mtx);
    }
    return NULL;
}

//Adds a new trash to the universe every 10 seconds (if there is at least one active ship)
void* trash_thread()
{
    SDL_Delay(10000); //Wait 10 seconds to start

    pthread_mutex_lock(&universe_mtx);

    while (!quit) {

        if (universe.num_ships > 0) {
            universe_add_trash(&universe);
        }

        // calculate time instant +10s
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 10;

        //Waits for 10 seconds
        pthread_cond_timedwait(&universe_cv, &universe_mtx, &ts); 
    }

    pthread_mutex_unlock(&universe_mtx);
    return NULL;
}


//Changes the recycle planet periodically(30s)
void* change_recycle_thread()
{
    SDL_Delay(30000); //Wait 30 seconds to start

    pthread_mutex_lock(&universe_mtx);

    while (!quit) 
    {
        universe_change_recycling_planet(&universe);

        // calculate time instant +30s
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 30;

        //Waits for 30 seconds
        pthread_cond_timedwait(&universe_cv, &universe_mtx, &ts);

    }

    pthread_mutex_unlock(&universe_mtx);
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
        pthread_cond_broadcast(&universe_cv);
        pthread_mutex_unlock(&universe_mtx);
        return NULL;
    }

    while (1) 
    {
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
        pthread_cond_broadcast(&universe_cv);
        pthread_mutex_unlock(&universe_mtx);
        return NULL;
    }

    fprintf(stdout, "PUB thread started on port %d\n", universe.pub_port);

    while (1) {
        SDL_Delay(33);  //30 updates/s

        pthread_mutex_lock(&universe_mtx);
        int local_quit = quit;

        //sends universe state to the clients
        comm_pub_send_universe_state(pub.sock, &universe, local_quit);

        //sends universe stats to the dashboard
        comm_pub_send_universe_stats(pub.sock, &universe, local_quit);

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


    pthread_t thread_id_step;
    pthread_create(&thread_id_step, NULL, physics_thread, NULL);

    pthread_t thread_id_trash;
    pthread_create(&thread_id_trash, NULL, trash_thread, NULL);
    
    pthread_t thread_id_change_recycle;
    pthread_create(&thread_id_change_recycle, NULL, change_recycle_thread, NULL);
    
    pthread_t thread_id_rep;
    pthread_create(&thread_id_rep, NULL, reqrep_thread, NULL);
    
    pthread_t thread_id_pub;
    pthread_create(&thread_id_pub, NULL, pub_thread, NULL);


    SDL_Delay(500);  //To make sure that the sockets are ready


    while (1) {
        pthread_mutex_lock(&universe_mtx);
        int local_quit = quit;
        pthread_mutex_unlock(&universe_mtx);
        if (local_quit) break;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                pthread_mutex_lock(&universe_mtx);
                quit = 1;
                pthread_cond_broadcast(&universe_cv);
                pthread_mutex_unlock(&universe_mtx);
            }
        }

        // desenha a ~30 fps
        pthread_mutex_lock(&universe_mtx);
        display_draw_universe(&display, &universe);
        pthread_mutex_unlock(&universe_mtx);

        SDL_Delay(33);
    }



    pthread_mutex_lock(&universe_mtx);
    display_show_game_over(&display, &universe);
    pthread_mutex_unlock(&universe_mtx);
    
    pthread_join(thread_id_step, NULL);
    pthread_join(thread_id_trash, NULL);
    pthread_join(thread_id_change_recycle, NULL);  
    pthread_join(thread_id_rep, NULL);
    pthread_join(thread_id_pub, NULL);
    

    //destroy_channel(&channel);
    display_shutdown(&display);
    universe_destroy(&universe);
    
    return 0;
}


