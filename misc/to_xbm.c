/*
 * Converts a bitmap to an XBM
 *
 * $ gcc -o toxbm -DTO_XBM_MAIN to_xbm.c ../bmp.c
 */
#ifdef TO_XBM_MAIN
#include <stdio.h>
#include "../bmp.h"
#endif

int bm_to_Xbm(Bitmap *b, const char *name) {
    int x, y, bit = 0, byte = 0;
    //unsigned int c = bm_get_color(b) & 0x00FFFFFF;
    char fname[30];
    snprintf(fname, sizeof fname, "%s.xbm", name);
    FILE *f = fopen(fname, "wb");
    if(!f) return 0;
    fprintf(f, "#define %s_width  %3d\n", name, b->w);
    fprintf(f, "#define %s_height %3d\n", name, b->h);
    fprintf(f, "static unsigned char %s_bits[] = {\n", name);
    for(y = 0; y < b->h; y++) {
        fputs("  ", f);
        for(x = 0, bit = 0, byte = 0; x < b->w; x++) {
            //if((bm_get(b, x, y) & 0x00FFFFFF) != c)
            if(!(bm_get(b, x, y) & 0x00FFFFFF))
                byte |= (1 << bit);
            if(++bit == 8) {
                if(x == b->w - 1 && y == b->h - 1)
                    fprintf(f, "0x%02x", byte);
                else
                    fprintf(f, "0x%02x,", byte);
                bit = 0;
                byte = 0;
            }
        }
        if(bit) {
            if(y == b->h - 1)
                fprintf(f, "0x%02x\n", byte);
            else
                fprintf(f, "0x%02x,\n", byte);
        } else {
            fputs("\n", f);
        }
    }
    fprintf(f, "};\n");
    return 1;
}

#ifdef TO_XBM_MAIN
int main(int argc, char *argv[]) {
	if(argc != 3) {
		fprintf(stderr, "usage: %s infile outname\n", argv[0]);
		return 1;
	}
	Bitmap *b = bm_load(argv[1]);
	bm_to_Xbm(b, argv[2]);
	bm_free(b);

	return 0;

}
#endif
