CC=gcc
CFLAGS=-c -Wall -Wextra -I /usr/local/include `libpng-config --cflags` -DUSEPNG -DUSEJPG
LDFLAGS=`libpng-config --ldflags` -lz -ljpeg -lm
AWK=awk

# Add your source files here:
LIB_SOURCES=bmp.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=libbmp.a

DOCS=doc/bitmap.html doc/README.html doc/freetype-fonts.html doc/built-in-fonts.html doc/LICENSE

ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG
LDFLAGS += -s
endif

all: libbmp.a docs utils bmph.h

debug:
	make BUILD=debug

libbmp.a: $(LIB_OBJECTS)
	ar rs $@ $^

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

bmp.o: bmp.c bmp.h

docs: doc $(DOCS)

doc:
	mkdir -p doc

doc/bitmap.html: bmp.h d.awk | doc
	$(AWK) -v Title="API Documentation" -f d.awk $< > $@

doc/LICENSE: LICENSE | doc
	cp $< $@

doc/README.html: README.md d.awk | doc
	$(AWK) -f d.awk -v Clean=1 -v Title="README" $< > $@

doc/freetype-fonts.html: ftypefont/ftfont.h d.awk | doc
	$(AWK) -v Title="FreeType Font Support" -f d.awk $< > $@

doc/built-in-fonts.html: fonts/instructions.md d.awk | doc
	$(AWK) -v Title="Raster Font Support" -f d.awk -v Clean=1 $< > $@

# Single header file library
bmph.h: bmp.c bmp.h
	@echo Making stb-style single header library $@ from $^
	@echo '/* STB-style single header bitmap library.' > $@
	@echo ' *' >> $@
	@echo ' * To use this file, add the following to _one_ source file:' >> $@
	@echo ' *' >> $@
	@echo ' *     #define BMPH_IMPLEMENTATION' >> $@
	@echo ' *     #include "bmph.h"' >> $@
	@echo ' *' >> $@
	@echo ' * and then use `#include "bmph.h"` in all other files that' >> $@
	@echo ' * use the bitmap functionality.' >> $@
	@echo ' *' >> $@
	@echo ' * This file was automatically generated by a script.' >> $@
	@echo ' * To modify the code, it is recommend to modify the original' >> $@
	@echo ' * sources and then regenerating this file by running `make bmph.h`.' >> $@
	@echo ' *' >> $@
	@echo ' * The original sources can be found at https://github.com/wernsey/bitmap' >> $@
	@echo ' * Contributions are welcome.' >> $@
	@echo ' *' >> $@
	@echo ' * For more information on stb-style single header libraries, see' >> $@
	@echo ' * https://github.com/nothings/stb/blob/master/docs/stb_howto.txt' >> $@
	@echo ' */' >> $@
	@cat bmp.h >> $@
	@echo '#ifdef BMPH_IMPLEMENTATION' >> $@
	@cat bmp.c >> $@
	@echo '#endif /* BMPH_IMPLEMENTATION */' >> $@

utils: util/hello util/bmfont util/dumpfonts util/cvrt util/imgdup

util/hello: hello.c libbmp.a | util
	$(CC) -o $@ $^ $(LDFLAGS)

util/bmfont: fonts/bmfont.c misc/to_xbm.c libbmp.a | util
	$(CC) -o $@ $^ $(LDFLAGS)

util/dumpfonts: fonts/dumpfonts.c misc/to_xbm.c libbmp.a | util
	$(CC) -o $@ $^ $(LDFLAGS)

util/cvrt: misc/cvrt.c libbmp.a | util
	$(CC) -o $@ $^ $(LDFLAGS)

util/imgdup: misc/imgdup.c libbmp.a | util
	$(CC) -o $@ $^ $(LDFLAGS)

util:
	mkdir -p util

.PHONY : clean

clean:
	-rm -f *.o $(LIB) bmph.h
	-rm -f hello *.exe test/*.exe
	-rm -rf $(DOCS)
	-rm -rf util doc

# The .exe above is for MinGW, btw.
