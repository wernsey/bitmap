/*
Wrapper around FreeType (http://www.freetype.org/) to allow rendering of 
freetype-supported fonts on Bitmaps from my bitmap module (as defined in bmp.h)
http://www.freetype.org/freetype2/docs/tutorial/step1.html
*/
#include <stdio.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#if defined(USESDL)
#  include <SDL2/SDL.h>
#endif

#include "bmp.h"
#include "ftfont.h"

static FT_Library library;

typedef struct bmf_font {
	char *name;
	BmFont *font;
	struct bmf_font *next;
} BmfFont;

static BmfFont *fonts = NULL;

int bmf_init() {
	int error;
	fonts = NULL;
	error = FT_Init_FreeType(&library);
	if(error) {
		return 0;
	}
	return 1;
}

static void deinit_font(BmfFont *bmf) {
	if(!bmf) return;
	if(bmf->next) 
		deinit_font(bmf->next);
	FT_Done_Face((FT_Face)(bmf->font->data));
	free(bmf->font);
	free(bmf->name);
	free(bmf);
}

void bmf_deinit() {
	deinit_font(fonts);
	FT_Done_FreeType(library);
}

static int bmf_puts(Bitmap *bmp, int pen_x, int pen_y, const char *text);

static int bmf_width(struct bitmap_font *font) {
	FT_Face face;
	if(strcmp(font->type, "FreeType"))
		return 0;				
	face = font->data;	
	return face->size->metrics.max_advance >> 6;
}

static int bmf_height(struct bitmap_font *font) {
	FT_Face face;
	if(strcmp(font->type, "FreeType"))
		return 0;
		
	face = font->data;
	return face->size->metrics.height/64;
}	

static BmFont *make_bmf_font(FT_Face face) {
	BmFont *font = malloc(sizeof *font);
	font->type = "FreeType";
	font->puts = bmf_puts;
	font->width = bmf_width; 
	font->height = bmf_height; 
	font->data = face;
	return font;
}

BmFont *bmf_load_font(const char *file) {
	int error;
	BmfFont *bmf;
	FT_Face face;
	
	if(!file) 
		return NULL;
		
	for(bmf = fonts; bmf; bmf = bmf->next) {
		if(bmf->name && !strcmp(bmf->name, file))
			return bmf->font;
	}
	
	bmf = malloc(sizeof *bmf);
	
	error = FT_New_Face(library, file, 0, &face);
	if(error == FT_Err_Unknown_File_Format) {
		free(bmf);
		return NULL;
	} else if(error) {
		free(bmf);
		return NULL;
	}
		
	bmf->name = strdup(file);
	bmf->font = make_bmf_font(face);
	
	bmf->next = fonts;
	fonts = bmf;
	
	return bmf->font;
}

#ifdef USESDL

/* 
The only guidelines I could find on the web on how to use this:
http://marc.info/?l=freetype&m=106967222917595&w=2
and SDL_ttf's source at http://hg.libsdl.org/SDL_ttf/file/b773c1cd55a2/SDL_ttf.c 
*/
static unsigned long rw_stream_read(
	FT_Stream stream,
	unsigned long   offset,
	unsigned char*  buffer,
	unsigned long   count) {
	
	SDL_RWops *rw = stream->descriptor.pointer;;
	SDL_RWseek(rw, offset, RW_SEEK_SET);	
	if(count == 0) return 0;
	return SDL_RWread(rw, buffer, 1, count);	
}
	  
static FT_Face face_from_rwops(SDL_RWops *rw) {
	FT_Open_Args open_args;
	FT_Stream rw_stream;
	int position, error;
	FT_Face face;
	
	rw_stream = malloc(sizeof *rw_stream);
	memset(rw_stream, 0, sizeof *rw_stream);
	
	position = SDL_RWtell(rw);
	if(position < 0) {
		return NULL;
	}
	
	rw_stream->pos = position;
	rw_stream->size = SDL_RWsize(rw) - rw_stream->pos;		
	rw_stream->descriptor.pointer = rw;
	rw_stream->read               = rw_stream_read;
	rw_stream->memory = NULL;
	
	memset(&open_args, 0, sizeof open_args);
	
	open_args.flags = FT_OPEN_STREAM;
	open_args.stream = rw_stream;
	
	error = FT_Open_Face(library, &open_args, 0, &face);	
	if(error) {
		return NULL;
	} else {
		return face;
	}
}

BmFont *bmf_load_font_rw(SDL_RWops *rw, const char *name) {
	BmfFont *bmf;
	FT_Face face;
	if(name) {
		for(bmf = fonts; bmf; bmf = bmf->next) {
			if(bmf->name && !strcmp(bmf->name, name))
				return bmf->font;
		}
	}
	
	bmf = malloc(sizeof *bmf);
	
	face = face_from_rwops(rw);
	if(!face) {
		free(bmf);
		return NULL;
	}
		
	if(name)
		bmf->name = strdup(name);
	else
		bmf->name = NULL;
		
	bmf->font = make_bmf_font(face);
	bmf->next = fonts;
	fonts = bmf;
	
	return bmf->font;
}
#endif

int bmf_set_size(BmFont *font, int px) {
	FT_Face face;
	int error;
	
	if(strcmp(font->type, "FreeType"))
		return 0;
		
	face = font->data;
	
	/* There are a couple of ways in which you can set the size of the font:*/
	/*error = FT_Set_Char_Size(face, 0, 16 * 64, 0, 0);*/
	error = FT_Set_Pixel_Sizes(face, 0, px);
	if(error) {
		/*fprintf(stderr, "Error setting face sizes (%d).\n", error);*/
		return 0;
	}
	return 1;
}

static void ft_draw_bitmap(Bitmap *bm, FT_Bitmap *fb, FT_Int x, FT_Int y) {
	FT_Int i, j, p, q;
	FT_Int x_max = x + fb->width;
	FT_Int y_max = y + fb->rows;
	
	unsigned char r, g, b;
	unsigned int color = bm_get_color(bm);
	bm_get_rgb(color, &r, &g, &b);
	
	for(i = x, p = 0; i < x_max; i++, p++) {
		if(i < 0) 
			continue;
		if(i >= bm->w) 
			break;
		for(j = y, q = 0; j < y_max; j++, q++) {
			unsigned int c;
			unsigned char r1, g1, b1;
			if(j < 0)
				continue;
			if(j >= bm->h)
				break;
				
			c = bm_get(bm, i, j);
			bm_get_rgb(c, &r1, &g1, &b1);
			
			c = fb->buffer[q * fb->width + p];			
			r1 = ((255 - c) * r1 + c * r) / 255;
			if(r1 > 255) r1 = 255;
			g1 = ((255 - c) * g1 + c * g) / 255;
			if(g1 > 255) g1 = 255;
			b1 = ((255 - c) * b1 + c * b) / 255;
			if(b1 > 255) b1 = 255;
			
			bm_set(bm, i, j, r1 << 16 | g1 << 8 | b1 << 0);
		}
	}
}

static int bmf_puts(Bitmap *bmp, int pen_x, int pen_y, const char *text) {	
	FT_Face face;
	BmFont *font = bmp->font;
	FT_Int error, x_start = pen_x;
	
	FT_GlyphSlot slot;
	FT_Long size;
	const char *i = text;
	
	if(strcmp(font->type, "FreeType"))
		return 0;
		
	face = font->data;
	slot = face->glyph;	
	size = face->size->metrics.height/64;
	
	for(i = text; *i; i++) {
		if(*i == '\n') {
			pen_x = x_start;
			pen_y += size;
			continue;
		}
		
		error = FT_Load_Char(face, *i, FT_LOAD_RENDER);
		if(error) {
			/*fprintf(stderr, "Error loading char '%c' (%d).\n", *i, error);*/
			continue;
		}
		
		ft_draw_bitmap(bmp, &slot->bitmap, pen_x + slot->bitmap_left, pen_y - slot->bitmap_top);
		
		pen_x += slot->advance.x >> 6;
		pen_y += slot->advance.y >> 6;
	}
	return 1;
}
