#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FRACTL_PI  3.14159265358979323846
#define MARGIN     10.0
typedef struct { double x, y; } Point;
typedef struct { double x, y, heading; } State;
static State   cur;
static char   *svg_path;
static Point  *pts;
static size_t  npts, cap_pts;
static size_t *strokes;
static size_t  nstrokes, cap_strokes;
static State  *stack;
static size_t  depth, cap_stack, max_depth;
static int     pen_lifted;

static void die(const char *msg) {
    fprintf(stderr, "fractl runtime: %s\n", msg);
    exit(1);
}

static void *grow(void *p, size_t *cap, size_t elem) {
    size_t n = *cap ? *cap * 2 : 1024;
    void *q = realloc(p, n * elem);
    if (!q) die("out of memory");
    *cap = n;
    return q;
}

static void add_point(double px, double py) {
    if (npts == cap_pts) pts = grow(pts, &cap_pts, sizeof *pts);
    pts[npts].x = px;
    pts[npts].y = py;
    npts++;
}

static void begin_stroke(void) {
    if (nstrokes == cap_strokes) strokes = grow(strokes, &cap_strokes, sizeof *strokes);
    strokes[nstrokes++] = npts;
    add_point(cur.x, cur.y);
    pen_lifted = 0;
}

static void advance(double dist) {
    double r = cur.heading * FRACTL_PI / 180.0;
    cur.x += dist * cos(r);
    cur.y += dist * sin(r);
}
void turtle_init(double step, double angle, double sx, double sy, double sheading,
                 const char *path) {
    (void)step;
    (void)angle;
    cur.x = sx;
    cur.y = sy;
    cur.heading = sheading;
    free(svg_path);
    svg_path = malloc(strlen(path) + 1);
    if (!svg_path) die("out of memory");
    strcpy(svg_path, path);
    npts = nstrokes = depth = max_depth = 0;
    begin_stroke();
}
void turtle_forward(double dist) {
    if (pen_lifted) begin_stroke();
    advance(dist);
    add_point(cur.x, cur.y);
}
void turtle_move(double dist) {
    advance(dist);
    pen_lifted = 1;
}
void turtle_turn(double degrees) {
    cur.heading += degrees;
}
void turtle_push(void) {
    if (depth == cap_stack) stack = grow(stack, &cap_stack, sizeof *stack);
    stack[depth++] = cur;
    if (depth > max_depth) max_depth = depth;
}
void turtle_pop(void) {
    if (depth == 0) die("state stack underflow (']' without matching '[')");
    cur = stack[--depth];
    pen_lifted = 1;
}

static size_t stroke_end(size_t s) {
    return s + 1 < nstrokes ? strokes[s + 1] : npts;
}

static void put_num(FILE *f, double v) {
    double r = round(v * 100.0) / 100.0;
    if (r == 0.0) r = 0.0;
    fprintf(f, "%.2f", r);
}

void turtle_finish(void) {
    const char *path = svg_path ? svg_path : "out.svg";
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); exit(1); }
    double minx = 0, maxx = 0, miny = 0, maxy = 0;
    size_t drawn = 0, segments = 0;
    for (size_t s = 0; s < nstrokes; s++) {
        size_t end = stroke_end(s);
        if (end - strokes[s] < 2) continue;
        segments += end - strokes[s] - 1;
        for (size_t i = strokes[s]; i < end; i++, drawn++) {
            if (drawn == 0 || pts[i].x < minx) minx = pts[i].x;
            if (drawn == 0 || pts[i].x > maxx) maxx = pts[i].x;
            if (drawn == 0 || pts[i].y < miny) miny = pts[i].y;
            if (drawn == 0 || pts[i].y > maxy) maxy = pts[i].y;
        }
    }
    double svg_w = (maxx - minx) + 2 * MARGIN;
    double svg_h = (maxy - miny) + 2 * MARGIN;

    fputs("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ", f);
    put_num(f, svg_w); fputc(' ', f); put_num(f, svg_h);
    fputs("\" width=\"", f);  put_num(f, svg_w);
    fputs("\" height=\"", f); put_num(f, svg_h);
    fputs("\">\n"
          "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n"
          "<g fill=\"none\" stroke=\"black\" stroke-width=\"1\" "
          "stroke-linejoin=\"round\" stroke-linecap=\"round\">\n", f);
    size_t written = 0;
    for (size_t s = 0; s < nstrokes; s++) {
        size_t end = stroke_end(s);
        if (end - strokes[s] < 2) continue;
        fputs("<polyline points=\"", f);
        for (size_t i = strokes[s]; i < end; i++) {
            if (i > strokes[s]) fputc(' ', f);
            put_num(f, MARGIN + (pts[i].x - minx));
            fputc(',', f);
            put_num(f, MARGIN + (maxy - pts[i].y));
        }
        fputs("\"/>\n", f);
        written++;
    }
    fputs("</g>\n</svg>\n", f);
    if (fclose(f) != 0) { perror(path); exit(1); }
    if (written == 1)
        printf("fractl: wrote %s (%zu segments, %.0f x %.0f px)\n",
               path, segments, svg_w, svg_h);
    else
        printf("fractl: wrote %s (%zu segments, %zu strokes, max depth %zu, %.0f x %.0f px)\n",
               path, segments, written, max_depth, svg_w, svg_h);
}

#include <stdint.h>
#include "runtime.h"

static uint64_t fr_mix(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}
static uint64_t fr_seed;
static uint64_t fr_ctr[64];
void fractl_seed(uint64_t s) {
    fr_seed = s;
    memset(fr_ctr, 0, sizeof fr_ctr);
}
double fractl_rand(int32_t level) {
    if (level < 0 || level > 63) level = 63;
    uint64_t h = fr_mix(fr_seed ^ fr_mix((uint64_t)level * 0x100000001B3ULL ^ fr_mix(fr_ctr[level]++)));
    return (double)(h >> 11) * (1.0 / 9007199254740992.0);
}

typedef struct { double p[3], h[3], l[3], u[3], width; int color; } Turtle3;
static Turtle3  t3;
static Turtle3 *st3;
static size_t   d3, cap3, maxd3;
static FILE    *out3;
static char    *path3;
static double   org3[3], cyaw3, syaw3, scale3, taper3;
static size_t   ntubes, nblades, ninst;

static void put3(double v) {
    double r = round(v * 1000.0) / 1000.0;
    if (r == 0.0) r = 0.0;
    fprintf(out3, " %.3f", r);
}
static void world(const double a[3], double w[3]) {
    double x = a[0] * scale3, y = a[1] * scale3, z = a[2] * scale3;
    w[0] = org3[0] + x * cyaw3 + z * syaw3;
    w[1] = org3[1] + y;
    w[2] = org3[2] - x * syaw3 + z * cyaw3;
}
static void wdir(const double a[3], double w[3]) {
    w[0] = a[0] * cyaw3 + a[2] * syaw3;
    w[1] = a[1];
    w[2] = -a[0] * syaw3 + a[2] * cyaw3;
}
static void norm3(double v[3]) {
    double n = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n > 0) { v[0] /= n; v[1] /= n; v[2] /= n; }
}
static void reframe(void) {
    norm3(t3.h);
    double d = t3.l[0] * t3.h[0] + t3.l[1] * t3.h[1] + t3.l[2] * t3.h[2];
    for (int i = 0; i < 3; i++) t3.l[i] -= d * t3.h[i];
    norm3(t3.l);
    t3.u[0] = t3.h[1] * t3.l[2] - t3.h[2] * t3.l[1];
    t3.u[1] = t3.h[2] * t3.l[0] - t3.h[0] * t3.l[2];
    t3.u[2] = t3.h[0] * t3.l[1] - t3.h[1] * t3.l[0];
}
static void rot3(double a[3], double b[3], double deg) {
    double r = deg * FRACTL_PI / 180.0, c = cos(r), s = sin(r);
    for (int i = 0; i < 3; i++) {
        double na = a[i] * c + b[i] * s, nb = -a[i] * s + b[i] * c;
        a[i] = na;
        b[i] = nb;
    }
    reframe();
}

void turtle3_init(const char *path) {
    free(path3);
    path3 = malloc(strlen(path) + 1);
    if (!path3) die("out of memory");
    strcpy(path3, path);
    out3 = fopen(path3, "w");
    if (!out3) { perror(path3); exit(1); }
    fputs("# FRACTL 3D segments v1\n", out3);
    ntubes = nblades = ninst = maxd3 = 0;
}
void turtle3_begin(double x, double y, double z, double yaw, double scale,
                   double width, double taper, const char *name, const char *colors) {
    double r = yaw * FRACTL_PI / 180.0;
    org3[0] = x; org3[1] = y; org3[2] = z;
    cyaw3 = cos(r); syaw3 = sin(r);
    scale3 = scale;
    taper3 = taper;
    memset(&t3, 0, sizeof t3);
    t3.h[1] = 1.0;
    t3.l[0] = -1.0;
    t3.u[2] = 1.0;
    t3.width = width;
    d3 = 0;
    fprintf(out3, "o %s %zu %s\n", name, ninst++, colors[0] ? colors : "-");
}
static void segment(char kind, double len) {
    double a[3], b[3], p1[3];
    for (int i = 0; i < 3; i++) p1[i] = t3.p[i] + t3.h[i] * len;
    world(t3.p, a);
    world(p1, b);
    fputc(kind, out3);
    for (int i = 0; i < 3; i++) put3(a[i]);
    for (int i = 0; i < 3; i++) put3(b[i]);
    if (kind == 't') {
        put3(t3.width * 0.5 * scale3);
        ntubes++;
    } else {
        double s[3];
        wdir(t3.l, s);
        for (int i = 0; i < 3; i++) put3(s[i]);
        put3(t3.width * scale3);
        put3(t3.width * taper3 * scale3);
        nblades++;
    }
    fprintf(out3, " %d\n", t3.color);
    memcpy(t3.p, p1, sizeof p1);
}
void turtle3_forward(double len) { segment('t', len); }
void turtle3_blade(double len)   { segment('b', len); }
void turtle3_move(double len) {
    for (int i = 0; i < 3; i++) t3.p[i] += t3.h[i] * len;
}
void turtle3_yaw(double degrees)   { rot3(t3.h, t3.l, degrees); }
void turtle3_pitch(double degrees) { rot3(t3.h, t3.u, -degrees); }
void turtle3_roll(double degrees)  { rot3(t3.l, t3.u, -degrees); }
void turtle3_push(void) {
    if (d3 == cap3) st3 = grow(st3, &cap3, sizeof *st3);
    st3[d3++] = t3;
    if (d3 > maxd3) maxd3 = d3;
}
void turtle3_pop(void) {
    if (d3 == 0) die("state stack underflow (']' without matching '[')");
    t3 = st3[--d3];
}
void turtle3_thin(double factor) { t3.width *= factor; }
void turtle3_color(void)         { t3.color++; }
void turtle3_end(void)           { d3 = 0; }
void turtle3_finish(void) {
    if (fclose(out3) != 0) { perror(path3); exit(1); }
    out3 = NULL;
    printf("fractl: wrote %s (%zu tubes, %zu blades, %zu instances, max depth %zu)\n",
           path3, ntubes, nblades, ninst, maxd3);
}
