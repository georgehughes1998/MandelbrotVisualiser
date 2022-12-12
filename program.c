#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <SDL_ttf.h>

#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>

#include <stdint.h>
#include <math.h>
#include <complex.h>

#define MAX_WIDTH 1280
#define MAX_HEIGHT 1080
#define CHANNELS 3
#define BUFFER_SIZE MAX_HEIGHT * MAX_WIDTH * CHANNELS

#define CONTROLLER_MAXIMUM 32768
#define CONTROLLER_OFFSET_SCALAR 0.08

const char *kernel_string =
"int mandelbrot(double x, double y) {"
    "double c_real = x;"
    "double c_imag = y;"
    "double z_real = 0;"
    "double z_imag = 0;"

    "int iterations = 0;"
    "int max_iterations = 800;"

    "double z_real_squared = 0;"
    "double z_imag_squared = 0;"

    "while (z_real_squared + z_imag_squared < 4 && iterations <= max_iterations) {"
        "z_real_squared = z_real * z_real;"
        "z_imag_squared = z_imag * z_imag;"
        "z_imag = 2 * z_real * z_imag + c_imag;"
        "z_real = z_real_squared - z_imag_squared + c_real;"
        "iterations++;"
    "}"

  "return iterations;"
"}"

"double normalise_int(int value, int min, int max, double out_min, double out_max) {"
    "return (double)(value - min) / (double)(max - min) * (out_max - out_min) + out_min;"
"}"

"void colourmap(int iintensity, uchar *red, uchar *green, uchar *blue) {"
  "double intensity = normalise_int(iintensity, 0, 800, 0, 1);"
  "if (intensity < 0.25) {"
    "// black to blue\n"
    "*red = 0;"
    "*green = 0;"
    "*blue = (uchar)(intensity * 4 * 255);"
  "} else if (intensity < 0.5) {"
    "// blue to green\n"
    "*red = 0;"
    "*green = (uchar)((intensity - 0.25) * 4 * 255);"
    "*blue = 255;"
  "} else if (intensity < 0.75) {"
    "// green to yellow\n"
    "*red = (uchar)((intensity - 0.5) * 4 * 255);"
    "*green = 255;"
    "*blue = 255 - (uchar)((intensity - 0.5) * 4 * 255);"
  "} else {"
    "// yellow to white\n"
    "*red = 255;"
    "*green = 255;"
    "*blue = (uchar)((1 - intensity) * 4 * 255);"
  "}"
"}"

"__kernel void mandelbrot_kernel(__global uchar *out,"
                                 "uint height,"
                                 "uint width,"
                                 "double xoffset,"
                                 "double yoffset,"
                                 "double zoom"
")"
"{"
    "//Get the index of the work-item\n"
    "long index = get_global_id(0) * 3;"

    "// Get row, column from index\n"
    "long j = (index / 3) % width;"
    "long i = index / (3 * width);"

    "double y = normalise_int(i, 0, height, -zoom, zoom);"
    "double x = normalise_int(j, 0, width, -zoom, zoom);"
    "int intensity = mandelbrot(x + xoffset, y + yoffset);"

    "// Convert to colourmap\n"
    "uchar r, g, b;"
    "colourmap(intensity, &r, &g, &b);"

    "*(out + index    ) = r;"
    "*(out + index + 1) = g;"
    "*(out + index + 2) = b;"
"}";

cl_uchar pixels[MAX_HEIGHT * MAX_WIDTH * CHANNELS];

double normalise_int(int value, int min, int max, double out_min, double out_max) {
  return (double)(value - min) / (double)(max - min) * (out_max - out_min) + out_min;
}

int main(int argc, char *argv[]) {
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    TTF_Font* font = NULL;
    SDL_Surface *surface;
    SDL_Surface *text_surface;
    SDL_Texture *texture;

    SDL_Color text_colour = {0, 255, 0};

    cl_uint width = 1280;
    cl_uint height = 720;
    cl_double xoffset = 0, yoffset = 0;
    cl_double zoom = 1;
    double fps = 0;
    char fps_text[20] = "fps: ";


    // Get the platform ID and the number of platforms
    cl_platform_id platform_id;
    cl_uint num_platforms;
    cl_int error = clGetPlatformIDs(1, &platform_id, &num_platforms);
    if (error != CL_SUCCESS) {
        printf("Error getting platform ID: %d\n", error);
        return 1;
    }

    // Get the device IDs and the number of devices for each platform
    cl_device_id *device_ids;
    cl_uint num_devices;
    error = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, 0, NULL, &num_devices);
    if (error != CL_SUCCESS) {
    printf("Error getting number of devices: %d\n", error);
    return 1;
    }

    device_ids = malloc(sizeof(cl_device_id) * num_devices);
    error = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_ALL, num_devices, device_ids, NULL);
    if (error != CL_SUCCESS) {
    printf("Error getting device IDs: %d\n", error);
    return 1;
    }

    // Log the device information
    for (int i = 0; i < num_devices; i++) {
        // Get the device name
        size_t size;
        error = clGetDeviceInfo(device_ids[i], CL_DEVICE_NAME, 0, NULL, &size);
        if (error != CL_SUCCESS) {
            printf("Error getting device name size: %d\n", error);
            continue;
        }

        char *name = malloc(size);
        error = clGetDeviceInfo(device_ids[i], CL_DEVICE_NAME, size, name, NULL);
        if (error != CL_SUCCESS) {
            printf("Error getting device name: %d\n", error);
            continue;
        }

        // Log the device name
        printf("Device %d: %s\n", i, name);

        free(name);
    }
    cl_device_id device_id = device_ids[1];

    cl_context context;
    context = clCreateContext( NULL, num_devices, device_ids, NULL, NULL, &error);
    if (error != CL_SUCCESS) {
      printf("Error creating context: %d\n", error);
      return 1;
    }

    cl_command_queue queue = clCreateCommandQueueWithProperties(context, device_id, NULL, &error);
    if (error != CL_SUCCESS) {
        printf("Error creating command queue: %d\n", error);
        return 1;
    }

    // Allocate memory for the data on the GPU
    cl_mem buffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, BUFFER_SIZE, NULL, &error);
    if (error != CL_SUCCESS) {
        printf("Error allocating buffer: %d\n", error);
        return 1;
    }

    // Create a program from the kernel source
    cl_program program = clCreateProgramWithSource(context, 1, &kernel_string, NULL, &error);
    if (error != CL_SUCCESS) {
        printf("Error creating program from kernel source: %d\n", error);
        return 1;
    }

    // Build the program
    error = clBuildProgram(program, num_devices, device_ids, NULL, NULL, NULL);
    if (error != CL_SUCCESS) {
        printf("Error building program: %d\n", error);

        // Display build log
        size_t log_size;
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char *) malloc(log_size);
        clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("%s\n", log);
    }

    // Create the OpenCL kernel object
    cl_kernel kernel = clCreateKernel(program, "mandelbrot_kernel", &error);
    if (error != CL_SUCCESS) {
        printf("Error creating kernel object: %d\n", error);
        return 1;
    }

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
    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

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

        // Set kernel arguments
        error = clSetKernelArg(kernel, 0, sizeof(cl_mem), (void *)&buffer);
        error = clSetKernelArg(kernel, 1, sizeof(cl_uint), (void *)&height);
        error = clSetKernelArg(kernel, 2, sizeof(cl_uint), (void *)&width);
        error = clSetKernelArg(kernel, 3, sizeof(cl_double), (void *)&xoffset);
        error = clSetKernelArg(kernel, 4, sizeof(cl_double), (void *)&yoffset);
        error = clSetKernelArg(kernel, 5, sizeof(cl_double), (void *)&zoom);

        // Queue the job
        size_t global_size = height * width;
        size_t local_size = 128;
        error = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &global_size, &local_size, 0, NULL, NULL);
        if (error != CL_SUCCESS) {
            printf("Error enqueueing job: %d\n", error);
            goto exit;
        }

        // Read (with blocking) from the buffer
        error = clEnqueueReadBuffer(queue, buffer, 1, 0, height * width * CHANNELS, pixels, 0, NULL, NULL);
        if (error != CL_SUCCESS) {
            printf("Error enqueueing read: %d\n", error);
            goto exit;
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
