#include <stdio.h>
#include <stdlib.h>
#include "../bmp.h"

#include "bold.xbm"
#include "circuit.xbm"
#include "hand.xbm"
#include "infocom.xbm"
#include "normal.xbm"
#include "small.xbm"
#include "smallinv.xbm"
#include "thick.xbm"
#include "thinsans.xbm"

typedef struct xbm_details {
    const char *name;
    unsigned char *bits;
    int width, height;
} xbm_details;

#define STRUCT_CONTENTS(name) { #name, name ## _bits, name ## _width, name ## _height }

xbm_details xbms[] = {
    STRUCT_CONTENTS(bold),
    STRUCT_CONTENTS(circuit),
    STRUCT_CONTENTS(hand),
    STRUCT_CONTENTS(infocom),
    STRUCT_CONTENTS(normal),
    STRUCT_CONTENTS(small),
    STRUCT_CONTENTS(smallinv),
    STRUCT_CONTENTS(thick),
    STRUCT_CONTENTS(thinsans),
    {NULL, NULL}
};

int main(int argc, char *argv[]) {
    xbm_details *d = xbms;
    while(d->name) {
        char outname[128];
        Bitmap *b;

        snprintf(outname, sizeof outname, "dump-%s.gif", d->name);
        b = bm_from_Xbm(d->width, d->height, d->bits);
        bm_save(b, outname);
        bm_free(b);

        printf("%s %d x %d: %s\n", d->name, d->width, d->height, outname);


        d++;
    }
}