#ifdef __MACH__
#include <OpenGL/gl.h>
#elif defined(__linux__)
#include <GL/gl.h>
#endif

#include "chip8.h"
#include <GLFW/glfw3.h>
#include <stdio.h>

int main(int argc, char **argv) {
  const char *path = "src/IBM Logo.ch8";

  // Load ROM into program
  FILE *f = fopen(path, "fb");
  if (!f) {
    fprintf(stderr, "Cannot Open File.");
    return -1;
  }

  // Initialize CHIP8
  Chip8 *chip = malloc(sizeof(Chip8));
  int sig = chip8_init(chip, f);
  // DO NOT CLOSE f here!! already closed in chip8_init

  // Initialize GLFW
  if (!glfwInit()) {
    fprintf(stderr, "Failed to initialize GLFW\n");
    return -1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

  // Create Window
  GLFWwindow *window = glfwCreateWindow(800, 600, "My Window", NULL, NULL);
  if (!window) {
    fprintf(stderr, "Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSetKeyCallback(window, key_callback);

  while (!glfwWindowShouldClose(window)) {
    chip8_step(chip);

    glClear(GL_COLOR_BUFFER_BIT);
    glRasterPos2i(0, 0);
    glDrawPixels(64, 32, GL_LUMINANCE, GL_UNSIGNED_BYTE, chip->display);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
