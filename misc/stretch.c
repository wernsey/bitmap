#include <math.h>
#include <assert.h>

#include "bmp.h"

// $ gcc -Wall -I.. stretch.c ../bmp.c
int main(int argc, char *argv[]) {
    Bitmap *b = bm_load("tile.gif"),
            *out1 = bm_create(100, 100),
            *out2 = bm_create(100, 100);

    BmPoint P[] = {{90, 80},{20, 90},{10, 10},{80, 20}, };
    //BmPoint P[] = {{20, 90},{10, 10},{80, 20},{90, 80}};

    //bm_clip(b, 2, 2, 9, 9);
    //bm_clip(out1, 20, 20, 50, 50);

    bm_stretch(out1, b, P);

    bm_save(out1, "out.gif");

    //bm_clip(out2, 10, 10, 80, 80);
    //bm_clip(out1, 0, 0, 80, 100);
    bm_destretch(out2, out1, P);

    bm_save(out2, "out2.gif");

    bm_free(b);
    bm_free(out1);
    bm_free(out2);
}
