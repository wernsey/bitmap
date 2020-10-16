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
 * ## C API
 *
 * The C API has just two functions, `bmlua_initialize()` and
 * `bmlua_push_bitmap()`, with their prototypes given as follows:
 *
 * ```
 * void bmlua_initialize(lua_State *L);
 * void bmlua_push_bitmap(lua_State *L, Bitmap *b);
 * ```
 *
 * `bmlua_initialize()` adds all the bitmap related functions to the
 * gives Lua State.
 *
 * ```
 * lua_State *L = luaL_newstate();
 * luaL_openlibs(L);
 * bmlua_initialize(L);
 * luaL_dofile(L, "script.lua");
 * lua_close(L);
 * ```
 *
 * `bmlua_push_bitmap()` pushes a bitmap onto the Lua stack from where
 * other Lua API functions can expose it to the Lua VM.
 *
 * ```
 * Bitmap *canvas = bm_retain(bm_create(120, 80));
 * bmlua_push_bitmap(L, canvas);
 * lua_setglobal(L, "canvas");
 * // do things ...
 * bm_release(canvas);
 * ```
 *
 * (the bitmap _must_ be reference counted through `bm_retain()` because
 * the luabmp module uses reference counting internally to manage memory and
 * to ensure Lua garbage collector don't collect bitmap objects that might
 * still be in use by the C program)
 */
void bmlua_initialize(lua_State *L);
void bmlua_push_bitmap(lua_State *L, Bitmap *b);

/**
 * ## Lua API
 *
 * ### Static functions
 */

/** #### `Bitmap.create(W, H)`
 * Creates a bitmap object of the specified size
 */
static int bmp_create(lua_State *L) {
	int w = luaL_checknumber(L,1);
	int h = luaL_checknumber(L,2);

	Bitmap **bp = lua_newuserdata(L, sizeof *bp);
	luaL_setmetatable(L, "Bitmap");

	*bp = bm_create(w, h);
	if(!*bp)
		luaL_error(L, "Unable to create bitmap");
	bm_retain(*bp);
	return 1;
}

/** #### `Bitmap.load(filename)`
 * Loads the bitmap file specified by `filename`
 */
static int bmp_load(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);

	Bitmap **bp = lua_newuserdata(L, sizeof *bp);
	luaL_setmetatable(L, "Bitmap");

	*bp = bm_load(filename);
	if(!*bp)
		luaL_error(L, "Unable to load bitmap '%s': %s", filename, bm_get_error());
	bm_retain(*bp);

	return 1;
}

/** #### `Bitmap.Font.loadRaster(filename, spacing)`
 * Loads the raster font specified by `filename`
 */
static int bmf_load_raster(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);
	int spacing;

	if(lua_gettop(L) > 1)
	 	spacing = luaL_checknumber(L, 2);
	else
		spacing = 8;

	BmFont **fp = lua_newuserdata(L, sizeof *fp);
	luaL_setmetatable(L, "BitmapFont");

	*fp = bm_make_ras_font(filename, spacing);
	if(!*fp) {
		luaL_error(L, "Unable to load raster font '%s': %s", filename, bm_get_error());
	}
	/* Fonts enter the world with a ref count of 1, so no need to retain them */
	return 1;
}

/** #### `Bitmap.Font.loadSFont(filename)`
 * Loads the [SFont][sfont] specified by `filename`
 *
 * [sfont]: http://www.linux-games.com/sfont/
 */
static int bmf_load_sfont(lua_State *L) {
	const char *filename = luaL_checkstring(L,1);

	BmFont **fp = lua_newuserdata(L, sizeof *fp);
	luaL_setmetatable(L, "BitmapFont");

	*fp = bm_make_sfont(filename);
	if(!*fp) {
		luaL_error(L, "Unable to load SFont '%s': %s", filename, bm_get_error());
	}
	return 1;
}

/**
 * ### The `Bitmap` object
 */

/** #### `Bitmap:__tostring()`
 *  Returns a string representation of the `Bitmap` instance.
 */
static int bmp_tostring(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	Bitmap *b = *bp;
	lua_pushfstring(L, "Bitmap[%dx%d]", bm_width(b), bm_height(b));
	return 1;
}

/** #### `Bitmap:__gc()`
 *  Garbage collects the `Bitmap` instance.
 */
static int gc_bmp_obj(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	bm_release(*bp);
	return 0;
}

/** #### `Bitmap:save(filename)`
 *  Saves the `Bitmap` to a file.
 */
static int bmp_save(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	const char *filename = luaL_checkstring(L, 2);
	if(!bm_save(*bp, filename))
		luaL_error(L, "Unable to save bitmap: %s", bm_get_error());

	return 0;
}

/** #### `Bitmap:copy()`
 *  Returns a copy of the `Bitmap` instance.
 */
static int bmp_copy(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	Bitmap *b = *bp;

	Bitmap *clone = bm_copy(b);
	if(!clone)
		luaL_error(L, "Unable to copy bitmap");
	bp = lua_newuserdata(L, sizeof *bp);
	luaL_setmetatable(L, "Bitmap");
	*bp = bm_retain(clone);
	return 1;
}

static double clamp(double n) {
	if(n < 0) return 0;
	if(n > 1) return 1;
	return n;
}

/** #### `Bitmap:setColor(color)`, `Bitmap:setColor(R,G,B [,A])`
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
		double R = clamp(luaL_checknumber(L,2));
		double G = clamp(luaL_checknumber(L,3));
		double B = clamp(luaL_checknumber(L,4));

		if(lua_gettop(L) == 5) {
			double A = clamp(luaL_checknumber(L,5));
			color = bm_rgba(R * 255, G * 255, B * 255, A * 255);
		} else
			color = bm_rgb(R * 255, G * 255, B * 255);
	}
	bm_set_color(*bp, color);
	return 0;
}

/** #### `R,G,B = Bitmap:getColor()`, `R,G,B = Bitmap:getColor(x, y)`
 * Gets the pen color of the bitmap.
 *
 * If the `x,y` parameters are supplied, the color of the
 * pixel at `x,y` is returned.
 */
static int bmp_get_color(lua_State *L) {
	unsigned char r,g,b;
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	unsigned int color;
	if(lua_gettop(L) == 3) {
		int x = luaL_checknumber(L,2);
		int y = luaL_checknumber(L,3);
		if(x < 0) x = 0;
		if(x >= bm_width(*bp)) x = bm_width(*bp) - 1;
		if(y < 0) y = 0;
		if(y >= bm_height(*bp)) y = bm_height(*bp) - 1;
		color = bm_get(*bp, x, y);
	}
	else
		color = bm_get_color(*bp);
	bm_get_rgb(color, &r, &g, &b);
	lua_pushnumber(L, (double)r / 255.0);
	lua_pushnumber(L, (double)g / 255.0);
	lua_pushnumber(L, (double)b / 255.0);
	return 3;
}

/** #### `W,H = Bitmap:size()`
 *  Returns the width and height of the bitmap
 */
static int bmp_size(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	lua_pushinteger(L, bm_width(*bp));
	lua_pushinteger(L, bm_height(*bp));
	return 2;
}

/** #### `Bitmap:resample(W, H, [mode])`
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

	bm_set_color(out, bm_get_color(*bp));
	bm_set_font(out, bm_get_font(*bp));

	bm_release(*bp);
	*bp = bm_retain(out);

	return 0;
}

/** #### `Bitmap:blit(src, dx, dy, [sx, sy, [dw, dh, [sw, sh]]])`
 *
 * Draws an instance `src` of `Bitmap` to this bitmap at (dx, dy).
 *
 * (sx,sy) specify the source (x,y) position and (dw,dh) specifies the
 * width and height of the destination area to draw.
 *
 * (sx,sy) defaults to (0,0) and (dw,dh) defaults to the entire
 * source bitmap.
 *
 * If (sw,sh) is specified, the bitmap is scaled so that the area on the
 * source bitmap from (sx,sy) with dimensions (sw,sh) is drawn onto the
 * screen at (dx,dy) with dimensions (dw,dh).
 */
static int bmp_blit(lua_State *L) {
	Bitmap **dest = luaL_checkudata(L,1, "Bitmap");
	Bitmap **src = luaL_checkudata(L,2, "Bitmap");

	int dx = luaL_checknumber(L, 3);
	int dy = luaL_checknumber(L, 4);

	int sx = 0, sy = 0, w = bm_width(*src), h = bm_height(*src);

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
		bm_blit_ex(*dest, dx, dy, w, h, *src, sx, sy, sw, sh, 0);
	} else {
		bm_blit(*dest, dx, dy, *src, sx, sy, w, h);
	}

	return 0;
}

/** #### `Bitmap:maskedblit(src, dx, dy, [sx, sy, [dw, dh, [sw, sh]]])`
 *
 * Draws an instance `src` of `Bitmap` to this bitmap at (dx, dy), using the
 * color of `src` as a mask.
 *
 * (sx,sy) specify the source (x,y) position and (dw,dh) specifies the
 * width and height of the destination area to draw.
 *
 * (sx,sy) defaults to (0,0) and (dw,dh) defaults to the entire
 * source bitmap.
 *
 * If (sw,sh) is specified, the bitmap is scaled so that the area on the
 * source bitmap from (sx,sy) with dimensions (sw,sh) is drawn onto the
 * screen at (dx,dy) with dimensions (dw,dh).
 */
static int bmp_maskedblit(lua_State *L) {
	Bitmap **dest = luaL_checkudata(L,1, "Bitmap");
	Bitmap **src = luaL_checkudata(L,2, "Bitmap");

	int dx = luaL_checknumber(L, 3);
	int dy = luaL_checknumber(L, 4);

	int sx = 0, sy = 0, w = bm_width(*src), h = bm_height(*src);

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

/** #### `Bitmap:clip(x0,y0, x1,y1)`
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

/** #### `x0,y0, x1,y1 = Bitmap:getClip()`
 * Gets the current clipping rectangle for drawing primitives.
 */
static int bmp_getclip(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	BmRect clip = bm_get_clip(*bp);
    lua_pushinteger(L, clip.x0);
    lua_pushinteger(L, clip.y0);
    lua_pushinteger(L, clip.x1);
    lua_pushinteger(L, clip.y1);
	return 4;
}

/** #### `Bitmap:unclip()`
 * Resets the clipping rectangle when drawing primitives.
 */
static int bmp_unclip(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	bm_unclip(*bp);
	return 0;
}

/** #### `Bitmap:putpixel(x,y)`
 * Plots a pixel at (x,y) on the screen.
 */
static int bmp_putpixel(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x = luaL_checknumber(L,2);
	int y = luaL_checknumber(L,3);
	bm_putpixel(*bp, x, y);
	return 0;
}


/** #### `Bitmap:line(x1, y1, x2, y2)`
 * Draws a line from (x1,y1) to (x2,y2)
 */
static int bmp_line(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_line(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:rect(x1, y1, x2, y2)`
 * Draws a rectangle between (x1,y1) and (x2,y2)
 */
static int bmp_rect(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_rect(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:fillrect(x1, y1, x2, y2)`
 * Draws a filled rectangle between (x1,y1) and (x2,y2)
 */
static int bmp_fillrect(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_fillrect(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:dithrect(x1, y1, x2, y2)`
 * Draws a dithered rectangle between (x1,y1) and (x2,y2)
 */
static int bmp_dithrect(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_dithrect(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:circle(x, y, r)`
 * Draws a circle of radius `r` centered at (x,y)
 */
static int bmp_circle(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x = luaL_checknumber(L,2);
	int y = luaL_checknumber(L,3);
	int r = luaL_checknumber(L,4);
	bm_circle(*bp, x, y, r);
	return 0;
}

/** #### `Bitmap:fillcircle(x, y, r)`
 * Draws a filled circle of radius `r` centered at (x,y)
 */
static int bmp_fillcircle(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x = luaL_checknumber(L,2);
	int y = luaL_checknumber(L,3);
	int r = luaL_checknumber(L,4);
	bm_fillcircle(*bp, x, y, r);
	return 0;
}

/** #### `Bitmap:ellipse(x1, y1, x2, y2)`
 * Draws an ellipse that is contained in the rectangle from (x1,y1) to (x2,y2)
 */
static int bmp_ellipse(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_ellipse(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:fillellipse(x1, y1, x2, y2)`
 * Draws a filled ellipse that is contained in the rectangle from (x1,y1) to (x2,y2)
 */
static int bmp_fillellipse(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	bm_fillellipse(*bp, x0, y0, x1, y1);
	return 0;
}

/** #### `Bitmap:roundrect(x1, y1, x2, y2, r)`
 * Draws a rectangle between (x1,y1) and (x2,y2) with rounded corners of radius `r`
 */
static int bmp_roundrect(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	int r = luaL_checknumber(L,6);
	bm_roundrect(*bp, x0, y0, x1, y1, r);
	return 0;
}

/** #### `Bitmap:fillroundrect(x1, y1, x2, y2, r)`
 * Draws a filled rectangle between (x1,y1) and (x2,y2) with rounded corners of radius `r`
 */
static int bmp_fillroundrect(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	int r = luaL_checknumber(L,6);
	bm_fillroundrect(*bp, x0, y0, x1, y1, r);
	return 0;
}

/** #### `Bitmap:bezier3(x0, y0, x1, y1, x2, y2)`
 * Draws a Bezier curve from (x0,y0) to (x2,y2)
 * with (x1,y1) as the control point.
 * Note that it doesn't pass through (x1,y1)
 */
static int bmp_bezier3(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	int x0 = luaL_checknumber(L,2);
	int y0 = luaL_checknumber(L,3);
	int x1 = luaL_checknumber(L,4);
	int y1 = luaL_checknumber(L,5);
	int x2 = luaL_checknumber(L,6);
	int y2 = luaL_checknumber(L,7);
	bm_bezier3(*bp, x0, y0, x1, y1, x2, y2);
	return 0;
}

/** #### `Bitmap:print(x,y,text)`
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

/** #### `w,h = Bitmap:textSize(text)`
 * Returns the width,height in pixels that the `text`
 * will occupy on the screen.
 *
 *     local w,h = bitmap:textSize(message);
 */
static int bmp_textdims(lua_State *L) {

	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	const char *s = luaL_checkstring(L, 2);

	lua_pushinteger(L, bm_text_width(*bp, s));
	lua_pushinteger(L, bm_text_height(*bp, s));

	return 2;
}

/** #### `Bitmap:setFont(font)`
 * Sets the font used by `Bitmap:print()`
 */
static int bmp_set_font(lua_State *L) {
	Bitmap **bp = luaL_checkudata(L,1, "Bitmap");
	if(lua_gettop(L) > 1) {
		BmFont **font = luaL_checkudata(L,2, "BitmapFont");
		bm_set_font(*bp, *font);
	} else
		bm_reset_font(*bp);

	return 0;
}

/** #### `BitmapFont:__gc()`
 *  Garbage collects the `BitmapFont` instance.
 */
static int gc_bmp_font(lua_State *L) {
	BmFont **fp = luaL_checkudata(L,1, "BitmapFont");
	bm_font_release(*fp);
	return 0;
}

void bmlua_initialize(lua_State *L) {

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
	lua_pushcfunction(L, bmp_maskedblit);
	lua_setfield(L, -2, "maskedblit");
	lua_pushcfunction(L, bmp_clip);
	lua_setfield(L, -2, "clip");
	lua_pushcfunction(L, bmp_getclip);
	lua_setfield(L, -2, "getclip");
	lua_pushcfunction(L, bmp_unclip);
	lua_setfield(L, -2, "unclip");

	lua_pushcfunction(L, bmp_putpixel);
	lua_setfield(L, -2, "putpixel");
	lua_pushcfunction(L, bmp_line);
	lua_setfield(L, -2, "line");
	lua_pushcfunction(L, bmp_rect);
	lua_setfield(L, -2, "rect");
	lua_pushcfunction(L, bmp_fillrect);
	lua_setfield(L, -2, "fillrect");
	lua_pushcfunction(L, bmp_dithrect);
	lua_setfield(L, -2, "dithrect");
	lua_pushcfunction(L, bmp_circle);
	lua_setfield(L, -2, "circle");
	lua_pushcfunction(L, bmp_fillcircle);
	lua_setfield(L, -2, "fillcircle");
	lua_pushcfunction(L, bmp_ellipse);
	lua_setfield(L, -2, "ellipse");
	lua_pushcfunction(L, bmp_fillellipse);
	lua_setfield(L, -2, "fillellipse");
	lua_pushcfunction(L, bmp_roundrect);
	lua_setfield(L, -2, "roundrect");
	lua_pushcfunction(L, bmp_fillroundrect);
	lua_setfield(L, -2, "fillroundrect");
	lua_pushcfunction(L, bmp_bezier3);
	lua_setfield(L, -2, "bezier3");

	lua_pushcfunction(L, bmp_print);
	lua_setfield(L, -2, "print");
	lua_pushcfunction(L, bmp_set_font);
	lua_setfield(L, -2, "setFont");
	lua_pushcfunction(L, bmp_textdims);
	lua_setfield(L, -2, "textSize");

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

void bmlua_push_bitmap(lua_State *L, Bitmap *b) {
	Bitmap **bp = lua_newuserdata(L, sizeof *bp);
	luaL_setmetatable(L, "Bitmap");
	*bp = bm_retain(b);
}

#define LUABMP_MAIN
#ifdef LUABMP_MAIN
#include <stdio.h>

int main(int argc, char *argv[]) {

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	/* Add all the Bitmap related functions to the Lua State */
	bmlua_initialize(L);

	/* This is how you would expose a Bitmap from your C program to a Lua script:
	First, create your bitmap if you haven't done so already.
	Then retain it with `bm_retain()`. It is important to use the reference counter
	for Bitmaps shared with Lua, because the luabmp module uses the reference counter
	internally to ensure that the Lua garbage collector don't delete bitmaps still
	in use by the C program. */
	Bitmap *canvas = bm_retain(bm_create(120, 80));
	if(!canvas) {
		fprintf(stderr, "Unable to create canvas: %s\n", bm_get_error());
		return 1;
	}

	/* Now push the bitmap onto the Lua stack: */
	bmlua_push_bitmap(L, canvas);

	/* You can now do Lua-related things. In this case I just save the bitmap
		into a Lua global variable named `canvas`. The Lua script can then call
		methods on it just as with any other Bitmap object */
	lua_setglobal(L, "canvas");

	/* Execute the Lua script */
	if(argc > 1) {
		if(luaL_dofile(L, argv[1])) {
			fprintf(stderr, "error: %s\n", lua_tostring(L, -1));
		}
	} else {
		fprintf(stderr, "Usage: %s script.lua", argv[0]);
	}

	lua_close(L);

	bm_save(canvas, "out-canvas.gif");

	/* Release our reference to the `canvas` bitmap */
	bm_release(canvas);

	return 0;
}
#endif
