CC=gcc
AWK=awk

# Add your source files here:
SOURCES=fttest.c ftfont.c

OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=fttest

CFLAGS = -Wall -Werror -DUSEPNG -I ../ `freetype-config --cflags`
LDFLAGS = `freetype-config --libs` -lpng -lz

# Just to test if the SDL2 related things actually work:
#CFLAGS += -DUSESDL `sdl2-config --cflags`
#LDFLAGS += `sdl2-config --libs`

ifeq ($(BUILD),debug)
# Debug
CFLAGS += -O0 -g
LDFLAGS +=
else
# Release mode
CFLAGS += -O2 -DNDEBUG
LDFLAGS += -s
endif

all: $(EXECUTABLE) docs

debug:
	make BUILD=debug

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) ../bmp.o $(LDFLAGS) -o $@
	
docs: ftfont.html

ftfont.html: ftfont.h ../comdown.awk
	$(AWK) -f ../comdown.awk $< > $@
	
.c.o:
	$(CC) $(CFLAGS) -c $< -o $@
	
fttest.o : fttest.c ../bmp.h ftfont.h
ftfont.o : ftfont.c ../bmp.h ftfont.h

clean:
	-rm -f *.o $(EXECUTABLE) $(EXECUTABLE).exe
	-rm -f ftfont.html out.gif