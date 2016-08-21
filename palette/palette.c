/*
 * Generate a palette
 *   ./palette -v -s 5 -c 5 -p palette.txt -o test.bmp
 * To reduce an image:
 * To the EGA palette:
 *   ./palette -v -E -r lenna.bmp -o test.bmp
 * To a custom palette:
 *   ./palette -v -p palette.txt -r lenna.bmp -o test.bmp
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#include "../bmp.h"

#define MAX_COLORS		256

#define TOK_SIZE		128

#define SYM_END    		0
#define SYM_COLOR		1

int steps = 3;
int verbose = 0;
int scale = 5;
int cols = -1;

unsigned int colors[MAX_COLORS], ncolors=0;

void add_color(unsigned int c) {
	if(ncolors == MAX_COLORS) {
		fprintf(stderr, "error: MAX_COLORS (%d) exceeded\n", MAX_COLORS);
	}
	if(verbose) {
		printf("colors[%d] => #%06X\n", ncolors, c);
	}
	colors[ncolors++] = c;
}

/* For the parser */
static int sym;
static int line;
static const char *in;
static char token[TOK_SIZE] ;

static int nextsym(void);

static int accept(int s) {
    if (sym == s) {
        nextsym();
        return 1;
    }
    return 0;
}

static int expect(int s) {
    if (accept(s))
        return 1;
    fprintf(stderr, "error: %d: unexpected symbol (%d)\n", line, sym);
    return 0;
}

static int nextsym() {
    /* TODO: Ought to guard against buffer overruns in tok, but not today. */
    char *tok = token;

	sym = SYM_END;
    *tok = '\0';

scan_start:
    while(isspace(*in)) {
		if(*in == '\n')
			line++;
        in++;
	}

    if(!*in)
        return SYM_END;

	if(*in == '%') {
        while(*in && *in != '\n')
            in++;
        goto scan_start;
    }

    if(isalnum(*in) || *in == '#') {
		while(isalnum(*in) || *in == '#')
            *tok++ = *in++;
        *tok = '\0';
        sym = SYM_COLOR;
    } else if(*in == '"') {
		in++;
        while(*in != '"')
            *tok++ = *in++;
        *tok = '\0';
		in++;
        sym = SYM_COLOR;
	} else
        sym = *in++;
    return sym;
}

int parse_palette(const char *text) {
	in = text;
	line = 1;
	nextsym();
	while(!accept(SYM_END)) {
		if(sym == SYM_COLOR) {
			unsigned int c1 = bm_atoi(token);
			nextsym();
			if(sym == '-') {
				nextsym();
				if(sym != SYM_COLOR) {
					fprintf(stderr, "error: %d: color expected\n", line);
					return 0;
				}
				unsigned int i, c2 = bm_atoi(token), st = steps;
				nextsym();
				if(sym == ':') {
					nextsym();
					if(sym != SYM_COLOR) {
						fprintf(stderr, "error: %d: steps expected\n", line);
						return 0;
					}
					st = atoi(token);
					nextsym();
				}
				if(st < 2) st = 2;
				for(i = 0; i < st; i++) {
					double t = (double)(i) / (st - 1);
					unsigned int c = bm_lerp(c1, c2, t);
					add_color(c);
				}
			} else {
				add_color(c1);
			}
		} else {
			expect(SYM_COLOR);
		}
	}
	return 1;
}

int ega_palette[] = {
	0x000000,
	0x0000AA,
	0x555555,
	0x5555FF,
	0x55FFFF,
	0x55FF55,
	0xFF55FF,
	0xFF5555,
	0xFFFFFF,
	0xFFFF55,
	0xAA5500,
	0x00AAAA,
	0x555555,
	0x00AA00,
	0xAAAAAA,
	0xAA00AA,
	0xAA0000,
	0xAAAAAA,
};

int bw_palette[] = {
	0x000000,
	0xFFFFFF,
};

int rgb_palette[] = {
	0x000000,
	0x0000FF,
	0x00FF00,
	0xFF0000,
	0xFFFFFF,
};

int cmyk_palette[] = {
	0x000000,
	0x00FFFF,
	0xFF00FF,
	0xFFFF00,
	0xFFFFFF,
};

/* DawnBringer's 16 color palette
http://www.pixeljoint.com/forum/forum_posts.asp?TID=12795
*/
int db16_palette[] = {
	0x140C1C,
	0x442434,
	0x30346D,
	0x597DCE,
	0xD27D2C,
	0x8595A1,
	0x6DAA2C,
	0x854C30,
	0x346524,
	0xD04648,
	0x757161,
	0xD2AA99,
	0x6DC2CA,
	0xDAD45E,
	0xDEEED6,
};

void load_palette(int palette[], size_t n) {
	size_t i;
	for(i = 0; i < n; i++) {
		add_color(palette[i]);
	}
}

#define LOAD_PALETTE(p) load_palette(p, (sizeof p)/sizeof(int));

void usage(const char *name) {
	printf("usage: %s [options]\n", name);
	printf("where options are:\n");
	printf("Input\n");
	printf(" -p filename    : Load a palette file\n");
	printf(" -a color       : Add a color\n");
	printf(" -r imgfile     : Reduce image file\n");
	printf(" -b imgfile     : Reduce image file with ordered dithering 8x8\n");
	printf("Output\n");
	printf(" -o outfile     : Write output file\n");
	printf(" -s steps       : Number of steps; [default=%d]\n", steps);
	printf(" -c cols        : Number of columns in output image; [default=%d]\n", cols);
	printf(" -v             : Verbose mode\n");
	printf("Preset Palettes\n");
	printf(" -E             : Load EGA palette\n");
	printf(" -B             : Load black and white palette\n");
	printf(" -R             : Load RGB palette\n");
	printf(" -C             : Load CMYK palette\n");
	printf(" -D             : Load DawnBringer's 16 color palette\n");
	printf(" ** Order of the arguments is important **\n");
}

static char *read_file(const char *fname);

extern void bm_reduce_palette_OD8(Bitmap *b, unsigned int palette[], size_t n);

int main(int argc, char *argv[]) {
	int opt;

	Bitmap *bmp = NULL;

	while((opt = getopt(argc, argv, "p:a:r:b:o:s:c:l:vEBRCD?")) != -1) {
		switch(opt) {
			case 'p': {
				char* text = read_file(optarg);
				if(!text) {
					fprintf(stderr, "error: Unable to read %s: %s\n", optarg, strerror(errno));
					return 1;
				}
				parse_palette(text);
				free(text);
			} break;
			case 'a': {
				add_color(bm_atoi(optarg));
			} break;
			case 'r': {
				bmp = bm_load(optarg);
				if(!bmp) {
					fprintf(stderr, "error: Unable to load %s\n", optarg);
				}
				bm_reduce_palette(bmp, colors, ncolors);
			} break;
			case 'b': {
				bmp = bm_load(optarg);
				if(!bmp) {
					fprintf(stderr, "error: Unable to load %s\n", optarg);
				}
				bm_reduce_palette_OD8(bmp, colors, ncolors);
			} break;
			case 'o': {
				if(bmp) {
					if(bm_save(bmp, optarg)) {
						if(verbose) {
							printf("Output written to %s\n", optarg);
						}
					} else {
						fprintf(stderr, "error: Unable to save bitmap to %s\n", optarg);
					}
					bm_free(bmp);
					bmp = NULL;
				} else {
					int x, y;
					int columns = cols;
					if(columns <= 0) {
						/* Try to make the image square... */
						columns = (int)sqrt(ncolors);
						/* ...but still a multiple of steps */
						if(steps > 1) columns -= (columns % steps);
					}
					int rows = ncolors / columns;
					if(ncolors % columns) rows++;
					Bitmap *b = bm_create(columns * scale, rows * scale);
					if(!b) {
						fprintf(stderr, "error: Couldn't create bitmap\n");
						return 1;
					}
					if(verbose) printf("dimensions: %d x %d\n", b->w, b->h);
					for(y = 0; y < b->h; y++) {
						int q = y / scale;
						for(x = 0; x < b->w; x++) {
							int p = x / scale;
							int i = q * columns + p;
							int c = 0;
							if(i < ncolors) c = colors[i];
							bm_set(b, x, y, c);
						}
					}
					if(bm_save(b, optarg)) {
						if(verbose) {
							printf("Output written to %s\n", optarg);
						}
					} else {
						fprintf(stderr, "error: Unable to save bitmap to %s\n", optarg);
					}
					bm_free(b);
				}
			} break;
			case 's' : {
				steps = atoi(optarg);
				if(steps < 2)
					steps = 2;
			} break;
			case 'v' : {
				verbose++;
			} break;
			case 'c' : {
				cols = atoi(optarg);
			} break;
			case 'l' : {
				scale = atoi(optarg);
			} break;
			case 'E' : LOAD_PALETTE(ega_palette); break;
			case 'B' : LOAD_PALETTE(bw_palette); break;
			case 'R' : LOAD_PALETTE(rgb_palette); break;
			case 'C' : LOAD_PALETTE(cmyk_palette); break;
			case 'D' : LOAD_PALETTE(db16_palette); break;
			case '?' : {
				usage(argv[0]);
				return 1;
			}
		}
	}

	return 0;
}

static char *read_file(const char *fname) {
	FILE *f;
	size_t len,r;
	char *bytes;

	if(!(f = fopen(fname, "rb")))
		return NULL;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	rewind(f);

	if(!(bytes = malloc(len+2)))
		return NULL;
	r = fread(bytes, 1, len, f);

	if(r != len) {
		free(bytes);
		return NULL;
	}

	fclose(f);
	bytes[len] = '\0';

	return bytes;
}

