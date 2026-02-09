#include "physics-rules.h"
#include <math.h>

// Helpers that implement the spec's vector model (amplitude + angle)

// Build a Vector from cartesian components (x, y)
static Vector make_vector(float x, float y)
{
    Vector v;
    v.amplitude = sqrtf(x * x + y * y);
    v.angle     = atan2f(y, x);
    return v;
}

// Add two vectors in (amplitude, angle) form
static Vector add_vectors(Vector a, Vector b)
{
    // convert a to cartesian
    float ax = a.amplitude * cosf(a.angle);
    float ay = a.amplitude * sinf(a.angle);

    // convert b to cartesian
    float bx = b.amplitude * cosf(b.angle);
    float by = b.amplitude * sinf(b.angle);

    // sum
    float sx = ax + bx;
    float sy = ay + by;

    return make_vector(sx, sy);
}

// Wrap a coordinate into [0, limit) with teleportation on edges
static float correct_position(float coord, float limit)
{
    while (coord < 0.0f)
        coord += limit;
    while (coord >= limit)
        coord -= limit;
    return coord;
}

/*
 * One physics step.
 *
 * Called once per frame from main:
 *   physics_step(&universe, dt);
 */
void physics_step(Universe *u)
{

    if (!u) return;

    if(u->trash){
        for (int n_trash = 0; n_trash < u->max_trash; n_trash++) 
        {
            Trash *t = &u->trash[n_trash];
            if (!t->active) continue;

            t->acceleration = physics_update_acceleration(t->position.x, t->position.y, u->num_planets, u->planets);
            t->velocity = physics_update_velocity(t->velocity, t->acceleration);
            t->position = physics_update_position(t->position, t->velocity, u->width, u->height);
        }
    }

    if(u->ships){
        for (int n_ship = 0; n_ship < MAX_SHIPS; n_ship++) 
        {
            Ship *s = &u->ships[n_ship];
            if (!s->active) continue;
            s->acceleration = physics_update_acceleration(s->position.x, s->position.y, u->num_planets, u->planets);
            s->velocity = physics_update_velocity(s->velocity, s->acceleration);
            s->position = physics_update_position(s->position, s->velocity, u->width, u->height);
        }
    }
}


/*
 * Compute new acceleration for all trash pieces.
 *
 * Direct translation of the spec's new_trash_acceleration():
 *  - For each trash, sum the gravitational force vectors from all planets.
 *  - Force direction is from trash to planet.
 *  - Amplitude is mass / r^2.
 *  - The result is stored in trash[n_trash].acceleration.
 */
Vector physics_update_acceleration(float x, float y, int N, Planet *planets)
{
    
    Vector total_vector_force;
    total_vector_force.amplitude = 0.0f;
    total_vector_force.angle     = 0.0f;

    for (int i = 0; i < N; i++) {
        Planet *p = &planets[i];

        float force_vector_x = p->position.x - x;
        float force_vector_y = p->position.y - y;

        Vector local_vector_force = make_vector(force_vector_x,
                                                force_vector_y);

        // avoid division by zero
        if (local_vector_force.amplitude < 1.0f)
            local_vector_force.amplitude = 1.0f;

        // amplitude = mass / r^2  (from spec)
        local_vector_force.amplitude =
            p->mass / (local_vector_force.amplitude * local_vector_force.amplitude);

        total_vector_force = add_vectors(local_vector_force, total_vector_force);
    }

    return total_vector_force;
}

/*
 * Update velocity of all trash.
 *
 * Direct translation of new_trash_velocity():
 *   velocity.amplitude *= 0.99      (friction)
 *   velocity = velocity + acceleration  (vector sum)
 *
 * The spec version does not use dt explicitly, so we ignore it.
 */
Vector physics_update_velocity(Vector v, Vector a)
{    
    // friction
    v.amplitude *= 0.99f;

    // v = v + a (both are vectors in amplitude/angle form)
    return  add_vectors(v, a);
}

/*
 * Update position of all trash.
 *
 * Direct translation of new_trash_position():
 *   x += v.amplitude * cos(v.angle)
 *   y += v.amplitude * sin(v.angle)
 *   correct_position(x), correct_position(y)
 */
Coordinates physics_update_position(Coordinates position, Vector velocity, int width, int height)
{
    position.x += velocity.amplitude * cosf(velocity.angle);
    position.y += velocity.amplitude * sinf(velocity.angle);

    position.x = correct_position(position.x, (float)width);
    position.y = correct_position(position.y, (float)height);

    return position;
}
