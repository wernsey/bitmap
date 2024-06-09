/*
 * Bitmap manipulation functions. See `bmp.h` for details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <float.h>
#include <assert.h>

#ifdef USESDL
#  ifdef ANDROID
#    include <SDL.h>
#  else
#    include <SDL2/SDL.h>
#  endif
#endif

/*
Use the -DUSEPNG compiler option to enable PNG support via libpng.
If you use it, you need to link against the libpng (-lpng)
and zlib (-lz) libraries.
Use the -DUSEJPG compiler option to enable JPG support via libjpg.
I've decided to keep both optional, for situations where
you may not want to import a bunch of third party libraries.
*/
#ifdef USEPNG
#   include <png.h>
#endif

#ifdef USEJPG
#   include <jpeglib.h>
#   include <setjmp.h>
#   include <jerror.h>
#endif

#ifndef BMP_H
#  include "bmp.h"
#endif

/* Ignore the alpha byte when comparing colors?
FIXME: Not all functions that should respect IGNORE_ALPHA does so.
*/
#ifndef IGNORE_ALPHA
#  define IGNORE_ALPHA 1
#endif

/* Experimental ABGR color mode.
 * Normally colors are stored as 0xARGB, but Emscripten uses an
 * 0xABGR format on the canvas, so use -DABGR=1 in your compiler
 * flags if you need to use this mode. */
#ifndef ABGR
#  define ABGR 0
#endif

/* Use RLE when saving TGA files? */
#define TGA_SAVE_RLE    1

/* Save NetPBM in binary format (P4,P5,P6)? */
#ifndef PPM_BINARY
#  define PPM_BINARY 0
#endif

/* Save transparent backgrounds when saving GIF?
It used to be on by default, but it turned out to be less useful
than I expected, and caused some confusion.
Still, it is here if you need it
*/
#ifndef SAVE_GIF_TRANSPARENT
#  define SAVE_GIF_TRANSPARENT 0
#endif

#ifndef SIZE_LIMITS
#  define SIZE_LIMITS 1
#endif

#if BM_LAST_ERROR
static const char *bm_last_error = "no error";
#  define SET_ERROR(e) bm_last_error = e
#else
#  define SET_ERROR(e) (void)e
#endif

/* Comparing colors is a bit of a rabbit hole because of how humans perception works.
If you define RGB_BETTER_COMPARE as non-zero, functions that need to compare colors
based my solution on this answer: https://stackoverflow.com/a/9085524/115589, which
in turn cites this document: https://www.compuphase.com/cmetric.htm

See `bm_reduce_palette_nearest()` and `bm_palette_nearest_index()`
*/
#ifndef RGB_BETTER_COMPARE
#  define RGB_BETTER_COMPARE 1
#endif

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define SWAPINT(a, b) do {int t = a; a = b; b = t;} while(0)

/* https://eli.thegreenplace.net/2009/11/16/void-and-casts-in-c-and-c */
#if defined(__cplusplus)
#  define CAST(T) static_cast<T>
#else
#  define CAST(T)
#endif

#if !defined(WIN32) && 0
/* TODO: Use `alloca()` if it is available */
#define ALLOCA(x) alloca(x)
#define FREEA(x)
#else
#define ALLOCA(x) malloc(x)
#define FREEA(x) free(x)
#endif

/* TODO: C11 defines fopen_s(), strncpy_s(), etc.
At the moment, I only use them if WIN32 is defined.
See __STDC_LIB_EXT1__
*/
#if defined(WIN32) && defined(_MSC_VER)
#  define SAFE_C11
#  define strdup _strdup
#endif

/* Definitions for Bitmap::flags */

/* FLAG_OWNS_DATA - set if the Bitmap owns the memory allocated to Bitmap::data.
    If it is set, then bm_free() will free it.
 */
#define FLAG_OWNS_DATA  1

/* TODO: I want to add flags to check if the bitmap had a palette and an alpha channel
    and so on in the file it was loaded from, etc.
 */

/*
 * Structure containing a bitmap image.
 *
 * The internal format is `0xAARRGGBB` little endian.
 * Meaning that `p[0]` contains B, `p[1]` contains G,
 * `p[2]` contains R and `p[3]` contains A
 * and the data buffer is an array of bytes BGRABGRABGRABGRABGRA...
 *
 * The member `color` contains the color that will be used for drawing
 * primitives, and for transparency while blitting.
 *
 * The member `font` is a pointer to a `BmFont` structure that is used
 * to render text. See the [Font Routines][] section for more details.
 * Don't modify this directly, since fonts are reference counted;
 * use `bm_set_font()` instead.
 *
 * The member `clip` is a `BmRect` that defines the clipping rectangle
 * when drawing primitives and text.
 */
struct bitmap {
    /* Dimesions of the bitmap */
    int w, h;

    /* The actual pixel data in RGBA format */
    unsigned char *data;

    /* Color for the pen of the canvas */
    unsigned int color;

    /* Font object for rendering text */
    struct bitmap_font *font;

    /* Reference count of the bitmap object */
    unsigned int ref_count;

    /* Some flags for the bitmap */
    unsigned int flags;

    /* Clipping rectangle */
    BmRect clip;

    /* (optional) Palette associated with the bitmap */
    BmPalette *palette;
};

struct bitmap_palette {

    /* Reference count */
    unsigned int ref_count;

    /* Number of colors allocated */
    int acolors;

    /* Actual number of colors; <= acolors */
    int ncolors;

    /* Array of colors */
    unsigned int *colors;
};

#pragma pack(push, 1) /* Don't use any padding (Windows compilers) */

/* Data structures for the header of BMP files. */
struct bmpfile_magic {
  unsigned char magic[2];
};

struct bmpfile_header {
  uint32_t filesz;
  uint16_t creator1;
  uint16_t creator2;
  uint32_t bmp_offset;
};

struct bmpfile_dibinfo {
  uint32_t header_sz;
  int32_t width;
  int32_t height;
  uint16_t nplanes;
  uint16_t bitspp;
  uint32_t compress_type;
  uint32_t bmp_bytesz;
  int32_t hres;
  int32_t vres;
  uint32_t ncolors;
  uint32_t nimpcolors;
};

struct bmpfile_colinfo {
    uint8_t b, g, r, a;
};

/* RGB triplet used for palettes in PCX and GIF support */
struct rgb_triplet {
    unsigned char r, g, b;
};
#pragma pack(pop)

#define BM_BPP          4 /* Bytes per Pixel */
#define BM_BLOB_SIZE(B) (B->w * B->h * BM_BPP)
#define BM_ROW_SIZE(B)  (B->w * BM_BPP)

#define BM_GET(b, x, y) (*((unsigned int*)(b->data + (y) * BM_ROW_SIZE(b) + (x) * BM_BPP)))
#define BM_SET(b, x, y, c) *((unsigned int*)(b->data + (y) * BM_ROW_SIZE(b) + (x) * BM_BPP)) = (c)

#if !ABGR
#  define BM_SET_RGBA(BMP, X, Y, R, G, B, A) do { \
        int _p = ((Y) * BM_ROW_SIZE(BMP) + (X)*BM_BPP); \
        BMP->data[_p++] = B;\
        BMP->data[_p++] = G;\
        BMP->data[_p++] = R;\
        BMP->data[_p++] = A;\
    } while(0)
#  define BM_GETB(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 0])
#  define BM_GETG(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 1])
#  define BM_GETR(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 2])
#  define BM_GETA(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 3])
#  define SET_COLOR_RGB(bm, r, g, b) bm->color = 0xFF000000 | ((r) << 16) | ((g) << 8) | (b)
#else
#  define BM_SET_RGBA(BMP, X, Y, R, G, B, A) do { \
        int _p = ((Y) * BM_ROW_SIZE(BMP) + (X)*BM_BPP); \
        BMP->data[_p++] = R;\
        BMP->data[_p++] = G;\
        BMP->data[_p++] = B;\
        BMP->data[_p++] = A;\
    } while(0)
#  define BM_GETR(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 0])
#  define BM_GETG(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 1])
#  define BM_GETB(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 2])
#  define BM_GETA(B,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + 3])
#  define SET_COLOR_RGB(bm, r, g, b) bm->color = 0xFF000000 | ((b) << 16) | ((g) << 8) | (r)
#endif

/* N=0 -> B, N=1 -> G, N=2 -> R, N=3 -> A */
#define BM_GETN(B,N,X,Y) (B->data[((Y) * BM_ROW_SIZE(B) + (X) * BM_BPP) + (N)])

static Bitmap *bm_create_internal(int w, int h) {
    SET_ERROR("no error");

    if(w <= 0 || h <= 0) {
        SET_ERROR("invalid dimensions");
        return NULL;
    }
#if SIZE_LIMITS
    if(w > 23000 || h > 23000 || w*h > 0x1FFFFFFF) {
        SET_ERROR("dimensions too large");
        return NULL;
    }
#endif

    Bitmap *b = CAST(Bitmap *)(malloc(sizeof *b));
    if(!b) {
        SET_ERROR("out of memory");
        return NULL;
    }

    b->w = w;
    b->h = h;

    b->clip.x0 = 0;
    b->clip.y0 = 0;
    b->clip.x1 = w;
    b->clip.y1 = h;

    b->data = NULL;
    b->flags = 0;

    b->font = NULL;
    bm_reset_font(b);

    bm_set_color(b, 0xFFFFFFFF);

    b->palette = NULL;

    b->ref_count = 0;

    return b;
}

Bitmap *bm_create(int w, int h) {
    Bitmap *b = bm_create_internal(w, h);
    if(!b)
        return NULL;

    b->data = CAST(unsigned char *)(malloc(BM_BLOB_SIZE(b)));
    if(!b->data) {
        SET_ERROR("out of memory");
        bm_free(b);
        return NULL;
    }
    memset(b->data, 0x00, BM_BLOB_SIZE(b));

    b->flags = FLAG_OWNS_DATA;
    return b;
}

#if defined(USESTB)
#  define STB_IMAGE_IMPLEMENTATION
#  ifdef __TINYC__
/* Yes, it compiles with the Tiny C Compiler, but without SIMD */
#    define STBI_NO_SIMD
#  endif
#  ifndef STBI_INCLUDE_STB_IMAGE_H
#    include "stb_image.h"
#    undef STB_IMAGE_IMPLEMENTATION
#  endif
#endif

/* Wraps around the stdio functions, so I don't have to duplicate my code
    for SDL2's RWops support.
    It is unfortunately an abstraction over an abstraction in the case of
    SDL_RWops, but such is life. */
typedef struct {
    void *data;
    size_t (*fread)(void* ptr, size_t size, size_t nobj, void* stream);
    long (*ftell)(void* stream);
    int (*fseek)(void* stream, long offset, int origin);
} BmReader;

static BmReader make_file_reader(FILE *fp) {
    BmReader rd;
    rd.data = fp;
    rd.fread = (size_t(*)(void*,size_t,size_t,void*))fread;
    rd.ftell = (long(*)(void* ))ftell;
    rd.fseek = (int(*)(void*,long,int))fseek;
    return rd;
}

typedef struct {
    const unsigned char *buffer;
    unsigned int len;
    unsigned int pos;
} BmMemReader;

static size_t memread(void *ptr, size_t size, size_t nobj, BmMemReader *mem) {
    size = size * nobj;
    if(mem->pos + size > mem->len) {
        return 0;
    }
    memcpy(ptr, mem->buffer + mem->pos, size);
    mem->pos += size;
    return nobj;
}
static long memtell(BmMemReader *mem) {
    return mem->pos;
}
static int memseek(BmMemReader *mem, long offset, int origin) {
    switch(origin) {
    case SEEK_SET: mem->pos = offset; break;
    case SEEK_CUR: mem->pos += offset; break;
    case SEEK_END: mem->pos = mem->len - offset; break;
    }
    if(mem->pos >= mem->len) {
        mem->pos = 0;
        return -1;
    }
    return 0;
}

static BmReader make_mem_reader(BmMemReader *mem) {
    BmReader rd;
    rd.data = mem;
    mem->pos = 0;
    rd.fread = (size_t(*)(void*,size_t,size_t,void*))memread;
    rd.ftell = (long(*)(void* ))memtell;
    rd.fseek = (int(*)(void*,long,int))memseek;
    return rd;
}

#ifdef USESDL
static size_t rw_fread(void *ptr, size_t size, size_t nobj, SDL_RWops *stream) {
    return SDL_RWread(stream, ptr, size, nobj);
}
static long rw_ftell(SDL_RWops *stream) {
    return SDL_RWtell(stream);
}
static int rw_fseek(SDL_RWops *stream, long offset, int origin) {
    switch (origin) {
        case SEEK_SET: origin = RW_SEEK_SET; break;
        case SEEK_CUR: origin = RW_SEEK_CUR; break;
        case SEEK_END: origin = RW_SEEK_END; break;
    }
    if(SDL_RWseek(stream, offset, origin) < 0)
        return 1;
    return 0;
}
static BmReader make_rwops_reader(SDL_RWops *rw) {
    BmReader rd;
    rd.data = rw;
    rd.fread = (size_t(*)(void*,size_t,size_t,void*))rw_fread;
    rd.ftell = (long(*)(void* ))rw_ftell;
    rd.fseek = (int(*)(void*,long,int))rw_fseek;
    return rd;
}
#endif

Bitmap *bm_load(const char *filename) {
    SET_ERROR("no error");

    Bitmap *bmp;
#ifdef SAFE_C11
    FILE *f;
    errno_t err = fopen_s(&f, filename, "rb");
    if (err != 0) f = 0;
#else
    FILE *f = fopen(filename, "rb");
#endif
    if(!f) {
        SET_ERROR("unable to open file");
        return NULL;
    }
    bmp = bm_load_fp(f);
    fclose(f);
    return bmp;
}

Bitmap *bm_loadf(const char *fmt, ...) {
    char fname[256];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(fname, sizeof fname, fmt, arg);
    va_end(arg);
    return bm_load(fname);
}

static int is_tga_file(BmReader rd);

static uint32_t count_trailing_zeroes(uint32_t v);

static Bitmap *bm_load_bmp_rd(BmReader rd);
static Bitmap *bm_load_gif_rd(BmReader rd);
static Bitmap *bm_load_pcx_rd(BmReader rd);
static Bitmap *bm_load_tga_rd(BmReader rd);
static Bitmap *bm_load_ppm_rd(BmReader rd);
#ifdef USEPNG
static Bitmap *bm_load_png_fp(FILE *f);
static Bitmap *bm_load_png_mem(const unsigned char *inbuffer, size_t insize);
#endif
#ifdef USEJPG
static Bitmap *bm_load_jpg_fp(FILE *f);
static Bitmap *bm_load_jpg_mem(const unsigned char *inbuffer, size_t insize);
#endif

Bitmap *bm_load_fp(FILE *f) {
    SET_ERROR("no error");
    unsigned char magic[4];

    long start, isbmp = 0, ispng = 0, isjpg = 0, ispcx = 0, isgif = 0, istga = 0, ispbm = 0;

    BmReader rd = make_file_reader(f);
    start = rd.ftell(rd.data);

    /* Tries to detect the type of file by looking at the first bytes.
    http://www.astro.keele.ac.uk/oldusers/rno/Computing/File_magic.html
    */
    if(rd.fread(magic, sizeof magic, 1, rd.data) == 1) {
        if(!memcmp(magic, "BM", 2))
            isbmp = 1;
        else if(!memcmp(magic, "GIF", 3))
            isgif = 1;
        else if(magic[0] == 0xFF && magic[1] == 0xD8)
            isjpg = 1;
        else if(magic[0] == 0x0A)
            ispcx = 1;
        else if(magic[0] == 0x89 && !memcmp(magic+1, "PNG", 3))
            ispng = 1;
        else if(magic[0] == 'P' && strchr("123456", magic[1]))
            ispbm = 1;
        else {
            /* Might be a TGA. TGA does not have a magic number :( */
            rd.fseek(rd.data, start, SEEK_SET);
            istga = is_tga_file(rd);
        }
    } else {
        SET_ERROR("couldn't determine filetype");
        return NULL;
    }
    rd.fseek(rd.data, start, SEEK_SET);

    if(isjpg) {
#ifdef USEJPG
        return bm_load_jpg_fp(f);
#elif defined(USESTB)
        int x, y, n;
        stbi_uc *data = stbi_load_from_file(f, &x, &y, &n, 4);
        if(!data) {
            SET_ERROR(stbi_failure_reason());
            return NULL;
        }
        return bm_from_stb(x, y, data);
#else
        (void)isjpg;
        SET_ERROR("JPEG support is not enabled");
        return NULL;
#endif
    }
    if(ispng) {
#ifdef USEPNG
        return bm_load_png_fp(f);
#elif defined(USESTB)
        int x, y, n;
        stbi_uc *data = stbi_load_from_file(f, &x, &y, &n, 4);
        if(!data) {
            SET_ERROR(stbi_failure_reason());
            return NULL;
        }
        return bm_from_stb(x, y, data);
#else
        (void)ispng;
        SET_ERROR("PNG support is not enabled");
        return NULL;
#endif
    }
    if(isgif) {
        return bm_load_gif_rd(rd);
    }
    if(ispcx) {
        return bm_load_pcx_rd(rd);
    }
    if(isbmp) {
        return bm_load_bmp_rd(rd);
    }
    if(istga) {
        return bm_load_tga_rd(rd);
    }
    if(ispbm) {
        return bm_load_ppm_rd(rd);
    }
    SET_ERROR("unsupported file type");
    return NULL;
}

Bitmap *bm_load_mem(const unsigned char *buffer, long len) {
    SET_ERROR("no error");

    unsigned char magic[4];

    long isbmp = 0, ispng = 0, isjpg = 0, ispcx = 0, isgif = 0, istga = 0, ispbm = 0;

    BmMemReader memr;
    memr.buffer = buffer;
    memr.len = len;

    BmReader rd = make_mem_reader(&memr);
    /* Tries to detect the type of file by looking at the first bytes.
    http://www.astro.keele.ac.uk/oldusers/rno/Computing/File_magic.html
    */
    if(rd.fread(magic, sizeof magic, 1, rd.data) == 1) {
        if(!memcmp(magic, "BM", 2))
            isbmp = 1;
        else if(!memcmp(magic, "GIF", 3))
            isgif = 1;
        else if(magic[0] == 0xFF && magic[1] == 0xD8)
            isjpg = 1;
        else if(magic[0] == 0x0A)
            ispcx = 1;
        else if(magic[0] == 0x89 && !memcmp(magic+1, "PNG", 3))
            ispng = 1;
        else if(magic[0] == 'P' && strchr("123456", magic[1]))
            ispbm = 1;
        else {
            /* Might be a TGA. TGA does not have a magic number :( */
            rd.fseek(rd.data, 0, SEEK_SET);
            istga = is_tga_file(rd);
        }
    } else {
        SET_ERROR("couldn't determine filetype");
        return NULL;
    }
    rd.fseek(rd.data, 0, SEEK_SET);

    if(isjpg) {
#ifdef USEJPG
        return bm_load_jpg_mem(buffer, len);
#elif defined(USESTB)
        int x, y, n;
        stbi_uc *data = stbi_load_from_memory(buffer, len, &x, &y, &n, 4);
        if(!data) {
            SET_ERROR(stbi_failure_reason());
            return NULL;
        }
        return bm_from_stb(x, y, data);
#else
        (void)isjpg;
        SET_ERROR("JPEG support is not enabled");
        return NULL;
#endif
    }
    if(ispng) {
#ifdef USEPNG
        return bm_load_png_mem(buffer, len);
#elif defined(USESTB)
        int x, y, n;
        stbi_uc *data = stbi_load_from_memory(buffer, len, &x, &y, &n, 4);
        if(!data) {
            SET_ERROR(stbi_failure_reason());
            return NULL;
        }
        return bm_from_stb(x, y, data);
#else
        (void)ispng;
        SET_ERROR("PNG support is not enabled");
        return NULL;
#endif
    }
    if(isgif) {
        return bm_load_gif_rd(rd);
    }
    if(ispcx) {
        return bm_load_pcx_rd(rd);
    }
    if(isbmp) {
        return bm_load_bmp_rd(rd);
    }
    if(istga) {
        return bm_load_tga_rd(rd);
    }
    if(ispbm) {
        return bm_load_ppm_rd(rd);
    }
    SET_ERROR("unsupported file type"); /* should not happen */
    return NULL;
}

Bitmap *bm_load_base64(const char *base64) {
    SET_ERROR("no error");
    /* It would've been cool to read the Base64 data
    in place with a custom BmReader object, but I
    found that decoding first makes it easier to deal with
    whitespace in the input data */
    if(!base64)
        return NULL;

    if(!memcmp(base64, "data:", 5)) {
        /* https://en.wikipedia.org/wiki/Data_URI_scheme
        I can ignore the parameters because we deduce the file type from the data,
        and assume the Base64 is in ASCII
        */
        base64 = strchr(base64, ',');
        if(!base64) {
            SET_ERROR("invalid data URI");
            return NULL;
        }
        base64++;
    }

    long len = strlen(base64);
    unsigned char *p, *buffer;
    const char *q;

    unsigned octet = 0, sextet, bits = 0;

    buffer = CAST(unsigned char*)(malloc(len + 1));
    if(!buffer) {
        SET_ERROR("out of memory");
        return NULL;
    }

    for(p = buffer, q = base64; *q; q++) {
        if(isspace(*q))
            continue;
        else if(isupper(*q))
            sextet = *q - 'A';
        else if(islower(*q))
            sextet = *q - 'a' + 26;
        else if(isdigit(*q))
            sextet = *q - '0' + 52;
        else if(*q == '+')
            sextet = 62;
        else if(*q == '/')
            sextet = 63;
        else if(*q == '=')
            break;
        else {
            SET_ERROR("invalid character in Base64 data");
            free(buffer);
            return NULL;
        }

        octet = (octet << 6) | sextet;
        bits += 6;
        if(bits > 8) {
            *p++ = (octet >> (bits - 8)) & 0xFF;
            bits -= 8;
        }
    }
    if(bits == 8)
        *p++ = octet & 0xFF;
    assert((p - buffer) < len);

    Bitmap *b = bm_load_mem(buffer, p - buffer);

    free(buffer);

    return b;
}

static Bitmap *bm_load_bmp_rd(BmReader rd) {
    struct bmpfile_magic magic;
    struct bmpfile_header hdr;
    struct bmpfile_dibinfo dib;
    BmPalette *pal = NULL;

    Bitmap *b = NULL;

    int i, j;
    unsigned rs;
    unsigned char *data = NULL;

    uint32_t rgbmask[3] = { 0, 0, 0 };
    uint32_t rgbshift[3] = { 0, 0, 0 };
    float rgbcorr[3] = { 0.0f, 0.0f, 0.0f };

    long start_offset = rd.ftell(rd.data);

    if(rd.fread(&magic, sizeof magic, 1, rd.data) != 1) {
        SET_ERROR("fread on magic");
        return NULL;
    }

    if(memcmp(magic.magic, "BM", 2)) {
        SET_ERROR("bad magic");
        return NULL;
    }

    if(rd.fread(&hdr, sizeof hdr, 1, rd.data) != 1 ||
        rd.fread(&dib, sizeof dib, 1, rd.data) != 1) {
        SET_ERROR("fread on header");
        return NULL;
    }

    if (dib.bitspp != 1 &&
        dib.bitspp != 4 &&
        dib.bitspp != 8 &&
        dib.bitspp != 24 &&
        dib.bitspp != 32) {
        /* Unsupported BMP type. Only 16bpp is missing now */
        SET_ERROR("unsupported BMP type");
        return NULL;
    }

    if(dib.compress_type != 0 && dib.compress_type != 3) {
        /* Unsupported compression type. Only uncompressed BI_RGB & BI_BITFIELDS are ok */
        SET_ERROR("unsupported compression type");
        return NULL;
    }

    b = bm_create(dib.width, dib.height);
    if(!b) {
        return NULL;
    }

    if(dib.bitspp <= 8) {
        struct bmpfile_colinfo palette[256];
        unsigned int i;
        if(!dib.ncolors) {
            dib.ncolors = 1 << dib.bitspp;
        }
        assert(dib.ncolors <= 256);

        if(rd.fread(palette, sizeof *palette, dib.ncolors, rd.data) != dib.ncolors) {
            SET_ERROR("fread on palette");
            goto error;
        }

        pal = bm_palette_create(dib.ncolors);
        if(!pal)
            goto error;
        for(i = 0; i < dib.ncolors; i++) {
            unsigned int color = palette[i].a << 24 | palette[i].r << 16 | palette[i].g << 8 | palette[i].b;
            bm_palette_set(pal, i, color);
        }
        bm_set_palette(b, pal);
    }

    /* standard bitmasks for 16 & 32 bpp, required when biCompression = BI_RGB */
    if (dib.bitspp == 32) {
        rgbmask[0] = 0x00FF0000;
        rgbmask[1] = 0x0000FF00;
        rgbmask[2] = 0x000000FF;
    } else if (dib.bitspp == 16) {
        rgbmask[0] = 0x00007C00;
        rgbmask[1] = 0x000003E0;
        rgbmask[2] = 0x0000001F;
    }

    /* biCompression = BI_BITFIELDS, so read the bitmask */
    if (dib.compress_type == 3) {
        if (rd.fread(rgbmask, 1, 12, rd.data) != 12) {
            SET_ERROR("fread on bitfields");
            goto error;
        }
    }

    /* 1. calculate how many bits we have to shift after masking */
    /* 2. calculate the bit depth of the input channels */
    /* 3. calculate the factor that maps the channel to 0-255 */
    for (i = 0; i < 3; ++i) {
        rgbshift[i] = count_trailing_zeroes(rgbmask[i]);
        uint32_t chdepth = rgbmask[i] >> rgbshift[i];
        rgbcorr[i] = chdepth ? 255.0f / chdepth : 0.0f;
    }

    if(rd.fseek(rd.data, hdr.bmp_offset + start_offset, SEEK_SET) != 0) {
        SET_ERROR("out of memory");
        goto error;
    }

    rs = ((dib.width * dib.bitspp / 8) + 3) & ~3;
    assert(rs % 4 == 0);

    if(dib.bmp_bytesz == 0) {
        data = CAST(unsigned char *)(malloc(rs * b->h));
        if(!data) {
            SET_ERROR("out of memory");
            goto error;
        }

        if(rd.fread(data, 1, rs * b->h, rd.data) != rs * b->h) {
            SET_ERROR("fread on data");
            goto error;
        }
    } else {
        data = CAST(unsigned char *)(malloc(dib.bmp_bytesz));
        if(!data) {
            SET_ERROR("out of memory");
            goto error;
        }

        if(rd.fread(data, 1, dib.bmp_bytesz, rd.data) != dib.bmp_bytesz) {
            SET_ERROR("fread on data");
            goto error;
        }
    }

    if(dib.bitspp == 8) {
        for(j = 0; j < b->h; j++) {
            int y = b->h - j - 1;
            for(i = 0; i < b->w; i++) {
                int byt = y * rs + i;
                uint8_t p = data[byt];

                assert(p < dib.ncolors);
                bm_set(b, i, j, bm_palette_get(pal, p));
            }
        }
    } else if(dib.bitspp == 4) {
        for(j = 0; j < b->h; j++) {
            int y = b->h - j - 1;
            for(i = 0; i < b->w; i++) {
                int byt = y * rs + (i >> 1);
                uint8_t p = ( (i & 0x01) ? data[byt] : (data[byt] >> 4) ) & 0x0F;
                assert(p < dib.ncolors);
                bm_set(b, i, j, bm_palette_get(pal, p));
            }
        }
    } else if (dib.bitspp == 1) {
        for(j = 0; j < b->h; j++) {
            int y = b->h - j - 1;
            for(i = 0; i < b->w; i++) {
                int byt = y * rs + (i >> 3);
                int bit = 7 - (i % 8);
                uint8_t p = (data[byt] & (1 << bit)) >> bit;

                assert(p < dib.ncolors);
                bm_set(b, i, j, bm_palette_get(pal, p));
            }
        }
    } else if (dib.bitspp == 32) {
        for(j = 0; j < b->h; j++) {
            uint32_t y = b->h - j - 1;
            for(i = 0; i < b->w; i++) {
                uint32_t p = y * rs + i * 4;

                uint32_t* pixel = (uint32_t*)(data + p);
                uint32_t r_unc = (*pixel & rgbmask[0]) >> rgbshift[0];
                uint32_t g_unc = (*pixel & rgbmask[1]) >> rgbshift[1];
                uint32_t b_unc = (*pixel & rgbmask[2]) >> rgbshift[2];

                BM_SET_RGBA(b, i, j,
                    (unsigned char)(r_unc * rgbcorr[0]),
                    (unsigned char)(g_unc * rgbcorr[1]),
                    (unsigned char)(b_unc * rgbcorr[2]), 0xFF);
            }
        }
    } else {
        for(j = 0; j < b->h; j++) {
            uint32_t y = b->h - j - 1;
            for(i = 0; i < b->w; i++) {
                uint32_t p = y * rs + i * 3;
                BM_SET_RGBA(b, i, j, data[p+2], data[p+1], data[p+0], 0xFF);
            }
        }
    }

end:
    if(data)
        free(data);
    if(pal != NULL)
        bm_palette_release(pal);

    return b;
error:
    if(b)
        bm_free(b);
    b = NULL;
    goto end;
}

static int put_byte(char byte, bm_write_fun writef, void *context) {
    return writef(&byte, 1, context);
}

static int put_text(bm_write_fun writef, void *context, const char *fmt, ...) {
    char buffer[256];
    int len;
    va_list arg;
    va_start(arg, fmt);
    len = vsnprintf(buffer, sizeof buffer, fmt, arg);
    va_end(arg);
    return writef(buffer, len, context);
}

static int bm_file_cb(void *data, int len, void *context) {
    return fwrite(data, len, 1, (FILE *)context) == 1;
}

static int bm_save_bmp(Bitmap *b, bm_write_fun cb, void *context);
static int bm_save_gif(Bitmap *b, bm_write_fun cb, void *context);
static int bm_save_pcx(Bitmap *b, bm_write_fun cb, void *context);
static int bm_save_tga(Bitmap *b, bm_write_fun cb, void *context);
static int bm_save_ppm(Bitmap *b, bm_write_fun cb, void *context, const char *ext);
#ifdef USEPNG
static int bm_save_png(Bitmap *b, bm_write_fun cb, void *context);
#endif
#ifdef USEJPG
static int bm_save_jpg(Bitmap *b, bm_write_fun cb, void *context);
#endif

#ifdef USESTBW
#  define STB_IMAGE_WRITE_IMPLEMENTATION
#  include "stb_image_write.h"

struct stb_writer {
    bm_write_fun writef;
    void *context;
};

static void stb_writer_fun(void *context, void *data, int size) {
    struct stb_writer *writer = context;
    writer->writef(data, size, writer->context);
}
#endif

static void swap_stb_bytes(int w, int h, unsigned char *data) {
#if !ABGR
    /* Unfortunately, the R and B channels of stb_image are
        swapped from the format I'd prefer them in. */
    int i;
    for(i = 0; i < w * h * 4; i += 4) {
        unsigned char c = data[i];
        data[i] = data[i+2];
        data[i+2] = c;
    }
#endif
}

int bm_save_custom(Bitmap *b, bm_write_fun cb, void *context, const char *ext) {
    SET_ERROR("no error");
    if(!bm_stricmp(ext, "gif"))
        return bm_save_gif(b, cb, context);
    else if(!bm_stricmp(ext, "pcx"))
        return bm_save_pcx(b, cb, context);
    else if(!bm_stricmp(ext, "tga"))
        return bm_save_tga(b, cb, context);
    else if(!bm_stricmp(ext, "pbm") || !bm_stricmp(ext, "pgm") || !bm_stricmp(ext, "ppm"))
        return bm_save_ppm(b, cb, context, ext);
    else if(!bm_stricmp(ext, "png")) {
#ifdef USEPNG
        return bm_save_png(b, cb, context);
#elif defined(USESTBW)
        int r;
        struct stb_writer writer = {cb, context};
        swap_stb_bytes(b->w, b->h, b->data);
        r = stbi_write_png_to_func(stb_writer_fun, &writer, b->w, b->h, 4, b->data, b->w * 4);
        swap_stb_bytes(b->w, b->h, b->data);
        return r;
#else
        SET_ERROR("PNG support is not enabled");
        return 0;
#endif
    } else if(!bm_stricmp(ext, "jpg") || !bm_stricmp(ext, "jpeg")) {
#ifdef USEJPG
        return bm_save_jpg(b, cb, context);
#elif defined(USESTBW)
        int r;
        struct stb_writer writer = {cb, context};
        swap_stb_bytes(b->w, b->h, b->data);
        r = stbi_write_jpg_to_func(stb_writer_fun, &writer, b->w, b->h, 4, b->data, b->w * 4);
        swap_stb_bytes(b->w, b->h, b->data);
        return r;
#else
        SET_ERROR("JPEG support is not enabled");
        return 0;
#endif
    } else
        return bm_save_bmp(b, cb, context);
}

int bm_save(Bitmap *b, const char *fname) {
    int ret;
    FILE *f;

    /* Chooses the file type to save as based on the
    extension in the filename */
    char *lname = strdup(fname), *ext, *c;

    for(c = lname; *c; c++)
        *c = tolower(*c);
    ext = strrchr(lname, '.');
    if(ext)
        ext++;
    else
        ext = "bmp";

#ifdef SAFE_C11
    {
        errno_t err = fopen_s(&f, fname, "wb");
        if(err != 0) {
            SET_ERROR("unable to open file for output");
            return 0;
        }
    }
#else
    f = fopen(fname, "wb");
    if(!f) {
        SET_ERROR("unable to open file for output");
        return 0;
    }
#endif

    ret = bm_save_custom(b, bm_file_cb, f, ext);

    fclose(f);
    free(lname);

    return ret;
}

int bm_savef(Bitmap *b, const char *fmt, ...) {
    char fname[256];
    va_list arg;
    assert(b);
    va_start(arg, fmt);
    vsnprintf(fname, sizeof fname, fmt, arg);
    va_end(arg);
    return bm_save(b, fname);
}

static int bm_save_bmp(Bitmap *b, bm_write_fun writef, void *context) {
    SET_ERROR("no error");
    /* TODO: Now that I have a function to count colors, maybe
        I should choose to save a bitmap as 8-bit if there
        are <= 256 colors in the image? */

    struct bmpfile_magic magic = {{'B','M'}};
    struct bmpfile_header hdr;
    struct bmpfile_dibinfo dib;

    int rs, padding, i, j;
    unsigned char *data;

    padding = 4 - ((b->w * 3) % 4);
    if(padding > 3) padding = 0;
    rs = b->w * 3 + padding;
    assert(rs % 4 == 0);

    hdr.creator1 = 0;
    hdr.creator2 = 0;
    hdr.bmp_offset = sizeof magic + sizeof hdr + sizeof dib;

    dib.header_sz = sizeof dib;
    dib.width = b->w;
    dib.height = b->h;
    dib.nplanes = 1;
    dib.bitspp = 24;
    dib.compress_type = 0;
    dib.hres = 2835;
    dib.vres = 2835;
    dib.ncolors = 0;
    dib.nimpcolors = 0;

    dib.bmp_bytesz = rs * b->h;
    hdr.filesz = hdr.bmp_offset + dib.bmp_bytesz;

    if(!writef(&magic, sizeof magic, context)
    || !writef(&hdr, sizeof hdr, context)
    || !writef(&dib, sizeof dib, context)) {
        SET_ERROR("unable to write BMP header");
        return 0;
    }

    data = CAST(unsigned char *)(malloc(dib.bmp_bytesz));
    if(!data) {
        SET_ERROR("out of memory");
        return 0;
    }
    memset(data, 0x00, dib.bmp_bytesz);

    for(j = 0; j < b->h; j++) {
        for(i = 0; i < b->w; i++) {
            int p = ((b->h - (j) - 1) * rs + (i)*3);
            data[p+2] = BM_GETR(b, i, j);
            data[p+1] = BM_GETG(b, i, j);
            data[p+0] = BM_GETB(b, i, j);
        }
    }

    writef(data, dib.bmp_bytesz, context);

    free(data);
    return 1;
}

#ifdef USEPNG
/*
http://zarb.org/~gc/html/libpng.html
http://www.labbookpages.co.uk/software/imgProc/libPNG.html
*/
static Bitmap *bm_load_png_fp(FILE *f) {
    Bitmap *bmp = NULL;

    unsigned char header[8];
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep * volatile rows = NULL;

    volatile int w, h, ct, bpp, x, y, il, has_alpha = 0;

    const char * volatile error_message = "";

    if((fread(header, 1, 8, f) != 8) || png_sig_cmp(header, 0, 8)) {
        error_message = "fread on PNG header";
        goto error;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        error_message = "png_create_read_struct failed";
        goto error;
    }
    info = png_create_info_struct(png);
    if(!info) {
        error_message = "png_create_info_struct failed";
        goto error;
    }

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    png_init_io(png, f);
    png_set_sig_bytes(png, 8);

    error_message = "png_read_info failed";
    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    bpp = png_get_bit_depth(png, info);
    ct = png_get_color_type(png, info);
    il = png_get_interlace_type(png, info);

    if(bpp == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
        png_set_scale_16(png);
#else
        png_set_strip_16(png);
#endif
    }

    if(ct == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    } else if (ct == PNG_COLOR_TYPE_GRAY && bpp < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    } else if(ct == PNG_COLOR_TYPE_RGBA)
        has_alpha = 1;

    if(ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
        has_alpha = (ct == PNG_COLOR_TYPE_GRAY_ALPHA);
    }

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if(il)
        png_set_interlace_handling(png);

    error_message = "png_read_update_info failed";

    png_read_update_info(png, info);

    bmp = bm_create(w,h);

    error_message = "png_read_image failed";

    rows = ALLOCA(h * sizeof *rows);
    for(y = 0; y < h; y++)
        rows[y] = malloc(png_get_rowbytes(png,info));

    png_read_image(png, rows);

    /* Convert to my internal representation */
    if(has_alpha) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 4]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }
    } else {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 3]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], 0xFF);
            }
        }
    }

    goto done;
error:
    SET_ERROR(error_message);
    if(bmp) bm_free(bmp);
    bmp = NULL;
done:
    if (info != NULL) png_free_data(png, info, PNG_FREE_ALL, -1);
    if (png != NULL) png_destroy_read_struct(&png, &info, NULL);
    if(rows) {
        for(y = 0; y < h; y++) {
            free(rows[y]);
        }
        FREEA(rows);
    }
    return bmp;
}

struct Png_io_buffer {
    const unsigned char *buffer;
    size_t size;
    size_t position;
};

static void user_read_data(png_structp png, png_bytep data, size_t length) {
    struct Png_io_buffer *io = png_get_io_ptr(png);
    if(io->position + length >= io->size) {
        png_error(png, "not enough data in buffer");
    }
    memcpy(data, io->buffer + io->position, length);
    io->position += length;
}

static Bitmap *bm_load_png_mem(const unsigned char *inbuffer, size_t insize) {
    Bitmap *bmp = NULL;

    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep * volatile rows = NULL;

    struct Png_io_buffer io;
    io.buffer = inbuffer;
    io.size = insize;
    io.position = 0;

    volatile int w, h, ct, bpp, x, y, il, has_alpha = 0;

    const char * volatile error_message = "";

    if(png_sig_cmp(inbuffer, 0, 8)) {
        error_message = "invalid PNG header";
        goto error;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        error_message = "png_create_read_struct failed";
        goto error;
    }
    info = png_create_info_struct(png);
    if(!info) {
        error_message = "png_create_info_struct failed";
        goto error;
    }

    if(setjmp(png_jmpbuf(png))) {
        goto error;
    }

    /* See `VI. Modifying/Customizing libpng` in libpng-manual.txt */
    png_set_read_fn(png, &io, user_read_data);

    error_message = "png_read_info failed";
    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    bpp = png_get_bit_depth(png, info);
    ct = png_get_color_type(png, info);
    il = png_get_interlace_type(png, info);

    if(bpp == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
        png_set_scale_16(png);
#else
        png_set_strip_16(png);
#endif
    }

    if(ct == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    } else if (ct == PNG_COLOR_TYPE_GRAY && bpp < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    } else if(ct == PNG_COLOR_TYPE_RGBA)
        has_alpha = 1;

    if(ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
        has_alpha = (ct == PNG_COLOR_TYPE_GRAY_ALPHA);
    }

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if(il)
        png_set_interlace_handling(png);

    error_message = "png_read_update_info failed";

    png_read_update_info(png, info);

    bmp = bm_create(w,h);

    error_message = "png_read_image failed";

    rows = ALLOCA(h * sizeof *rows);
    for(y = 0; y < h; y++)
        rows[y] = malloc(png_get_rowbytes(png,info));

    png_read_image(png, rows);

    /* Convert to my internal representation */
    if(has_alpha) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 4]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }
    } else {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 3]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], 0xFF);
            }
        }
    }

    goto done;
error:
    SET_ERROR(error_message);
    if(bmp) bm_free(bmp);
    bmp = NULL;
done:
    if (info != NULL) png_free_data(png, info, PNG_FREE_ALL, -1);
    if (png != NULL) png_destroy_read_struct(&png, &info, NULL);
    if(rows) {
        for(y = 0; y < h; y++) {
            free(rows[y]);
        }
        FREEA(rows);
    }
    return bmp;
}

struct custom_png_writer {
    bm_write_fun writef;
    void *context;
};

static void png_write(png_structp png, png_bytep data, png_size_t length) {
    struct custom_png_writer *writer = png_get_io_ptr(png);
    writer->writef(data, (int)length, writer->context);
}

static void png_flush(png_structp png) {
    (void)png;
}

static int bm_save_png(Bitmap *b, bm_write_fun writef, void *context) {

    png_structp png = NULL;
    png_infop info = NULL;
    int y, rv;
    struct custom_png_writer writer = {writef, context};

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        SET_ERROR("png_create_write_struct failed");
        goto error;
    }

    info = png_create_info_struct(png);
    if(!info) {
        SET_ERROR("png_create_info_struct failed");
        goto error;
    }

    if(setjmp(png_jmpbuf(png))) {
        SET_ERROR("png_init_io failed");
        goto error;
    }

    png_set_write_fn(png, &writer, png_write, png_flush);

    if(setjmp(png_jmpbuf(png))) {
        SET_ERROR("png_write_row failed");
        goto error;
    }

    png_set_IHDR(png, info, b->w, b->h, 8, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
        PNG_FILTER_TYPE_BASE);

    png_write_info(png, info);

    png_bytep row = malloc(4 * b->w * sizeof *row);
    int x;
    for(y = 0; y < b->h; y++) {
        png_bytep r = row;
        for(x = 0; x < b->w; x++) {
            *r++ = BM_GETR(b,x,y);
            *r++ = BM_GETG(b,x,y);
            *r++ = BM_GETB(b,x,y);
            *r++ = BM_GETA(b,x,y);
        }
        png_write_row(png, row);
    }
     png_write_end(png, NULL);

    rv = 1;
    goto done;
error:
    rv = 0;
done:
    if(info) png_free_data(png, info, PNG_FREE_ALL, -1);
    if(png) png_destroy_write_struct(&png, NULL);
    return rv;
}
#endif

#ifdef USEJPG
struct jpg_err_handler {
    struct jpeg_error_mgr pub;
    jmp_buf jbuf;
};

static void jpg_on_error(j_common_ptr cinfo) {
    struct jpg_err_handler *err = (struct jpg_err_handler *) cinfo->err;
    longjmp(err->jbuf, 1);
}

static Bitmap *bm_load_jpg_fp(FILE *f) {
    struct jpeg_decompress_struct cinfo;
    struct jpg_err_handler jerr;
    Bitmap *bmp = NULL;
    int i, row_stride;
    unsigned int j;
    unsigned char *data;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        SET_ERROR("JPEG loading failed");
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, f);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;

    bmp = bm_create(cinfo.image_width, cinfo.image_height);
    if(!bmp) {
        SET_ERROR("out of memory");
        return NULL;
    }
    row_stride = bmp->w * 3;

    data = ALLOCA(row_stride);
    if(!data) {
        SET_ERROR("out of memory");
        return NULL;
    }
    memset(data, 0x00, row_stride);
    row_pointer[0] = data;

    jpeg_start_decompress(&cinfo);

    for(j = 0; j < cinfo.output_height; j++) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < bmp->w; i++) {
            unsigned char *ptr = &(data[i * 3]);
            BM_SET_RGBA(bmp, i, j, ptr[0], ptr[1], ptr[2], 0xFF);
        }
    }
    FREEA(data);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp;
}

static Bitmap *bm_load_jpg_mem(const unsigned char *inbuffer, size_t insize) {
    struct jpeg_decompress_struct cinfo;
    struct jpg_err_handler jerr;
    Bitmap *bmp = NULL;
    int i, row_stride;
    unsigned int j;
    unsigned char *data;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        SET_ERROR("JPEG loading failed");
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, inbuffer, insize);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;

    bmp = bm_create(cinfo.image_width, cinfo.image_height);
    if(!bmp) {
        SET_ERROR("out of memory");
        return NULL;
    }
    row_stride = bmp->w * 3;

    data = ALLOCA(row_stride);
    if(!data) {
        SET_ERROR("out of memory");
        return NULL;
    }
    memset(data, 0x00, row_stride);
    row_pointer[0] = data;

    jpeg_start_decompress(&cinfo);

    for(j = 0; j < cinfo.output_height; j++) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < bmp->w; i++) {
            unsigned char *ptr = &(data[i * 3]);
            BM_SET_RGBA(bmp, i, j, ptr[0], ptr[1], ptr[2], 0xFF);
        }
    }
    FREEA(data);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp;
}

/*
* https://github.com/kornelski/libjpeg/blob/master/libjpeg.doc#L1372
* `jdatadst.c` has an example of how `jpeg_stdio_dest()` works
*/
#define OUTPUT_BUF_SIZE 4096

struct custom_jpg_writer {
    struct jpeg_destination_mgr pub;

    bm_write_fun writef;
    void *context;

    JOCTET *buffer;
    size_t bufsize;
};

void jpg_custom_init_destination(j_compress_ptr cinfo) {
    struct custom_jpg_writer *dest = (struct custom_jpg_writer*)cinfo->dest;

    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

boolean jpg_custom_empty_output_buffer(j_compress_ptr cinfo) {
    struct custom_jpg_writer *dest = (struct custom_jpg_writer *) cinfo->dest;

    if(dest->writef(dest->buffer, OUTPUT_BUF_SIZE, dest->context) != 1) {
        ERREXIT(cinfo, JERR_FILE_WRITE);
    }

    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;

    return TRUE;
}

void jpg_custom_term_destination(j_compress_ptr cinfo) {
    struct custom_jpg_writer *dest = (struct custom_jpg_writer *) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;

    if(datacount > 0) {
        if(dest->writef(dest->buffer, datacount, dest->context) != 1) {
            ERREXIT(cinfo, JERR_FILE_WRITE);
        }
    }
}

static void custom_jpg_dest(j_compress_ptr cinfo, bm_write_fun writef, void *context) {
    struct custom_jpg_writer *dest;

    if(cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr*)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof (struct custom_jpg_writer));
    }
    dest = (struct custom_jpg_writer *)cinfo->dest;
    dest->pub.init_destination = jpg_custom_init_destination;
    dest->pub.empty_output_buffer = jpg_custom_empty_output_buffer;
    dest->pub.term_destination = jpg_custom_term_destination;
    dest->writef = writef;
    dest->context = context;
}

static int bm_save_jpg(Bitmap *b, bm_write_fun writef, void *context) {
    struct jpeg_compress_struct cinfo;
    struct jpg_err_handler jerr;
    int i, j;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned char *data;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        SET_ERROR("JPEG saving failed");
        jpeg_destroy_compress(&cinfo);
        return 0;
    }
    jpeg_create_compress(&cinfo);

    custom_jpg_dest(&cinfo, writef, context);

    cinfo.image_width = b->w;
    cinfo.image_height = b->h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    /*jpeg_set_quality(&cinfo, 100, TRUE);*/

    row_stride = b->w * 3;

    data = malloc(row_stride);
    if(!data) {
        SET_ERROR("out of memory");
        return 0;
    }
    memset(data, 0x00, row_stride);

    jpeg_start_compress(&cinfo, TRUE);
    for(j = 0; j < b->h; j++) {
        for(i = 0; i < b->w; i++) {
            data[i*3+0] = BM_GETR(b, i, j);
            data[i*3+1] = BM_GETG(b, i, j);
            data[i*3+2] = BM_GETB(b, i, j);
        }
        row_pointer[0] = data;
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    free(data);

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    return 1;
}
#endif

#ifdef USESDL
/* Some functions to load graphics through the SDL_RWops
    related functions.
*/
#  ifdef USEPNG
static Bitmap *bm_load_png_rw(SDL_RWops *rw);
#  endif
#  ifdef USEJPG
static Bitmap *bm_load_jpg_rw(SDL_RWops *rw);
#  endif

Bitmap *bm_load_rw(SDL_RWops *rw) {
    SET_ERROR("no error");
    unsigned char magic[3];
    long start = SDL_RWtell(rw);
    long isbmp = 0, ispng = 0, isjpg = 0, ispcx = 0, isgif = 0;
    if(SDL_RWread(rw, magic, sizeof magic, 1) == 1) {
        if(!memcmp(magic, "BM", 2))
            isbmp = 1;
        else if(!memcmp(magic, "GIF", 3))
            isgif = 1;
        else if(magic[0] == 0xFF && magic[1] == 0xD8)
            isjpg = 1;
        else if(magic[0] == 0x0A)
            ispcx = 1;
        else
            ispng = 1; /* Assume PNG by default.
                    JPG and BMP magic numbers are simpler */
    }
    SDL_RWseek(rw, start, RW_SEEK_SET);

    if(isjpg) {
#  ifdef USEJPG
        return bm_load_jpg_rw(rw);
#  else
        (void)isjpg;
        SET_ERROR("JPEG support is not enabled");
        return NULL;
#  endif
    }
    if(ispng) {
#  ifdef USEPNG
        return bm_load_png_rw(rw);
#  else
        (void)ispng;
        SET_ERROR("PNG support is not enabled");
        return NULL;
#  endif
    }
    if(isgif) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_gif_rd(rd);
    }
    if(ispcx) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_pcx_rd(rd);
    }
    if(isbmp) {
        BmReader rd = make_rwops_reader(rw);
        return bm_load_bmp_rd(rd);
    }
    SET_ERROR("unsupported filetype");
    return NULL;
}

#  ifdef USEPNG
/*
Code to read a PNG from a SDL_RWops
http://www.libpng.org/pub/png/libpng-1.2.5-manual.html
http://blog.hammerian.net/2009/reading-png-images-from-memory/
*/
static void read_rwo_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    SDL_RWops *rw = png_get_io_ptr(png_ptr);
    SDL_RWread(rw, data, 1, length);
}

static Bitmap *bm_load_png_rw(SDL_RWops *rw) {
        Bitmap *bmp = NULL;

    unsigned char header[8];
    png_structp png = NULL;
    png_infop info = NULL;
    int number_of_passes;
    png_bytep * rows = NULL;

    int w, h, ct, bpp, x, y;

    if((SDL_RWread(rw, header, 1, 8) != 8) || png_sig_cmp(header, 0, 8)) {
        SET_ERROR("SDL_RWread on PNG header");
        goto error;
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!png) {
        SET_ERROR("png_create_read_struct failed");
        goto error;
    }
    info = png_create_info_struct(png);
    if(!info) {
        SET_ERROR("png_create_info_struct failed");
        goto error;
    }
    if(setjmp(png_jmpbuf(png))) {
        SET_ERROR("png_read_info failed");
        goto error;
    }

    png_init_io(png, NULL);
    png_set_read_fn(png, rw, read_rwo_data);

    png_set_sig_bytes(png, 8);

    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    bpp = png_get_bit_depth(png, info);
    ct = png_get_color_type(png, info);
    il = png_get_interlace_type(png, info);

    /* FIXME: I did encounter some 8-bit PNGs in the wild that failed here... */
    if(bpp == 16) {
#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
        png_set_scale_16(png);
#else
        png_set_strip_16(png);
#endif
    }

    if(ct == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (ct == PNG_COLOR_TYPE_GRAY && bpp < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if(ct == PNG_COLOR_TYPE_GRAY)
        png_set_gray_to_rgb(png);

    if(png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    if(ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    if(il)
        png_set_interlace_handling(png);

    png_read_update_info(png, info);

    bmp = bm_create(w,h);

    if(setjmp(png_jmpbuf(png))) {
        SET_ERROR("png_read_image failed");
        goto error;
    }

    rows = malloc(h * sizeof *rows);
    for(y = 0; y < h; y++)
        rows[y] = malloc(png_get_rowbytes(png,info));

    png_read_image(png, rows);

    /* Convert to my internal representation */
    if(ct == PNG_COLOR_TYPE_RGBA) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 4]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }
        }
    } else if(ct == PNG_COLOR_TYPE_RGB) {
        for(y = 0; y < h; y++) {
            png_byte *row = rows[y];
            for(x = 0; x < w; x++) {
                png_byte *ptr = &(row[x * 3]);
                BM_SET_RGBA(bmp, x, y, ptr[0], ptr[1], ptr[2], 0xFF);
            }
        }
    }

    goto done;
error:
    if(bmp) bm_free(bmp);
    bmp = NULL;
done:
    if (info != NULL) png_free_data(png, info, PNG_FREE_ALL, -1);
    if (png != NULL) png_destroy_read_struct(&png, NULL, NULL);
    if(rows) {
        for(y = 0; y < h; y++) {
            free(rows[y]);
        }
        free(rows);
    }
    return bmp;
}
#  endif /* USEPNG */
#  ifdef USEJPG

/*
Code to read a JPEG from an SDL_RWops.
Refer to jdatasrc.c in libjpeg's code.
See also
http://www.cs.stanford.edu/~acoates/decompressJpegFromMemory.txt
*/
#define JPEG_INPUT_BUFFER_SIZE  4096
struct rw_jpeg_src_mgr {
    struct jpeg_source_mgr pub;
    SDL_RWops *rw;
    JOCTET *buffer;
    boolean start_of_file;
};

static void rw_init_source(j_decompress_ptr cinfo) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    src->start_of_file = TRUE;
}

static boolean rw_fill_input_buffer(j_decompress_ptr cinfo) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    size_t nbytes = SDL_RWread(src->rw, src->buffer, 1, JPEG_INPUT_BUFFER_SIZE);

    if(nbytes <= 0) {
        /*if(src->start_of_file)
            ERREXIT(cinfo, JERR_INPUT_EMPTY);
        WARNMS(cinfo, JWRN_JPEG_EOF);*/
        src->buffer[0] = (JOCTET)0xFF;
        src->buffer[1] = (JOCTET)JPEG_EOI;
        nbytes = 2;
    }

    src->pub.next_input_byte = src->buffer;
    src->pub.bytes_in_buffer = nbytes;

    src->start_of_file = TRUE;
    return TRUE;
}

static void rw_skip_input_data(j_decompress_ptr cinfo, long nbytes) {
    struct rw_jpeg_src_mgr *src = (struct rw_jpeg_src_mgr *)cinfo->src;
    if(nbytes > 0) {
        while(nbytes > src->pub.bytes_in_buffer) {
            nbytes -= src->pub.bytes_in_buffer;
            (void)(*src->pub.fill_input_buffer)(cinfo);
        }
        src->pub.next_input_byte += nbytes;
        src->pub.bytes_in_buffer -= nbytes;
    }
}

static void rw_term_source(j_decompress_ptr cinfo) {
    /* Apparently nothing to do here */
}

static void rw_set_source_mgr(j_decompress_ptr cinfo, SDL_RWops *rw) {
    struct rw_jpeg_src_mgr *src;
    if(!cinfo->src) {
        cinfo->src = (struct jpeg_source_mgr *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof *src);
        src = (struct rw_jpeg_src_mgr *)cinfo->src;
        src->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr)cinfo, JPOOL_PERMANENT, JPEG_INPUT_BUFFER_SIZE * sizeof(JOCTET));
    }

    src = (struct rw_jpeg_src_mgr *)cinfo->src;

    src->pub.init_source = rw_init_source;
    src->pub.fill_input_buffer = rw_fill_input_buffer;
    src->pub.skip_input_data = rw_skip_input_data;
    src->pub.term_source = rw_term_source;
    src->pub.resync_to_restart = jpeg_resync_to_restart;

    src->pub.bytes_in_buffer = 0;
    src->pub.next_input_byte = NULL;

    src->rw = rw;
}

static Bitmap *bm_load_jpg_rw(SDL_RWops *rw) {
    struct jpeg_decompress_struct cinfo;
    struct jpg_err_handler jerr;
    Bitmap *bmp = NULL;
    int i, j, row_stride;
    unsigned char *data;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpg_on_error;
    if(setjmp(jerr.jbuf)) {
        SET_ERROR("JPEG loading failed");
        jpeg_destroy_decompress(&cinfo);
        return NULL;
    }
    jpeg_create_decompress(&cinfo);

    /* jpeg_stdio_src(&cinfo, f); */
    rw_set_source_mgr(&cinfo, rw);

    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;

    bmp = bm_create(cinfo.image_width, cinfo.image_height);
    if(!bmp) {
        SET_ERROR("out of memory");
        return NULL;
    }
    row_stride = bmp->w * 3;

    data = ALLOCA(row_stride);
    if(!data) {
        SET_ERROR("out of memory");
        return NULL;
    }
    memset(data, 0x00, row_stride);
    row_pointer[0] = data;

    jpeg_start_decompress(&cinfo);

    for(j = 0; j < cinfo.output_height; j++) {
        jpeg_read_scanlines(&cinfo, row_pointer, 1);
        for(i = 0; i < bmp->w; i++) {
            unsigned char *ptr = &(data[i * 3]);
            BM_SET_RGBA(bmp, i, j, ptr[0], ptr[1], ptr[2], 0xFF);
        }
    }
    FREEA(data);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bmp;
}
#  endif /* USEJPG */

#endif /* USESDL */

/* These functions are used for the palettes in my GIF and PCX support: */

struct palette_mapping {
    unsigned int color;
    int index;
};

static int mapping_color_comp(const void *ap, const void *bp) {
    const struct palette_mapping *a = ap, *b = bp;
    return (a->color & 0x00FFFFFF) - (b->color & 0x00FFFFFF);
}

static int make_palette_mapping(BmPalette *palette, struct palette_mapping mapping[256], int *count) {
    int i;
    *count = bm_palette_count(palette);

    assert(*count > 0 && *count <= 256);
    for(i = 0; i < *count; i++) {
        mapping[i].color = bm_palette_get(palette, i);
        mapping[i].index = i;
    }
    qsort(mapping, *count, sizeof mapping[0], mapping_color_comp);
    return 1;
}

static int get_palette_mapping(struct palette_mapping mapping[256], unsigned int color, int count) {
    struct palette_mapping key = {color, 0}, *found;
    found = bsearch(&key, mapping, count, sizeof key, mapping_color_comp);
    if(!found)
        return -1;
    return found->index;
}

static void triplets_from_palette(BmPalette *palette, struct rgb_triplet rgb[256]){
    int i;
    memset(rgb, 0, 256 * sizeof *rgb);
    for(i = 0; i < bm_palette_count(palette); i++) {
        bm_get_rgb(bm_palette_get(palette, i), &rgb[i].r, &rgb[i].g, &rgb[i].b);
    }
}

/* GIF support
http://www.w3.org/Graphics/GIF/spec-gif89a.txt
Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011
http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp
*/

enum gif_type {gif_87a, gif_89a};

#pragma pack(push, 1) /* Don't use any padding */
typedef struct {

    /* header */
    struct {
        char signature[3];
        char version[3];
    } header;

    enum gif_type version;

    /* logical screen descriptor */
    struct {
        unsigned short width;
        unsigned short height;
        unsigned char fields;
        unsigned char background;
        unsigned char par; /* pixel aspect ratio */
    } lsd;

    Bitmap *bmp;

} GIF;

/* GIF Graphic Control Extension */
typedef struct {
    unsigned char block_size;
    unsigned char fields;
    unsigned short delay;
    unsigned char trans_index;
    unsigned char terminator;
} GIF_GCE;

/* GIF Image Descriptor */
typedef struct {
    unsigned char separator;
    unsigned short left;
    unsigned short top;
    unsigned short width;
    unsigned short height;
    unsigned char fields;
} GIF_ID;

/* GIF Application Extension */
typedef struct {
    unsigned char block_size;
    char app_id[8];
    char auth_code[3];
} GIF_APP_EXT;

/* GIF text extension */
typedef struct {
    unsigned char block_size;
    unsigned short grid_left;
    unsigned short grid_top;
    unsigned short grid_width;
    unsigned short grid_height;
    unsigned char text_fg;
    unsigned char text_bg;
} GIF_TXT_EXT;
#pragma pack(pop)

static int gif_read_image(BmReader rd, GIF *gif, BmPalette *pal);
static int gif_read_tbid(BmReader rd, GIF *gif, GIF_ID *gif_id, GIF_GCE *gce, BmPalette *pal);
static unsigned char *gif_data_sub_blocks(BmReader rd, int *r_tsize);
static unsigned char *lzw_decode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len);

static Bitmap *bm_load_gif_rd(BmReader rd) {
    GIF gif;

    /* From the packed fields in the logical screen descriptor */
    unsigned gct, sgct;

    BmPalette *pal = NULL;

    unsigned char trailer;

    gif.bmp = NULL;

    /* Section 17. Header. */
    if(rd.fread(&gif.header, sizeof gif.header, 1, rd.data) != 1) {
        SET_ERROR("unable to read GIF header");
        return NULL;
    }
    if(memcmp(gif.header.signature, "GIF", 3)){
        SET_ERROR("bad GIF signature");
        return NULL;
    }
    if(!memcmp(gif.header.version, "87a", 3)){
        gif.version = gif_87a;
    } else if(!memcmp(gif.header.version, "89a", 3)){
        gif.version = gif_89a;
    } else {
        SET_ERROR("unable to determine GIF version");
        return NULL;
    }

    /* Section 18. Logical Screen Descriptor. */

    /* Ugh, I once used a compiler that added a padding byte */
    assert(sizeof gif.lsd == 7);
    assert(sizeof(struct rgb_triplet) == 3);

    if(rd.fread(&gif.lsd, sizeof gif.lsd, 1, rd.data) != 1) {
        SET_ERROR("unable to read GIF LSD");
        return NULL;
    }

    gct = !!(gif.lsd.fields & 0x80);
    sgct = gif.lsd.fields & 0x07;

    if(gct) {
        /* raise 2 to the power of [sgct+1] */
        sgct = 1 << (sgct + 1);
        assert(sgct <= 256);
    }

    gif.bmp = bm_create(gif.lsd.width, gif.lsd.height);

    if(gct) {
        /* Section 19. Global Color Table. */
        struct rgb_triplet *bg, palette[256];
        unsigned int i;

        if(rd.fread(palette, sizeof *palette, sgct, rd.data) != sgct) {
            SET_ERROR("unable to read GIF palette");
            return NULL;
        }

        /* Set the Bitmap's color to the background color.*/
        bg = &palette[gif.lsd.background];
        SET_COLOR_RGB(gif.bmp, bg->r, bg->g, bg->b);
        bm_clear(gif.bmp);
        SET_COLOR_RGB(gif.bmp, 0, 0, 0);
        bm_set_alpha(gif.bmp, 0);

        pal = bm_palette_create(sgct);
        if(!pal)
            return NULL;
        for(i = 0; i < sgct; i++)
            bm_palette_set(pal, i, (palette[i].r << 16) | (palette[i].g << 8) | palette[i].b);

    } else {
        /* what? */
        SET_ERROR("don't know what to do about GIF palette");
        pal = bm_palette_create(sgct);
        if(!pal)
            return NULL;
    }

    bm_set_palette(gif.bmp, pal);
    bm_palette_release(pal);

    for(;;) {
        long pos = rd.ftell(rd.data);
        if(!gif_read_image(rd, &gif, pal)) {
            rd.fseek(rd.data, pos, SEEK_SET);
            break;
        }
    }

    /* Section 27. Trailer. */
    if((rd.fread(&trailer, 1, 1, rd.data) != 1) || trailer != 0x3B) {
        bm_free(gif.bmp);
        SET_ERROR("unable to read GIF trailer");
        return NULL;
    }

    return gif.bmp;
}

static int gif_read_extension(BmReader rd, GIF_GCE *gce) {
    unsigned char introducer, label;

    if((rd.fread(&introducer, 1, 1, rd.data) != 1) || introducer != 0x21) {
        SET_ERROR("couldn't read GIF extension introducer");
        return 0;
    }
    if(rd.fread(&label, 1, 1, rd.data) != 1) {
        SET_ERROR("couldn't read GIF extension label");
        return 0;
    }

    if(label == 0xF9) {
        /* 23. Graphic Control Extension. */
        if(rd.fread(gce, sizeof *gce, 1, rd.data) != 1) {
            SET_ERROR("couldn't read GIF graphic control extension");
            return 0;
        }
    } else if(label == 0xFE) {
        /* Section 24. Comment Extension. */
        int len;
        if(!gif_data_sub_blocks(rd, &len)) {
            SET_ERROR("couldn't read GIF comment extension");
            return 0;
        }
    } else if(label == 0x01) {
        /* Section 25. Plain Text Extension. */
        GIF_TXT_EXT te;
        int len;
        if(rd.fread(&te, sizeof te, 1, rd.data) != 1) {
            SET_ERROR("couldn't read GIF plain text extension");
            return 0;
        }
        if(!gif_data_sub_blocks(rd, &len)) return 0;
    } else if(label == 0xFF) {
        /* Section 26. Application Extension. */
        GIF_APP_EXT ae;
        int len;
        if(rd.fread(&ae, sizeof ae, 1, rd.data) != 1) {
            SET_ERROR("couldn't read GIF application extension");
            return 0;
        }
        if(!gif_data_sub_blocks(rd, &len)) return 0; /* Skip it */
    } else {
        return 0;
    }
    return 1;
}

/* Section 20. Image Descriptor. */
static int gif_read_image(BmReader rd, GIF *gif, BmPalette *pal) {
    GIF_GCE gce;
    GIF_ID gif_id;
    int rv = 1;

    /* Packed fields in the Image Descriptor */
    unsigned int lct, slct;

    memset(&gce, 0, sizeof gce);

    if(gif->version >= gif_89a) {
        for(;;) {
            long pos = rd.ftell(rd.data);
            if(!gif_read_extension(rd, &gce)) {
                SET_ERROR("unable to read GIF extension");
                rd.fseek(rd.data, pos, SEEK_SET);
                break;
            }
        }
    }

    if(rd.fread(&gif_id, sizeof gif_id, 1, rd.data) != 1) {
        SET_ERROR("no more blocks to read");
        return 0; /* no more blocks to read */
    }

    if(gif_id.separator != 0x2C) {
        SET_ERROR("GIF separator not 0x2C as expected");
        return 0;
    }

    lct = !!(gif_id.fields & 0x80);
    slct = gif_id.fields & 0x07;
    if(lct) {
        /* Section 21. Local Color Table. */
        struct rgb_triplet palette[256];
        unsigned int i;

        /* raise 2 to the power of [slct+1] */
        slct = 1 << (slct + 1);
        assert(slct <= 256);

        if(rd.fread(palette, sizeof *palette, slct, rd.data) != slct) {
            SET_ERROR("couldn't read GIF LCT");
            return 0;
        }

        pal = bm_palette_create(slct);
        if(!pal)
            return 0;
        for(i = 0; i < slct; i++)
            bm_palette_set(pal, i, (palette[i].r << 16) | (palette[i].g << 8) | palette[i].b);
    }

    if(!gif_read_tbid(rd, gif, &gif_id, &gce, pal)) {
        SET_ERROR("unable to read GIF TBID");
        rv = 0; /* what? */
    }

    if(lct) {
        assert(pal != gif->bmp->palette);
        bm_palette_release(pal);
    }

    return rv;
}

/* Section 15. Data Sub-blocks. */
static unsigned char *gif_data_sub_blocks(BmReader rd, int *r_tsize) {
    unsigned char *buffer = NULL, *pos, size;
    int tsize = 0;

    if(r_tsize)
        *r_tsize = 0;

    if(rd.fread(&size, 1, 1, rd.data) != 1) {
        SET_ERROR("error reading GIF subblock size");
        return NULL;
    }
    unsigned char *tbuf = CAST(unsigned char*)(realloc(buffer, 1));
    if (!tbuf) {
        free(buffer);
        return NULL;
    }
    buffer = tbuf;

    while(size > 0) {
        tbuf = CAST(unsigned char*)(realloc(buffer, tsize + size + 1));
        if (!tbuf) {
            free(buffer);
            return NULL;
        }
        buffer = tbuf;

        pos = buffer + tsize;

        if(rd.fread(pos, sizeof *pos, size, rd.data) != size) {
            SET_ERROR("error reading GIF subblock");
            free(buffer);
            return NULL;
        }

        tsize += size;
        if(rd.fread(&size, 1, 1, rd.data) != 1) {
            SET_ERROR("error reading GIF subblock");
            free(buffer);
            return NULL;
        }
    }

    if(r_tsize)
        *r_tsize = tsize;
    buffer[tsize] = '\0';
    return buffer;
}

/* Section 22. Table Based Image Data. */
static int gif_read_tbid(BmReader rd, GIF *gif, GIF_ID *gif_id, GIF_GCE *gce, BmPalette *pal) {
    int len, rv = 1;
    unsigned char *bytes, min_code_size;

    if(rd.fread(&min_code_size, 1, 1, rd.data) != 1) {
        return 0;
    }

    bytes = gif_data_sub_blocks(rd, &len);
    if(bytes && len > 0) {
        int i, outlen, x, y;

        /* Packed fields in the Graphic Control Extension */
        int intl, dispose = 0, trans_flag = 0;

        intl = !!(gif_id->fields & 0x40); /* Interlaced? */

        if(gce->block_size) {
            /* gce->block_size will be 4 if the GCE is present, 0 otherwise */
            dispose = (gce->fields >> 2) & 0x07;
            trans_flag = gce->fields & 0x01;
            if(trans_flag) {
                /* Mmmm, my bitmap module won't be able to handle
                    situations where different image blocks in the
                    GIF has different transparent colors */
                bm_set_color(gif->bmp, bm_palette_get(pal, gce->trans_index));
            }
        }

        if(gif_id->top + gif_id->height > gif->bmp->h ||
            gif_id->left + gif_id->width > gif->bmp->w) {
            /* This image descriptor doesn't fall within the bounds of the image */
            return 0;
        }

        if(dispose == 2) {
            /* Restore the background color */
            for(y = 0; y < gif_id->height; y++) {
                for(x = 0; x < gif_id->width; x++) {
                    bm_set(gif->bmp, x + gif_id->left, y + gif_id->top, gif->bmp->color);
                }
            }
        } else if(dispose != 3) {
            /* dispose = 0 or 1; if dispose is 3, we leave ignore the new image */
            unsigned char *decoded = lzw_decode_bytes(bytes, len, min_code_size, &outlen);
            if(decoded) {
                if(outlen != gif_id->width * gif_id->height) {
                    /* Shouldn't happen unless the file is corrupt */
                    SET_ERROR("error decoding GIF LZW");
                    rv = 0;
                } else {
                    /* Vars for interlacing: */
                    int grp = 1, /* Group we're in */
                        inty = 0, /* Y we're currently at */
                        inti = 8, /* amount by which we should increment inty */
                        truey; /* True Y, taking interlacing and the image descriptor into account */
                    for(i = 0, y = 0; y < gif_id->height && rv; y++) {
                        /* Appendix E. Interlaced Images. */
                        if(intl) {
                            truey = inty + gif_id->top;
                            inty += inti;
                            if(inty >= gif_id->height) {
                                switch(++grp) {
                                    case 2: inti = 8; inty = 4; break;
                                    case 3: inti = 4; inty = 2; break;
                                    case 4: inti = 2; inty = 1;break;
                                }
                            }
                        } else {
                            truey = y + gif_id->top;
                        }
                        assert(truey >= 0 && truey < gif->bmp->h);
                        for(x = 0; x < gif_id->width && rv; x++, i++) {
                            int c = decoded[i];
                            if(c < bm_palette_count(pal)) {
                                assert(x + gif_id->left >= 0 && x + gif_id->left < gif->bmp->w);
                                unsigned int col = bm_palette_get(pal, c);
                                if(trans_flag && col == gce->trans_index) {
                                    bm_set(gif->bmp, x + gif_id->left, truey, col & 0xFFFFFF);
                                } else {
                                    bm_set(gif->bmp, x + gif_id->left, truey, col | 0xFF000000);
                                }
                            } else {
                                /* Decode error */
                                SET_ERROR("invalid color value encountered");
                                rv = 0;
                            }
                        }
                    }
                }
                free(decoded);
            }
        }
        free(bytes);
    }
    return rv;
}

typedef struct {
    int prev;
    int code;
} gif_dict;

static int lzw_read_code(unsigned char bytes[], int bits, int *pos) {
    int i, bi, code = 0;
    assert(pos);
    for(i = *pos, bi=1; i < *pos + bits; i++, bi <<=1) {
        int byte = i >> 3;
        int bit = i & 0x07;
        if(bytes[byte] & (1 << bit))
            code |= bi;
    }
    *pos = i;
    return code;
}

static unsigned char *lzw_decode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len) {
    unsigned char *out = NULL;
    int out_size = 32;
    int outp = 0;

    int base_size = code_size;

    int pos = 0, code, old = -1;

    /* Clear and end of stream codes */
    int clr = 1 << code_size;
    int end = clr + 1;

    /* Dictionary */
    int di, dict_size = 1 << (code_size + 1);
    gif_dict *dict = CAST(gif_dict*)(realloc(NULL, dict_size * sizeof *dict));
    if (!dict)
        return NULL;

    /* Stack so we don't need to recurse down the dictionary */
    int stack_size = 2;
    unsigned char *stack = CAST(unsigned char *)(realloc(NULL, stack_size));
    if (!stack) {
        free(dict);
        return NULL;
    }
    int sp = 0;
    int sym = -1, ptr;

    *out_len = 0;
    out = CAST(unsigned char *)(realloc(NULL, out_size));
    if (!out) {
        free(dict);
        free(stack);
        return NULL;
    }

    /* Initialize the dictionary */
    for(di = 0; di < dict_size; di++) {
        dict[di].prev = -1;
        dict[di].code = di;
    }
    di = end + 1;

    code = lzw_read_code(bytes, code_size + 1, &pos);
    while((pos >> 3) <= data_len + 1) {
        if(code == clr) {
            code_size = base_size;
            dict_size = 1 << (code_size + 1);
            di = end + 1;
            code = lzw_read_code(bytes, code_size + 1, &pos);
            old = -1;
            continue;
        } else if(code == end) {
            break;
        }

        if(code > di) {
            /* Shouldn't happen, unless file corrupted */
            free(out);
            free(dict);
            free(stack);
            return NULL;
        }

        if(code == di) {
            /* Code is not in the table */
            ptr = old;
            stack[sp++] = sym;
        } else {
            /* Code is in the table */
            ptr = code;
        }

        /* Walk down the dictionary and push the codes onto a stack */
        while(ptr >= 0) {
            stack[sp++] = dict[ptr].code;
            if(sp == stack_size) {
                stack_size <<= 1;
                unsigned char *tbuf = CAST(unsigned char*)(realloc(stack, stack_size));
                if (!tbuf) {
                    free(stack);
                    return NULL;
                }
                stack = tbuf;
            }
            ptr = dict[ptr].prev;
        }
        sym = stack[sp-1];

        /* Output the decoded bytes */
        while(sp > 0) {
            out[outp++] = stack[--sp];
            if(outp == out_size) {
                out_size <<= 1;
                unsigned char *tbuf = CAST(unsigned char*)(realloc(out, out_size));
                if (!tbuf) {
                    free(out);
                    return NULL;
                }
                out = tbuf;
            }
        }

        /* update the dictionary */
        if(old >= 0) {
            if(di < dict_size) {
                dict[di].prev = old;
                dict[di].code = sym;
                di++;
            }
            /* Resize the dictionary? */
            if(di == dict_size && code_size < 11) {
                code_size++;
                dict_size = 1 << (code_size + 1);
                gif_dict *tmp = CAST(gif_dict*)(realloc(dict, dict_size * sizeof *dict));
                if (!tmp) {
                    free(dict);
                    return NULL;
                }
                dict = tmp;
            }
        }

        old = code;
        code = lzw_read_code(bytes, code_size + 1, &pos);
    }
    free(stack);
    free(dict);

    *out_len = outp;
    return out;
}

static void lzw_emit_code(unsigned char **buffer, int *buf_size, int *pos, int c, int bits) {
    int i, m;
    for(i = *pos, m = 1; i < *pos + bits; i++, m <<= 1) {
        int byte = i >> 3;
        int bit = i & 0x07;
        if(!bit) {
            if(byte == *buf_size) {
                *buf_size <<= 1;
                unsigned char *tmp = CAST(unsigned char *)(realloc(*buffer, *buf_size));
                if (!tmp) {
                    free(*buffer);
                    return;
                }
                *buffer = tmp;
            }
            assert(byte < *buf_size);
            (*buffer)[byte] = 0x00;
        }
        if (c & m) {
            assert(byte < *buf_size);
            (*buffer)[byte] |= (1 << bit);
        }
    }
    *pos += bits;
}

static unsigned char *lzw_encode_bytes(unsigned char *bytes, int data_len, int code_size, int *out_len) {
    int base_size = code_size;

    /* Clear and end of stream codes */
    int clr = 1 << code_size;
    int end = clr + 1;

    /* dictionary */
    int i, di, dict_size = 1 << (code_size + 1);
    gif_dict *dict = CAST(gif_dict*)(realloc(NULL, dict_size * sizeof *dict));
    if (!dict)
        return NULL;

    int buf_size = 4;
    int pos = 0;
    unsigned char *buffer = CAST(unsigned char *)(realloc(NULL, buf_size));
    if (!buffer) {
        free(dict);
        return NULL;
    }

    int ii, string, prev, tlen;

    *out_len = 0;

    /* Initialize the dictionary */
    for(di = 0; di < dict_size; di++) {
        dict[di].prev = -1;
        dict[di].code = di;
    }
    di = end+1;

    dict[clr].prev = -1;
    dict[clr].code = -1;
    dict[end].prev = -1;
    dict[end].code = -1;

    string = -1;
    prev = clr;

    lzw_emit_code(&buffer, &buf_size, &pos, clr, code_size + 1);

    for(ii = 0; ii < data_len; ii++) {
        int character, res;
reread:
        character = bytes[ii];

        /* Find it in the dictionary; If the entry is in the dict, it can't be
        before dict[string], therefore we can eliminate the first couple of entries. */
        for(res = -1, i = string>0?string:0; i < di; i++) {
            assert(i < dict_size);
            if(dict[i].prev == string && dict[i].code == character) {
                res = i;
                break;
            }
        }

        if(res >= 0) {
            /* Found */
            string = res;
            prev = res;
        } else {
            /* Not found */
            lzw_emit_code(&buffer, &buf_size, &pos, prev, code_size + 1);

            /* update the dictionary */
            if(di == dict_size) {
                /* Resize the dictionary */
                if(code_size < 11) {
                    code_size++;
                    dict_size = 1 << (code_size + 1);
                    gif_dict *tmp = CAST(gif_dict*)(realloc(dict, dict_size * sizeof *dict));
                    if (!tmp) {
                        free(dict);
                        return NULL;
                    }
                    dict = tmp;
                } else {
                    /* lzw_emit_code a clear code */
                    lzw_emit_code(&buffer, &buf_size, &pos, clr,code_size + 1);
                    code_size = base_size;
                    dict_size = 1 << (code_size + 1);
                    di = end + 1;
                    string = -1;
                    prev = clr;
                    goto reread;
                }
            }

            dict[di].prev = string;
            dict[di].code = character;
            di++;

            string = character;
            prev = character;
        }
    }

    lzw_emit_code(&buffer, &buf_size, &pos, prev, code_size + 1);
    lzw_emit_code(&buffer, &buf_size, &pos, end, code_size + 1);

    /* Total length */
    tlen = (pos >> 3);
    if(pos & 0x07) tlen++;
    *out_len = tlen;

    free(dict);

    return buffer;
}

static int bm_save_gif(Bitmap *b, bm_write_fun writef, void *context) {
    GIF gif;
    GIF_GCE gce;
    GIF_ID gif_id;
    int sgct, bg;
    unsigned char code_size = 0x08;

    BmPalette *palette;
    struct palette_mapping mapping[256];
    int color_count;

    /* For encoding */
    int len = 0, x, y, p;
    unsigned char *bytes, *pixels;

    memcpy(gif.header.signature, "GIF", 3);
    memcpy(gif.header.version, "89a", 3);
    gif.version = gif_89a;
    gif.lsd.width = b->w;
    gif.lsd.height = b->h;
    gif.lsd.background = 0;
    gif.lsd.par = 0;

    /* Using global color table, color resolution = 8-bits */
    gif.lsd.fields = 0xF0;

    palette = bm_get_palette(b);
    if(!palette) {
        if(!bm_make_palette(b))
            return 0;
        palette = bm_get_palette(b);
        assert(palette);
    }

    if(bm_palette_count(palette) > 256) {
        SET_ERROR("too many palette colors to save GIF");
        return 0;
    }

    /* Copy the image and dither it to match the palette */
    b = bm_copy(b);
    bm_reduce_palette(b, palette);

    if(!make_palette_mapping(palette, mapping, &color_count))
        return 0;

    if(color_count > 128) {
        sgct = 256;
        gif.lsd.fields |= 0x07;
    } else if(color_count > 64) {
        sgct = 128;
        gif.lsd.fields |= 0x06;
        code_size = 7;
    } else if(color_count > 32) {
        sgct = 64;
        gif.lsd.fields |= 0x05;
        code_size = 6;
    } else if(color_count > 16) {
        sgct = 32;
        gif.lsd.fields |= 0x04;
        code_size = 5;
    } else if(color_count > 8) {
        sgct = 16;
        gif.lsd.fields |= 0x03;
        code_size = 4;
    } else {
        sgct = 8;
        gif.lsd.fields |= 0x02;
        code_size = 3;
    }

    /* See if we can find the background color in the palette */
#ifndef IGNORE_ALPHA
    bg = b->color & 0x00FFFFFF;
#else
    bg = b->color;
#endif
    bg = get_palette_mapping(mapping, bg, color_count);
    if(bg >= 0) {
        gif.lsd.background = bg;
    }

    /* Map the pixels in the image to their palette indices */
    pixels = CAST(unsigned char*)(malloc(b->w * b->h));
    for(y = 0, p = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int i, c = BM_GET(b, x, y);
            i = get_palette_mapping(mapping, c, color_count);
            /* At this point in time, the color MUST be in the palette */
            assert(i >= 0 && i < color_count);
            assert(i < sgct);
            pixels[p++] = i;
        }
    }
    assert(p == b->w * b->h);

    {
        /* Save the header and global color table */
        struct rgb_triplet gct[256];
        triplets_from_palette(palette, gct);

        assert(sgct <= 256);

        if(!writef(&gif.header, sizeof gif.header, context) ||
            !writef(&gif.lsd, sizeof gif.lsd, context) ||
            !writef(gct, sizeof gct[0] * sgct, context)) {
            SET_ERROR("couldn't write GIF header");
            return 0;
        }
    }

    /* Nothing of use here */
    gce.block_size = 4;
    gce.fields = 0;
    gce.delay = 0;
#if SAVE_GIF_TRANSPARENT
    if(bg >= 0) {
        gce.fields |= 0x01;
        gce.trans_index = bg;
    } else {
        gce.trans_index = 0;
    }
#else
    gce.trans_index = 0;
#endif
    gce.terminator = 0x00;

    put_byte(0x21, writef, context);
    put_byte(0xF9, writef, context);
    if(!writef(&gce, sizeof gce, context)) {
        return 0;
    }

    gif_id.separator = 0x2C;
    gif_id.left = 0x00;
    gif_id.top = 0x00;
    gif_id.width = b->w;
    gif_id.height = b->h;
    /* Not using local color table or interlacing */
    gif_id.fields = 0;
    if(!writef(&gif_id, sizeof gif_id, context)) {
        SET_ERROR("couldn't write GIF info");
        return 0;
    }

    put_byte(code_size, writef, context);

    /* Perform the LZW compression */
    bytes = lzw_encode_bytes(pixels, b->w * b->h, code_size, &len);
    free(pixels);

    /* Write out the data sub-blocks */
    for(p = 0; p < len; p++) {
        if(p % 0xFF == 0) {
            /* beginning of a new block; lzw_emit_code the length byte */
            if(len - p >= 0xFF) {
                put_byte(0xFF, writef, context);
            } else {
                put_byte(len - p, writef, context);
            }
        }
        put_byte(bytes[p], writef, context);
    }
    free(bytes);

    put_byte(0x00, writef, context); /* terminating block */

    put_byte(0x3B, writef, context); /* trailer byte */

    bm_free(b);

    return 1;
}

/* PCX support
http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt
http://www.shikadi.net/moddingwiki/PCX_Format
*/
#pragma pack(push, 1)
struct pcx_header {
    char manuf;
    char version;
    char encoding;
    char bpp;
    unsigned short xmin, ymin, xmax, ymax;
    unsigned short vert_dpi, hori_dpi;

    union {
        unsigned char bytes[48];
        struct rgb_triplet rgb[16];
    } palette;

    char reserved;
    char planes;
    unsigned short bytes_per_line;
    unsigned short paltype;
    unsigned short hscrsize, vscrsize;
    char pad[54];
};
#pragma pack(pop)

static Bitmap *bm_load_pcx_rd(BmReader rd) {
    struct pcx_header hdr;
    Bitmap *b = NULL;
    int y;
    BmPalette *pal = NULL;

    if(rd.fread(&hdr, sizeof hdr, 1, rd.data) != 1) {
        SET_ERROR("couldn't read PCX header");
        return NULL;
    }
    if(hdr.manuf != 0x0A) {
        SET_ERROR("bad PCX header");
        return NULL;
    }

    if(hdr.version != 5 || hdr.encoding != 1 || hdr.bpp != 8 || (hdr.planes != 1 && hdr.planes != 3)) {
        /* We might want to support these PCX types at a later stage... */
        SET_ERROR("unsupported PCX type");
        return NULL;
    }

    if(hdr.planes == 1) {
        long pos = rd.ftell(rd.data), i;
        char pbyte;
        struct rgb_triplet rgb[256];

        rd.fseek(rd.data, -769, SEEK_END);
        if(rd.fread(&pbyte, sizeof pbyte, 1, rd.data) != 1) {
            SET_ERROR("error reading PCX info");
            return NULL;
        }
        if(pbyte != 12) {
            SET_ERROR("bad PCX info");
            return NULL;
        }
        if(rd.fread(&rgb, sizeof rgb[0], 256, rd.data) != 256) {
            SET_ERROR("error reading PCX palette");
            return NULL;
        }
        pal = bm_palette_create(256);
        if(!pal)
            return NULL;
        for(i = 0; i < 256; i++)
            bm_palette_set(pal, i, (rgb[i].r << 16) | (rgb[i].g << 8) | rgb[i].b);

        rd.fseek(rd.data, pos, SEEK_SET);
    }

    b = bm_create(hdr.xmax - hdr.xmin + 1, hdr.ymax - hdr.ymin + 1);
    if(!b)
        return NULL;
    bm_set_palette(b, pal);
    bm_palette_release(pal);

    for(y = 0; y < b->h; y++) {
        int p;
        for(p = 0; p < hdr.planes; p++) {
            int x = 0;
            while(x < hdr.bytes_per_line) {
                int cnt = 1;
                unsigned char i;
                if(rd.fread(&i, sizeof i, 1, rd.data) != 1)
                    goto read_error;

                if((i & 0xC0) == 0xC0) {
                    cnt = i & 0x3F;
                    if(rd.fread(&i, sizeof i, 1, rd.data) != 1)
                        goto read_error;
                }
                if(hdr.planes == 1) {
                    unsigned int c = bm_palette_get(pal, i);
                    while(cnt--) {
                        if(x == b->w) goto next;
                        BM_SET(b, x++, y, c);
                    }
                } else {
                    while(cnt--) {
                        int c = BM_GET(b, x, y);
                        switch(p) {
                        case 0: c |= (i << 16); break;
                        case 1: c |= (i << 8); break;
                        case 2: c |= (i << 0); break;
                        }
                        bm_set(b, x++, y, c);
                    }
                }
            }
next:       continue;
        }
    }

    return b;
read_error:
    SET_ERROR("error reading PCX data");
    bm_free(b);
    return NULL;
}

static int bm_save_pcx(Bitmap *b, bm_write_fun writef, void *context) {
    int i, x, y, rv = 1;

    BmPalette *palette;

    struct palette_mapping mapping[256];
    int color_count;
    struct pcx_header hdr;

    if(!b)
        return 0;

    palette = bm_get_palette(b);
    if(!palette) {
        if(!bm_make_palette(b))
            return 0;
        palette = bm_get_palette(b);
        assert(palette);
    }
    if(bm_palette_count(palette) > 256) {
        SET_ERROR("too many palette colors to save PCX");
        return 0;
    }

    {
        /* Write the header */
        memset(&hdr, 0, sizeof hdr);

        hdr.manuf = 0x0A;
        hdr.version = 5;
        hdr.encoding = 1;
        hdr.bpp = 8;

        hdr.xmin = 0;
        hdr.ymin = 0;
        hdr.xmax = b->w - 1;
        hdr.ymax = b->h - 1;

        hdr.vert_dpi = b->h;
        hdr.hori_dpi = b->w;

        hdr.reserved = 0;
        hdr.planes = 1;
        hdr.bytes_per_line = b->w;
        if(hdr.bytes_per_line & 0x1) /* Must be an even number */
            hdr.bytes_per_line++;
        hdr.paltype = 1;
        hdr.hscrsize = 0;
        hdr.vscrsize = 0;

        if(!writef(&hdr, sizeof hdr, context)) {
            SET_ERROR("error writing PCX header");
            return 0;
        }
    }

    /* Copy the image and dither it to match the palette */
    b = bm_copy(b);
    bm_reduce_palette(b, palette);
    bm_set_palette(b, palette);

    if(!make_palette_mapping(palette, mapping, &color_count))
        return 0;

    for(y = 0; y < b->h; y++) {
        x = 0;
        while(x < b->w) {
            int cnt = 1;
            unsigned int c = BM_GET(b, x++, y);
            while(x < b->w && cnt < 63) {
                unsigned int n = BM_GET(b, x, y);
                if(c != n)
                    break;
                x++;
                cnt++;
            }

            i = get_palette_mapping(mapping, c, color_count);
            /* At this point in time, the color MUST be in the palette */
            assert(i >= 0 && i < color_count);

            /*
            i = bm_palette_nearest_index(palette, c);
            assert(i >=0 && i < palette->ncolors);
            */

            if(cnt == 1 && i < 192) {
                put_byte(i, writef, context);
            } else {
                put_byte(0xC0 | cnt, writef, context);
                put_byte(i, writef, context);
            }
        }
        while(x < hdr.bytes_per_line) {
            put_byte(0x00, writef, context);
            x++;
        }
    }

    put_byte(12, writef, context);

    {
        /* Save the palette */
        struct rgb_triplet rgb[256];
        triplets_from_palette(palette, rgb);
        if(!writef(rgb, sizeof rgb[0] * 256, context)) {
            SET_ERROR("error writing PCX palette");
            rv = 0;
        }
    }

    bm_free(b); /* Delete the copy */

    return rv;
}

/*
Targa (.TGA) support
https://en.wikipedia.org/wiki/Truevision_TGA
http://paulbourke.net/dataformats/tga/
http://www.ludorg.net/amnesia/TGA_File_Format_Spec.html
*/

#pragma pack(push, 1)
struct tga_header {
    uint8_t     id_length;
    uint8_t     map_type;
    uint8_t     img_type;
    struct {
        uint16_t    index;
        uint16_t    length;
        uint8_t     size;
    } map_spec;
    struct {
        uint16_t    xo, yo;
        uint16_t    w, h;
        uint8_t     bpp;
        uint8_t     img_desc;
    } img_spec;
};
#pragma pack(pop)

static int is_tga_file(BmReader rd) {
    /* TGA does not have a magic number in the header, so we have to take
       an educated guess as to whether it looks like one from its header */
    struct tga_header head;
    long start = rd.ftell(rd.data), rv = 1;

    static const uint8_t tga_types[] = {0,1,2,3,9,10,11, /* 32,33 */};
    /* 32 and 33 seem complex and not widely supported, so I'm not going
    to bother with them. */
    int n, m = sizeof(tga_types)/ sizeof(tga_types[0]);

    if(rd.fread(&head, sizeof head, 1, rd.data) != 1) {
        rv = 0;
        goto done;
    }

    if(head.map_type != 0 && head.map_type != 1) {
        rv = 0;
        goto done;
    }
    for(n = 0; n < m; n++) {
        if(head.img_type == tga_types[n])
            break;
    }
    if(n == m) {
        rv = 0;
    } else if(head.map_type) {
        if(head.map_spec.size != 8
            && head.map_spec.size != 15
            && head.map_spec.size != 16
            && head.map_spec.size != 24
            && head.map_spec.size != 32) {
            rv = 0;
        }
    } else if( head.img_spec.bpp != 8
            && head.img_spec.bpp != 15
            && head.img_spec.bpp != 16
            && head.img_spec.bpp != 24
            && head.img_spec.bpp != 32) {
        rv = 0;
    }
done:
    rd.fseek(rd.data, start, SEEK_SET);
    return rv;
}

static int tga_decode_pixel(Bitmap *bmp, int x, int y, uint8_t bytes[], struct tga_header *head, uint8_t *color_map) {
    int bpp = head->img_spec.bpp;
    switch(head->img_type & 0x07) {
        case 1: {
            assert(head->img_spec.bpp == 8);
            if(head->img_spec.bpp != 8) return 0;
            if(!color_map) return 0;
            int index = bytes[0];
            bpp = head->map_spec.size;
            bytes = &color_map[index * bpp / 8 - head->map_spec.index];
        }
        /* fall through */
        case 2: {
            switch(bpp) {
            case 15:
            case 16: {
                uint16_t c16 = (bytes[1] << 8) | bytes[0];
                uint8_t b = (c16 & 0x1F) << 3;
                uint8_t g = ((c16 >> 5) & 0x1F) << 3;
                uint8_t r = ((c16 >> 10) & 0x1F) << 3;
                bm_set(bmp, x, y, bm_rgb(r, g, b));
            } break;
            case 24: bm_set(bmp, x, y, bm_rgb(bytes[2], bytes[1], bytes[0])); break;
            case 32: bm_set(bmp, x, y, bm_rgba(bytes[2], bytes[1], bytes[0], bytes[3])); break;
            }
        } break;
        case 3: {
            assert(head->img_spec.bpp == 8);
            if(head->img_spec.bpp != 8) return 0;
            bm_set(bmp, x, y, bm_rgb(bytes[0], bytes[0], bytes[0]));
        } break;
        default: return 0; break;
    }
    return 1;
}

static Bitmap *bm_load_tga_rd(BmReader rd) {
    struct tga_header head;
    int i = 0, j, np;
    assert(sizeof(struct tga_header) == 18);

    /* Just try to catch cases where is_tga_file() might fail... */
    assert(is_tga_file(rd));

    if(rd.fread(&head, sizeof head, 1, rd.data) != 1) {
        SET_ERROR("error reading TGA header");
        return NULL;
    }

    if(head.img_type == 0)
        return bm_create(head.img_spec.w, head.img_spec.h);

    if(head.id_length > 0) {
        /* skip the ID field */
        rd.fseek(rd.data, head.id_length, SEEK_CUR);
    }

    uint8_t bytes[4];
    uint8_t *color_map = NULL;
    Bitmap *bmp = bm_create(head.img_spec.w, head.img_spec.h);
    if(!bmp)
        return NULL;

    if(head.map_type) {
        color_map = CAST(uint8_t*)(calloc(head.map_spec.length, head.map_spec.size));
        int r = rd.fread(color_map, head.map_spec.size / 8, head.map_spec.length, rd.data);
        if(r != head.map_spec.length) {
            SET_ERROR("error reading TGA color map");
            goto error;
        }
    }

    np = head.img_spec.w * head.img_spec.h;

    while(i < np) {

        uint8_t nreps, rle = 0;
        if(head.img_type & 0x08) {
            if(rd.fread(&rle, 1, 1, rd.data) != 1) {
                goto error;
            }
            nreps = (rle & 0x7F) + 1;
        } else {
            nreps = 0xFF;
            if(i + nreps >= np)
                nreps = np - i;
        }

        for (j = 0; j < nreps; j++) {
            int x, y;
            y = i / head.img_spec.w;
            x = i % head.img_spec.w;

            /* Bit 5 of img_desc determines the image origin (lower left or upper left): */
            if(!(head.img_spec.img_desc & 0x20))
                y = head.img_spec.h - 1 - y;

            assert(x < bmp->w);
            assert(y < bmp->h);
            if(!(rle & 0x80) || ((rle & 0x80) && !j)) {
                if(rd.fread(bytes, head.img_spec.bpp / 8, 1, rd.data) != 1) {
                    SET_ERROR("error reading TGA data");
                    goto error;
                }
            }
            if(!tga_decode_pixel(bmp, x, y, bytes, &head, color_map)) {
                SET_ERROR("error decoding TGA data");
                goto error;
            }
            i++;
        }
    }

    if(color_map) free(color_map);
    return bmp;
error:
    if(color_map) free(color_map);
    if(bmp) bm_free(bmp);
    return NULL;
}

static int bm_save_tga(Bitmap *b, bm_write_fun writef, void *context) {
    /* Always saves as 24-bit TGA */
    struct tga_header head;

    memset(&head, 0, sizeof head);

    head.img_type = (TGA_SAVE_RLE) ? 10 : 2;
    head.img_spec.w = b->w;
    head.img_spec.h = b->h;
    head.img_spec.bpp = 24;

    if(!writef(&head, sizeof head, context)) {
        SET_ERROR("error opening file for TGA output");
        return 0;
    }

    int i = 0;
    while(i < b->w * b->h) {
        int x, y;
        uint8_t bytes[1 + 3*128];
        y = i / b->w;
        y = b->h - 1 - y;
        x = i % b->w;
        unsigned int c = BM_GET(b, x, y);
#if TGA_SAVE_RLE
        uint8_t n = 1;
        size_t nb = 4;
        bm_get_rgb(c, &bytes[3], &bytes[2], &bytes[1]);
        if(x < b->w - 1 && BM_GET(b, x + 1, y) == c) {
            while(n < 128 && x + n < b->w && BM_GET(b, x + n, y) == c)
                n++;
            bytes[0] = 0x80 | (n - 1);
        } else {
            while(n < 128 && x + n < b->w) {
                c = BM_GET(b, x + n, y);
                if(x + n + 1 < b->w && BM_GET(b, x + n + 1, y) == c)
                    break;
                bm_get_rgb(c, &bytes[nb + 2], &bytes[nb + 1], &bytes[nb + 0]);
                nb += 3;
                n++;
            }
            bytes[0] = (n - 1);
        }
        assert(n <= 128);
        assert(nb <= sizeof bytes);
        if(!writef(&bytes, nb, context)) {
            SET_ERROR("error writing TGA data");
            return 0;
        }
        i += n;
#else
        bm_get_rgb(c, &bytes[2], &bytes[1], &bytes[0]);
        if(!writef(&bytes, 3, context)) {
            SET_ERROR("error writing TGA palette");
            return 0;
        }
        i++;
#endif
    }

    return 1;
}

/*
Netpbm support
https://en.wikipedia.org/wiki/Netpbm

Specs and example files can be found here:
http://paulbourke.net/dataformats/ppm/
https://people.sc.fsu.edu/~jburkardt/data/pbmb/pbmb.html
https://people.sc.fsu.edu/~jburkardt/data/pgmb/pgmb.html
https://people.sc.fsu.edu/~jburkardt/data/ppmb/ppmb.html
*/

static char *tokenize_pbm(char *s, char **r) {
    char *p = s, *e;

    if(!s && r)
        p = *r;

    if(!p) return NULL;

start:
    while(isspace(*p))
        p++;
    if(*p == '\0')
        return NULL;
    if(*p == '#') {
        while(*p != '\n') {
            if(*p == '\0')
                return NULL;
            p++;
        }
        goto start;
    }

    for(e = p; *e && !isspace(*e); e++);

    if(r) {
        if(*e != '\0')
            *r = e + 1;
        else
            *r = e;
    }

    *e = '\0';

    return p;
}

static Bitmap *bm_load_ppm_rd(BmReader rd) {

    Bitmap *bm = NULL;

    rd.fseek(rd.data, 0, SEEK_END);
    long len = rd.ftell(rd.data);
    rd.fseek(rd.data, 0, SEEK_SET);

    char *str = malloc(len+2);
    if(!str) {
        SET_ERROR("ppm: out of memory");
        return NULL;
    }
    long read = rd.fread(str, 1, len, rd.data);

    if(read != len) {
        SET_ERROR("ppm: error reading data");
        free(str);
        return NULL;
    }
    str[len] = '\0';

    char *p, *r;
    int type = 0, w, h, d = 1, x, y;

    p = tokenize_pbm(str, &r);
    if(!p) {
        SET_ERROR("ppm: couldn't determine type");
        goto error;
    }

    if(p[0] != 'P' || !strchr("123456", p[1]) || p[2]) {
        SET_ERROR("ppm: invalid type");
        goto error;
    }

    type = p[1] - '0';

#define GET_INT(v, error_msg)   do{ if(!(p = tokenize_pbm(NULL, &r))) { \
                                    SET_ERROR(error_msg); \
                                    goto error;\
                                }\
                                v = atoi(p); } while(0)

    GET_INT(w, "ppm: bad width");
    GET_INT(h, "ppm: bad height");

    if(type != 1 && type != 4)
        GET_INT(d, "ppm: bad depth");

    if(w <= 0 || h <= 0 || d <= 0) {
        SET_ERROR("ppm: invalid dimensions");
        goto error;
    }

    bm = bm_create(w,h);

    int pr = 0, pg = 0, pb = 0, c;

    switch(type) {
        case 1: {
            x = 0;
            y = 0;
            while(y < h) {
                if(r - str >= len) {
                    SET_ERROR("ppm: unexpected end of file");
                    goto error;
                }
                while(isspace(*r))
                    r++;
                if(!r[0]) {
                    SET_ERROR("ppm: insufficient data");
                    goto error;
                }
                else if(r[0] == '#') {
                    while(r[0] && r[0] != '\n')
                        r++;
                    continue;
                }
                else if(r[0] == '0')
                    bm_set(bm, x, y, 0xFFFFFFFF);
                else if(r[0] == '1')
                    bm_set(bm, x, y, 0xFF000000);
                else {
                    SET_ERROR("ppm: bad data");
                    goto error;
                }
                r++;
                if(++x == w) {
                    x = 0;
                    y++;
                }
            }
        } break;
        case 2: {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    GET_INT(pr, "ppm: bad value");
                    pr = pr * 255 / d;
                    c = 0xFF000000 | (pr << 16) | (pr << 8) | pr;
                    bm_set(bm, x, y, c);
                }
            }
        } break;
        case 3: {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    GET_INT(pr, "ppm: bad R value");
                    pr = pr * 255 / d;
                    GET_INT(pg, "ppm: bad G value");
                    pg = pg * 255 / d;
                    GET_INT(pb, "ppm: bad B value");
                    pb = pb * 255 / d;
                    c = 0xFF000000 | (pr << 16) | (pg << 8) | pb;
                    bm_set(bm, x, y, c);
                }
            }
        } break;
        case 4: {
            for(y = 0; y < h; y++) {
                int mask = 0x80;
                unsigned char byte = *(r++);
                for(x = 0; x < w; x++) {
                    bm_set(bm, x, y, (byte & mask) ? 0xFF000000 : 0xFFFFFFFF);
                    if(!(mask >>= 1)) {
                        if(r - str >= len) {
                            SET_ERROR("ppm: unexpected end of file");
                            goto error;
                        }
                        mask = 0x80;
                        byte = *(r++);
                    }
                }
            }
        } break;
        case 5: {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(r - str >= len) {
                        SET_ERROR("ppm: unexpected end of file");
                        goto error;
                    }
                    pr = (*(r++) * 255 / d) & 0xFF;
                    c = 0xFF000000 | (pr << 16) | (pr << 8) | pr;
                    bm_set(bm, x, y, c);
                }
            }
        } break;
        case 6: {
            for(y = 0; y < h; y++) {
                for(x = 0; x < w; x++) {
                    if(r - str > len - 3) {
                        SET_ERROR("ppm: unexpected end of file");
                        goto error;
                    }
                    pr = (*(r++) * 255 / d) & 0xFF;
                    pg = (*(r++) * 255 / d) & 0xFF;
                    pb = (*(r++) * 255 / d) & 0xFF;
                    c = 0xFF000000 | (pr << 16) | (pg << 8) | pb;
                    bm_set(bm, x, y, c);
                }
            }
        } break;
        default:
            SET_ERROR("ppm: format not supported");
            goto error;
    }

    goto done;
error:
    if(bm) bm_free(bm);
    bm = NULL;
done:
    free(str);
    return bm;

#undef GET_INT

}

static int bm_save_ppm(Bitmap *b, bm_write_fun writef, void *context, const char *ext) {

    int type = 3;
    if(!ext || strlen(ext) != 3) {
        SET_ERROR("ppm: bad extension");
        return 0;
    }

    switch(ext[1]) {
#if !PPM_BINARY
        case 'b': case 'B': type = 1; break;
        case 'g': case 'G': type = 2; break;
        case 'p': case 'P': type = 3; break;
#else
        case 'b': case 'B': type = 4; break;
        case 'g': case 'G': type = 5; break;
        case 'p': case 'P': type = 6; break;
#endif
    }

    put_text(writef, context, "P%d\n", type);
    put_text(writef, context, "%d %d\n", b->w, b->h);
    if(type != 1 && type != 4)
        put_text(writef, context, "255\n");

    int x, y;
    unsigned char p[3] = {0, 0, 0};

    switch(type) {
        case 1:
            for(y = 0; y < b->h; y++) {
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    put_text(writef, context, "%c", c & 0xFFFFFF ? '0' : '1');
                }
                put_byte('\n', writef, context);
            }
            break;
        case 2:
            for(y = 0; y < b->h; y++) {
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    put_text(writef, context, "%u ", bm_graypixel(c));
                }
                put_byte('\n', writef, context);
            }
            break;
        case 3:
            for(y = 0; y < b->h; y++) {
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    bm_get_rgb(c, &p[0], &p[1], &p[2]);
                    put_text(writef, context, "%u %u %u ", p[0], p[1], p[2]);
                }
                put_byte('\n', writef, context);
            }
            break;
        case 4: {
            for(y = 0; y < b->h; y++) {
                int mask = 0x80;
                unsigned char byte = 0;
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    if(!(c & 0xFFFFFF))
                        byte |= mask;
                    if(!(mask >>= 1)) {
                        writef(&byte, 1, context);
                        byte = 0;
                        mask = 0x80;
                    }
                }
                if(mask)
                    writef(&byte, 1, context);
            }
        } break;
        case 5: {
            for(y = 0; y < b->h; y++) {
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    unsigned char g = (unsigned char)bm_graypixel(c);
                    writef(&g, 1, context);
                }
            }
        } break;
        case 6:
            for(y = 0; y < b->h; y++) {
                for(x = 0; x < b->w; x++) {
                    unsigned int c = BM_GET(b, x, y);
                    bm_get_rgb(c, &p[0], &p[1], &p[2]);
                    writef(&p, 3, context);
                }
            }
            break;
        default: break;
    }
    if(type <= 3)
        put_byte('\n', writef, context);

    if(type == 4 && p[1])
        writef(&p, 1, context);

    return 1;
}

Bitmap *bm_from_stb(int w, int h, unsigned char *data) {

    Bitmap *b = bm_create_internal(w, h);
    b->data = data;
    b->flags |= FLAG_OWNS_DATA;

    swap_stb_bytes(w, h, data);

    return b;
}

#if defined(USESTB)
Bitmap *bm_load_stb(const char *filename) {
    int w, h, n;
    unsigned char *data = stbi_load(filename, &w, &h, &n, 4);
    if(!data)
        return NULL;
    return bm_from_stb(w, h, data);
}
#endif /* USESTB */

Bitmap *bm_copy(Bitmap *b) {
    Bitmap *out = bm_create(b->w, b->h);
    if(!out)
        return NULL;
    memcpy(out->data, b->data, BM_BLOB_SIZE(b));

    out->color = b->color;
    bm_set_font(out, b->font);

    bm_set_palette(out, b->palette);

    memcpy(&out->clip, &b->clip, sizeof b->clip);

    return out;
}

Bitmap *bm_crop(Bitmap *b, int x, int y, int w, int h) {
    Bitmap *o = bm_create(w, h);
    if(!o)
        return NULL;
    bm_blit(o, 0, 0, b, x, y, w, h);

    o->color = b->color;
    bm_set_font(o, b->font);
    bm_set_palette(o, b->palette);

    return o;
}

void bm_free(Bitmap *b) {
    if(!b)
        return;

    assert(b->ref_count == 0 && "Attempt to free() a bitmap that still has references");
    if((b->flags & FLAG_OWNS_DATA) && b->data)
        free(b->data);
    bm_font_release(b->font);

    if(b->palette)
        bm_palette_release(b->palette);

    free(b);
}

Bitmap *bm_retain(Bitmap *b) {
    if(!b)
        return NULL;
    b->ref_count++;
    return b;
}

void bm_release(Bitmap *b) {
    assert(b->ref_count > 0 && "Attempt to release a bitmap that is not reference counted");
    b->ref_count--;
    if(!b->ref_count)
        bm_free(b);
}

Bitmap *bm_bind(int w, int h, unsigned char *data) {
    Bitmap *b = bm_create_internal(w, h);
    if(!b)
        return NULL;
    b->data = data;
    return b;
}

void bm_rebind(Bitmap *b, unsigned char *data) {
    b->data = data;
}

void bm_unbind(Bitmap *b) {
    assert(!(b->flags & FLAG_OWNS_DATA) && "Attempt to unbind a bitmap that is not bound");
    bm_free(b);
}

#ifdef USESDL
SDL_Texture *bm_create_SDL_texture(Bitmap *b, SDL_Renderer *renderer) {
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, b->w, b->h);
    if(!texture) {
        SET_ERROR(SDL_GetError());
        return NULL;
    }
    SDL_UpdateTexture(texture, NULL, b->data, b->w * 4);
    return texture;
}
#endif

void bm_flip_vertical(Bitmap *b) {
    int y;
    size_t s = BM_ROW_SIZE(b);
    unsigned char *trow = CAST(unsigned char*)(ALLOCA(s));
    if (!trow)
        return;
    for(y = 0; y < b->h/2; y++) {
        unsigned char *row1 = &b->data[y * s];
        unsigned char *row2 = &b->data[(b->h - y - 1) * s];
        memcpy(trow, row1, s);
        memcpy(row1, row2, s);
        memcpy(row2, trow, s);
    }
    FREEA(trow);
}

unsigned int bm_get(Bitmap *b, int x, int y) {
    unsigned int *p;
    assert(b);
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    p = (unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP);
    return *p;
}

void bm_set(Bitmap *b, int x, int y, unsigned int c) {
    unsigned int *p;
    assert(b);
    assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
    p = (unsigned int*)(b->data + y * BM_ROW_SIZE(b) + x * BM_BPP);
    *p = c;
}

void bm_set_color(Bitmap *bm, unsigned int col) {
    bm->color = col;
}

unsigned int bm_get_color(Bitmap *b) {
    assert(b);
    return b->color;
}

Bitmap *bm_from_Xbm(int w, int h, unsigned char *data) {
    int x,y;

    Bitmap *bmp = bm_create(w, h);
    if(!bmp)
        return NULL;

    int byte = 0;
    for(y = 0; y < h; y++)
        for(x = 0; x < w;) {
            int i, b;
            b = data[byte++];
            for(i = 0; i < 8 && x < w; i++) {
                unsigned char c = (b & (1 << i))?0x00:0xFF;
                BM_SET_RGBA(bmp, x++, y, c, c, c, c);
            }
        }
    return bmp;
}

/*
See also https://www.fileformat.info/format/xpm/egff.htm
*/
Bitmap *bm_from_Xpm(const char *xpm[]) {
#define XPM_MAX_COLORS 256
    Bitmap *b;
    int w, h, nc, cp;
    int i, j, r;
    unsigned int colors[XPM_MAX_COLORS];
    unsigned char chars[XPM_MAX_COLORS];

    unsigned int tc = 0; /* transparent color */
    int tci = XPM_MAX_COLORS;

    /* FYI: It is possible for the first row to have 6 values, where
    the last two will be the hot-spot */

#ifdef SAFE_C11
    r = sscanf_s(xpm[0], "%d %d %d %d", &w, &h, &nc, &cp);
#else
    r = sscanf(xpm[0], "%d %d %d %d", &w, &h, &nc, &cp);
#endif
    assert(r == 4);(void)r;
    assert(w > 0 && h > 0);
    assert(nc > 0 && nc < XPM_MAX_COLORS);
    assert(cp == 1); /* cp != 1 not supported */

    b = bm_create(w, h);
    if(!b)
        return NULL;

#ifdef SAFE_C11
    memset(colors, 0, sizeof colors);
#endif

    for(i = 0; i < nc; i++) {
        char col[20];
        char k;
        col[sizeof col - 1] = 0;
        chars[i] = xpm[i+1][0]; /* to allow spaces */
#ifdef SAFE_C11
        r = sscanf_s(xpm[i+1] + 1, " %c %s", &k, 1, col, sizeof col);
#else
        r = sscanf(xpm[i+1] + 1, " %c %s", &k, col);
#endif
        assert(r == 2);(void)r;
        assert(k == 'c'); /* other keys not supported */
        assert(col[sizeof col - 1] == 0);

        /* TODO: If col starts with % it is a HSV value */

        if(!bm_stricmp(col,"none"))
            tci = i;
        else
            colors[i] = bm_atoi(col);
    }

    /* Set `tc` to a color that is not in the palette. */
    int found = 0;
    do {
        tc++;
        found = 0;
        for(j = 0; j < nc; j++)
            if(colors[j] == tc) {
                found = 1;
                break;
            }
    } while(found);

    if(tci < XPM_MAX_COLORS)
        colors[tci] = tc;

    /* Get the actual pixel data */
    for(j = 0; j < h; j++) {
        const char *row = xpm[1 + nc + j];
        for(i = 0; i < w; i++) {
            assert(row[i]);
            for(r = 0; r < nc; r++) {
                if(chars[r] == row[i]) {
                    bm_set_color(b, colors[r]);
                    break;
                }
            }
            bm_putpixel(b, i, j);
        }
    }

    /* Now set the color of the bitmap to `tc` to preserve its transparency */
    bm_set_color(b, tc);

    return b;
}

void bm_clip(Bitmap *b, int x0, int y0, int x1, int y1) {
    assert(b);
    if(x0 > x1)
        SWAPINT(x0, x1);
    if(y0 > y1)
        SWAPINT(y0, y1);
    if(x0 < 0) x0 = 0;
    if(x1 > b->w) x1 = b->w;
    if(y0 < 0) y0 = 0;
    if(y1 > b->h) y1 = b->h;

    b->clip.x0 = x0;
    b->clip.y0 = y0;
    b->clip.x1 = x1;
    b->clip.y1 = y1;
}

void bm_unclip(Bitmap *b) {
    assert(b);
    b->clip.x0 = 0;
    b->clip.y0 = 0;
    b->clip.x1 = b->w;
    b->clip.y1 = b->h;
}


BmRect bm_get_clip(Bitmap *b) {
    assert(b);
    return b->clip;
}

void bm_set_clip(Bitmap *b, const BmRect rect) {
    assert(b);
    b->clip = rect;
}

int bm_inclip(Bitmap *b, int x, int y) {
    assert(b);
    assert(b->clip.x1 > b->clip.x0 && b->clip.x0 >= 0 && b->clip.x1 < b->w);
    assert(b->clip.y1 > b->clip.y0 && b->clip.y0 >= 0 && b->clip.y1 < b->h);
    return x >= b->clip.x0 && y >= b->clip.y0 &&
        x < b->clip.x1 && y < b->clip.y1;
}

void bm_blit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h) {
    int x,y, i, j;

    if(sx < 0) {
        int delta = -sx;
        sx = 0;
        dx += delta;
        w -= delta;
    }

    if(dx < dst->clip.x0) {
        int delta = dst->clip.x0 - dx;
        sx += delta;
        w -= delta;
        dx = dst->clip.x0;
    }

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(dx + w > dst->clip.x1) {
        int delta = dx + w - dst->clip.x1;
        w -= delta;
    }

    if(sy < 0) {
        int delta = -sy;
        sy = 0;
        dy += delta;
        h -= delta;
    }

    if(dy < dst->clip.y0) {
        int delta = dst->clip.y0 - dy;
        sy += delta;
        h -= delta;
        dy = dst->clip.y0;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    if(dy + h > dst->clip.y1) {
        int delta = dy + h - dst->clip.y1;
        h -= delta;
    }

    if(w <= 0 || h <= 0)
        return;
    if(dx >= dst->clip.x1 || dx + w < dst->clip.x0)
        return;
    if(dy >= dst->clip.y1 || dy + h < dst->clip.y0)
        return;
    if(sx >= src->w || sx + w < 0)
        return;
    if(sy >= src->h || sy + h < 0)
        return;

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    assert(dst->clip.x1 <= dst->w);
    assert(dst->clip.y1 <= dst->h);
    assert(dx >= 0 && dx + w <= dst->clip.x1);
    assert(dy >= 0 && dy + h <= dst->clip.y1);
    assert(sx >= 0 && sx + w <= src->w);
    assert(sy >= 0 && sy + h <= src->h);

    j = sy;
    for(y = dy; y < dy + h; y++) {
        i = sx;
        for(x = dx; x < dx + w; x++) {
            unsigned int c = BM_GET(src, i, j);
            BM_SET(dst, x, y, c);
            i++;
        }
        j++;
    }
}

void bm_maskedblit(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, int w, int h) {
    int x,y, i, j;

    if(sx < 0) {
        int delta = -sx;
        sx = 0;
        dx += delta;
        w -= delta;
    }

    if(dx < dst->clip.x0) {
        int delta = dst->clip.x0 - dx;
        sx += delta;
        w -= delta;
        dx = dst->clip.x0;
    }

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(dx + w > dst->clip.x1) {
        int delta = dx + w - dst->clip.x1;
        w -= delta;
    }

    if(sy < 0) {
        int delta = -sy;
        sy = 0;
        dy += delta;
        h -= delta;
    }

    if(dy < dst->clip.y0) {
        int delta = dst->clip.y0 - dy;
        sy += delta;
        h -= delta;
        dy = dst->clip.y0;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    if(dy + h > dst->clip.y1) {
        int delta = dy + h - dst->clip.y1;
        h -= delta;
    }

    if(w <= 0 || h <= 0)
        return;
    if(dx >= dst->clip.x1 || dx + w < dst->clip.x0)
        return;
    if(dy >= dst->clip.y1 || dy + h < dst->clip.y0)
        return;
    if(sx >= src->w || sx + w < 0)
        return;
    if(sy >= src->h || sy + h < 0)
        return;

    if(sx + w > src->w) {
        int delta = sx + w - src->w;
        w -= delta;
    }

    if(sy + h > src->h) {
        int delta = sy + h - src->h;
        h -= delta;
    }

    assert(dst->clip.x1 <= dst->w);
    assert(dst->clip.y1 <= dst->h);
    assert(dx >= 0 && dx + w <= dst->clip.x1);
    assert(dy >= 0 && dy + h <= dst->clip.y1);
    assert(sx >= 0 && sx + w <= src->w);
    assert(sy >= 0 && sy + h <= src->h);

    j = sy;
    for(y = dy; y < dy + h; y++) {
        i = sx;
        for(x = dx; x < dx + w; x++) {
#if IGNORE_ALPHA
            unsigned int c = BM_GET(src, i, j) & 0x00FFFFFF;
            if(c != (src->color & 0x00FFFFFF))
                BM_SET(dst, x, y, c);
#else
            int c = BM_GET(src, i, j);
            if(c != src->color)
                BM_SET(dst, x, y, c);
#endif
            i++;
        }
        j++;
    }
}

void bm_blit_ex(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, int mask) {
    int x, y, ssx;
    int ynum = 0;
    int xnum = 0;
#if IGNORE_ALPHA
    unsigned int maskc = bm_get_color(src) & 0x00FFFFFF;
#else
    unsigned int maskc = bm_get_color(src);
#endif
    /*
    Uses Bresenham's algoritm to implement a simple scaling while blitting.
    See the article "Scaling Bitmaps with Bresenham" by Tim Kientzle in the
    October 1995 issue of C/C++ Users Journal

    Or see these links:
        http://www.drdobbs.com/image-scaling-with-bresenham/184405045
        http://www.compuphase.com/graphic/scale.htm
    */

    if(sw == dw && sh == dh) {
        /* Special cases, no scaling */
        if(mask) {
            bm_maskedblit(dst, dx, dy, src, sx, sy, dw, dh);
        } else {
            bm_blit(dst, dx, dy, src, sx, sy, dw, dh);
        }
        return;
    }

    if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;

    /* Clip on the Y */
    for(y = dy; y < dst->clip.y0 || sy < 0; y++) {
        for(ynum += sh; ynum > dh; sy++)
            ynum -= dh;
    }

    if(dy >= dst->clip.y1 || dy + dh < dst->clip.y0)
        return;

    /* Clip on the X */
    for(x = dx; x < dst->clip.x0 || sx < 0; x++, dw--) {
        for(xnum += sw; xnum > dw; sx++, sw--)
            xnum -= dw;
    }
    dx = x;

    if(dx >= dst->clip.x1 || dx + dw < dst->clip.x0)
        return;

    ssx = sx; /* Save sx for the next row */
    for(; y < dy + dh; y++){
        if(sy >= src->h || y >= dst->clip.y1)
            break;
        xnum = 0;
        sx = ssx;

        assert(y >= dst->clip.y0 && sy >= 0);
        for(x = dx; x < dx + dw; x++) {
            unsigned int c;
            if(sx >= src->w || x >= dst->clip.x1)
                break;
            assert(x >= dst->clip.x0 && sx >= 0);
#if IGNORE_ALPHA
            c = BM_GET(src, sx, sy) & 0x00FFFFFF;
#else
            c = BM_GET(src, sx, sy);
#endif
            if(!mask || c != maskc)
                BM_SET(dst, x, y, c);

            xnum += sw;
            while(xnum > dw) {
                xnum -= dw;
                sx++;
            }
        }
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
    }
}

/*
Works the same as bm_blit_ex(), but calls the callback for each pixel
typedef unsigned int (*bm_sampler_function)(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color);
*/
void bm_blit_callback(Bitmap *dst, int dx, int dy, int dw, int dh, Bitmap *src, int sx, int sy, int sw, int sh, bm_sampler_function sampler) {
    int x, y, ssx;
    int ynum = 0;
    int xnum = 0;
    BmRect save_clip;

    assert(src && dst);
    if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return;

    save_clip = bm_get_clip(src);
    bm_clip(src, sx, sy, sx + sw, sy + sh);

    /* Clip on the Y */
    for(y = dy; y < dst->clip.y0 || sy < 0; y++) {
        for(ynum += sh; ynum > dh; sy++)
            ynum -= dh;
    }

    if(dy >= dst->clip.y1 || dy + dh < dst->clip.y0)
        return;

    /* Clip on the X */
    for(x = dx; x < dst->clip.x0 || sx < 0; x++, dw--) {
        for(xnum += sw; xnum > dw; sx++, sw--)
            xnum -= dw;
    }
    dx = x;

    if(dx >= dst->clip.x1 || dx + dw < dst->clip.x0)
        return;

    ssx = sx; /* Save sx for the next row */
    for(; y < dy + dh; y++){
        if(sy >= src->h || y >= dst->clip.y1)
            break;
        xnum = 0;
        sx = ssx;

        assert(y >= dst->clip.y0 && sy >= 0);
        for(x = dx; x < dx + dw; x++) {
            unsigned int c;
            if(sx >= src->w || x >= dst->clip.x1)
                break;
            assert(x >= dst->clip.x0 && sx >= 0);

            c = BM_GET(dst, x, y);
            c = sampler(dst, x, y, src, sx, sy, c);
            BM_SET(dst, x, y, c);

            xnum += sw;
            while(xnum > dw) {
                xnum -= dw;
                sx++;
            }
        }
        ynum += sh;
        while(ynum > dh) {
            ynum -= dh;
            sy++;
        }
    }

    bm_set_clip(src, save_clip);
}

unsigned int bm_smp_outline(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color) {
    (void)dx;(void)dy;
    if(bm_colcmp(src->color, BM_GET(src, sx, sy))) {
        if(sx > src->clip.x0) {
            if(!bm_colcmp(src->color, BM_GET(src, sx-1, sy)))
                return dst->color;
        }
        if(sx < src->clip.x1-1) {
            if(!bm_colcmp(src->color, BM_GET(src, sx+1, sy)))
                return dst->color;
        }
        if(sy > src->clip.y0) {
            if(!bm_colcmp(src->color, BM_GET(src, sx, sy-1)))
                return dst->color;
        }
        if(sy < src->clip.y1-1) {
            if(!bm_colcmp(src->color, BM_GET(src, sx, sy+1)))
                return dst->color;
        }
    } else {
        if(sx == src->clip.x0 || sx == src->clip.x1 - 1)
            return dst->color;
        if(sy == src->clip.y0 || sy == src->clip.y1 - 1)
            return dst->color;
    }
    return dest_color;
}

unsigned int bm_smp_border(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color) {
    (void)dx;(void)dy;
    if(!bm_colcmp(src->color, BM_GET(src, sx, sy))) {

        if(sx > src->clip.x0) {
            if(bm_colcmp(src->color, BM_GET(src, sx-1, sy)))
                return dst->color;
        } else
            return dst->color;

        if(sx < src->clip.x1-1) {
            if(bm_colcmp(src->color, BM_GET(src, sx+1, sy)))
                return dst->color;
        } else
            return dst->color;

        if(sy > src->clip.y0) {
            if(bm_colcmp(src->color, BM_GET(src, sx, sy-1)))
                return dst->color;
        } else
            return dst->color;

        if(sy < src->clip.y1-1) {
            if(bm_colcmp(src->color, BM_GET(src, sx, sy+1)))
                return dst->color;
        } else
            return dst->color;
    }

    return dest_color;
}

unsigned int bm_smp_binary(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color) {
    (void)dx;(void)dy;
    if(!bm_colcmp(src->color, BM_GET(src, sx, sy))) {
        return dst->color;
    }
    return dest_color;
}

unsigned int bm_smp_blend50(Bitmap *dst, int dx, int dy, Bitmap *src, int sx, int sy, unsigned int dest_color) {
    (void)dst;(void)dx;(void)dy;
    unsigned int c = BM_GET(src, sx, sy);
    if(bm_colcmp(src->color, c))
        return dest_color;

    dest_color = (dest_color >> 1) & 0x7F7F7F;
    c = (c >> 1) & 0x7F7F7F;

    return dest_color + c;
}

void bm_rotate_blit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale) {
    /*
    "Fast Bitmap Rotation and Scaling" By Steven Mortimer, Dr Dobbs' Journal, July 01, 2001
    http://www.drdobbs.com/architecture-and-design/fast-bitmap-rotation-and-scaling/184416337
    See also http://www.efg2.com/Lab/ImageProcessing/RotateScanline.htm

    Addendum: I only recently learned about the technique of rotating a
    bitmap with three sheers, and it would be a nice addition to this library.
    <https://cohost.org/tomforsyth/post/891823-rotation-with-three> and
    <https://www.ocf.berkeley.edu/~fricke/projects/israel/paeth/rotation_by_shearing.html>
    */
    int x,y;

    int minx = dst->clip.x1, miny = dst->clip.y1;
    int maxx = dst->clip.x0, maxy = dst->clip.y0;

    double sinAngle = sin(angle);
    double cosAngle = cos(angle);

    double dx, dy;
    /* Compute the position of where each corner on the source bitmap
    will be on the destination to get a bounding box for scanning */
    dx = -cosAngle * px * scale + sinAngle * py * scale + ox;
    dy = -sinAngle * px * scale - cosAngle * py * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = cosAngle * ((double)src->w - px) * scale + sinAngle * py * scale + ox;
    dy = sinAngle * ((double)src->w - px) * scale - cosAngle * py * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = cosAngle * ((double)src->w - px) * scale - sinAngle * ((double)src->h - py) * scale + ox;
    dy = sinAngle * ((double)src->w - px) * scale + cosAngle * ((double)src->h - py) * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = -cosAngle * px * scale - sinAngle * ((double)src->h - py) * scale + ox;
    dy = -sinAngle * px * scale + cosAngle * ((double)src->h - py) * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    /* Clipping */
    if(minx < dst->clip.x0) minx = dst->clip.x0;
    if(maxx > dst->clip.x1 - 1) maxx = dst->clip.x1 - 1;
    if(miny < dst->clip.y0) miny = dst->clip.y0;
    if(maxy > dst->clip.y1 - 1) maxy = dst->clip.y1 - 1;

    double dvCol = cos(angle) / scale;
    double duCol = sin(angle) / scale;

    double duRow = dvCol;
    double dvRow = -duCol;

    double startu = px - (ox * dvCol + oy * duCol);
    double startv = py - (ox * dvRow + oy * duRow);

    double rowu = startu + miny * duCol;
    double rowv = startv + miny * dvCol;

    for(y = miny; y <= maxy; y++) {
        double u = rowu + minx * duRow;
        double v = rowv + minx * dvRow;
        for(x = minx; x <= maxx; x++) {
            if(u >= 0 && u < src->w && v >= 0 && v < src->h) {
                unsigned int c = BM_GET(src, (int)u, (int)v);
                BM_SET(dst, x, y, c);
            }
            u += duRow;
            v += dvRow;
        }
        rowu += duCol;
        rowv += dvCol;
    }
}

void bm_rotate_maskedblit(Bitmap *dst, int ox, int oy, Bitmap *src, int px, int py, double angle, double scale) {
#if IGNORE_ALPHA
    unsigned int maskc = bm_get_color(src) & 0x00FFFFFF;
#else
    unsigned int maskc = bm_get_color(src);
#endif
    int x,y;

    int minx = dst->clip.x1, miny = dst->clip.y1;
    int maxx = dst->clip.x0, maxy = dst->clip.y0;

    double sinAngle = sin(angle);
    double cosAngle = cos(angle);

    double dx, dy;
    /* Compute the position of where each corner on the source bitmap
    will be on the destination to get a bounding box for scanning */
    dx = -cosAngle * px * scale + sinAngle * py * scale + ox;
    dy = -sinAngle * px * scale - cosAngle * py * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = cosAngle * ((double)src->w - px) * scale + sinAngle * py * scale + ox;
    dy = sinAngle * ((double)src->w - px) * scale - cosAngle * py * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = cosAngle * ((double)src->w - px) * scale - sinAngle * ((double)src->h - py) * scale + ox;
    dy = sinAngle * ((double)src->w - px) * scale + cosAngle * ((double)src->h - py) * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    dx = -cosAngle * px * scale - sinAngle * ((double)src->h - py) * scale + ox;
    dy = -sinAngle * px * scale + cosAngle * ((double)src->h - py) * scale + oy;
    if(dx < minx) minx = (int)dx;
    if(dx > maxx) maxx = (int)dx;
    if(dy < miny) miny = (int)dy;
    if(dy > maxy) maxy = (int)dy;

    /* Clipping */
    if(minx < dst->clip.x0) minx = dst->clip.x0;
    if(maxx > dst->clip.x1 - 1) maxx = dst->clip.x1 - 1;
    if(miny < dst->clip.y0) miny = dst->clip.y0;
    if(maxy > dst->clip.y1 - 1) maxy = dst->clip.y1 - 1;

    double dvCol = cos(angle) / scale;
    double duCol = sin(angle) / scale;

    double duRow = dvCol;
    double dvRow = -duCol;

    double startu = px - (ox * dvCol + oy * duCol);
    double startv = py - (ox * dvRow + oy * duRow);

    double rowu = startu + miny * duCol;
    double rowv = startv + miny * dvCol;

    for(y = miny; y <= maxy; y++) {
        double u = rowu + minx * duRow;
        double v = rowv + minx * dvRow;
        for(x = minx; x <= maxx; x++) {
            if(u >= 0 && u < src->w && v >= 0 && v < src->h) {
#if IGNORE_ALPHA
                unsigned int c = BM_GET(src, (int)u, (int)v) & 0x00FFFFFF;
#else
                unsigned int c = BM_GET(src, (int)u, (int)v);
#endif
                if(c != maskc) BM_SET(dst, x, y, c);
            }
            u += duRow;
            v += dvRow;
        }
        rowu += duCol;
        rowv += dvCol;
    }
}

/* I based bm_stretch() on this implementation.
 * http://www.codeproject.com/Articles/36145/Free-Image-Transformation
 *
 * See also
 *  - https://math.stackexchange.com/a/104595
 *  - http://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
 *  - https://www.codeproject.com/Articles/247214/Quadrilateral-Distortion
 *  - http://www.vbforums.com/showthread.php?700187-Code-for-a-four-point-transformation-of-an-image
 */
static BmPoint vec2_sub(BmPoint v1, BmPoint v2) {
    BmPoint v = {v1.x - v2.x, v1.y - v2.y};
    return v;
}
static int vec2_cross(BmPoint v1, BmPoint v2) {
    /* Fun fact about "2D cross product":
     https://stackoverflow.com/a/243984/115589 */
    return v1.x * v2.y - v1.y * v2.x;
}

static BmPoint vec2_interp(BmPoint P, BmPoint D, double t) {
    BmPoint v = {(int)(P.x + t * D.x), (int)(P.y + t * D.y)};
    return v;
}

void bm_stretch(Bitmap *dst, Bitmap *src, BmPoint P[4]) {
    int i;
    int minx = P[0].x, maxx = P[0].x;
    int miny = P[0].y, maxy = P[0].y;
    for(i = 1; i < 4; i++)
    {
        if(P[i].x < minx) minx = P[i].x;
        if(P[i].x > maxx) maxx = P[i].x;
        if(P[i].y < miny) miny = P[i].y;
        if(P[i].y > maxy) maxy = P[i].y;
    }

    BmPoint AB = vec2_sub(P[1], P[0]);
    BmPoint BC = vec2_sub(P[2], P[1]);
    BmPoint CD = vec2_sub(P[3], P[2]);
    BmPoint DA = vec2_sub(P[0], P[3]);

    if(minx < dst->clip.x0)
        minx = dst->clip.x0;
    if(maxx > dst->clip.x1)
        maxx = dst->clip.x1;
    if(miny < dst->clip.y0)
        miny = dst->clip.y0;
    if(maxy > dst->clip.y1)
        maxy = dst->clip.y1;

    BmPoint q;
    for(q.y = miny; q.y < maxy; q.y++) {
        for(q.x = minx; q.x < maxx; q.x++) {

            double nab = vec2_cross(vec2_sub(q, P[0]), AB);
            double nbc = vec2_cross(vec2_sub(q, P[1]), BC);
            double ncd = vec2_cross(vec2_sub(q, P[2]), CD);
            double nda = vec2_cross(vec2_sub(q, P[3]), DA);

            if(nab <= 0 && nbc <= 0 && ncd <= 0 && nda <= 0) {
                int u = (int)((src->clip.x1 - 1 - src->clip.x0) * (nda / (nda + nbc)) + src->clip.x0);
                int v = (int)((src->clip.y1 - 1 - src->clip.y0) * (nab / (nab + ncd)) + src->clip.y0);

                if(u >= 0 && u < src->w && v >= 0 && v < src->h) {
                    unsigned int c = BM_GET(src, u, v);
                    BM_SET(dst, q.x, q.y, c);
                }
            }
        }
    }
}

void bm_destretch(Bitmap *dst, Bitmap *src, BmPoint P[4]) {
    int x, y, w, h;

    w = dst->clip.x1 - dst->clip.x0;
    h = dst->clip.y1 - dst->clip.y0;

    BmPoint AB = vec2_sub(P[1],P[0]);
    BmPoint DC = vec2_sub(P[2],P[3]);

    double ty = 0.0, dty = 1.0 / h;
    double tx = 0.0, dtx = 1.0 / w;

    for(y = dst->clip.y0; y < dst->clip.y1; y++, ty += dty) {
        for(tx = 0.0, x = dst->clip.x0; x < dst->clip.x1; x++, tx += dtx) {
            BmPoint x0 = vec2_interp(P[0], AB, tx);
            BmPoint x1 = vec2_interp(P[3], DC, tx);

            BmPoint uv = vec2_interp(x0, vec2_sub(x1, x0), ty);

            if(uv.x < src->clip.x0 || uv.x >= src->clip.x1 || uv.y < src->clip.y0 || uv.y >= src->clip.y1)
                continue;

            unsigned int c = BM_GET(src, uv.x, uv.y);
            BM_SET(dst,x,y,c);
        }
    }
}

void bm_blit_xbm(Bitmap *dst, int dx, int dy, int sx, int sy, int w, int h, int xbm_w, int xbm_h, unsigned char xbm_data[]) {
  int i,j;
  unsigned int c = bm_get_color(dst);

  assert(sx >= 0 && sx + w <= xbm_w);
  assert(sy >= 0 && sy + h <= xbm_h);
  (void)xbm_h;

  int delta = dst->clip.x0 - dx;
  if(delta > 0) {
    dx = dst->clip.x0;
    sx += delta;
    w -= delta;
  }
  if((dx + w) > dst->clip.x1) {
    w = dst->clip.x1 - dx;
  }
  delta = dst->clip.y0 - dy;
  if(delta > 0) {
    dy = dst->clip.y0;
    sy += delta;
    h -= delta;
  }
  if((dy + h) > dst->clip.y1) {
    h = dst->clip.y1 - dy;
  }

  for(j = 0; j < h; j++) {
    int pix = (sy + j) * xbm_w + sx;
    for(i = 0; i < w; i++, pix++) {
      int byte = pix >> 3;
      int shift = pix & 0x07;
      if(!(xbm_data[byte] & (1 << shift)))
        bm_set(dst, dx + i, dy + j, c);
    }
  }

}

Bitmap *bm_swap_rb(Bitmap *b) {
    int i;
    for(i = 0; i < b->w * b->h; i++) {
        unsigned int *pixp = ((unsigned int *)b->data) + i;
        unsigned int c = *pixp;
        *pixp = (c & 0xFF00FF00) | ((c & 0xFF) << 16) | ((c >> 16) & 0xFF);
    }
    return b;
}

void bm_smooth(Bitmap *b) {
    Bitmap *tmp = bm_create(b->w, b->h);
    int x, y;

    /* http://prideout.net/archive/bloom/ */
    int kernel[] = {1,4,6,4,1};

    assert(b->clip.y0 < b->clip.y1);
    assert(b->clip.x0 < b->clip.x1);

    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            int p, k, c = 0;
            float R = 0, G = 0, B = 0, A = 0;
            for(p = x-2, k = 0; p < x+2; p++, k++) {
                if(p < 0 || p >= b->w)
                    continue;
                R += kernel[k] * BM_GETR(b,p,y);
                G += kernel[k] * BM_GETG(b,p,y);
                B += kernel[k] * BM_GETB(b,p,y);
                A += kernel[k] * BM_GETA(b,p,y);
                c += kernel[k];
            }
            BM_SET_RGBA(tmp, x, y, (unsigned char)(R/c), (unsigned char)(G/c), (unsigned char)(B/c), (unsigned char)(A/c));
        }

    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            int p, k, c = 0;
            float R = 0, G = 0, B = 0, A = 0;
            for(p = y-2, k = 0; p < y+2; p++, k++) {
                if(p < 0 || p >= b->h)
                    continue;
                R += kernel[k] * BM_GETR(tmp,x,p);
                G += kernel[k] * BM_GETG(tmp,x,p);
                B += kernel[k] * BM_GETB(tmp,x,p);
                A += kernel[k] * BM_GETA(tmp,x,p);
                c += kernel[k];
            }
            BM_SET_RGBA(tmp, x, y, (unsigned char)(R/c), (unsigned char)(G/c), (unsigned char)(B/c), (unsigned char)(A/c));
        }

    memcpy(b->data, tmp->data, b->w * b->h * 4);
    bm_free(tmp);
}

void bm_apply_kernel(Bitmap *b, int dim, float kernel[]) {
    Bitmap *tmp = bm_create(b->w, b->h);
    int x, y;
    int kf = dim >> 1;

    assert(b->clip.y0 < b->clip.y1);
    assert(b->clip.x0 < b->clip.x1);

    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int p, q, u, v;
            float R = 0, G = 0, B = 0, A = 0, c = 0;
            for(p = x-kf, u = 0; p <= x+kf; p++, u++) {
                if(p < 0 || p >= b->w)
                    continue;
                for(q = y-kf, v = 0; q <= y+kf; q++, v++) {
                    if(q < 0 || q >= b->h)
                        continue;
                    R += kernel[u + v * dim] * BM_GETR(b,p,q);
                    G += kernel[u + v * dim] * BM_GETG(b,p,q);
                    B += kernel[u + v * dim] * BM_GETB(b,p,q);
                    A += kernel[u + v * dim] * BM_GETA(b,p,q);
                    c += kernel[u + v * dim];
                }
            }
            R /= c; if(R > 255) R = 255;if(R < 0) R = 0;
            G /= c; if(G > 255) G = 255;if(G < 0) G = 0;
            B /= c; if(B > 255) B = 255;if(B < 0) B = 0;
            A /= c; if(A > 255) A = 255;if(A < 0) A = 0;
            BM_SET_RGBA(tmp, x, y, (unsigned char)R, (unsigned char)G, (unsigned char)B, (unsigned char)A);
        }
    }

    memcpy(b->data, tmp->data, b->w * b->h * 4);
    bm_free(tmp);
}

/*
Image scaling functions:
 - bm_resample() : Uses the nearest neighbour
 - bm_resample_blin() : Uses bilinear interpolation.
 - bm_resample_bcub() : Uses bicubic interpolation.
Bilinear Interpolation is better suited for making an image larger.
Bicubic Interpolation is better suited for making an image smaller.
http://blog.codinghorror.com/better-image-resizing/
*/
Bitmap *bm_resample_into(const Bitmap *in, Bitmap *out) {
    int x, y;
    int nw = out->w, nh = out->h;
    for(y = 0; y < nh; y++)
        for(x = 0; x < nw; x++) {
            int sx = x * in->w/nw;
            int sy = y * in->h/nh;
            assert(sx < in->w && sy < in->h);
            BM_SET(out, x, y, BM_GET(in,sx,sy));
        }
    bm_set_palette(out, bm_get_palette(in));
    return out;
}

Bitmap *bm_resample(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    if(!out)
        return NULL;
    return bm_resample_into(in, out);
}

/* http://rosettacode.org/wiki/Bilinear_interpolation */
static double lerp(double s, double e, double t) {
    return s + (e-s)*t;
}
static double blerp(double c00, double c10, double c01, double c11, double tx, double ty) {
    return lerp(
        lerp(c00, c10, tx),
        lerp(c01, c11, tx),
        ty);
}

Bitmap *bm_resample_blin_into(const Bitmap *in, Bitmap *out) {
    int x, y;
    int nw = out->w, nh = out->h;
    for(y = 0; y < nh; y++)
        for(x = 0; x < nw; x++) {
            int C[4], c;
            double gx = (double)x * in->w/(double)nw;
            int sx = (int)gx;
            double gy = (double)y * in->h/(double)nh;
            int sy = (int)gy;
            int dx = 1, dy = 1;
            assert(sx < in->w && sy < in->h);
            if(sx + 1 >= in->w){ sx=in->w-1; dx = 0; }
            if(sy + 1 >= in->h){ sy=in->h-1; dy = 0; }
            for(c = 0; c < 4; c++) {
                int p00 = BM_GETN(in,c,sx,sy);
                int p10 = BM_GETN(in,c,sx+dx,sy);
                int p01 = BM_GETN(in,c,sx,sy+dy);
                int p11 = BM_GETN(in,c,sx+dx,sy+dy);
                C[c] = (int)blerp(p00, p10, p01, p11, gx-sx, gy-sy);
            }
#if !ABGR
            BM_SET_RGBA(out, x, y, C[2], C[1], C[0], C[3]);
#else
            BM_SET_RGBA(out, x, y, C[0], C[1], C[2], C[3]);
#endif
        }
    bm_set_palette(out, bm_get_palette(in));
    return out;
}

Bitmap *bm_resample_blin(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    if(!out)
        return NULL;
    return out = bm_resample_blin_into(in, out);
}

/*
http://www.codeproject.com/Articles/236394/Bi-Cubic-and-Bi-Linear-Interpolation-with-GLSL
except I ported the GLSL code to straight C
*/
static double triangular_fun(double b) {
    b = b * 1.5 / 2.0;
    if( -1.0 < b && b <= 0.0) {
        return b + 1.0;
    } else if(0.0 < b && b <= 1.0) {
        return 1.0 - b;
    }
    return 0;
}

Bitmap *bm_resample_bcub_into(const Bitmap *in, Bitmap *out) {
    int x, y;
    int nw = out->w, nh = out->h;

    for(y = 0; y < nh; y++)
    for(x = 0; x < nw; x++) {

        double sum[4] = {0.0, 0.0, 0.0, 0.0};
        double denom[4] = {0.0, 0.0, 0.0, 0.0};

        double a = (double)x * in->w/(double)nw;
        int sx = (int)a;
        double b = (double)y * in->h/(double)nh;
        int sy = (int)b;

        int m, n, c, C;
        for(m = -1; m < 3; m++ )
        for(n = -1; n < 3; n++) {
            double f = triangular_fun((double)sx - a);
            double f1 = triangular_fun(-((double)sy - b));
            for(c = 0; c < 4; c++) {
                int i = sx+m;
                int j = sy+n;
                if(i < 0) i = 0;
                if(i >= in->w) i = in->w - 1;
                if(j < 0) j = 0;
                if(j >= in->h) j = in->h - 1;
                C = BM_GETN(in, c, i, j);
                sum[c] = sum[c] + C * f1 * f;
                denom[c] = denom[c] + f1 * f;
            }
        }

#if !ABGR
        BM_SET_RGBA(out, x, y, (unsigned char)(sum[2]/denom[2]), (unsigned char)(sum[1]/denom[1]), (unsigned char)(sum[0]/denom[0]), (unsigned char)(sum[3]/denom[3]));
#else
        BM_SET_RGBA(out, x, y, sum[0]/denom[0], sum[1]/denom[1], sum[2]/denom[2], sum[3]/denom[3]);
#endif
    }
    bm_set_palette(out, bm_get_palette(in));
    return out;
}

Bitmap *bm_resample_bcub(const Bitmap *in, int nw, int nh) {
    Bitmap *out = bm_create(nw, nh);
    if(!out)
        return NULL;
    return bm_resample_bcub_into(in, out);
}

Bitmap *bm_rotate_cw(const Bitmap *in) {
    int cw = in->clip.x1 - in->clip.x0;
    int ch = in->clip.y1 - in->clip.y0;
    int oy = in->clip.y0, ox = in->clip.x0;
    int x, y;
    Bitmap *out = bm_create(ch, cw);
    if(!out)
        return NULL;
    for(y = oy; y < in->clip.y1; y++) {
        for(x = ox; x < in->clip.x1; x++) {
            unsigned int c = BM_GET(in, x, y);
            BM_SET(out,ch - (y - oy) - 1, x - ox, c);
        }
    }
    return out;
}

Bitmap *bm_rotate_ccw(const Bitmap *in) {
    int cw = in->clip.x1 - in->clip.x0;
    int ch = in->clip.y1 - in->clip.y0;
    int oy = in->clip.y0, ox = in->clip.x0;
    int x, y;
    Bitmap *out = bm_create(ch, cw);
    if(!out)
        return NULL;
    for(y = oy; y < in->clip.y1; y++) {
        for(x = ox; x < in->clip.x1; x++) {
            unsigned int c = BM_GET(in, x, y);
            BM_SET(out, y - oy, cw - (x - ox) - 1, c);
        }
    }
    return out;
}

void bm_set_alpha(Bitmap *bm, int a) {
    if(a < 0) a = 0;
    if(a > 255) a = 255;
    bm->color = (bm->color & 0x00FFFFFF) | (a << 24);
}

/* Lookup table for bm_atoi()
 * This list is based on the HTML and X11 colors on the
 * Wikipedia's list of web colors:
 * http://en.wikipedia.org/wiki/Web_colors
 * I also felt a bit nostalgic for the EGA graphics from my earliest
 * computer memories, so I added the EGA colors (prefixed with "EGA") from here:
 * http://en.wikipedia.org/wiki/Enhanced_Graphics_Adapter
 *
 * Keep the list sorted because a binary search is used.
 *
 * bm_atoi()'s text parameter is not case sensitive and spaces are
 * ignored, so for example "darkred" and "Dark Red" are equivalent.
 */
static const struct color_map_entry {
    const char *name;
    unsigned int color;
} color_map[] = {
    {"ALICEBLUE", 0xF0F8FF},
    {"ANTIQUEWHITE", 0xFAEBD7},
    {"AQUA", 0x00FFFF},
    {"AQUAMARINE", 0x7FFFD4},
    {"AZURE", 0xF0FFFF},
    {"BEIGE", 0xF5F5DC},
    {"BISQUE", 0xFFE4C4},
    {"BLACK", 0x000000},
    {"BLANCHEDALMOND", 0xFFEBCD},
    {"BLUE", 0x0000FF},
    {"BLUEVIOLET", 0x8A2BE2},
    {"BROWN", 0xA52A2A},
    {"BURLYWOOD", 0xDEB887},
    {"CADETBLUE", 0x5F9EA0},
    {"CHARTREUSE", 0x7FFF00},
    {"CHOCOLATE", 0xD2691E},
    {"CORAL", 0xFF7F50},
    {"CORNFLOWERBLUE", 0x6495ED},
    {"CORNSILK", 0xFFF8DC},
    {"CRIMSON", 0xDC143C},
    {"CYAN", 0x00FFFF},
    {"DARKBLUE", 0x00008B},
    {"DARKCYAN", 0x008B8B},
    {"DARKGOLDENROD", 0xB8860B},
    {"DARKGRAY", 0xA9A9A9},
    {"DARKGREEN", 0x006400},
    {"DARKKHAKI", 0xBDB76B},
    {"DARKMAGENTA", 0x8B008B},
    {"DARKOLIVEGREEN", 0x556B2F},
    {"DARKORANGE", 0xFF8C00},
    {"DARKORCHID", 0x9932CC},
    {"DARKRED", 0x8B0000},
    {"DARKSALMON", 0xE9967A},
    {"DARKSEAGREEN", 0x8FBC8F},
    {"DARKSLATEBLUE", 0x483D8B},
    {"DARKSLATEGRAY", 0x2F4F4F},
    {"DARKTURQUOISE", 0x00CED1},
    {"DARKVIOLET", 0x9400D3},
    {"DEEPPINK", 0xFF1493},
    {"DEEPSKYBLUE", 0x00BFFF},
    {"DIMGRAY", 0x696969},
    {"DODGERBLUE", 0x1E90FF},
    {"EGABLACK", 0x000000},
    {"EGABLUE", 0x0000AA},
    {"EGABRIGHTBLACK", 0x555555},
    {"EGABRIGHTBLUE", 0x5555FF},
    {"EGABRIGHTCYAN", 0x55FFFF},
    {"EGABRIGHTGREEN", 0x55FF55},
    {"EGABRIGHTMAGENTA", 0xFF55FF},
    {"EGABRIGHTRED", 0xFF5555},
    {"EGABRIGHTWHITE", 0xFFFFFF},
    {"EGABRIGHTYELLOW", 0xFFFF55},
    {"EGABROWN", 0xAA5500},
    {"EGACYAN", 0x00AAAA},
    {"EGADARKGRAY", 0x555555},
    {"EGAGREEN", 0x00AA00},
    {"EGALIGHTGRAY", 0xAAAAAA},
    {"EGAMAGENTA", 0xAA00AA},
    {"EGARED", 0xAA0000},
    {"EGAWHITE", 0xAAAAAA},
    {"FIREBRICK", 0xB22222},
    {"FLORALWHITE", 0xFFFAF0},
    {"FORESTGREEN", 0x228B22},
    {"FUCHSIA", 0xFF00FF},
    {"GAINSBORO", 0xDCDCDC},
    {"GHOSTWHITE", 0xF8F8FF},
    {"GOLD", 0xFFD700},
    {"GOLDENROD", 0xDAA520},
    {"GRAY", 0x808080},
    {"GREEN", 0x008000},
    {"GREENYELLOW", 0xADFF2F},
    {"HONEYDEW", 0xF0FFF0},
    {"HOTPINK", 0xFF69B4},
    {"INDIANRED", 0xCD5C5C},
    {"INDIGO", 0x4B0082},
    {"IVORY", 0xFFFFF0},
    {"KHAKI", 0xF0E68C},
    {"LAVENDER", 0xE6E6FA},
    {"LAVENDERBLUSH", 0xFFF0F5},
    {"LAWNGREEN", 0x7CFC00},
    {"LEMONCHIFFON", 0xFFFACD},
    {"LIGHTBLUE", 0xADD8E6},
    {"LIGHTCORAL", 0xF08080},
    {"LIGHTCYAN", 0xE0FFFF},
    {"LIGHTGOLDENRODYELLOW", 0xFAFAD2},
    {"LIGHTGRAY", 0xD3D3D3},
    {"LIGHTGREEN", 0x90EE90},
    {"LIGHTPINK", 0xFFB6C1},
    {"LIGHTSALMON", 0xFFA07A},
    {"LIGHTSEAGREEN", 0x20B2AA},
    {"LIGHTSKYBLUE", 0x87CEFA},
    {"LIGHTSLATEGRAY", 0x778899},
    {"LIGHTSTEELBLUE", 0xB0C4DE},
    {"LIGHTYELLOW", 0xFFFFE0},
    {"LIME", 0x00FF00},
    {"LIMEGREEN", 0x32CD32},
    {"LINEN", 0xFAF0E6},
    {"MAGENTA", 0xFF00FF},
    {"MAROON", 0x800000},
    {"MEDIUMAQUAMARINE", 0x66CDAA},
    {"MEDIUMBLUE", 0x0000CD},
    {"MEDIUMORCHID", 0xBA55D3},
    {"MEDIUMPURPLE", 0x9370DB},
    {"MEDIUMSEAGREEN", 0x3CB371},
    {"MEDIUMSLATEBLUE", 0x7B68EE},
    {"MEDIUMSPRINGGREEN", 0x00FA9A},
    {"MEDIUMTURQUOISE", 0x48D1CC},
    {"MEDIUMVIOLETRED", 0xC71585},
    {"MIDNIGHTBLUE", 0x191970},
    {"MINTCREAM", 0xF5FFFA},
    {"MISTYROSE", 0xFFE4E1},
    {"MOCCASIN", 0xFFE4B5},
    {"NAVAJOWHITE", 0xFFDEAD},
    {"NAVY", 0x000080},
    {"OLDLACE", 0xFDF5E6},
    {"OLIVE", 0x808000},
    {"OLIVEDRAB", 0x6B8E23},
    {"ORANGE", 0xFFA500},
    {"ORANGERED", 0xFF4500},
    {"ORCHID", 0xDA70D6},
    {"PALEGOLDENROD", 0xEEE8AA},
    {"PALEGREEN", 0x98FB98},
    {"PALETURQUOISE", 0xAFEEEE},
    {"PALEVIOLETRED", 0xDB7093},
    {"PAPAYAWHIP", 0xFFEFD5},
    {"PEACHPUFF", 0xFFDAB9},
    {"PERU", 0xCD853F},
    {"PINK", 0xFFC0CB},
    {"PLUM", 0xDDA0DD},
    {"POWDERBLUE", 0xB0E0E6},
    {"PURPLE", 0x800080},
    {"RED", 0xFF0000},
    {"ROSYBROWN", 0xBC8F8F},
    {"ROYALBLUE", 0x4169E1},
    {"SADDLEBROWN", 0x8B4513},
    {"SALMON", 0xFA8072},
    {"SANDYBROWN", 0xF4A460},
    {"SEAGREEN", 0x2E8B57},
    {"SEASHELL", 0xFFF5EE},
    {"SIENNA", 0xA0522D},
    {"SILVER", 0xC0C0C0},
    {"SKYBLUE", 0x87CEEB},
    {"SLATEBLUE", 0x6A5ACD},
    {"SLATEGRAY", 0x708090},
    {"SNOW", 0xFFFAFA},
    {"SPRINGGREEN", 0x00FF7F},
    {"STEELBLUE", 0x4682B4},
    {"TAN", 0xD2B48C},
    {"TEAL", 0x008080},
    {"THISTLE", 0xD8BFD8},
    {"TOMATO", 0xFF6347},
    {"TURQUOISE", 0x40E0D0},
    {"VIOLET", 0xEE82EE},
    {"WHEAT", 0xF5DEB3},
    {"WHITE", 0xFFFFFF},
    {"WHITESMOKE", 0xF5F5F5},
    {"YELLOW", 0xFFFF00},
    {"YELLOWGREEN", 0x9ACD32},
    {NULL, 0}
};

unsigned int bm_atoi(const char *text) {
    unsigned int col = 0;

    /* Caveat: If you have 8 hex digits preceded by a #, then
    it is treated as a #RRGGBBAA as in CSS;
    If you have 8 hex digits not preceded by a #, then it
    is treated as AARRGGBB
    */
    int swap_alpha = 0;

    if(!text) return 0;

    while(isspace(text[0]))
        text++;

    if(tolower(text[0]) == 'r' && tolower(text[1]) == 'g' && tolower(text[2]) == 'b') {
        /* Color is given like RGB(r,g,b) or RGBA(r,g,b,a) */
        int i = 0,a = 0, c[4];
        text += 3;
        if(text[0] == 'a') {
            a = 1;
            text++;
        }
        if(text[0] != '(') return 0;
        do {
            text++;
            size_t len;
            char buf[10];
            while(isspace(text[0]))
                text++;
            len = strspn(text, "0123456789.");
            if(len >= sizeof buf)
                return 0;
#ifdef SAFE_C11
            strncpy_s(buf, sizeof buf, text, len);
#else
            strncpy(buf,text,len);
#endif
            buf[len] = '\0';
            text += len;

            if(text[0] == '%') {
                double p = atof(buf);
                c[i++] = (int)(p * 255 / 100);
                text++;
            } else {
                if(i == 3) {
                    /* alpha value is given as a value between 0.0 and 1.0 */
                    double p = atof(buf);
                    c[i++] = (int)(p * 255);
                } else {
                    c[i++] = atoi(buf);
                }
            }
            while(isspace(text[0]))
                text++;

        } while(text[0] == ',' && i < 4);

        if(text[0] != ')' || i != (a ? 4 : 3))
            return 0;

        if(a)
            return bm_rgba(c[0], c[1], c[2], c[3]);
        else
            return bm_rgb(c[0], c[1], c[2]);
    } else if(tolower(text[0]) == 'h' && tolower(text[1]) == 's' && tolower(text[2]) == 'l') {
        /* Color is given like HSL(h,s,l) or HSLA(h,s,l,a) */
        int i = 0,a = 0;
        double c[4];
        text += 3;
        if(text[0] == 'a') {
            a = 1;
            text++;
        }
        if(text[0] != '(') return 0;
        do {
            text++;
            size_t len;
            char buf[10];
            while(isspace(text[0]))
                text++;
            len = strspn(text, "0123456789.");
            if(len >= sizeof buf)
                return 0;
#ifdef SAFE_C11
            strncpy_s(buf, sizeof buf, text, len);
#else
            strncpy(buf, text, len);
#endif
            buf[len] = '\0';
            text += len;

            c[i] = atof(buf);
            if(i == 1 || i == 2) {
                if(text[0] == '%')
                    text++;
            }
            i++;

            while(isspace(text[0]))
                text++;

        } while(text[0] == ',' && i < 4);

        if(text[0] != ')' || i != (a ? 4 : 3))
            return 0;

        if(a)
            return bm_hsla(c[0], c[1], c[2], c[3] * 100);
        else
            return bm_hsl(c[0], c[1], c[2]);

    } else if(isalpha(text[0])) {
        const char *q, *p;

        int min = 0, max = ((sizeof color_map)/(sizeof color_map[0])) - 1;
        while(min <= max) {
            int i = (max + min) >> 1, r;

            p = text;
            q = color_map[i].name;

            /* Hacky case insensitive strcmp() that ignores spaces in p */
            while(*p) {
                if(*p == ' ') p++;
                else {
                    if(tolower(*p) != tolower(*q))
                        break;
                    p++; q++;
                }
            }
            r = tolower(*p) - tolower(*q);

            if(r == 0)
                return bm_byte_order(color_map[i].color);
            else if(r < 0) {
                max = i - 1;
            } else {
                min = i + 1;
            }
        }
        /* Drop through: You may be dealing with a color like 'a6664c' */
    } else if(text[0] == '#') {
        text++;
        swap_alpha = 1;
        if(strlen(text) == 3) {
            /* Special case of #RGB that should be treated as #RRGGBB */
            while(text[0]) {
                int c = tolower(text[0]);
                if(c >= 'a' && c <= 'f') {
                    col = (col << 4) + (c - 'a' + 10);
                    col = (col << 4) + (c - 'a' + 10);
                } else {
                    col = (col << 4) + (c - '0');
                    col = (col << 4) + (c - '0');
                }
                text++;
            }
            return bm_byte_order(col);
        }
    } else if(text[0] == '0' && tolower(text[1]) == 'x') {
        text += 2;
    }

    if(tolower(text[0]) == 'g' && tolower(text[1]) == 'r'
        && (tolower(text[2]) == 'a' || tolower(text[2]) == 'e') && tolower(text[3]) == 'y'
        && isdigit(text[4])) {
        /* Color specified like "Gray50", see
            https://en.wikipedia.org/wiki/X11_color_names#Shades_of_gray
            I don't intend to support the other X11 variations. */
        col = atoi(text+4) * 255 / 100;
        return col | (col << 8) | (col << 16);
    }

    if(strlen(text) == 8) {
        while(isxdigit(text[0])) {
            int c = tolower(text[0]);
            if(c >= 'a' && c <= 'f') {
                col = (col << 4) + (c - 'a' + 10);
            } else {
                col = (col << 4) + (c - '0');
            }
            text++;
        }
        if(swap_alpha) {
            col = ((col & 0xFF) << 24) | ((col & 0xFFFFFF00) >> 8);
        }
    } else if(strlen(text) == 6) {
        while(isxdigit(text[0])) {
            int c = tolower(text[0]);
            if(c >= 'a' && c <= 'f') {
                col = (col << 4) + (c - 'a' + 10);
            } else {
                col = (col << 4) + (c - '0');
            }
            text++;
        }
    } else {
        return 0;
    }
    return bm_byte_order(col);
}

unsigned int bm_rgb(unsigned char R, unsigned char G, unsigned char B) {
#if !ABGR
    return 0xFF000000 | ((R) << 16) | ((G) << 8) | (B);
#else
    return 0xFF000000 | ((B) << 16) | ((G) << 8) | (R);
#endif
}

int bm_colcmp(unsigned int c1, unsigned int c2) {
    return (c1 & 0xFFFFFF) == (c2 & 0xFFFFFF);
}

unsigned int bm_rgba(unsigned char R, unsigned char G, unsigned char B, unsigned char A) {
#if !ABGR
    return ((A) << 24) | ((R) << 16) | ((G) << 8) | (B);
#else
    return ((A) << 24) | ((B) << 16) | ((G) << 8) | (R);
#endif
}

void bm_get_rgb(unsigned int col, unsigned char *R, unsigned char *G, unsigned char *B) {
    assert(R);
    assert(G);
    assert(B);
#if !ABGR
    *R = (col >> 16) & 0xFF;
    *G = (col >> 8) & 0xFF;
    *B = (col >> 0) & 0xFF;
#else
    *B = (col >> 16) & 0xFF;
    *G = (col >> 8) & 0xFF;
    *R = (col >> 0) & 0xFF;
#endif
}

unsigned int bm_hsl(double H, double S, double L) {
    /* The formula is based on the one on the wikipedia:
     * https://en.wikipedia.org/wiki/HSL_and_HSV#Converting_to_RGB
     */
    double R = 0, G = 0, B = 0;
    double C, X, m;

    if(H > 0)
        H = fmod(H, 360.0);
    if(S < 0) S = 0;
    if(S > 100.0) S = 100.0;
    S /= 100.0;
    if(L < 0) L = 0;
    if(L > 100.0) L = 100;
    L /= 100.0;

    C = (1.0 - fabs(2.0 * L - 1.0)) * S;
    H = H / 60.0;
    X = C * (1.0 - fabs(fmod(H, 2.0) - 1.0));

    /* Treat H < 0 as H undefined */
    if(H >= 0 && H < 1) {
        R = C; G = X; B = 0;
    } else if (H < 2) {
        R = X; G = C; B = 0;
    } else if (H < 3) {
        R = 0; G = C; B = X;
    } else if (H < 4) {
        R = 0; G = X; B = C;
    } else if (H < 5) {
        R = X; G = 0; B = C;
    } else if (H < 6) {
        R = C; G = 0; B = X;
    }
    m = L - 0.5 * C;

    return bm_rgb((unsigned char)((R + m) * 255.0), (unsigned char)((G + m) * 255.0), (unsigned char)((B + m) * 255.0));
}

unsigned int bm_hsla(double H, double S, double L, double A) {
    unsigned int a = (unsigned int)(A * 255 / 100);
    unsigned int c = bm_hsl(H,S,L);
    return (c & 0x00FFFFFF) | ((a & 0xFF) << 24);
}

void bm_get_hsl(unsigned int col, double *H, double *S, double *L) {
    unsigned char R, G, B, M, m, C;
    assert(H && S && L);
    bm_get_rgb(col, &R, &G, &B);
    M = MAX(R, MAX(G, B));
    m = MIN(R, MIN(G, B));
    C = M - m;
    if(C == 0) {
        *H = 0;
    } else if (M == R) {
        *H = fmod(((double)G - (double)B)/(double)C, 6);
    } else if (M == G) {
        *H = (double)(B - R)/C + 2.0;
    } else if (M == B) {
        *H = (double)(R - G)/C + 4.0;
    }
    *H = fmod(*H * 60.0, 360);
    if(*H < 0) *H = 360.0 + *H;
    *L = 0.5 * ((double)M + (double)m) / 255.0;
    if(C == 0) {
        *S = 0;
    } else {
        *S = (double)C / (1.0 - fabs(2.0 * *L - 1.0)) / 255.0;
    }
    *L *= 100;
    *S *= 100;
}

unsigned int bm_byte_order(unsigned int col) {
#if !ABGR
    return col;
#else
    return (col & 0xFF00FF00) | ((col >> 16) & 0x000000FF) | ((col & 0x000000FF) << 16);
#endif
}

unsigned int bm_lerp(unsigned int color1, unsigned int color2, double t) {
    int r1, g1, b1;
    int r2, g2, b2;
    int r3, g3, b3;

    if(t <= 0.0) return color1;
    if(t >= 1.0) return color2;

    r1 = (color1 >> 16) & 0xFF; g1 = (color1 >> 8) & 0xFF; b1 = (color1 >> 0) & 0xFF;
    r2 = (color2 >> 16) & 0xFF; g2 = (color2 >> 8) & 0xFF; b2 = (color2 >> 0) & 0xFF;

    r3 = (int)(r1 + t * ((double)r2 - r1));
    g3 = (int)(g1 + t * ((double)g2 - g1));
    b3 = (int)(b1 + t * ((double)b2 - b1));

    return (r3 << 16) | (g3 << 8) | (b3 << 0);
}

unsigned int bm_graypixel(unsigned int c) {
    unsigned char R,G,B;
    bm_get_rgb(c, &R, &G, &B);
    return (2126 * R + 7152 * G + 722 * B)/10000;
}

void bm_grayscale(Bitmap *b) {
    /* https://en.wikipedia.org/wiki/Grayscale */
    int x, y;
    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            unsigned int c = BM_GET(b, x, y);
            unsigned char R,G,B;
            bm_get_rgb(c, &R, &G, &B);
            c = (2126 * R + 7152 * G + 722 * B)/10000;
            BM_SET(b, x, y, bm_rgb(c, c, c));
        }
}

void bm_swap_color(Bitmap *b, unsigned int src, unsigned int dest) {
    /* Why does this function exist again? */
    int x,y;
#if IGNORE_ALPHA
    src |= 0xFF000000; dest |= 0xFF000000;
#endif
    for(y = 0; y < b->h; y++)
        for(x = 0; x < b->w; x++) {
            if(BM_GET(b,x,y) == src) {
                BM_SET(b, x, y, dest);
            }
        }
}

unsigned int bm_picker(Bitmap *b, int x, int y) {
    assert(b);
    if(x < 0 || x >= b->w || y < 0 || y >= b->h)
        return 0;
    b->color = BM_GET(b, x, y);
    return b->color;
}

int bm_width(Bitmap *b) {
    assert(b);
    return b->w;
}

int bm_height(Bitmap *b) {
    assert(b);
    return b->h;
}

unsigned char *bm_raw_data(Bitmap *b) {
    assert(b);
    return b->data;
}

int bm_pixel_count(Bitmap *b) {
    assert(b);
    return b->w * b->h;
}

void bm_clear(Bitmap *b) {
    int i, j;
    assert(b);
    for(j = 0; j < b->h; j++)
        for(i = 0; i < b->w; i++) {
            BM_SET(b, i, j, b->color);
        }
}

void bm_putpixel(Bitmap *b, int x, int y) {
    assert(b);
    if(x < b->clip.x0 || x >= b->clip.x1 || y < b->clip.y0 || y >= b->clip.y1)
        return;
    BM_SET(b, x, y, b->color);
}

void bm_line(Bitmap *b, int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx, sy;
    int err, e2;

    assert(b);
    if(dx < 0) dx = -dx;
    if(dy < 0) dy = -dy;

    if(x0 < x1)
        sx = 1;
    else
        sx = -1;
    if(y0 < y1)
        sy = 1;
    else
        sy = -1;

    err = dx - dy;

    for(;;) {
        /* Clipping can probably be more effective... */
        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0, y0, b->color);

        if(x0 == x1 && y0 == y1) break;

        e2 = 2 * err;

        if(e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if(e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/*
Xiaolin Wu's line algorithm.
This code is pretty much based on the [Wikipedia](https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm)
version, though I did read Michael Abrash's article in the June 1992 issue of Dr Dobbs'.
See also <https://www.geeksforgeeks.org/anti-aliased-line-xiaolin-wus-algorithm/>, though their
implementation looks like a straight C port of the Wikipedia code
*/
void bm_line_aa(Bitmap *b, int x0, int y0, int x1, int y1) {
#define FPART(x) ((x) - (int)(x))
    int x, y, dx, dy;
    unsigned int c0, c1 = bm_get_color(b);
    double gradient, intery;

    int steep = abs(y1 - y0) > abs(x1 - x0);
    if(steep) {
        SWAPINT(x0, y0);
        SWAPINT(x1, y1);
    }
    if(x0 > x1) {
        SWAPINT(x0, x1);
        SWAPINT(y0, y1);
    }

    dx = x1 - x0;
    dy = y1 - y0;

    /* Clipping */
    if(steep) {
        if(x0 >= b->clip.y1 || x1 < b->clip.y0)
            return;
        if(y0 < y1) {
            if(y0 >= b->clip.x1 || y1 < b->clip.x0)
                return;
        } else {
            if(y1 >= b->clip.x1 || y0 < b->clip.x0)
                return;
        }
    } else {
        if(x0 >= b->clip.x1 || x1 < b->clip.x0)
            return;
        if(y0 < y1) {
            if(y0 >= b->clip.y1 || y1 < b->clip.y0)
                return;
        } else {
            if(y1 >= b->clip.y1 || y0 < b->clip.y0)
                return;
        }
    }

    /* Special cases */
    if(dy == 0) {

        if(steep) {
            if(y0 < b->clip.x0 || y0 >= b->clip.x1)
                return;
            for(x = x0; x <= x1; x++) {
                if(x < b->clip.y0)
                    continue;
                else if(x >= b->clip.y1)
                    break;
                bm_set(b, y0, x, c1);
            }
        } else {
            if(y0 < b->clip.y0 || y0 >= b->clip.y1)
                return;
            for(x = x0; x <= x1; x++) {
                if(x < b->clip.x0)
                    continue;
                else if(x >= b->clip.x1)
                    break;
                bm_set(b, x, y0, c1);
            }
        }

        return;
    /* } else if(dx == 0) { -- doesn't matter because of the earlier X/Y swap */
    } else if(dx == dy) {
        dy = y0 < y1 ? 1 : -1;
        for(x = x0, y = y0; x <= x1; x++, y += dy) {
            if(x < b->clip.x0)
                continue;
            else if(x >= b->clip.x1)
                break;
            if(y < b->clip.y0 || y >= b->clip.y1)
                continue;

            bm_set(b, x, y, c1);
        }
        return;
    }

    /* I omit the Wikipedia's part about the endpoints
    because x0,y0 and x1,y1 are integers.
    TODO: Perhaps they should not be integers.
    */

    gradient = (double)dy / dx;
    intery = y0;

    if(steep) {
        for(x = x0; x <= x1; x++, intery += gradient) {

            if(x < b->clip.y0)
                continue;
            else if(x >= b->clip.y1)
                break;

            y = (int)intery;

            if(y < b->clip.x0 || y >= b->clip.x1)
                continue;

            c0 = BM_GET(b, y, x);
            bm_set(b, y, x, bm_lerp(c0, c1, 1.0 - FPART(intery)));

            y++;

            if(y < b->clip.x0 || y >= b->clip.x1)
                continue;

            c0 = BM_GET(b, y, x);
            bm_set(b, y, x, bm_lerp(c0, c1, FPART(intery)));
        }
    } else {
        for(x = x0; x <= x1; x++, intery += gradient) {

            if(x < b->clip.x0)
                continue;
            else if(x >= b->clip.x1)
                break;

            y = (int)intery;
            if(y < b->clip.y0 || y >= b->clip.y1)
                continue;
            c0 = BM_GET(b, x, y);
            bm_set(b, x, y, bm_lerp(c0, c1, 1.0 - FPART(intery)));

            y++;
            if(y < b->clip.y0 || y >= b->clip.y1)
                continue;
            c0 = BM_GET(b, x, y);
            bm_set(b, x, y, bm_lerp(c0, c1, FPART(intery)));
        }
    }
}

void bm_rect(Bitmap *b, int x0, int y0, int x1, int y1) {
    assert(b);
    bm_line(b, x0, y0, x1, y0);
    bm_line(b, x1, y0, x1, y1);
    bm_line(b, x1, y1, x0, y1);
    bm_line(b, x0, y1, x0, y0);
}

void bm_fillrect(Bitmap *b, int x0, int y0, int x1, int y1) {
    int x,y;
    assert(b);
    if(x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }
    if(y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    for(y = MAX(y0, b->clip.y0); y < MIN(y1 + 1, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x < MIN(x1 + 1, b->clip.x1); x++) {
            assert(y >= 0 && y < b->h && x >= 0 && x < b->w);
            BM_SET(b, x, y, b->color);
        }
    }
}

void bm_dithrect(Bitmap *b, int x0, int y0, int x1, int y1) {
    int x,y;
    assert(b);
    if(x1 < x0) {
        x = x0;
        x0 = x1;
        x1 = x;
    }
    if(y1 < y0) {
        y = y0;
        y0 = y1;
        y1 = y;
    }
    for(y = MAX(y0, b->clip.y0); y < MIN(y1 + 1, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x < MIN(x1 + 1, b->clip.x1); x++) {
            if((x + y) & 0x1) continue;
            assert(y >= 0 && y < b->h && x >= 0 && x < b->w);
            BM_SET(b, x, y, b->color);
        }
    }
}

void bm_circle(Bitmap *b, int x0, int y0, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    assert(b);
    do {
        int xp, yp;

        /* Lower Right */
        xp = x0 - x; yp = y0 + y;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Lower Left */
        xp = x0 - y; yp = y0 - x;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Left */
        xp = x0 + x; yp = y0 - y;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Right */
        xp = x0 + y; yp = y0 + x;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_fillcircle(Bitmap *b, int x0, int y0, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    assert(b);
    do {
        int i;
        for(i = x0 + x; i <= x0 - x; i++) {
            /* Maybe the clipping can be more effective... */
            int yp = y0 + y;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
            yp = y0 - y;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
        }

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_ellipse(Bitmap *b, int x0, int y0, int x1, int y1) {
    int a = abs(x1-x0), b0 = abs(y1-y0), b1 = b0 & 1;
    long dx = 4 * (1 - a) * b0 * b0,
        dy = 4*(b1 + 1) * a * a;
    long err = dx + dy + b1*a*a, e2;

    assert(b);
    if(x0 > x1) { x0 = x1; x1 += a; }
    if(y0 > y1) { y0 = y1; }
    y0 += (b0+1)/2;
    y1 = y0 - b1;
    a *= 8*a;
    b1 = 8 * b0 * b0;

    do {
        if(x1 >= b->clip.x0 && x1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1, y0, b->color);

        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0, y0, b->color);

        if(x0 >= b->clip.x0 && x0 < b->clip.x1 && y1 >= b->clip.y0 && y1 < b->clip.y1)
            BM_SET(b, x0, y1, b->color);

        if(x1 >= b->clip.x0 && x1 < b->clip.x1 && y1 >= b->clip.y0 && y1 < b->clip.y1)
            BM_SET(b, x1, y1, b->color);

        e2 = 2 * err;
        if(e2 <= dy) {
            y0++; y1--; err += dy += a;
        }
        if(e2 >= dx || 2*err > dy) {
            x0++; x1--; err += dx += b1;
        }
    } while(x0 <= x1);

    while(y0 - y1 < b0) {
        if(x0 - 1 >= b->clip.x0 && x0 - 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0 - 1, y0, b->color);

        if(x1 + 1 >= b->clip.x0 && x1 + 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1 + 1, y0, b->color);
        y0++;

        if(x0 - 1 >= b->clip.x0 && x0 - 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x0 - 1, y1, b->color);

        if(x1 + 1 >= b->clip.x0 && x1 + 1 < b->clip.x1 && y0 >= b->clip.y0 && y0 < b->clip.y1)
            BM_SET(b, x1 + 1, y1, b->color);
        y1--;
    }
}

void bm_fillellipse(Bitmap *b, int x0, int y0, int x1, int y1) {
    int a = abs(x1-x0), b0 = abs(y1-y0), b1 = b0 & 1;
    long dx = 4 * (1 - a) * b0 * b0,
        dy = 4*(b1 + 1) * a * a;
    long err = dx + dy + b1*a*a, e2;

    int xs, xe, x;

    assert(b);
    if(x0 > x1) { x0 = x1; x1 += a; }
    if(y0 > y1) { y0 = y1; }
    y0 += (b0+1)/2;
    y1 = y0 - b1;
    a *= 8*a;
    b1 = 8 * b0 * b0;

    do {

        if(y0 >= b->clip.y0 && y0 < b->clip.y1) {
            xs = x0;
            xe = x1;
            if(xs < b->clip.x0)
                xs = b->clip.x0;
            if(xe >= b->clip.x1)
                xe = b->clip.x1 - 1;
            for(x = xs; x <= xe; x++)
                BM_SET(b, x, y0, b->color);
        }

        if(y1 >= b->clip.y0 && y1 < b->clip.y1) {
            xs = x0;
            xe = x1;
            if(xs < b->clip.x0)
                xs = b->clip.x0;
            if(xe >= b->clip.x1)
                xe = b->clip.x1 - 1;
            for(x = xs; x <= xe; x++)
                BM_SET(b, x, y1, b->color);
        }

        e2 = 2 * err;
        if(e2 <= dy) {
            y0++; y1--; err += dy += a;
        }
        if(e2 >= dx || 2*err > dy) {
            x0++; x1--; err += dx += b1;
        }
    } while(x0 <= x1);

    while(y0 - y1 < b0) {
        /* When is this part used again? */

        if(y0 >= b->clip.y0 && y0 < b->clip.y1) {
            xs = x0 - 1;
            xe = x1 + 1;
            if(xs < b->clip.x0)
                xs = b->clip.x0;
            if(xe >= b->clip.x1)
                xe = b->clip.x1 - 1;
            for(x = xs; x <= xe; x++)
                BM_SET(b, x, y0, b->color);
        }
        y0++;

        if(y1 >= b->clip.y0 && y1 < b->clip.y1) {
            xs = x0 - 1;
            xe = x1 + 1;
            if(xs < b->clip.x0)
                xs = b->clip.x0;
            if(xe >= b->clip.x1)
                xe = b->clip.x1 - 1;
            for(x = xs; x <= xe; x++)
                BM_SET(b, x, y1, b->color);
        }
        y1--;
    }
}

void bm_roundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    int rad = r;

    assert(b);
    bm_line(b, x0 + r, y0, x1 - r, y0);
    bm_line(b, x0, y0 + r, x0, y1 - r);
    bm_line(b, x0 + r, y1, x1 - r, y1);
    bm_line(b, x1, y0 + r, x1, y1 - r);

    do {
        int xp, yp;

        /* Lower Right */
        xp = x1 - x - rad; yp = y1 + y - rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Lower Left */
        xp = x0 - y + rad; yp = y1 - x - rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Left */
        xp = x0 + x + rad; yp = y0 - y + rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        /* Upper Right */
        xp = x1 + y - rad; yp = y0 + x + rad;
        if(xp >= b->clip.x0 && xp < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
            BM_SET(b, xp, yp, b->color);

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);
}

void bm_fillroundrect(Bitmap *b, int x0, int y0, int x1, int y1, int r) {
    int x = -r;
    int y = 0;
    int err = 2 - 2 * r;
    int rad = r;
    assert(b);
    do {
        int xp, xq, yp, i;

        xp = x0 + x + rad;
        xq = x1 - x - rad;
        for(i = xp; i <= xq; i++) {
            yp = y1 + y - rad;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
            yp = y0 - y + rad;
            if(i >= b->clip.x0 && i < b->clip.x1 && yp >= b->clip.y0 && yp < b->clip.y1)
                BM_SET(b, i, yp, b->color);
        }

        r = err;
        if(r > x) {
            x++;
            err += x*2 + 1;
        }
        if(r <= y) {
            y++;
            err += y * 2 + 1;
        }
    } while(x < 0);

    for(y = MAX(y0 + rad + 1, b->clip.y0); y < MIN(y1 - rad, b->clip.y1); y++) {
        for(x = MAX(x0, b->clip.x0); x <= MIN(x1,b->clip.x1 - 1); x++) {
            assert(x >= 0 && x < b->w && y >= 0 && y < b->h);
            BM_SET(b, x, y, b->color);
        }
    }
}

/* The problem with drawing bezier functions is finding a suitable value for
 * incrementing the `t` parameter with. My implementation takes a guess from
 * the distance between the control points, and adjusts the guess if new pixels
 * on the curve are too close or too far from the last pixel drawn. It seemed
 * to work pretty well for my test cases.
 *
 * [bez][] describes an alternative that subdivides the curve with different
 * values of `t` until a certain quality threshold is met and then draws lines
 * between those points.
 *
 * [bez]: http://devmag.org.za/2011/04/05/bzier-curves-a-tutorial/
 */
void bm_bezier3(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2) {
    /* Quadratic Bezier curve */
    int x, y, lx = x0, ly = y0;

    assert(b);

    bm_putpixel(b, x0, y0);

    /* Take a guess as to how many pixels need to be drawn
    using the Manhattan distance between the control points */
    int steps = abs(x1 - x0) + abs(y1 - y0) + abs(x2 - x1) + abs(y2 - y1) + abs(x2 - x1) + abs(y2 - y1);
    if(steps == 0)
        return;
    double  t = 0, inc = 1.0/steps;
    do {
        double dt = t + inc, nt = 1.0 - dt;
        double dbx = nt*nt*x0 + 2.0*nt*dt*x1 + dt*dt*x2 + 0.5;
        double dby = nt*nt*y0 + 2.0*nt*dt*y1 + dt*dt*y2 + 0.5;
        x = (int)dbx, y = (int)dby;

        int dx = abs(x - lx), dy = abs(y - ly);

        if(dx > 1 || dy > 1) {
            /* the new point is too far from the last point, so
            decrease `inc` and try again. */
            inc *= 0.75;
        } else if(dx == 0 && dy == 0) {
            /* The new pixel is on top of the last pixel, so
            increase `inc` and try again. */
            inc *= 1.5;
        } else {
            if(x >= b->clip.x0 && x < b->clip.x1 && y >= b->clip.y0 && y < b->clip.y1)
                BM_SET(b, x, y, b->color);

            t += inc;

            /* This has the effect of smoothing out the curve: */
            inc *= 1.05;

            /* Remember the last pixel drawn */
            lx = x;
            ly = y;
        }
    } while(t < 1.0);
    bm_putpixel(b, x2, y2);
}

void bm_bezier4(Bitmap *b, int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3) {
    /* Cubic Bezier curve */
    int x, y, lx = x0, ly = y0;

    assert(b);

    bm_putpixel(b, x0, y0);

    int steps = abs(x1 - x0) + abs(y1 - y0) + abs(x2 - x1) + abs(y2 - y1) + abs(x3 - x2) + abs(y3 - y2);
    if(steps == 0)
        return;
    double t = 0, inc = 1.0/steps;
    do {
        double dt = t + inc, nt = 1.0 - dt;
        double dbx = nt*nt*nt*x0 + 3.0*nt*nt*dt*x1 + 3*nt*dt*dt*x2 + dt*dt*dt*x3 + 0.5;
        double dby = nt*nt*nt*y0 + 3.0*nt*nt*dt*y1 + 3*nt*dt*dt*y2 + dt*dt*dt*y3 + 0.5;

        x = (int)dbx, y = (int)dby;

        int dx = abs(x - lx), dy = abs(y - ly);

        if(dx > 1 || dy > 1) {
            inc *= 0.75;
        } else if(dx == 0 && dy == 0) {
            inc *= 1.5;
        } else {
            if(x >= b->clip.x0 && x < b->clip.x1 && y >= b->clip.y0 && y < b->clip.y1)
                BM_SET(b, x, y, b->color);
            t += inc;
            lx = x;
            ly = y;
        }
    } while(t < 1.0);
    bm_putpixel(b, x3, y3);
}

void bm_poly(Bitmap *b, BmPoint points[], unsigned int n) {
    unsigned int i;
    assert(b);
    if(n < 2) return;
    for(i = 0; i < n - 1; i++) {
        bm_line(b, points[i].x, points[i].y, points[i+1].x, points[i+1].y);
    }
    bm_line(b, points[0].x, points[0].y, points[i].x, points[i].y);
}

#define MAX_POLY_CORNERS 32

void bm_fillpoly(Bitmap *b, BmPoint points[], unsigned int n) {
    /* http://alienryderflex.com/polygon_fill/
    https://hackernoon.com/computer-graphics-scan-line-polygon-fill-algorithm-3cb47283df6

    You might also be interested in this article:
    http://nothings.org/gamedev/rasterize/
    */
    unsigned int i, j, c;
    int x, y;
    assert(b);
    c = bm_get_color(b);
    if(n < 2)
        return;
    else if(n == 2) {
        bm_line(b, points[0].x, points[0].y, points[1].x, points[1].y);
        return;
    }

    int nodeX_static[MAX_POLY_CORNERS], *nodeX = nodeX_static;
    unsigned int nodes;

    if(n > MAX_POLY_CORNERS) {
        nodeX = CAST(int*)(calloc(n, sizeof *nodeX));
        if(!nodeX) return;
    }

    BmRect area = {b->w, b->h, 0, 0};
    for(i = 0; i < n; i++) {
        x = points[i].x;
        y = points[i].y;
        if(x < area.x0) area.x0 = x;
        if(y < area.y0) area.y0 = y;
        if(x > area.x1) area.x1 = x;
        if(y > area.y1) area.y1 = y;
    }
    if(area.x0 < b->clip.x0) area.x0 = b->clip.x0;
    if(area.y0 < b->clip.y0) area.y0 = b->clip.y0;
    if(area.x1 >= b->clip.x1) area.x1 = b->clip.x1 - 1;
    if(area.y1 >= b->clip.y1) area.y1 = b->clip.y1 - 1;

    for(y = area.y0; y <= area.y1; y++) {
        nodes = 0;
        j = n - 1;

        for(i = 0; i < n; i++) {
            if((points[i].y < y && points[j].y >= y)
                || (points[j].y < y && points[i].y >= y)) {
                nodeX[nodes++] = (int)((double)points[i].x + ((double)y - points[i].y) * ((double)points[j].x - points[i].x) / ((double)points[j].y - points[i].y));
            }
            j = i;
        }

        assert(nodes < n);
        if(nodes < 1) continue;

        i = 0;
        while(i < nodes - 1) {
            if(nodeX[i] > nodeX[i+1]) {
                SWAPINT(nodeX[i], nodeX[i + 1]);
                if(i) i--;
            } else {
                i++;
            }
        }

        for(i = 0; i < nodes; i += 2) {
            if(nodeX[i] >= area.x1)
                break;
            if(nodeX[i + 1] > area.x0) {
                if(nodeX[i] < area.x0)
                    nodeX[i] = area.x0;
                if(nodeX[i+1] > area.x1)
                    nodeX[i+1] = area.x1;

                for(x = nodeX[i]; x <= nodeX[i+1]; x++)
                    BM_SET(b, x, y, c);
            }
        }
    }

    if(nodeX != nodeX_static)
        free(nodeX);
}

void bm_fill(Bitmap *b, int x, int y) {
    BmPoint *queue, n;
    int qs = 0, /* queue size */
        mqs = 128; /* Max queue size */
    unsigned int sc, dc; /* Source and Destination colors */

    assert(b);

    dc = b->color;
    b->color = BM_GET(b, x, y);
    sc = b->color;

    /* Don't fill if source == dest
     * It leads to major performance problems otherwise
     */
    if(sc == dc)
        return;

    queue = CAST(BmPoint*)(calloc(mqs, sizeof *queue));
    if(!queue)
        return;

    n.x = x; n.y = y;
    queue[qs++] = n;

    while(qs > 0) {
        BmPoint w,e, nn;
        int i;

        n = queue[--qs];
        w = n;
        e = n;

        if(BM_GET(b, n.x, n.y) != sc)
            continue;

        while(w.x > b->clip.x0) {
            if(BM_GET(b, w.x-1, w.y) != sc) {
                break;
            }
            w.x--;
        }
        while(e.x < b->clip.x1 - 1) {
            if(BM_GET(b, e.x+1, e.y) != sc) {
                break;
            }
            e.x++;
        }
        for(i = w.x; i <= e.x; i++) {
            assert(i >= 0 && i < b->w);
            BM_SET(b, i, w.y, dc);
            if(w.y > b->clip.y0) {
                if(BM_GET(b, i, w.y - 1) == sc) {
                    nn.x = i; nn.y = w.y - 1;
                    queue[qs++] = nn;
                    if(qs == mqs) {
                        mqs <<= 1;
                        BmPoint *tmp = CAST(BmPoint*)(realloc(queue, mqs * sizeof *queue));
                        if (!tmp) {
                            free(queue);
                            return;
                        }
                        queue = tmp;
                    }
                }
            }
            if(w.y < b->clip.y1 - 1) {
                if(BM_GET(b, i, w.y + 1) == sc) {
                    nn.x = i; nn.y = w.y + 1;
                    queue[qs++] = nn;
                    if(qs == mqs) {
                        mqs <<= 1;
                        BmPoint *tmp = CAST(BmPoint*)(realloc(queue, mqs * sizeof *queue));
                        if (!tmp) {
                            free(queue);
                            return;
                        }
                        queue = tmp;
                    }
                }
            }
        }
    }
    free(queue);
    b->color = dc;
}

static uint32_t count_trailing_zeroes(uint32_t v) {
    /* Count the consecutive zero bits (trailing) on the right in parallel
       https://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightParallel */
    uint32_t c = 32;
    v &= -(int32_t)(v);
    if (v) c--;
    if (v & 0x0000FFFF) c -= 16;
    if (v & 0x00FF00FF) c -= 8;
    if (v & 0x0F0F0F0F) c -= 4;
    if (v & 0x33333333) c -= 2;
    if (v & 0x55555555) c -= 1;
    return c;
}

static void fs_add_factor(Bitmap *b, int x, int y, int er, int eg, int eb, int f) {
    int c, R, G, B;
    if(x < 0 || x >= b->w || y < 0 || y >= b->h)
        return;
    c = BM_GET(b, x, y);

    R = ((c >> 16) & 0xFF) + ((f * er) >> 4);
    G = ((c >> 8) & 0xFF) + ((f * eg) >> 4);
    B = ((c >> 0) & 0xFF) + ((f * eb) >> 4);

    if(R > 255) R = 255;
    if(R < 0) R = 0;
    if(G > 255) G = 255;
    if(G < 0) G = 0;
    if(B > 255) B = 255;
    if(B < 0) B = 0;

    BM_SET_RGBA(b, x, y, R, G, B, 0);
}

void bm_reduce_palette(Bitmap *b, BmPalette *pal) {
    /* Floyd-Steinberg (error-diffusion) dithering
        http://en.wikipedia.org/wiki/Floyd%E2%80%93Steinberg_dithering */
    int x, y;
    if(!b)
        return;
    assert(pal);
    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            unsigned int r1, g1, b1;
            unsigned int r2, g2, b2;
            unsigned int er, eg, eb;
            unsigned int newpixel, oldpixel = BM_GET(b, x, y);

            newpixel = bm_palette_nearest_color(pal, oldpixel);

            bm_set(b, x, y, newpixel);

            r1 = (oldpixel >> 16) & 0xFF; g1 = (oldpixel >> 8) & 0xFF; b1 = (oldpixel >> 0) & 0xFF;
            r2 = (newpixel >> 16) & 0xFF; g2 = (newpixel >> 8) & 0xFF; b2 = (newpixel >> 0) & 0xFF;
            er = r1 - r2; eg = g1 - g2; eb = b1 - b2;

            fs_add_factor(b, x + 1, y    , er, eg, eb, 7);
            fs_add_factor(b, x - 1, y + 1, er, eg, eb, 3);
            fs_add_factor(b, x    , y + 1, er, eg, eb, 5);
            fs_add_factor(b, x + 1, y + 1, er, eg, eb, 1);
        }
    }
}

static void atk_add_factor(Bitmap *b, int x, int y, int er, int eg, int eb) {
    int c, R, G, B;
    if(x < 0 || x >= b->w || y < 0 || y >= b->h)
        return;
    c = BM_GET(b, x, y);

    R = ((c >> 16) & 0xFF) + (er >> 3);
    G = ((c >> 8) & 0xFF) + (eg >> 3);
    B = ((c >> 0) & 0xFF) + (eb >> 3);

    if(R > 255) R = 255;
    if(R < 0) R = 0;
    if(G > 255) G = 255;
    if(G < 0) G = 0;
    if(B > 255) B = 255;
    if(B < 0) B = 0;

    BM_SET_RGBA(b, x, y, R, G, B, 0);
}

void bm_reduce_palette_atk(Bitmap *b, BmPalette *pal) {
    /* Atkinson Dithering:
     * https://beyondloom.com/blog/dither.html
     * https://en.wikipedia.org/wiki/Atkinson_dithering
     *
     * "Atkinson's algorithm seems to produce richer contrast, at the cost of
     * some detail in very light or dark areas of the image." - beyondloom
     */
    int x, y;
    if(!b)
        return;
    assert(pal);
    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            unsigned int r1, g1, b1;
            unsigned int r2, g2, b2;
            unsigned int er, eg, eb;
            unsigned int newpixel, oldpixel = BM_GET(b, x, y);

            newpixel = bm_palette_nearest_color(pal, oldpixel);

            bm_set(b, x, y, newpixel);

            r1 = (oldpixel >> 16) & 0xFF; g1 = (oldpixel >> 8) & 0xFF; b1 = (oldpixel >> 0) & 0xFF;
            r2 = (newpixel >> 16) & 0xFF; g2 = (newpixel >> 8) & 0xFF; b2 = (newpixel >> 0) & 0xFF;
            er = r1 - r2; eg = g1 - g2; eb = b1 - b2;

            atk_add_factor(b, x + 1, y    , er, eg, eb);
            atk_add_factor(b, x + 2, y    , er, eg, eb);
            atk_add_factor(b, x - 1, y + 1, er, eg, eb);
            atk_add_factor(b, x    , y + 1, er, eg, eb);
            atk_add_factor(b, x + 1, y + 1, er, eg, eb);
            atk_add_factor(b, x    , y + 2, er, eg, eb);
        }
    }
}

static int bayer4x4[16] = { /* (1/17) */
    1,  9,  3, 11,
    13, 5, 15,  7,
    4, 12,  2, 10,
    16, 8, 14,  6
};
static int bayer8x8[64] = { /*(1/65)*/
    1,  49, 13, 61,  4, 52, 16, 64,
    33, 17, 45, 29, 36, 20, 48, 32,
    9,  57,  5, 53, 12, 60,  8, 56,
    41, 25, 37, 21, 44, 28, 40, 24,
    3,  51, 15, 63,  2, 50, 14, 62,
    35, 19, 47, 31, 34, 18, 46, 30,
    11, 59,  7, 55, 10, 58,  6, 54,
    43, 27, 39, 23, 42, 26, 38, 22,
};

/*
TODO: (maybe) - here's a guy with a 16x16 matrix
https://stackoverflow.com/a/68192472/115589
*/

static void reduce_palette_bayer(Bitmap *b, BmPalette *pal, int bayer[], int dim, unsigned int fac) {
    /* Ordered dithering: https://en.wikipedia.org/wiki/Ordered_dithering
    The resulting image may be of lower quality than you would get with
    Floyd-Steinberg, but it does have some advantages:
        * the repeating patterns compress better
        * it is better suited for line-art graphics
        * if you were to make a limited palette animation (e.g. animated GIF)
            subsequent frames would be less jittery than error-diffusion.
    */
    int x, y;
    int af = dim - 1; /* mod factor */
    if(!b)
        return;
    for(y = 0; y < b->h; y++) {
        for(x = 0; x < b->w; x++) {
            int R, G, B;
            unsigned int newpixel, oldpixel = BM_GET(b, x, y);

            R = (oldpixel >> 16) & 0xFF; G = (oldpixel >> 8) & 0xFF; B = (oldpixel >> 0) & 0xFF;

            unsigned int f = bayer[(y & af) * dim + (x & af)];

            R += R * f / fac - fac/2;
            if(R > 255)
                R = 255;
            if(R < 0)
                R = 0;
            G += G * f / fac - fac/2;
            if(G > 255)
                G = 255;
            if(G < 0)
                G = 0;
            B += B * f / fac - fac/2;
            if(B > 255)
                B = 255;
            if(B < 0)
                B = 0;
            oldpixel = (R << 16) | (G << 8) | B;
            newpixel = bm_palette_nearest_color(pal, oldpixel);
            BM_SET(b, x, y, newpixel);
        }
    }
}

void bm_reduce_palette_OD4(Bitmap *b, BmPalette *pal) {
    assert(pal);
    reduce_palette_bayer(b, pal, bayer4x4, 4, 17);
}

void bm_reduce_palette_OD8(Bitmap *b, BmPalette *pal) {
    assert(pal);
    reduce_palette_bayer(b, pal, bayer8x8, 8, 65);
}

void bm_reduce_palette_nearest(Bitmap *b, BmPalette *pal) {
    int i, k;
    int np = bm_pixel_count(b), nc = bm_palette_count(pal);
    unsigned int *bytes = (unsigned int *)bm_raw_data(b);
    for(i = 0; i < np; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(bytes[i], &iR, &iG, &iB);

#if RGB_BETTER_COMPARE
        double minD = INT_MAX;
#else
        int minD = INT_MAX;
#endif
        int dk = 0;
        for(k = 0; k < nc; k++) {
            unsigned char pR, pG, pB;
            bm_get_rgb(bm_palette_get(pal, k), &pR, &pG, &pB);
            int dR = (int)iR - (int)pR;
            int dG = (int)iG - (int)pG;
            int dB = (int)iB - (int)pB;
#if RGB_BETTER_COMPARE
            int rmean = ((int)iR + (int)pR) / 2;
            double d = sqrt( (((512 + rmean)*dR*dR)>>8) + 4*dG*dG + (((767-rmean)*dB*dB)>>8) );
#else
            int d = dR*dR + dG*dG + dB*dB;
#endif
            if(d < minD) {
                minD = d;
                dk = k;
            }
        }
        bytes[i] = bm_palette_get(pal, dk);
    }
}

BmPalette *bm_palette_create(unsigned int ncolors) {
    BmPalette *pal;
    pal = malloc(sizeof *pal);
    if(!pal) {
        SET_ERROR("out of memory creating palette");
        return NULL;
    }
    pal->ncolors = (int)ncolors;
    pal->acolors = 32;
    while(pal->acolors < (int)ncolors)
        pal->acolors <<= 1;
    pal->colors = calloc(pal->acolors, sizeof *pal->colors);
    if(!pal->colors) {
        SET_ERROR("out of memory creating palette");
        free(pal);
        return NULL;
    }

    pal->ref_count = 1;
    return pal;
}

BmPalette *bm_palette_retain(BmPalette *pal) {
    assert(pal);
    pal->ref_count++;
    return pal;
}

unsigned int bm_palette_release(BmPalette *pal) {
    assert(pal);
    assert(pal->ref_count > 0);
    pal->ref_count--;
    if(!pal->ref_count) {
        free(pal->colors);
        free(pal);
        return 0;
    }
    return pal->ref_count;
}

int bm_palette_count(BmPalette *pal) {
    assert(pal);
    return pal->ncolors;
}

int bm_palette_add(BmPalette *pal, unsigned int color) {
    int index = pal->ncolors++;
    color &= 0xFFFFFF;
    if(pal->ncolors >= pal->acolors) {
        pal->acolors <<= 1;
        unsigned int *tcolors = realloc(pal->colors, pal->acolors * sizeof *pal->colors);
        if(!tcolors)
            return -1;
        pal->colors = tcolors;
    }
    pal->colors[index] = color;
    return index;
}

int bm_palette_set(BmPalette *pal, int index, unsigned int color) {
    assert(pal);
    if(index < 0 || index >= pal->ncolors)
        return -1;
    pal->colors[index] = color;
    return index;
}

unsigned int bm_palette_get(BmPalette *pal, int index) {
    assert(pal);
    if(index < 0 || index >= pal->ncolors)
        return 0;
    return pal->colors[index];
}

/* At some point I experimented with whether a kd-tree would
improve the performance. It didn't.
See commit cae63c0a1fad72a76c1e913c8ee26e1293888de0 */
unsigned int bm_palette_nearest_index(BmPalette *pal, unsigned int color) {
    int i, m = 0;
#if RGB_BETTER_COMPARE
    unsigned char R1, G1, B1, R2, G2, B2;
    int rmean, r, g, b;
    double d, md = 1e10;
    bm_get_rgb(color, &R1, &G1, &B1);
    for(i = 0; i < pal->ncolors; i++) {
        bm_get_rgb(pal->colors[i], &R2, &G2, &B2);

        rmean = (R1 + R2) / 2;
        r = R1 - R2;
        g = G1 - G2;
        b = B1 - B2;
        d = sqrt( (((512 + rmean)*r*r)>>8) + 4*g*g + (((767-rmean)*b*b)>>8) );

        if(d < md) {
            md = d;
            m = i;
        }
    }
#else
    /* This path just takes the euclidean distance */
    unsigned int dr, dg, db;
    unsigned int r0 = (color >> 16) & 0xFF,
        g0 = (color >> 8) & 0xFF,
        b0 = (color >> 0) & 0xFF;

    int md = INT_MAX;
    for(i = 0; i < pal->ncolors; i++) {
        color = pal->colors[i];
        dr = r0 - ((color >> 16) & 0xFF);
        dg = g0 - ((color >> 8) & 0xFF);
        db = b0 - ((color >> 0) & 0xFF);
        int d = dr * dr + dg * dg + db * db;

        /* another thing that didn't improve performance as I expected:

        if(!d) return i;

        It seems that for colorful images the color is unlikely
        to be in the palette, so it has to go through the whole palette
        anyway, but then add this check on each iteration. */
        if(d < md) {
            md = d;
            m = i;
        }
    }
#endif
    return m;
}

unsigned int bm_palette_nearest_color(BmPalette *pal, unsigned int color) {
    unsigned int index = bm_palette_nearest_index(pal, color);
    return pal->colors[index];
}

static int cnt_comp_noalpha(const void*ap, const void*bp) {
    int a = *(int*)ap, b = *(int*)bp;
    return (a & 0x00FFFFFF) - (b & 0x00FFFFFF);
}

/* Counts the colors in the image and builds an 8-bit palette while it is counting.
 * It returns -1 in case there are more than 256 colors in the palette, meaning the
 * image will have to be quantized first.
 * It also ignores the alpha values of the pixels.
 */
static int count_colors_build_palette(Bitmap *b, unsigned int colors[256]) {
    int count = 1, i;
    int npx = b->w * b->h;
    int *sort = CAST(int *)(ALLOCA(npx * sizeof *sort));
    if (!sort) return 0;
    memcpy(sort, b->data, npx * sizeof *sort);
    qsort(sort, npx, sizeof(int), cnt_comp_noalpha);
    colors[0] = sort[0] & 0x00FFFFFF;
    for(i = 1; i < npx; i++){
        unsigned int c = sort[i] & 0x00FFFFFF;
        if(c != (sort[i-1] & 0x00FFFFFF)) {
            if(count == 256)
                return -1;
            colors[count] = c;
            count++;
        }
    }
    FREEA(sort);
    return count;
}

int bm_make_palette(Bitmap *b) {
    BmPalette *palette;
    unsigned int colors[256];
    int ncolors = count_colors_build_palette(b, colors);

    if(ncolors > 0) {
        palette = bm_palette_create(ncolors);
        if(!palette)
            return 0;
        memcpy(palette->colors, colors, ncolors * sizeof colors[0]);
    } else {
        /* More than 256 colors in the image */
        palette = bm_quantize_uniform(b, 256);
        ncolors = palette->ncolors;
    }

    bm_set_palette(b, palette);
    bm_palette_release(palette);
    return ncolors;
}

static char *read_pal_line(FILE *f, char *buffer, size_t buf_len) {
    char *c = NULL;
    do {
        if(!fgets(buffer, buf_len, f))
            return NULL;
        c = buffer;
        while (*c && isspace(*c)) c++;
    } while(*c == '\0');
    return c;
}

int tokenize_pal_line(char **cp, unsigned int *component) {
    char *c = *cp, *s;
    while(isspace(*c)) c++;
    s = c;
    while(isdigit(*c)) c++;
    if(!*c) {
        SET_ERROR("bad value in palette");
        return 0;
    }
    if(*c != '\0') {
        *c = '\0';
        c++;
    }
    *cp = c;
    *component = atoi(s);
    return 1;
}

static int read_pal_rgb(char *line, unsigned int *r, unsigned int *g, unsigned int *b) {
    char *c = line;
    if(!tokenize_pal_line(&c, r)) return 0;
    if(!tokenize_pal_line(&c, g)) return 0;
    if(!tokenize_pal_line(&c, b)) return 0;
    return 1;
}

BmPalette *bm_load_palette(const char* filename) {
    FILE* f = NULL;
    char buf[64];
    BmPalette *palette = NULL;
    int n = 0;
    char *s, *e, *c;
    unsigned int r, g, b;

    if (!filename) return NULL;

#ifdef SAFE_C11
    errno_t err = fopen_s(&f, filename, "r");
    if (err != 0) return NULL;
#else
    f = fopen(filename, "r");
    if (!f) return NULL;
#endif

    if(!fgets(buf, sizeof buf, f)) {
        SET_ERROR("couldn't read palette first line");
        goto error;
    }

    if (!strncmp(buf, "JASC-PAL", 8)) {
        /* Paintshop Pro palette http://www.cryer.co.uk/file-types/p/pal.htm */
        int version;
        int count;
#ifdef SAFE_C11
        if (fscanf_s(f, "%d %u", &version, &count) != 2)
            goto error;
#else
        if (fscanf(f, "%d %u", &version, &count) != 2)
            goto error;
#endif
        (void)version;

        palette = bm_palette_create(count);
        if(!palette)
            goto error;

        for (n = 0; n < count; n++) {
            c = read_pal_line(f, buf, sizeof buf);
            if(!read_pal_rgb(c, &r, &g, &b))
                goto error;
            bm_palette_set(palette, n, (r << 16) | (g << 8) | b);
        }
        fclose(f);
        return palette;
    } else if (!strncmp(buf, "GIMP Palette", 12)) {
        /* TODO: Support GIMP Palettes...
        It seems there are two versions. Both has "GIMP Palette" on the first line.

        The newer version of the format has two lines that start with "Name: " and
        "Columns: " repsectively. If these lines are omitted, then it is the older format.

        The `Name` and `Columns` aren't relevant for my purposes - they're used to
        present the palette within GIMP itself.

        Either way, the file then contains lines with n colors. The line has three decimal
        values for the R, G and B channels respectively. Optionally the channels are followed
        by a name for the color.
        (The values are officially separated by tabs, but I might as well support any
        whitespace between the values)

        example: 255 0 0 red

        Lines that start with `#` are comments, and are ignored. Empty lines are also ignored.

        + The source: https://gitlab.gnome.org/GNOME/gimp/-/blob/gimp-2-10/app/core/gimppalette-load.c#L39
        + https://stackoverflow.com/a/60920172/115589

        */

        do {
            c = read_pal_line(f, buf, sizeof buf);
            if(!c) {
                SET_ERROR("bad GIMP palette");
                goto error;
            }
        } while(*c == '#' || !strncmp("Name: ", c, 6) || !strncmp("Columns: ", c, 9));

        palette = bm_palette_create(0);
        if(!palette)
            goto error;

        do {
            if(!read_pal_rgb(c, &r, &g, &b))
                goto error;
            bm_palette_add(palette, (r << 16) | (g << 8) | b);
            c = read_pal_line(f, buf, sizeof buf);
        } while(c);

        fclose(f);
        return palette;
    } else if(!strncmp(buf, "RIFF", 4)) {
        /* TODO: Here's a spec for the Microsoft PAL format
        https://worms2d.info/Palette_file (of all places!)
        See also https://www.codeproject.com/Articles/1172812/Loading-Microsoft-RIFF-Palette-pal-Files-with-Csha
        */
        rewind(f);
        SET_ERROR("RIFF palettes are not supported");
        goto error;
    } else {
        rewind(f);

        palette = bm_palette_create(0);
        if(!palette)
            goto error;

        while ((c = read_pal_line(f, buf, sizeof buf)) && n < 256) {
            s = c;
            if (!*s) continue;
            while (*c) {
                if (*c == ';') {
                    *c = '\0';
                    break;
                }
                c++;
            }
            e = c - 1;
            while (e > s && isspace(*e)) {
                *e = '\0';
                e--;
            }
            if (e <= s) continue;

            if(bm_palette_add(palette, bm_atoi(s)) < 0)
                goto error;
            n++;
        }
        if (n == 0) {
            SET_ERROR("no colors in palette");
            goto error;
        }

        fclose(f);
        return palette;
    }
error:
    fclose(f);
    if (palette)
        bm_palette_release(palette);
    return NULL;
}

int bm_save_palette(BmPalette *pal, const char* filename) {
    int i;
    FILE* f;

    if (!pal || !filename)
        return 0;

#ifdef SAFE_C11
    errno_t err = fopen_s(&f, filename, "rb");
    if (err != 0)
        return 0;
#else
    f = fopen(filename, "w");
    if (!f)
        return 0;
#endif
    fputs("JASC-PAL\n", f);
    fputs("0100\n", f);
    fprintf(f, "%u\n", pal->ncolors);
    for (i = 0; i < pal->ncolors; i++) {
        unsigned char R, G, B;
        bm_get_rgb(bm_palette_get(pal, i), &R, &G, &B);
        fprintf(f, "%u %u %u\n", R, G, B);
    }
    fclose(f);
    return 1;
}

static int q_sortByR(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x00FF0000) - (*(unsigned int *)qp & 0x00FF0000);
}
static int q_sortByG(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x0000FF00) - (*(unsigned int *)qp & 0x0000FF00);
}
static int q_sortByB(const void *pp, const void *qp) {
    return (*(unsigned int *)pp & 0x000000FF) - (*(unsigned int *)qp & 0x000000FF);
}

static void qrecurse(unsigned int *pixels, int start, int end, int n, unsigned int *pal, int *pindex) {

    int len = end - start, i;

    if(n == 1) {
        unsigned int aR = 0, aG = 0, aB = 0;
        for(i = start; i < end; i++) {
            unsigned char iR, iG, iB;
            bm_get_rgb(pixels[i], &iR, &iG, &iB);
            aR += iR;
            aG += iG;
            aB += iB;
        }
        aR /= len;
        aG /= len;
        aB /= len;

        pal[(*pindex)++] = bm_rgb(aR, aG, aB);
        return;
    }

    int minR = 256, minG = 256, minB = 256;
    int maxR = 0, maxG = 0, maxB = 0;
    for(i = start; i < end; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(pixels[i], &iR, &iG, &iB);
        if(iR < minR) minR = iR;
        if(iR > maxR) maxR = iR;
        if(iG < minG) minG = iG;
        if(iG > maxG) maxG = iG;
        if(iB < minB) minB = iB;
        if(iB > maxB) maxB = iB;
    }
    int spreadR = maxR - minR;
    int spreadG = maxG - minG;
    int spreadB = maxB - minB;

    if(spreadR > spreadG) {
        if(spreadR > spreadB)
            qsort(pixels + start, len, sizeof *pixels, q_sortByR);
        else
            qsort(pixels + start, len, sizeof *pixels, q_sortByB);
    } else {
        if(spreadG > spreadB)
            qsort(pixels + start, len, sizeof *pixels, q_sortByG);
        else
            qsort(pixels + start, len, sizeof *pixels, q_sortByB);
    }

    int mid = (start + end)/2;
    qrecurse(pixels, start, mid, n >> 1, pal, pindex);
    qrecurse(pixels, mid, end, n >> 1, pal, pindex);
}

BmPalette *bm_quantize(Bitmap *b, int n) {
    BmPalette *palette;
    int w = bm_width(b), h = bm_height(b);

    /* n must be a power of 2, up to 256 */
    assert(n > 1 && n <= 256);
    assert((n != 0) && ((n & (n - 1)) == 0)); /* https://stackoverflow.com/a/600306/115589 */

    unsigned int *data = malloc(w * h * sizeof *data);
    if(!data)
        return 0;

    memcpy(data, bm_raw_data(b), w * h * sizeof *data);

    palette = bm_palette_create(n);
    if(!palette)
        return NULL;

    int pindex = 0;
    qrecurse(data, 0, w*h, n, palette->colors, &pindex);

    free(data);
    return palette;
}

#define MAX_K           256
#define MAX_ITERATIONS  128

struct kmeans_bucket {
    unsigned int color;
    unsigned int count;
};

static int kmeans_bucket_cmp(const void*ap, const void*bp) {
    const struct kmeans_bucket *a = ap, *b = bp;
    return (b->count) - (a->count);
}

static int kmeans_categorize_pixels(unsigned int *bytes, int np, int *cat, struct kmeans_bucket buckets[MAX_K], int K) {
    int i, k, change = 0;
    for(i = 0; i < np; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(bytes[i], &iR, &iG, &iB);
        int minD = INT_MAX, dk = 0;
        for(k = 0; k < K; k++) {
            unsigned char pR, pG, pB;
            bm_get_rgb(buckets[k].color, &pR, &pG, &pB);
            int dR = (int)iR - (int)pR;
            int dG = (int)iG - (int)pG;
            int dB = (int)iB - (int)pB;
            int d = dR*dR + dG*dG + dB*dB;
            if(d < minD) {
                minD = d;
                dk = k;
            }
        }
        if(cat[i] != dk)
            change++;
        cat[i] = dk;
    }
    return change;
}

static void kmeans_adjust(unsigned int *bytes, int np, int *cat, struct kmeans_bucket buckets[MAX_K], int K) {
    int i, k;

    unsigned int sR[MAX_K], sG[MAX_K], sB[MAX_K];
    for(k = 0; k < K; k++) {
        buckets[k].count = 0;
        sR[k] = 0; sG[k] = 0; sB[k] = 0;
    }
    for(i = 0; i < np; i++) {
        unsigned char iR, iG, iB;
        bm_get_rgb(bytes[i], &iR, &iG, &iB);
        k = cat[i];
        buckets[k].count++;
        sR[k] += iR; sG[k] += iG; sB[k] += iB;
    }
    for(k = 0; k < K; k++) {
        if(buckets[k].count == 0) continue;
        sR[k] /= buckets[k].count;
        sG[k] /= buckets[k].count;
        sB[k] /= buckets[k].count;
        buckets[k].color = bm_rgb(sR[k], sG[k], sB[k]);
    }
}

BmPalette *bm_quantize_kmeans(Bitmap *b, int K) {
    int np = bm_pixel_count(b);
    assert(K > 1 && K <= MAX_K);

    /* I'd really like to get rid of this, but how will you track whether
    there were changes to the categories? */
    int *cat = calloc(np, sizeof *cat);

    struct kmeans_bucket buckets[MAX_K];
    unsigned int *bytes = (unsigned int *)bm_raw_data(b);
    int i, k;

    unsigned int *pixels = malloc(np * sizeof *pixels);
    if(!pixels) {
        free(cat);
        return NULL;
    }

    memcpy(pixels, bytes, np * sizeof *pixels);
    qsort(pixels, np, sizeof *pixels, cnt_comp_noalpha);
    for(k = 0; k < K; k++) {
        int x = k * (np - 1) / (K - 1);
        buckets[k].color = pixels[x];
    }

    for(i = 0; i < MAX_ITERATIONS; i++) {
        int changes = kmeans_categorize_pixels(bytes, np, cat, buckets, K);
        if(!changes)
            break;
        kmeans_adjust(bytes, np, cat, buckets, K);
    }

    /* for(i = 0; i < K; i++) printf("#%06X %u\n", buckets[i].color, buckets[i].count); */

    qsort(buckets, K, sizeof buckets[0], kmeans_bucket_cmp);
    while(K > 0 && buckets[K-1].count == 0)
        K--;

    BmPalette *palette = bm_palette_create(K);
    if(!palette)
        return NULL;
    for(i = 0; i < K; i++) {
        bm_palette_set(palette, i, buckets[i].color);
    }

    free(cat);
    free(pixels);
    return palette;
}

BmPalette *bm_quantize_uniform(Bitmap *b, int K) {
    int np = bm_pixel_count(b), i;
    assert(K > 1 && K <= MAX_K);

    unsigned int *pixels = malloc(np * sizeof *pixels);
    if(!pixels)
        return NULL;

    memcpy(pixels, bm_raw_data(b), np * sizeof *pixels);
    qsort(pixels, np, sizeof *pixels, cnt_comp_noalpha);

    BmPalette *palette = bm_palette_create(K);
    if(!palette) {
        free(pixels);
        return NULL;
    }

    for(i = 0; i < K; i++) {
        int x = i * (np - 1) / (K - 1);
        palette->colors[i] = pixels[x];
    }

    free(pixels);
    return palette;
}

BmPalette *bm_quantize_random(Bitmap *b, int K) {
    int np = bm_pixel_count(b), i;
    assert(K > 1 && K <= MAX_K);

    unsigned int *bytes = (unsigned int *)bm_raw_data(b);

    BmPalette *palette = bm_palette_create(K);
    if(!palette)
        return NULL;

    for(i = 0; i < K; i++) {
        palette->colors[i] = bytes[rand() % np];
    }

    return palette;
}

void bm_set_palette(Bitmap *b, BmPalette *pal) {
    assert(b);
    if(pal)
        bm_palette_retain(pal);
    if(b->palette)
        bm_palette_release(b->palette);
    b->palette = pal;
}

BmPalette *bm_get_palette(const Bitmap *b) {
    assert(b);
    return b->palette;
}

int bm_stricmp(const char *p, const char *q) {
    for(;*p && tolower(*p) == tolower(*q); p++, q++);
    return tolower(*p) - tolower(*q);
}

void bm_set_font(Bitmap *b, BmFont *font) {
    assert(b);
    BmFont *old = b->font;
    b->font = bm_font_retain(font);
    if(old)
        bm_font_release(old);
}

BmFont *bm_get_font(Bitmap *b) {
    assert(b);
    return b->font;
}

const char *bm_utf8_next_codepoint(const char *in, unsigned int *codepoint) {
    if (in == NULL)
        return NULL;

    if (*in == 0x00)
        return NULL;

    unsigned int cp = 0;
    if((*in & 0xE0) == 0xC0) { /* two-byte */
        cp += (*in++ & 0x1F) << 6;
        if (*in != 0) cp += (*in++ & 0x3F);
    } else if((*in & 0xF0) == 0xE0) { /* three-byte */
      cp += (*in++ & 0x0F) << 12;
      if (*in != 0) cp += (*in++ & 0x3F) << 6;
      if (*in != 0) cp += (*in++ & 0x3F);
    } else if((*in & 0xF8) == 0xF0) { /* four-byte */
        cp += (*in++ & 0x07) << 18;
        if (*in != 0) cp += (*in++ & 0x3F) << 12;
        if (*in != 0) cp += (*in++ & 0x3F) << 6;
        if (*in != 0) cp += (*in++ & 0x3F);
    } else {
        cp = *in++;
    }

    if(codepoint)
        *codepoint = cp;
    return in;
}

int bm_text_width(Bitmap *b, const char *s) {
    int max_width = 0;
    int width = 0;

    assert(b);
    if(!b->font || !b->font->width)
        return 0;

    const char *ptr = s;
    unsigned int codepoint = 0;
    while((ptr = bm_utf8_next_codepoint(ptr, &codepoint))) {
        if(codepoint == '\n') {
            if(width > max_width)
                max_width = width;
            width = 0;
        } else if(codepoint == '\t') {
            width += b->font->width(b->font, ' ') * 4;
        } else {
            width += b->font->width(b->font, codepoint);
        }
    }

    if(width > max_width)
        max_width = width;
    return max_width;
}

int bm_text_height(Bitmap *b, const char *s) {
    int max_height = 0;
    int height = 0;
    int lines = 1;

    assert(b);
    if(!b->font || !b->font->height)
        return 0;

    const char *ptr = s;
    unsigned int codepoint = 0;
    while((ptr = bm_utf8_next_codepoint(ptr, &codepoint))) {
        if(codepoint == '\n') {
            lines++;
        } else {
            height = b->font->height(b->font, codepoint);
            if(height > max_height)
                max_height = height;
        }
    }

    return lines * max_height;
}

void bm_text_measure(Bitmap *b, const char *s, int *w, int* h, int* dx, int* dy) {
    assert(b);

    if(!b->font)
        return;

    if(b->font->measure) {
        b->font->measure(b->font, s, w, h, dx, dy);
    } else {
        *dx = 0;
        *dy = 0; /* assuming this fonts bm_puts() starts top/left */
        *w = bm_text_width(b, s);
        *h = bm_text_height(b, s);
    }
}

int bm_putc(Bitmap *b, int x, int y, char c) {
    char text[2] = {c, 0};
    assert(b);
    return bm_puts(b, x, y, text);
}

int bm_puts(Bitmap *b, int x, int y, const char *text) {
    assert(b);
    if(!b->font || !b->font->puts)
        return 0;
    return b->font->puts(b, x, y, text);
}

int bm_printf(Bitmap *b, int x, int y, const char *fmt, ...) {
    char buffer[256];
    va_list arg;
    assert(b);
    if(!b->font || !b->font->puts) return 0;
    va_start(arg, fmt);
    vsnprintf(buffer, sizeof buffer, fmt, arg);
    va_end(arg);
    return bm_puts(b, x, y, buffer);
}

BmFont *bm_font_retain(BmFont *font) {
    assert(font);
    font->ref_count++;
    return font;
}

unsigned int bm_font_release(BmFont *font) {
    assert(font);
    assert(font->ref_count > 0);
    font->ref_count--;
    if(!font->ref_count) {
        if(font->dtor)
            font->dtor(font);
        return 0;
    }
    return font->ref_count;
}

/** RASTER FONT FUNCTIONS ******************************************************/

typedef struct {
    Bitmap *bmp;
    int width;
    int height;
    int spacing;
} RasterFontData;

static int rf_puts(Bitmap *b, int x, int y, const char *s) {
    assert(!strcmp(b->font->type, "RASTER_FONT"));
    RasterFontData *data = CAST(RasterFontData *)(b->font->data);
    int x0 = x;
    while(*s) {
        if(*s == '\n') {
            y += data->height;
            x = x0;
        } else if(*s == '\b') {
            if(x > x0) x -= data->spacing;
        } else if(*s == '\r') {
            x = x0;
        } else if(*s == '\t') {
            x += 4 * data->spacing;
        } else {
            int c = *s;
            c -= 32;
            int sy = (c >> 4) * data->height;
            int sx = (c & 0xF) * data->width;
            bm_maskedblit(b, x, y, data->bmp, sx, sy, data->width, data->height);
            x += data->spacing;
        }
        s++;
    }
    return 1;
}

static int rf_width(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "RASTER_FONT"));
    (void)codepoint;
    RasterFontData *data = CAST(RasterFontData *)(font->data);
    return data->width;
}
static int rf_height(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "RASTER_FONT"));
    (void)codepoint;
    RasterFontData *data = CAST(RasterFontData *)(font->data);
    return data->height;
}

static void rf_free_font(BmFont *font) {
    if(!font || strcmp(font->type, "RASTER_FONT"))
        return;
    bm_free(((RasterFontData*)font->data)->bmp);
    free(font->data);
    free(font);
}

BmFont *bm_make_ras_font(const char *file, int spacing) {
    unsigned int bg = 0;
    BmFont *font = CAST(BmFont *)(malloc(sizeof *font));
    if (!font)
        return NULL;
    font->type = "RASTER_FONT";
    font->ref_count = 1;
    font->puts = rf_puts;
    font->width = rf_width;
    font->height = rf_height;
    font->measure = NULL;
    font->dtor = rf_free_font;
    RasterFontData *data = CAST(RasterFontData *)(malloc(sizeof *data));
    if (!data) {
        SET_ERROR("out of memory");
        free(font);
        return NULL;
    }
    data->bmp = bm_load(file);
    if(!data->bmp) {
        free(data);
        free(font);
        return NULL;
    }
    /* The top-left character is a space, so we can safely assume that that pixel
       is the transparent color. */
    bg = BM_GET(data->bmp, 0, 0);
    bm_set_color(data->bmp, bg);
    /* The width/height depends on the bitmap being laid out as prescribed */
    data->width = data->bmp->w / 16;
    data->height = data->bmp->h / 6;
    if(spacing <= 0) spacing = data->width;
    data->spacing = spacing;
    font->data = data;
    return font;
}

/** SFont and GraFX2 Font Functions ********************************************/

typedef struct {
    Bitmap *bmp;
    int offset[94];
    int widths[94];
    int num;
    int width;
    int height;
} SFontData;

static int sf_puts(Bitmap *b, int x, int y, const char *s) {
    assert(!strcmp(b->font->type, "SFONT"));
    SFontData *data = CAST(SFontData *)(b->font->data);
    int x0 = x;

    int cw = 0, ch = data->bmp->h - 1;
    if(data->num < 'Z' - 33) {
        /* bad font */
        return 0;
    }

    while(*s) {
        if(*s == '\n') {
            y += ch + 1;
            x = x0;
        } else if(*s == ' ') {
            x += data->width;
        } else if(*s == '\b') {
            if(x > x0) x -= cw;
        } else if(*s == '\r') {
            x = x0;
        } else if(*s == '\t') {
            x += 4 * data->width;
        } else {
            int c = *s - 33;
            if(c >= data->num) {
                /* Unsupported character */
                if(isalpha(*s))
                    c = toupper(*s) - 33;
                else
                    c = '*' - 33;
            }
            assert(c < data->num);

            int sx = data->offset[c];
            int sy = 1;
            cw = data->widths[c];
            bm_maskedblit(b, x, y, data->bmp, sx, sy, cw, ch);
            x += cw;
        }
        s++;
    }
    return 1;
}

static int sf_width(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "SFONT"));
    (void)codepoint;
    SFontData *data = CAST(SFontData *)(font->data);
    return data->width;
}

static int sf_height(BmFont *font, unsigned int codepoint) {
    assert(!strcmp(font->type, "SFONT"));
    (void)codepoint;
    SFontData *data = CAST(SFontData *)(font->data);
    return data->height;
}

static void sf_dtor(BmFont *font) {
    if(!font || strcmp(font->type, "SFONT"))
        return;
    bm_free(((SFontData*)font->data)->bmp);
    free(font->data);
    free(font);
}

BmFont *bm_make_sfont(const char *file) {
    SET_ERROR("no error");
    unsigned int bg, mark;
    int cnt = 0, x, w = 1, s = 0, mw = 0, state = 0;
    Bitmap *b = NULL;
    SFontData *data = NULL;
    BmFont *font = CAST(BmFont *)(malloc(sizeof *font));
    if(!font) {
        SET_ERROR("out of memory");
        return NULL;
    }
    font->type = "SFONT";
    font->ref_count = 1;
    font->puts = sf_puts;
    font->width = sf_width;
    font->height = sf_height;
    font->measure = NULL;
    font->dtor = sf_dtor;
    data = CAST(SFontData *)(malloc(sizeof *data));
    if(!data) {
        SET_ERROR("out of memory");
        goto error;
    }
    data->bmp = bm_load(file);
    if(!data->bmp)
        goto error;
    b = data->bmp;

    /* Find the marker color */
    mark = BM_GET(data->bmp, 0, 0);
    for(x = 1; (bg = BM_GET(b, x, 0)) == mark; x++) {
        if(x >= b->w) {
            SET_ERROR("invalid SFont");
            goto error;
        }
    }

    /* Use a small state machine to extract all the
    characters' offsets and widths */
    for(x = 0; x < b->w; x++) {
        unsigned int col = BM_GET(b, x, 0);
        if(cnt == 94)
            break;
        if(state == 0) {
            if(col != mark) {
                s = x;
                state = 1;
            }
        } else {
            if(col == mark) {
                /* printf("'%c' x=%u; w=%u\n", cnt+33, s, w); */
                data->offset[cnt] = s;
                data->widths[cnt] = w;
                if(w > mw) mw = w;
                cnt++;
                w = 1;
                state = 0;
            } else {
                w++;
            }
        }
    }
    /* The last character: */
    if(state) {
        /* printf("'%c' x=%u; w=%u\n", cnt+33, s, w); */
        data->offset[cnt] = s;
        data->widths[cnt] = w;
        if(w > mw) mw = w;
        cnt++;
    }

    bm_set_color(b, bg);

    /* Yes, `width` is the maximum width; It borks
     * `bm_text_width()` if you don't a fixed width font.
     */
    data->width = mw;
    data->height = b->h - 1;
    data->num = cnt;

    font->data = data;
    return font;
error:
    if(data) free(data);
    if(font) free(font);
    if(b) bm_free(b);
    return NULL;
}

/** XBM FONT FUNCTIONS *********************************************************/

/* --- normal.xbm ------------------------------------------------------------ */
#define normal_width 128
#define normal_height 48
static unsigned char normal_bits[] = {
   0xff, 0xf7, 0xeb, 0xff, 0xf7, 0xff, 0xfb, 0xf7, 0xef, 0xfb, 0xf7, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xeb, 0xeb, 0xc3, 0xd9, 0xf5, 0xf7,
   0xf7, 0xf7, 0xd5, 0xf7, 0xff, 0xff, 0xff, 0xdf, 0xff, 0xf7, 0xff, 0xc1,
   0xf5, 0xe9, 0xf5, 0xff, 0xfb, 0xef, 0xe3, 0xf7, 0xff, 0xff, 0xff, 0xef,
   0xff, 0xf7, 0xff, 0xeb, 0xe3, 0xf7, 0xfb, 0xff, 0xfb, 0xef, 0xf7, 0xc1,
   0xff, 0xc3, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xc1, 0xd7, 0xcb, 0xd5, 0xff,
   0xfb, 0xef, 0xe3, 0xf7, 0xff, 0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xeb,
   0xe1, 0xcd, 0xed, 0xff, 0xf7, 0xf7, 0xd5, 0xf7, 0xef, 0xff, 0xff, 0xfd,
   0xff, 0xf7, 0xff, 0xff, 0xf7, 0xff, 0xd3, 0xff, 0xef, 0xfb, 0xf7, 0xff,
   0xef, 0xff, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xe3, 0xf7, 0xe3, 0xc1,
   0xef, 0xc1, 0xe3, 0xc1, 0xe3, 0xe3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3,
   0xdd, 0xf3, 0xdd, 0xdf, 0xe7, 0xfd, 0xdd, 0xdf, 0xdd, 0xdd, 0xff, 0xff,
   0xef, 0xff, 0xfb, 0xdd, 0xcd, 0xf7, 0xdf, 0xef, 0xeb, 0xe1, 0xfd, 0xef,
   0xdd, 0xdd, 0xef, 0xef, 0xf7, 0xe3, 0xf7, 0xdf, 0xd5, 0xf7, 0xe7, 0xe7,
   0xed, 0xdf, 0xe1, 0xf7, 0xe3, 0xc3, 0xff, 0xff, 0xfb, 0xff, 0xef, 0xe7,
   0xd9, 0xf7, 0xfb, 0xdf, 0xc1, 0xdf, 0xdd, 0xfb, 0xdd, 0xdf, 0xff, 0xff,
   0xf7, 0xe3, 0xf7, 0xf7, 0xdd, 0xf7, 0xfd, 0xdd, 0xef, 0xdd, 0xdd, 0xfb,
   0xdd, 0xdd, 0xff, 0xef, 0xef, 0xff, 0xfb, 0xff, 0xe3, 0xe3, 0xc1, 0xe3,
   0xef, 0xe3, 0xe3, 0xfb, 0xe3, 0xe3, 0xef, 0xef, 0xff, 0xff, 0xff, 0xf7,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7,
   0xff, 0xff, 0xff, 0xff, 0xe3, 0xf7, 0xe1, 0xe3, 0xe1, 0xc1, 0xc1, 0xe3,
   0xdd, 0xe3, 0xcf, 0xdd, 0xfd, 0xdd, 0xdd, 0xe3, 0xdd, 0xeb, 0xdd, 0xdd,
   0xdd, 0xfd, 0xfd, 0xdd, 0xdd, 0xf7, 0xdf, 0xdd, 0xfd, 0xc9, 0xdd, 0xdd,
   0xc5, 0xdd, 0xdd, 0xfd, 0xdd, 0xfd, 0xfd, 0xfd, 0xdd, 0xf7, 0xdf, 0xed,
   0xfd, 0xd5, 0xd9, 0xdd, 0xd5, 0xdd, 0xe1, 0xfd, 0xdd, 0xe1, 0xe1, 0xc5,
   0xc1, 0xf7, 0xdf, 0xf1, 0xfd, 0xdd, 0xd5, 0xdd, 0xe5, 0xc1, 0xdd, 0xfd,
   0xdd, 0xfd, 0xfd, 0xdd, 0xdd, 0xf7, 0xdf, 0xed, 0xfd, 0xdd, 0xcd, 0xdd,
   0xfd, 0xdd, 0xdd, 0xdd, 0xdd, 0xfd, 0xfd, 0xdd, 0xdd, 0xf7, 0xdd, 0xdd,
   0xfd, 0xdd, 0xdd, 0xdd, 0xe3, 0xdd, 0xe1, 0xe3, 0xe1, 0xc1, 0xfd, 0xe3,
   0xdd, 0xe3, 0xe3, 0xdd, 0xc1, 0xdd, 0xdd, 0xe3, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xe1, 0xe3, 0xe1, 0xe3, 0xc1, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xc1, 0xe3,
   0xff, 0xe3, 0xf7, 0xff, 0xdd, 0xdd, 0xdd, 0xdd, 0xf7, 0xdd, 0xdd, 0xdd,
   0xdd, 0xdd, 0xdf, 0xfb, 0xfd, 0xef, 0xeb, 0xff, 0xdd, 0xdd, 0xdd, 0xfd,
   0xf7, 0xdd, 0xdd, 0xdd, 0xeb, 0xeb, 0xef, 0xfb, 0xfb, 0xef, 0xdd, 0xff,
   0xe1, 0xdd, 0xe1, 0xe3, 0xf7, 0xdd, 0xdd, 0xdd, 0xf7, 0xf7, 0xf7, 0xfb,
   0xf7, 0xef, 0xff, 0xff, 0xfd, 0xd5, 0xdd, 0xdf, 0xf7, 0xdd, 0xdd, 0xd5,
   0xeb, 0xf7, 0xfb, 0xfb, 0xef, 0xef, 0xff, 0xff, 0xfd, 0xed, 0xdd, 0xdd,
   0xf7, 0xdd, 0xeb, 0xc9, 0xdd, 0xf7, 0xfd, 0xfb, 0xdf, 0xef, 0xff, 0xff,
   0xfd, 0xd3, 0xdd, 0xe3, 0xf7, 0xc3, 0xf7, 0xdd, 0xdd, 0xf7, 0xc1, 0xe3,
   0xff, 0xe3, 0xff, 0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xfd, 0xff,
   0xdf, 0xff, 0xe7, 0xff, 0xfd, 0xf7, 0xef, 0xfd, 0xf3, 0xff, 0xff, 0xff,
   0xe7, 0xff, 0xfd, 0xff, 0xdf, 0xff, 0xdb, 0xff, 0xfd, 0xff, 0xff, 0xfd,
   0xf7, 0xff, 0xff, 0xff, 0xef, 0xe3, 0xe1, 0xc3, 0xc3, 0xe3, 0xfb, 0xe3,
   0xfd, 0xf3, 0xe7, 0xed, 0xf7, 0xe9, 0xe5, 0xe3, 0xff, 0xdf, 0xdd, 0xfd,
   0xdd, 0xdd, 0xf1, 0xdd, 0xe1, 0xf7, 0xef, 0xf5, 0xf7, 0xd5, 0xd9, 0xdd,
   0xff, 0xc3, 0xdd, 0xfd, 0xdd, 0xc1, 0xfb, 0xdd, 0xdd, 0xf7, 0xef, 0xf9,
   0xf7, 0xdd, 0xdd, 0xdd, 0xff, 0xdd, 0xdd, 0xfd, 0xdd, 0xfd, 0xfb, 0xc3,
   0xdd, 0xf7, 0xef, 0xf5, 0xf7, 0xdd, 0xdd, 0xdd, 0xff, 0xc3, 0xe1, 0xc3,
   0xc3, 0xc3, 0xfb, 0xdf, 0xdd, 0xe3, 0xed, 0xed, 0xe3, 0xdd, 0xdd, 0xe3,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xf3, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xcf, 0xf7, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xf7, 0xf7, 0xd3, 0xff,
   0xe1, 0xc3, 0xe5, 0xe3, 0xe1, 0xdd, 0xdd, 0xdd, 0xdd, 0xdd, 0xc1, 0xf7,
   0xf7, 0xf7, 0xe5, 0xff, 0xdd, 0xdd, 0xd9, 0xfd, 0xfb, 0xdd, 0xdd, 0xdd,
   0xeb, 0xdd, 0xef, 0xf9, 0xff, 0xcf, 0xff, 0xff, 0xdd, 0xdd, 0xfd, 0xf3,
   0xfb, 0xdd, 0xdd, 0xdd, 0xf7, 0xdd, 0xf7, 0xf7, 0xf7, 0xf7, 0xff, 0xff,
   0xe1, 0xc3, 0xfd, 0xef, 0xdb, 0xcd, 0xeb, 0xd5, 0xeb, 0xc3, 0xfb, 0xf7,
   0xf7, 0xf7, 0xff, 0xff, 0xfd, 0xdf, 0xfd, 0xf1, 0xe7, 0xd3, 0xf7, 0xeb,
   0xdd, 0xdf, 0xc1, 0xcf, 0xf7, 0xf9, 0xff, 0xff, 0xfd, 0xdf, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xe3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
/* --------------------------------------------------------------------------- */

#define XBM_FONT_WIDTH 128

typedef struct {
    const unsigned char *bits;
    int spacing;
} XbmFontInfo;

static void xbmf_putc(Bitmap *b, const unsigned char *xbm_bits, int x, int y, unsigned int col, char c) {
    int frow, fcol, byte;
    int i, j;

    if(c < 32)
        return;

    c -= 32;
    frow = (c >> 4);
    fcol = c & 0xF;
    byte = frow * XBM_FONT_WIDTH + fcol;

    for(j = 0; j < 8 && y + j < b->clip.y1; j++) {
        if(y + j >= b->clip.y0) {
            char bits = xbm_bits[byte];
            for(i = 0; i < 8 && x + i < b->clip.x1; i++) {
                if(x + i >= b->clip.x0 && !(bits & (1 << i))) {
                    BM_SET(b, x + i, y + j, col);
                }
            }
        }
        byte += XBM_FONT_WIDTH >> 3;
    }
}

static int xbmf_puts(Bitmap *b, int x, int y, const char *text) {
    XbmFontInfo *info;
    int xs = x, spacing;
    const unsigned char *bits;
    unsigned int col = bm_get_color(b);

    if(!b->font) return 0;

    info = CAST(XbmFontInfo*)(b->font->data);
    if(info) {
        spacing = info->spacing;
        bits = info->bits;
    } else {
        spacing = 6;
        bits = normal_bits;
    }

    while(text[0]) {
        if(text[0] == '\n') {
            y += 8;
            x = xs;
        } else if(text[0] == '\t') {
            /* I briefly toyed with the idea of having tabs line up,
             * but it doesn't really make sense because
             * this isn't exactly a character based terminal.
             */
            x += 4 * spacing;
        } else if(text[0] == '\r') {
            /* why would anyone find this useful? */
            x = xs;
        } else {
            xbmf_putc(b, bits, x, y, col, text[0]);
            x += spacing;
        }
        text++;
        if(y > b->h) {
            /* I used to check x >= b->w as well,
            but it doesn't take \n's into account */
            return 1;
        }
    }
    return 1;
}

static int xbmf_width(BmFont *font, unsigned int codepoint) {
    XbmFontInfo *info = CAST(XbmFontInfo *)(font->data);
    (void)codepoint;
    if(!info) return 6;
    return info->spacing;
}

static int xbmf_height(BmFont *font, unsigned int codepoint) {
    (void)font; (void)codepoint;
    return 8;
}

static void xbmf_free(BmFont *font) {
    XbmFontInfo *info = CAST(XbmFontInfo *)(font->data);
    assert(!strcmp(font->type, "XBM"));
    free(info);
    free(font);
}

BmFont *bm_make_xbm_font(const unsigned char *bits, int spacing) {
    SET_ERROR("no error");
    BmFont *font;
    XbmFontInfo *info;
    font = CAST(BmFont *)(malloc(sizeof *font));
    if(!font) {
        SET_ERROR("out of memory");
        return NULL;
    }
    info = CAST(XbmFontInfo *)(malloc(sizeof *info));
    if(!info) {
        SET_ERROR("out of memory");
        free(font);
        return NULL;
    }

    info->bits = bits;
    info->spacing = spacing;

    font->type = "XBM";
    font->ref_count = 1;
    font->puts = xbmf_puts;
    font->width = xbmf_width;
    font->height = xbmf_height;
    font->measure = NULL;
    font->dtor = xbmf_free;
    font->data = info;

    return font;
}

/*
 * Support for Damien Gaurd's ZX Origins fonts.
 * https://damieng.com/typography/zx-origins/
 */
typedef struct {
    int owned;
    union {char *b; const uint8_t *c;} bits;
} ZxoFontInfo;

static void zxo_putc(Bitmap *b, const unsigned char *zxo_bits, int x, int y, unsigned int col, char c) {
    int byte;
    int i, j;

    if(c < 32)
        return;

    byte = (c - 32) * 8;
    for(j = 0; j < 8 && y + j < b->clip.y1; j++, byte++) {
        if(y + j >= b->clip.y0) {
            char bits = zxo_bits[byte];
            for(i = 0; i < 8 && x + i < b->clip.x1; i++) {
                if(x + i >= b->clip.x0 && (bits & (0x80 >> i))) {
                    BM_SET(b, x + i, y + j, col);
                }
            }
        }
    }
}

static int zxo_puts(Bitmap *b, int x, int y, const char *text) {
    ZxoFontInfo *info;
    int xs = x;
    unsigned int col = bm_get_color(b);

    if(!b->font) return 0;

    info = CAST(ZxoFontInfo*)(b->font->data);

    while(text[0]) {
        if(text[0] == '\n') {
            y += 8;
            x = xs;
        } else if(text[0] == '\t') {
            x += 4 * 8;
        } else if(text[0] == '\r') {
            x = xs;
        } else {
            zxo_putc(b, info->bits.c, x, y, col, text[0]);
            x += 8;
        }
        text++;
        if(y > b->h) {
            return 1;
        }
    }
    return 1;
}

static int zxo_width(BmFont *font, unsigned int codepoint) {
    (void)font; (void)codepoint;
    return 8;
}

static int zxo_height(BmFont *font, unsigned int codepoint) {
    (void)font; (void)codepoint;
    return 8;
}

static void zxo_free(BmFont *font) {
    ZxoFontInfo *info = CAST(ZxoFontInfo *)(font->data);
    assert(!strcmp(font->type, "ZXO"));
    if(info->owned)
        free(info->bits.b);
    free(info);
    free(font);
}

BmFont *bm_make_zxo_font(const unsigned char *bits) {
    SET_ERROR("no error");
    BmFont *font;
    ZxoFontInfo *info;
    font = CAST(BmFont *)(malloc(sizeof *font));
    if(!font) {
        SET_ERROR("out of memory");
        return NULL;
    }
    info = CAST(ZxoFontInfo *)(malloc(sizeof *info));
    if(!info) {
        SET_ERROR("out of memory");
        free(font);
        return NULL;
    }

    info->bits.c = bits;
    info->owned = 0;

    font->type = "ZXO";
    font->ref_count = 1;
    font->puts = zxo_puts;
    font->width = zxo_width;
    font->height = zxo_height;
    font->measure = NULL;
    font->dtor = zxo_free;
    font->data = info;

    return font;
}

static BmFont *bm_load_zxo_font_rd(BmReader *rd) {

    BmFont *font;
    ZxoFontInfo *info;

    SET_ERROR("no error");
    font = CAST(BmFont *)(malloc(sizeof *font));
    if(!font) {
        SET_ERROR("out of memory");
        return NULL;
    }
    info = CAST(ZxoFontInfo *)(malloc(sizeof *info));
    if(!info) {
        SET_ERROR("out of memory");
        free(font);
        return NULL;
    }

    info->bits.b = malloc(768);
    if(!info->bits.b) {
        SET_ERROR("out of memory");
        free(info);
        free(font);
        return NULL;
    }
    info->owned = 1;

    if(rd->fread(info->bits.b, 1, 768, rd->data) != 768) {
        SET_ERROR("bad font file");
        free(info->bits.b);
        free(info);
        free(font);
        return NULL;
    }

    font->type = "ZXO";
    font->ref_count = 1;
    font->puts = zxo_puts;
    font->width = zxo_width;
    font->height = zxo_height;
    font->measure = NULL;
    font->dtor = zxo_free;
    font->data = info;

    return font;
}

/* Loads the Spectrum/font.ch8 binary bytes */
BmFont *bm_load_zxo_font(const char *filename) {
    SET_ERROR("no error");
    FILE *f = fopen(filename, "rb");
    if(!f) return NULL;
    BmReader rd = make_file_reader(f);
    BmFont *font = bm_load_zxo_font_rd(&rd);
    fclose(f);
    return font;
}


void bm_reset_font(Bitmap *b) {
    static BmFont font = {"XBM",1,xbmf_puts,xbmf_width,xbmf_height,NULL,NULL,NULL};
    bm_set_font(b, &font);
}

const char *bm_get_error() {
#if BM_LAST_ERROR
    return bm_last_error;
#else
    return "";
#endif
}

void bm_set_error(const char *e) {
#if BM_LAST_ERROR
    bm_last_error = e;
#endif
}

/* Not all systems have a `strtok_r()`, so here's mine */
char *bm_strtok_r(char *str, const char *delim, char **saveptr) {
    char *s;
    if(!str)
        str = *saveptr;
    if(!str[0]) {
        *saveptr = str;
        return NULL;
    }
    s = strpbrk(str, delim);
    if(s) {
        s[0] = '\0';
        *saveptr = s + 1;
        while((*saveptr)[0] && strchr(delim, (*saveptr)[0]))
            (*saveptr)++;
    } else
        for(*saveptr = str; (*saveptr)[0]; (*saveptr)++);
    return str;
}
