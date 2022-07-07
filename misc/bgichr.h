/**
 * # bgichr.h
 *
 * Module to use Borland [BGI][bgi] fonts as `BmFont` objects.
 *
 * It is packaged as a [stb][]-style single header library.
 * To use it, define `BGICHR_IMPLEMENTATION` in _one_ of your
 * C files before including `bgichr.h`:
 *
 * ```
 * #include "bmp.h"
 *
 * #define BGICHR_IMPLEMENTATION
 * #include "bgichr.h"
 * ```
 *
 * By default, the x,y-coordinates of `bm_puts()` and related functions
 * specify the top-left corner of where to draw the font (to be
 * consistent with the other types of `BmFont`). Alternatively you may
 * `#define BGICHR_BASELINE 1` before including `bgichr.h`, which will
 * cause the y-coordinate to be treated as the base (or bottom) of
 * where to draw the font.
 *
 * There is an example at the end of this file.
 *
 * ## API
 *
 * `BmFont *bm_make_bgi_font(const char *filename)`
 *
 * Loads the font in a file named `filename` and returns it as a
 * `BmFont` object.
 *
 * This object should be deallocated after use through
 * `bm_font_release()`.
 *
 * It returns NULL if the font could not be loaded, in which case
 * the specific error can be retrieved through `bm_get_error()`.
 *
 * `void bm_chr_scale(BmFont *font, double scale)`
 *
 * Sets the scale at which the font will be drawn to the factor
 * `scale`.
 *
 * ## References
 *
 * * <https://moddingwiki.shikadi.net/wiki/BGI_Stroked_Font>
 * * <https://www.fileformat.info/format/borland-chr/corion.htm>
 * * You can find some fonts here in this RIPscrip project:
 *   <https://github.com/Kirkman/ripscrip.js/tree/master/fonts>
 *
 * [bgi]: https://en.wikipedia.org/wiki/Borland_Graphics_Interface
 * [stb]: https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
 */

#ifndef BGICHR_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BGICHR_BASELINE
#  define BGICHR_BASELINE 0
#endif

BmFont *bm_make_bgi_font(const char *filename);

void bm_chr_scale(BmFont *font, double scale);

#  ifdef BGICHR_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "bmp.h"

typedef struct {
    int16_t char_count;
    uint8_t starting_char;
    int8_t  orig_to_ascender;
    int8_t  orig_to_baseline;
    int8_t  orig_to_descender;
    uint8_t     *widths;
    uint16_t    *offsets;
    uint8_t     *strokes;
    double scale;
} BgiFont;

static int chr_width(BgiFont *chrfnt, unsigned int codepoint) {
    if(codepoint - chrfnt->starting_char < 0
    || codepoint - chrfnt->starting_char >= chrfnt->char_count)
        return 0;
    return chrfnt->widths[codepoint - chrfnt->starting_char] * chrfnt->scale;
}

static int chr_height(BgiFont *chrfnt) {
    return (chrfnt->orig_to_ascender - chrfnt->orig_to_descender) * chrfnt->scale;
}

static void bgi_draw_char(Bitmap *bmp, BgiFont *font, int xo, int yo, unsigned char chr) {

    double x = 0, y = 0;

    if(chr - font->starting_char < 0
    || chr - font->starting_char >= font->char_count)
        return;

    int off = font->offsets[chr - font->starting_char];

    int8_t x_cmd, y_cmd, code;

    uint8_t *strokes = &font->strokes[off];

    yo -= 64 * font->scale;

    for(;;) {
        x_cmd = *(strokes++);
        y_cmd = *(strokes++);
        code = ((x_cmd >> 6) & 0x02) | ((y_cmd >> 7) & 0x01);

        if(code == 0) break;
        else if(code == 1) {
            /* no op */
        } else {
            if(x_cmd & 0x40)
                x_cmd = -(64 - (x_cmd & 0x3F));
            else
                x_cmd &= 0x3F;
            if(y_cmd & 0x40) {
                y_cmd = -(64 - (y_cmd & 0x3F));
            } else
                y_cmd &= 0x3F;
            y_cmd = 64 - y_cmd;

            double xn = x_cmd * font->scale + xo;
            double yn = y_cmd * font->scale + yo;

            if(code == 2) {
                x = xn;
                y = yn;
            } else {
                bm_line(bmp, x, y, xn, yn);
                x = xn;
                y = yn;
            }
        }
    }
}

static int bgi_puts(Bitmap *b, int x, int y, const char *s) {
    BmFont *font = bm_get_font(b);
    assert(!strcmp(font->type, "BGIFONT"));
    BgiFont *chrfnt = font->data;

    int x0 = x, lw = 0;

#if !BGICHR_BASELINE
    y += chrfnt->orig_to_ascender * chrfnt->scale;
#endif

    while(*s) {
        if(*s == '\n') {
            y += chr_height(chrfnt) + 1;
            x = x0;
        } else if(*s == ' ') {
            x += chr_width(chrfnt, ' ');
        } else if(*s == '\b') {
            /* why would you do this? */
            if(x > x0) x -= lw;
        } else if(*s == '\r') {
            x = x0;
        } else if(*s == '\t') {
            x += 4 * chr_width(chrfnt, ' ');
        } else {
            unsigned char c = *(unsigned char*)s;
            if(c < chrfnt->starting_char || c >= chrfnt->starting_char + chrfnt->char_count)
                c = '*';
            bgi_draw_char(b, chrfnt, x, y, c);
            lw = chr_width(chrfnt, c);
            x += lw;
        }
        s++;
    }
    return 1;
}

static int bgi_width(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "BGIFONT"));
    BgiFont *chrfnt = font->data;
    return chr_width(chrfnt, codepoint);
}

static int bgi_height(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "BGIFONT"));
    (void)codepoint;
    BgiFont *chrfnt = font->data;
    return chr_height(chrfnt);
}

static void bgi_dtor(BmFont *font) {
    if(!font || strcmp(font->type, "BGIFONT"))
        return;
    BgiFont *chrfnt = font->data;
    free(chrfnt->widths);
    free(chrfnt->offsets);
    free(chrfnt->strokes);
    free(chrfnt);
    free(font);
}

BmFont *bm_make_bgi_font(const char *filename) {

    int c;
    int16_t hdr_size;
    char signature[4], font_id[5];
    int16_t data_size, maj_ver, min_ver;
    long size;
    long strokes_size;

    BgiFont *chrfnt = NULL;
    BmFont *font = NULL;

    FILE *f = fopen(filename, "rb");
    if(!f) {
        bm_set_error(strerror(errno));
        return NULL;
    }

    fseek(f,0, SEEK_END);
    size = ftell(f);
    rewind(f);

    if(fread(signature, 4, 1, f) != 1 || memcmp(signature, "PK\x08\x08", 4)) {
        bm_set_error("bad signature");
        fclose(f);
        return NULL;
    }

    while((c = fgetc(f)) != EOF && c != 0x1A);

    if(fread(&hdr_size, sizeof hdr_size, 1, f) != 1
    || fread(font_id, 4, 1, f) != 1
    || fread(&data_size, sizeof data_size, 1, f) != 1
    || fread(&maj_ver, sizeof maj_ver, 1, f) != 1
    || fread(&min_ver, sizeof min_ver, 1, f) != 1) {
        bm_set_error("couldn't read font information");
        fclose(f);
        return NULL;
    }

    font_id[4] = 0;

    fseek(f, hdr_size, SEEK_SET);

#pragma pack(push, 1)
    struct {
        uint8_t stroke_check;
        int16_t char_count;
        uint8_t undefined;
        uint8_t starting_char;
        uint16_t strokes_offset;
        int8_t  scan_flag;
        int8_t  orig_to_ascender;
        int8_t  orig_to_baseline;
        int8_t  orig_to_descender;
    } stroke_hdr;
#pragma pack(pop)

    if(fread(&stroke_hdr, sizeof stroke_hdr, 1, f) != 1 || stroke_hdr.stroke_check != '+') {
        bm_set_error("not a valid stroke file");
        fclose(f);
        return NULL;
    }

    /* The stroke header is padded to be 0x0010 bytes in size */
    fseek(f, hdr_size + 0x0010, SEEK_SET);

    chrfnt = malloc(sizeof *chrfnt);
    if(!chrfnt) {
        bm_set_error("out of memory");
        fclose(f);
        return NULL;
    }

    chrfnt->scale = 1.0;
    chrfnt->char_count = stroke_hdr.char_count;
    chrfnt->starting_char = stroke_hdr.starting_char;
    chrfnt->orig_to_ascender = stroke_hdr.orig_to_ascender;
    chrfnt->orig_to_baseline = stroke_hdr.orig_to_baseline;
    chrfnt->orig_to_descender = stroke_hdr.orig_to_descender;

    chrfnt->offsets = malloc(stroke_hdr.char_count * sizeof chrfnt->offsets[0]);
    chrfnt->widths = malloc(stroke_hdr.char_count * sizeof chrfnt->widths[0]);
    chrfnt->strokes = NULL;
    if(!chrfnt->offsets || !chrfnt->widths) {
        bm_set_error("out of memory");
        goto error;
    }

    if(fread(chrfnt->offsets, sizeof chrfnt->offsets[0], stroke_hdr.char_count, f) != stroke_hdr.char_count) {
        bm_set_error("unable to read offsets");
        goto error;
    }

    if(fread(chrfnt->widths, sizeof chrfnt->widths[0], stroke_hdr.char_count, f) != stroke_hdr.char_count) {
        bm_set_error("unable to read widths");
        goto error;
    }

    strokes_size = size - ftell(f);
    chrfnt->strokes = malloc(strokes_size);
    if(!chrfnt->strokes) {
        bm_set_error("out of memory");
        goto error;
    }

    if(fread(chrfnt->strokes, 1, strokes_size, f) != strokes_size) {
        bm_set_error("unable to read strokes");
        goto error;
    }

    fclose(f);

    font = malloc(sizeof *font);
    if(!font) {
        bm_set_error("out of memory");
        goto error;
    }
    font->type = "BGIFONT";
    font->ref_count = 1;
    font->puts = bgi_puts;
    font->width = bgi_width;
    font->height = bgi_height;
    font->measure = NULL;
    font->dtor = bgi_dtor;
    font->data = chrfnt;

    return font;

error:
    fclose(f);
    free(chrfnt->strokes);
    free(chrfnt->offsets);
    free(chrfnt->widths);
    free(chrfnt);
    return NULL;
}

void bm_chr_scale(BmFont *font, double scale) {
    if(strcmp(font->type, "BGIFONT"))
        return;
    assert(scale > 0);
    BgiFont *chrfnt = font->data;
    chrfnt->scale = scale;
}

#  endif /* BGICHR_IMPLEMENTATION */

#ifdef __cplusplus
} /* extern "C" */
#endif

/**
 * ## Usage example
 *
 * ```
 * #include <stdio.h>
 * #include "bmp.h"
 *
 * //#define BGICHR_BASELINE    1
 *
 * #define BGICHR_IMPLEMENTATION
 * #include "bgichr.h"
 *
 * int main(int argc, char *argv[]) {
 *
 *     Bitmap *bmp = NULL;
 *     char *font_name = "SANS.CHR";
 *
 *     if(argc > 1)
 *      font_name = argv[1];
 *
 *     BmFont *font = bm_make_bgi_font(font_name);
 *     if(!font) {
 *      fprintf(stderr, "Unable to load font %s: %s\n", font_name, bm_get_error());
 *      return 1;
 *     }
 *
 *     bm_chr_scale(font, 1.5);
 *
 *     bmp = bm_create(800, 600);
 *     bm_set_font(bmp, font);
 *
 *     int Y = 80;
 *
 *     bm_set_color(bmp, 0x550000);
 *     bm_line(bmp, 0, Y, 800, Y);
 *     bm_set_color(bmp, 0xFFFFFF);
 *
 *     bm_puts(bmp, 10, Y, "the quick brown fox\njumps over the lazy dog\n"
 *         "THE QUICK BROWN FOX\nJUMPS OVER THE LAZY DOG\n{}[];'\"\xFE\xE4\xAB");
 *
 *     bm_save(bmp, "test.bmp");
 *     bm_free(bmp);
 *
 *     bm_font_release(font);
 *
 *     return 0;
 * }
 * ```
 */

#endif /* BGICHR_H */
