/*
 * Median Cut algorithm to quantize the colors in an image
 * https://en.wikipedia.org/wiki/Median_cut
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "../bmp.h"

#define MAX_N 256

int main(int argc, char *argv[]) {

    if(argc < 2) {
        fprintf(stderr, "error: no input file\n");
        return 1;
    }
    Bitmap *o;
    Bitmap *b = bm_load(argv[1]);
    if(!b) {
        fprintf(stderr, "error: unable to load %s\n", argv[1]);
        return 1;
    }

    unsigned int N = 4, i;
    if(argc > 2) {
        N = atoi(argv[2]);
        if(N < 2 || N > MAX_N) {
            fprintf(stderr, "error: invalid N value '%s'\n", argv[2]);
            return 1;
        }
    }

    //unsigned int pal[MAX_N];
    BmPalette *palette = bm_quantize(b, N);

    printf("%d colors\n", bm_palette_count(palette));
    for(i = 0; i < N; i++) {
        printf("#%06X\n", bm_palette_get(palette, i));
    }

    o = bm_copy(b);
    bm_reduce_palette_nearest(o, palette);
    bm_save(o, "final.gif");
    bm_free(o);

    bm_reduce_palette(b, palette);
    bm_save(b, "final-fs.gif");

    o = bm_create(20, 20 * N);
    for(i = 0; i < N; i++) {
        bm_set_color(o, bm_palette_get(palette, i));
        bm_fillrect(o, 0, i * 20, 20, i*20 + 20);
    }
    bm_save(o, "palette.gif");
    bm_free(o);


    bm_free(b);

    return 0;
}