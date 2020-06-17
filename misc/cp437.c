/*
 * Toy program for drawing bitmaps with Code Page 437 type
 * graphics.
 *
 * Compile like so:
 *  $ gcc -DCP437_MAIN cp437.c ../bmp.c
 *
 *    See https://en.wikipedia.org/wiki/Code_page_437
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

#include "../bmp.h"
#include "cp437.h"
#include "cp437.xbm"

static void draw_tile(Bitmap *b, int x, int y, unsigned short c);

static unsigned int cga_colors[] = {
    /* actually based on https://en.wikipedia.org/wiki/Web_colors#HTML_color_names */
    0x000000, // 0 (black)
    0x000080, // 1 (low blue)
    0x008000, // 2 (low green)
    0x008080, // 3 (low cyan)
    0x800000, // 4 (low red)
    0x800080, // 5 (low magenta)
    0x808000, // 6 (brown)
    0xC0C0C0, // 7 (light gray)
    0x808080, // 8 (dark gray)
    0x0000FF, // 9 (high blue)
    0x00FF00, // 10 (high green); green
    0x00FFFF, // 11 (high cyan); cyan
    0xFF0000, // 12 (high red)
    0xFF00FF, // 13 (high magenta); magenta
    0xFFFF00, // 14 (yellow)
    0xFFFFFF, // 15 (white)
};

Grid *gr_create(int cols, int rows) {
    Grid *g = malloc(sizeof *g);
    g->r = rows;
    g->c = cols;
    g->data = calloc(rows * cols, sizeof *g->data);
    return g;
}

void gr_free(Grid *g) {
    free(g->data);
    free(g);
}

void gr_foreground(Grid *g, COLOR fg) {
    g->fg = fg & 0x0F;
}
void gr_background(Grid *g, COLOR bg) {
    g->bg = bg & 0x0F;
}
void gr_set_raw(Grid *g, int x, int y, unsigned short v);

void gr_set(Grid *g, int x, int y, unsigned char c) {
    unsigned short v = (c & 0xFF) | (g->fg << 8) | (g->bg << 12);
    gr_set_raw(g, x, y, v);
}

void gr_set_raw(Grid *g, int x, int y, unsigned short v) {
    if(x < 0 || x >= g->c || y < 0 || y >= g->r)
        return;
    g->data[y * g->c + x] = v;
}

unsigned short gr_get_raw(Grid *g, int x, int y) {
    if(x < 0 || x >= g->c || y < 0 || y >= g->r)
        return 0;
    return g->data[y * g->c + x];
}

void gr_draw(Grid *g, Bitmap *b, int xo, int yo) {
    int x, y;
    for(y = 0; y < g->r; y++) {
        for(x = 0; x < g->c; x++) {
            draw_tile(b, xo + x * 8, yo + y * 8, gr_get_raw(g, x, y));
        }
    }
}

static void draw_tile(Bitmap *b, int x, int y, unsigned short c) {
    int i, j;
    int row, col, byte;
    row = (c >> 4) & 0x0F;
    col = c & 0x0F;
    byte = (row * 128 + col);
    unsigned int fgc = cga_colors[(c >> 8) & 0x0F];
    unsigned int bgc = cga_colors[(c >> 12) & 0x0F];
    for(j = 0; j < 8; j++) {
        if(y + j < b->clip.y0) continue;
        else if(y + j >= b->clip.y1) break;
        for(i = 0; i < 8; i++) {
            if(x + i < b->clip.x0) continue;
            else if(x + i >= b->clip.x1) break;
            if(cp437_bits[byte] & (1 << i)) {
                bm_set(b, x + i, y + j, bgc);
            } else {
                bm_set(b, x + i, y + j, fgc);
            }
        }
        byte += 128/8;
    }
}

static void gr_ibox(Grid *g, int x0, int y0, int x1, int y1, unsigned char border[6]) {
    int i;
    assert(x0 < x1);
    assert(y0 < y1);
    for(i = x0; i <= x1; i++) {
        gr_set(g, i, y0, border[0]);
        gr_set(g, i, y1, border[0]);
    }
    for(i = y0; i <= y1; i++) {
        gr_set(g, x0, i, border[1]);
        gr_set(g, x1, i, border[1]);
    }
    gr_set(g, x0, y0, border[2]);
    gr_set(g, x1, y0, border[3]);
    gr_set(g, x0, y1, border[4]);
    gr_set(g, x1, y1, border[5]);
}

void gr_dbox(Grid *g, int x0, int y0, int x1, int y1) {
    static unsigned char border[] = {0xCD, 0xBA, 0xC9, 0xBB, 0xC8, 0xBC};
    gr_ibox(g, x0, y0, x1, y1, border);
}

void gr_box(Grid *g, int x0, int y0, int x1, int y1) {
    static unsigned char border[] = {0xC4, 0xB3, 0xDA, 0xBF, 0xC0, 0xD9};
    gr_ibox(g, x0, y0, x1, y1, border);
}

int gr_puts(Grid *g, int x, int y, const char *s) {
    int x0 = x;
    while(*s) {
        if(*s == '\n') {
            y++;
            x = x0;
        } else if(*s == '\r') {
            x = x0;
        } else if(*s == '\t') {
            x = (x + 4) & 0xFC;
        } else
            gr_set(g, x++, y, *s);
        s++;
    }
}

int gr_printf(Grid *g, int x, int y, const char *fmt, ...) {
    char buffer[256];
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(buffer, sizeof buffer, fmt, arg);
    va_end(arg);
    return gr_puts(g, x, y, buffer);
}

void draw_legend(const char *outfile) {
    int x, y, a = 0;
    Bitmap *b = bm_create(16 * 8 + 10 + 16, 16 * 8 + 10);

    Grid *g = gr_create(16, 16);

    for(y = 0; y < 16; y++) {
        gr_background(g, y % 2 ? BLACK : DARK_BLUE);
        for(x = 0; x < 16; x++) {
            gr_foreground(g, x % 2 ? LIGHT_GRAY : WHITE);
            gr_set(g, x, y, a++);
        }
    }

    gr_draw(g, b, 10, 10);
    for(y = 0; y < 16; y++) {
        bm_set_color(b, y % 2 ? cga_colors[LIGHT_GRAY] : cga_colors[WHITE]);
        bm_printf(b, y * 8 + 10, 1, "%X", y);
        bm_printf(b, 1, y * 8 + 10, "%X", y);
        bm_set_color(b, cga_colors[y]);
        bm_fillrect(b, 16 * 8 + 10, y * 8 + 10, 16 * 8 + 10 + 15, (y + 1) * 8 - 1 + 10);
        if(y < 6 || y == 8 || y == 9)
            bm_set_color(b, cga_colors[WHITE]);
        else
            bm_set_color(b, cga_colors[BLACK]);
        bm_printf(b, 16 * 8 + 11 + 4, y * 8 + 11, "%X", y);
    }
    bm_set_color(b, cga_colors[LIGHT_GRAY]);
    bm_line(b, 0, 9, b->w, 9);
    bm_line(b, 9, 0, 9, b->h);


    gr_free(g);

    /* Set the bitmap b's color to something that is not in the pallette,
    otherwise that palette colour ends up being transparent if you save to
    a GIF. Should I do something about it? */
    bm_set_color(b, 1);

    bm_save(b, outfile);
    bm_free(b);
}

#ifdef CP437_MAIN

#define GRID_W 20
#define GRID_H 16
int main(int argc, char *argv[]) {

    draw_legend("out-legend.gif");

    int x, y, a = 0;
    Bitmap *b = bm_create(GRID_W * 8, GRID_H * 8);

    Grid *g = gr_create(GRID_W, GRID_H);

    gr_background(g, BLACK);
    gr_foreground(g, LIGHT_GRAY);
#if 0
    for(y = 0; y < GRID_H; y++) {
        gr_background(g, y);
        for(x = 0; x < GRID_W; x++) {
            gr_foreground(g, x);
            gr_set(g, x, y, a++);
        }
    }
#else
    gr_box(g, 2, 4, 10, 12);
    gr_dbox(g, 12, 2, 18, 14);

    gr_printf(g, 1, 0, "Printf test\nnewline, %X", 123);
    gr_printf(g, 0, 4, "aaaa\tbb\tbb\nccc\tddddd\n\tyyy");
#endif
    gr_draw(g, b, 0, 0);

    gr_free(g);

    bm_save(b, "out.gif");
    bm_free(b);

    return 0;
}
#endif
