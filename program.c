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

uint8_t pixels[MAX_HEIGHT][MAX_WIDTH][CHANNELS];

float normalize_int(int value, int min, int max, float out_min, float out_max) {
  return (float)(value - min) / (float)(max - min) * (out_max - out_min) + out_min;
}



uint8_t mandelbrot(float x, float y) {
  // Set the starting point for the iteration
  complex float c = x + y * I;
  complex float z = 0.0 + 0.0 * I;

  // Set the maximum number of iterations
  int max_iterations = 255;

  // Iterate until the point escapes or the maximum number of iterations is reached
  int i;
  for (i = 0; i < max_iterations && cabs(z) < 2.0; i++) {
    z = z * z + c;
  }

  // Set the color based on the number of iterations
  return (uint8_t)i;
}



void colormap(uint8_t intensity, uint8_t *r, uint8_t *g, uint8_t *b) {
  // Check if intensity is within valid range [0, 255]
  if (intensity > 255) intensity = 255;
  if (intensity < 0) intensity = 0;

  // Map intensity to a color value using a simple linear scale
  *r = (uint8_t) (255 - intensity);
  *g = (uint8_t) (intensity * 0.5);
  *b = (uint8_t) (intensity * 0.25);
}


int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surface;
    SDL_Texture *texture;

    float fps;
    int width = MAX_WIDTH;
    int height = 480;

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
            if (event.type == SDL_CONTROLLERAXISMOTION) {
                printf("Axis %d moved to %f\n", event.caxis.axis, normalize_int(event.caxis.value, -32768, 32768, -1, 1));
            }
        }

        // Do some stuff
        for (int i=0; i<height; i++)
        {
            for (int j=0; j<width; j++)
            {
                float y = normalize_int(i, 0, height, -1, 1);
                float x = normalize_int(j, 0, width, -1, 1);

                uint8_t r, g, b;

                uint8_t intensity = mandelbrot(x, y);
                colormap(intensity, &r, &g, &b);

                pixels[i][j][0] = r;
                pixels[i][j][1] = g;
                pixels[i][j][2] = b;
            }
        }

        // Clear the screen
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Display pixel array
        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_DestroyTexture(texture);

        // Update the screen
        SDL_RenderPresent(renderer);

        // Get end ticks for fps
        uint32_t frame_time = SDL_GetTicks()-start_time;
        fps = (frame_time > 0) ? 1000.0f / frame_time : 0.0f;
        printf("FPS: %f\n", fps);
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
