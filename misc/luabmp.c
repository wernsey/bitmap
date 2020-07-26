#include <lua.h>
#include <lauxlib.h>
#include <lualib.h> 

#include "bmp.h"

/** # luabmp.c
 * 
 * [Lua][] bindings for the bitmap library.
 * 
 * [Lua]: https://www.lua.org
 */

/**
 * ## Static functions
 */

/** ### `Bitmap.create(W, H)`
 * Creates a bitmap object of the specified size
 */
static int bmp_create(lua_State *L) {
	int w = luaL_checknumber(L,1);
	int h = luaL_checknumber(L,2);
	
	Bitmap **bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "Bitmap");
	
	*bp = bm_create(w, h);
	if(!*bp) {
		luaL_error(L, "Unable to create bitmap");
	}
	return 1;
}

/** ### `Bitmap.load(filename)`
 * Loads the bitmap file specified by `filename`
 */
static int bmp_load(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	
	Bitmap **bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "Bitmap");
	
	*bp = bm_load(filename);
	if(!*bp) {
		luaL_error(L, "Unable to load bitmap '%s': %s", filename, bm_get_error());
	}
	return 1;
}

/** ### `Bitmap.Font.loadRaster(filename, spacing)`
 * Loads the raster font specified by `filename`
 */
static int bmf_load_raster(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	int spacing;
	
	if(lua_gettop(L) > 1)
	 	spacing = luaL_checknumber(L, 2);
	else 
		spacing = 8;
	
	BmFont **bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "BitmapFont");
	
	*bp = bm_make_ras_font(filename, spacing);
	if(!*bp) {
		luaL_error(L, "Unable to load raster font '%s': %s", filename, bm_get_error());
	}
	return 1;
}

/** ### `Bitmap.Font.loadSFont(filename)`
 * Loads the [SFont][sfont] specified by `filename`
 * 
 * [sfont]: http://www.linux-games.com/sfont/
 */
static int bmf_load_sfont(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	
	BmFont **bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "BitmapFont");
	
	*bp = bm_make_sfont(filename);
	if(!*bp) {
		luaL_error(L, "Unable to load SFont '%s': %s", filename, bm_get_error());
	}
	return 1;
}

/**
 * ## The `Bitmap` object
 */

/** ### `Bitmap:__tostring()`
 *  Returns a string representation of the `Bitmap` instance.
 */
static int bmp_tostring(lua_State *L) {	
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	Bitmap *b = *bp;
	lua_pushfstring(L, "Bitmap[%dx%d]", b->w, b->h);
	return 1;
}

/** ### `Bitmap:__gc()`
 *  Garbage collects the `Bitmap` instance.
 */
static int gc_bmp_obj(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	bm_free(*bp);
	return 0;
}

/** ### `Bitmap:save(filename)`
 *  Saves the `Bitmap` to a file.
 */
static int bmp_save(lua_State *L) {	
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	const char *filename = luaL_checkstring(L, 2);
	if(!bm_save(*bp, filename))
		luaL_error(L, "Unable to save bitmap: %s", bm_get_error());

	return 0;
}

/** ### `Bitmap:copy()`
 *  Returns a copy of the `Bitmap` instance.
 */
static int bmp_copy(lua_State *L) {	
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	Bitmap *b = *bp;

	/* My TODO in `bmp_set_font()` applies here as well if you changed the font.
	(perhaps I should just reset it?) */

	Bitmap *clone = bm_copy(b);
	if(!clone)
		luaL_error(L, "Unable to copy bitmap");
	bp = lua_newuserdata(L, sizeof *bp);	
	luaL_setmetatable(L, "Bitmap");
	*bp = clone;
	return 1;
}

/** ### `Bitmap:setColor(color)`, `Bitmap:setColor(R,G,B [,A])`
 *
 *  Sets the pen color of the bitmap.
 * 
 * The first form expects a CSS-style string value, whereas the
 * second form takes numeric R,G,B and an optional Alpha values
 * between 0.0 and 1.0
 */
static int bmp_set_color(lua_State *L) {	
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	unsigned int color = 0;
	if(lua_gettop(L) == 2) {
		const char *mask = luaL_checkstring(L, 2);
		color = bm_atoi(mask);
	} else if(lua_gettop(L) == 4 || lua_gettop(L) == 5) {
		double R = luaL_checknumber(L,2);
		double G = luaL_checknumber(L,3);
		double B = luaL_checknumber(L,4);
		
		if(R < 0.0) R = 0.0; if(R > 1.0) R = 1.0;
		if(G < 0.0) G = 0.0; if(G > 1.0) G = 1.0;
		if(B < 0.0) B = 0.0; if(B > 1.0) B = 1.0;

		if(lua_gettop(L) == 5) {
			double A = luaL_checknumber(L,5);
			if(A < 0.0) A = 0.0; if(A > 1.0) A = 1.0;
			color = bm_rgba(R * 255, G * 255, B * 255, A * 255);
		} else 
			color = bm_rgb(R * 255, G * 255, B * 255);
	}
	bm_set_color(*bp, color);
	return 0;
}

/** ### `R,G,B = Bitmap:getColor()`, `R,G,B = Bitmap:getColor(x, y)`
 * Gets the pen color of the bitmap.
 * 
 * If the `x,y` parameters are supplied, the color of the
 * pixel at `x,y` is returned.
 */
static int bmp_get_color(lua_State *L) {
	unsigned char r,g,b;
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	if(lua_gettop(L) == 3) {
		int x = luaL_checknumber(L,2);
		int y = luaL_checknumber(L,3);
		if(x < 0) x = 0;
		if(x >= (*bp)->w) x = (*bp)->w - 1;
		if(y < 0) y = 0;
		if(y >= (*bp)->h) y = (*bp)->h - 1;		
		bm_picker(*bp, x, y);
	}
	
	unsigned int color = bm_get_color(*bp);
	bm_get_rgb(color, &r, &g, &b);
	lua_pushinteger(L, r);
	lua_pushinteger(L, g);
	lua_pushinteger(L, b);
	return 3;
}

/** ### `W,H = Bitmap:size()`
 *  Returns the width and height of the bitmap
 */
static int bmp_size(lua_State *L) {	
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	lua_pushinteger(L, (*bp)->w);
	lua_pushinteger(L, (*bp)->h);
	return 2;
}

/** ### `Bitmap:resample(W, H, [mode])`
 * 
 * Resizes a bitmap to the new width, height.
 * 
 * `mode` is either `"blin"` for bilinear, `"bcub"` for bicubic, 
 * or `"near"` for nearest neighbour
 */
static int bmp_resample(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int nw = luaL_checknumber(L, 2);
	int nh = luaL_checknumber(L, 3);
	const char *mode = "near";
	
	if(lua_gettop(L) == 4)
		mode = luaL_checkstring(L, 4);
	
	Bitmap *out;
	if(!bm_stricmp(mode, "blin"))
		out = bm_resample_blin(*bp, nw, nh);
	else if(!bm_stricmp(mode, "bcub"))
		out = bm_resample_bcub(*bp, nw, nh);
	else
		out = bm_resample(*bp, nw, nh);

	out->color = (*bp)->color;
	out->font = (*bp)->font;	// See my TODO in `bmp_set_font`

	bm_free(*bp);
	*bp = out;

	return 0;
}

/** ### `Bitmap:blit(src, dx, dy, [sx, sy, [dw, dh, [sw, sh]]])`
 * 
 * Draws an instance {{src}} of {{Bitmap}} to this bitmap at `dx, dy`.
 * 
 * `sx,sy` specify the source x,y position and `dw,dh` specifies the
 * width and height of the destination area to draw.
 * 
 * `sx,sy` defaults to `0,0` and `dw,dh` defaults to the entire 
 * source bitmap.
 * 
 * If `sw,sh` is specified, the bitmap is scaled so that the area on the 
 * source bitmap from `sx,sy` with dimensions `sw,sh` is drawn onto the
 * screen at `dx,dy` with dimensions `dw,dh`.
 */
static int bmp_blit(lua_State *L) {
	Bitmap **dest = luaL_checkudata(L,1, "Bitmap");
	Bitmap **src = luaL_checkudata(L,2, "Bitmap");
	
	int dx = luaL_checknumber(L, 3);
	int dy = luaL_checknumber(L, 4);
	
	int sx = 0, sy = 0, w = (*src)->w, h = (*src)->h;
	
	if(lua_gettop(L) > 5) {
		sx = luaL_checknumber(L, 5);
		sy = luaL_checknumber(L, 6);
	}
	if(lua_gettop(L) > 7) {
		w = luaL_checknumber(L, 7);
		h = luaL_checknumber(L, 8);
	}
	if(lua_gettop(L) > 9) {
		int sw = luaL_checknumber(L, 9);
		int sh = luaL_checknumber(L, 10);
		bm_blit_ex(*dest, dx, dy, w, h, *src, sx, sy, sw, sh, 1);
	} else {
		bm_maskedblit(*dest, dx, dy, *src, sx, sy, w, h);
	}
	
	return 0;
}

/** ### `Bitmap:clip(x0,y0, x1,y1)`
 * Sets the clipping rectangle when drawing primitives.
 */
static int bmp_clip(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_clip(*bp, x0, y0, x1, y1);
	return 0;
}

/** ### `x0,y0, x1,y1 = Bitmap:getClip()`
 * Gets the current clipping rectangle for drawing primitives.
 */
static int bmp_getclip(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
    lua_pushinteger(L, (*bp)->clip.x0);
    lua_pushinteger(L, (*bp)->clip.y0);
    lua_pushinteger(L, (*bp)->clip.x1);
    lua_pushinteger(L, (*bp)->clip.y1);
	return 4;
}

/** ### `Bitmap:unclip()`
 * Resets the clipping rectangle when drawing primitives.
 */
static int bmp_unclip(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	bm_unclip(*bp);
	return 0;
}

/** ### `Bitmap:print(x,y,text)`
 *  Prints the `text` to the bitmap, with its top left position at `x,y`.
 */
static int bmp_print(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x = luaL_checknumber(L, 2);
	int y = luaL_checknumber(L, 3);
	const char *s = luaL_checkstring(L, 4);
	
	bm_puts(*bp, x, y, s);
	
	return 0;
}

/** ### `Bitmap:setFont(font)`
 * 
 *
 */
static int bmp_set_font(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	if(lua_gettop(L) > 1) {
		BmFont **font = luaL_checkudata(L,2, "BitmapFont");
	
		/* TODO: We need to somehow tell Lua that the 
		font is in use and not to be garbage collected... */

		bm_set_font(*bp, *font);
	} else
		bm_reset_font(*bp);
	
	return 0;
}

/** ### `BitmapFont:__gc()`
 *  Garbage collects the `BitmapFont` instance.
 */
static int gc_bmp_font(lua_State *L) {
	BmFont **bp = luaL_checkudata(L,1, "BitmapFont");
	bm_free_font(*bp);
	return 0;
}

void register_bmp_functions(lua_State *L) {
	
	luaL_newmetatable(L, "Bitmap");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index"); /* Bitmap.__index = Bitmap */
	
	/* Add methods */
	lua_pushcfunction(L, bmp_save);
	lua_setfield(L, -2, "save");
	lua_pushcfunction(L, bmp_size);
	lua_setfield(L, -2, "size");
	lua_pushcfunction(L, bmp_copy);
	lua_setfield(L, -2, "copy");
	lua_pushcfunction(L, bmp_set_color);
	lua_setfield(L, -2, "setColor");
	lua_pushcfunction(L, bmp_get_color);
	lua_setfield(L, -2, "getColor");
	lua_pushcfunction(L, bmp_resample);
	lua_setfield(L, -2, "resample");
	lua_pushcfunction(L, bmp_blit);
	lua_setfield(L, -2, "blit");
	lua_pushcfunction(L, bmp_clip);
	lua_setfield(L, -2, "clip");
	lua_pushcfunction(L, bmp_getclip);
	lua_setfield(L, -2, "getclip");
	lua_pushcfunction(L, bmp_unclip);
	lua_setfield(L, -2, "unclip");
	lua_pushcfunction(L, bmp_print);
	lua_setfield(L, -2, "print");
	lua_pushcfunction(L, bmp_set_font);
	lua_setfield(L, -2, "setFont");
	
	lua_pushcfunction(L, bmp_tostring);
	lua_setfield(L, -2, "__tostring");	
	lua_pushcfunction(L, gc_bmp_obj);
	lua_setfield(L, -2, "__gc");

	
	luaL_newmetatable(L, "BitmapFont");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index"); /* Bitmap.__index = Bitmap */
	lua_pushcfunction(L, gc_bmp_font);
	lua_setfield(L, -2, "__gc");

	
	/* The global table Bitmap with static function load() */
	lua_createtable (L, 0, 3);
	
	lua_pushcfunction(L, bmp_create);
	lua_setfield(L, -2, "create");	
	lua_pushcfunction(L, bmp_load);
	lua_setfield(L, -2, "load");

	lua_createtable (L, 0, 2);
	lua_pushcfunction(L, bmf_load_raster);
	lua_setfield(L, -2, "loadRaster");
	lua_pushcfunction(L, bmf_load_sfont);
	lua_setfield(L, -2, "loadSFont");
	lua_setfield(L, -2, "Font");

	lua_setglobal(L, "Bitmap");
}

#define LUABMP_MAIN
#ifdef LUABMP_MAIN
#include <stdio.h>

int main(int argc, char *argv[]) {
	
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	register_bmp_functions(L);

	if(argc > 1) {
		if(luaL_dofile(L, argv[1])) {
			fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
		}
	} else {
		fprintf(stderr, "Usage: %s script.lua", argv[0]);
	}

	lua_close(L);
	return 0;
}
#endif
