/*
Small program to convert Netpbm files to bitmap images.
http://en.wikipedia.org/wiki/Netpbm_format
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bmp.h"

static char *readfile(const char *fname) {
	FILE *f;
	long len,r;
	char *str;
	
	if(!(f = fopen(fname, "rb")))	
		return NULL;
	
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	rewind(f);
	
	if(!(str = malloc(len+2)))
		return NULL;	
	r = fread(str, 1, len, f);
	
	if(r != len) {
		free(str);
		return NULL;
	}	
	fclose(f);	
	str[len] = '\0';
	return str;
}

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

struct bitmap *parse_pbm(const char *filename) {
    struct bitmap *bm = NULL;
    char *s = readfile(filename);
    char *p, *r;
    int type = 0, w, h, d = 1, x, y;
    if(!s) {
        fprintf(stderr, "error: Unable open %s\n", filename);
        return NULL;
    }
    p = tokenize_pbm(s, &r);
    if(!p) {
        fprintf(stderr, "error: Couldn't determine type\n");
        goto error;
    }
    if(!strcmp(p, "P1")) {
        type = 1;
    } else if(!strcmp(p, "P2")) {
        type = 2;
    } else if(!strcmp(p, "P3")) {
        type = 3;
    } else {
        fprintf(stderr, "error: Invalid type %s\n", p);
        goto error;
    }
        
#define GET_INT(v, error_msg)   do{ if(!(p = tokenize_pbm(NULL, &r))) { \
                                    fprintf(stderr, "error: %s\n", error_msg); \
                                    goto error;\
                                }\
                                v = atoi(p); } while(0)
    
    GET_INT(w, "Bad width");
    GET_INT(h, "Bad height");
    
    if(type > 1) {
        GET_INT(d, "Bad depth");
    }
    
    if(w <= 0 || h <= 0 || d <= 0) {
        fprintf(stderr, "error: invalid dimensions\n");
        goto error;
    }
    
    printf("Creating bitmap %d x %d @ %d\n", w, h, d);
    
    bm = bm_create(w,h);
    
    for(y = 0; y < h; y++) {
        for(x = 0; x < w; x++) {
            int pr,pg,pb, c;
            switch(type) {
                case 1: 
                case 2: 
                    GET_INT(pr, "Bad value");
                    pr = pr * 255 / d;
                    pg = pr;
                    pb = pr;
                    break;
                case 3:
                    GET_INT(pr, "Bad R value");
                    pr = pr * 255 / d;
                    GET_INT(pg, "Bad G value");
                    pg = pg * 255 / d;
                    GET_INT(pb, "Bad B value");
                    pb = pb * 255 / d;
                    break;
            }
			c = 0xFF000000 | (pr << 16) | (pg << 8) | pb;
            bm_set(bm, x, y, c);
        }
    }
    
    goto done;
error:
    if(bm) bm_free(bm);
done:
    free(s);
    return bm;
}

#ifdef TEST
int main(int argc, char *argv[]) {
    int i;
    for(i = 1; i < argc; i++) {
        struct bitmap *b = parse_pbm(argv[i]);
        if(b) {
            bm_save(b, "out.bmp");
            bm_free(b); 
        } else {
            fprintf(stderr, "error: Unable to process %s\n", argv[i]);
        }
    }
    
    return 0;
}
#endif
