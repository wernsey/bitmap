int bm_to_Xbm(Bitmap *b, const char *name) {
    int x, y, bit = 0, byte = 0;
    //unsigned int c = bm_get_color(b) & 0x00FFFFFF;
    char fname[30];
    snprintf(fname, sizeof fname, "%s.xbm", name);
    FILE *f = fopen(fname, "w");
    if(!f) return 0;
    fprintf(f, "#define %s_width  %3d\n", name, b->w);
    fprintf(f, "#define %s_height %3d\n", name, b->h);
    fprintf(f, "static unsigned char %s_bits[] = {\n", name);
    for(y = 0; y < b->h; y++) {
        fputs("  ", f);
        for(x = 0, bit = 0, byte = 0; x < b->w; x++) {
            //if((bm_get(b, x, y) & 0x00FFFFFF) != c)
            if(!bm_get(b, x, y))
                byte |= (1 << bit);
            if(++bit == 8) {
                if(x == b->w - 1 && y == b->h - 1)
                    fprintf(f, "0x%02x", byte);
                else
                    fprintf(f, "0x%02x,", byte);
                bit = 0;
                byte = 0;
            }
        }
        if(bit) {
            if(y == b->h - 1)
                fprintf(f, "0x%02x\n", byte);
            else
                fprintf(f, "0x%02x,\n", byte);
        } else {
            fputs("\n", f);
        }
    }
    fprintf(f, "};\n");
    return 1;
}
