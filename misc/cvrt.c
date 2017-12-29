#include "../bmp.h"

int main(int argc, char *argv[]) {
    Bitmap *b;
    if(argc > 1) {
        b = bm_load(argv[1]);
        if(argc > 2) {
            bm_save(b, argv[2]);
        }
        bm_free(b);
    }
}