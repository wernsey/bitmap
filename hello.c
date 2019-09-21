#include <stdio.h>

#if 1
#include "bmp.h"
#else
/* Alternatively, there is a stb-style single header: */
#define BMPH_IMPLEMENTATION
#include "bmph.h"
#endif

int main(int argc, char *argv[]) {
    Bitmap *b = bm_create(128,128);

    bm_set_color(b, bm_atoi("white"));
    bm_puts(b, 30, 60, "Hello World");

    bm_save(b, "out.gif");
    bm_free(b);
    return 0;
}
