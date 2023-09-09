/**
 * Bitmap API (bmp.h/bmp.c)
 * ========================
 * ![toc-]
 *
 * Low-level routines to manipulate bitmap graphic objects in memory and files on disk.
 *
 * Official repository: <https://github.com/wernsey/bitmap>
 *
 * * It supports BMP, GIF, PCX and TGA files without any third party dependencies.
 * * PNG support is optional through [libpng][]. Use `-DUSEPNG` when compiling.
 * * JPG support is optional through [libjpeg][]. Use `-DUSEJPG` when compiling.
 * * Alternatively, JPG and PNG files can be loaded through [stb_image.h][stb_image].
 *    Put `stb_image.h` in the same directory as `bmp.c` and compile with `-DUSESTB`.
 *
 * [libpng]: http://www.libpng.org/pub/png/libpng.html
 * [libjpeg]: http://www.ijg.org/
 * [stb_image]: https://github.com/nothings/stb/blob/master/stb_image.h
 *
 * License
 * -------
 *
 * ```
 * MIT No Attribution
 *
 * Copyright (c) 2017 Werner Stoop <wstoop@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ```
 *
 * API
 * ---
 */

#ifndef BMP_H
#define BMP_H

#if defined(__cplusplus)
extern "C" {
#endif

/* See `bm_get_error()` for an explaination */
#ifndef BM_LAST_ERROR
#  define BM_LAST_ERROR 1
#endif

/**
 * ### Structures
 */

/**
 * #### `typedef struct bitmap Bitmap;`
 *
 * Structure containing a bitmap image.
 *
 * - Use `bm_width()` and `bm_height()` to get the dimensions of a bitmap
 * - Use `bm_set_color()` and `bm_get_color()` to access the color used when drawing
 * - Use `bm_clip()`, `bm_get_clip()` and `bm_set_clip()` to manipulate the clipping
 *      rectangle used when drawing
 * - Use `bm_set_font()` and `bm_get_font()` to access the font used with `bm_puts()`
 *      and related routines
 * - Use `bm_raw_data()` to access the raw pixel data in the bitmap.
 */
typedef struct bitmap Bitmap;

/**
 * #### `typedef struct BmPoint BmPoint;`
 * A point, with `x` and `y` coordinates
 */
typedef struct BmPoint {
    int x, y;
} BmPoint;

/**
 * #### `typedef struct BmRect BmRect;`
 * Rectangle structure.
 * `(x0,y0)` is inclusive.
 * `(x1,y1)` is exclusive.
 */
typedef struct BmRect {
    int x0, y0;
    int x1, y1;
} BmRect;

/**
 * #### `typedef struct bitmap_font BmFont;`
 *
 * Structure that represents the details about a font.
 *
 * See the section on [Font Routines](#font-routines) for more details.
 *
 * It has these members:
 *
 * * `const char *type` - a text description of the type of font
 * * `ref_count` - a reference count for the font - This should be set
 *      to `1` when the font is created.
 * * `int (*puts)(Bitmap *b, int x, int y, const char *text)` -
 *      Pointer to the function that will actually render the text.
 * * `int (*width)(struct bitmap_font *font)` - Function that returns the
 *      width (in pixels) of a single character in the font.
 * * `int (*height)(struct bitmap_font *font)` - Function that returns the
 *      height (in pixels) of a single character in the font.
 * * `void (*dtor)(struct bitmap_font *font)` - Destructor function that
 *      deallocates all memory allocated to the `BmFont` object.
 * * `void *data` - Additional data that may be required by the font.
 */
typedef struct bitmap_font {
    const char *type;
    unsigned int ref_count;
    int (*puts)(Bitmap *b, int x, int y, const char *text);
    int (*width)(struct bitmap_font *font, unsigned int codepoint);
    int (*height)(struct bitmap_font *font, unsigned int codepoint);
    void (*measure)(struct bitmap_font *font, const char *text, int *w, int* h, int* dx, int* dy);
    void (*dtor)(struct bitmap_font *font);
    void *data;
} BmFont;

/**
 * #### `typedef struct bitmap_palette BmPalette;`
 *
 * Structure that contains a palette.
 *
 * See the section on [Palette Functions](#palette-functions) for more details.
 */
typedef struct bitmap_palette BmPalette;

/**
 * ### Creating and Destroying bitmaps
 */

/**
 * #### `Bitmap *bm_create(int w, int h)`
 *
 * Creates a bitmap of the specified dimensions `w` &times; `h`.
 */
Bitmap *bm_create(int w, int h);

/**
 * #### `void bm_free(Bitmap *b)`
 *
 * Destroys a bitmap previously created with `bm_create()`
 */
void bm_free(Bitmap *b);

/**
 * #### `Bitmap *bm_copy(Bitmap *b)`
 *
 * Creates a duplicate of the bitmap `b`.
 */
Bitmap *bm_copy(Bitmap *b);

/**
 * #### `Bitmap *bm_crop(Bitmap *b, int x, int y, int w, int h)`
 *
 * Crops the bitmap `b` to the region defined by `{x,y,w,h}`
 */
Bitmap *bm_crop(Bitmap *b, int x, int y, int w, int h);

/**
 * #### `Bitmap *bm_from_Xbm(int w, int h, unsigned char *data)`
 *
 * Creates a `Bitmap` object from [XBM data][XBM].
 *
 * The XBM image is imported into a program through a `#include "include.xbm"` directive.
 *
 * The width `w` and height `h` are the `_width` and `_height` variables at the top of the XBM file.
 * The `data` parameter is the `_bits` variable in the XBM file.
 *
 * [XBM]: https://en.wikipedia.org/wiki/X_BitMap
 */
Bitmap *bm_from_Xbm(int w, int h, unsigned char *data);

/**
 * #### `Bitmap *bm_from_Xpm(const char *xpm[])`
 *
 * Creates a `Bitmap` object from [X PixMap](https://en.wikipedia.org/wiki/X_PixMap)
 * data in a source file.
 */
Bitmap *bm_from_Xpm(const char *xpm[]);

/**
 * #### `int bm_width(Bitmap *b)`
 *
 * Retrieves the width of the bitmap `b`
 */
int bm_width(Bitmap *b);

/**
 * #### `int bm_height(Bitmap *b)`
 *
 * Retrieves the height of the bitmap `b`
 */
int bm_height(Bitmap *b);

/**
 * #### `unsigned char *bm_raw_data(Bitmap *b)`
 *
 * Retrieves the raw pixels in the bitmap `b`.
 *
 * The internal format is `0xAARRGGBB` little endian.
 * Meaning that `p[0]` contains B, `p[1]` contains G,
 * `p[2]` contains R and `p[3]` contains A
 * and the data buffer is an array of bytes BGRABGRABGRABGRABGRA...
 */
unsigned char *bm_raw_data(Bitmap *b);

/**
 * #### `int bm_pixel_count(Bitmap *b)`
 *
 * Returns the number of pixels in the bitmap `b`.
 *
 * Essentially `bm_width(b) * bm_height(b)`
 */
int bm_pixel_count(Bitmap *b);

/**
 * ### File I/O Functions
 * These functions are for reading graphics files into a `Bitmap` structure,
 * or for writing a `Bitmap` structure to a file.
 */

/**
 * #### `Bitmap *bm_load(const char *filename)`
 *
 * Loads a bitmap file `filename` into a bitmap structure.
 *
 * It tries to detect the file type from the first bytes in the file.
 *
 * BMP, GIF, PCX and TGA support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load(const char *filename);

/**
 * #### `int bm_loadf(const char *fmt, ...)`
 *
 * Like `bm_load()`, but the filename is given as a `printf()`-style format string.
 */
Bitmap *bm_loadf(const char *fmt, ...);

#ifdef EOF /* <stdio.h> included? http://stackoverflow.com/q/29117606/115589 */
/**
 * #### `Bitmap *bm_load_fp(FILE *f)`
 *
 * Loads a bitmap from a `FILE*` that's already open.
 *
 * BMP, GIF and PCX support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load_fp(FILE *f);
#endif

/**
 * #### `Bitmap *bm_load_mem(const unsigned char *buffer, long len)`
 *
 * Loads a bitmap file from an array of bytes `buffer` of size `len`.
 *
 * It tries to detect the file type from the first bytes in the file.
 *
 * Only supports BMP, GIF, PCX and TGA at the moment.
 * _Don't use it with untrusted input._
 *
 * Returns `NULL` if the file could not be loaded.
 */
Bitmap *bm_load_mem(const unsigned char *buffer, long len);

/**
 * #### `Bitmap *bm_load_base64(const char *base64)`
 *
 * Loads a bitmap file from a [Base64][] encoded string. It uses
 * `bm_load_mem()` internally, so the same caveats apply.
 *
 * Returns `NULL` if the bitmap could not be loaded.
 *
 * [Base64]: https://en.wikipedia.org/wiki/Base64
 */
Bitmap *bm_load_base64(const char *base64);

#if defined(USESDL) && defined(SDL_h_)
/**
 * #### `Bitmap *bm_load_rw(SDL_RWops *file)`
 *
 * Loads a bitmap from a SDL `SDL_RWops*` structure,
 * for use with the [SDL library](http://www.libsdl.org).
 *
 * BMP, GIF and PCX support is always enabled, while JPG and PNG support
 * depends on how the library was compiled.
 *
 * Returns `NULL` if the file could not be loaded.
 *
 * **Note** This function is only available if the `USESDL` preprocessor macro
 * is defined, and `SDL.h` is included before `bmp.h`.
 */
Bitmap *bm_load_rw(SDL_RWops *file);
#endif

#ifdef USESTB
/** #### `Bitmap *bm_load_stb(const char *filename)`
 *
 * Loads a `Bitmap` through the Sean Barrett's [stb_image][]
 * image loader library (currently at v2.16).
 *
 * [stb_image][] provides support for loading JPEG and PNG files without
 * relying on `libjpeg` and `libpng`. It also provides support for other
 * file formats, such as PSD.
 *
 * To use this function, `stb_image.h` must be in the same directory
 * as `bmp.c` and `-D USESTB` must be addeed to your compiler flags.
 */
Bitmap *bm_load_stb(const char *filename);

/** #### `Bitmap *bm_from_stb(int w, int h, unsigned char *data)`
 *
 * Creates a `Bitmap` object from the data returned by one of the
 * `stbi_load*` functions of [stb_image][].
 *
 * The `desired_channels` parameter of the `stbi_load*` function
 * _must_ be set to 4 for this function to work correctly.
 *
 */
Bitmap *bm_from_stb(int w, int h, unsigned char *data);
#endif /* USESTB */

/**
 * #### `int bm_save(Bitmap *b, const char *fname)`
 *
 * Saves the bitmap `b` to a bitmap file named `fname`.
 *
 * If the filename contains `".png"`, `".gif"`, `".pcx"`, `".tga"` or `".jpg"` the file is
 * saved as a PNG, GIF, PCX or JPG, respectively, otherwise the BMP format is the default.
 * It can only save to JPG or PNG if JPG or PNG support is enabled
 * at compile time, otherwise it saves to a BMP file.
 *
 * Returns 1 on success, 0 on failure.
 */
int bm_save(Bitmap *b, const char *fname);

/**
 * #### `int bm_savef(Bitmap *b, const char *fname, ...)`
 *
 * Like `bm_save()`, but the filename is given as a `printf()`-style format string.
 */
int bm_savef(Bitmap *b, const char *fname, ...);

/**
 * #### `int bm_save_custom(Bitmap *b, bm_write_fun fun, void *context, const char *ext)`
 *
 * Saves a bitmap `b` using a custom function `fun` to output the individual bytes.
 *
 * `context` is a pointer to a structure that is passed directly to the callback function to
 * receive the bytes.
 *
 * The `ext` parameter determines the file type: "bmp", "gif", "pcx", "tga", "pbm", "pgm", "png" or "jpg"
 * (PNG and JPG support must be enabled through the `USEPNG` and `USEJPG` preprocessor definitions).
 *
 * The custom function `fun` has this prototype:
 *
 * ```
 * int (*bm_write_fun)(void *data, int len, void *context);
 * ```
 *
 * * `data` is the bytes to write,
 * * `len` is the number of bytes to write, and
 * * `context` is the pointer passed through directly from `bm_save_custom()`
 *
 * The `bm_write_fun` should return 1 on success, 0 on failure.
 */
typedef int (*bm_write_fun)(void *data, int len, void *context);

int bm_save_custom(Bitmap *b, bm_write_fun fun, void *context, const char *ext);

/**
 * ### Reference Counting Functions
 *
 * These functions implement reference counting on `Bitmap` objects.
 *
 * Call `bm_retain()` on a bitmap at every location where a reference is held.
 * Then call `bm_release()` on the bitmap when those references are no longer being held.
 * When the last reference is released, the bitmap will be freed automatically.
 *
 * _Reference counting is optional:_ Bitmap objects are created with a reference
 * count of zero to indicate they are not managed by the reference counter, and those
 * should be destroyed through `bm_free()`.
 */

/**
 * #### `Bitmap *bm_retain(Bitmap *b)`
 *
 * Increments the reference count of a `Bitmap` object `b`.
 *
 * It returns the object.
 */
Bitmap *bm_retain(Bitmap *b);

/**
 * #### `void bm_release(Bitmap *b)`
 *
 * Decrements the reference count of a `Bitmap` object.
 *
 * If the reference count reaches 0, `bm_free()` is called on
 * the object, and the pointer is no longer valid.
 */
void bm_release(Bitmap *b);

/**
 * ### Binding Functions
 * These functions are used to bind a `Bitmap` structure to
 * an existing memory buffer such as an OpenGL texture, an
 * SDL Surface or a Win32 GDI context.
 */

/**
 * #### `Bitmap *bm_bind(int w, int h, unsigned char *data)`
 *
 * Creates a bitmap structure bound to an existing array
 * of pixel data (for example, an OpenGL texture or a SDL surface). The
 * `data` must be an array of `w` &times; `h` &times; 4 bytes of ARGB pixel data.
 *
 * ~~The returned `Bitmap*` must be destroyed with `bm_unbind()`
 * rather than `bm_free()`.~~ In the latest versions, `bm_unbind()` just calls
 * `bm_free()`
 */
Bitmap *bm_bind(int w, int h, unsigned char *data);

/**
 * #### `void bm_rebind(Bitmap *b, unsigned char *data)`
 *
 * Changes the data referred to by a bitmap structure previously
 * created with a call to `bm_bind()`.
 *
 * The new data must be of the same dimensions as specified
 * in the original `bm_bind()` call.
 */
void bm_rebind(Bitmap *b, unsigned char *data);

/**
 * #### `void bm_unbind(Bitmap *b)`
 *
 * Deallocates the memory of a bitmap structure previously created
 * through `bm_bind()`.
 *
 * **Deprecated** - in the newest versions, this function just
 * calls `bm_free()`
 */
void bm_unbind(Bitmap *b);

#if defined(USESDL) && defined(SDL_h_)
/**
 * #### `SDL_Texture *bm_create_SDL_texture(Bitmap *b, SDL_Renderer *renderer)`
 *
 * Creates a
 *
 * **Note** This function is only available if the `USESDL` preprocessor macro
 * is defined, and `SDL.h` is included before `bmp.h`.
 */
SDL_Texture *bm_create_SDL_texture(Bitmap *b, SDL_Renderer *renderer);
#endif

/**
 * ### Clipping and Buffer Manipulation Functions
 */

/**
 * #### `void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Sets the clipping rectangle on the bitmap from `x0,y0` (inclusive)
 * to `x1,y1` exclusive.
 */
void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_unclip(Bitmap *b)`
 *
 * Resets the bitmap `b`'s clipping rectangle.
 */
void bm_unclip(Bitmap *b);

/**
 * #### `int bm_inclip(Bitmap *b, int x, int y)`
 *
 * Tests whether the point `x,y` is in the bitmap `b`'s
 * clipping region. Returns non-zero if it is, zero if it isn't.
 */
int bm_inclip(Bitmap *b, int x, int y);

/**
 * #### `BmRect bm_get_clip(Bitmap *b)`
 *
 * Retrieves bitmap `b`'s clipping rectangle.
 */
BmRect bm_get_clip(Bitmap *b);
/**
 * #### `void bm_set_clip(Bitmap *b, const BmRect rect)`
 *
 * Sets bitmap `b`'s clipping rectangle to `rect`.
 */
void bm_set_clip(Bitmap *b, const BmRect rect);

/**
 * #### `void bm_flip_vertical(Bitmap *b)`
 *
 * Flips the bitmap vertically.
 */
void bm_flip_vertical(Bitmap *b);

/**
 * ### Pixel Functions
 */

/**
 * #### `unsigned int bm_get(Bitmap *b, int x, int y)`
 *
 * Retrieves the value of the pixel at `x,y` as an integer.
 *
 * The return value is in the form `0xAABBGGRR`
 */
unsigned int bm_get(Bitmap *b, int x, int y);

/**
 * #### `void bm_set(Bitmap *b, int x, int y, unsigned int c)`
 *
 * Sets the value of the pixel at `x,y` to the color `c`.
 *
 * `c` is in the form `0xAABBGGRR`.
 */
void bm_set(Bitmap *b, int x, int y, unsigned int c);

/**
 * ### Color functions
 * Functions for manipulating colors in the image.
 */

/**
 * #### `void bm_set_color(Bitmap *bm, unsigned int col)`
 *
 * Sets the color of the pen to a color represented
 * by an integer, like `0xAARRGGBB`
 */
void bm_set_color(Bitmap *bm, unsigned int col);

/**
 * #### `unsigned int bm_get_color(Bitmap *bc)`
 *
 * Retrieves the pen color.
 */
unsigned int bm_get_color(Bitmap *bm);

/**
 * #### `void bm_set_alpha(Bitmap *bm, int a)`
 *
 * Sets the alpha value of the pen to `a`
 */
void bm_set_alpha(Bitmap *bm, int a);

/**
 * #### `unsigned int bm_picker(Bitmap *bm, int x, int y)`
 *
 * Sets the color of the pen to the color of the pixel at <x,y>
 * on the bitmap.
 *
 * The pen color can then be retrieved through `bm_get_color()`.
 *
 * It returns the integer representation of the color.
 */
unsigned int bm_picker(Bitmap *bm, int x, int y);

/**
 * #### `unsigned int bm_atoi(const char *text)`
 *
 * Converts a text string like "#FF00FF" or "white" to
 * an integer of the form `0xFF00FF`.
 * The `text` parameter is not case sensitive and spaces are
 * ignored, so for example "darkred" and "Dark Red" are equivalent.
 *
 * The shorthand "#RGB" format is also supported
 * (eg. "#0fb", which is the same as "#00FFBB").
 *
 * Additionally, it also supports the CSS syntax for "RGB(r,g,b)",
 * "RGBA(r,g,b,a)", "HSL(h,s,l)" and "HSLA(h,s,l,a)".
 *
 * The list of supported colors are based on Wikipedia's
 * list of HTML and X11 [Web colors](http://en.wikipedia.org/wiki/Web_colors).
 *
 * It returns 0 (black) if the string couldn't be parsed.
 */
unsigned int bm_atoi(const char *text);

/**
 * #### `unsigned int bm_rgb(unsigned char R, unsigned char G, unsigned char B)`
 *
 * Builds a color from the specified `(R,G,B)` values
 */
unsigned int bm_rgb(unsigned char R, unsigned char G, unsigned char B);

/**
 * #### `unsigned int bm_rgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A)`
 *
 * Builds a color from the specified `(R,G,B,A)` values
 */
unsigned int bm_rgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A);

/**
 * #### `void bm_get_rgb(unsigned int col, unsigned char *R, unsigned char *G, unsigned char *B)`
 *
 * Decomposes a color `col` into its `R,G,B` components.
 */
void bm_get_rgb(unsigned int col, unsigned char *R, unsigned char *G, unsigned char *B);

/**
 * #### `unsigned int bm_hsl(double H, double S, double L)`
 *
 * Creates a color from the given Hue/Saturation/Lightness values.
 * See <https://en.wikipedia.org/wiki/HSL_and_HSV> for more information.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) are given as percentages from 0 to 100%.
 */
unsigned int bm_hsl(double H, double S, double L);

/**
 * #### `unsigned int bm_hsla(double H, double S, double L, double A)`
 *
 * Creates a color from the given Hue/Saturation/Lightness and alpha values.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) and Alpha (`A`) are given as percentages from 0 to 100%.
 */
unsigned int bm_hsla(double H, double S, double L, double A);

/**
 * #### `bm_get_hsl(unsigned int col, double *H, double *S, double *L)`
 *
 * Decomposes a color `col` into its Hue/Saturation/Lightness components.
 *
 * Hue (`H`) is given as an angle in degrees from 0&deg; to 360&deg;.
 * Saturation (`S`) and Lightness (`L`) are given as percentages from 0 to 100%.
 */
void bm_get_hsl(unsigned int col, double *H, double *S, double *L);

/**
 * #### `int bm_colcmp(unsigned int c1, unsigned int c2)`
 *
 * Compares the RGB values of two colors, ignoring the alphas values
 * (If the alpha values are important you can just use `==`).
 *
 * Returns non-zero if the RGB values of `c1` and `c2` are the same,
 * zero otherwise.
 */
int bm_colcmp(unsigned int c1, unsigned int c2);

/**
 * #### `unsigned int bm_byte_order(unsigned int col)`
 *
 * Fixes the input color to be in the proper byte order.
 *
 * The input color should be in the format `0xAARRGGBB`. The output
 * will be in either `0xAARRGGBB` or `0xAABBGGRR` depending on how the
 * library was compiled.
 */
unsigned int bm_byte_order(unsigned int col);

/**
 * #### `unsigned int bm_lerp(unsigned int color1, unsigned int color2, double t)`
 *
 * Computes the color that is a distance `t` along the line between
 * `color1` and `color2`.
 *
 * If `t` is 0 it returns `color1`. If `t` is 1.0 it returns `color2`.
 */
unsigned int bm_lerp(unsigned int color1, unsigned int color2, double t);

/**
 * #### `unsigned int bm_graypixel(unsigned int c)`
 *
 * Converts a color to its grayscale value.
 *
 * See <https://en.wikipedia.org/wiki/Grayscale>
 */
unsigned int bm_graypixel(unsigned int c);

/**
 * #### `void bm_swap_color(Bitmap *b, unsigned int src, unsigned int dest)`
 *
 * Replaces all pixels of color `src` in bitmap `b` with the color `dest`.
 */
void bm_swap_color(Bitmap *b, unsigned int src, unsigned int dest);

/**
 * #### `Bitmap *bm_swap_rb(Bitmap *b)`
 *
 * Swaps the Red and Blue channels in a bitmap.
 *
 * (It is meant for certain use cases where a buffer is BGRA instead of RGBA)
 */
Bitmap *bm_swap_rb(Bitmap *b);

/**
 * ### Blitting Functions
 */

/**
 * #### `void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)`
 *
 * Blits an area of `w` &times; `h` pixels at `sx,sy` on the source bitmap `src` to
 * `dx,dy` on the destination bitmap `dst`.
 */
void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/**
 * #### `void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h)`
 *
 * Blits an area of `w` &times; `h` pixels at `sx,sy` on the `src` bitmap to
 * `dx,dy` on the `dst` bitmap.
 *
 * Pixels on the `src` bitmap that matches the `src` bitmap color are not blitted.
 * The alpha value of the pixels on the `src` bitmap is not taken into account.
 */
void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h);

/**
 * #### `void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask)`
 *
 * Extended blit function. Blits an area of `sw` &times; `sh` pixels at `sx,sy` from the `src` bitmap to
 * `dx,dy` on the `dst` bitmap into an area of `dw` &times; `dh` pixels, stretching or shrinking the blitted area as neccessary.
 *
 * If `mask` is non-zero, pixels on the `src` bitmap that matches the `src` bitmap color are not blitted.
 * Whether the alpha value of the pixels is taken into account depends on whether `IGNORE_ALPHA` is enabled.
 */
void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask);

/**
 * #### `void bm_blit_callback(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_sampler_function fun)`
 *
 * Blits a source bitmap to a destination similar to `bm_blit_ex()`, except
 * that it calls a callback function for every pixel
 *
 * The callback function takes this form:
 *
 *     typedef unsigned int (*bm_sampler_function)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color)
 *
 * Where
 *
 * * `dst` is the destination bitmap.
 * * `dx`,`dy` is the pixel coordinates on the destination being blitted to.
 * * `src` is the source bitmap being sampled from.
 * * `sx`,`sy` is the coordinates of pixel on the source bitmap being sampled
 * * `dest_color` is the current color of `dx`,`dy` on the destination, useful for blending.
 *
 * It will set the clipping region on `src` to the area defined by `sx,sy,sw,sh`
 * before calling the callback, so that the callback can rely on it (The
 * clipping region will be restored afterwards).
 */
typedef unsigned int (*bm_sampler_function)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);

void bm_blit_callback(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_sampler_function fun);

/**
 * Some built-in sampling functions for use with `bm_blit_callback()`:
 *
 * - `bm_smp_outline` - Highlights the outline of the `src` bitmap using
 *    the `dst->color`.
 * - `bm_smp_border` - Highlights the border of the `src` bitmap using
 *    the `dst->color`.
 * - `bm_smp_binary` - If the pixel on `src` matches `src->color`, set the
 *    pixel on `dst` to `dst->color`, otherwise leave it blank.
 * - `bm_smp_blend50` - Uses a bit shift trick to do a 50/50 blend between
 *    the source and destination pixels
 */
unsigned int bm_smp_outline(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);
unsigned int bm_smp_border(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);
unsigned int bm_smp_binary(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);
unsigned int bm_smp_blend50(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);

/**
 * #### `void bm_rotate_blit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale);`
 *
 * Rotates a source bitmap `src` around a pivot point `px,py` and blits it onto a destination bitmap `dst`.
 *
 * The bitmap is positioned such that the point `px,py` on the source is at the offset `ox,oy` on the destination.
 *
 * The `angle` is clockwise, in radians. The bitmap is also scaled by the factor `scale`.
 */
void bm_rotate_blit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale);

/**
 * #### `void bm_rotate_maskedblit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale);`
 *
 * Rotates a source bitmap `src` around a pivot point `px,py` and blits it onto a destination bitmap `dst`.
 *
 * The bitmap is positioned such that the point `px,py` on the source is at the offset `ox,oy` on the destination.
 *
 * The `angle` is clockwise, in radians. The bitmap is also scaled by the factor `scale`.
 *
 * Pixels on the `src` bitmap that matches the `src` bitmap color are not blitted.
 * The alpha value of the pixels on the `src` bitmap is not taken into account.
 */
void bm_rotate_maskedblit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale);

/**
 * #### `void bm_stretch(Bitmap *dst, Bitmap *src, BmPoint P[4])`
 *
 * Stretches a bitmap `src` onto the quadrilateral defined by the four points `P`
 * on the destination `dst`.
 *
 * The clipping rectangle of `src` controls the region of `src` that should be stretched
 * onto `dst`. It won't draw anything outside of `dst`'s clipping region.
 *
 * Vertices in `P` are in clockwise order. `P[0]` corresponds to the top left, `P[1]` to
 * the top right, `P[2]` to the bottom right and `P[3]` to the bottom left of `src`.
 */
void bm_stretch(Bitmap *dst, Bitmap *src, BmPoint P[4]);

/**
 * #### `void bm_destretch(Bitmap *dst, Bitmap *src, BmPoint P[4])`
 *
 * Fits the quadrilateral defined by the four points `P` on the bitmap `src` into the
 * destination bitmap `dst`.
 *
 * It is the inverse operation of `bm_stretch()`.
 *
 * The clipping rectangle of `dst` defines the region into which the quadrilateral should be mapped.
 * Pixels outside of `src`'s clipping rectangle won't be mapped.
 *
 * Vertices in `P` are in clockwise order. `P[0]` corresponds to the top left, `P[1]` to
 * the top right, `P[2]` to the bottom right and `P[3]` to the bottom left of `dst`.
 */
void bm_destretch(Bitmap *dst, Bitmap *src, BmPoint P[4]);

/**
 * #### `void bm_blit_xbm(Bitmap *dst, int dx, int dy, int sx, int sy, int w, int h, int xbm_w, int xbm_h, unsigned char xbm_data[]);`
 *
 * Blits an area of `w` &times; `h` pixels at `sx,sy` in [XBM image data][XBM] to
 * `dx,dy` on the destination bitmap `dst`.
 *
 * It uses the color of `dst` as the foreground. Backdround pixels are unchanged.
 *
 * `xbm_w` and `xbm_h` is the width and height of the XBM image respectively. `xbm_data` is the XBM bytes.
 */
void bm_blit_xbm(Bitmap *dst, int dx, int dy, int sx, int sy, int w, int h, int xbm_w, int xbm_h, unsigned char xbm_data[]);

/**
 * ### Filter Functions
 */

/** #### `void bm_grayscale(Bitmap *b)`
 *
 * Converts an image to grayscale.
 */
void bm_grayscale(Bitmap *b);

/**
 * #### `void bm_smooth(Bitmap *b)`
 *
 * Smoothes the bitmap `b` by applying a 5&times;5 Gaussian filter.
 */
void bm_smooth(Bitmap *b);

/**
 * #### `void bm_apply_kernel(Bitmap *b, int dim, float kernel[])`
 *
 * Applies a `dim` &times; `dim` kernel to the image.
 *
 * ```
 * float smooth_kernel[] = { 0.0, 0.1, 0.0,
 *                           0.1, 0.6, 0.1,
 *                           0.0, 0.1, 0.0};
 * bm_apply_kernel(screen, 3, smooth_kernel);
 * ```
 */
void bm_apply_kernel(Bitmap *b, int dim, float kernel[]);

/**
 * #### `Bitmap *bm_resample(const Bitmap *in, int nw, int nh)`
 *
 * Creates a new bitmap of dimensions `nw` &times; `nh` that is a scaled
 * using the Nearest Neighbour method the input bitmap.
 *
 * The input bimap remains untouched.
 */
Bitmap *bm_resample(const Bitmap *in, int nw, int nh);

/**
 * #### `Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh)`
 *
 * Creates a new bitmap of dimensions `nw` &times; `nh` that is a scaled
 * using Bilinear Interpolation from the input bitmap.
 *
 * The input bimap remains untouched.
 *
 * _Bilinear Interpolation is better suited for making an image larger._
 */
Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh);

/**
 * #### `Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh)`
 *
 * Creates a new bitmap of dimensions `nw` &times; `nh` that is a scaled
 * using Bicubic Interpolation from the input bitmap.
 *
 * The input bimap remains untouched.
 *
 * _Bicubic Interpolation is better suited for making an image smaller._
 */
Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh);

/**
 * #### `Bitmap *bm_resample_into(const Bitmap *in, Bitmap *out)`
 *
 * Resamples a bitmap `in` to fit into a bitmap `out` using nearest neighbour.
 */
Bitmap *bm_resample_into(const Bitmap *in, Bitmap *out);

/**
 * #### `Bitmap *bm_resample_blin_into(const Bitmap *in, Bitmap *out)`
 *
 * Resamples a bitmap `in` to fit into a bitmap `out` using bilinear interpolation.
 */
Bitmap *bm_resample_blin_into(const Bitmap *in, Bitmap *out);

/**
 * #### `Bitmap *bm_resample_bcub_into(const Bitmap *in, Bitmap *out)`
 *
 * Resamples a bitmap `in` to fit into a bitmap `out` using bicubic interpolation.
 */
Bitmap *bm_resample_bcub_into(const Bitmap *in, Bitmap *out);

/**
 * #### `Bitmap *bm_rotate_cw(const Bitmap *in)`
 *
 * Creates a new bitmap from `in` that is rotated 90&deg; clockwise.
 *
 * It takes the clipping region of `in` into account.
 */
Bitmap *bm_rotate_cw(const Bitmap *in);

/**
 * #### `Bitmap *bm_rotate_ccw(const Bitmap *in)`
 *
 * Creates a new bitmap from `in` that is rotated 90&deg; counter-clockwise.
 *
 * It takes the clipping region of `in` into account.
 */
Bitmap *bm_rotate_ccw(const Bitmap *in);

/**
 * ### Palette Functions
 *
 * `bmp.h` provides these methods for manipulating color palettes.
 */

/**
 * #### `BmPalette *bm_palette_create(unsigned int ncolors)`
 *
 * Creates a palette object with space for `ncolors`.
 *
 * This palette will be used to reduce the colors in the image when
 * saving to an 8-bit format like GIF or PCX.
 *
 * The reference count of the palette is set to `1` initially.
 * Call `bm_palette_release()` when the palette is no longer in use
 * which will destro.
 *
 * It returns `NULL` on error.
 */
BmPalette *bm_palette_create(unsigned int ncolors);

/**
 * #### `void bm_set_palette(Bitmap *b, BmPalette *pal)`
 *
 * Associates a palette `pal` with the bitmap `b`.
 *
 * `pal` may be `NULL` to dissociate a palette with the bitmap.
 *
 * It will increment the reference count of `pal` and decrement
 * the reference counts of the previous palette assigned so that
 * the palette's memory is reclaimed when it is no longer in use.
 */
void bm_set_palette(Bitmap *b, BmPalette *pal);

/**
 * #### `BmPalette *bm_get_palette(const Bitmap *b)`
 *
 * Retrieves the palette object associated with a bitmap `b`.
 *
 * It may return `NULL` if the bitmap does not have a palette
 * associated with it.
 *
 * It does not change the reference count of the palette object,
 * so if you want to hang on to this pointer you need to call
 * `bm_palette_retain()` on it and then call `bm_palette_release()`
 * when you're done.
 */
BmPalette *bm_get_palette(const Bitmap *b);

/**
 * #### `BmPalette *bm_palette_retain(BmPalette *pal)`
 *
 * Increments a palette's reference counter.
 */
BmPalette *bm_palette_retain(BmPalette *pal);

/**
 * #### `unsigned int bm_palette_release(BmPalette *pal)`
 *
 * Decrements a palette's reference counter, and if it's 0, destroys it.
 *
 * It returns the reference count of the palette, so 0 means the palette was destroyed.
 */
unsigned int bm_palette_release(BmPalette *pal);

/**
 * #### `int bm_make_palette(Bitmap *b)`
 *
 * Generates a palette for a bitmap `b`:
 *
 * * If `b` has 256 colors or less, then the generated palette
 *   contains only those colors.
 * * If `b` has more than 256 colors, then a palette will be created
 *   through `bm_quantize_uniform()`
 *
 * The generated palette can be retrieved through `bm_get_palette()`
 */
int bm_make_palette(Bitmap *b);

/**
 * #### `int bm_palette_count(BmPalette *pal)`
 *
 * Returns the number of colors in a palette `pal`
 */
int bm_palette_count(BmPalette *pal);

/**
 * #### `int bm_palette_add(BmPalette *pal, unsigned int color)`
 *
 * Adds a new color `color` to the palette `pal`.
 *
 * The new color is added to the end of the internal list of colors.
 *
 * It returns the index of the added color.
 */
int bm_palette_add(BmPalette *pal, unsigned int color);

/**
 * #### `int bm_palette_set(BmPalette *pal, int index, unsigned int color)`
 *
 * Changes the color in the palette `pal` at position `index` to the value `color`
 *
 * It returns the index, or -1 if the index is invalid.
 */
int bm_palette_set(BmPalette *pal, int index, unsigned int color);

/**
 * #### `unsigned int bm_palette_get(BmPalette *pal, int index)`
 *
 * Retrieves the color at position `index` in the palette `pal`.
 *
 * It returns black (#000000) if the index is invalid.
 */
unsigned int bm_palette_get(BmPalette *pal, int index);

/**
 * #### `unsigned int bm_palette_nearest_index(BmPalette *pal, unsigned int color)`
 *
 * Finds the index of the color in the palette `pal` that is closest to `color`.
 */
unsigned int bm_palette_nearest_index(BmPalette *pal, unsigned int color);

/**
 * #### `unsigned int bm_palette_nearest_color(BmPalette *pal, unsigned int color)`
 *
 * Finds the color in the palette `pal` that is closest to `color`.
 *
 * It is functionally equivalent to `bm_palette_get(pal, bm_palette_nearest_index(pal, color));`
 */
unsigned int bm_palette_nearest_color(BmPalette *pal, unsigned int color);

/**
 * #### `BmPalette *bm_load_palette(const char * filename)`
 *
 * Loads a palette from a file named `filename`.
 *
 * It returns a `BmPalette` pointer, or `NULL` on error.
 *
 * The returned pointer must be `bm_palette_release()`ed after use.
 *
 * These formats are supported:
 *
 * - If the first line in the file is `JASC-PAL`, the file is read as a
 *   [Paintshop Pro-type palette][Psp-pal].
 * - If the first line in the file is `GIMP Palette`, the file is read as a
 *   [GIMP palette][gimp-pal].
 * - Otherwise the file is read as a text file with a colour on each
 *   line. Blank lines are ignored and semicolons indicate comments.
 *   The format is similar to [Paint.NET palette files][Pdn-pal] except
 *   the colours can be specified in any format supported by `bm_atoi()`,
 *   and up to 256 colours can be defined.
 *
 * [Psp-pal]: http://www.cryer.co.uk/file-types/p/pal.htm
 * [gimp-pal]: https://docs.gimp.org/en/gimp-concepts-palettes.html
 * [Pdn-pal]: https://www.getpaint.net/doc/latest/WorkingWithPalettes.html
 */
BmPalette *bm_load_palette(const char * filename);

/**
 * #### `int bm_save_palette(BmPalette *pal, const char* filename)`
 *
 * Saves a palette `pal` to a file named `filename`.
 *
 * The file is always saved in the [Paintshop Pro palette format][Psp-pal].
 *
 * Returns 1 on success, 0 on failure.
 */
int bm_save_palette(BmPalette *pal, const char* filename);

/**
 * #### `BmPalette *bm_quantize(Bitmap *b, unsigned int n)`
 *
 * Creates a palette of `n` colors from the bitmap `b` using the
 * [Median Cut](https://en.wikipedia.org/wiki/Median_cut)
 * algorithm to choose the best colors.
 */
BmPalette *bm_quantize(Bitmap *b, int n);

/**
 * #### `BmPalette *bm_quantize_kmeans(Bitmap *b, unsigned int K)`
 *
 * Creates a palette of `K` colors from the bitmap `b` using the
 * [K-means clustering](https://en.wikipedia.org/wiki/K-means_clustering)
 * algorithm to choose the best colors.
 *
 * The colors in the palette are sorted so that the most common one
 * is first, the second most common one is next and so on. This
 * could be useful if you need to find dominant colors in an image.
 *
 * This implementation is quite slow for larger values of K, so the
 * other algorithms might be better if you don't need the sorting
 * functionality.
 */
BmPalette *bm_quantize_kmeans(Bitmap *b, int K);

/**
 * #### `BmPalette *bm_quantize_uniform(Bitmap *b, int K)`
 *
 * Creates a palette from the bitmap `b` by sorting the
 * pixels in the image, and then choosing `K` evenly spaced pixels.
 *
 * It is fast, but the results aren't optimal.
 */
BmPalette *bm_quantize_uniform(Bitmap *b, int K);

/**
 * #### `BmPalette *bm_quantize_random(Bitmap *b, int K)`
 *
 * Creates a palette of `K` colors from the bitmap `b` by choosing
 * `K` random pixels from the image.
 *
 * It is much faster than the other algorithms, but it
 * sacrifices a lot of quality.
 */
BmPalette *bm_quantize_random(Bitmap *b, int K);

/**
 * #### `void bm_reduce_palette(Bitmap *b, BmPalette *palette)`
 *
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying [Floyd-Steinberg dithering](http://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering)
 */
void bm_reduce_palette(Bitmap *b, BmPalette *palette);

/**
 * #### `void bm_reduce_palette_OD4(Bitmap *b, BmPalette *palette)`
 *
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying [ordered dithering](https://en.wikipedia.org/wiki/Ordered_dithering)
 * and a 4x4 Bayer matrix.
 */
void bm_reduce_palette_OD4(Bitmap *b, BmPalette *palette);

/**
 * #### `void bm_reduce_palette_OD8(Bitmap *b, BmPalette *palette)`
 *
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by applying [ordered dithering](https://en.wikipedia.org/wiki/Ordered_dithering)
 * and a 8x8 Bayer matrix.
 */
void bm_reduce_palette_OD8(Bitmap *b, BmPalette *palette);

/**
 * #### `void bm_reduce_palette_nearest(Bitmap *b, BmPalette *palette)`
 *
 * Reduces the colors in the bitmap `b` to the colors in `palette`
 * by matching each color in the source image to the closest one in the
 * palette.
 */
void bm_reduce_palette_nearest(Bitmap *b, BmPalette *palette);

/**
 * ### Drawing Primitives
 *
 * `bmp.h` provides these methods for drawing graphics primitives.
 */

/**
 * #### `void bm_clear(Bitmap *b)`
 *
 * Clears the bitmap to the pen color.
 */
void bm_clear(Bitmap *b);

/**
 * #### `void bm_putpixel(Bitmap *b, int x, int y)`
 *
 * Draws a single pixel at <x,y> using the pen color.
 */
void bm_putpixel(Bitmap *b, int x, int y);

/**
 * #### `void bm_line(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws a line from <x0,y0> to <x1,y1> using the pen color.
 */
void bm_line(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_line_aa(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws a line from <x0,y0> to <x1,y1> using the pen color that is
 * anti-aliased using [Xiaolin Wu's line algorithm](https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm)
 */
void bm_line_aa(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws a rectangle from <x0,y0> to <x1,y1> using the pen color.
 */
void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Fills a rectangle from <x0,y0> to <x1,y1> using the pen color.
 */
void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws a rectangle dithered in a checkerboard pattern
 * from `<x0,y0>` to `<x1,y1>`, using the pen color.
 */
void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_circle(Bitmap *b, int x0, int y0, int r)`
 *
 * Draws a circle of radius `r` centered at <x,y> using the pen color.
 */
void bm_circle(Bitmap *b, int x0, int y0, int r);

/**
 * #### `void bm_fillcircle(Bitmap *b, int x0, int y0, int r)`
 *
 * Fills a circle of radius `r` centered at <x,y> using the pen color.
 */
void bm_fillcircle(Bitmap *b, int x0, int y0, int r);

/**
 * #### `void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws an ellipse that occupies the rectangle from <x0,y0> to
 * <x1,y1>, using the pen color
 */
void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_fillellipse(Bitmap *b, int x0, int y0, int x1, int y1)`
 *
 * Draws a filled ellipse that occupies the rectangle from <x0,y0> to
 * <x1,y1>, using the pen color
 */
void bm_fillellipse(Bitmap *b, int x0, int y0, int x1, int y1);

/**
 * #### `void bm_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)`
 *
 * Draws a rect from <x0,y0> to <x1,y1> using the pen color with rounded corners
 * of radius `r`
 */
void bm_roundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/**
 * #### `void bm_fill_round_rect(Bitmap *b, int x0, int y0, int x1, int y1, int r)`
 *
 * Fills a rectangle from <x0,y0> to <x1,y1> using the pen color with rounded corners
 * of radius `r`
 */
void bm_fillroundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r);

/**
 * #### `void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2)`
 *
 * Draws a Quadratic Bezier curve with 3 control points `<x0,y0>`, `<x1,y1>` and `<x2,y2>`.
 */

void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2);
/**
 * #### `void bm_bezier4(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3)`
 *
 * Draws a Cubic Bezier curve with 3 control points `<x0,y0>`, `<x1,y1>`, `<x2,y2>`
 * and `<x3,y3>`.
 */
void bm_bezier4(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3);

/**
 * #### `void bm_poly(Bitmap *b, BmPoint points[], unsigned int n)`
 *
 * Draws a polygon using the pen color.
 *
 * `points` is an array of `n` `BmPoints` of the polygon's vertices.
 */
void bm_poly(Bitmap *b, BmPoint points[], unsigned int n);

/**
 * #### `void bm_fillpoly(Bitmap *b, BmPoint points[], unsigned int n)`
 *
 * Draws a filled polygon using the pen color.
 *
 * `points` is an array of `n` `BmPoints` of the polygon's vertices.
 */
void bm_fillpoly(Bitmap *b, BmPoint points[], unsigned int n);

/**
 * #### `void bm_fill(Bitmap *b, int x, int y)`
 *
 * Floodfills from `<x,y>` using the pen color.
 *
 * The color of the pixel at `<x,y>` is used as the source color.
 * The color of the pen is used as the target color.
 */
void bm_fill(Bitmap *b, int x, int y);

/**
 * ### Font Routines
 */

/**
 * #### `void bm_set_font(Bitmap *b, BmFont *font)`
 *
 * Changes the font used to render text on the bitmap.
 */
void bm_set_font(Bitmap *b, BmFont *font);

/**
 * #### `BmFont *bm_get_font(Bitmap *b)`
 *
 * Retrieves the current font used to render text on the bitmap.
 */
BmFont *bm_get_font(Bitmap *b);

/**
 * #### `void bm_reset_font(BmFont *b)`
 *
 * Resets the font used to draw on the `Bitmap` to the
 * default *Apple II*-inspired font
 */
void bm_reset_font(Bitmap *b);

/**
 * #### `int bm_text_width(Bitmap *b, const char *s)`
 *
 * Returns the width in pixels of a string of text.
 */
int bm_text_width(Bitmap *b, const char *s);

/**
 * #### `int bm_text_height(Bitmap *b, const char *s)`
 *
 * Returns the height in pixels of a string of text.
 */
int bm_text_height(Bitmap *b, const char *s);

/**
 * #### `void bm_text_measure(Bitmap *b, const char *s)`
 *
 * Returns the width and height in pixels of a text.
 *
 * `dx` and `dy` help position the rectangle `<w,h>` around the text.
 */
void bm_text_measure(Bitmap *b, const char *s, int *w, int* h, int* dx, int* dy);

/**
 * #### `void bm_putc(Bitmap *b, int x, int y, char c)`
 *
 * Prints the character `c` at position `x,y` on the bitmap `b`.
 */
int bm_putc(Bitmap *b, int x, int y, char c);

/**
 * #### `void bm_puts(Bitmap *b, int x, int y, const char *s)`
 *
 * Prints the string `s` at position `x,y` on the bitmap `b`.
 */
int bm_puts(Bitmap *b, int x, int y, const char *text);

/**
 * #### `void bm_printf(Bitmap *b, int x, int y, const char *fmt, ...)`
 *
 * Prints a `printf()`-formatted style string to the bitmap `b`,
 * `fmt` is a `printf()`-style format string (It uses `vsnprintf()` internally).
 */
int bm_printf(Bitmap *b, int x, int y, const char *fmt, ...);

/**
 * #### `BmFont *bm_font_retain(BmFont *font)`
 *
 * Increments a font's reference counter.
 */
BmFont *bm_font_retain(BmFont *font);

/**
 * #### `unsigned int bm_font_release(BmFont *font)`
 *
 * Decrements a font's reference counter, and if it's 0,
 * destroys it by calling its `dtor` function on itself.
 *
 * It returns the reference count of the font, so 0
 * means the font was destroyed.
 */
unsigned int bm_font_release(BmFont *font);

/**
 * #### `BmFont *bm_make_ras_font(const char *file, int spacing)`
 *
 * Creates a raster font from a bitmap file named `file` in any of the
 * supported file types.
 *
 * The characters in the bitmap must be arranged like this:
 * ```
 *  !"#$%&'()*+,-./
 * 0123456789:;<=>?
 * @ABCDEFGHIJKLMNO
 * PQRSTUVWXYZ[\]^_
 * `abcdefghijklmno
 * pqrstuvwxyz{|}~
 * ```
 * The characters are in ASCII sequence, without the first 32 control characters.
 * The pixel width and hight of the individual characters is calculated by dividing
 * the width and height of the bitmap by 16 and 6 respectively.
 *
 * `fonts/font.gif` is an example of an font that can be used in this way:  \
 * ![sample font image](fonts/font.gif)
 *
 * The image is 128x48 pixels, so the individual characters are 8x8 pixels.
 * (128/16=8 and 48/6=8)
 *
 * The color of the pixel in the top-left corner is taken to be the transparent
 * color (since the character there is a space).
 *
 * The `spacing` parameter determines. If it is zero, the width of the
 * characters is used.
 *
 * The returned font's `type` will be set to `"RASTER_FONT"`
 */
BmFont *bm_make_ras_font(const char *file, int spacing);

/** #### `BmFont *bm_make_sfont(const char *file)`
 *
 * Creates a raster font from a SFont or a GrafX2-style font from any of
 * the supported file types.
 *
 * A [SFont][sfont] is a bitmap (in any supported file format) that contains all
 * ASCII characters from 33 (`'!'`) to 127 in a single row. There is an additional
 * row of pixels at the top that describes the width of each character using magenta
 * (`#FF00FF`) pixels for the spacing. The height of the font is the height of the
 * rest of the bitmap.
 *
 * [GrafX2][grafx2] is a pixel art paint program that uses a similar format, except
 * the pixels in the first row don't have to use magenta.
 *
 * The returned font's `type` will be set to `"SFONT"`
 *
 * [sfont]: http://www.linux-games.com/sfont/
 * [grafx2]: https://en.wikipedia.org/wiki/GrafX2
 */
BmFont *bm_make_sfont(const char *file);

/** #### `BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing)`
 *
 * Creates a font from a XBM bitmap. The XBM bitmaps can be compiled directly into
 * a program's executable rather than being loaded at runtime.
 *
 * The XBM bitmap should have the same layout as `bm_make_ras_font()` above.
 * The `fonts/` directory has several examples available.
 */
BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing);

/** #### `BmFont *bm_make_zxo_font(const uint8_t *bits)`
 *
 * Creates a `BmFont` object from one of the [ZX-Origins][] fonts' C headers.
 *
 * Each of those fonts has a `Source/` directory with a header file
 * named after the font. You include that file in your program, and
 * then pass the `uint8_t` array therein to `bm_make_zxo_font()`.
 *
 * For example, to use the [Prince][zx-prince] font, you would need
 * to `#include` the `Prince.h` file, and then call `bm_make_zxo_font()`
 * with `FONT_PRINCE_BITMAP`.
 *
 * ```
 * #include "Prince/Source/Prince.h"
 * ...
 * BmFont *font = bm_make_zxo_font(FONT_PRINCE_BITMAP);
 * ```
 *
 * **NOTE:** The line in the header with the bytes for the `'\'` character ends
 *  with a`// \` in some of the font, which the C preprocessor will see as a
 * line continuation and treat the next line (containing the bytes of the `']'`
 * character) as a comment. This results in the wrong glyph being displayed for
 * lower case characters. If this happens, simply remove the `// \` from the
 * header, and recompile.
 *
 * [zx-origins]: https://damieng.com/typography/zx-origins/
 * [zx-prince]: https://damieng.com/typography/zx-origins/prince/
 */
BmFont *bm_make_zxo_font(const uint8_t *bits);

/** #### `BmFont *bm_load_zxo_font(const char *filename)`
 *
 * Creates a `BmFont` object from one of the [ZX-Origins][] fonts Spectrum binary files.
 *
 * Each of those fonts has a `Spectrum/` directory with a `.ch8` file that contains the
 * 768 bytes of the font in a binary file.
 *
 * For example, to use the [Prince][zx-prince] font, you would invoke this function like so:
 *
 * ```
 * BmFont * font = bm_load_zxo_font("Prince/Spectrum/Prince.ch8");
 * ```
 */
BmFont *bm_load_zxo_font(const char *filename);


/**
 * ### Error Handling Functions
 */

/** #### `const char *bm_get_error()`
 * Gets the last error message.
 *
 * Tracking error messages uses a global variable internally,
 * which is not reentrant. To disable this functionality,
 * `#define BM_LAST_ERROR 0` before including this header
 * or define it as 0 in your compiler's command-line options.
 */
const char *bm_get_error();

/** #### `void bm_set_error(const char *e)`
 * Sets the internal error message.
 */
void bm_set_error(const char *e);

/**
 * ### Utility Functions
 *
 * These functions are not directly related to image processing,
 * bu
 */

/** #### `int bm_stricmp(const char *p, const char *q)`
 * Compares strings `p` and `q` case-insensitively.
 *
 * It is used the in the same way as `strcmp()` in the standard
 * C library.
 */
int bm_stricmp(const char *p, const char *q);

/** #### `char *bm_strtok_r(char *str, const char *delim, char **saveptr)`
 * `strtok_r()` implementation for systems where it is unavailable.
 */
char *bm_strtok_r(char *str, const char *delim, char **saveptr);

/** #### `const char* bm_utf8_next_codepoint(const char* in, unsigned int* codepoint)`
 * Decodes the next code point in a UTF-8 encoded string.
 *
 * The decoded codepoint is stored in `codepoint`.
 *
 * It returns a pointer to the next character after the decoded codepoint.
 * It returns `NULL` at the end of the string on error.
 */
const char* bm_utf8_next_codepoint(const char* in, unsigned int* codepoint);

/**
 * TODO
 * ----
 *
 * - [ ] [k-d trees][kdtree] have been suggested as a way to speed up nearest neighbour searches.
 *       I'm thinking in particular in my color quantization code which is a bit naive at the moment.
 * - [ ] `bm_atoi()` does not parse `chucknorris` correctly.  \
 *       See <https://stackoverflow.com/a/8333464/115589>
 * - [ ] I'm regretting my decision to have the BmFont.width function not look at the
 *       actual character you want to draw, so `bm_text_width()` is broken if you
 *       aren't using a fixed width font.
 * - [ ] I only recently learned of [Wuffs][wuffs]. It might be worth integrating it in the same way
 *       I integrate `stb_image` for security sensitive applications.
 *       ([HN link](https://news.ycombinator.com/item?id=26714831))
 *
 * [kdtree]: https://en.wikipedia.org/wiki/K-d_tree
 * [wuffs]: https://github.com/google/wuffs
 *
 * References
 * ----------
 *
 * * [BMP file format](http://en.wikipedia.org/wiki/BMP_file_format) on Wikipedia
 * * [Bresenham's line algorithm](http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm) on Wikipedia
 * * <http://members.chello.at/~easyfilter/bresenham.html>
 * * [Flood fill](http://en.wikipedia.org/wiki/Flood_fill) on Wikipedia
 * * [Midpoint circle algorithm](http://en.wikipedia.org/wiki/Midpoint_circle_algorithm) on Wikipedia
 * * <http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html>
 * * [Typography in 8 bits: System fonts](http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts)
 * * [GIF89a specification](http://www.w3.org/Graphics/GIF/spec-gif89a.txt)
 * * Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
 * * <http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011>
 * * [What's In A GIF](http://www.matthewflickinger.com/lab/whatsinagif/index.html) by Matthew Flickinger
 * * <http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt>
 * * <http://www.shikadi.net/moddingwiki/PCX_Format>
 * * [Truevision TGA](https://en.wikipedia.org/wiki/Truevision_TGA) on Wikipedia
 * * <http://paulbourke.net/dataformats/tga/>
 * * <http://www.ludorg.net/amnesia/TGA_File_Format_Spec.html>
 * * [X PixMap](https://en.wikipedia.org/wiki/X_PixMap) on Wikipedia
 * * <http://www.fileformat.info/format/xpm/egff.htm>
 * * "Fast Bitmap Rotation and Scaling" By Steven Mortimer, Dr Dobbs' Journal, July 01, 2001  \
 *   <http://www.drdobbs.com/architecture-and-design/fast-bitmap-rotation-and-scaling/184416337>
 * * <http://www.efg2.com/Lab/ImageProcessing/RotateScanline.htm>
 * * [Image Filtering](http://lodev.org/cgtutor/filtering.html) in _Lode's Computer Graphics Tutorial_
 * * [Efficient Polygon Fill Algorithm With C Code Sample](http://alienryderflex.com/polygon_fill/)
 *   by Darel Rex Finley.
 * * [Computer Graphics: Scan Line Polygon Fill Algorithm](https://hackernoon.com/computer-graphics-scan-line-polygon-fill-algorithm-3cb47283df6)
 *   by Alberto Scicali
 * * [Count the consecutive zero bits (trailing) on the right in parallel](https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightParallel)
 */

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* BMP_H */
