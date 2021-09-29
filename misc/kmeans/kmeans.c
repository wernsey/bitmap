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

#include "../../bmp.h"

#define MAX_K           256
#define MAX_ITERATIONS  128

#define SHOW_ITERATIONS 0

static int categorize_pixels(unsigned int *bytes, int np, int *cat, unsigned int *pal, unsigned int K) {
    int i, k, change = 0;
    for(i = 0; i < np; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(bytes[i], &iR, &iG, &iB);
        int minD = INT_MAX, dk = 0;
        for(k = 0; k < K; k++) {
            unsigned char pR, pG, pB;
            bm_get_rgb(pal[k], &pR, &pG, &pB);
            int dR = (int)iR - (int)pR;
            int dG = (int)iG - (int)pG;
            int dB = (int)iB - (int)pB;
            int d = dR*dR + dG*dG + dB*dB;
            if(d < minD) {
                minD = d;
                dk = k;
            }
        }
        if(cat[i] != dk)
            change++;
        cat[i] = dk;
    }
    return change;
}

static void adjust(unsigned int *bytes, int np, int *cat, unsigned int *pal, unsigned int K, unsigned int *n) {
    int i, k;

    unsigned int sR[MAX_K], sG[MAX_K], sB[MAX_K];
    for(k = 0; k < K; k++) {
        n[k] = 0; sR[k] = 0; sG[k] = 0; sB[k] = 0;
    }
    for(i = 0; i < np; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(bytes[i], &iR, &iG, &iB);
        k = cat[i];
        n[k]++;
        sR[k] += iR; sG[k] += iG; sB[k] += iB;
    }
    for(k = 0; k < K; k++) {
        if(n[k] == 0) continue;
        sR[k] /= n[k]; sG[k] /= n[k]; sB[k] /= n[k];
        pal[k] = bm_rgb(sR[k], sG[k], sB[k]);
    }
}

BmPalette *cluster(Bitmap *b, unsigned int K) {
    int np = bm_pixel_count(b);
    assert(K <= MAX_K);

    /* I'd really like to get rid of this, but how will you track whether
    there were changes to the categories? */
    int *cat = calloc(np, sizeof *cat);

    unsigned int n[MAX_K];
    unsigned int *bytes = (unsigned int *)bm_raw_data(b);
    int i, k;

    BmPalette *palette = bm_palette_create(K);
    if(!palette)
        return NULL;

    for(k = 0; k < K; k++) {
        /* FIXME: Don't depend on `rand()` */
        palette->colors[k] = bytes[rand() % np];
    }

    for(i = 0; i < MAX_ITERATIONS; i++) {
        int changes = categorize_pixels(bytes, np, cat, palette->colors, K);
        if(!changes)
            break;
        adjust(bytes, np, cat, palette->colors, K, n);
    }
    free(cat);

    return palette;
}

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
        if(K < 2 || K > MAX_K) {
            fprintf(stderr, "error: invalid K value '%s'\n", argv[2]);
            return 1;
        }
    }

    BmPalette *palette = cluster(b, K);

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

    return 0;
}
