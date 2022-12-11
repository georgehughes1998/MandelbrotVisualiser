#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_ttf.h>

#include <stdint.h>
#include <math.h>
#include <complex.h>

#define MAX_WIDTH 640
#define MAX_HEIGHT 640
#define CHANNELS 3

#define CONTROLLER_MAXIMUM 32768
#define CONTROLLER_OFFSET_SCALAR 0.1

uint8_t pixels[MAX_HEIGHT * MAX_WIDTH * CHANNELS];

double normalise_int(int value, int min, int max, double out_min, double out_max) {
  return (double)(value - min) / (double)(max - min) * (out_max - out_min) + out_min;
}

int mandelbrot(double x, double y) {
  double c_real = x;
  double c_imag = y;
  double z_real = 0;
  double z_imag = 0;

  int iterations = 0;
  int max_iterations = 200;

  double z_real_squared = 0;
  double z_imag_squared = 0;

  while (z_real_squared + z_imag_squared < 4 && iterations < max_iterations) {
    z_real_squared = z_real * z_real;
    z_imag_squared = z_imag * z_imag;
    z_imag = 2 * z_real * z_imag + c_imag;
    z_real = z_real_squared - z_imag_squared + c_real;
    iterations++;
  }

  return iterations;
}

void colourmap(double intensity, uint8_t *red, uint8_t *green, uint8_t *blue) {
  if (intensity < 0.25) {
    // black to blue
    *red = 0;
    *green = 0;
    *blue = (int)(intensity * 4 * 255);
  } else if (intensity < 0.5) {
    // blue to green
    *red = 0;
    *green = (int)((intensity - 0.25) * 4 * 255);
    *blue = 255;
  } else if (intensity < 0.75) {
    // green to yellow
    *red = (int)((intensity - 0.5) * 4 * 255);
    *green = 255;
    *blue = 255 - (int)((intensity - 0.5) * 4 * 255);
  } else {
    // yellow to white
    *red = 255;
    *green = 255;
    *blue = (int)((1 - intensity) * 4 * 255);
  }
}


int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font* font = NULL;
    SDL_Surface *surface;
    SDL_Surface *text_surface;
    SDL_Texture *texture;

    SDL_Color text_colour = {0, 255, 0};

    int width = 640;
    int height = 480;
    double xoffset = 0, yoffset = 0;
    double zoom = 1;
    double fps = 0;
    char fps_text[20] = "fps: ";

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        printf("Error initializing SDL: %s\n", SDL_GetError());
        return 1;
    }

    // Create the window and renderer
    window = SDL_CreateWindow("2D Array", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("Error creating window: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Error creating renderer: %s\n", SDL_GetError());
        return 1;
    }

    //Initialize SDL_ttf
    if( TTF_Init() == -1 )
    {
        printf( "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError() );
        return 1;
    }

    font = TTF_OpenFont("arial.ttf", 28 );
    if (font == NULL)
    {
        fprintf(stderr, "Error: failed to load font: %s\n", TTF_GetError());
        return 1;
    }

    // Initialise controller
    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks < 1) {
        fprintf(stderr, "No game controllers detected!\n");
        return 1;
    }

    SDL_GameController *controller = SDL_GameControllerOpen(0);
    if (!controller) {
        fprintf(stderr, "Failed to open game controller! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    surface = SDL_CreateRGBSurfaceFrom((void*) pixels, width, height, 24, width * 3, 0x0000FF, 0x00FF00, 0xFF0000, 0);

    // Enter the main loop
    while (1) {
        // Get start ticks for fps
        uint32_t start_time = SDL_GetTicks();

        // Check for events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                // Quit the main loop if the window is closed
                goto exit;
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym)
                case SDLK_ESCAPE: goto exit;
            }
        }

        int16_t xaxis = SDL_GameControllerGetAxis(controller, 0);
        int16_t yaxis = SDL_GameControllerGetAxis(controller, 1);
        int16_t ltrigger = SDL_GameControllerGetAxis(controller, 4);
        int16_t rtrigger = SDL_GameControllerGetAxis(controller, 5);

        xoffset += normalise_int(xaxis, -CONTROLLER_MAXIMUM, CONTROLLER_MAXIMUM, -CONTROLLER_OFFSET_SCALAR * zoom, CONTROLLER_OFFSET_SCALAR * zoom);
        yoffset += normalise_int(yaxis, -CONTROLLER_MAXIMUM, CONTROLLER_MAXIMUM, -CONTROLLER_OFFSET_SCALAR * zoom, CONTROLLER_OFFSET_SCALAR * zoom);
        zoom *= normalise_int(ltrigger, -CONTROLLER_MAXIMUM, CONTROLLER_MAXIMUM, 0.1, 2);
        zoom /= normalise_int(rtrigger, -CONTROLLER_MAXIMUM, CONTROLLER_MAXIMUM, 0.1, 2);

        // Modify the pixel grid for display
        for (int p=0; p<height*width*CHANNELS; p+=CHANNELS)
        {
            // Get row, column from index
            int j = (p / CHANNELS) % width;
            int i = p / (CHANNELS * width);

            // Calculate Mandelbrot
            double y = normalise_int(i, 0, height, -zoom, zoom);
            double x = normalise_int(j, 0, width, -zoom, zoom);
            uint8_t intensity = mandelbrot(x + xoffset, y + yoffset);

            // Convert to colourmap
            uint8_t r, g, b;
            colourmap((double)intensity / 255, &r, &g, &b);

            pixels[p    ] = r;
            pixels[p + 1] = g;
            pixels[p + 2] = b;
        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Render font

        sprintf(fps_text+5, "%.2f", fps);
        text_surface = TTF_RenderText_Solid(font, fps_text,text_colour);
        SDL_BlitSurface(text_surface, NULL, surface, NULL);

        // Display pixel array
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_DestroyTexture(texture);

        // Update the screen
        SDL_RenderPresent(renderer);

        // Get end ticks for fps
        uint32_t frame_time = SDL_GetTicks()-start_time;
        fps = (frame_time > 0) ? 1000.0f / frame_time : 0.0f;
    }

    exit:
        // Clean up
        SDL_GameControllerClose(controller);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyTexture(texture);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
}
