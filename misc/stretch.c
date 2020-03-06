#include <math.h>

#include "bmp.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* http://www.codeproject.com/Articles/36145/Free-Image-Transformation
 * See also
 *  - https://math.stackexchange.com/a/104595
 *  - https://www.codeproject.com/Articles/247214/Quadrilateral-Distortion
 *  - http://www.vbforums.com/showthread.php?700187-Code-for-a-four-point-transformation-of-an-image
 */
typedef struct {
    double x,y;
} Vector2;
static Vector2 vec2(double x, double y) {
    Vector2 v = {x, y};
    return v;
}
static Vector2 vec2_sub(Vector2 v1, Vector2 v2) {
    return vec2(v1.x - v2.x, v1.y - v2.y);
}
static double vec2_cross(Vector2 v1, Vector2 v2) {
    return v1.x * v2.y - v1.y * v2.x;
}
static int vec2_isCCW(Vector2 v1, Vector2 v2, Vector2 v3) {
    return vec2_cross(vec2_sub(v1, v2), vec2_sub(v3, v2)) > 0;
}
static int on_plane(Vector2 v, Vector2 A, Vector2 B, Vector2 C, Vector2 D) {
    return !vec2_isCCW(v, A, B) && !vec2_isCCW(v, B, C) && !vec2_isCCW(v, C, D) && !vec2_isCCW(v, D, A);
}

/* Vertices in dx are in clockwise order */
void bm_transform_image(Bitmap *dst, Bitmap *src, BmPoint dx[4]) {
    int x, y;
    int minx = MIN(MIN(dx[0].x, dx[1].x),MIN(dx[2].x, dx[3].x));
    int maxx = MAX(MAX(dx[0].x, dx[1].x),MAX(dx[2].x, dx[3].x));
    int miny = MIN(MIN(dx[0].y, dx[1].y),MIN(dx[2].y, dx[3].y));
    int maxy = MAX(MAX(dx[0].y, dx[1].y),MAX(dx[2].y, dx[3].y));

    Vector2 points[4];
    for(x = 0; x < 4; x++)
        points[x] = vec2(dx[x].x, dx[x].y);

    Vector2 AB = vec2_sub(points[1], points[0]);
    Vector2 BC = vec2_sub(points[2], points[1]);
    Vector2 CD = vec2_sub(points[3], points[2]);
    Vector2 DA = vec2_sub(points[0], points[3]);

    for(y = miny; y < maxy; y++) {
        for(x = minx; x < maxx; x++) {
            Vector2 p = {x,y};
            if(on_plane(p, points[0], points[1], points[2], points[3])) {
                double dab = fabs(vec2_cross(vec2_sub(p, points[0]), AB));
                double dbc = fabs(vec2_cross(vec2_sub(p, points[1]), BC));
                double dcd = fabs(vec2_cross(vec2_sub(p, points[2]), CD));
                double dda = fabs(vec2_cross(vec2_sub(p, points[3]), DA));
                double spx = (double)(src->clip.x1 - src->clip.x0) * (dda / (dda + dbc));
                double spy = (double)(src->clip.y1 - src->clip.y0) * (dab / (dab + dcd));
                int u = (int)spx + src->clip.x0;
                int v = (int)spy + src->clip.y0;

                if(u >= 0 && u < src->w && v >= 0 && v < src->h) {
                    int c = bm_get(src, u, v);
                    bm_set_color(dst, c);
                    bm_putpixel(dst, x, y);
                }
            }
        }
    }
}