#ifndef DISPLAY_H
#define DISPLAY_H

#include <SDL2/SDL.h>
#include "universe-data.h"
#include "direction.h"

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
} Display;


/**
 * @brief Initialize the SDL Window used by the client.
 *
 * Creates a small centered SDL window (200x200) and a renderer,
 * then clears the screen to a black background. This window is used to capture
 * keyboard input and display a simple arrow symbol.
 *
 * @param d Pointer to a Display structure.
 * @return 0 on success; -1 on failure.
 */
int display_control_init(Display *d);


/**
 * @brief Initialize the SDL window used to render the universe (server window).
 *
 * Creates a centered SDL window sized to the universe dimensions (u->width x u->height)
 * and a renderer. The renderer is cleared to a black background before
 * returning. This window is used by the server to draw the universe.
 *
 * @param d Pointer to a Display structure.
 * @param u Pointer to a Universe structure.
 * @return 0 on success; -1 on failure.
 */
int display_universe_init(Display *d, const Universe *u);


/**
 * @brief Render all planets in the universe.
 *
 * Draws each planet as a filled circle and prints its label next to it. The recycling
 * planet is rendered with a distinct color; for the recycling planet the label also
 * includes the current stored trash count in the format "P(n)".
 *
 * @param d Pointer to a Display structure.
 * @param u Pointer to the Universe structure containing the planets to be rendered.
 */
void display_draw_planets(const Display *d, const Universe *u);


/**
 * @brief Render all active trash items in the universe.
 *
 * Iterates over the trash slots and draws each active trash as a small white
 * filled square at its current (x, y) position.
 *
 * @param d Pointer to Display structure.
 * @param u Pointer to the Universe containing the trash array and limits.
 */
void display_draw_trash(const Display *d, const Universe *u);


/**
 * @brief Render all active ships in the universe.
 *
 * Iterates over the ship array and draws each active ship as a filled diamond
 * centered at its (x, y) position. Next to each ship it renders a label in
 * the format "ID(cargo)", where ID is the ship identifier
 * and cargo is the amount of trash currently carried by that ship.
 *
 * @param d Pointer Display structure.
 * @param u Pointer to the Universe containing the ships to be rendered.
 */
void display_draw_ships(const Display *d, const Universe *u);


/**
 * @brief Render a complete frame of the universe (planets, trash, and ships).
 *
 * Clears the renderer to a black background, draws all universe entities
 * (planets, trash, and ships), and presents the final frame to the window.
 *
 * @param d Pointer to Display structure.
 * @param u Pointer to the Universe to be rendered.
 */
void display_draw_universe(const Display *d, const Universe *u);


/**
 * @brief Draw a directional arrow in the client control window.
 *
 * Clears the client window to a black background and renders a white filled
 * triangle centered in the window, oriented according to the given direction.
 *
 * @param d Pointer to Display structure.
 * @param dir Direction to be represented by the arrow (UP/DOWN/LEFT/RIGHT).
 */
void display_control_draw_arrow(const Display *d, direction_t dir);


/**
 * @brief Display a "GAME OVER" string on the window.
 *
 * Clears the current render target to a black background and draws the text
 * "GAME OVER" in white. If a valid Universe is provided, the text is placed
 * approximately at the center of the universe window; otherwise, it falls back
 * to a default position (100, 100). 
 *
 * @param d Pointer to Display structure.
 * @param u Pointer to Universe structure.
 */
void display_show_game_over(const Display *d, const Universe *u);


/**
 * @brief Clear SDL resources associated with a Display and shut down SDL.
 *
 * Destroys the SDL_Renderer and SDL_Window owned by the Display structure,
 * calls SDL_Quit() to clean up SDL subsystems, and resets pointers to NULL.
 *
 * @param d Pointer to the Display structure to be shut down.
 */
void display_shutdown(Display *d);

#endif
