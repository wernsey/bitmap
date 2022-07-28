/*
 *
 * https://en.wikipedia.org/wiki/Glyph_Bitmap_Distribution_Format
 * https://adobe-type-tools.github.io/font-tech-notes/pdfs/5005.BDF_Spec.pdf
 *
 * Some fonts:
 *  * <https://github.com/slavfox/Cozette>
 *  * <https://github.com/Tecate/bitmap-fonts>
 *  * <https://www.cl.cam.ac.uk/~mgk25/ucs-fonts.html>
 *
 * I learned about these sources from [libtcod].
 *
 * [libtcod]: https://github.com/libtcod/libtcod/tree/master/data/fonts#bdf
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>

#include "../bmp.h"

struct bdf_bbox {
    short w,h,x,y;
};

struct bdf_vector {
    short x,y;
};

struct bdf_char {
    unsigned int encoding;
    struct bdf_bbox bbox;
    struct bdf_vector swidth, dwidth, vvector, swidth1, dwidth1;
    uint32_t offset;
};

static int bdf_char_cmp(const void *a, const void *b) {
    return ((struct bdf_char*)a)->encoding - ((struct bdf_char*)b)->encoding;
}

typedef struct  {
    float version;
    struct {
        short pts, xres, yres;
    } size;
    struct bdf_bbox bbox;

    struct bdf_char *chars;
    uint32_t nchars;

    uint32_t abytes, nbytes;
    uint8_t *bytes;

} BdfFont;

static struct bdf_char *find_glyph(BdfFont *bdf, unsigned int glyph) {
    struct bdf_char key;
    key.encoding = glyph;
    return bsearch(&key, bdf->chars, bdf->nchars, sizeof *bdf->chars, bdf_char_cmp);
}

static int bdf_drawglyph(Bitmap *b, BdfFont *bdf, struct bdf_char *glyph, int x, int y, unsigned int col) {
    int i, j;
    uint8_t *bytep = &bdf->bytes[glyph->offset], bit;

    BmRect clip = bm_get_clip(b);

    x += glyph->bbox.x;
    y = y - glyph->bbox.h - glyph->bbox.y;
    for(j = 0; j < glyph->bbox.h; j++) {
        if(y + j < clip.y0) continue;
        if(y + j >= clip.y1) break;
        for(i = 0, bit = 1 << 7; i < glyph->bbox.w; i++, bit >>= 1) {
            if(!bit) {
                bit = 1 << 7;
                bytep++;
            }
            if(x + i < clip.x0) continue;
            if(x + i >= clip.x1) break;
            if(*bytep & bit)
                bm_set(b, x + i, y + j, col);
        }
        bytep++;
    }
    return 1;
}

static int bdf_puts(Bitmap *b, int x, int y, const char *text) {
    const char *ptr = text;
    BmFont *font = bm_get_font(b);
    BdfFont *bdf;
    struct bdf_char *glyph;
    unsigned int col = bm_get_color(b);
    unsigned int codepoint = 0;
    int pen_x = x, pen_y = y;

    if(strcmp(font->type, "BDFFONT"))
		return 0;

    bdf = font->data;

    while((ptr = bm_utf8_next_codepoint(ptr, &codepoint))) {

        if(codepoint == '\n') {
            pen_x = x;
            pen_y += bdf->size.pts;
            continue;
        } else if(codepoint == '\t') {
            glyph = find_glyph(bdf, ' ');
            if(glyph) {
                pen_x += 4 * glyph->dwidth.x;
                pen_y += 4 * glyph->dwidth.y;
            }
            continue;
        }

        glyph = find_glyph(bdf, codepoint);
        if(!glyph) {
            if(!(glyph = find_glyph(bdf, 0xFFFD))
             && !(glyph = find_glyph(bdf, '*')))
                continue;
        }
        bdf_drawglyph(b, bdf, glyph, pen_x, pen_y, col);

        pen_x += glyph->dwidth.x;
        pen_y += glyph->dwidth.y;
    }

    return 1;
}

static int bdf_width(BmFont *font, unsigned int codepoint) {
    if(!font || strcmp(font->type, "BDFFONT"))
        return 0;
    BdfFont *bdf = font->data;
    struct bdf_char *glyph = find_glyph(bdf, codepoint);
    if(!glyph) return 0;
    return glyph->dwidth.x;
}

static int bdf_height(BmFont *font, unsigned int codepoint) {
    if(!font || strcmp(font->type, "BDFFONT"))
        return 0;
    BdfFont *bdf = font->data;
    return bdf->size.pts;
}

static void bdf_measure(BmFont *font, const char *text, int *width, int* height, int* xoffs, int* yoffs) {
    const char *ptr = text;
    BdfFont *bdf;
    struct bdf_char *glyph;
    unsigned int codepoint = 0;

	int max_line_width = 0;
	int cur_line_width = 0;
	int num_lines = 1;

	*width = 0;
	*height = 0;
	*xoffs = 0;
	*yoffs = 0;

    if(strcmp(font->type, "BDFFONT"))
		return;

    bdf = font->data;

    while((ptr = bm_utf8_next_codepoint(ptr, &codepoint))) {

        if(codepoint == '\n') {
            if(cur_line_width > max_line_width) {
				max_line_width = cur_line_width;
			}
			cur_line_width = 0;
			num_lines++;
            continue;
        } else if(codepoint == '\t') {
            glyph = find_glyph(bdf, ' ');
            cur_line_width += 4 * glyph->dwidth.x;
            continue;
        }

        glyph = find_glyph(bdf, codepoint);
        if(!glyph) {
            if(!(glyph = find_glyph(bdf, 0xFFFD))
             && !(glyph = find_glyph(bdf, '*')))
                continue;
        }
        cur_line_width += 4 * glyph->dwidth.x;
    }

    if(cur_line_width > max_line_width) {
		max_line_width = cur_line_width;
	}

    *width = max_line_width;
	*height = bdf->size.pts * num_lines;

	*xoffs = 0;
	*yoffs = - bdf->bbox.h - bdf->bbox.y;
}

static void bdf_dtor(BmFont *font) {
    if(!font || strcmp(font->type, "BDFFONT"))
        return;
    BdfFont *bdffont = font->data;
    if(bdffont) {
        free(bdffont->chars);
        free(bdffont->bytes);
        free(bdffont);
    }
    free(font);
}

static char *bdf_get_line(char *line, size_t linesize, FILE *f) {
    int i;
    do {
        do {
            if(!fgets(line, linesize, f)) {
                return NULL;
            }
        } while (!strncmp(line, "COMMENT", 7));
        for(i = 0; line[i]; i++)
            if(line[i] == '\r' || line[i] == '\n') {
                line[i] = '\0';
                break;
            }
    } while(line[0] == '\0');
    return line;
}

static uint8_t *bdf_add_bytes(BdfFont *bdf, uint8_t *bytes, uint32_t n) {
    uint8_t *r;
    if(bdf->nbytes + n > bdf->abytes) {
        r = bdf->bytes;
        bdf->abytes += bdf->abytes >> 1;
        bdf->bytes = realloc(bdf->bytes, bdf->abytes);
        if(!bdf->bytes) {
            bdf->bytes = r;
            return NULL;
        }
    }
    r = &bdf->bytes[bdf->nbytes];
    memcpy(r, bytes, n);
    bdf->nbytes += n;
    return r;
}

static int bdf_read_vector(struct bdf_vector *vec, char **saveptr) {
    char *tok = bm_strtok_r(NULL, " ", saveptr);
    if(!tok) return 0;
    vec->x = atoi(tok);
    tok = bm_strtok_r(NULL, " ", saveptr);
    if(!tok) return 0;
    vec->y = atoi(tok);
    return 1;
}

BmFont *bm_make_bdf_font(const char *filename) {
    char line[256], *saveptr, *tok;
    float version;
    BmFont *font = NULL;
    BdfFont *bdf = NULL;
    int i;
    uint32_t ccount = 0;

    FILE *f = fopen(filename, "r");
    if(!f) {
        bm_set_error("unable to open file");
        return NULL;
    }
    if(!fgets(line, sizeof line, f)) {
        goto line_error;
    }

    tok = bm_strtok_r(line, " ", &saveptr);
    if(strcmp(tok, "STARTFONT")) {
        goto bad_font;
    }
    tok = bm_strtok_r(NULL, " ", &saveptr);
    if(!tok) {
        goto bad_font;
    }
    version = atof(tok);

    if(!(font = malloc(sizeof *font)) || !(bdf = malloc(sizeof *bdf))) {
        bm_set_error("out of memory");
        goto error;
    }
    font->type = "BDFFONT";
    font->ref_count = 1;
    font->puts = bdf_puts;
    font->width = bdf_width;
    font->height = bdf_height;
    font->measure = bdf_measure;
    font->dtor = bdf_dtor;
    font->data = bdf;

    memset(bdf, 0, sizeof *bdf);
    bdf->version = version;
    bdf->abytes = 256;
    bdf->bytes = malloc(bdf->abytes);

    for(;;) {
        if(!bdf_get_line(line, sizeof line, f)) {
            goto line_error;
        }
        tok = bm_strtok_r(line, " ", &saveptr);
        if(!tok) goto bad_font;
        if(!strcmp(tok, "ENDFONT")) {
            break;
        } else if(!strcmp(tok, "SIZE")) {
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->size.pts = atoi(tok);
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->size.xres = atoi(tok);
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->size.yres = atoi(tok);
        } else if(!strcmp(tok, "FONTBOUNDINGBOX")) {
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->bbox.w = atoi(tok);
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->bbox.h = atoi(tok);
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->bbox.x = atoi(tok);
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            bdf->bbox.y = atoi(tok);
        } else if(!strcmp(tok, "STARTPROPERTIES")) {
            int nprops;
            tok = bm_strtok_r(NULL, " ", &saveptr);
            if(!tok) goto bad_font;
            nprops = atoi(tok);
            if(nprops <= 0) goto bad_font;
            for(i = 0;;i++) {
                if(!bdf_get_line(line, sizeof line, f)) {
                    goto line_error;
                }
                tok = bm_strtok_r(line, " ", &saveptr);
                if(!tok)
                    goto bad_font;
                else if(!strcmp(tok, "ENDPROPERTIES"))
                    break;
                /* printf("property %d: '%s' = '%s'\n", i, tok, saveptr); */
            }
        } else if(!strcmp(tok, "CHARS")) {
            if(bdf->chars || bdf->nchars > 0)
                goto bad_font;
            bdf->nchars = atoi(saveptr);
            if(bdf->nchars <= 0) {
                goto bad_font;
            }
            bdf->chars = malloc(bdf->nchars * sizeof *bdf->chars);
            if(!bdf->chars) {
                bm_set_error("out of memory");
                goto error;
            }
        } else if(!strcmp(tok, "STARTCHAR")) {
            struct bdf_char *chr;
            uint32_t c, t, offset = bdf->nbytes;

            if(ccount >= bdf->nchars)
                goto bad_font;
            chr = &bdf->chars[ccount++];

            chr->encoding = -1;
            memset(chr, 0, sizeof *chr);

            for(;;) {
                if(!bdf_get_line(line, sizeof line, f)) {
                    goto line_error;
                }
                tok = bm_strtok_r(line, " ", &saveptr);
                if(!strcmp(tok, "ENDCHAR")) break;
                else if(!strcmp(tok, "ENCODING")) {
                    chr->encoding = atoi(saveptr);
                } else if(!strcmp(tok, "BBX")) {
                    tok = bm_strtok_r(NULL, " ", &saveptr);
                    if(!tok) goto bad_font;
                    chr->bbox.w = atoi(tok);
                    tok = bm_strtok_r(NULL, " ", &saveptr);
                    if(!tok) goto bad_font;
                    chr->bbox.h = atoi(tok);
                    tok = bm_strtok_r(NULL, " ", &saveptr);
                    if(!tok) goto bad_font;
                    chr->bbox.x = atoi(tok);
                    tok = bm_strtok_r(NULL, " ", &saveptr);
                    if(!tok) goto bad_font;
                    chr->bbox.y = atoi(tok);
                } else if(!strcmp(tok, "SWIDTH")) {
                    if(!bdf_read_vector(&chr->swidth, &saveptr))
                        goto bad_font;
                } else if(!strcmp(tok, "DWIDTH")) {
                    if(!bdf_read_vector(&chr->dwidth, &saveptr))
                        goto bad_font;
                } else if(!strcmp(tok, "VVECTOR")) {
                    if(!bdf_read_vector(&chr->vvector, &saveptr))
                        goto bad_font;
                } else if(!strcmp(tok, "SWIDTH1")) {
                    if(!bdf_read_vector(&chr->swidth1, &saveptr))
                        goto bad_font;
                } else if(!strcmp(tok, "DWIDTH1")) {
                    if(!bdf_read_vector(&chr->dwidth1, &saveptr))
                        goto bad_font;
                } else if(!strcmp(tok, "BITMAP")) {
                    unsigned char bytes[16];

                    if(chr->bbox.h < 0) goto bad_font;
                    for(i = 0, c = 0; i < chr->bbox.h; i++) {
                        if(!bdf_get_line(line, sizeof line, f)) goto line_error;

                        for(t = 0; line[t]; t++) {
                            int x = tolower(line[t]);
                            if(x >= 'a' && x <= 'f') {
                                bytes[c] = (bytes[c] << 4) + (x - 'a' + 10);
                            } else {
                                bytes[c] = (bytes[c] << 4) + (x - '0');
                            }
                            if(t & 0x1) {
                                c++;
                                if(c == sizeof(bytes)) {
                                    if(!bdf_add_bytes(bdf, bytes, c)) {
                                        bm_set_error("out of memory");
                                        goto error;
                                    }
                                    c = 0;
                                }
                            }
                        }
                    }

                    if(c > 0) {
                        if(!bdf_add_bytes(bdf, bytes, c)) {
                            bm_set_error("out of memory");
                            goto error;
                        }
                    }

                    chr->offset = offset;
                }
            }

            /* The Cozette font has some glyphs at the
            end of the file for which the encoding is -1.
            I couldn't figure out what to do in this case.
            It seems you can derive the encoding from the name,
            but I don't know if this is standard. */
            if(chr->encoding < 0)
                ccount--;

            /*
            printf("SWIDTH ...: %d %d\n", chr->swidth.x, chr->swidth.y);
            printf("DWIDTH ...: %d %d\n", chr->dwidth.x, chr->dwidth.y);
            printf("SWIDTH1 ..: %d %d\n", chr->swidth1.x, chr->swidth1.y);
            printf("DWIDTH1 ..: %d %d\n", chr->dwidth1.x, chr->dwidth1.y);
            printf("VVECTOR ..: %d %d\n", chr->vvector.x, chr->vvector.y);
            */
        }
    }

    if(ccount < bdf->nchars)
        bdf->nchars = ccount;

    qsort(bdf->chars, bdf->nchars, sizeof *bdf->chars, bdf_char_cmp);

    /*
    for(i = 0; i < bdf->nchars; i++) {
        struct bdf_char *chr = &bdf->chars[i];
        printf("%d glyph %d: +%u; %d %d %d %d\n", i, chr->encoding, chr->offset, chr->bbox.w, chr->bbox.h, chr->bbox.x, chr->bbox.y);
    }
    */

    fclose(f);
    return font;

bad_font:
    bm_set_error("bad font");
    goto error;
line_error:
    bm_set_error("unexpected end of file");
error:
    fclose(f);
    bdf_dtor(font);
    return NULL;
}

int main(int argc, char *argv[]) {

    const char * fontfile = "cozette.bdf";
    if(argc > 1) {
        fontfile = argv[1];
    }
    BmFont *font = bm_make_bdf_font(fontfile);
    if(!font) {
        fprintf(stderr, "unable to load %s: %s\n", fontfile, bm_get_error());
        return 1;
    }

    BdfFont *bdf = font->data;
    struct bdf_char * chr = find_glyph(bdf, 'A');
    if(!chr) {
        fprintf(stderr, "Couldn't find 'A's glyph\n");
        return 1;
    }

    printf("glyph 'A' @ %u\n", chr->offset);
    assert(chr->offset < bdf->nbytes);
    int x, y;
    uint8_t *bytep = &bdf->bytes[chr->offset];
    for(y = 0; y < chr->bbox.h; y++) {
        uint8_t bit = 1 << 7;
        for(x = 0; x < chr->bbox.w; x++) {
            if(!bit) {
                bit = 1 << 7;
                bytep++;
            }
            if(*bytep & bit)
                putc('#', stdout);
            else
                putc('.', stdout);
            bit >>= 1;
        }
        bytep++;
        putc('\n', stdout);
    }

    Bitmap *bmp = bm_create(800, 600);
    bm_set_font(bmp, font);

    int Y = 20;

    bm_set_color(bmp, 0x550000);
    bm_line(bmp, 0, Y, 800, Y);
    bm_set_color(bmp, 0xFFFFFF);

    /* Some UTF-8 strings from here:
    https://www.w3.org/2001/06/utf-8-test/UTF-8-demo.html */
    bm_puts(bmp, 10, Y, "the quick brown fox jumps over the lazy dog"
        "\nTHE QUICK BROWN FOX JUMPS OVER THE LAZY DOG"
        "\n0123456789\ttab {}[];'\""
        "\n∂ ∆ ∏ ∑ − ∕ ∙ √ ∞ ∟ ∩ ∫ ≈ ≠ ≡ ≤ ≥ € 𝄞"
        "\nЗарегистрируйтесь сейчас на Десятую Международную"
        "\nᚻᛖ ᚳᚹᚫᚦ ᚦᚫᛏ ᚻᛖ ᛒᚢᛞᛖ ᚩᚾ ᚦᚫᛗ ᛚᚪᚾᛞᛖ ᚾᚩᚱᚦᚹᛖᚪᚱᛞᚢᛗ ᚹᛁᚦ ᚦᚪ ᚹᛖᛥᚫ"
        "\n⡌⠁⠧⠑ ⠼⠁⠒  ⡍⠜⠇⠑⠹⠰⠎ ⡣⠕⠌"
        "\n–—‘“”„†•…‰™œŠŸž€ ΑΒΓΔΩαβγδω АБВГДабвгд"
        "\n∀∂∈ℝ∧∪≡∞ ↑↗↨↻⇣ ┐┼╔╘░►☺♀ ﬁ�⑀₂ἠḂӥẄɐː⍎אԱა"
        "\n∮ E⋅da = Q,  n → ∞, ∑ f(i) = ∏ g(i), ∀x∈ℝ: ⌈x⌉ = −⌊−x⌋, α ∧ ¬β = ¬(¬α ∨ β),"
        "\nℕ ⊆ ℕ₀ ⊂ ℤ ⊂ ℚ ⊂ ℝ ⊂ ℂ, ⊥ < a ≠ b ≡ c ≤ d ≪ ⊤ ⇒ (A ⇔ B),"
        "\n2H₂ + O₂ ⇌ 2H₂O, R = 4.7 kΩ, ⌀ 200 mm"
        "\n▲△▴▵▶▷▸▹►▻▼▽▾▿◀◁◂◃◄◅ ✚✘●➦☰☲─"
        "\nαιϱβκσγλςδμτϵνυεξϕζοφηπχθϖψϑρω ΓΞΦΔΠΨΘΣΩΛΥ"
        "\nℵ′∀ℏ∅∃ı∇¬j√♭ℓ⊤♮℘⊥♯ℜ∥♣ℑ∠♢∂△e♡∞♠□◇ ∑⋂⨀∏⋃⨂∐⨆⨁∫⋁⨄∮⋀"
        "\n±∩∨∓∪∧∖⊎⊕⋅⊓⊖×⊔⊗∗◁⊘⋆▷⊙⋄≀†∘◯‡∙△⨿÷▽⊴⊲⊳⊵"
        "\n≤≥≡≺≻∼≼≽≃≪≫≍⊂⊃≈⊆⊇≅∈∋∝⊢⊣⊨⌣∣≐"
        "\n∥⊥⊏⊐⨝⊑⊒⋈ ≮≯≠≰≱≢⊀⊁≁⋠⋡≄⊄⊅≉⊈⊉≇⋢⋣≭"
        "\n←⟵↑⇐⟸ ⇑→⟶↓⇒⟹⇓↩↪↘↼⇀↙↽⇁↖⇌↝ ↔⟷↕⇔⟺⇕↦⟼↗"
        "\n[⌊⌈⟦{⟨⟪]⌋⌉⟧}⟩⟫ ≠≤≥{}→←∋∧∨¬∣∥⋮⋯⋱"
        "\n’‘”“‐–—−′″‴⁗ﬀﬁﬂﬃﬄ¡¿œŒæÆåÅøØłŁß§¶†‡©£…"
        );

    /*
    unsigned int cp;
    bm_utf8_next_codepoint("\uFFFD", &cp);
    printf("%u 0x%X\n",cp, cp);
    */

    bm_save(bmp, "test.bmp");
    bm_free(bmp);

    bm_font_release(font);

    return 0;
}