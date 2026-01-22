#ifndef UNIVERSE_DATA_H
#define UNIVERSE_DATA_H

#include <stdbool.h>
#include <stdint.h>
#include "direction.h"

#define MAX_SHIPS 26

// Vector used for velocity and acceleration (needed in physics rules).
typedef struct {
    float amplitude;   // length of the vector
    float angle;       // direction in radians
} Vector;

typedef struct {
    float x;
    float y;
    float radius;
    float mass;
    char  planet_name;
    bool recycle;       //flag to identify recycle planet
    int trash_count;
} Planet;

typedef struct {
    float x;
    float y;
    Vector velocity;       
    Vector acceleration;   
    int   active;       // 1 if this trash is roaming(?), 0 if free slot
} Trash;

typedef struct {
    float x;
    float y;
    Vector velocity;
    Vector acceleration;
    float radius;
    int   active;       // 1 if ship is in use
    char  ship_id;   
    int   cargo; 
    uint32_t token;     //used to get access to the ship ID in the universe server 
} Ship;

typedef struct {
    int reqrep_port;
    int pub_port;

    int width;
    int height;

    int num_planets;
    int num_trash;      // current number of active trash
    int num_ships;
    int max_trash;
    int initial_trash;
    int ship_capacity;
    int recycling_planet;  // index of recycling planet (0,1,2,...)

    //Lists of universe elements
    Planet *planets;
    Trash *trash; 
    Ship *ships; 
} Universe;

typedef struct {
    char server_address[128];
    int  reqrep_port;
    int  pub_port;
    int  width;
    int  height;
} ClientConfig;



int client_load_from_config(ClientConfig *cfg, const char *filename);


/**
 * @brief Load universe parameters from a libconfig configuration file.
 *
 * Reads required integer settings from the given .cfg file (universe dimensions,
 * number of planets, trash limits/initial amount, and ship capacity), validates
 * the values, and stores them in the provided Universe structure.
 *
 * @param u Pointer to the Universe structure.
 * @param filename Path to the configuration file to read.
 * @return 0 on success; -1 on error.
 */
int universe_load_from_config(Universe *u, const char *filename);


/**
 * @brief Allocate and initialize the planet array for the universe.
 *
 * Allocates memory for u->num_planets planets and initializes each planet with
 * a fixed radius and mass, a random position within the universe bounds, and a
 * letter name ('A'..'Z', wrapping around if there are more than 26 planets).
 * After initialization, one planet is randomly selected as the recycling planet.
 *
 * @param u Pointer to Universe structure 
 */
void universe_create_planets(Universe *u);


/**
 * @brief Allocate and initialize the trash array for the universe.
 *
 * Allocates memory for u->max_trash trash slots and initializes all entries as
 * inactive with zero velocity and acceleration. Then it activates and places
 * the first u->initial_trash items at random positions within the universe
 * bounds, updating u->num_trash accordingly.
 *
 * @param u Pointer to universe structure
 */
void universe_create_trash(Universe *u);   


/**
 * @brief Add a new trash item to the universe.
 *
 * Searches the trash array for an inactive slot, activates it by spawning a new
 * trash item (position and initial state are set by universe_spawn_trash()), and
 * increments the active trash counter. If the universe is already at capacity
 * (num_trash >= max_trash) or no inactive slot is found, the function fails.
 *
 * @param u Pointer to the Universe structure.
 * @return Index of the newly activated trash slot on success; -1 on failure.
 */
int universe_add_trash(Universe *u);


/**
 * @brief Spawn a trash item in a given slot.
 *
 * Initializes the trash entry at the specified index by placing it at a random
 * position within the universe bounds, resetting its velocity and acceleration
 * to zero, and marking it as active.
 *
 * @param u Pointer to the Universe structure.
 * @param index Index of the trash slot to be activated in the universe.
 */
void universe_spawn_trash(Universe *u, int index);


/**
 * @brief Add (or retrieve) a ship entry in the universe by ship ID.
 *
 * Allocates the ships array on first use (MAX_SHIPS slots) and initializes
 * all slots as inactive. If a ship with the given ship_id already exists and is
 * active, the function returns its index without creating a duplicate. Otherwise,
 * it finds the first free slot, initializes the ship fields and spawns the ship at the
 * center of the universe. The function also updates u->num_ships so that
 * rendering/update loops reach the highest active index.
 *
 * @param u Pointer to the Universe structure.
 * @param ship_id Identifier to assign to the ship.
 * @param token Authentication token associated with the client controlling this ship.
 * @return Index of the existing/new ship slot on success; -1 on failure.
 */
int universe_add_ship(Universe *u, char ship_id, uint32_t token);


/**
 * @brief Move a ship one step in the specified direction, applying wrap-around at borders.
 *
 * Searches for the active ship with the given ship_id and updates its position by a
 * fixed step size (5 pixels) according to the direction character.
 * After moving, the position is wrapped around the universe boundaries.
 *
 * @param u Pointer to the Universe structure.
 * @param ship_id Identifier of the ship to move.
 * @param dir Direction command character ('u', 'd', 'l', 'r').
 */
void universe_move_ship(Universe *u, char ship_id, direction_t dir);


/**
 * @brief Update ship interactions with trash and planets (collect, deposit, spill).
 *
 * Performs proximity checks between ships and (1) active trash items
 * and (2) planets. If an active ship touches an active trash item and still has
 * available capacity, the trash is collected (deactivated), u->num_trash is
 * decremented, and the ship cargo is incremented. If a ship carrying cargo touches
 * a planet, then:
 *   - On the recycling planet, all cargo is deposited into the planet's trash_count.
 *   - On a non-recycling planet, the ship "crashes" and its entire cargo is spilled
 *     back into the universe by spawning that many new trash items.
 *
 * Collision checks use a circle–circle distance test,
 * treating trash as a small circle with radius 2 (matching a 4x4 drawn square).
 *
 * @param u Pointer to the Universe structure.
 */
void universe_update_ship_interactions(Universe *u);


/**
 * @brief Free all dynamically allocated resources owned by a Universe.
 *
 * Releases the planets, trash, and ships arrays (if allocated) and resets the
 * corresponding pointers and counters.
 *
 * @param u Pointer to the Universe structure.
 */
void universe_destroy(Universe *u);

void universe_change_recycling_planet(Universe *u);

int universe_remove_ship(Universe *u, char ship_id, uint32_t token);


#endif 