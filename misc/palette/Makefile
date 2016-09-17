EXECUTABLE=palette

# Add your source files here:
OBJECTS=palette.o

CFLAGS=-c -Wall -DNO_FONTS
LDFLAGS=

all: $(EXECUTABLE)

lib: $(LIB)

$(EXECUTABLE) : palette.o ../../bmp.o
	$(CC) $(LDFLAGS) -o $@ $^

.c.o:
	$(CC) $(CFLAGS) $< -o $@

palette.o : ../../bmp.h

.PHONY : clean

clean:
	-rm -f *.o
	-rm -rf $(EXECUTABLE) $(EXECUTABLE).exe
