#include "universe-data.h"
#include "direction.h"

#include <stdio.h>
#include <stdlib.h>
#include <libconfig.h>
#include <math.h>
#include <string.h>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static Vector make_vector(float x, float y)
{
    Vector v;
    v.amplitude = sqrtf(x * x + y * y);
    v.angle     = atan2f(y, x);
    return v;
}

static Vector add_vectors(Vector a, Vector b)
{
    float ax = a.amplitude * cosf(a.angle);
    float ay = a.amplitude * sinf(a.angle);

    float bx = b.amplitude * cosf(b.angle);
    float by = b.amplitude * sinf(b.angle);

    return make_vector(ax + bx, ay + by);
}

int universe_load_from_config(Universe *u, const char *filename)
{
    if (!u || !filename) return -1;

    config_t cfg;
    config_init(&cfg);

    if (!config_read_file(&cfg, filename)) {
        fprintf(stderr,
                "Config error: %s:%d - %s\n",
                config_error_file(&cfg),
                config_error_line(&cfg),
                config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    int width, height;
    int num_planets;
    int max_trash, initial_trash;
    int ship_capacity;

    int reqrep_port, pub_port;

    //Get int values from the .cfg file
    
    if (!config_lookup_int(&cfg, "reqrep_port", &reqrep_port) ||
        !config_lookup_int(&cfg, "pub_port", &pub_port) ||
        !config_lookup_int(&cfg, "width", &width) ||
        !config_lookup_int(&cfg, "height", &height) ||
        !config_lookup_int(&cfg, "num_planets", &num_planets) ||
        !config_lookup_int(&cfg, "max_trash", &max_trash) ||
        !config_lookup_int(&cfg, "initial_trash", &initial_trash) ||
        !config_lookup_int(&cfg, "ship_capacity", &ship_capacity)) {
        fprintf(stderr, "Missing or invalid configuration setting\n");
        config_destroy(&cfg);
        return -1;
    }

    config_destroy(&cfg);


    if (reqrep_port <= 0 || reqrep_port > 65535 ||
        pub_port <= 0 || pub_port > 65535) {
        fprintf(stderr, "Invalid port values in configuration file\n");
        return -1;
    }

    u->reqrep_port = reqrep_port;
    u->pub_port    = pub_port;
    


    //Validates values read from the file
    if (width  <= 0 || height <= 0 ||
        num_planets <= 0 ||
        max_trash   <= 0 ||
        initial_trash < 0 ||
        ship_capacity <= 0) {
        fprintf(stderr, "Invalid values in configuration file\n");
        return -1;
    }

    //Initial trash can't exceed max trash
    if(initial_trash > max_trash){
        initial_trash = max_trash;
    }

    //Assigns universe values
    u->width  = width;
    u->height = height;
    u->num_planets = num_planets;
    u->max_trash = max_trash;
    u->initial_trash = initial_trash;
    u->num_trash = 0;          // Will be created in universe_create_trash
    u->ship_capacity = ship_capacity;
    u->planets = NULL;
    u->trash   = NULL;
    u->ships = NULL;
    u->num_ships = 0;
    u->recycling_planet = -1;

    return 0;
}


void universe_create_planets(Universe *u)
{
    if (!u || u->num_planets <= 0) return;

    u->planets = malloc(sizeof(Planet) * u->num_planets);
    if (!u->planets) {
        fprintf(stderr, "Failed to allocate planets array\n");
        u->num_planets = 0;
        return;
    }

    for (int i = 0; i < u->num_planets; i++) {
        Planet *p = &u->planets[i];

       //Same radius and mass for every planet
        p->radius = 20.0f;
        p->mass   = 10.0f;

        // Random positioning inside window boundaries
        p->x = (float)(rand() % u->width);
        p->y = (float)(rand() % u->height);

        // Assign letter (A-Z) name to planet
        // When all letters are used it starts from the beggining again
        int letter_index = i % 26;
        p->planet_name = (char)('A' + letter_index);

        p->recycle = false;
        p->trash_count = 0;
    }

    // Assign a random planet the Recycler role
    int recycle_index = rand() % u->num_planets;   // [0, num_planets-1]
    u->planets[recycle_index].recycle = true;
    u->recycling_planet = recycle_index;
}


void universe_create_trash(Universe *u)
{
    if (!u || u->max_trash <= 0) return;

    //Allocate an array big enough for the maximum possible trash
    u->trash = malloc(sizeof(Trash) * u->max_trash);
    if (!u->trash) {
        fprintf(stderr, "Failed to allocate trash array\n");
        u->num_trash = 0;
        return;
    }

    // initialize all entries as inactive, zero velocity/acceleration
    for (int i = 0; i < u->max_trash; ++i) {
        u->trash[i].active = 0;
        u->trash[i].velocity.amplitude = 0.0f;
        u->trash[i].velocity.angle     = 0.0f;
        u->trash[i].acceleration.amplitude = 0.0f;
        u->trash[i].acceleration.angle     = 0.0f;
    }

    // create the initial_trash elements at random positions within window boundaries
    u->num_trash = u->initial_trash;
    for (int i = 0; i < u->initial_trash && i < u->max_trash; ++i) {
        u->trash[i].x = (float)(rand() % u->width);
        u->trash[i].y = (float)(rand() % u->height);
        u->trash[i].active = 1;
    }
}


int universe_add_trash(Universe *u)
{
    if (!u || !u->trash) return -1;
    if (u->num_trash >= u->max_trash) return -1;

    //Finds inactive trash slot, activates it and adds it to the universe
    for (int i = 0; i < u->max_trash; ++i) {
        if (!u->trash[i].active) {
            universe_spawn_trash(u, i);
            u->num_trash++;
            return i;
        }
    }
    return -1;
}


void universe_spawn_trash(Universe *u, int index)
{
    if (!u || !u->trash) return;
    if (index < 0 || index >= u->max_trash) return;

    Trash *t = &u->trash[index];

    //Random postition within universe boundaries
    t->x = (float)(rand() % u->width);
    t->y = (float)(rand() % u->height);

    t->velocity.amplitude      = 0.0f;
    t->velocity.angle          = 0.0f;
    t->acceleration.amplitude  = 0.0f;
    t->acceleration.angle      = 0.0f;

    t->active = 1;
}


int universe_add_ship(Universe *u, char ship_id, uint32_t token)
{
    if (!u) return -1;

    // Allocate ships array
    if (!u->ships) {
        u->ships = malloc(sizeof(Ship) * MAX_SHIPS);
        if (!u->ships) {
            fprintf(stderr, "Failed to allocate ships array\n");
            return -1;
        }
        u->num_ships = 0;
        for (int i = 0; i < MAX_SHIPS; ++i) {
            u->ships[i].active  = 0;
            u->ships[i].ship_id = '?';
            u->ships[i].x       = 0.0f;
            u->ships[i].y       = 0.0f;
            u->ships[i].radius  = 8.0f;
            u->ships[i].cargo   = 0;
            u->ships[i].token   = 0;
            u->ships[i].velocity.amplitude = 0.0f;
            u->ships[i].velocity.angle     = 0.0f;
            u->ships[i].acceleration.amplitude = 0.0f;
            u->ships[i].acceleration.angle     = 0.0f;
        }
    }

    // Checks if ship already exists by it's ID. If it already exists it retuns the respective index
    for (int i = 0; i < MAX_SHIPS; ++i) {
        if (u->ships[i].active && u->ships[i].ship_id == ship_id) {
            return i;
        }
    }

    int index = (int)(ship_id - 'A');
    if (index < 0 || index >= MAX_SHIPS) return -1;
    if (u->ships[index].active) return -1; // slot ocupado

    //Initializes ship parameters
    Ship *s = &u->ships[index];
    s->active  = 1;
    s->ship_id = (char)('A' + index);
    s->radius  = 8.0f;
    s->cargo   = 0;      // starts empty
    s->token   = token;

    s->velocity.amplitude = 0.0f;
    s->velocity.angle     = 0.0f;
    s->acceleration.amplitude = 0.0f;
    s->acceleration.angle     = 0.0f;

    // spawn in the middle of the universe
    s->x = u->width  * 0.5f;
    s->y = u->height * 0.5f;

    int active = 0;
    for (int i = 0; i < MAX_SHIPS; ++i) if (u->ships[i].active) active++;
    u->num_ships = active;

    return index;
}


void universe_move_ship(Universe *u, char ship_id, direction_t dir)
{
    if (!u || !u->ships) return;

    //Find ship by it's id
    Ship *ship = NULL;
    for (int i = 0; i < MAX_SHIPS; ++i) {
        if (u->ships[i].active && u->ships[i].ship_id == ship_id) {
            ship = &u->ships[i];
            break;
        }
    }
    if (!ship) return;

    //Intensity of impulse to be added to the velocity vector
    const float kick = 1.0f; 

    Vector impulse;
    impulse.amplitude = kick;

    //Convert direction to angle(rad)
    switch (dir) {
        case UP:    impulse.angle = -((float)M_PI / 2.0f); break;
        case DOWN:  impulse.angle =  ((float)M_PI / 2.0f); break;
        case LEFT:  impulse.angle =  (float)M_PI;          break;
        case RIGHT: impulse.angle =  0.0f;                 break;
        default:    return;
    }

    ship->velocity = add_vectors(ship->velocity, impulse);
}


/**
 * @brief Compute the square of a floating point value (x^2).
 *
 * Small helper used in distance comparisons to avoid calling sqrt().
 *
 * @param x Input value.
 * @return x multiplied by itself.
 */
static float sqr(float x) { return x * x; }


void universe_update_ship_interactions(Universe *u)
{
    if (!u || !u->ships) return;

    const float trash_radius = 2.0f;  // trash is a 4x4 square, radius = 2

    // 1) collection of trash by ships
    if (u->trash) {
        for (int si = 0; si < MAX_SHIPS; ++si) {      //For each active ship
            Ship *s = &u->ships[si];
            if (!s->active) continue;

            for (int ti = 0; ti < u->max_trash; ++ti) {  //For each active trash
                Trash *t = &u->trash[ti];
                if (!t->active) continue;

                //Calculates distance between trash and ship
                float dx = s->x - t->x;
                float dy = s->y - t->y;
                float dist2 = sqr(dx) + sqr(dy);
                float limit = s->radius + trash_radius;

                //Checks if ship hits trash and if ship has capacity
                if (dist2 <= sqr(limit) && (s->cargo < u->ship_capacity)) {
                    t->active = 0;   // trash disappears (collected)
                    u->num_trash--;
                    s->cargo++;      // increments ship cargo
                }
            }
        }
    }

    // 2) interaction with planets: deposit or spill
    if (u->planets) {
        for (int si = 0; si < MAX_SHIPS; ++si) {      //for each active ship
            Ship *s = &u->ships[si];
            if (!s->active) continue;
            if (s->cargo == 0) continue;   // no cargo, nothing to deposit/spill

            for (int pi = 0; pi < u->num_planets; ++pi) { //for each planet
                Planet *p = &u->planets[pi];

                //Calculates distance between ship and planet
                float dx = s->x - p->x;
                float dy = s->y - p->y;
                float dist2   = sqr(dx) + sqr(dy);
                float limit   = s->radius + p->radius;

                //Checks for ship colision with planet
                if (dist2 <= sqr(limit)) {

                    //If it is recycle planet it deposits trash
                    if (p->recycle) {
                        p->trash_count += s->cargo;
                        s->cargo = 0;

                    //Crashes into regular planet(trash spill)
                    } else {
                        int spill = s->cargo;
                        s->cargo = 0;

                        for (int k = 0; k < spill; ++k) {
                            universe_add_trash(u);
                        }
                    }
                    break;
                }
            }
        }
    }
}


void universe_destroy(Universe *u)
{
    if (!u) return;
    free(u->planets);
    u->planets = NULL;
    u->num_planets = 0;

    free(u->trash);
    u->trash = NULL;
    u->num_trash = 0;

    free(u->ships);
    u->ships = NULL;
    u->num_ships = 0;
}

void universe_change_recycling_planet(Universe *u)
{
    if (!u || !u->planets || u->num_planets <= 0) return;

    int old = u->recycling_planet;
    if (old >= 0 && old < u->num_planets) {
        u->planets[old].recycle = false;
    }

    int next = old;
    if (u->num_planets == 1) {
        next = 0;
    } else {
        while (next == old) next = rand() % u->num_planets;
    }

    u->recycling_planet = next;
    u->planets[next].recycle = true;
}


int universe_remove_ship(Universe *u, char ship_id, uint32_t token)
{
    if (!u || !u->ships) return -1;

    int idx = (int)(ship_id - 'A');
    if (idx < 0 || idx >= MAX_SHIPS) return -1;

    Ship *s = &u->ships[idx];
    if (!s->active) return -1;
    if (s->token != token) return -1;

    s->active = 0;
    s->cargo  = 0;
    s->token  = 0;

    s->x = 0.0f;
    s->y = 0.0f;

    s->velocity.amplitude = 0.0f;
    s->velocity.angle     = 0.0f;
    s->acceleration.amplitude = 0.0f;
    s->acceleration.angle     = 0.0f;

    s->ship_id = (char)('A' + idx); 

    //recount active ships for consistency
    int active = 0;
    for (int i = 0; i < MAX_SHIPS; ++i) if (u->ships[i].active) active++;
    u->num_ships = active;

    return 0;
}



int client_load_from_config(ClientConfig *cfg, const char *filename)
{
    if (!cfg || !filename) return -1;

    memset(cfg, 0, sizeof(*cfg));

    config_t c;
    config_init(&c);

    if (!config_read_file(&c, filename)) {
        fprintf(stderr, "Config error: %s:%d - %s\n",
                config_error_file(&c),
                config_error_line(&c),
                config_error_text(&c));
        config_destroy(&c);
        return -1;
    }

    const char *addr = NULL;
    int reqrep_port=0, pub_port=0, width=0, height=0;

    if (!config_lookup_string(&c, "server_address", &addr) ||
        !config_lookup_int(&c, "reqrep_port", &reqrep_port) ||
        !config_lookup_int(&c, "pub_port", &pub_port) ||
        !config_lookup_int(&c, "width", &width) ||
        !config_lookup_int(&c, "height", &height)) {
        fprintf(stderr, "Client cfg: missing/invalid setting\n");
        config_destroy(&c);
        return -1;
    }

    if (!addr || addr[0] == '\0') { config_destroy(&c); return -1; }
    if (reqrep_port <= 0 || reqrep_port > 65535) { config_destroy(&c); return -1; }
    if (pub_port <= 0 || pub_port > 65535) { config_destroy(&c); return -1; }
    if (width <= 0 || height <= 0) { config_destroy(&c); return -1; }

    strncpy(cfg->server_address, addr, sizeof(cfg->server_address) - 1);
    cfg->server_address[sizeof(cfg->server_address) - 1] = '\0';

    cfg->reqrep_port = reqrep_port;
    cfg->pub_port    = pub_port;
    cfg->width       = width;
    cfg->height      = height;

    config_destroy(&c);
    return 0;
}

