typedef enum {
    BLACK = 0,
    DARK_BLUE,
    DARK_GREEN,
    DARK_CYAN,
    DARK_RED,
    DARK_MAGENTA,
    BROWN,
    LIGHT_GRAY,
    GRAY,
    BLUE,
    GREEN,
    CYAN,
    RED,
    MAGENTA,
    YELLOW,
    WHITE
} COLOR;

typedef struct {
    int r, c;
    COLOR fg, bg;
    unsigned short *data;
} Grid;

Grid *gr_create(int cols, int rows);

void gr_free(Grid *g);

void gr_foreground(Grid *g, COLOR fg);

void gr_background(Grid *g, COLOR bg);

void gr_set(Grid *g, int x, int y, unsigned char c);

void gr_set_raw(Grid *g, int x, int y, unsigned short v);

unsigned short gr_get_raw(Grid *g, int x, int y);

void gr_draw(Grid *g, Bitmap *b, int xo, int yo);

void gr_dbox(Grid *g, int x0, int y0, int x1, int y1);

void gr_box(Grid *g, int x0, int y0, int x1, int y1);

int gr_puts(Grid *g, int x, int y, const char *s);

int gr_printf(Grid *g, int x, int y, const char *fmt, ...);

void draw_legend(const char *outfile);