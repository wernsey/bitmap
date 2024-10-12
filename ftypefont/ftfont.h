/**
 * ftfont.h
 * ========
 #
 * Wrapper around [FreeType](http://www.freetype.org/) to allow rendering of 
 * freetype-supported fonts on `Bitmap`s from my bitmap module in **bmp.h**.
 *
 * All functions in this module is prefixed by `bmft_` for clarity.
 *
 * In order to use this, you must have the development libraries for **freetype**, 
 * **libpng** and **zlib** installed, so you need something like the following 
 * in your Makefile:
 *
 *     CFLAGS = `freetype-config --cflags`
 *     LDFLAGS = `freetype-config --libs` -lpng -lz
 *
 * API 
 * ---
 */

#ifndef FTFONT_H
#define FTFONT_H

#if defined(__cplusplus)
extern "C" {
#endif

/** `int bmft_init()`  \
 * Initializes the FreeType wrapper.  \
 * Returns 1 on success, 0 on failure.
 */
int bmft_init();

/** `BmFont *bmft_load_font(const char *file, const char *name)`  \
 * Loads a font in the file specified by the `file` parameter.  \
 * Returns `NULL` on failure.
 */
BmFont *bmft_load_font(const char *file);

#ifdef USESDL
/** BmFont *bmft_load_font_rw(SDL_RWops *rw, const char *name)
 * If you're using SDL2, you can use the `SDL_RWops` mechanisms
 * to load a font using this function.  \
 * The `name` parameter is used for caching the font.  \
 * Returns `NULL` on failure.
 */
BmFont *bmft_load_font_rw(SDL_RWops *rw, const char *name);
#endif

/** `int bmft_set_size(BmfFont *font, int px)`  \
 * Sets the `size` of a font in pixels.  \
 * Returns 1 on success, 0 on failure.
 */
int bmft_set_size(BmFont *font, int px);

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* FTFONT_H */
