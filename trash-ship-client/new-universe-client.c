#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <SDL2/SDL.h>

#include "display.h"
#include "comm.h"
#include "space_trash.pb-c.h"  // UniverseState, universe_state__free_unpacked, ...

// ----- Shared "view" universe updated by SUB thread -----
static Universe view_universe;
static pthread_mutex_t view_mtx = PTHREAD_MUTEX_INITIALIZER;
static int client_quit = 0;

// Custom SDL user event codes
enum {
    UE_DRAW        = 10,
    UE_NEW_STATE   = 11,
    UE_SERVER_DOWN = 12,
    UE_GAME_OVER   = 13
};

static Uint32 timer_callback(Uint32 interval, void *param)
{
    SDL_Event ev;
    SDL_zero(ev);
    ev.type = SDL_USEREVENT;
    ev.user.code = (int)(intptr_t)param;
    SDL_PushEvent(&ev);
    return interval;
}

static void ensure_view_alloc(Universe *u, int num_planets, int max_trash)
{
    if (!u) return;

    //realloc if necessary 

    // planets
    if (num_planets > 0 && (u->planets == NULL || u->num_planets != num_planets)) {
        free(u->planets);
        u->planets = calloc((size_t)num_planets, sizeof(Planet));
        u->num_planets = num_planets;
    }

    // trash
    if (max_trash > 0 && (u->trash == NULL || u->max_trash != max_trash)) {
        free(u->trash);
        u->trash = calloc((size_t)max_trash, sizeof(Trash));
        u->max_trash = max_trash;
    }

    // ships (fixed MAX_SHIPS)
    if (u->ships == NULL) {
        u->ships = calloc((size_t)MAX_SHIPS, sizeof(Ship));
    }
}

static void apply_state_to_view(Universe *u, const UniverseState *st)
{
    if (!u || !st) return;

    u->width  = (int)st->width;
    u->height = (int)st->height;

    ensure_view_alloc(u, (int)st->n_planets, (int)st->max_trash);

    u->recycling_planet = (int)st->recycling_planet;
    u->num_trash = (int)st->n_trash;

    // ----- planets -----
    if (u->planets && st->n_planets > 0) {
        for (size_t i = 0; i < st->n_planets; ++i) {
            const PlanetState *ps = st->planets[i];
            Planet *p = &u->planets[i];

            p->position.x = ps->x;
            p->position.y = ps->y;
            p->radius = 20.0f;          
            p->mass   = 100.0f;        
            p->planet_name = (char)ps->name_ascii;
            p->recycle = ps->recycle ? true : false;
            p->trash_count = (int)ps->trash_count;
        }
    }

    // ----- trash -----
    if (u->trash) {
        for (int i = 0; i < u->max_trash; ++i) u->trash[i].active = 0;

        size_t k = 0;
        for (size_t i = 0; i < st->n_trash && k < (size_t)u->max_trash; ++i, ++k) {
            const TrashState *ts = st->trash[i];
            u->trash[k].active = 1;
            u->trash[k].position.x = ts->x;
            u->trash[k].position.y = ts->y;
        }
    }

    // ----- ships -----
    if (u->ships) {
        for (int i = 0; i < MAX_SHIPS; ++i) {
            u->ships[i].active = 0;
            u->ships[i].ship_id = (char)('A' + i);
            u->ships[i].radius = 8.0f;
            u->ships[i].cargo = 0;
            u->ships[i].velocity.amplitude = 0.0f;
            u->ships[i].velocity.angle     = 0.0f;
        }

        for (size_t i = 0; i < st->n_ships; ++i) {
            const ShipState *ss = st->ships[i];
            int idx = (int)((char)ss->ship_ascii - 'A');
            if (idx < 0 || idx >= MAX_SHIPS) continue;

            Ship *s = &u->ships[idx];
            s->ship_id = (char)ss->ship_ascii;
            s->active  = ss->active ? 1 : 0;
            s->position.x       = ss->x;
            s->position.y       = ss->y;
            s->radius  = 8.0f;
            s->cargo   = (int)ss->cargo;
            s->velocity.amplitude = ss->v_amplitude;
            s->velocity.angle     = ss->v_angle;
        }
    }

    int active = 0;
    for (int i = 0; i < MAX_SHIPS; ++i) if (u->ships[i].active) active++;
    u->num_ships = active;
}

typedef struct {
    zmqChannel sub;
} SubArgs;

static void* sub_thread(void *arg)
{
    SubArgs *a = (SubArgs*)arg;
    if (!a) return NULL;

    int timeouts = 0;

    while (!client_quit) {
        UniverseState *st = comm_sub_recv_universe_state(a->sub.sock);
        if (!st) {
            timeouts++;
            if (timeouts >= 3) {
                SDL_Event ev;
                SDL_zero(ev);
                ev.type = SDL_USEREVENT;
                ev.user.code = UE_SERVER_DOWN;
                SDL_PushEvent(&ev);
                break;
            }
            continue;
        }
        timeouts = 0;

        pthread_mutex_lock(&view_mtx);
        apply_state_to_view(&view_universe, st);
        pthread_mutex_unlock(&view_mtx);


        if (st->game_over) {
            SDL_Event ev;
            SDL_zero(ev);
            ev.type = SDL_USEREVENT;
            ev.user.code = UE_GAME_OVER;
            SDL_PushEvent(&ev);

            universe_state__free_unpacked(st, NULL);
            break; 
        }

        universe_state__free_unpacked(st, NULL);

        SDL_Event ev;
        SDL_zero(ev);
        ev.type = SDL_USEREVENT;
        ev.user.code = UE_NEW_STATE;
        SDL_PushEvent(&ev);
    }
    return NULL;
}

int main(void)
{
    Display display;

    ClientConfig cfg = {0};
    if (client_load_from_config(&cfg, "client.cfg") != 0) {
        fprintf(stderr, "Client: failed to load client.cfg\n");
        return 1;
    }

    printf("CFG: address='%s' reqrep_port=%d pub_port=%d w=%d h=%d\n",
       cfg.server_address, cfg.reqrep_port, cfg.pub_port, cfg.width, cfg.height);

    view_universe.width  = cfg.width;
    view_universe.height = cfg.height;

    ensure_view_alloc(&view_universe, 0, 0);

    if (display_universe_init(&display, &view_universe) != 0) {
        fprintf(stderr, "Failed to initialize universe display\n");
        universe_destroy(&view_universe);
        return 1;
    }

    // ---- REQ channel (commands) ----
    zmqChannel req = create_client_channel(cfg.server_address, cfg.reqrep_port);
    if (!req.sock) {
        fprintf(stderr, "Client: failed to create REQ channel\n");
        display_shutdown(&display);
        universe_destroy(&view_universe);
        return 1;
    }

    // CONNECT
    send_connection_message(req.sock);
    char my_id = '?';
    uint32_t my_token = 0;
    if (receive_connect_ack(req.sock, &my_id, &my_token) != 0) {
        fprintf(stderr, "Client: failed to CONNECT\n");
        destroy_channel(&req);
        display_shutdown(&display);
        universe_destroy(&view_universe);
        return 1;
    }
    printf("Client connected: ship=%c token=%u\n", my_id, my_token);

    // ---- SUB channel (state updates) ----
    zmqChannel sub = create_client_sub_channel(cfg.server_address, cfg.pub_port, "STATE");
    if (!sub.sock) {
        fprintf(stderr, "Client: failed to create SUB channel\n");
        destroy_channel(&req);
        display_shutdown(&display);
        universe_destroy(&view_universe);
        return 1;
    }

    pthread_t tid_sub;
    SubArgs sub_args = { .sub = sub };
    pthread_create(&tid_sub, NULL, sub_thread, &sub_args);

    // Draw timer @ 30Hz
    SDL_TimerID draw_timer = SDL_AddTimer(33, timer_callback, (void*)(intptr_t)UE_DRAW);
    int need_draw = 1;

    while (!client_quit) {
        SDL_Event e;
        SDL_WaitEvent(&e);
        do {
            if (e.type == SDL_QUIT) {
                client_quit = 1;
            } else if (e.type == SDL_USEREVENT) {
                if (e.user.code == UE_DRAW) {
                    need_draw = 1;
                } else if (e.user.code == UE_SERVER_DOWN) {
                    fprintf(stderr, "Client: server seems down (no PUB updates)\n");
                    client_quit = 1;
                } else if (e.user.code == UE_GAME_OVER) {
                    pthread_mutex_lock(&view_mtx);
                    display_show_game_over(&display, &view_universe);
                    pthread_mutex_unlock(&view_mtx);

                    SDL_Delay(2000); 
                    client_quit = 1;
                }
            } else if (e.type == SDL_KEYDOWN) {
                direction_t dir;
                bool valid = true;

                switch (e.key.keysym.sym) {
                    case SDLK_UP:    dir = UP; break;
                    case SDLK_DOWN:  dir = DOWN; break;
                    case SDLK_LEFT:  dir = LEFT; break;
                    case SDLK_RIGHT: dir = RIGHT; break;
                    default: valid = false; break;
                }

                if (valid) {
                    send_movement_message(req.sock, my_id, dir, my_token);

                    char response[64];
                    receive_response(req.sock, response);
                    if (response[0] == '\0' || strcmp(response, "OK") != 0) {
                        fprintf(stderr, "Client: MOVE rejected or server not responding\n");
                    }
                }
            }
        } while (SDL_PollEvent(&e));

        if (need_draw) {
            pthread_mutex_lock(&view_mtx);
            display_draw_universe(&display, &view_universe);
            pthread_mutex_unlock(&view_mtx);
            need_draw = 0;
        }

        SDL_Delay(1);
    }

    SDL_RemoveTimer(draw_timer);

    pthread_join(tid_sub, NULL);


    if (req.sock && my_id != '?' && my_token != 0) {
        send_quit_message(req.sock, my_id, my_token);
        char response[64];
        receive_response(req.sock, response); 
    }


    destroy_channel(&sub);
    destroy_channel(&req);
    display_shutdown(&display);
    universe_destroy(&view_universe);

    return 0;
}
