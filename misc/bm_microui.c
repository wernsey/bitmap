/**
 * bm_microui.c
 * ============
 *
 * Renderer for the [microui][] GUI library that can render to
 * a `Bitmap` structure.
 *
 * This is just the rendering part.
 * You still need to call the **microui** functions that handles input
 * (like `mu_input_mousemove`, `mu_input_mousedown`, `mu_input_mouseup`,
 * `mu_input_keydown`, `mu_input_keyup` and `mu_input_text`) separately
 * depending on your framework.
 *
 * There are two functions that make up the API:
 *
 * ```
 * extern void bm_mu_init(Bitmap *target, mu_Context *context);
 * extern void bm_mu_render(Bitmap *target, mu_Context *context);
 * ```
 *
 * Call `bm_mu_init()` at the start of your program, after `mu_init`,
 * then call `bm_mu_render()` on every frame to do the actual rendering.
 *
 * [microui]: https://github.com/rxi/microui
 */

#include <string.h>

#include "microui.h"
#include "bmp.h"

/* icons.xbm ************************************/
#define icons_width 32
#define icons_height 48
static unsigned char icons_bits[] = {
   0xff, 0xff, 0xff, 0xbf, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xcf,
   0xe7, 0xe7, 0xff, 0xe7, 0xc7, 0xe3, 0xff, 0xf3, 0x8f, 0xf1, 0xff, 0xf9,
   0x9f, 0xf9, 0xff, 0xbc, 0xff, 0xff, 0x7f, 0x9e, 0xff, 0xff, 0x3f, 0xcf,
   0x9f, 0xf9, 0x9f, 0xe7, 0x8f, 0xf1, 0xcf, 0xf3, 0xc7, 0xe3, 0xe7, 0xf9,
   0xe7, 0xe7, 0xf3, 0xbc, 0xff, 0xff, 0x79, 0x9e, 0xff, 0xff, 0x3c, 0x8f,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xdf, 0x1f, 0xff,
   0xff, 0xcf, 0x3f, 0xfe, 0xff, 0xe7, 0x7f, 0xfc, 0xff, 0xf3, 0xff, 0xf8,
   0xff, 0xf9, 0x7f, 0xfc, 0xfd, 0xfc, 0x3f, 0xfe, 0x79, 0xfe, 0x1f, 0xff,
   0x33, 0xff, 0x9f, 0xff, 0x87, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xe7, 0xf3, 0xff, 0xff, 0xc7, 0xf1, 0xff, 0xff,
   0x8f, 0xf8, 0xff, 0xff, 0x1f, 0xfc, 0xff, 0xff, 0x3f, 0xfe, 0xff, 0xff,
   0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
   0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/************************************************/

static void set_color(Bitmap *tgt, const mu_Color color) {
    bm_set_color(tgt, bm_rgba(color.r, color.g, color.b, color.a));
}

static int text_width(mu_Font font, const char *text, int len) {
  if (len < 0) { len = strlen(text); }
  if(!font)
    return 7*len;
  BmFont *f = (BmFont *)font;
  return f->width(f) * len;
}

static int text_height(mu_Font font) {
  if(!font)
    return 8;
  BmFont *f = (BmFont *)font;
  return f->height(f);
}

void bm_mu_init(Bitmap *target, mu_Context *context) {
    context->text_width = text_width;
    context->text_height = text_height;
    context->style->font = target->font;
}

void bm_mu_render(Bitmap *target, mu_Context *context) {

    unsigned int save_color = bm_get_color(target);

    mu_Command *cmd = NULL;
    while (mu_next_command(context, &cmd)) {
      switch (cmd->type) {
        case MU_COMMAND_TEXT:
            set_color(target, cmd->text.color);
            if(context->style->font)
              bm_set_font(target, context->style->font);
            bm_puts(target, cmd->text.pos.x, cmd->text.pos.y, cmd->text.str);
            break;
        case MU_COMMAND_RECT:
            set_color(target, cmd->rect.color);
            bm_fillrect(target, cmd->rect.rect.x, cmd->rect.rect.y, cmd->rect.rect.x + cmd->rect.rect.w, cmd->rect.rect.y + cmd->rect.rect.h);
            break;
        case MU_COMMAND_ICON:
            set_color(target, cmd->icon.color);
            int x = cmd->icon.rect.x + (cmd->icon.rect.w - 16)/2;
            int y = cmd->icon.rect.y + (cmd->icon.rect.h - 16)/2;
            int r,c;
            switch(cmd->icon.id) {
              case MU_ICON_CLOSE: r = 0, c = 0; break;
              case MU_ICON_RESIZE: r = 0, c = 1; break;
              case MU_ICON_CHECK: r = 1, c = 0; break;
              case MU_ICON_COLLAPSED: r = 1, c = 1; break;
              case MU_ICON_EXPANDED: r = 2, c = 0; break;
              default: r = 0, c = 0; break;
            }
            bm_blit_xbm(target, x, y, c*16, r*16, 16, 16, icons_width, icons_height, icons_bits);

            break;
        case MU_COMMAND_CLIP:
            bm_clip(target, cmd->clip.rect.x, cmd->clip.rect.y, cmd->clip.rect.x + cmd->clip.rect.w, cmd->clip.rect.y + cmd->clip.rect.h);
            break;
      }
    }
    bm_set_color(target, save_color);
}