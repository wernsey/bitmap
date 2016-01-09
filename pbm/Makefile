all: pbm

CFLAGS = -O0 -g -Wall -DTEST -I ..
LFLAGS = 

pbm : pbm.o bmp.o
	gcc -o $@ $^ $(LFLAGS)

pbm.o : pbm.c ../bmp.h
	gcc $(CFLAGS) -o $@ -c $<

bmp.o : ../bmp.c ../bmp.h
	gcc -o $@ $(CFLAGS) -c $<
    
clean:
	rm -f *.o pbm.exe pbm *.bmp
