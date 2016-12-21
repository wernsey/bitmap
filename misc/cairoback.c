/*
 * Example of using the bitmap module as a backend for Cairo...
 * https://cairographics.org
 *
 * See also https://cairographics.org/manual/cairo-Image-Surfaces.html and
 * http://stackoverflow.com/a/24317443/115589
 *
 * On Windows, I can compile it with MinGW like so:
 *   $ gcc -I/local/include cairoback.c ../bmp.c -L/local/lib -lcairo -lpixman-1 -lfreetype -lpng -lz -mwindows
 * Compiling libcairo with MinGW is a different story, but this may help:
 *   https://www.gaia-gis.it/spatialite-3.0.0-BETA/mingw_how_to.html#libcairo
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <cairo/cairo.h>

#include "../bmp.h"

int main(int argc, char *argv[]) {

    Bitmap *b = bm_create(240, 80);

    int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, b->w);
    assert(stride == b->w * 4); /* If this fails, we have a problem... */

    /* Create the Cairo objects over our Bitmap */
    cairo_surface_t *surface = cairo_image_surface_create_for_data(b->data,
        CAIRO_FORMAT_ARGB32, b->w, b->h, stride);
    cairo_t *cr = cairo_create (surface);

    /* Do your Cairo drawing. This is based on the example in https://cairographics.org/FAQ/ */
    cairo_select_font_face (cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size (cr, 32.0);
    cairo_set_source_rgb (cr, 0.2, 1.0, 0.2);
    cairo_move_to (cr, 10.0, 50.0);
    cairo_show_text (cr, "Hello, Cairo");

    /* Done with Cairo... */
    cairo_destroy (cr);
    cairo_surface_destroy (surface);

    /* Done with our Bitmap... */
    bm_save(b, "cairobe.bmp");
    bm_free(b);

    return 0;
}
