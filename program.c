#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>

// The width and height of the window
const int WIDTH = 640;
const int HEIGHT = 480;

void draw_shapes(SDL_Renderer *renderer) {
  // Set the drawing color to red
  SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

  // Draw a rectangle at position (10, 10) with size (100, 100)
  SDL_Rect rect = {10, 10, 100, 100};
  SDL_RenderFillRect(renderer, &rect);

  // Draw a circle centered at (200, 200) with radius 50
  int x = 200, y = 200, r = 50;
  for (int w = 0; w < r * 2; w++) {
    for (int h = 0; h < r * 2; h++) {
      int dx = x - w;
      int dy = y - h;
      if ((dx * dx + dy * dy) <= (r * r)) {
        SDL_RenderDrawPoint(renderer, w, h);
      }
    }
  }

  // Draw a triangle with vertices at (300, 300), (350, 350), and (400, 300)
  SDL_Point points[3] = {{300, 300}, {350, 350}, {400, 300}};
  SDL_RenderDrawLines(renderer, points, 3);
}


int main(int argc, char *argv[]) {
  // The SDL window and renderer
  SDL_Window *window = NULL;
  SDL_Renderer *renderer = NULL;

  // Initialize SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
    printf("Error initializing SDL: %s\n", SDL_GetError());
    return 1;
  }

  // Create the window and renderer
  window = SDL_CreateWindow("2D Array", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
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

  // Enter the main loop
  while (1) {
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
        printf("Axis %d moved to %d\n", event.caxis.axis, event.caxis.value);
      }
    }

    // Clear the screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    draw_shapes(renderer);

    // Update the screen
    SDL_RenderPresent(renderer);
  }

exit:
  // Clean up
  SDL_GameControllerClose(controller);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
