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

static int q_sortByR(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x00FF0000) - (*(unsigned int *)qp & 0x00FF0000);
}
static int q_sortByG(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x0000FF00) - (*(unsigned int *)qp & 0x0000FF00);
}
static int q_sortByB(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x000000FF) - (*(unsigned int *)qp & 0x000000FF);
}

static void qrecurse(unsigned int *pixels, int start, int end, int n, unsigned int *pal, int *pindex) {

    int len = end - start, i;

    if(n == 1) {
        unsigned int aR = 0, aG = 0, aB = 0;
        for(i = start; i < end; i++) {
            unsigned char iR, iG, iB;
            bm_get_rgb(pixels[i], &iR, &iG, &iB);
            aR += iR;
            aG += iG;
            aB += iB;
        }
        aR /= len;
        aG /= len;
        aB /= len;

        pal[(*pindex)++] = bm_rgb(aR, aG, aB);
        return;
    }

    int minR = 256, minG = 256, minB = 256;
    int maxR = 0, maxG = 0, maxB = 0;
    for(i = start; i < end; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(pixels[i], &iR, &iG, &iB);
        if(iR < minR) minR = iR;
        if(iR > maxR) maxR = iR;
        if(iG < minG) minG = iG;
        if(iG > maxG) maxG = iG;
        if(iB < minB) minB = iB;
        if(iB > maxB) maxB = iB;
    }
    int spreadR = maxR - minR;
    int spreadG = maxG - minG;
    int spreadB = maxB - minB;

    if(spreadR > spreadG) {
        if(spreadR > spreadB)
            qsort(pixels + start, len, sizeof *pixels, q_sortByR);
        else
            qsort(pixels + start, len, sizeof *pixels, q_sortByB);
    } else {
        if(spreadG > spreadB)
            qsort(pixels + start, len, sizeof *pixels, q_sortByG);
        else
            qsort(pixels + start, len, sizeof *pixels, q_sortByB);
    }

    int mid = (start + end)/2;
    qrecurse(pixels, start, mid, n >> 1, pal, pindex);
    qrecurse(pixels, mid, end, n >> 1, pal, pindex);
}

BmPalette *quantize(Bitmap *b, unsigned int n) {
    BmPalette *palette;
    int w = bm_width(b), h = bm_height(b);

    /* n must be a power of 2, up to 256 */
    assert(n > 1 && n <= 256);
    assert((n != 0) && ((n & (n - 1)) == 0)); // https://stackoverflow.com/a/600306/115589

    unsigned int *data = malloc(w * h * sizeof *data);
    if(!data)
        return 0;

    memcpy(data, bm_raw_data(b), w * h * sizeof *data);

    palette = bm_palette_create(n);
    if(!palette)
        return NULL;

    int pindex = 0;
    qrecurse(data, 0, w*h, n, palette->colors, &pindex);

    free(data);
    return palette;
}

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
    BmPalette *palette = quantize(b, N);

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