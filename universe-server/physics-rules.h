#ifndef PHYSICS_RULES_H
#define PHYSICS_RULES_H

#include "universe-data.h"

void physics_step(Universe *u);

Vector physics_update_acceleration(float x, float y, int N, Planet *planets);
Vector physics_update_velocity(Vector v, Vector a);
Coordinates physics_update_position(Coordinates position, Vector velocity, int width, int height);





#endif // PHYSICS_RULES_H
