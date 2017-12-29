CC=gcc
CFLAGS=-c -Wall -I /usr/local/include `libpng-config --cflags` -DUSEPNG -DUSEJPG
LDFLAGS=`libpng-config --ldflags` -lz -ljpeg
AWK=awk

# Add your source files here:
LIB_SOURCES=bmp.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=libbmp.a

DOCS=bitmap.html README.html fonts/instructions.html

ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG
LDFLAGS += -s
endif

all: libbmp.a hello docs

debug:
	make BUILD=debug

libbmp.a: $(LIB_OBJECTS)
	ar rs $@ $^

.c.o:
	$(CC) $(CFLAGS) $< -o $@

bmp.o: bmp.c bmp.h

docs: $(DOCS)

bitmap.html: bmp.h d.awk
	$(AWK) -f d.awk $< > $@

README.html: README.md d.awk
	$(AWK) -f d.awk -v Clean=1 $< > $@

fonts/instructions.html: fonts/instructions.md d.awk
	$(AWK) -f d.awk -v Clean=1 $< > $@

hello: hello.c libbmp.a
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY : clean

clean:
	-rm -f *.o $(LIB)
	-rm -f hello *.exe test/*.exe
	-rm -rf $(DOCS)

# The .exe above is for MinGW, btw.
