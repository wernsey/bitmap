/*

To compile this:
$ gcc -I .. -I /usr/local/include/freetype2/ -o fttest fttest.c ftfont.c ../bmp.c -lfreetype `libpng-config --ldflags` -lz

Alternatively: $ make ftypefont/fttest
*/

#include <stdio.h>

#if defined(USESDL)
#  include <SDL2/SDL.h>
#endif

#include "bmp.h"
#include "ftfont.h"

#include "fonts/circuit.xbm"

FILE *outfile;

int main(int argc, char *argv[]) {
	int x,w;
	BmFont *font;
	BmFont *bfont_circuit = bm_make_xbm_font(circuit_bits, 7);;
	Bitmap *bmp = bm_create(320, 240);
	const char * str = "Hello World";

#ifdef USESDL
	SDL_RWops *rw;
	outfile = fopen("log.txt", "w");
#else
	outfile = stderr;
#endif

	bm_set_color(bmp, bm_atoi("#6666FF"));
	bm_clear(bmp);
	bm_set_color(bmp, bm_atoi("#F58D31"));

	bm_printf(bmp, 10, 10, "A built-in font");
	bm_set_font(bmp, bfont_circuit);
	bm_printf(bmp, 10, 20, "Another built-in font");

	bmft_init();

	/*font = bmft_load_font("lcarsfont/lcars.ttf");*/
#ifdef USESDL
	rw = SDL_RWFromFile("lcarsfont/lcars.ttf", "rb");
	/*rw = SDL_RWFromFile("lcarsfont/lcars.ttf", "rb");*/
	if(!rw){
		fprintf(outfile, "Unable to open font file RW\n");
	}
	font = bmft_load_font_rw(rw, "lcars");
#else
	font = bmft_load_font("lcarsfont/lcars.ttf");
#endif

	if(!font) {
		fprintf(outfile, "Unable to load font\n");
		return 1;
	}

	bmft_set_size(font, 20);

	bm_set_font(bmp, font);

	w = bm_text_width(bmp, str);
	x = (bm_width(bmp) - w) / 2;

	bm_printf(bmp, x, 120, "%s", str);

	bmft_set_size(font, 30);
	bm_printf(bmp, x, 150, "Lorem Ipsum");

	bm_set_color(bmp, 1);
	bm_save(bmp, "out.gif");

	bm_font_release(bfont_circuit);
	bm_font_release(font);

	bm_free(bmp);

#ifdef USESDL
	fclose(outfile);
#endif
	return 0;
}