# bitmap

A C library for manipulating bitmap graphics.

Features:
* Supported formats:
  * Windows BMP, GIF, PCX and TGA files can be loaded and saved without third-party
    dependencies.
  * PNG through [libpng](http://www.libpng.org)
  * JPEG through [libjpeg](http://www.ijg.org/)
* Support fo SDL2' `SDL_RWops` file manipulation routines for loading images in
  the supported formats.
* Supports manipulation of OpenGL textures, [SDL](https://www.libsdl.org/) surfaces,
  [GDI](https://en.wikipedia.org/wiki/Graphics_Device_Interface) contexts. See the
  `bm_bind()` function. It can also serve as a back end for
  [Cairo graphics](https://cairographics.org)
* Primitives: Lines, circles, elipses, bezier curves, etc.
  * Clipping rectangles are obeyed.
* Floodfill and filled primitives
* Image resizing: nearest neighbour, bilinear and bicubic
* Blitting, blitting with masks and scaled blitting.
* Text rendering: built-in _8-bit_ style raster fonts and FreeType support.
* Filtering with kernels and smoothing.
* [CSS-style](http://en.wikipedia.org/wiki/Web_colors) colors.
* Loading images from [XBM](https://en.wikipedia.org/wiki/X_BitMap) and
  [X PixMap](https://en.wikipedia.org/wiki/X_PixMap) data.
* Cross platform. It's been known to compile under Windows (MinGW), Linux,
  Android and Web via Emscripten.

The `fonts/` directory contains some 8-bit style bitmap fonts in XBM format.

## Getting Started

Copy `bmp.c` and `bmp.h` to your project directory, and add `bmp.c` to your
list of files to compile.

To enable PNG support you need zlib and [libpng](http://www.libpng.org) (the
development versions) installed. Use the `-DUSEPNG` command line option when
compiling.

Likewise, to enable JPEG support, you need [libjpeg](http://www.ijg.org/)
installed and specify the `-DUSEJPG` command line option when compiling.

Use `bm_create()` to create `Bitmap` objects and `bm_free()` to destroy them.

`bm_bind()` can be used to wrap a Bitmap object around an existing buffer of
bytes, such as OpenGL textures and SDL surfaces.

The `Makefile` generates HTML documentation from `bmp.h` through the `doc.awk`
script.

## Additional Utilities

* The `fonts/` directory contains some 8&times;8 bitmap fonts in XBM format
  that can be loaded via the `bm_make_xbm_font()` function.
  * Most of the fonts are based on 80's home computers, drawn from the examples
  at http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts
  * The `INSTRUCTIONS` file contains information for using them.
  * The `tomthumb.c` file contains a 4&times;6 font based on the "Tom Thumb"
    monospace font at http://robey.lag.net/2010/01/23/tiny-monospace-font.html
* The `ftypefont/` directory contains a wrapper for
  [FreeType](http://www.freetype.org/) to allow rendering of freetype-supported
  fonts on `Bitmap` structures.
* The `misc/` directory contains
  * `gif.c` and `gif.h` - code for programmatically manipulating animated GIFs.
    It originates from the file I used to develop the GIF encoder/decoder originally.
  * `cairoback.c` - A demo of how the bitmap objects can be used as a back-end
    for the [Cairo graphics library](https://cairographics.org)
  * The `palette/` subdirectory contains a utility for generating palettes and
    converting images to those palettes.
  * `pbm.c` and `xbm.c`: Samples on how to use the module with PBM and XBM file
    formats.
  * The `kmeans/` subdirectory contains a program that uses
    [K-means clustering](https://en.wikipedia.org/wiki/K-means_clustering)
    to identify the dominant colors in an image.

## References

- <http://en.wikipedia.org/wiki/BMP_file_format>
- <http://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm>
- <http://members.chello.at/~easyfilter/bresenham.html>
- <http://en.wikipedia.org/wiki/Flood_fill>
- <http://en.wikipedia.org/wiki/Midpoint_circle_algorithm>
- <http://web.archive.org/web/20110706093850/http://free.pages.at/easyfilter/bresenham.html>
- <http://damieng.com/blog/2011/02/20/typography-in-8-bits-system-fonts>
- <http://www.w3.org/Graphics/GIF/spec-gif89a.txt>
- Nelson, M.R. : "LZW Data Compression", Dr. Dobb's Journal, October 1989.
- <http://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011>
- <http://www.matthewflickinger.com/lab/whatsinagif/bits_and_bytes.asp>
- <http://web.archive.org/web/20100206055706/http://www.qzx.com/pc-gpe/pcx.txt>
- <http://www.shikadi.net/moddingwiki/PCX_Format>
- <https://en.wikipedia.org/wiki/Truevision_TGA>
- <http://paulbourke.net/dataformats/tga/>
- <http://www.ludorg.net/amnesia/TGA_File_Format_Spec.html>
