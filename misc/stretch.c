#include <math.h>
#include <assert.h>

#include "bmp.h"

/* http://www.codeproject.com/Articles/36145/Free-Image-Transformation
 *
 * See also
 *  - https://math.stackexchange.com/a/104595
 *  - http://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
 *  - https://www.codeproject.com/Articles/247214/Quadrilateral-Distortion
 *  - http://www.vbforums.com/showthread.php?700187-Code-for-a-four-point-transformation-of-an-image
 */
static BmPoint vec2_sub(BmPoint v1, BmPoint v2) {
    BmPoint v = {v1.x - v2.x, v1.y - v2.y};
    return v;
}
static int vec2_cross(BmPoint v1, BmPoint v2) {
    // Fun fact about "2D cross product" https://stackoverflow.com/a/243984/115589
    return v1.x * v2.y - v1.y * v2.x;
}

static BmPoint vec2_interp(BmPoint P, BmPoint D, double t) {
    BmPoint v = {P.x + t * D.x, P.y + t * D.y};
    return v;
}

/* Vertices in `P` are in clockwise order */
void bm_stretch(Bitmap *dst, Bitmap *src, BmPoint P[4]) {
    int i;
    int minx = P[0].x, maxx = P[0].x;
    int miny = P[0].y, maxy = P[0].y;
    for(i = 1; i < 4; i++)
    {
        if(P[i].x < minx) minx = P[i].x;
        if(P[i].x > maxx) maxx = P[i].x;
        if(P[i].y < miny) miny = P[i].y;
        if(P[i].y > maxy) maxy = P[i].y;
    }

    BmPoint AB = vec2_sub(P[1], P[0]);
    BmPoint BC = vec2_sub(P[2], P[1]);
    BmPoint CD = vec2_sub(P[3], P[2]);
    BmPoint DA = vec2_sub(P[0], P[3]);

    if(minx < dst->clip.x0)
        minx = dst->clip.x0;
    if(maxx > dst->clip.x1)
        maxx = dst->clip.x1;
    if(miny < dst->clip.y0)
        miny = dst->clip.y0;
    if(maxy > dst->clip.y1)
        maxy = dst->clip.y1;

    BmPoint q;
    for(q.y = miny; q.y < maxy; q.y++) {
        for(q.x = minx; q.x < maxx; q.x++) {

            double nab = vec2_cross(vec2_sub(q, P[0]), AB);
            double nbc = vec2_cross(vec2_sub(q, P[1]), BC);
            double ncd = vec2_cross(vec2_sub(q, P[2]), CD);
            double nda = vec2_cross(vec2_sub(q, P[3]), DA);

            if(nab <= 0 && nbc <= 0 && ncd <= 0 && nda <= 0) {
                double u = (double)(src->clip.x1 - src->clip.x0) * (nda / (nda + nbc)) + src->clip.x0;
                double v = (double)(src->clip.y1 - src->clip.y0) * (nab / (nab + ncd)) + src->clip.y0;

                if(u >= 0 && u < src->w && v >= 0 && v < src->h) {
                    unsigned int c = bm_get(src, u, v);
                    bm_set(dst, q.x, q.y, c);
                }
            }
        }
    }
}

void bm_destretch(Bitmap *dst, Bitmap *src, BmPoint P[4]) {
    int x, y, w, h;

    w = dst->clip.x1 - dst->clip.x0;
    h = dst->clip.y1 - dst->clip.y0;

    BmPoint AB = vec2_sub(P[1],P[0]);
    BmPoint DC = vec2_sub(P[2],P[3]);

    double ty = 0.0, dty = 1.0 / h;
    double tx = 0.0, dtx = 1.0 / w;

    for(y = dst->clip.y0; y < dst->clip.y1; y++, ty += dty) {
        for(tx = 0.0, x = dst->clip.x0; x < dst->clip.x1; x++, tx += dtx) {
            BmPoint x0 = vec2_interp(P[0], AB, tx);
            BmPoint x1 = vec2_interp(P[3], DC, tx);

            BmPoint uv = vec2_interp(x0, vec2_sub(x1, x0), ty);

            if(uv.x < src->clip.x0 || uv.x >= src->clip.x1 || uv.y < src->clip.y0 || uv.y >= src->clip.y1)
                continue;

            unsigned int c = bm_get(src, uv.x, uv.y);
            bm_set(dst,x,y,c);
        }
    }
}

#ifdef TEST
// $ gcc -Wall -DTEST -I.. stretch.c ../bmp.c
int main(int argc, char *argv[]) {
    Bitmap *b = bm_load("../tile.gif"),
            *out1 = bm_create(100, 100),
            *out2 = bm_create(100, 100);

    BmPoint P[] = {{90, 80},{20, 90},{10, 10},{80, 20}, };
    //BmPoint P[] = {{20, 90},{10, 10},{80, 20},{90, 80}};

    //bm_clip(b, 2, 2, 9, 9);
    //bm_clip(out, 20, 20, 50, 50);

    bm_stretch(out1, b, P);

    bm_save(out1, "out.gif");

    bm_destretch(out2, out1, P);

    bm_save(out2, "out2.gif");

    bm_free(b);
    bm_free(out1);
    bm_free(out2);
}
#endif
