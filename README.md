# bitmap
A C library for manipulating bitmap graphics.

Features:
* Supported formats:
  * Windows BMP, GIF and PCX files can be loaded and saved without third-party dependencies.
  * PNG through [libpng](http://www.libpng.org)
  * JPEG through [libjpeg](http://www.ijg.org/)
  * `Bitmap` structures can also be created from XBM data.
* Support fo SDL2' `SDL_RWops` file manipulation routines for loading images in the supported formats.
* Supports manipulation of OpenGL textures, SDL surfaces and so on. See the `bm_bind()` function.
* Primitives: Lines, circles, elipses, bezier curves, etc.
  * Clipping rectangles are obeyed.
* Floodfill and filled primitives
* Image resizing: nearest neighbour, bilinear and bicubic
* Blitting, blitting with masks and scaled blitting.
* Filtering with kernels and smoothing.
* [CSS-style](http://en.wikipedia.org/wiki/Web_colors) colors.
* Cross platform. It's been known to compile under Windows (MinGW), Linux, Android and Web via Emscripten.

The `fonts/` directory contains some 8-bit style bitmap fonts in XBM format.

## Getting Started

Copy `bmp.c` and `bmp.h` to your project directory, and add `bmp.c` to your list of files to compile.

Copy the `fonts/` directory into your project directory. If you don't want the 8-bit 
fonts, compile with `-DNO_FONTS` instead.

To enable PNG support you need zlib and [libpng](http://www.libpng.org) (the development versions) installed. 
Use the `-DUSEPNG` command line option when compiling.

Likewise, to enable JPEG support, you need [libjpeg](http://www.ijg.org/) installed and specify the `-DUSEJPG`
command line option when compiling.

Use `bm_create()` to create `Bitmap` objects and `bm_free()` to destroy them.

`bm_bind()` can be used to wrap a Bitmap object around an existing buffer of bytes, such as
OpenGL textures and SDL surfaces.

The `Makefile` generates HTML documentation from `bmp.h` through the `doc.awk` script.

## Additional Utilities

* `bmpfont.c` and `bmpfont.h` - Wrapper around [FreeType](http://www.freetype.org/) to 
   allow rendering of freetype-supported fonts on {{Bitmap}} structures.
* The `palette/` directory contains a utility for generating palettes and converting 
   images to those palettes.
* The `misc/` directory contains
** `gif.c` - the file I used to develop the GIF encoder/decoder originally. I've kept 
   it in case I want to do something advanced later, like animated GIFs.

## References

- http://en.wikipedia.org/wiki/BMP_file_format
- http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
- http://members.chello.at/~easyfilter/bresenham.html
- http://en.wikipedia.org/wiki/Flood_fill
- http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
- http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html
- http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts
- http://www.w3.org/Graphics/GIF/spec-gif89a.txt
- Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
- http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011
- http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp
- http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt
- http://www.shikadi.net/moddingwiki/PCX_Format
