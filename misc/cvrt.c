#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../bmp.h"

void usage(const char *name) {
    fprintf(stderr, "Usage: %s [options] infile outfile\n", name);
    fprintf(stderr, "where options:\n");
    fprintf(stderr, " -w width       : Width of output file\n");
    fprintf(stderr, " -h height      : Height of output file\n");
    fprintf(stderr, " -p percentage  : Resize file by percentage%%\n");
    fprintf(stderr, " -n             : Nearest neighbor resampling\n");
    fprintf(stderr, " -P palettefile : Reduce the bitmap to the colours in the\n");
    fprintf(stderr, "                  palette file.\n");
}

int main(int argc, char *argv[]) {
    Bitmap *b;
    const char *infile, *outfile;
    int opt, ow = -1, oh = -1, nn = 0;
    double perc = -1.0;
    unsigned int *pal = NULL, npal = 0;

    while((opt = getopt(argc, argv, "w:h:np:P:?")) != -1) {
        switch(opt) {
            case 'w' : {
                ow = atoi(optarg);
            } break;
            case 'h' : {
                oh = atoi(optarg);
            } break;
            case 'p' : {
                perc = atof(optarg) / 100.0;
            } break;
            case 'P' : {
                pal = bm_load_palette(optarg, &npal);
                if(!pal) {
                    fprintf(stderr, "Unable to load palette %s\n", optarg);
                    return 1;
                }
            } break;
            case 'n' : {
                nn = 1;
            } break;
            case '?' : {
                usage(argv[0]);
                return 1;
            }
        }
    }

    if(optind > argc - 2) {
        usage(argv[0]);
        return 1;
    }

    infile = argv[optind++];
    outfile = argv[optind++];

    b = bm_load(infile);
    if(!b) {
        fprintf(stderr, "Unable to load %s: %s\n", infile, bm_get_error());
        return 1;
    }

    int iw = bm_width(b);
    int ih = bm_height(b);

    if(perc > 0) {
        ow = iw * perc;
        oh = ih * perc;
    }

    if(ow > 0 || oh > 0) {
        Bitmap *tmp;

        /* Maintain aspect ratio if only new width/height specified */
        if(ow <= 0) ow = iw * oh / ih;
        if(oh <= 0) oh = ih * ow / iw;

        if(nn) {
            tmp = bm_resample(b, ow, oh);
        } else {
            if(ow > iw || oh > ih) {
                /* Use bilinear filtering to make image larger */
                tmp = bm_resample_blin(b, ow, oh);
            } else {
                /* Use bicubic filtering to make image smaller */
                tmp = bm_resample_bcub(b, ow, oh);
            }
        }
        bm_free(b);
        b = tmp;
    }

    if(pal) {
        bm_reduce_palette(b, pal, npal);
        free(pal);
    }

    bm_save(b, outfile);

    bm_free(b);
}
