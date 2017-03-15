/*
Loads X11-style XPM files into a Bitmap structure.
Since the XPM file is compiled into the final executable, it
just assumes the input XPM is valid, so it only uses assertions
for error checking.
https://en.wikipedia.org/wiki/X_PixMap
http://www.fileformat.info/format/xpm/egff.htm
*/

#include <stdio.h>
#include <assert.h>

#include "../bmp.h"

/* -- Wikipedia XPM3 examples ----------------------------- */

/* XPM */
static char * XFACE[] = {
/* <Values> */
/* <width/cols> <height/rows> <colors> <char on pixel>*/
"48 4 2 1",
/* <Colors> */
"a c #ffffff",
"b c #000000",
/* <Pixels> */
"abaabaababaaabaabababaabaabaababaabaaababaabaaab",
"abaabaababaaabaabababaabaabaababaabaaababaabaaab",
"abaabaababaaabaabababaabaabaababaabaaababaabaaab",
"abaabaababaaabaabababaabaabaababaabaaababaabaaab"
};

/* XPM */
static char * blarg_xpm[] = {
"16 7 2 1",
"* c #000000",
". c #ffffff",
"**..*...........",
"*.*.*...........",
"**..*..**.**..**",
"*.*.*.*.*.*..*.*",
"**..*..**.*...**",
"...............*",
".............**."
};

/* -------------------------------------------------------- */

int main(int argc, char *argv[]) {
    Bitmap *b = bm_from_Xpm(blarg_xpm);
    bm_save(b, "out.gif");
    bm_free(b);
    return 1;
}

