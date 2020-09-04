/*
Scans a directory for duplicate images using the dHash algorithm.

Compile like so:
$ gcc -Wall -I /usr/local/include `libpng-config --cflags` -DUSEPNG -DUSEJPG -o imgdup imgdup.c ../bmp.c `libpng-config --ldflags` -lz -ljpeg

References:
    * https://7webpages.com/blog/image-duplicates-detection-python/
    * http://blog.iconfinder.com/detecting-duplicate-images-using-python/
    * http://www.hackerfactor.com/blog/?/archives/529-Kind-of-Like-That.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <dirent.h>

#include "../bmp.h"

#define BM_GET bm_get
#define BM_SET bm_set

void bm_dhash(Bitmap *b, unsigned char bytes[8]) {
    b = bm_copy(b);
    bm_grayscale(b);

    Bitmap * diff = bm_create(9, 8);

    bm_resample_bcub_into(b, diff);

    int x, y;
    for(y = 0; y < 8; y++) {
        bytes[y] = 0;
        for(x = 0; x < 8; x++) {
            int c1 = BM_GET(diff, x, y) & 0xFF;
            int c2 = BM_GET(diff, x + 1, y) & 0xFF;
            if(c1 < c2)
                bytes[y] |= (1 << x);
        }
    }
    bm_free(b);
    bm_free(diff);
}

int hamming_dist(unsigned char bytes1[8], unsigned char bytes2[8]) {
    int sum = 0, x,y;
    for(y = 0; y < 8; y++) {
        unsigned char diff = bytes1[y] ^ bytes2[y];
        for(x = 0; x < 8; x++) {
            if(diff & (1 << x))
                sum++;
        }
    }
    return (64 - sum) * 100 / 64;
}


char *dhash_to_string(unsigned char bytes[8]) {
    static char buffer[17]; /* Not reentrant!!! */
    int i;
    for(i = 0; i < 8; i++) {
        sprintf(buffer + i*2, "%02X", bytes[i]);
    }
    return buffer;
}

typedef struct image_hash {
    char *name;
    unsigned char hash[8];
    struct image_hash *next;
} image_hash;

image_hash *img_list = NULL;

static int verbose = 2;

image_hash *add_image(const char *filename) {
    image_hash *ih;

    if(verbose > 1) {
        printf("hashing '%s' ...\n", filename);
    }

    Bitmap *b = bm_load(filename);
    if(!b) {
        fprintf(stderr, "error reading %s\n", filename);
        return NULL;
    }

    ih = malloc(sizeof *ih);
    ih->name = strdup(filename);

    bm_dhash(b, ih->hash);
    bm_free(b);

    ih->next = img_list;
    img_list = ih;

    //if(verbose) {
        printf("%-20s : %s                \r", filename, dhash_to_string(ih->hash));
    //}

    return ih;
}

void walkdir(const char *path) {
    DIR *dp = opendir(path);
    if(!dp) {
        perror(path);
        return;
    }
    struct dirent *entry;
    while((entry = readdir(dp))) {
        if(entry->d_name[0] == '.') continue;
        char filepath[128];
        snprintf(filepath, sizeof filepath, "%s/%s", path, entry->d_name);
        add_image(filepath);
    }
    closedir(dp);
}

int main(int argc, char *argv[]) {
    int i;
    if(argc < 2) {
        fprintf(stderr, "usage: %s indir\n", argv[0]);
        return 1;
    }

    for(i = 1; i < argc; i++) {
        walkdir(argv[i]);
    }

	printf("\n");
    image_hash *hi, *hj;
    for(hi = img_list; hi->next; hi = hi->next) {
        for(hj = hi->next; hj; hj = hj->next) {
            int d = hamming_dist(hi->hash, hj->hash);
            if(verbose) {
                printf("%-20s vs %-20s: %d%%\n", hi->name, hj->name, d);
            }

			if(d > 80)
				printf("%-20s vs %-20s: %d%%\n", hi->name, hj->name, d);
        }
    }

    /* cleanup */
    while(img_list) {
        image_hash *ih = img_list;
        img_list = ih->next;
        free(ih->name);
        free(ih);
    }

    return 0;
}
