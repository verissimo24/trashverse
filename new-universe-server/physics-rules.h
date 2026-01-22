#ifndef PHYSICS_RULES_H
#define PHYSICS_RULES_H

#include "universe-data.h"

void physics_update_acceleration(Universe *u);
void physics_update_velocity(Universe *u);
void physics_update_position(Universe *u);

void physics_step(Universe *u);

void physics_handle_collisions(Universe *u);

void physics_update_ship_acceleration(Universe *u);
void physics_update_ship_velocity(Universe *u);
void physics_update_ship_position(Universe *u);

#endif // PHYSICS_RULES_H
