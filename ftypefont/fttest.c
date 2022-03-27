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
	int x,y;
	int w, h, dx, dy;
	BmFont *font;
	BmFont *bfont_circuit = bm_make_xbm_font(circuit_bits, 7);
	Bitmap *bmp = bm_create(320, 240);
	const char * txt_utf = "Unicode? ひらがな";
	const char* txt_multiline = "Lorem Îpsum\ndolor sit amet";
	const char* txt_centered = "Centered";
	

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

	bm_text_measure(bmp, "Text background", &w, &h, &dx, &dy);
	bm_set_color(bmp, bm_atoi("#FFFFFF"));
	bm_fillrect(bmp, 10 + dx, 30 + dy, 10 + dx + w, 30 + dy + h);
	bm_set_color(bmp, bm_atoi("#F58D31"));
	bm_printf(bmp, 10, 30, "Text background");

	bmft_init();

#ifdef USESDL
	rw = SDL_RWFromFile("arial/arial.ttf", "rb");
	/*rw = SDL_RWFromFile("lcarsfont/lcars.ttf", "rb");*/
	if(!rw){
		fprintf(outfile, "Unable to open font file RW\n");
	}
	font = bmft_load_font_rw(rw, "lcars");
#else
	font = bmft_load_font("arial/arial.ttf");
#endif

	if(!font) {
		fprintf(outfile, "Unable to load font\n");
		return 1;
	}
	bm_set_font(bmp, font);
	
	/* Print UTF-8 encoded text */
	x = 10;
	y = 60;
	bmft_set_size(font, 20);
	bm_printf(bmp, x, y, txt_utf);

	/* Multiline text with bounding box to demonstrate text measurement */
	y += 30;
	bmft_set_size(font, 14);
	bm_text_measure(bmp, txt_multiline, &w, &h, &dx, &dy);
	bm_set_color(bmp, bm_atoi("#FFFFFF"));
	bm_rect(bmp, x + dx, y + dy, x + dx + w, y + dy + h);
	bm_set_color(bmp, bm_atoi("#F58D31"));
	bm_printf(bmp, x, y, txt_multiline);

	/* Display centered text to demonstrate other use of text measurement */
	bmft_set_size(font, 30);
	bm_text_measure(bmp, txt_centered, &w, &h, &dx, &dy);
	x = (bm_width(bmp) - w) / 2;
	y = 160;
	bm_printf(bmp, x, y, txt_centered);

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