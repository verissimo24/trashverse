#include "display.h"
#include <stdio.h>
#include <SDL2/SDL2_gfxPrimitives.h>


int display_control_init(Display *d)
{
    if (!d) return -1;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return -1;
    }

    //Creates centered/squared window
    d->window = SDL_CreateWindow(
        "Universe Client - Use arrow keys (ESC to quit)", //title
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        200,  //width
        200,  //height
        SDL_WINDOW_SHOWN
    );

    if (!d->window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    //Renderer creation
    Uint32 flags = SDL_RENDERER_ACCELERATED;
    d->renderer = SDL_CreateRenderer(d->window, -1, flags);

    if (!d->renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(d->window);
        SDL_Quit();
        return -1;
    }

    // black background by default
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);
    SDL_RenderPresent(d->renderer);

    return 0;
}


int display_universe_init(Display *d, const Universe *u)
{
    if (!d || !u) return -1;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return -1;
    }

    //Creates centered window
    d->window = SDL_CreateWindow(
        "Universe Simulator", //title
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        u->width,
        u->height,
        0
    );

    if (!d->window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    //Renderer creation
    Uint32 flags = SDL_RENDERER_ACCELERATED;
    d->renderer = SDL_CreateRenderer(d->window, -1, flags);

    if (!d->renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(d->window);
        SDL_Quit();
        return -1;
    }

    // black background by default
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);
    SDL_RenderPresent(d->renderer);

    return 0;
}


void display_draw_planets(const Display *d, const Universe *u)
{
    if (!d || !d->renderer || !u || !u->planets) return;

    Uint32 planet_color;
    Uint32 default_color = 0xFF8080A0;  //grey-ish
    Uint32 recycle_color  = 0xFF00FF00; //Recycling Planet color - Green 
    Uint32 text_color   = 0xFFFFFFFF;   //white

    char label[16];

    for (int i = 0; i < u->num_planets; ++i) {
        const Planet *p = &u->planets[i];

        //Checks if it is the recycling planet and chooses the planet color acordingly 
        if (p->recycle) {
            planet_color = recycle_color;
        }
        else{
            planet_color = default_color;
        }

        // Draws the planet
        filledCircleColor(  
            d->renderer,
            (Sint16)p->position.x,
            (Sint16)p->position.y,
            (Sint16)p->radius,
            planet_color
        );

        //  planet name: "planet_name(num_trash)" -> Number of trash that has been recycled by this planet
        snprintf(label, sizeof(label), "%c(%d)", p->planet_name, p->trash_count);

        //prints planet name
        stringColor(
            d->renderer,
            (Sint16)(p->position.x + p->radius + 4),  // x position
            (Sint16)(p->position.y - 4),              // y position
            label,
            text_color
        );
    }
}


void display_draw_trash(const Display *d, const Universe *u)
{
    if (!d || !d->renderer || !u || !u->trash) return;

    for (int i = 0; i < u->max_trash; ++i) {
        const Trash *t = &u->trash[i];
        if (!t->active) continue; //checks if trash is active or not

        int size = 4; //size of the square

        //draws square
        boxColor(
            d->renderer,
            (Sint16)t->position.x,
            (Sint16)t->position.y,
            (Sint16)(t->position.x + size),
            (Sint16)(t->position.y + size),
            0xFFFFFFFF    // white
        );
    }
}


void display_draw_ships(const Display *d, const Universe *u)
{
    if (!d || !d->renderer || !u || !u->ships) return;

    for (int i = 0; i < MAX_SHIPS; ++i) {
        const Ship *s = &u->ships[i];
        if (!s->active) continue;

        Uint32 ship_color = 0xFFFF8000;  // blue
        Uint32 text_color = 0xFFFFFFFF;  // white

        int cx = (int)s->position.x;
        int cy = (int)s->position.y;

        //size of the ship
        int r = (int)s->radius;

        // Four vertices of the diamond centered at (cx, cy) with radius r
        Sint16 vx[4] = { (Sint16)cx, (Sint16)(cx + r), (Sint16)cx, (Sint16)(cx - r) };
        Sint16 vy[4] = { (Sint16)(cy - r), (Sint16)cy, (Sint16)(cy + r), (Sint16)cy };

        // Draws diamond 
        filledPolygonColor(d->renderer, vx, vy, 4, ship_color);

        // Label: "A(n)" where A is ship id and n is cargo
        char label[16];
        snprintf(label, sizeof(label), "%c(%d)", s->ship_id, s->cargo);

        // Put the label slightly to the right of the ship
        stringColor(d->renderer, cx + r + 2, cy - 4, label, text_color);
    }
}


void display_draw_universe(const Display *d, const Universe *u)
{
    if (!d || !d->renderer) return;

    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);  
    SDL_RenderClear(d->renderer);  //clears the frame

    display_draw_planets(d, u);                         
    display_draw_trash(d, u); 
    display_draw_ships(d, u);   
    

    SDL_RenderPresent(d->renderer); //draws frame                
}


void display_control_draw_arrow(const Display *d, direction_t dir)
{
    if (!d || !d->renderer || !d->window) return;

    //clears window
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);

    //Get window center
    int w = 0, h = 0;
    SDL_GetWindowSize(d->window, &w, &h);
    int cx = w / 2;
    int cy = h / 2;

    // Size of the triangle (arrow)
    int tip_len  = 70;  // Height of the triangle
    int half_base = 25; // half of the size of the triangle base

    Uint32 white = 0xFFFFFFFF;

    Sint16 x1, y1, x2, y2, x3, y3;

    //Select triangle vertices according to the direction of the arrow
    switch (dir) {
        case UP:
            x1 = cx;              y1 = cy - tip_len;   
            x2 = cx - half_base;  y2 = cy + tip_len/2; 
            x3 = cx + half_base;  y3 = cy + tip_len/2; 
            break;

        case DOWN:
            x1 = cx;              y1 = cy + tip_len;
            x2 = cx - half_base;  y2 = cy - tip_len/2;
            x3 = cx + half_base;  y3 = cy - tip_len/2;
            break;

        case LEFT:
            x1 = cx - tip_len;    y1 = cy;
            x2 = cx + tip_len/2;  y2 = cy - half_base;
            x3 = cx + tip_len/2;  y3 = cy + half_base;
            break;

        case RIGHT:
            x1 = cx + tip_len;    y1 = cy;
            x2 = cx - tip_len/2;  y2 = cy - half_base;
            x3 = cx - tip_len/2;  y3 = cy + half_base;
            break;

        default:
            SDL_RenderPresent(d->renderer);  //Not a valid direction
            return;
    }

    //Draw triangle
    filledTrigonColor(d->renderer, x1, y1, x2, y2, x3, y3, white);
    SDL_RenderPresent(d->renderer);
}


void display_show_game_over(const Display *d, const Universe *u)
{
    if (!d || !d->renderer) return;

    // Black background
    SDL_SetRenderDrawColor(d->renderer, 0, 0, 0, 255);
    SDL_RenderClear(d->renderer);

    const char *msg = "GAME OVER";
    Uint32 text_color = 0xFFFFFFFF; // white

    // centering the text
    Sint16 x = 100;
    Sint16 y = 100;

    if (u) {
        x = (Sint16)(u->width  / 2 - 50);  
        y = (Sint16)(u->height / 2 - 8);
    }

    stringColor(d->renderer, x,   y,   msg, text_color);
    stringColor(d->renderer, x+1, y,   msg, text_color);
    stringColor(d->renderer, x,   y+1, msg, text_color);

    SDL_RenderPresent(d->renderer);

    SDL_Delay(3000);
}


void display_shutdown(Display *d)
{
    if (!d) return;

    if (d->renderer) SDL_DestroyRenderer(d->renderer);
    if (d->window)   SDL_DestroyWindow(d->window);

    SDL_Quit();

    d->renderer = NULL;
    d->window   = NULL;
}

