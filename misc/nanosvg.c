/*
Example of how to use the bitmap headers with
Mikko Mononen's [NanoSVG][] library to load SVG files.

Compile the test program like so:

    $ gcc -Wall -DSVG_TEST misc/nanosvg.c

[NanoSVG]: https://github.com/memononen/nanosvg
*/
#include <stdio.h>

#define BMPH_IMPLEMENTATION
#include "../bmph.h"

#define NANOSVG_IMPLEMENTATION
#include "../3rd-party/nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "../3rd-party/nanosvgrast.h"

#define SVG_UNITS   "px"
#define SVG_DPI     96.0f

Bitmap *bm_load_svg(const char *filename) {
    bm_set_error("no error");

    NSVGimage *image = nsvgParseFromFile(filename, SVG_UNITS, SVG_DPI);
    if(!image) {
        bm_set_error("couldn't load SVG file");
        return NULL;
    }

    int w = image->width, h = image->height;

    NSVGrasterizer *rast = nsvgCreateRasterizer();
    if(!rast) {
        bm_set_error("couldn't create rasterizer");
        return NULL;
    }

    Bitmap *b = bm_create(w, h);

    /* FIXME: The background is supposed to be white. */
    bm_set_color(b, 0xFFFFFF);
    bm_clear(b);

    nsvgRasterize(rast, image, 0,0, b->w/image->width, b->data, b->w, b->h, b->w * 4);

    bm_swap_rb(b);

    nsvgDeleteRasterizer(rast);
    nsvgDelete(image);
    return b;
}

#ifdef SVG_TEST
int main(int argc, char *argv[]) {

    if(argc < 2) {
        fprintf(stderr, "usage: %s infile\n", argv[0]);
        return 1;
    }

    Bitmap *b = bm_load_svg(argv[1]);
    if(!b) {
        fprintf(stderr, "Unable to load %s: %s\n", argv[1], bm_get_error());
        return 1;
    }

    bm_save(b, "svg.bmp");
    bm_free(b);

    return 0;
}
#endif