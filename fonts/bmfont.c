/*
 * Font test utility.
 *
 * Compile like so:
 * $ gcc -o bmfont bmfont.c  ../bmp.o `libpng-config --ldflags` -lz -ljpeg
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <unistd.h>

#include "../bmp.h"

extern int bm_to_Xbm(Bitmap *b, const char *name);

static void usage(const char *name) {
	fprintf(stderr, "Usage: %s [options] infile.bmp\n", name);
	fprintf(stderr, "where options:\n");
	fprintf(stderr, " -o outfile      : Name of the output file\n");
	fprintf(stderr, " -s spacing      : Font spacing\n");
	/* fprintf(stderr, " -f fgcolor      : Foreground color\n"); */
	fprintf(stderr, " -b bgcolor      : Background color\n");
}

int main(int argc, char *argv[]) {
	const char *infile, *outfile = "out.gif", *xout = NULL;
	int opt, spacing = 7;
	unsigned int fg = 0xFFFFFF, bg = 0x000055;
	Bitmap *b;

	while((opt = getopt(argc, argv, "o:s:f:b:X:?")) != -1) {
		switch(opt) {
			case 'o' : {
				outfile = optarg;
			} break;
			case 's' : {
				spacing = atoi(optarg);
			} break;
			case 'f' : {
				/* Setting the foreground has no effect because
				you're working with a raster font */
				fg = bm_atoi(optarg);
			} break;
			case 'b' : {
				bg = bm_atoi(optarg);
			} break;
			case 'X' : {
				xout = optarg;
			} break;
			case '?' : {
				usage(argv[0]);
				return 1;
			}
		}
	}

	if(optind >= argc) {
		fprintf(stderr, "error: expected infile\n");
		usage(argv[0]);
		return 1;
	}
	infile = argv[optind++];

	BmFont *font = bm_make_ras_font(infile, spacing);
	if(!font) {
		fprintf(stderr, "error: %s\n", infile);
		return 1;
	}

	b = bm_create(240, 160);
	bm_set_color(b, bg);
	bm_clear(b);
    bm_set_color(b, fg);

	bm_set_font(b, font);

	bm_printf(b, 10, 10, "the quick brown fox jumps\nover the lazy dog");
	bm_printf(b, 10, 30, "THE QUICK BROWN FOX JUMPS\nOVER THE LAZY DOG");
	bm_printf(b, 10, 50, "1234567890!@#$%%^&*()-=_+");
	bm_printf(b, 10, 60, "[](){}:;.,<>/?\\|~'`\"\a\b");

    bm_printf(b, 10, 70, "Hello World! %d", 123);

    bm_set_color(b, 0xFFFFFE);

    bm_save(b, outfile);
	bm_free(b);

	bm_free_font(font);

	if(xout) {
		b = bm_load(infile);
		bm_to_Xbm(b, xout);
		bm_free(b);
	}

	return 0;
}