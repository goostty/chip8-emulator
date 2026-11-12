#define GL_SILENCE_DEPRECATION

#ifdef __MACH__
#include <OpenGL/gl.h>
#elif defined(__linux__)
#include <GL/gl.h>
#endif

#include "chip8.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define STEPS_PER_FRAME 10 // ~600 instructions/sec at 60 Hz
#define FRAME_MS 16

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <rom>\n", argv[0]);
    return EXIT_FAILURE;
  }

  FILE *f = fopen(argv[1], "rb");
  if (!f) {
    perror(argv[1]);
    return EXIT_FAILURE;
  }

  Chip8 *chip = malloc(sizeof(*chip));
  if (!chip) {
    fprintf(stderr, "Out of memory\n");
    fclose(f);
    return EXIT_FAILURE;
  }

  if (chip8_init(chip, f) != 0) {
    free(chip); // chip8_init closes the file
    return EXIT_FAILURE;
  }

  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    free(chip);
    return EXIT_FAILURE;
  }

  // Legacy OpenGL 2.1: glDrawPixels/glPixelZoom are available (deprecated).
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

  GLFWwindow *window =
      glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "CHIP-8", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    free(chip);
    return EXIT_FAILURE;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync
  glfwSetKeyCallback(window, key_callback);

  uint8_t pixels[HEIGHT][WIDTH];
  chip->last_time = get_ticks_ms();

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    chip8_keys_update(chip, window);

    // --- 60 Hz timer tick ---
    uint32_t now = get_ticks_ms();
    chip->accumulator += now - chip->last_time;
    chip->last_time = now;

    if (chip->accumulator > 100)
      chip->accumulator = 100; // avoid spiral of death after a stall

    while (chip->accumulator >= FRAME_MS) {
      tick_timers(chip);
      chip->accumulator -= FRAME_MS;
    }

    // --- CPU ---
    for (int i = 0; i < STEPS_PER_FRAME; ++i)
      chip8_step(chip);

    // --- Render: 0/1 -> 0/255, scaled, vertically flipped ---
    for (int y = 0; y < HEIGHT; ++y)
      for (int x = 0; x < WIDTH; ++x)
        pixels[y][x] = chip->display[y][x] ? 255 : 0;

    // glRasterPos uses object coordinates transformed by the current
    // projection. With the default identity matrices only NDC [-1, 1] is
    // valid, so use glWindowPos (framebuffer pixels) instead. Also query the
    // framebuffer size: on a Retina display it is 2x the window size.
    int fb_width, fb_height;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    glViewport(0, 0, fb_width, fb_height);

    int scale = fb_width / WIDTH;
    if (fb_height / HEIGHT < scale)
      scale = fb_height / HEIGHT;
    if (scale < 1)
      scale = 1;

    int draw_width = WIDTH * scale;
    int draw_height = HEIGHT * scale;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelZoom((float)scale, (float)-scale); // negative Y: row 0 at the top
    glWindowPos2i((fb_width - draw_width) / 2, (fb_height + draw_height) / 2);
    glDrawPixels(WIDTH, HEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, pixels);

    glfwSwapBuffers(window);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  free(chip);

  return EXIT_SUCCESS;
}
