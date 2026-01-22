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
    physics_update_acceleration(u);
    physics_update_velocity(u);
    physics_update_position(u);
    physics_handle_collisions(u);

    physics_update_ship_acceleration(u);
    physics_update_ship_velocity(u);
    physics_update_ship_position(u);
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
void physics_update_acceleration(Universe *u)
{
    if (!u || !u->trash || !u->planets) return;

    for (int n_trash = 0; n_trash < u->max_trash; n_trash++) {
        Trash *t = &u->trash[n_trash];
        if (!t->active) continue;

        Vector total_vector_force;
        total_vector_force.amplitude = 0.0f;
        total_vector_force.angle     = 0.0f;

        for (int n_planet = 0; n_planet < u->num_planets; n_planet++) {
            Planet *p = &u->planets[n_planet];

            float force_vector_x = p->x - t->x;
            float force_vector_y = p->y - t->y;

            Vector local_vector_force = make_vector(force_vector_x,
                                                    force_vector_y);

            // avoid division by zero
            if (local_vector_force.amplitude < 1.0f)
                local_vector_force.amplitude = 1.0f;

            // amplitude = mass / r^2  (from spec)
            local_vector_force.amplitude =
                p->mass / (local_vector_force.amplitude * local_vector_force.amplitude);

            total_vector_force = add_vectors(local_vector_force,
                                             total_vector_force);
        }

        t->acceleration = total_vector_force;
    }
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
void physics_update_velocity(Universe *u)
{
    if (!u || !u->trash) return;

    for (int n_trash = 0; n_trash < u->max_trash; n_trash++) {
        Trash *t = &u->trash[n_trash];
        if (!t->active) continue;

        // friction
        t->velocity.amplitude *= 0.99f;

        // v = v + a (both are vectors in amplitude/angle form)
        t->velocity = add_vectors(t->velocity, t->acceleration);
    }
}

/*
 * Update position of all trash.
 *
 * Direct translation of new_trash_position():
 *   x += v.amplitude * cos(v.angle)
 *   y += v.amplitude * sin(v.angle)
 *   correct_position(x), correct_position(y)
 */
void physics_update_position(Universe *u)
{
    if (!u || !u->trash) return;

    for (int n_trash = 0; n_trash < u->max_trash; n_trash++) {
        Trash *t = &u->trash[n_trash];
        if (!t->active) continue;

        t->x += t->velocity.amplitude * cosf(t->velocity.angle);
        t->y += t->velocity.amplitude * sinf(t->velocity.angle);

        t->x = correct_position(t->x, (float)u->width);
        t->y = correct_position(t->y, (float)u->height);
    }
}

/**
 * Handles colision of trash on planets.
 *
 * When a trash piece reaches the center of a planet (distance < 1.0),
 * we respawn it somewhere else using universe_respawn_trash().
 */
void physics_handle_collisions(Universe *u)
{
    if (!u || !u->trash || !u->planets) return;

    for (int i = 0; i < u->max_trash; i++) {
        Trash *t = &u->trash[i];
        if (!t->active) continue;

        for (int p = 0; p < u->num_planets; p++) {
            Planet *pl = &u->planets[p];

            float dx = pl->x - t->x;
            float dy = pl->y - t->y;
            float dist2 = dx * dx + dy * dy;
            float dist  = sqrtf(dist2);

            //Check if it colided with planet and if there are active ships
            if ((dist < 1.0f) && (u->num_ships>0)) {
                universe_add_trash(u);
                break;  // done with this trash
            }
        }
    }
}



void physics_update_ship_acceleration(Universe *u)
{
    if (!u || !u->ships || !u->planets) return;

    for (int si = 0; si < MAX_SHIPS; ++si) {
        Ship *s = &u->ships[si];
        if (!s->active) continue;

        Vector total;
        total.amplitude = 0.0f;
        total.angle     = 0.0f;

        for (int pi = 0; pi < u->num_planets; ++pi) {
            Planet *p = &u->planets[pi];

            float dx = p->x - s->x;
            float dy = p->y - s->y;

            Vector local = make_vector(dx, dy);

            if (local.amplitude < 1.0f) local.amplitude = 1.0f;

            local.amplitude = p->mass / (local.amplitude * local.amplitude);

            total = add_vectors(local, total);
        }

        s->acceleration = total;
    }
}

void physics_update_ship_velocity(Universe *u)
{
    if (!u || !u->ships) return;

    for (int si = 0; si < MAX_SHIPS; ++si) {
        Ship *s = &u->ships[si];
        if (!s->active) continue;

        // atrito 1%
        s->velocity.amplitude *= 0.99f;

        // v = v + a
        s->velocity = add_vectors(s->velocity, s->acceleration);
    }
}

void physics_update_ship_position(Universe *u)
{
    if (!u || !u->ships) return;

    for (int si = 0; si < MAX_SHIPS; ++si) {
        Ship *s = &u->ships[si];
        if (!s->active) continue;

        s->x += s->velocity.amplitude * cosf(s->velocity.angle);
        s->y += s->velocity.amplitude * sinf(s->velocity.angle);

        s->x = correct_position(s->x, (float)u->width);
        s->y = correct_position(s->y, (float)u->height);
    }
}


