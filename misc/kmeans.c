/*
 * Uses K-means clustering on an image to find the K dominant colors.
 *
 * See https://en.wikipedia.org/wiki/K-means_clustering
 *
 * To compile with MinGW on Windows:
 * $ gcc -o kmeans -Wall -DUSEPNG -DUSEJPG kmeans.c ../../bmp.o -L/usr/local/lib -lpng -lz -ljpeg
 *
 * I've found these links after writing this implementation:
 * - http://stackoverflow.com/q/13637892/115589, which led to:
 * - http://charlesleifer.com/blog/using-python-and-k-means-to-find-the-dominant-colors-in-images/
 * - https://news.ycombinator.com/item?id=4694663 - Interesting discussion on the above.
 *
 * Some things worth mentioning:
 * - The colours in the resulting palette will likely not be in the original image.
 * - The difference in RGB space might not be the best metric wrt human perception of colors.
 *     See https://en.wikipedia.org/wiki/Color_difference and https://en.wikipedia.org/wiki/Lab_color_space
 * - For performance, you might want to scale large images down before running the algorithm.
 *
 * For the iTunes-style use case, you might want to use the first color as the background color,
 * then choose the next color as the one with the highest contrast ratio from the remaining ones.
 * http://stackoverflow.com/q/27869740/115589 http://stackoverflow.com/a/9733420/115589 https://www.w3.org/TR/AERT#color-contrast
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <assert.h>

#include "../bmp.h"

#define SHOW_ITERATIONS 0

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

    srand(time(NULL));

    unsigned int K = 4, i;
    if(argc > 2) {
        K = atoi(argv[2]);
        if(K < 2 || K > 256) {
            fprintf(stderr, "error: invalid K value '%s'\n", argv[2]);
            return 1;
        }
    }

    BmPalette *palette = bm_quantize_kmeans(b, K);
    if(!palette) {
        fprintf(stderr, "couldn't create palette: %s", bm_get_error());
        return 1;
    }

    /* The palette may actually have less than K colors */
    K = bm_palette_count(palette);

    printf("%d colors\n", K);
    for(i = 0; i < K; i++) {
        printf("#%06X\n", bm_palette_get(palette, i));
    }

    o = bm_copy(b);
    bm_reduce_palette_nearest(o, palette);
    bm_save(o, "final.gif");
    bm_free(o);

    bm_reduce_palette(b, palette);
    bm_save(b, "final-fs.gif");

    o = bm_create(20, 20 * K);
    for(i = 0; i < K; i++) {
        bm_set_color(o, bm_palette_get(palette, i));
        bm_fillrect(o, 0, i * 20, 20, i*20 + 20);
    }
    bm_save(o, "palette.gif");
    bm_free(o);

    bm_free(b);
    bm_palette_release(palette);

    return 0;
}
