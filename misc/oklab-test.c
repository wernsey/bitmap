/*
$ gcc misc/oklab-test.c bmp.c -o oklab
*/
#include <stdio.h>
#include <stdlib.h>
#include "../bmp.h"

void print_rgb(rgb_t rgb, const char *prefix) {
  printf("  %s: (%g, %g, %g)\n", prefix, rgb.r, rgb.g, rgb.b);
}

void test(rgb_t srgb, const char *name) {
  printf("Test %s:\n", name);
  print_rgb(srgb, "Input sRGB");
  
  oklab_t o = bm_rgb_to_oklab(srgb);
  printf("  Oklab: (%g, %g, %g)\n", o.L, o.a, o.b);
  
  srgb = bm_oklab_to_rgb(o);
  print_rgb(srgb, "Output sRGB");
}

int main(int argc, char *argv[]) {
  
  if(argc > 3) {
    rgb_t rgb = {
      .r = atof(argv[1]),
      .g = atof(argv[2]),
      .b = atof(argv[3]),
    };
    char name[256];
    snprintf(name, sizeof name, "RGB(%.2g, %.2g, %.2g)", rgb.r, rgb.g, rgb.b);
    test(rgb, name);
  } else if(argc > 1) {
    bm_color_t color = bm_atoi(argv[1]);
    rgb_t rgb = bm_to_rgb(color);
    test(rgb, argv[1]);
  } else {
    test((rgb_t){.r = 1.0, .g = 1.0, .b = 1.0}, "Pure white");
    test((rgb_t){.r = 0.0, .g = 0.0, .b = 0.0}, "Pure black");
    test((rgb_t){.r = 1.0, .g = 0.0, .b = 0.0}, "Intense red");
    test((rgb_t){.r = 0.25, .g = 0.5, .b = 0.75}, "Clean non-neutral color");
  }
  
  return 0;
}