#include "chip8.h"
#include <GL/gl.h>
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

  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glfwPollEvents();

    chip8_step(chip);
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
