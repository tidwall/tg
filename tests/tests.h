#ifndef TESTS_H
#define TESTS_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>
#include "../tg.h"

#define P(x, y) ((struct tg_point){(x), (y)})
#define S(a,b,c,d) ((struct tg_segment){P(a,b),P(c,d)})
#define R(a,b,c,d) ((struct tg_rect){P(a,b),P(c,d)})
#define SA(a,b,c,d) ((double[]){(a),(b),(c),(d)})

#define RING_INDEX(ix, ...) ({ \
    struct tg_point points[] = { __VA_ARGS__ }; \
    struct tg_ring *ring = tg_ring_new_ix(points, \
        sizeof(points)/sizeof(struct tg_point), (ix)); \
    assert(ring); \
    ring; \
})

#define RING(...)          gc_ring(RING_INDEX(TG_DEFAULT, __VA_ARGS__))
#define RING_DEFAULT(...)  gc_ring(RING_INDEX(TG_DEFAULT, __VA_ARGS__))
#define RING_NONE(...)     gc_ring(RING_INDEX(TG_NONE, __VA_ARGS__))
#define RING_NATURAL(...)  gc_ring(RING_INDEX(TG_NATURAL, __VA_ARGS__))
#define RING_YSTRIPES(...) gc_ring(RING_INDEX(TG_YSTRIPES, __VA_ARGS__))

#define CIRCLE_INDEX(ix, center, radius, steps) ({ \
    struct tg_ring *ring = tg_circle_new_ix((center), (radius), (steps), (ix)); \
    assert(ring); \
    ring; \
})
#define CIRCLE(center, radius, steps)          gc_ring(CIRCLE_INDEX(TG_DEFAULT, (center), (radius), (steps)))
#define CIRCLE_DEFAULT(center, radius, steps)  gc_ring(CIRCLE_INDEX(TG_DEFAULT, (center), (radius), (steps)))
#define CIRCLE_NONE(center, radius, steps)     gc_ring(CIRCLE_INDEX(TG_NONE, (center), (radius), (steps)))
#define CIRCLE_NATURAL(center, radius, steps)  gc_ring(CIRCLE_INDEX(TG_NATURAL, (center), (radius), (steps)))
#define CIRCLE_YSTRIPES(center, radius, steps) gc_ring(CIRCLE_INDEX(TG_YSTRIPES, (center), (radius), (steps)))

#define RANDOM_INDEX(ix, npoints) ({ \
    struct tg_ring *ring = load_random_ring((npoints), (ix)); \
    assert(ring); \
    ring; \
})
#define RANDOM(npoints)          gc_ring(RANDOM_INDEX(TG_DEFAULT, (npoints)))
#define RANDOM_DEFAULT(npoints)  gc_ring(RANDOM_INDEX(TG_DEFAULT, (npoints)))
#define RANDOM_NONE(npoints)     gc_ring(RANDOM_INDEX(TG_NONE, (npoints)))
#define RANDOM_NATURAL(npoints)  gc_ring(RANDOM_INDEX(TG_NATURAL, (npoints)))
#define RANDOM_YSTRIPES(npoints) gc_ring(RANDOM_INDEX(TG_YSTRIPES, (npoints)))



// expose private features
struct ring_result {
    bool hit; // contains/intersects
    int idx;  // edge index
};

enum tg_raycast_result {
    TG_OUT, // point is above, below, or to the right of the segment
    TG_IN,  // point is to the left (and inside the vertical bounds)
    TG_ON,  // point is on the segment
};

void *tg_malloc(size_t size);
void *tg_realloc(void *ptr, size_t size);
void tg_free(void *ptr);

bool tg_ring_empty(const struct tg_ring *ring);
bool tg_line_empty(const struct tg_line *line);
bool tg_poly_empty(const struct tg_poly *poly);
void tg_rect_search(struct tg_rect rect, struct tg_rect target, 
    bool(*iter)(const struct tg_segment seg, int index, void *udata),
    void *udata);
void tg_ring_search(const struct tg_ring *ring, struct tg_rect rect, 
    bool(*iter)(const struct tg_segment seg, int index, void *udata), 
    void *udata);
void tg_line_search(const struct tg_line *ring, struct tg_rect rect, 
    bool(*iter)(const struct tg_segment seg, int index, void *udata), 
    void *udata);
void tg_geom_foreach(const struct tg_geom *geom, 
    bool(*iter)(const struct tg_geom *geom, void *udata), void *udata);
size_t tg_boxed_point_size(void);
struct tg_geom *tg_fill_boxed_point_memory(void *mem, struct tg_point point);

struct ring_result tg_ring_contains_point(const struct tg_ring *ring, 
    struct tg_point point, bool allow_on_edge);

enum tg_raycast_result tg_raycast(struct tg_segment s, struct tg_point p);

bool tg_ring_intersects_segment(const struct tg_ring *ring, 
    struct tg_segment seg, bool allow_on_edge);

bool tg_ring_contains_segment(const struct tg_ring *ring, 
    struct tg_segment seg, bool allow_on_edge);

bool tg_ring_contains_ring(const struct tg_ring *ring,
    const struct tg_ring *other, bool allow_on_edge);

bool tg_ring_intersects_ring(const struct tg_ring *ring,
    const struct tg_ring *other, bool allow_on_edge);

bool tg_ring_intersects_line(const struct tg_ring *ring, 
    const struct tg_line *line, bool allow_on_edge);

int tg_ring_index_num_levels(const struct tg_ring *ring);
int tg_ring_index_level_num_rects(const struct tg_ring *ring, int levelidx);
struct tg_rect tg_ring_index_level_rect(const struct tg_ring *ring, 
    int levelidx, int rectidx);

struct tg_poly *tg_poly_move(const struct tg_poly *poly, 
    double delta_x, double delta_y);
struct tg_point tg_point_move(struct tg_point point, 
    double delta_x, double delta_y);
struct tg_segment tg_segment_move(struct tg_segment seg, 
    double delta_x, double delta_y);
struct tg_rect tg_rect_move(struct tg_rect rect, 
    double delta_x, double delta_y);
struct tg_line *tg_line_move(const struct tg_line *line, 
    double delta_x, double delta_y);
struct tg_ring *tg_ring_move(const struct tg_ring *ring, 
    double delta_x, double delta_y);
double tg_ring_polsby_popper_score(const struct tg_ring *ring);
double tg_line_polsby_popper_score(const struct tg_line *line);
size_t tg_aligned_size(size_t size);
struct tg_ring *tg_circle_new(struct tg_point center, double radius, int steps);
struct tg_ring *tg_circle_new_ix(struct tg_point center, double radius, 
    int steps, enum tg_index ix);
bool tg_geom_within0(const struct tg_geom *geom, const struct tg_geom *other);
double tg_point_distance_point(struct tg_point a, struct tg_point b);
double tg_point_distance_segment(struct tg_point p, struct tg_segment s);
double tg_point_distance_rect(struct tg_point p, struct tg_rect r);
double tg_segment_distance_segment(struct tg_segment a, struct tg_rect b);
double tg_segment_distance_rect(struct tg_segment s, struct tg_rect r);
double tg_rect_distance_rect(struct tg_rect a, struct tg_rect b);
bool tg_rect_intersects_point(struct tg_rect rect, struct tg_point point);
int tg_rect_num_points(struct tg_rect rect);
struct tg_point tg_rect_point_at(struct tg_rect rect, int index);
int tg_rect_num_segments(struct tg_rect rect);
struct tg_segment tg_rect_segment_at(struct tg_rect rect, int index);
bool tg_geom_covers(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_covers_point(const struct tg_geom *a, struct tg_point b);
bool tg_geom_covers_xy(const struct tg_geom *a, double x, double y);
bool tg_point_covers_point(struct tg_point a, struct tg_point b);
bool tg_point_covers_rect(struct tg_point a, struct tg_rect b);
bool tg_point_covers_line(struct tg_point a, const struct tg_line *b);
bool tg_point_covers_poly(struct tg_point a, const struct tg_poly *b);
bool tg_segment_covers_segment(struct tg_segment a, struct tg_segment b);
bool tg_segment_covers_point(struct tg_segment a, struct tg_point b);
bool tg_segment_covers_rect(struct tg_segment a, struct tg_rect b);
bool tg_rect_covers_point(struct tg_rect a, struct tg_point b);
bool tg_rect_covers_xy(struct tg_rect a, double x, double y);
bool tg_rect_covers_rect(struct tg_rect a, struct tg_rect b);
bool tg_rect_covers_line(struct tg_rect a, const struct tg_line *b);
bool tg_rect_covers_poly(struct tg_rect a, const struct tg_poly *b);
bool tg_line_covers_point(const struct tg_line *a, struct tg_point b);
bool tg_line_covers_rect(const struct tg_line *a, struct tg_rect b);
bool tg_line_covers_line(const struct tg_line *a, const struct tg_line *b);
bool tg_line_covers_poly(const struct tg_line *a, const struct tg_poly *b);
bool tg_poly_covers_point(const struct tg_poly *a, struct tg_point b);
bool tg_poly_covers_rect(const struct tg_poly *a, struct tg_rect b);
bool tg_poly_covers_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_covers_poly(const struct tg_poly *a, const struct tg_poly *b);
bool tg_poly_covers_xy(const struct tg_poly *a, double x, double y);
bool tg_geom_intersects(const struct tg_geom *a, const struct tg_geom *b);
bool tg_poly_intersects_point(const struct tg_poly *a, struct tg_point b);
bool tg_poly_intersects_rect(const struct tg_poly *a, struct tg_rect b);
bool tg_poly_intersects_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_intersects_poly(const struct tg_poly *a, const struct tg_poly *b);
int tg_geom_de9im_dims(const struct tg_geom *geom);
enum tg_index tg_index_extract_spread(enum tg_index ix, int *spread);
bool tg_point_touches_line(struct tg_point point, const struct tg_line *line);
bool tg_poly_contains_geom(struct tg_poly *a, const struct tg_geom *b);
bool tg_line_contains_geom(struct tg_line *a, const struct tg_geom *b);
bool tg_point_contains_geom(struct tg_point a, const struct tg_geom *b);
bool tg_poly_touches_geom(struct tg_poly *a, const struct tg_geom *b);
bool tg_line_touches_geom(struct tg_line *a, const struct tg_geom *b);
bool tg_point_touches_geom(struct tg_point a, const struct tg_geom *b);
bool tg_poly_intersects_point(const struct tg_poly *a, struct tg_point b);
bool tg_poly_intersects_rect(const struct tg_poly *a, struct tg_rect b);
bool tg_poly_intersects_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_intersects_poly(const struct tg_poly *a, const struct tg_poly *b);
bool tg_line_intersects_point(const struct tg_line *a, struct tg_point b);
bool tg_line_intersects_rect(const struct tg_line *a, struct tg_rect b);
bool tg_line_intersects_line(const struct tg_line *a, const struct tg_line *b);
bool tg_line_intersects_poly(const struct tg_line *a, const struct tg_poly *b);
bool tg_point_intersects_point(struct tg_point a, struct tg_point b);
bool tg_point_intersects_rect(struct tg_point a, struct tg_rect b);
bool tg_point_intersects_line(struct tg_point a, const struct tg_line *b);
bool tg_point_intersects_poly(struct tg_point a, const struct tg_poly *b);
bool tg_rect_intersects_rect(struct tg_rect a, struct tg_rect b);
bool tg_rect_intersects_line(struct tg_rect a, const struct tg_line *b);
bool tg_rect_intersects_poly(struct tg_rect a, const struct tg_poly *b);
bool tg_segment_intersects_segment(struct tg_segment a, struct tg_segment b);
bool tg_geom_crosses(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_overlaps(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_intersects_point(const struct tg_geom *a, struct tg_point b);
void tg_ring_ring_search(const struct tg_ring *a, const struct tg_ring *b, 
    bool (*iter)(struct tg_segment seg_a, int index_a, struct tg_segment seg_b, 
        int index_b, void *udata),
    void *udata);
void tg_line_line_search(const struct tg_line *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment seg_a, int index_a, struct tg_segment seg_b, 
        int index_b, void *udata),
    void *udata);
void tg_ring_line_search(const struct tg_ring *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment seg_a, int index_a, struct tg_segment seg_b, 
        int index_b, void *udata),
    void *udata);
bool tg_ring_contains_line(const struct tg_ring *a, const struct tg_line *b, 
    bool allow_on_edge, bool respect_boundaries);
bool tg_poly_contains_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_touches_line(const struct tg_poly *a, const struct tg_line *b);
enum tg_index tg_env_get_default_index(void);
int tg_env_get_index_spread(void);
enum tg_index tg_index_with_spread(enum tg_index ix, int spread);
int tg_geom_multi_index_spread(const struct tg_geom *geom);
int tg_geom_multi_index_num_levels(const struct tg_geom *geom);
int tg_geom_multi_index_level_num_rects(const struct tg_geom *geom, int levelidx);
struct tg_rect tg_geom_multi_index_level_rect(const struct tg_geom *geom, int levelidx, int rectidx);
uint32_t tg_point_hilbert(struct tg_point point, struct tg_rect rect);

uint64_t mkrandseed(void) {
    uint64_t seed = 0;
    FILE *urandom = fopen("/dev/urandom", "r");
    assert(urandom);
    assert(fread(&seed, sizeof(uint64_t), 1, urandom));
    fclose(urandom);
    return seed;
}

void seedrand(void) {
    uint64_t seed = mkrandseed();
    srand(seed);
}

#define do_test(name) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        printf("%s\n", #name); \
        seedrand(); \
        init_test_allocator(false); \
        name(); \
        cleanup(); \
        cleanup_test_allocator(); \
    } \
}

#define do_chaos_test(name) { \
    if (argc < 2 || strstr(#name, argv[1])) { \
        printf("%s\n", #name); \
        seedrand(); \
        init_test_allocator(true); \
        name(); \
        cleanup(); \
        cleanup_test_allocator(); \
    } \
}

int test_rings_len = 0;
struct tg_ring **test_rings = NULL;

int test_lines_len = 0;
struct tg_line **test_lines = NULL;

int test_polys_len = 0;
struct tg_poly **test_polys = NULL;

int test_geoms_len = 0;
struct tg_geom **test_geoms = NULL;


struct tg_ring *gc_ring(struct tg_ring *ring) {
    test_rings_len++;
    test_rings = realloc(test_rings, sizeof(struct tg_ring*)*test_rings_len);
    assert(test_rings);
    test_rings[test_rings_len-1] = ring;
    return ring;
}

struct tg_line *gc_line(struct tg_line *line) {
    test_lines_len++;
    test_lines = realloc(test_lines, sizeof(struct tg_line*)*test_lines_len);
    assert(test_lines);
    test_lines[test_lines_len-1] = line;
    return line;
}

struct tg_poly * gc_poly(struct tg_poly *poly) {
    test_polys_len++;
    test_polys = realloc(test_polys, sizeof(struct tg_poly*)*test_polys_len);
    assert(test_polys);
    test_polys[test_polys_len-1] = poly;
    return poly;
}

struct tg_geom *gc_geom(struct tg_geom *geom) {
    test_geoms_len++;
    test_geoms = realloc(test_geoms, sizeof(struct tg_geom*)*test_geoms_len);
    assert(test_geoms);
    test_geoms[test_geoms_len-1] = geom;
    return geom;
}

void cleanup(void) {
    for (int i = 0; i < test_rings_len; i++) {
        tg_ring_free(test_rings[i]);
    }
    test_rings_len = 0;
    if (test_rings) free(test_rings);
    test_rings = NULL;

    for (int i = 0; i < test_lines_len; i++) {
        tg_line_free(test_lines[i]);
    }
    test_lines_len = 0;
    if (test_lines) free(test_lines);
    test_lines = NULL;

    for (int i = 0; i < test_polys_len; i++) {
        tg_poly_free(test_polys[i]);
    }
    test_polys_len = 0;
    if (test_polys) free(test_polys);
    test_polys = NULL;

    for (int i = 0; i < test_geoms_len; i++) {
        tg_geom_free(test_geoms[i]);
    }
    test_geoms_len = 0;
    if (test_geoms) free(test_geoms);
    test_geoms = NULL;
}

struct tg_ring *rect_to_ring_test(struct tg_rect rect){
    int npoints = tg_rect_num_points(rect);
    struct tg_point *points = malloc(sizeof(struct tg_point)*npoints);
    assert(points);
    for (int i = 0; i < npoints; i++) {
        points[i] = tg_rect_point_at(rect, i);
    }
    struct tg_ring *ring = tg_ring_new(points, npoints);
    assert(ring);
    free(points);
    gc_ring(ring);
    return ring;
}

#define RR(a,b,c,d) (rect_to_ring_test(R((a),(b),(c),(d)))) 

#define LINE(...) ({ \
    struct tg_point points[] = { __VA_ARGS__ }; \
    struct tg_line *line = tg_line_new(points, sizeof(points)/sizeof(struct tg_point)); \
    assert(line); \
    gc_line(line); \
    line; \
})

#define PNUMARGS(...)  (sizeof((const struct tg_ring*[]){__VA_ARGS__})/sizeof(struct tg_ring*))
#define POLY(exterior, ...) ({ \
    size_t nholes = PNUMARGS(__VA_ARGS__); \
    if (nholes > 100) { \
        fprintf(stderr, "cannot use POLY with over 100 holes\n"); \
        assert(nholes <= 100); \
    } \
    struct tg_ring *holes[100] = { __VA_ARGS__ }; \
    tg_poly_new_gc((exterior), (const struct tg_ring*const*)holes, nholes); \
})

// Runs a function on a polygon using every possible options.
#define DUAL_POLY_TEST(poly_, func) { \
    enum tg_index opts[] = { \
        TG_NONE,             TG_NATURAL,             \
        TG_NONE|TG_YSTRIPES, TG_NATURAL|TG_YSTRIPES, \
    }; \
    struct tg_poly *poly = (poly_); \
    const struct tg_ring *exterior = tg_poly_exterior(poly); \
    int npoints = tg_ring_num_points(exterior); \
    const struct tg_point *points = tg_ring_points(exterior); \
    int nholes = tg_poly_num_holes(poly); \
    struct tg_ring **holes = malloc(nholes*sizeof(struct tg_ring*)); \
    assert(holes); \
    for (int j = 0; j < nholes; j++) { \
        holes[j] = (struct tg_ring*)tg_poly_hole_at(poly, j); \
    } \
    for (size_t i = 0; i < sizeof(opts)/sizeof(enum tg_index); i++) { \
        struct tg_ring *exterior = tg_ring_new_ix(points, npoints, opts[i]); \
        struct tg_poly *poly = tg_poly_new(exterior, \
            (const struct tg_ring **)holes, nholes); \
        gc_ring(exterior); \
        func; \
        gc_poly(poly); \
    } \
    free(holes); \
}




bool pointeq(struct tg_point a, struct tg_point b) {
    return a.x == b.x && a.y == b.y;
}

bool recteq(struct tg_rect a, struct tg_rect b) {
    return pointeq(a.min, b.min) && pointeq(a.max, b.max);
}

bool segeq(struct tg_segment a, struct tg_segment b) {
    return pointeq(a.a, b.a) && pointeq(a.b, b.b);
}

// eqish should only be used for comparing calculations like distance or area.
// Spefically for meters where you need cm accuracy
bool eqish(double a, double b) {
    return fabs(a-b) < 0.001;
}

// create ring definitions
#define rectangle {0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0}
#define pentagon {2, 2}, {8, 0}, {10, 6}, {5, 10}, {0, 6}, {2, 2}
#define triangle {0, 0}, {10, 0}, {5, 10}, {0, 0}
#define trapezoid {0, 0}, {10, 0}, {8, 10}, {2, 10}, {0, 0}
#define octagon {3, 0}, {7, 0}, {10, 3}, {10, 7}, {7, 10}, {3, 10}, {0, 7}, {0, 3}, {3, 0}
#define concave1 {5, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 5}, {5, 5}, {5, 0}
#define concave2 {0, 0}, {5, 0}, {5, 5}, {10, 5}, {10, 10}, {0, 10}, {0, 0}
#define concave3 {0, 0}, {10, 0}, {10, 5}, {5, 5}, {5, 10}, {0, 10}, {0, 0}
#define concave4 {0, 0}, {10, 0}, {10, 10}, {5, 10}, {5, 5}, {0, 5}, {0, 0}
#define bowtie {0, 0}, {5, 4}, {10, 0}, {10, 10}, {5, 6}, {0, 10}, {0, 0}
#define notClosed {0, 0}, {10, 0}, {10, 10}, {0, 10}
#define u1 {0, 10}, {0, 0}, {10, 0}, {10, 10}
#define u2 {0, 0}, {10, 0}, {10, 10}, {0, 10}
#define u3 {10, 0}, {10, 10}, {0, 10}, {0, 0}
#define u4 {10, 10}, {0, 10}, {0, 0}, {10, 0}
#define v1 {0, 10}, {5, 0}, {10, 10}
#define v2 {0, 0}, {10, 5}, {0, 10}
#define v3 {10, 0}, {5, 10}, {0, 0}
#define v4 {10, 10}, {0, 5}, {10, 0}

#include "shapes.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wunknown-warning-option"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wgnu-empty-initializer"
#pragma clang diagnostic ignored "-Wzero-length-array"
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wc2x-extensions"
#endif
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Woverlength-strings"

size_t total_allocs = 0;
size_t total_mem = 0;

char *commaize(unsigned int n) {
    char s1[64];
    char *s2 = malloc(64);
    assert(s2);
    memset(s2, 0, sizeof(64));
    snprintf(s1, sizeof(s1), "%d", n);
    int i = strlen(s1)-1; 
    int j = 0;
	while (i >= 0) {
		if (j%3 == 0 && j != 0) {
            memmove(s2+1, s2, strlen(s2)+1);
            s2[0] = ',';
		}
        memmove(s2+1, s2, strlen(s2)+1);
		s2[0] = s1[i];
        i--;
        j++;
	}
	return s2;
}

double clock_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (now.tv_sec*1e9 + now.tv_nsec) / 1e9;
}

double now(void) {
    return clock_now();
}

static bool rand_alloc_fail = false;
// 1 in 10 chance malloc or realloc will fail.
static int rand_alloc_fail_odds = 10; 

static void *xmalloc(size_t size) {
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    void *mem = malloc(sizeof(uint64_t)+size);
    assert(mem);
    *(uint64_t*)mem = size;
    total_allocs++;
    total_mem += size;
    return (char*)mem+sizeof(uint64_t);
}

static void xfree(void *ptr) {
    if (ptr) {
        total_mem -= *(uint64_t*)((char*)ptr-sizeof(uint64_t));
        total_allocs--;
        free((char*)ptr-sizeof(uint64_t));
    }
}

static void *xrealloc(void *ptr, size_t size) {
    if (!ptr) {
        return xmalloc(size);
    }
    if (rand_alloc_fail && rand()%rand_alloc_fail_odds == 0) {
        return NULL;
    }
    size_t psize = *(uint64_t*)((char*)ptr-sizeof(uint64_t));
    void *mem = realloc((char*)ptr-sizeof(uint64_t), sizeof(uint64_t)+size);
    assert(mem);
    *(uint64_t*)mem = size;
    total_mem -= psize;
    total_mem += size;
    return (char*)mem+sizeof(uint64_t);
}

void init_test_allocator(bool random_failures) {
    rand_alloc_fail = random_failures;
    tg_env_set_allocator(xmalloc, xrealloc, xfree);
}

void cleanup_test_allocator(void) {
    if (total_allocs > 0 || total_mem > 0) {
        fprintf(stderr, "test failed: %d unfreed allocations, %d bytes\n",
            (int)total_allocs, (int)total_mem);
        exit(1);
    }
    tg_env_set_allocator(NULL, NULL, NULL);
}


struct tg_line *tg_line_new_gc(const struct tg_point *points, int npoints) {
    struct tg_line *line = tg_line_new(points, npoints);
    gc_line(line);
    return line;
}

struct tg_line *tg_line_move_gc(const struct tg_line *line,
    double delta_x, double delta_y)
{
    struct tg_line *line2 = tg_line_move(line, delta_x, delta_y);
    gc_line(line2);
    return line2;
}

struct tg_ring *tg_ring_move_gc(const struct tg_ring *ring,
    double delta_x, double delta_y)
{
    struct tg_ring *ring2 = tg_ring_move(ring, delta_x, delta_y);
    gc_ring(ring2);
    return ring2;
}


struct tg_poly *tg_poly_new_gc(const struct tg_ring *exterior, 
    const struct tg_ring *const holes[], int nholes) 
{
    struct tg_poly *poly = tg_poly_new(exterior, holes, nholes);
    gc_poly(poly);
    return poly;
}

struct tg_poly *tg_poly_move_gc(const struct tg_poly *poly,
    double delta_x, double delta_y)
{
    struct tg_poly *poly2 = tg_poly_move(poly, delta_x, delta_y);
    gc_poly(poly2);
    return poly2;

}

double rand_double(void) {
    return (double)rand()/((double)(RAND_MAX)+1);
}

struct tg_point rand_point(struct tg_rect rect) {
    return (struct tg_point) { 
        (rect.max.x-rect.min.x)*rand_double()+rect.min.x,
        (rect.max.y-rect.min.y)*rand_double()+rect.min.y,
    };
}


void rand_str(char *buf, size_t n) {
    const char chars[] = "abcdefghijklmnopqrstuvwxyz 01234";
    for (size_t i = 0; i < n; i++) {
        buf[i] = chars[(rand()&31)];
    }
    buf[n] = '\0';
}

void rand_hex(char *buf, size_t n) {
    const char chars[] = "0123456789ABCDEF";
    for (size_t i = 0; i < n; i++) {
        buf[i] = chars[(rand()&15)];
    }
    buf[n] = '\0';
}

struct tg_segment rand_segment(struct tg_rect rect) {
    double w = (rect.max.x-rect.min.x);
    struct tg_point a = (struct tg_point) { 
        (rect.max.x-rect.min.x)*rand_double()+rect.min.x,
        (rect.max.y-rect.min.y)*rand_double()+rect.min.y,
    };
    struct tg_point b = a;
    if (rand() % 2) {
        b.x -= w * 0.1 * rand_double();
    } else {
        b.x += w * 0.1 * rand_double();
    }
    if (rand() % 2) {
        b.y -= w * 0.1 * rand_double();
    } else {
        b.y += w * 0.1 * rand_double();
    }
    return (struct tg_segment) { a, b };
}

struct tg_poly *az_with_hole(void) {
    struct tg_ring *ring1 = RING_NONE(az);
    // add a 1x1 hole to the center of arizona
    struct tg_point center = tg_rect_center(tg_ring_rect(ring1));
    struct tg_point a = tg_point_move(center, -0.5, -0.5);
    struct tg_point b = tg_point_move(center, 0.5, -0.5);
    struct tg_point c = tg_point_move(center, 0.5, 0.5);
    struct tg_point d = tg_point_move(center, -0.5, 0.5);
    struct tg_ring *hole1 = RING(a, b, c, d, a);
    return POLY(ring1, hole1);
}

char *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    size_t n = ftell(f);
    rewind(f);
    char *data = malloc(n+1);
    assert(data);
    assert(fread(data, 1, n, f) == n);
    data[n] = 0;
    fclose(f);
    if (size) *size = n;
    return data;
}

void cmpfullrect(int dims, double min[4], double max[4], int dims2, double min2[4], double max2[4]) {
    if (dims != dims2) {
        printf("expected\n\tdims( %d )\ngot\n\tdims( %d )\n", dims, dims2);
        assert(0);
    }
    for (int i = 0; i < dims; i++) {
        if (min[i] != min2[i] || max[i] != max2[i]) {
            printf("expected\n\tmin( ");
            for (int i = 0; i < dims; i++) {
                printf("%.0f ", min2[i]);
            }
            printf(") max( ");
            for (int i = 0; i < dims; i++) {
                printf("%.0f ", max2[i]);
            }
            printf(")\ngot\n\tmin( ");
            for (int i = 0; i < dims; i++) {
                printf("%.0f ", min[i]);
            }
            printf(") max( ");
            for (int i = 0; i < dims; i++) {
                printf("%.0f ", max[i]);
            }
            printf(")\n");
            assert(0);
        }
    }
}

static double *make_polygon(int n, double x, double y, double r, double c, double s) {
    s = s < 0.0 ? 0.0 : s > 1.0 ? 1.0 : s;
    c = c < 0.0 ? 0.0 : c > 1.0 ? 1.0 : c;
    n = n < 3 ? 3 : n;
    double *points = malloc(sizeof(double)*n*2);
    if (!points) return NULL;
    srand(time(NULL));
    double p = 360.0/(double)(n-1);
    double sx = 0;
    double sy = 0;
    for (int i = 0; i < n; i++) {
        double j = (double)i;
        double th = p*j + p*0.5 - p*(s*((double)rand()/(double)RAND_MAX));
        double radians = (M_PI / 180.0) * th;
        double e = r - r*(c*((double)rand()/(double)RAND_MAX));
        double px = x + cos(radians)*e;
        double py = y + sin(radians)*e;
        if (i == n-1) {
            px = sx;
            py = sy;
        }
        points[i*2+0] = px; 
        points[i*2+1] = py;
        if (i == 0) {
            sx = px;
            sy = py;
        } 
    }
    return points;
}

static struct tg_point *make_rand_polygon(int npoints) {
    double s = 0.7;
    double c = 0.7;
    double x = -111.9575;
    double y = 33.4329;
    double r = 0.002;
    struct tg_point *points = (void*)make_polygon(npoints, x, y, r, c, s);
    assert(points);
    return points;
}

struct tg_ring *load_random_ring(int npoints, enum tg_index ix) {
    struct tg_point *points = make_rand_polygon(npoints);
    if (!points) return 0;
    struct tg_ring *ring = tg_ring_new_ix(points, npoints, ix);
    free(points);
    return ring;
}


struct tg_geom *flip_geom(struct tg_geom *geom, enum tg_index ix) {
    // only works for polygons, and only the exterior points
    const struct tg_ring *ring = tg_poly_exterior(tg_geom_poly(geom));
    if (!ring) {
        return NULL;
    }
    struct tg_rect rect = tg_ring_rect(ring);
    double h = rect.max.y - rect.min.y;
    int npoints = tg_ring_num_points(ring);
    const struct tg_point *points = tg_ring_points(ring);
    struct tg_point *points2 = malloc(npoints * sizeof(struct tg_point));
    assert(points2);
    for (int i = 0; i < npoints; i++) {
        struct tg_point point = points[i];
        points2[i].x = point.x;
        points2[i].y = ((1.0 - ((point.y-rect.min.y) / h)) * h) + rect.min.y;
    }
    struct tg_geom *geom2 = (struct tg_geom *)tg_ring_new_ix(points2, npoints, ix);
    free(points2);
    return (struct tg_geom*)geom2;
}

struct tg_geom *_load_geom(const char *name, enum tg_index ix, bool flipped) {
    struct tg_geom *geom;
    if (strlen(name) >= 2 && strncmp(name, "rd", 2) == 0) {
        int npoints = 100;
        if (name[2] == '.') {
            npoints = atoi(name+3);
        }
        geom = (struct tg_geom*)load_random_ring(npoints, ix);
    } else if (strcmp(name, "br") == 0) {
        struct tg_point points[] = { br };
        size_t npoints = sizeof(points)/sizeof(struct tg_point);
        geom = (struct tg_geom *)tg_ring_new_ix(points, npoints, ix);
    } else if (strcmp(name, "bc") == 0) {
        struct tg_point points[] = { bc };
        size_t npoints = sizeof(points)/sizeof(struct tg_point);
        geom = (struct tg_geom *)tg_ring_new_ix(points, npoints, ix);
    } else if (strcmp(name, "az") == 0) {
        struct tg_point points[] = { az };
        size_t npoints = sizeof(points)/sizeof(struct tg_point);
        geom = (struct tg_geom *)tg_ring_new_ix(points, npoints, ix);
    } else if (strcmp(name, "tx") == 0) {
        struct tg_point points[] = { tx };
        size_t npoints = sizeof(points)/sizeof(struct tg_point);
        geom = (struct tg_geom *)tg_ring_new_ix(points, npoints, ix);
    } else if (strcmp(name, "ri") == 0) {
        struct tg_point points[] = { ri };
        size_t npoints = sizeof(points)/sizeof(struct tg_point);
        geom = (struct tg_geom *)tg_ring_new_ix(points, npoints, ix);
    } else {
        fprintf(stderr, "invalid geom name '%s'\n", name);
        abort();
    }
    if (flipped) {
        struct tg_geom *geom2 = flip_geom(geom, ix);
        tg_free(geom);
        geom = geom2;
    }
    return geom;
}
struct tg_geom *load_geom(const char *name, enum tg_index ix) {
    return _load_geom(name, ix, false);
}
struct tg_geom *load_geom_flipped(const char *name, enum tg_index ix) {
    return _load_geom(name, ix, true);
}

#include "relations.h" // Auto generated from "run.sh"

#endif // TESTS_H
