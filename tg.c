// https://github.com/tidwall/tg
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <math.h>
#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "tg.h"

/******************************************************************************

Implementation Notes:
- Right-hand-rule is not required for polygons.
- The "properties" GeoJSON field is not required for Features.
- All polygons with at least 32 points are indexed automatically (TG_NATURAL).

Developer notes:
- This tg.c source file includes the entire tg library. (amalgamation)
- All dependencies are embedded between the BEGIN/END tags.
- Do not edit a dependency directly in this file. Instead edit the file in
the deps directory and then run deps/embed.sh to replace out its code in
this file.

*******************************************************************************/

enum base {
    BASE_POINT = 1, // tg_point
    BASE_LINE  = 2, // tg_line
    BASE_RING  = 3, // tg_ring
    BASE_POLY  = 4, // tg_poly
    BASE_GEOM  = 5, // tg_geom
};

enum flags {
    HAS_Z          = 1<<0,  // Geometry has additional Z coordinates
    HAS_M          = 1<<1,  // Geometry has additional M coordinates
    IS_ERROR       = 1<<2,  // Geometry is a parse error. Falls back to POINT
    IS_EMPTY       = 1<<3,  // Same as a GeoJSON null object (empty coords)
    IS_FEATURE     = 1<<4,  // GeoJSON. Geometry is Feature
    IS_FEATURE_COL = 1<<5,  // GeoJSON. GeometryGollection is FeatureCollection
    HAS_NULL_PROPS = 1<<6,  // GeoJSON. 'Feature' with 'properties'=null
    IS_UNLOCATED   = 1<<7,  // GeoJSON. 'Feature' with 'geometry'=null
};

// Optionally use non-atomic reference counting when TG_NOATOMICS is defined.
#ifdef TG_NOATOMICS

typedef int rc_t;
static void rc_init(rc_t *rc) {
    *rc = 0;
}
static void rc_retain(rc_t *rc) {
    *rc++;
}
static bool rc_release(rc_t *rc) {
    *rc--;
    return *rc == 1;
}

#else

#include <stdatomic.h>

/*
The relaxed/release/acquire pattern is based on:
http://boost.org/doc/libs/1_87_0/libs/atomic/doc/html/atomic/usage_examples.html
*/

typedef atomic_int rc_t;
static void rc_init(rc_t *rc) {
    atomic_init(rc, 0);
}
static void rc_retain(rc_t *rc) {
    atomic_fetch_add_explicit(rc, 1, __ATOMIC_RELAXED);
}
static bool rc_release(rc_t *rc) {
    if (atomic_fetch_sub_explicit(rc, 1, __ATOMIC_RELEASE) == 1) {
        atomic_thread_fence(__ATOMIC_ACQUIRE);
        return true;
    }
    return false;
}

#endif

struct head { 
    rc_t rc;
    bool noheap;
    enum base base:4;
    enum tg_geom_type type:4;
    enum flags flags:8;
};

/// A ring is series of tg_segment which creates a shape that does not
/// self-intersect and is fully closed, where the start and end points are the
/// exact same value.
///
/// **Creating**
///
/// To create a new ring use the tg_ring_new() function.
///
/// ```
/// struct tg_ring *ring = tg_ring_new(points, npoints);
/// ```
///
/// **Upcasting**
///
/// A tg_ring can always be safely upcasted to a tg_poly or tg_geom; allowing
/// it to use any tg_poly_&ast;() or tg_geom_&ast;() function.
///
/// ```
/// struct tg_poly *poly = (struct tg_poly*)ring; // Cast to a tg_poly
/// struct tg_geom *geom = (struct tg_geom*)ring; // Cast to a tg_geom
/// ```
/// @see RingFuncs
/// @see PolyFuncs
struct tg_ring {
    struct head head;
    bool closed;
    bool clockwise;
    bool convex;
    double area;
    int npoints;
    int nsegs;
    struct tg_rect rect;
    struct index *index;
    struct ystripes *ystripes;
    struct tg_point points[]; 
};

/// A line is a series of tg_segment that make up a linestring.
///
/// **Creating**
///
/// To create a new line use the tg_line_new() function.
///
/// ```
/// struct tg_line *line = tg_line_new(points, npoints);
/// ```
///
/// **Upcasting**
///
/// A tg_line can always be safely upcasted to a tg_geom; allowing
/// it to use any tg_geom_&ast;() function.
///
/// ```
/// struct tg_geom *geom = (struct tg_geom*)line; // Cast to a tg_geom
/// ```
///
/// @see LineFuncs
struct tg_line { int _; };

/// A polygon consists of one exterior ring and zero or more holes.
/// 
/// **Creating**
///
/// To create a new polygon use the tg_poly_new() function.
///
/// ```
/// struct tg_poly *poly = tg_poly_new(exterior, holes, nholes);
/// ```
///
/// **Upcasting**
///
/// A tg_poly can always be safely upcasted to a tg_geom; allowing
/// it to use any tg_geom_&ast;() function.
///
/// ```
/// struct tg_geom *geom = (struct tg_geom*)poly; // Cast to a tg_geom
/// ```
///
/// @see PolyFuncs
struct tg_poly {
    struct head head;
    struct tg_ring *exterior;
    int nholes;
    struct tg_ring **holes;
};

struct multi {
    struct tg_geom **geoms;
    int ngeoms;
    struct tg_rect rect; // unioned rect child geometries
    struct index *index; // geometry index, or NULL if not indexed
    int *ixgeoms;        // indexed geometries, or NULL if not indexed
};

/// A geometry is the common generic type that can represent a Point,
/// LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, or 
/// GeometryCollection. 
/// 
/// For geometries that are derived from GeoJSON, they may have addtional 
/// attributes such as being a Feature or a FeatureCollection; or include
/// extra json fields.
///
/// **Creating**
///
/// To create a new geometry use one of the @ref GeometryConstructors or
/// @ref GeometryParsing functions.
///
/// ```
/// struct tg_geom *geom = tg_geom_new_point(point);
/// struct tg_geom *geom = tg_geom_new_polygon(poly);
/// struct tg_geom *geom = tg_parse_geojson(geojson);
/// ```
///
/// **Upcasting**
///
/// Other types, specifically tg_line, tg_ring, and tg_poly, can be safely
/// upcasted to a tg_geom; allowing them to use any tg_geom_&ast;()
/// function.
///
/// ```
/// struct tg_geom *geom1 = (struct tg_geom*)line; // Cast to a LineString
/// struct tg_geom *geom2 = (struct tg_geom*)ring; // Cast to a Polygon
/// struct tg_geom *geom3 = (struct tg_geom*)poly; // Cast to a Polygon
/// ```
///
/// @see GeometryConstructors
/// @see GeometryAccessors
/// @see GeometryPredicates
/// @see GeometryParsing
/// @see GeometryWriting
struct tg_geom {
    struct head head;
    union {
        struct tg_point point;
        struct tg_line *line;
        struct tg_poly *poly;
        struct multi *multi;
    };
    union {
        struct {  // TG_POINT
            double z;
            double m;
        };
        struct {  // !TG_POINT
            double *coords; // extra dimensinal coordinates
            int ncoords;
        };
    };
    union {
        char *xjson; // extra json fields, such as "id", "properties", etc.
        char *error; // an error message, when flag IS_ERROR
    };
};

struct boxed_point {
    struct head head;
    struct tg_point point;
};

#define todo(msg) { \
    fprintf(stderr, "todo: %s, line: %d\n", (msg), __LINE__); \
    exit(1); \
}

// private methods
bool tg_ring_empty(const struct tg_ring *ring);
bool tg_line_empty(const struct tg_line *line);
bool tg_poly_empty(const struct tg_poly *poly);
void tg_rect_search(struct tg_rect rect, struct tg_rect target, 
    bool(*iter)(struct tg_segment seg, int index, void *udata),
    void *udata);
void tg_ring_search(const struct tg_ring *ring, struct tg_rect rect, 
    bool(*iter)(struct tg_segment seg, int index, void *udata), 
    void *udata);
void tg_line_search(const struct tg_line *ring, struct tg_rect rect, 
    bool(*iter)(struct tg_segment seg, int index, void *udata), 
    void *udata);
void tg_geom_foreach(const struct tg_geom *geom, 
    bool(*iter)(const struct tg_geom *geom, void *udata), void *udata);
double tg_ring_polsby_popper_score(const struct tg_ring *ring);
double tg_line_polsby_popper_score(const struct tg_line *line);
int tg_rect_num_points(struct tg_rect rect);
struct tg_point tg_rect_point_at(struct tg_rect rect, int index);
int tg_rect_num_segments(struct tg_rect rect);
struct tg_segment tg_rect_segment_at(struct tg_rect rect, int index);
int tg_geom_de9im_dims(const struct tg_geom *geom);
bool tg_point_covers_point(struct tg_point a, struct tg_point b);
bool tg_point_covers_rect(struct tg_point a, struct tg_rect b);
bool tg_point_covers_line(struct tg_point a, const struct tg_line *b);
bool tg_point_covers_poly(struct tg_point a, const struct tg_poly *b);
bool tg_geom_covers_point(const struct tg_geom *a, struct tg_point b);
bool tg_geom_covers_xy(const struct tg_geom *a, double x, double y);
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
bool tg_poly_covers_xy(const struct tg_poly *a, double x, double y);
bool tg_poly_touches_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_coveredby_poly(const struct tg_poly *a, const struct tg_poly *b);
bool tg_poly_covers_point(const struct tg_poly *a, struct tg_point b);
bool tg_poly_covers_rect(const struct tg_poly *a, struct tg_rect b);
bool tg_poly_covers_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_covers_poly(const struct tg_poly *a, const struct tg_poly *b);
bool tg_poly_intersects_point(const struct tg_poly *a, struct tg_point b);
bool tg_poly_intersects_rect(const struct tg_poly *a, struct tg_rect b);
bool tg_poly_intersects_line(const struct tg_poly *a, const struct tg_line *b);
bool tg_poly_intersects_poly(const struct tg_poly *a, const struct tg_poly *b);
bool tg_geom_intersects_point(const struct tg_geom *a, struct tg_point b);

static_assert(sizeof(int) == 4 || sizeof(int) == 8,  "invalid int size");

// Function attribute for noinline.
#if defined(__GNUC__)
#define __attr_noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#define __attr_noinline __declspec(noinline)
#else
#define __attr_noinline
#endif

// Fast floating-point min and max for gcc and clang on arm64 and x64.
#if defined(__GNUC__) && defined(__aarch64__)
// arm64 already uses branchless operations for fmin and fmax
static inline double fmin0(double x, double y) {
    return fmin(x, y);
}
static inline double fmax0(double x, double y) {
    return fmax(x, y);
}
static inline float fminf0(float x, float y) {
    return fminf(x, y);
}
static inline float fmaxf0(float x, float y) {
    return fmaxf(x, y);
}
#elif defined(__GNUC__) && defined(__x86_64__)
// gcc/amd64 sometimes uses branching with fmin/fmax. 
// This code use single a asm op instead.
// https://gcc.gnu.org/bugzilla//show_bug.cgi?id=94497
#pragma GCC diagnostic push
#ifdef __clang__
#pragma clang diagnostic ignored "-Wlanguage-extension-token"
#endif
static inline double fmin0(double x, double y) {
    asm("minsd %1, %0" : "+x" (x) : "x" (y));
    return x;
}
static inline double fmax0(double x, double y) {
    asm("maxsd %1, %0" : "+x" (x) : "x" (y));
    return x;
}
static inline float fminf0(float x, float y) {
    asm("minss %1, %0" : "+x" (x) : "x" (y));
    return x;
}
static inline float fmaxf0(float x, float y) {
    asm("maxss %1, %0" : "+x" (x) : "x" (y));
    return x;
}
#pragma GCC diagnostic pop
#else
// for everything else, let the compiler figure it out.
static inline double fmin0(double x, double y) {
    return x < y ? x : y;
}
static inline double fmax0(double x, double y) {
    return x > y ? x : y;
}
static inline float fminf0(double x, float y) {
    return x < y ? x : y;
}
static inline float fmaxf0(float x, float y) {
    return x > y ? x : y;
}
#endif

static inline double fclamp0(double f, double min, double max) {
    return fmin0(fmax0(f, min), max);
}

static bool feq(double x, double y) {
    return !((x < y) | (x > y));
}

static bool eq_zero(double x) {
    return feq(x, 0);
}

static bool collinear(
    double x1, double y1, // point 1
    double x2, double y2, // point 2
    double x3, double y3  // point 3
) { 
    bool x1x2 = feq(x1, x2);
    bool x1x3 = feq(x1, x3);
    bool x2x3 = feq(x2, x3);
    bool y1y2 = feq(y1, y2);
    bool y1y3 = feq(y1, y3);
    bool y2y3 = feq(y2, y3);
    if (x1x2) {
        return x1x3;
    }
    if (y1y2) {
        return y1y3;
    }
    if ((x1x2 & y1y2) | (x1x3 & y1y3) | (x2x3 & y2y3)) {
        return true;
    }
    double cx1 = x3 - x1;
    double cy1 = y3 - y1;
    double cx2 = x2 - x1;
    double cy2 = y2 - y1;
    double s1 = cx1 * cy2;
    double s2 = cy1 * cx2;
    // Check if precision was lost.
    double s3 = (s1 / cy2) - cx1;
    double s4 = (s2 / cx2) - cy1;
    if (s3 < 0) {
        s1 = nexttoward(s1, -INFINITY);
    } else if (s3 > 0) {
        s1 = nexttoward(s1, +INFINITY);
    }
    if (s4 < 0) {
        s2 = nexttoward(s2, -INFINITY);
    } else if (s4 > 0) {
        s2 = nexttoward(s2, +INFINITY);
    }
    return eq_zero(s1 - s2);
}

static double length(double x1, double y1, double x2, double y2) {
    return sqrt((x1-x2) * (x1-x2) + (y1-y2) * (y1-y2));
}
    
#ifndef ludo
#define ludo
#define ludo1(i,f) f; i++;
#define ludo2(i,f) ludo1(i,f); ludo1(i,f);
#define ludo4(i,f) ludo2(i,f); ludo2(i,f);
#define ludo8(i,f) ludo4(i,f); ludo4(i,f);
#define ludo16(i,f) ludo8(i,f); ludo8(i,f);
#define ludo32(i,f) ludo16(i,f); ludo16(i,f);
#define ludo64(i,f) ludo32(i,f); ludo32(i,f);
#define for1(i,n,f) while(i+1<=(n)) { ludo1(i,f); }
#define for2(i,n,f) while(i+2<=(n)) { ludo2(i,f); } for1(i,n,f);
#define for4(i,n,f) while(i+4<=(n)) { ludo4(i,f); } for1(i,n,f);
#define for8(i,n,f) while(i+8<=(n)) { ludo8(i,f); } for1(i,n,f);
#define for16(i,n,f) while(i+16<=(n)) { ludo16(i,f); } for1(i,n,f);
#define for32(i,n,f) while(i+32<=(n)) { ludo32(i,f); } for1(i,n,f);
#define for64(i,n,f) while(i+64<=(n)) { ludo64(i,f); } for1(i,n,f);
#endif

static size_t grow_cap(size_t cap, size_t init_cap) {
    return cap == 0 ? init_cap : cap < 1000 ? cap * 2 : cap * 1.25;
}

#define print_segment(s) { \
    printf("(%f %f,%f %f)", (s).a.x, (s).a.y, (s).b.x, (s).b.y); \
}

#define print_geom(g) { \
    size_t sz = tg_geom_wkt(g, 0, 0); \
    char *wkt = tg_malloc(sz+1); \
    assert(wkt); \
    tg_geom_wkt(g, wkt, sz+1); \
    printf("%s\n", wkt); \
    free(wkt); \
}
/////////////////////
// global behaviors
/////////////////////

// Global variables shared by all TG functions.

static void *(*_malloc)(size_t) = NULL;
static void *(*_realloc)(void*, size_t) = NULL;
static void (*_free)(void*) = NULL;
static enum tg_index default_index = TG_NATURAL;
static int default_index_spread = 16;
static bool print_fixed_floats = false;

/// Set the floating point printing to be fixed size.
/// By default floating points are printed using their smallest textual 
/// representation. Such as 800000 is converted to "8e5". This is ideal when
/// both accuracy and size are desired. But there may be times when only
/// fixed epresentations are wanted, in that case set this param to true.
void tg_env_set_print_fixed_floats(bool print) {
    print_fixed_floats = print;
}

/// Allow for configuring a custom allocator.
///
/// This overrides the built-in malloc, realloc, and free functions for all
/// TG operations.
/// @warning This function, if needed, should be called **only once** at
/// program start up and prior to calling any other tg_*() function.
/// @see GlobalFuncs
void tg_env_set_allocator(
    void *(*malloc)(size_t), 
    void *(*realloc)(void*, size_t),
    void (*free)(void*)) 
{
    _malloc = malloc;
    _realloc = realloc;
    _free = free;
}

void *tg_malloc(size_t nbytes) {
    return (_malloc?_malloc:malloc)(nbytes);
}

void *tg_realloc(void *ptr, size_t nbytes) {
    return (_realloc?_realloc:realloc)(ptr, nbytes);
}

void tg_free(void *ptr) {
    (_free?_free:free)(ptr);
}

/// Set the geometry indexing default.
/// 
/// This is a global override to the indexing for all yet-to-be created 
/// geometries.
/// @warning This function, if needed, should be called **only once** at
/// program start up and prior to calling any other tg_*() function.
/// @see [tg_index](.#tg_index)
/// @see tg_env_set_index()
/// @see GlobalFuncs
void tg_env_set_index(enum tg_index ix) {
    switch (ix) {
    case TG_NONE: 
    case TG_NATURAL: 
    case TG_YSTRIPES:
        // only change for none, natural, and ystripes
        default_index = ix;
        break;
    default:
        // no change
        break;
    }
}

/// Get the current geometry indexing default.
/// @see [tg_index](.#tg_index)
/// @see tg_env_set_index()
/// @see GlobalFuncs
enum tg_index tg_env_get_default_index(void) {
    return default_index;
}

/// Set the default index spread.
/// 
/// The "spread" is how many rectangles are grouped together on an indexed
/// level before propagating up to a higher level.
/// 
/// Default is 16.
///
/// This is a global override to the indexing spread for all yet-to-be created 
/// geometries.
/// @warning This function, if needed, should be called **only once** at
/// program start up and prior to calling any other tg_*() function.
/// @see [tg_index](.#tg_index)
/// @see tg_env_set_index()
/// @see <a href="POLYGON_INDEXING.md">About TG indexing</a>
/// @see GlobalFuncs
void tg_env_set_index_spread(int spread) {
    // only allow spreads between 2 and 1024
    if (spread >= 2 && spread <= 4096) {
        default_index_spread = spread;
    }
}
int tg_env_get_index_spread(void) {
    return default_index_spread;
}

enum tg_index tg_index_with_spread(enum tg_index ix, int spread) {
    // Only 16 bits of the tg_index is used.
    // first 4 bits is the index. The next 12 is the spread.
    if (spread != 0) {
        spread = spread < 2 ? 2 : spread > 4096 ? 4096 : spread;
        spread--; // ensure range 1-4095 (but will actually be 2-4096)
    }
    return (ix & 0xF) | (spread << 4);
}

enum tg_index tg_index_extract_spread(enum tg_index ix, int *spread) {
    int ixspread = ((unsigned int)(ix >> 4)) & 4095;
    if (ixspread > 0) {
        ixspread++;
    }
    if (ixspread == 0) {
        ixspread = tg_env_get_index_spread();
    }
    if (spread) {
        *spread = ixspread;
    }
    return ix & 0xF;
}

////////////////////
// point
////////////////////

static bool pteq(struct tg_point a, struct tg_point b) {
    return feq(a.x, b.x) && feq(a.y, b.y);
}

/// Returns the minimum bounding rectangle of a point. 
/// @see PointFuncs
struct tg_rect tg_point_rect(struct tg_point point) {
    return (struct tg_rect){ .min = point, .max = point };
}

/// Tests whether a point fully contains another point.
/// @see PointFuncs
bool tg_point_covers_point(struct tg_point point, struct tg_point other) {
    return pteq(point, other);
}

bool tg_point_contains_point(struct tg_point point, struct tg_point other) {
    return pteq(point, other);
}

/// Tests whether a point intersects another point.
/// @see PointFuncs
bool tg_point_intersects_point(struct tg_point a, struct tg_point b) {
    return pteq(a, b);
}

bool tg_point_touches_point(struct tg_point a, struct tg_point b) {
    // Points do not have boundaries
    (void)a; (void)b;
    return false;
}

/// Tests whether a point fully contains a rectangle.
/// @see PointFuncs
bool tg_point_covers_rect(struct tg_point point, struct tg_rect rect) {
    return pteq(rect.min, point) && pteq(rect.max, point);
}

/// Tests whether a point fully intersects a rectangle.
/// @see PointFuncs
bool tg_point_intersects_rect(struct tg_point point, struct tg_rect rect) { 
    return tg_rect_covers_point(rect, point);
}

/// Tests whether a point fully contains a line.
/// @see PointFuncs
bool tg_point_covers_line(struct tg_point point, const struct tg_line *line) {
    return !tg_line_empty(line) && 
           tg_point_covers_rect(point, tg_line_rect(line));
}

bool tg_point_contains_line(struct tg_point point, const struct tg_line *line) {
    return !tg_line_empty(line) && 
           tg_point_covers_rect(point, tg_line_rect(line));
}

/// Tests whether a point intersects a line.
/// @see PointFuncs
bool tg_point_intersects_line(struct tg_point point, 
    const struct tg_line *line)
{
    return tg_line_intersects_point(line, point);
}

bool tg_point_touches_line(struct tg_point point, const struct tg_line *line) {
    int nsegs = tg_line_num_segments(line);
    if (nsegs == 0) {
        return false;
    }
    struct tg_segment s0 = tg_line_segment_at(line, 0);
    struct tg_segment sN = tg_line_segment_at(line, nsegs-1);
    return pteq(point, s0.a) || pteq(point, sN.b);
}

/// Tests whether a point fully contains a polygon.
/// @see PointFuncs
bool tg_point_covers_poly(struct tg_point point, const struct tg_poly *poly) {
    return !tg_poly_empty(poly) && 
           tg_point_covers_rect(point, tg_poly_rect(poly));
}

bool tg_point_contains_poly(struct tg_point point, const struct tg_poly *poly) {
    // not possible
    (void)point; (void)poly;
    return false;
}

/// Tests whether a point intersects a polygon.
/// @see PointFuncs
bool tg_point_intersects_poly(struct tg_point point,
    const struct tg_poly *poly)
{
    return tg_poly_intersects_point(poly, point);
}

bool tg_point_touches_poly(struct tg_point point, const struct tg_poly *poly) {
    // Return true if the point touches the boundary of the exterior ring or
    // the boundary of the interior holes. 
    const struct tg_ring *ring = tg_poly_exterior(poly);
    if (tg_line_covers_point((struct tg_line*)ring, point)) {
        return true;
    }
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0; i < nholes; i++) {
        const struct tg_ring *ring = tg_poly_hole_at(poly, i);
        if (tg_line_covers_point((struct tg_line*)ring, point)) {
           return true;
        }
    }
    return false;
}

////////////////////
// segment
////////////////////

static bool point_on_segment(struct tg_point p, struct tg_segment s) {
    if (!tg_rect_covers_point(tg_segment_rect(s), p)) {
        return false;
    }
    return collinear(s.a.x, s.a.y, s.b.x, s.b.y, p.x, p.y);
}

enum tg_raycast_result {
    TG_OUT,  // point is above, below, or to the right of the segment
    TG_IN,   // point is to the left (and inside the vertical bounds)
    TG_ON,   // point is on the segment
};

static enum tg_raycast_result raycast(struct tg_segment seg, 
    struct tg_point p)
{
    struct tg_rect r = tg_segment_rect(seg);
    if (p.y < r.min.y || p.y > r.max.y) {
        return TG_OUT;
    }
    if (p.x < r.min.x) {
        if (p.y != r.min.y && p.y != r.max.y) {
            return TG_IN;
        }
    } else if (p.x > r.max.x) {
        if (r.min.y != r.max.y && r.min.x != r.max.x) {
            return TG_OUT;
        }
    }
    struct tg_point a = seg.a;
    struct tg_point b = seg.b;
    if (b.y < a.y) {
        struct tg_point t = a;
        a = b;
        b = t;
    }
    if (pteq(p, a) || pteq(p, b)) {
        return TG_ON;
    }
    if (a.y == b.y) {
        if (a.x == b.x) {
            return TG_OUT;
        }
        if (p.y == b.y) {
            if (!(p.x < r.min.x || p.x > r.max.x)) {
                return TG_ON;
            }
        }
    }
    if (a.x == b.x && p.x == b.x) {
        if (p.y >= a.y && p.y <= b.y) {
            return TG_ON;
        }
    }
    if (collinear(a.x, a.y, b.x, b.y, p.x, p.y)) {
        if (p.x < r.min.x) {
            if (r.min.y == r.max.y) {
                return TG_OUT;
            }
        } else if (p.x > r.max.x) {
            return TG_OUT;
        }
        return TG_ON;
    }
    if (p.y == a.y || p.y == b.y) {
        p.y = nexttoward(p.y, INFINITY);
    }
    if (p.y < a.y || p.y > b.y) {
        return TG_OUT;
    }
    if (a.x > b.x) {
        if (p.x >= a.x) {
            return TG_OUT;
        }
        if (p.x <= b.x) {
            return TG_IN;
        }
    } else {
        if (p.x >= b.x) {
            return TG_OUT;
        }
        if (p.x <= a.x) {
            return TG_IN;
        }
    }
    if ((p.y-a.y)/(p.x-a.x) >= (b.y-a.y)/(b.x-a.x)) {
        return TG_IN;
    }
    return TG_OUT;
}

/// Performs the raycast operation of a point on segment
enum tg_raycast_result tg_raycast(struct tg_segment seg, struct tg_point p) {
    return raycast(seg, p);
}

struct tg_point tg_point_move(struct tg_point point, 
    double delta_x, double delta_y)
{
    return (struct tg_point){ .x = point.x + delta_x, .y = point.y + delta_y };
}

struct tg_segment tg_segment_move(struct tg_segment seg,
    double delta_x, double delta_y)
{
    return (struct tg_segment){
        .a = tg_point_move(seg.a, delta_x, delta_y),
        .b = tg_point_move(seg.b, delta_x, delta_y),
    };
}

/// Tests whether a segment fully contains a point.
/// @see SegmentFuncs
bool tg_segment_covers_point(struct tg_segment seg, struct tg_point p) {
    return point_on_segment(p, seg);
}

/// Tests whether a segment fully contains another segment.
/// @see SegmentFuncs
bool tg_segment_covers_segment(struct tg_segment a, struct tg_segment b) {
    return tg_segment_covers_point(a, b.a) && 
           tg_segment_covers_point(a, b.b);
}

static void segment_fill_rect(const struct tg_segment *seg, 
    struct tg_rect *rect)
{
    rect->min.x = fmin0(seg->a.x, seg->b.x);
    rect->min.y = fmin0(seg->a.y, seg->b.y);
    rect->max.x = fmax0(seg->a.x, seg->b.x);
    rect->max.y = fmax0(seg->a.y, seg->b.y);
}

/// Returns the minimum bounding rectangle of a segment. 
/// @see SegmentFuncs
struct tg_rect tg_segment_rect(struct tg_segment seg) {
    struct tg_rect rect;
    segment_fill_rect(&seg, &rect);
    return rect;
}

static bool rect_intersects_rect(struct tg_rect *a, struct tg_rect *b) {
    return !(b->min.x > a->max.x || b->max.x < a->min.x ||
             b->min.y > a->max.y || b->max.y < a->min.y);
}

/// Tests whether a rectangle intersects another rectangle.
/// @see RectFuncs
bool tg_rect_intersects_rect(struct tg_rect a, struct tg_rect b) {
    return rect_intersects_rect(&a, &b);
}

/// Tests whether a rectangle full contains another rectangle.
bool tg_rect_covers_rect(struct tg_rect a, struct tg_rect b) {
    return !(b.min.x < a.min.x || b.max.x > a.max.x ||
             b.min.y < a.min.y || b.max.y > a.max.y);
}

/// Returns the number of points. Always 5 for rects.
int tg_rect_num_points(struct tg_rect r) {
    (void)r;
    return 5;
}

/// Returns the number of segments. Always 4 for rects.
int tg_rect_num_segments(struct tg_rect r) {
    (void)r;
    return 4;
}

/// Returns the point at index.
struct tg_point tg_rect_point_at(struct tg_rect r, int index) {
    switch (index) {
    case 0:
        return (struct tg_point){ r.min.x, r.min.y };
    case 1:
        return (struct tg_point){ r.max.x, r.min.y };
    case 2:
        return (struct tg_point){ r.max.x, r.max.y };
    case 3:
        return (struct tg_point){ r.min.x, r.max.y };
    case 4:
        return (struct tg_point){ r.min.x, r.min.y };
    default:
        return (struct tg_point){ 0 };
    }
}

/// Returns the segment at index.
struct tg_segment tg_rect_segment_at(struct tg_rect r, int index) {
    switch (index) {
    case 0:
        return (struct tg_segment){ { r.min.x, r.min.y}, { r.max.x, r.min.y} };
    case 1:
        return (struct tg_segment){ { r.max.x, r.min.y}, { r.max.x, r.max.y} };
    case 2:
        return (struct tg_segment){ { r.max.x, r.max.y}, { r.min.x, r.max.y} };
    case 3:
        return (struct tg_segment){ { r.min.x, r.max.y}, { r.min.x, r.min.y} };
    default:
        return (struct tg_segment){ 0 };
    }
}

/// Tests whether a segment intersects another segment.
/// @see SegmentFuncs
bool tg_segment_intersects_segment(struct tg_segment seg_a, 
    struct tg_segment seg_b)
{
    struct tg_point a = seg_a.a;
    struct tg_point b = seg_a.b;
    struct tg_point c = seg_b.a;
    struct tg_point d = seg_b.b;
    if (!tg_rect_intersects_rect(tg_segment_rect(seg_a), 
        tg_segment_rect(seg_b)))
    {
        return false;
    }
    
    if (pteq(seg_a.a, seg_b.a) || pteq(seg_a.a, seg_b.b) ||
        pteq(seg_a.b, seg_b.a) || pteq(seg_a.b, seg_b.b))
    {
        return true;
    }

    double cmpx = c.x-a.x;
    double cmpy = c.y-a.y;
    double rx = b.x-a.x;
    double ry = b.y-a.y;
    double cmpxr = cmpx*ry - cmpy*rx;
    if (eq_zero(cmpxr)) {
        // collinear, and so intersect if they have any overlap
        if (!(((c.x-a.x <= 0) != (c.x-b.x <= 0))) ||
              ((c.y-a.y <= 0) != (c.y-b.y <= 0)))
        {
            return tg_segment_covers_point(seg_a, seg_b.a) || 
                   tg_segment_covers_point(seg_a, seg_b.b) ||
                   tg_segment_covers_point(seg_b, seg_a.a) ||
                   tg_segment_covers_point(seg_b, seg_a.b);
        }
        return true;
    }
    double sx = d.x-c.x;
    double sy = d.y-c.y;
    double rxs = rx*sy - ry*sx;
    if (eq_zero(rxs)) {
        // Segments are parallel.
        return false; 
    }
    double cmpxs = cmpx*sy - cmpy*sx;
    double rxsr = 1 / rxs;
    double t = cmpxs * rxsr;
    double u = cmpxr * rxsr;
    if (!((t >= 0) && (t <= 1) && (u >= 0) && (u <= 1))) {
        return false;
    }
    return true;
}

/// Tests whether a segment fully contains a rectangle.
/// @see SegmentFuncs
bool tg_segment_covers_rect(struct tg_segment seg, struct tg_rect rect) {
    return tg_segment_covers_point(seg, rect.min) && 
           tg_segment_covers_point(seg, rect.max);
}

//////////////////
// ystripes
//////////////////

struct ystripe {
    int count;
    int *indexes;
};

struct buf {
    uint8_t *data;
    size_t len, cap;
};

static bool buf_ensure(struct buf *buf, size_t len) {
    if (buf->cap-buf->len >= len) return true;
    size_t cap = buf->cap;
    do {
        cap = grow_cap(cap, 16);
    } while (cap-buf->len < len);
    uint8_t *data = tg_realloc(buf->data, cap+1);
    if (!data) return false;
    buf->data = data;
    buf->cap = cap;
    buf->data[buf->cap] = '\0';
    return true;
}

// buf_append_byte append byte to buffer. 
// The buf->len should be greater than before, otherwise out of memory.
static bool buf_append_byte(struct buf *buf, uint8_t b) {
    if (!buf_ensure(buf, 1)) return false;
    buf->data[buf->len++] = b;
    return true;
}

static bool buf_append_bytes(struct buf *buf, uint8_t *bytes, size_t nbytes) {
    if (!buf_ensure(buf, nbytes)) return false;
    memcpy(buf->data+buf->len, bytes, nbytes);
    buf->len += nbytes;
    return true;
}

static bool buf_trunc(struct buf *buf) {
    if (buf->cap-buf->len > 8) {
        uint8_t *data = tg_realloc(buf->data, buf->len);
        if (!data) return false;
        buf->data = data;
        buf->cap = buf->len;
    }
    return true;
}

//////////////////
// index
//////////////////

#if !defined(TG_IXFLOAT64)

// 32-bit floats

struct ixpoint {
    float x;
    float y;
};
struct ixrect {
    struct ixpoint min;
    struct ixpoint max;
};

static inline void ixrect_expand(struct ixrect *rect, struct ixrect *other) {
    rect->min.x = fminf0(rect->min.x, other->min.x);
    rect->min.y = fminf0(rect->min.y, other->min.y);
    rect->max.x = fmaxf0(rect->max.x, other->max.x);
    rect->max.y = fmaxf0(rect->max.y, other->max.y);
}

// fdown/fup returns a value that is a little larger or smaller.
// Works similarly to nextafter, though it's faster and less accurate.
//    fdown(d) -> nextafter(d, -INFINITY);
//    fup(d)   -> nextafter(d, INFINITY); 
// fnext0 returns the next value after d. 
// Param dir can be 0 (towards -INFINITY) or 1 (towards INFINITY). 
static double fnext0(double d, int dir) {
    static double vals[2] = {
        1.0 - 1.0/8388608.0, /* towards zero */
        1.0 + 1.0/8388608.0  /* away from zero */
    };
    return d * vals[((d<0)+dir)&1];
}

static double fdown(double d) {
    return fnext0(d, 0);
}

static double fup(double d) {
    return fnext0(d, 1);
}

static void tg_rect_to_ixrect(struct tg_rect *rect, 
    struct ixrect *ixrect)
{
    ixrect->min.x = fdown(rect->min.x);
    ixrect->min.y = fdown(rect->min.y);
    ixrect->max.x = fup(rect->max.x);
    ixrect->max.y = fup(rect->max.y);
}

#else

// 64-bit floats

struct ixpoint {
    double x;
    double y;
};

struct ixrect {
    struct ixpoint min;
    struct ixpoint max;
};

static inline void ixrect_expand(struct ixrect *rect, struct ixrect *other) {
    rect->min.x = fmin0(rect->min.x, other->min.x);
    rect->min.y = fmin0(rect->min.y, other->min.y);
    rect->max.x = fmax0(rect->max.x, other->max.x);
    rect->max.y = fmax0(rect->max.y, other->max.y);
}
static inline void tg_rect_to_ixrect(struct tg_rect *rect, 
    struct ixrect *ixrect)
{
    ixrect->min.x = rect->min.x;
    ixrect->min.y = rect->min.y;
    ixrect->max.x = rect->max.x;
    ixrect->max.y = rect->max.y;
}

#endif

struct level {
    int nrects;
    struct ixrect *rects;
};

struct index {
    size_t memsz;             // memory size of index
    int spread;               // index spread (num items in each "node")
    int nlevels;              // number of levels
    struct level levels[];    // all levels starting with root
};

static inline void tg_point_to_ixpoint(struct tg_point *point, 
    struct ixpoint *ixpoint)
{
    ixpoint->x = point->x;
    ixpoint->y = point->y;
}


static inline void ixrect_to_tg_rect(struct ixrect *ixrect,
    struct tg_rect *rect)
{
    rect->min.x = ixrect->min.x;
    rect->min.y = ixrect->min.y;
    rect->max.x = ixrect->max.x;
    rect->max.y = ixrect->max.y;
}

static inline bool ixrect_intersects_ixrect(struct ixrect *a, 
    struct ixrect *b)
{
    if (a->min.y > b->max.y || a->max.y < b->min.y) return false;
    if (a->min.x > b->max.x || a->max.x < b->min.x) return false;
    return true;
}

static int index_spread(const struct index *index) {
    return index ? index->spread : 0;
}

static int index_num_levels(const struct index *index) {
    return index ? index->nlevels : 0;
}

static int index_level_num_rects(const struct index *index, int levelidx) {
    if (!index) return 0;
    if (levelidx < 0 || levelidx >= index->nlevels) return 0;
    return index->levels[levelidx].nrects;
}

static struct tg_rect index_level_rect(const struct index *index, 
    int levelidx, int rectidx)
{
    if (!index) return (struct tg_rect) { 0 };
    if (levelidx < 0 || levelidx >= index->nlevels) {
        return (struct tg_rect) { 0 };
    }
    const struct level *level = &index->levels[levelidx];
    if (rectidx < 0 || rectidx >= level->nrects) {
        return (struct tg_rect) { 0 };
    }
    struct tg_rect rect;
    ixrect_to_tg_rect(&level->rects[rectidx], &rect);
    return rect;
}

static int calc_num_keys(int spread, int level, int count) {
    return (int)ceil((double)count / pow((double)spread, (double)level));
}

static int calc_num_levels(int spread, int count) {
    int level = 1;
    for (; calc_num_keys(spread, level, count) > 1; level++);
    return level;
}

static size_t aligned_size(size_t size) {
    return size&7 ? size+8-(size&7) : size;
}

size_t tg_aligned_size(size_t size) {
    return aligned_size(size);
}

// calc_index_size returns the space needed to hold all the data for an index.
static size_t calc_index_size(int ixspread, int nsegs, int *nlevelsout) {
    int nlevels = calc_num_levels(ixspread, nsegs);
    int inlevels = nlevels-1;
    size_t size = sizeof(struct index);
    size += inlevels*sizeof(struct level);
    for (int i = 0; i < inlevels; i++) {
        int nkeys = calc_num_keys(ixspread, inlevels-i, nsegs);
        size += nkeys*sizeof(struct ixrect);
    }
    *nlevelsout = nlevels;
    size = aligned_size(size);
    return size;
}

struct ystripes {
    size_t memsz;
    int nstripes;
    struct ystripe stripes[];
};

static bool process_ystripes(struct tg_ring *ring) {
    double score = tg_ring_polsby_popper_score(ring);
    int nstripes = ring->nsegs * score;
    nstripes = fmax0(nstripes, 32);
    double height = ring->rect.max.y - ring->rect.min.y;

    // ycounts is used to log the number of segments in each stripe.
    int *ycounts = tg_malloc(nstripes*sizeof(int));
    if (!ycounts) {
        return false;
    }
    memset(ycounts, 0, nstripes*sizeof(int));

    // nmap is the the total number of mapped segments. This will be greater
    // than the total number of segments because a segment can exist in
    // multiple stripes.
    int nmap = 0;

    // Run through each segment and determine which stripes it belongs to and
    // increment the nmap counter.
    for (int i = 0; i < ring->nsegs; i++) {
        double ymin = fmin0(ring->points[i].y, ring->points[i+1].y);
        double ymax = fmax0(ring->points[i].y, ring->points[i+1].y);
        int min = (ymin - ring->rect.min.y) / height * (double)nstripes;
        int max = (ymax - ring->rect.min.y) / height * (double)nstripes;
        min = fmax0(min, 0);
        max = fmin0(max, nstripes-1);
        for (int j = min; j <= max; j++) {
            ycounts[j]++;
            nmap++;
        }
    }

    size_t tsize = sizeof(struct ystripes);
    tsize += nstripes*sizeof(struct ystripe);
    size_t mark = tsize;
    tsize += nmap*sizeof(int);
    struct ystripes *ystripes = tg_malloc(tsize);
    if (!ystripes) {
        tg_free(ycounts);
        return false;
    }
    ystripes->memsz = tsize;
    ystripes->nstripes = nstripes;

    size_t pos = mark;
    for (int i = 0; i < nstripes; i++) {
        ystripes->stripes[i].count = 0;
        ystripes->stripes[i].indexes = (void*)&((char*)ystripes)[pos]; 
        pos += ycounts[i]*sizeof(int);
    }
    tg_free(ycounts);

    for (int i = 0; i < ring->nsegs; i++) {
        double ymin = fmin0(ring->points[i].y, ring->points[i+1].y);
        double ymax = fmax0(ring->points[i].y, ring->points[i+1].y);
        int min = (ymin - ring->rect.min.y) / height * (double)nstripes;
        int max = (ymax - ring->rect.min.y) / height * (double)nstripes;
        min = fmax0(min, 0);
        max = fmin0(max, nstripes-1);
        for (int j = min; j <= max; j++) {
            struct ystripe *stripe = &ystripes->stripes[j];
            stripe->indexes[stripe->count++] = i;
        }
    }
    ring->ystripes = ystripes;
    return true;
}

static struct tg_segment ring_segment_at(const struct tg_ring *ring, int index);

//////////////////
// rect
//////////////////

/// Tests whether a rectangle fully contains a point using xy coordinates.
/// @see RectFuncs
bool tg_rect_covers_xy(struct tg_rect rect, double x, double y) {
    return !(x < rect.min.x || y < rect.min.y || 
             x > rect.max.x || y > rect.max.y);
}

/// Tests whether a rectangle fully contains a point.
/// @see RectFuncs
bool tg_rect_covers_point(struct tg_rect rect, struct tg_point point) {
    return tg_rect_covers_xy(rect, point.x, point.y);
}

/// Tests whether a rectangle and a point intersect.
/// @see RectFuncs
bool tg_rect_intersects_point(struct tg_rect rect, struct tg_point point) {
    return tg_rect_covers_point(rect, point);
}

/// Returns the center point of a rectangle
/// @see RectFuncs
struct tg_point tg_rect_center(struct tg_rect rect) {
    return (struct tg_point){ 
        .x = (rect.max.x + rect.min.x) / 2,
        .y = (rect.max.y + rect.min.y) / 2,
    };
}

static void rect_inflate(struct tg_rect *rect, struct tg_rect *other) {
    rect->min.x = fmin0(rect->min.x, other->min.x);
    rect->min.y = fmin0(rect->min.y, other->min.y);
    rect->max.x = fmax0(rect->max.x, other->max.x);
    rect->max.y = fmax0(rect->max.y, other->max.y);
}

static void rect_inflate_point(struct tg_rect *rect, struct tg_point *point) {
    rect->min.x = fmin0(rect->min.x, point->x);
    rect->min.y = fmin0(rect->min.y, point->y);
    rect->max.x = fmax0(rect->max.x, point->x);
    rect->max.y = fmax0(rect->max.y, point->y);
}

/// Expands a rectangle to include an additional rectangle.
/// @param rect Input rectangle
/// @param other Input rectangle
/// @return Expanded rectangle
/// @see RectFuncs
struct tg_rect tg_rect_expand(struct tg_rect rect, struct tg_rect other) {
    rect_inflate(&rect, &other);
    return rect;
}

/// Expands a rectangle to include an additional point.
/// @param rect Input rectangle
/// @param point Input Point
/// @return Expanded rectangle
/// @see RectFuncs
struct tg_rect tg_rect_expand_point(struct tg_rect rect, struct tg_point point)
{
    rect_inflate_point(&rect, &point);
    return rect;
}

// rect_to_ring fills the ring with rect data.
static void rect_to_ring(struct tg_rect rect, struct tg_ring *ring) {
    memset(ring, 0, sizeof(struct tg_ring));
    ring->head.base = BASE_RING;
    ring->head.type = TG_POLYGON;
    ring->rect = rect;
    ring->closed = true;
    ring->convex = true;
    ring->npoints = 5;
    ring->nsegs = 4;
    for (int i = 0; i < 5; i++) {
        ring->points[i] = tg_rect_point_at(rect, i);
    }
}

static void segment_to_ring(struct tg_segment seg, struct tg_ring *ring) {
    memset(ring, 0, sizeof(struct tg_ring));
    ring->rect = tg_segment_rect(seg);
    ring->closed = false;
    ring->convex = true;
    ring->npoints = 2;
    ring->nsegs = 1;
    ring->points[0] = seg.a;
    ring->points[1] = seg.b;
}

void tg_rect_search(const struct tg_rect rect, struct tg_rect target, 
    bool(*iter)(struct tg_segment seg, int index, void *udata),
    void *udata)
{
    int nsegs = tg_rect_num_segments(rect);
    for (int i = 0; i < nsegs; i++) {
        struct tg_segment seg = tg_rect_segment_at(rect, i);
        if (tg_rect_intersects_rect(tg_segment_rect(seg), target)) {
            if (!iter(seg, i, udata)) return;
        }
    }
}

/// Tests whether a rectangle fully contains line.
bool tg_rect_covers_line(struct tg_rect rect, const struct tg_line *line) {
    return !tg_line_empty(line) && 
        tg_rect_covers_rect(rect, tg_line_rect(line));
}

////////////////////
// ring
////////////////////

static void fill_in_upper_index_levels(struct index *index) {
    int ixspread = index->spread;
    for (int lvl = 1; lvl < index->nlevels; lvl++) {
        struct level *level = &index->levels[index->nlevels-1-lvl+0];
        struct level *plevel = &index->levels[index->nlevels-1-lvl+1];
        for (int i = 0; i < level->nrects; i++) {
            int s = i*ixspread;
            int e = s+ixspread;
            if (e > plevel->nrects) e = plevel->nrects;
            struct ixrect rect = plevel->rects[s];
            for (int j = s+1; j < e; j++) {
                ixrect_expand(&rect, &plevel->rects[j]);
            }
            level->rects[i] = rect;
        }
    }
}

// stack_ring returns a ring on the stack that has enough points to be capable
// of storing a rectangle. This is here to allow for tg_ring style operations
// on tg_rect without an allocation or vla.
#define stack_ring() \
(struct tg_ring*)(&(char[sizeof(struct tg_ring)+sizeof(struct tg_point)*5]){0})

static struct tg_rect process_points(const struct tg_point *points,
    int npoints, struct tg_point *ring_points, struct index *index,
    bool *convex, bool *clockwise, double *area)
{
    struct tg_rect rect = { 0 };
    if (npoints < 2) {
        if (points) {
            memcpy(ring_points, points, sizeof(struct tg_point) * npoints);
        }
        *convex = false;
        *clockwise = false;
        *area = 0;
        return rect;
    }
    
    struct tg_point a, b, c;
    bool concave = false;
    int dir = 0;
    double cwc = 0;
    int ixspread = index ? index->spread : 0;

    // Fill the initial rectangle with the first point.
    rect.min = points[0];
    rect.max = points[0];

    // 
    struct tg_rect spreadrect;
    spreadrect.min = points[0];
    spreadrect.max = points[0];

    // gather some point positions for concave and clockwise detection
    #define step_gather_abc_nowrap() { \
        a = points[i]; \
        b = points[i+1]; \
        c = points[i+2]; \
        ring_points[i] = a; \
    }
    
    #define step_gather_abc_wrap() { \
        a = points[i]; \
        if (i < npoints-2) { \
            b = points[i+1]; \
            c = points[i+2]; \
        } else if (i == npoints-1) { \
            b = points[0]; \
            c = points[1]; \
        } else { \
            b = points[i+1]; \
            c = points[0]; \
        } \
        ring_points[i] = a; \
    }
    
    #define step_inflate_rect_no_index() { \
        rect_inflate_point(&rect, &a); \
    }

    #define inflate_mbr_and_copy_next_index_rect() { \
        rect_inflate(&rect, &spreadrect); \
        /* convert the index rect to tg_rect for storing in tmesa level */ \
        tg_rect_to_ixrect(&spreadrect, \
            &index->levels[index->nlevels-1].rects[r]); \
        r++; \
    }

    // process the rectangle inflation
    #define step_inflate_rect_with_index() { \
        rect_inflate_point(&spreadrect, &a); \
        j++; \
        if (j == ixspread) { \
            rect_inflate_point(&spreadrect, &b); \
            inflate_mbr_and_copy_next_index_rect(); \
            spreadrect.min = b; \
            spreadrect.max = b; \
            j = 0; \
        } \
    }

    #define step_calc_clockwise() { \
        cwc += (b.x - a.x) * (b.y + a.y); \
    }

    #define step_calc_concave(label) { \
        double z_cross_product = (b.x-a.x)*(c.y-b.y) - (b.y-a.y)*(c.x-b.x); \
        if (dir == 0) { \
            dir = z_cross_product < 0 ? -1 : 1; \
        } else if (z_cross_product < 0) { \
            if (dir == 1) { \
                concave = true; \
                i++; \
                goto label; \
            } \
        } else if (z_cross_product > 0) { \
            if (dir == -1) { \
                concave = true; \
                i++; \
                goto label; \
            } \
        } \
    }

    // The following code is a large loop that scans over every point and 
    // performs various processing. 
    //
    // There are two major branch groups, Indexed and Non-indexed.
    // - Indexed is when there is a provided tablemesa type index structure,
    //   and each segment spread (usually eight segments at a time) are
    //   naturally inflated into a single rectangle that is stored contiguously 
    //   in a series of rectangles.
    // - Non-indexed is when there is no provided index. The entire MBR is
    //   inflated to included every point.
    //
    // For each group, there are the two outer loops. Non-wrapped and Wrapped.
    // - Non-wrapped takes the next points A, B, C from points[i+0],
    //   points[i+1], and points[i+2], without checking the points array
    //   boundary. This is safe because of the "npoints-3" condition.
    //   This is be quick because all internal operations are loop-unrolled.
    // - Wrapped is the remaining loop steps, up to three.
    //
    // Finally, there is a Convex and Concave section.
    // - Convex is the initial state. Each step will do some calcuations until
    //   it is determined if the ring is concave. Once it's known to be concave
    //   the code jumps to the Concave section.
    // - Concave section is just like the Convex section, but there is no
    //   calculation needed because it's known that the ring is concave.

    int i = 0;
    int j = 0;
    int r = 0;
    
    // LOOP START
    // Convex section
    if (!index) {
        // Non-index branch group
        // Non-wrapped outer loop
        for (;i < npoints-3; i++) {
            step_gather_abc_nowrap();
            step_inflate_rect_no_index();
            step_calc_clockwise();
            step_calc_concave(lconcave0);
        }
    lconcave0:
        // Wrapped outer loop
        for (;i < npoints; i++) {
            step_gather_abc_wrap();
            step_inflate_rect_no_index();
            step_calc_clockwise();
            step_calc_concave(lconcave);
        }
    } else {
        // Index branch group
        // Non-wrapped outer loop
        for (;i < npoints-3; i++) {
            step_gather_abc_nowrap();
            step_inflate_rect_with_index();
            step_calc_clockwise();
            step_calc_concave(lconcave1);
        }
    lconcave1:
        // Wrapped outer loop
        for (;i < npoints; i++) {
            step_gather_abc_wrap();
            step_inflate_rect_with_index();
            step_calc_clockwise();
            step_calc_concave(lconcave);
        }
    }
lconcave:
    // Concave section
    if (!index) {
        // Non-index branch group
        // Non-wrapped outer loop
        for (;i < npoints-3; i++) {
            step_gather_abc_nowrap();
            step_inflate_rect_no_index();
            step_calc_clockwise();
        }
        // Wrapped outer loop
        for (;i < npoints; i++) {
            step_gather_abc_wrap();
            step_inflate_rect_no_index();
            step_calc_clockwise();
        }
    } else {
        // Index branch group
        // Non-wrapped outer loop
        for (;i < npoints-3; i++) {
            step_gather_abc_nowrap();
            step_inflate_rect_with_index();
            step_calc_clockwise();
        }
        // Wrapped outer loop
        for (;i < npoints; i++) {
            step_gather_abc_wrap();
            step_inflate_rect_with_index();
            step_calc_clockwise();
        }
    }
    // LOOP END

    if (index) {
        // Fill in the final indexing rectangles
        if (r != index->levels[index->nlevels-1].nrects) {
            // There's one last index rectangle remaining.
            inflate_mbr_and_copy_next_index_rect();
        }
        fill_in_upper_index_levels(index);
    }

    *area = fabs(cwc / 2.0);
    *convex = !concave;
    *clockwise = cwc > 0;

    return rect;
}

static int num_segments(const struct tg_point *points, int npoints, bool closed) 
{
    if (closed) {
        if (npoints < 3) return 0;
        if (pteq(points[npoints-1], points[0])) return npoints - 1;
        return npoints;
    }
    if (npoints < 2) return 0;
    return npoints - 1;
}

static size_t calc_series_size(int npoints) {
    // Make room for an extra point to ensure a perfect ring close so that
    // that ring->points[ring->nsegs] never overflows.
    // This also allows for ring calculations to be safely run on lines.
    npoints++;
    size_t size = offsetof(struct tg_ring, points);
    size += sizeof(struct tg_point)*(npoints < 5 ? 5 : npoints);
    size = aligned_size(size);
    return size;
}

static void fill_index_struct(struct index *index, int nlevels, int nsegs, 
    int ixspread, size_t size)
{
    // Allocate the memory needed to store the entire index plus the size 
    // of the allocation in a pre header.
    index->nlevels = nlevels-1;
    index->memsz = size;
    index->spread = ixspread;

    // fill in the index structure fields
    size_t np = sizeof(struct index);
    np += index->nlevels*sizeof(struct level);
    for (int i = 0; i < index->nlevels; i++) {
        int nkeys = calc_num_keys(ixspread, index->nlevels-i, nsegs);
        index->levels[i].nrects = nkeys;
        index->levels[i].rects = (void*)&(((char*)index)[np]);
        np += nkeys*sizeof(struct ixrect);
    }
}

static struct tg_ring *series_new(const struct tg_point *points, int npoints, 
    bool closed, enum tg_index ix) 
{
    npoints = npoints <= 0 ? 0 : npoints;
    int nsegs = num_segments(points, npoints, closed);    
    size_t size = calc_series_size(npoints);

    int ixspread;
    ix = tg_index_extract_spread(ix, &ixspread);
    bool ystripes = false;
    int ixminpoints = ixspread*2;
    size_t ixsize = 0;
    int nlevels = 0;
    if (npoints >= ixminpoints) {
        bool indexed = false;
        if (ix == TG_DEFAULT) {
            ix = tg_env_get_default_index();
        }
        switch (ix) {
        case TG_NATURAL:
        case TG_YSTRIPES:
            indexed = true;
            break;
        default:
            indexed = false;
        }
        if ((ix&TG_NONE) == TG_NONE) {
            // no base index
        } else {
            // use TG_NATURAL
            indexed = true;
        }
        if (closed && ix == TG_YSTRIPES) {
            // Process ystripes for closed series only. e.g. rings, not lines.
            ystripes = true;
        }
        if (indexed) {
            ixsize = calc_index_size(ixspread, nsegs, &nlevels);
        }
    }

    // Allocate the entire ring structure and index in a single allocation.
    struct tg_ring *ring = tg_malloc(size+ixsize);
    if (!ring) return NULL;
    memset(ring, 0, sizeof(struct tg_ring));
    rc_init(&ring->head.rc);
    rc_retain(&ring->head.rc);
    ring->closed = closed;
    ring->npoints = npoints;
    ring->nsegs = nsegs;
    if (ixsize) {
        // size is always 64-bit aligned.
        // assert((size & 7) == 0);
        ring->index = (struct index *)(((char*)ring)+size);
        fill_index_struct(ring->index, nlevels, nsegs, ixspread, ixsize);
    }

    ring->rect = process_points(points, npoints, ring->points, ring->index,
        &ring->convex, &ring->clockwise, &ring->area);
    
    // Fill extra point to ensure perfect close.
    ring->points[npoints] = ring->points[0];

    if (closed) {
        ring->head.base = BASE_RING;
        ring->head.type = TG_POLYGON;
    } else {
        ring->head.base = BASE_LINE;
        ring->head.type = TG_LINESTRING;
    }
    if (ystripes) {
        // Process ystripes for closed series only. e.g. rings, not lines.
        if (!process_ystripes(ring)) {
            tg_ring_free(ring);
            return NULL;
        }
    }
    return ring;
}

static struct tg_ring *series_move(const struct tg_ring *ring, bool closed, 
    double delta_x, double delta_y)
{
    if (!ring) return NULL;
    struct tg_point *points = tg_malloc(ring->npoints*sizeof(struct tg_point));
    if (!points) return NULL;
    for (int i = 0; i < ring->npoints; i++) {
        points[i] = tg_point_move(ring->points[i], delta_x, delta_y);
    }
    enum tg_index ix = 0;
    if (ring->ystripes) {
        ix = TG_YSTRIPES;
    } else if (ring->index) {
        ix = TG_NATURAL;
    } else {
        ix = TG_NONE;
    }
    struct tg_ring *final = series_new(points, ring->npoints, closed, ix);
    tg_free(points);
    return final;
}


/// Creates a ring from a series of points.
/// @param points Array of points
/// @param npoints Number of points in array
/// @return A newly allocated ring
/// @return NULL if out of memory
/// @note A tg_ring can be safely upcasted to a tg_geom. `(struct tg_geom*)ring`
/// @note A tg_ring can be safely upcasted to a tg_poly. `(struct tg_poly*)ring`
/// @note All rings with 32 or more points are automatically indexed.
/// @see tg_ring_new_ix()
/// @see RingFuncs
struct tg_ring *tg_ring_new(const struct tg_point *points, int npoints) {
    return tg_ring_new_ix(points, npoints, TG_DEFAULT);
}

/// Creates a ring from a series of points using provided index option.
/// @param points Array of points
/// @param npoints Number of points in array
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @return A newly allocated ring
/// @return NULL if out of memory
/// @note A tg_ring can be safely upcasted to a tg_geom. `(struct tg_geom*)ring`
/// @note A tg_ring can be safely upcasted to a tg_poly. `(struct tg_poly*)ring`
/// @see tg_ring_new()
/// @see [tg_index](.#tg_index)
/// @see RingFuncs
struct tg_ring *tg_ring_new_ix(const struct tg_point *points, int npoints,
    enum tg_index ix)
{
    return series_new(points, npoints, true, ix);
}

/// Releases the memory associated with a ring.
/// @param ring Input ring
/// @see RingFuncs
void tg_ring_free(struct tg_ring *ring) {
    if (!ring || ring->head.noheap || !rc_release(&ring->head.rc)) return;
    if (ring->ystripes) tg_free(ring->ystripes);
    tg_free(ring);
}

static size_t ring_alloc_size(const struct tg_ring *ring) {
    if (ring->index) {
        // The index shares same allocation as the ring.
        return (size_t)ring->index + ring->index->memsz - (size_t)ring;
    } else {
        // There is no index. Calculate the size.
        return calc_series_size(ring->npoints);
    }
}

/// Clones a ring
/// @param ring Input ring, caller retains ownership.
/// @return A duplicate of the provided ring. 
/// @note The caller is responsible for freeing with tg_ring_free().
/// @note This method of cloning uses implicit sharing through an atomic 
/// reference counter.
/// @see RingFuncs
struct tg_ring *tg_ring_clone(const struct tg_ring *ring) {
    if (!ring || ring->head.noheap) {
        return tg_ring_copy(ring);
    }
    struct tg_ring *ring_mut = (struct tg_ring*)ring;
    rc_retain(&ring_mut->head.rc);
    return ring_mut;
}

/// Returns the allocation size of the ring. 
/// @param ring Input ring
/// @return Size of ring in bytes
/// @see RingFuncs
size_t tg_ring_memsize(const struct tg_ring *ring) {
    if (!ring) return 0;
    size_t size = ring_alloc_size(ring);
    if (ring->ystripes) {
        size += ring->ystripes->memsz;
    }
    return size;
}

/// Returns the number of points.
/// @param ring Input ring
/// @return Number of points
/// @see tg_ring_point_at()
/// @see RingFuncs
int tg_ring_num_points(const struct tg_ring *ring) {
    if (!ring) return 0;
    return ring->npoints;
}

/// Returns the minimum bounding rectangle of a rect.
/// @param ring Input ring
/// @returns Minimum bounding retangle
/// @see RingFuncs
struct tg_rect tg_ring_rect(const struct tg_ring *ring) {
    if (!ring) return (struct tg_rect){ 0 };
    return ring->rect;
}

/// Returns the point at index.
/// @param ring Input ring
/// @param index Index of point
/// @return The point at index
/// @note This function performs bounds checking. Use tg_ring_points() for
/// direct access to the points.
/// @see tg_ring_num_points()
/// @see RingFuncs
struct tg_point tg_ring_point_at(const struct tg_ring *ring, int index) {
    if (!ring || index < 0 || index >= ring->npoints) {
        return (struct tg_point){ 0 };
    }
    return ring->points[index];
}

/// Returns the number of segments.
/// @param ring Input ring
/// @return Number of segments
/// @see tg_ring_segment_at()
/// @see RingFuncs
int tg_ring_num_segments(const struct tg_ring *ring) {
    if (!ring) return 0;
    return ring->nsegs;
}

/// Returns the segment at index without bounds checking
static struct tg_segment ring_segment_at(const struct tg_ring *ring, int i) {
    // The process_points operation ensures that there always one point more
    // than the number of segments.
    return (struct tg_segment) { ring->points[i], ring->points[i+1] };
}

static struct tg_segment line_segment_at(const struct tg_line *line, int i) {
    return ring_segment_at((struct tg_ring*)line, i);
}

/// Returns the segment at index.
/// @param ring Input ring
/// @param index Index of segment
/// @return The segment at index
/// @see tg_ring_num_segments()
/// @see RingFuncs
struct tg_segment tg_ring_segment_at(const struct tg_ring *ring, int index) {
    if (!ring || index < 0 || index >= ring->nsegs) {
        return (struct tg_segment){ 0 };
    }
    return ring_segment_at(ring, index);
}

static bool segment_rect_intersects_rect(const struct tg_segment *seg, 
    struct tg_rect *rect) 
{
    struct tg_rect rect2;
    segment_fill_rect(seg, &rect2);
    return rect_intersects_rect(rect, &rect2);
}

static bool index_search(const struct tg_ring *ring, struct tg_rect *rect, 
    int lvl, int start,
    bool(*iter)(struct tg_segment seg, int index, void *udata),
    void *udata)
{
    const struct index *ix = ring->index;
    int ixspread = ix->spread;
    if (lvl == ix->nlevels) {
        int nsegs = ring->nsegs;
        int i = start;
        int e = i+ixspread;
        if (e > nsegs) e = nsegs;
        for (; i < e; i++) {
            int j = i;
            struct tg_segment *seg = (struct tg_segment *)(&ring->points[j]);
            if (segment_rect_intersects_rect(seg, rect)) {
                if (!iter(*seg, j, udata)) {
                    return false;
                }
            }
        }
    } else {
        struct ixrect ixrect;
        tg_rect_to_ixrect(rect, &ixrect);
        const struct level *level = &ix->levels[lvl];
        int i = start;
        int e = i+ixspread;
        if (e > level->nrects) e = level->nrects;
        for (; i < e; i++) {
            if (ixrect_intersects_ixrect(&level->rects[i], &ixrect)) {
                if (!index_search(ring, rect, lvl+1, i*ixspread, iter, 
                    udata)) 
                {
                    return false;
                }
            }
        }
    }
    return true;
}

// ring_search searches for ring segments that intersect the provided rect.
void tg_ring_search(const struct tg_ring *ring, struct tg_rect rect, 
    bool (*iter)(struct tg_segment seg, int index, void *udata),
    void *udata)
{
    if (!ring || !iter) {
        return;
    }
    if (ring->index) {
        index_search(ring, &rect, 0, 0, iter, udata);
    } else {
        for (int i = 0; i < ring->nsegs; i++) {
            struct tg_segment *seg = (struct tg_segment *)(&ring->points[i]);
            if (segment_rect_intersects_rect(seg, &rect)) {
                if (!iter(*seg, i, udata)) return;
            }
        }
    }
}

struct ring_ring_iter_ctx {
    void *udata;
    struct tg_segment seg;
    int index;
    bool swapped;
    bool stop;
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata);
};

static bool ring_ring_iter(struct tg_segment seg, int index, void *udata) {
    struct ring_ring_iter_ctx *ctx = udata;
    if (tg_segment_intersects_segment(seg, ctx->seg)) {
        bool ok = ctx->swapped ? 
            ctx->iter(ctx->seg, ctx->index, seg, index, ctx->udata):
            ctx->iter(seg, index, ctx->seg, ctx->index, ctx->udata);
        if (!ok) {
            ctx->stop = true;
            return false;
        }
    }
    return true;
}

static bool ring_ring_ix(const struct tg_ring *a, int alvl, int aidx, 
    int aspread, const struct tg_ring *b, int blvl, int bidx, int bspread,
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata)
{
    int aleaf = alvl == a->index->nlevels;
    int bleaf = blvl == b->index->nlevels;
    int anrects = aleaf ? a->nsegs : a->index->levels[alvl].nrects;
    int bnrects = bleaf ? b->nsegs : b->index->levels[blvl].nrects;
    int as = aidx;
    int ae = as + aspread;
    if (ae > anrects) ae = anrects;
    int bs = bidx;
    int be = bs + bspread;
    if (be > bnrects) be = bnrects;
    if (aleaf && bleaf) {
        // both are leaves
        for (int i = as; i < ae; i++) {
            struct tg_segment seg_a = ring_segment_at(a, i);
            for (int j = bs; j < be; j++) {
                struct tg_segment seg_b = ring_segment_at(b, j);
                if (tg_segment_intersects_segment(seg_a, seg_b)) {
                    if (!iter(seg_a, i, seg_b, j, udata)) {
                        return false;
                    }
                }
            }
        }
    } else if (aleaf) {
        // A is a leaf and B is a branch
        struct tg_rect arect, brect;
        for (int i = as; i < ae; i++) {
            struct tg_segment seg = ring_segment_at(a, i);
            segment_fill_rect(&seg, &arect);
            for (int j = bs; j < be; j++) {
                ixrect_to_tg_rect(&b->index->levels[blvl].rects[j], &brect);
                if (rect_intersects_rect(&arect, &brect)) {
                    if (!ring_ring_ix(a, alvl, i, 1,
                        b, blvl+1, j*bspread, bspread, iter, udata))
                    {
                        return false;
                    }
                }
            }
        }
    } else if (bleaf) {
        // B is a leaf and A is a branch
        struct tg_rect arect, brect;
        for (int i = as; i < ae; i++) {
            ixrect_to_tg_rect(&a->index->levels[alvl].rects[i], &arect);
            for (int j = bs; j < be; j++) {
                struct tg_segment seg = ring_segment_at(b, j);
                segment_fill_rect(&seg, &brect);
                if (rect_intersects_rect(&arect, &brect)) {
                    if (!ring_ring_ix(a, alvl+1, i*aspread, aspread,
                        b, blvl, j, 1, iter, udata))
                    {
                        return false;
                    }
                }
            }
        }
    } else {
        // both are branches
        for (int i = as; i < ae; i++) {
            for (int j = bs; j < be; j++) {
                struct ixrect *arect = &a->index->levels[alvl].rects[i];
                struct ixrect *brect = &b->index->levels[blvl].rects[j];
                if (ixrect_intersects_ixrect(arect, brect)) {
                    if (!ring_ring_ix(a, alvl+1, i*a->index->spread, aspread,
                        b, blvl+1, j*b->index->spread, bspread, iter, udata))
                    {
                        return false;
                    }
                } 
            }
        }
    }
    return true;
}

/// Iterates over all segments in ring A that intersect with segments in ring B.
/// @note This efficently uses the indexes of each geometry, if available.
/// @see RingFuncs
void tg_ring_ring_search(const struct tg_ring *a, const struct tg_ring *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata)
{
    if (tg_ring_empty(a) || tg_ring_empty(b) || !iter || 
        !tg_rect_intersects_rect(tg_ring_rect(a), tg_ring_rect(b)))
    {
        return;
    }
    if (a->index && b->index) {
        // both indexes are available
        ring_ring_ix(a, 0, 0, a->index->spread, 
            b, 0, 0, b->index->spread, iter, udata);
    } else if (a->index || b->index) {
        // only one index is available
        const struct tg_ring *c = b->index ? b : a;
        const struct tg_ring *d = b->index ? a : b;
        struct ring_ring_iter_ctx ctx = { 
            .iter = iter, 
            .swapped = c == b,
            .udata = udata,
        };
        for (int i = 0; i < d->nsegs; i++) {
            struct tg_segment seg = ring_segment_at(d, i);
            struct tg_rect rect;
            segment_fill_rect(&seg, &rect);
            ctx.seg = seg;
            ctx.index = i;
            tg_ring_search(c, rect, ring_ring_iter, &ctx);
            if (ctx.stop) {
                return;
            }
        }
    } else {
        // no indexes are available
        for (int i = 0; i < a->nsegs; i++) {
            struct tg_segment seg_a = ring_segment_at(a, i);
            for (int j = 0; j < b->nsegs; j++) {
                struct tg_segment seg_b = ring_segment_at(b, j);
                if (tg_segment_intersects_segment(seg_a, seg_b)) {
                    if (!iter(seg_a, i, seg_b, j, udata)) {
                        return;
                    }
                }
            }
        }
    }
}

/// Iterates over all segments in line A that intersect with segments in line B.
/// @note This efficently uses the indexes of each geometry, if available.
/// @see LineFuncs
void tg_line_line_search(const struct tg_line *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata)
{
    tg_ring_ring_search((struct tg_ring*)a, (struct tg_ring*)b, iter, udata);
}

/// Iterates over all segments in ring A that intersect with segments in line B.
/// @note This efficently uses the indexes of each geometry, if available.
/// @see RingFuncs
void tg_ring_line_search(const struct tg_ring *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata)
{
    tg_ring_ring_search(a, (struct tg_ring*)b, iter, udata);
}


__attr_noinline
static void pip_eval_seg_slow(const struct tg_ring *ring, int i, 
    struct tg_point point, bool allow_on_edge, bool *in, int *idx)
{
    struct tg_segment seg = { ring->points[i], ring->points[i+1] };    
    switch (raycast(seg, point)) {
    case TG_OUT:
        break;
    case TG_IN:
        if (*idx == -1) {
            *in = !*in;
        }
        break;
    case TG_ON:
        *in = allow_on_edge;
        *idx = i;
        break;
    }
}

static inline void pip_eval_seg(const struct tg_ring *ring, int i, 
    struct tg_point point, bool allow_on_edge, bool *in, int *idx)
{
    // Performs fail-fast raycast boundary tests first.
    double ymin = fmin0(ring->points[i].y, ring->points[i+1].y);
    double ymax = fmax0(ring->points[i].y, ring->points[i+1].y);
    if (point.y < ymin || point.y > ymax) {
        return;
    }
    double xmin = fmin0(ring->points[i].x, ring->points[i+1].x);
    double xmax = fmax0(ring->points[i].x, ring->points[i+1].x);
    if (point.x < xmin) {
        if (point.y != ymin && point.y != ymax) {
            if (*idx != -1) return;
            *in = !*in;
            return;
        }    
    } else if (point.x > xmax) {
        if (ymin != ymax && xmin != xmax) {
            return;
        }
    }
    pip_eval_seg_slow(ring, i, point, allow_on_edge, in, idx);
}

struct ring_result {
    bool hit; // contains/intersects
    int idx;  // edge index
};

static struct ring_result ystripes_pip(const struct tg_ring *ring, 
    struct tg_point point, bool allow_on_edge)
{
    bool in = false;
    int idx = -1;
    struct ystripes *ystripes = ring->ystripes;
    double height = ring->rect.max.y-ring->rect.min.y;
    int y = (point.y - ring->rect.min.y) / height * (double)ystripes->nstripes;
    y = fclamp0(y, 0, ystripes->nstripes-1);
    struct ystripe *ystripe = &ystripes->stripes[y];
    for (int i = 0; i < ystripe->count; i++) {
        int j = ystripe->indexes[i];
        pip_eval_seg(ring, j, point, allow_on_edge, &in, &idx); 
    }
    return (struct ring_result){ .hit = in, .idx = idx};
}

static struct ring_result standard_pip(const struct tg_ring *ring, 
    struct tg_point point, bool allow_on_edge)
{
    bool in = false;
    int idx = -1;
    int i = 0;
    while (i < ring->nsegs) {
        for16(i, ring->nsegs, {
            double ymin = fmin0(ring->points[i].y, ring->points[i+1].y);
            double ymax = fmax0(ring->points[i].y, ring->points[i+1].y);
            if (!(point.y < ymin || point.y > ymax)) {
                goto do_pip;
            }
        });
        break;
    do_pip:
        pip_eval_seg_slow(ring, i, point, allow_on_edge, &in, &idx);
        i++;
    }
    return (struct ring_result){ .hit = in, .idx = idx};
}

static bool branch_maybe_in(struct ixpoint point, struct ixrect rect) {
    if (point.y < rect.min.y || point.y > rect.max.y) {
        return false;
    }
    if (point.x > rect.max.x) {
        if (rect.min.y != rect.max.y && rect.min.x != rect.max.x) {
            return false;
        }
    }
    return true;
}

static void index_pip_counter(const struct tg_ring *ring, struct tg_point point,
    bool allow_on_edge, int lvl, int start, bool *in, int *idx)
{
    struct index *ix = ring->index;
    int ixspread = ix->spread;
    if (lvl == ix->nlevels) {
        // leaf segments
        int i = start;
        int e = i+ixspread;
        if (e > ring->nsegs) e = ring->nsegs;
        for16(i, e, {
            pip_eval_seg(ring, i, point, allow_on_edge, in, idx);
        });
    } else {
        struct ixpoint ixpoint;
        tg_point_to_ixpoint(&point, &ixpoint);
        // branch rectangles
        const struct level *level = &ix->levels[lvl];
        int i = start;
        int e = i+ixspread;
        if (e > level->nrects) e = level->nrects;
        for16(i, e, {
            if (branch_maybe_in(ixpoint, level->rects[i])) {
                index_pip_counter(ring, point, allow_on_edge, lvl+1, 
                    i*ixspread, in, idx);
            }
        });
    }
}

static struct ring_result index_pip(const struct tg_ring *ring, 
    struct tg_point point, bool allow_on_edge)
{
    bool in = false;
    int idx = -1;
    index_pip_counter(ring, point, allow_on_edge, 0, 0, &in, &idx);
    return (struct ring_result){ .hit = in, .idx = idx};
}

struct ring_result tg_ring_contains_point(const struct tg_ring *ring, 
    struct tg_point point, bool allow_on_edge)
{
    if (!tg_rect_covers_point(ring->rect, point)) {
        return (struct ring_result){ .hit = false, .idx = -1 };
    }
    if (ring->ystripes) {
        return ystripes_pip(ring, point, allow_on_edge);
    }
    if (ring->index) {
        return index_pip(ring, point, allow_on_edge);
    }
    return standard_pip(ring, point, allow_on_edge);
}

/// Returns true if ring is convex. 
/// @param ring Input ring
/// @return True if ring is convex.
/// @return False if ring is concave.
/// @see RingFuncs
bool tg_ring_convex(const struct tg_ring *ring) {
    if (!ring) return false;
    return ring->convex;
}

/// Returns true if winding order is clockwise. 
/// @param ring Input ring
/// @return True if clockwise
/// @return False if counter-clockwise
/// @see RingFuncs
bool tg_ring_clockwise(const struct tg_ring *ring) {
    if (!ring) return false;
    return ring->clockwise;
}

struct contsegiterctx {
    struct tg_segment seg;
    bool intersects;
};

static bool contsegiter1(struct tg_segment seg2, int index, void *udata) {
    (void)index;
    struct contsegiterctx *ctx = udata;
    if (tg_segment_intersects_segment(ctx->seg, seg2)) {
        if (tg_raycast(seg2, ctx->seg.a) != TG_ON && 
            tg_raycast(seg2, ctx->seg.b) != TG_ON)
        {
            ctx->intersects = true;
            return false;
        }
    }
    return true;
}

static bool contsegiter4(struct tg_segment seg2, int index, void *udata) {
    (void)index;
    struct contsegiterctx *ctx = udata;
    if (tg_segment_intersects_segment(ctx->seg, seg2)) {
        if (tg_raycast(seg2, ctx->seg.a) != TG_ON) {
            ctx->intersects = true;
            return false;
        }
    }
    return true;
}

static bool contsegiter2(struct tg_segment seg2, int index, void *udata) {
    (void)index;
    struct contsegiterctx *ctx = udata;
    if (tg_segment_intersects_segment(ctx->seg, seg2)) {
        if (tg_raycast(seg2, ctx->seg.b) != TG_ON) {
            ctx->intersects = true;
            return false;
        }
    }
    return true;
}
static bool contsegiter5(struct tg_segment seg2, int index, void *udata) {
    (void)index;
    struct contsegiterctx *ctx = udata;
    if (tg_segment_intersects_segment(ctx->seg, seg2)) {
        if (tg_raycast(ctx->seg, seg2.a) != TG_ON && 
            tg_raycast(ctx->seg, seg2.b) != TG_ON)
        {
            ctx->intersects = true;
            return false;
        }
    }
    return true;
}

static bool contsegiter3(struct tg_segment seg2, int index, void *udata) {
    (void)index;
    struct contsegiterctx *ctx = udata;
    if (tg_segment_intersects_segment(ctx->seg, seg2)) {
            ctx->intersects = true;
            return false;
    }
    return true;
}

bool tg_ring_contains_segment(const struct tg_ring *ring, 
    struct tg_segment seg, bool allow_on_edge)
{
    if (!tg_rect_covers_rect(ring->rect, tg_segment_rect(seg))) {
        return false;
    }
    // Test that segment points are contained in the ring.
    struct ring_result res_a = tg_ring_contains_point(ring, seg.a, 
        allow_on_edge);
    if (!res_a.hit) {
        // seg A is not inside ring
        return false;
    }
    if (pteq(seg.b, seg.a)) {
        return true;
    }
    struct ring_result res_b = tg_ring_contains_point(ring, seg.b,
        allow_on_edge);
    if (!res_b.hit) {
        // seg B is not inside ring
        return false;
    }
    if (ring->convex) {
        // ring is convex so the segment must be contained
        return true;
    }

    // The ring is concave so it's possible that the segment crosses over the
    // edge of the ring.
    if (allow_on_edge) {
        // do some logic around seg points that are on the edge of the ring.
        if (res_a.idx != -1) {
           // seg A is on a ring segment
            if (res_b.idx != -1) {
                // seg B is on a ring segment
                if (res_b.idx == res_a.idx) {
                    // seg A and B share the same ring segment, so it must be
                    // on the inside.
                    return true;
                }

                // seg A and seg B are on different segments.
                // determine if the space that the seg passes over is inside or
                // outside of the ring. To do so we create a ring from the two
                // ring segments and check if that ring winding order matches
                // the winding order of the ring.
                // -- create a ring

                struct tg_segment r_seg_a = ring_segment_at(ring, res_a.idx);
                struct tg_segment r_seg_b = ring_segment_at(ring, res_b.idx);
                if (pteq(r_seg_a.a, seg.a) || pteq(r_seg_a.b, seg.a) ||
                    pteq(r_seg_b.a, seg.a) || pteq(r_seg_b.b, seg.a) ||
                    pteq(r_seg_a.a, seg.b) || pteq(r_seg_a.b, seg.b) ||
                    pteq(r_seg_b.a, seg.b) || pteq(r_seg_b.b, seg.b))
                {
                    return true;
                }

                // fix the order of the
                if (res_b.idx < res_a.idx) {
                    struct tg_segment tmp = r_seg_a;
                    r_seg_a = r_seg_b;
                    r_seg_b = tmp;
                }

                struct tg_point pts[] = {
                    r_seg_a.a, r_seg_a.b, r_seg_b.a, r_seg_b.b, r_seg_a.a
                };
                // -- calc winding order
                double cwc = 0.0;
                for (int i = 0; i < 4; i++) {
                    struct tg_point a = pts[i];
                    struct tg_point b = pts[i+1];
                    cwc += (b.x - a.x) * (b.y + a.y);
                }
                bool clockwise = cwc > 0;
                if (clockwise != ring->clockwise) {
                    // -- on the outside
                    return false;
                }
                // the passover space is on the inside of the ring.
                // check if seg intersects any ring segments where A and B are
                // not on.
                struct contsegiterctx ctx = {
                    .intersects = false,
                    .seg = seg,
                };
                tg_ring_search(ring, tg_segment_rect(seg), contsegiter1, &ctx);
                return !ctx.intersects;
            }
            // case (4)
            // seg A is on a ring segment, but seg B is not.
            // check if seg intersects any ring segments where A is not on.
            struct contsegiterctx ctx = {
                .intersects = false,
                .seg = seg,
            };
            tg_ring_search(ring, tg_segment_rect(seg), contsegiter4, &ctx);
            return !ctx.intersects;
        } else if (res_b.idx != -1) {
            // case (2)
            // seg B is on a ring segment, but seg A is not.
            // check if seg intersects any ring segments where B is not on.
            struct contsegiterctx ctx = {
                .intersects = false,
                .seg = seg,
            };
            tg_ring_search(ring, tg_segment_rect(seg), contsegiter2, &ctx);
            return !ctx.intersects;
        }
        // case (5) (15)
        struct contsegiterctx ctx = {
            .intersects = false,
            .seg = seg,
        };
        tg_ring_search(ring, tg_segment_rect(seg), contsegiter5, &ctx);
        return !ctx.intersects;
    }
    // allow_on_edge is false. (not allow on edge)
    struct contsegiterctx ctx = {
        .intersects = false,
        .seg = seg,
    };
    tg_ring_search(ring, tg_segment_rect(seg), contsegiter3, &ctx);
    return !ctx.intersects;
}

struct intersegiterctx {
    struct tg_segment seg;
    int count;
    bool allow_on_edge;
    bool seg_a_on;
    bool seg_b_on;
    // bool yes;
};

static bool intersegiter(struct tg_segment seg, int index, void *udata) {
    (void)index;
    struct intersegiterctx *ctx = udata;

    if (!tg_segment_intersects_segment(ctx->seg, seg)) {
        return true;
    }
    if (ctx->allow_on_edge) {
        ctx->count++;
        return ctx->count < 2;
    }

    struct tg_point a = ctx->seg.a;
    struct tg_point b = ctx->seg.b;
    struct tg_point c = seg.a;
    struct tg_point d = seg.b;

    // bool acol = collinear(c.x, c.y, d.x, d.y, a.x, a.y);
    // bool bcol = collinear(c.x, c.y, d.x, d.y, b.x, b.y);
    bool ccol = collinear(a.x, a.y, b.x, b.y, c.x, c.y);
    bool dcol = collinear(a.x, a.y, b.x, b.y, d.x, d.y);

    if (ccol && dcol) {
        // lines are parallel.
        ctx->count = 0;
    } else if (!ccol || !dcol) {
        if (!ctx->seg_a_on) {
            if (pteq(a, c) || pteq(a, d)) {
                ctx->seg_a_on = true;
                return true;
            }
        }
        if (!ctx->seg_b_on) {
            if (pteq(b, c) || pteq(b, d)) {
                ctx->seg_b_on = true;
                return true;
            }
        }
        ctx->count++;
    }
    return ctx->count < 2;
}

bool tg_ring_intersects_segment(const struct tg_ring *ring, 
    struct tg_segment seg, bool allow_on_edge)
{
    if (!tg_rect_intersects_rect(tg_segment_rect(seg), ring->rect)) {
        return false;
    }
    // Quick check that either point is inside of the ring
    if (tg_ring_contains_point(ring, seg.a, allow_on_edge).hit ||
        tg_ring_contains_point(ring, seg.b, allow_on_edge).hit)
    {
        return true;
    }

    // Neither point A or B is inside of the ring. It's possible that both
    // are on the outside and are passing over segments. If the segment passes
    // over at least two ring segments then it's intersecting.
    struct intersegiterctx ctx = { 
        .seg = seg,
        .allow_on_edge = allow_on_edge,
    };
    tg_ring_search(ring, tg_segment_rect(seg), intersegiter, &ctx);
    return ctx.count >= 2;
}

// tg_ring_empty returns true when the ring is NULL, or does not form a closed
// ring, ie. it cannot be used as a valid spatial geometry.
bool tg_ring_empty(const struct tg_ring *ring) {
    if (!ring) return true;
    return (ring->closed && ring->npoints < 3) || ring->npoints < 2;
}

bool tg_ring_contains_ring(const struct tg_ring *a, const struct tg_ring *b,
    bool allow_on_edge)
{
    if (tg_ring_empty(a) || tg_ring_empty(b)) {
        return false;
    }
    // test if the inner rect does not contain the outer rect
    if (!tg_rect_covers_rect(a->rect, b->rect)) {
        // not fully contained so it's not possible for the outer ring to
        // contain the inner ring
        return false;
    }
    if (a->convex) {
        // outer ring is convex so test that all inner points are inside of
        // the outer ring
        for (int i = 0; i < b->npoints; i++) {
            if (!tg_ring_contains_point(a, b->points[i], allow_on_edge).hit) {
                // point is on the outside the outer ring
                return false;
            }
        }
    } else {
        // outer ring is concave so let's make sure that all inner segments are
        // fully contained inside of the outer ring.
        for (int i = 0; i < b->nsegs; i++) {
            struct tg_segment seg = ring_segment_at(b, i);
            if (!tg_ring_contains_segment(a, seg, allow_on_edge)) {
                return false;
            }
        }
    }
    return true;
}

struct tg_rect tg_rect_move(struct tg_rect rect, 
    double delta_x, double delta_y)
{
    rect.min = tg_point_move(rect.min, delta_x, delta_y);
    rect.max = tg_point_move(rect.max, delta_x, delta_y);
    return rect;
}

static double rect_area(struct tg_rect rect) {
    return (rect.max.x - rect.min.x) * (rect.max.y - rect.min.y);
}

bool tg_ring_intersects_ring(const struct tg_ring *ring,
    const struct tg_ring *other, bool allow_on_edge)
{
    if (tg_ring_empty(ring) || tg_ring_empty(other)) {
        return false;
    }
    // check outer and innter rects intersection first
    if (!tg_rect_intersects_rect(ring->rect, other->rect)) {
        return false;
    }
    double a1 = rect_area(tg_ring_rect(ring));
    double a2 = rect_area(tg_ring_rect(other));
    if (a2 > a1) {
        // swap the rings so that the inner ring is smaller than the outer ring
        const struct tg_ring *tmp = ring;
        ring = other;
        other = tmp;
    }
    for (int i = 0; i < other->nsegs; i++) {
        if (tg_ring_intersects_segment(ring, ring_segment_at(other, i), 
            allow_on_edge)) 
        {
            return true;
        }
    }
    return false;
}

bool tg_ring_contains_line(const struct tg_ring *a, const struct tg_line *b, 
    bool allow_on_edge, bool respect_boundaries)
{
    // Almost the same logic as tg_ring_contains_line except for boundaries
    // detection for the input line. 
    if (tg_ring_empty(a) || tg_line_empty(b)) {
        return false;
    }
    // test if the inner rect does not contain the outer rect
    if (!tg_rect_covers_rect(a->rect, tg_line_rect(b))) {
        // not fully contained so it's not possible for the outer ring to
        // contain the inner ring
        return false;
    }

    // if (a->convex && !respect_boundaries) {
    //     // outer ring is convex so test that all inner points are inside of
    //     // the outer ring
    //     int npoints = tg_line_num_points(b);
    //     const struct tg_point *points = tg_line_points(b);
    //     for (int i = 0; i < npoints; i++) {
    //         if (!tg_ring_contains_point(a, points[i], allow_on_edge).hit) {
    //             // point is on the outside the outer ring
    //             return false;
    //         }
    //     }
    // } else 
    
    if (!allow_on_edge && respect_boundaries) {
        // outer ring is concave so let's make sure that all inner segments are
        // fully contained inside of the outer ring.
        int nsegs = tg_line_num_segments(b);
        for (int i = 0; i < nsegs; i++) {
            struct tg_segment seg = line_segment_at(b, i);
            if (!tg_ring_contains_segment(a, seg, true)) {
                return false;
            }
            if (!tg_ring_intersects_segment(a, seg, false)) {
                return false;
            }
        }
    } else {
        // outer ring is concave so let's make sure that all inner segments are
        // fully contained inside of the outer ring.
        int nsegs = tg_line_num_segments(b);
        for (int i = 0; i < nsegs; i++) {
            struct tg_segment seg = line_segment_at(b, i);
            if (!tg_ring_contains_segment(a, seg, allow_on_edge)) {
                return false;
            }
        }
    }
    return true;

}

/// Tests whether a ring intersects a line.
/// @see RingFuncs
bool tg_ring_intersects_line(const struct tg_ring *ring, 
    const struct tg_line *line, bool allow_on_edge)
{
    if (tg_ring_empty(ring) || tg_line_empty(line)) {
        return false;
    }
    // check outer and innter rects intersection first
    if (!tg_rect_intersects_rect(tg_ring_rect(ring), tg_line_rect(line))) {
        return false;
    }
    // check if any points are inside ring
    // TODO: use line index if available.
    int nsegs = tg_line_num_segments(line);
    for (int i = 0; i < nsegs; i++) {
        if (tg_ring_intersects_segment(ring, tg_line_segment_at(line, i), 
            allow_on_edge))
        {
            return true;
        }
    }
    return false;
}

/// Tests whether a rectangle intersects a line.
/// @see RectFuncs
bool tg_rect_intersects_line(struct tg_rect rect, const struct tg_line *line) {
    struct tg_ring *ring = stack_ring();
    rect_to_ring(rect, ring);
    return tg_ring_intersects_line(ring, line, true);
}

/// Tests whether a rectangle intersects a polygon.
/// @see RectFuncs
bool tg_rect_intersects_poly(struct tg_rect rect, const struct tg_poly *poly) {
    return tg_poly_intersects_rect(poly, rect);
}

/// Tests whether a rectangle fully contains a polygon.
/// @see RectFuncs
bool tg_rect_covers_poly(struct tg_rect rect, const struct tg_poly *poly) {
    return !tg_poly_empty(poly) && 
        tg_rect_covers_rect(rect, tg_poly_rect(poly));
}

struct tg_ring *tg_ring_move(const struct tg_ring *ring,
    double delta_x, double delta_y)
{
    return series_move(ring, true, delta_x, delta_y);
}

/// Returns the underlying point array of a ring.
/// @param ring Input ring
/// @return Array or points
/// @see tg_ring_num_points()
/// @see RingFuncs
const struct tg_point *tg_ring_points(const struct tg_ring *ring) {
    if (!ring) return NULL;
    return ring->points;
}

////////////////////
// line
////////////////////

/// Creates a line from a series of points.
/// @param points Array of points
/// @param npoints Number of points in array
/// @return A newly allocated line
/// @return NULL if out of memory
/// @note A tg_line can be safely upcasted to a tg_geom. `(struct tg_geom*)line`
/// @note All lines with 32 or more points are automatically indexed.
/// @see LineFuncs
struct tg_line *tg_line_new(const struct tg_point *points, int npoints) {
    return tg_line_new_ix(points, npoints, TG_DEFAULT);
}

/// Creates a line from a series of points using provided index option.
/// @param points Array of points
/// @param npoints Number of points in array
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @return A newly allocated line
/// @return NULL if out of memory
/// @note A tg_line can be safely upcasted to a tg_geom. `(struct tg_geom*)poly`
/// @see [tg_index](.#tg_index)
/// @see LineFuncs
struct tg_line *tg_line_new_ix(const struct tg_point *points, int npoints,
    enum tg_index ix)
{
    return (struct tg_line*)series_new(points, npoints, false, ix);
}

/// Releases the memory associated with a line.
/// @param line Input line
/// @see LineFuncs
void tg_line_free(struct tg_line *line) {
    struct tg_ring *ring = (struct tg_ring *)line;
    tg_ring_free(ring);
}

/// Returns the minimum bounding rectangle of a line.
/// @see LineFuncs
struct tg_rect tg_line_rect(const struct tg_line *line) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_rect(ring);
}

/// Returns the number of points.
/// @param line Input line
/// @return Number of points
/// @see tg_line_point_at()
/// @see LineFuncs
int tg_line_num_points(const struct tg_line *line) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_num_points(ring);
}

/// Returns the point at index.
/// @param line Input line
/// @param index Index of point
/// @return The point at index
/// @note This function performs bounds checking. Use tg_line_points() for
/// direct access to the points.
/// @see tg_line_num_points()
/// @see LineFuncs
struct tg_point tg_line_point_at(const struct tg_line *line, int index) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_point_at(ring, index);
}

/// Returns the number of segments.
/// @param line Input line
/// @return Number of segments
/// @see tg_line_segment_at()
/// @see LineFuncs
int tg_line_num_segments(const struct tg_line *line) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_num_segments(ring);
}

/// Returns the segment at index.
/// @param line Input line
/// @param index Index of segment
/// @return The segment at index
/// @see tg_line_num_segments()
/// @see LineFuncs
struct tg_segment tg_line_segment_at(const struct tg_line *line, int index) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_segment_at(ring, index);
}

// tg_line_empty returns true when the line is NULL or has less than two
// points, ie. it cannot be used as a valid spatial geometry.
bool tg_line_empty(const struct tg_line *line) {
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_empty(ring);
}

void tg_line_search(const struct tg_line *line, struct tg_rect rect, 
    bool(*iter)(struct tg_segment seg, int index, void *udata),
    void *udata)
{
    struct tg_ring *ring = (struct tg_ring *)line;
    tg_ring_search(ring, rect, iter, udata);
}

struct line_covers_point_iter_ctx {
    bool covers;
    struct tg_point point;
};

static bool line_covers_point_iter(struct tg_segment seg, 
    int index, void *udata)
{
    (void)index;
    struct line_covers_point_iter_ctx *ctx = udata;
    if (tg_segment_covers_point(seg, ctx->point)) {
        ctx->covers = true;
        return false;
    }
    return true;
}

/// Tests whether a line fully contains a point
/// @see LineFuncs
bool tg_line_covers_point(const struct tg_line *line, struct tg_point point) {
    struct line_covers_point_iter_ctx ctx = { 
        .point = point,
    };
    tg_line_search(line, (struct tg_rect){ point, point }, 
        line_covers_point_iter, &ctx);
    return ctx.covers;
}

bool tg_line_contains_point(const struct tg_line *line, struct tg_point point) {
    if (!tg_line_covers_point(line, point)) {
        return false;
    }
    int nsegs = tg_line_num_segments(line);
    if (pteq(point, tg_line_segment_at(line, 0).a) || 
        pteq(point, tg_line_segment_at(line, nsegs-1).b))
    {
        return false;
    }
    return true;
}

/// Tests whether a line intersects a point
/// @see LineFuncs
bool tg_line_intersects_point(const struct tg_line *line, 
    struct tg_point point)
{
    return tg_line_covers_point(line, point);
}

bool tg_line_touches_point(const struct tg_line *line, struct tg_point point) {
    return tg_point_touches_line(point, line);
}

/// Tests whether a line fully contains a rectangle
/// @see LineFuncs
bool tg_line_covers_rect(const struct tg_line *line, struct tg_rect rect) {
    // Convert rect into a poly
    struct tg_ring *exterior = stack_ring();
    rect_to_ring(rect, exterior);
    struct tg_poly poly = { .exterior = exterior };
    return tg_line_covers_poly(line, &poly);
}

/// Tests whether a line intersects a rectangle
/// @see LineFuncs
bool tg_line_intersects_rect(const struct tg_line *line, struct tg_rect rect) {
    return tg_rect_intersects_line(rect, line);
}

/// Tests whether a line contains another line
/// @see LineFuncs
bool tg_line_covers_line(const struct tg_line *a, const struct tg_line *b) {
    if (tg_line_empty(a) || tg_line_empty(b)) return false;

    if (!tg_rect_covers_rect(tg_line_rect(a), tg_line_rect(b))) {
        return false;
    }
    // locate the first "other" segment that contains the first "line" segment.
    int ansegs = tg_line_num_segments(a);
    int j = -1;
    for (int k = 0; k < ansegs; k++) {
        if (tg_segment_covers_segment(tg_line_segment_at(a, k),
            tg_line_segment_at(b, 0)))
        {
            j = k;
            break;
        }
    }
    if (j == -1) {
        return false;
    }
    int bnsegs = tg_line_num_segments(b);
    for (int i = 1; i < bnsegs && j < ansegs; i++) {
        struct tg_segment aseg = tg_line_segment_at(a, j);
        struct tg_segment bseg = tg_line_segment_at(b, i);
        if (tg_segment_covers_segment(aseg, bseg)) {
            continue;
        }
        if (pteq(bseg.a, aseg.a)) {
            // reverse it
            if (j == 0) {
                return false;
            }
            j--;
            i--;
        } else if (pteq(bseg.a, aseg.b)) {
            // forward it
            j++;
            i--;
        }
    }
    return true;
}

bool tg_line_contains_line(const struct tg_line *line, 
    const struct tg_line *other)
{
    return tg_line_covers_line(line, other);
}

enum segment_intersects_kind {
    SI_INTERSECTS, 
    SI_TOUCHES,
};

struct segment_intersects_iter_ctx {
    bool yes;
    int ansegs;
    int bnsegs;
    enum segment_intersects_kind kind;
};

static bool segment_touches0(struct tg_segment seg,
    struct tg_point a, struct tg_point b)
{
    if (!tg_segment_covers_point(seg, a)) {
        return false;
    }
    if (!collinear(seg.a.x, seg.a.y, seg.b.x, seg.b.y, b.x, b.y)) {
        return true;
    }
    if (pteq(seg.a, a)) {
        return !tg_segment_covers_point((struct tg_segment){ a, b }, seg.b);
    }
    if (pteq(seg.b, a)) {
        return !tg_segment_covers_point((struct tg_segment){ a, b }, seg.a);
    }
    return false;
}

static bool any_touching(struct tg_segment a, int aidx, int ansegs,
    struct tg_segment b, int bidx, int bnsegs)
{
    return (aidx == 0 && segment_touches0(b, a.a, a.b)) ||
           (aidx == ansegs-1 && segment_touches0(b, a.b, a.a)) ||
           (bidx == 0 && segment_touches0(a, b.a, b.b)) ||
           (bidx == bnsegs-1 && segment_touches0(a, b.b, b.a));
}

static bool segment_intersects_iter(struct tg_segment a, int aidx, 
    struct tg_segment b, int bidx, void *udata)
{
    struct segment_intersects_iter_ctx *ctx = udata;
    switch (ctx->kind) {
    case SI_INTERSECTS:
        ctx->yes = true;
        break;
    case SI_TOUCHES:
        if (any_touching(a, aidx, ctx->ansegs, b, bidx, ctx->bnsegs)) {
            ctx->yes = true;
            return true;
        }
        ctx->yes = false;
        break;
    }
    return false;
}

static bool line_intersects_line(const struct tg_line *a, 
    const struct tg_line *b, enum segment_intersects_kind kind)
{
    struct segment_intersects_iter_ctx ctx = { 
        .kind = kind,
        .ansegs = tg_line_num_segments(a),
        .bnsegs = tg_line_num_segments(b),
     };
    tg_line_line_search(a, b, segment_intersects_iter, &ctx);
    return ctx.yes;
}

/// Tests whether a line intersects another line
/// @see LineFuncs
bool tg_line_intersects_line(const struct tg_line *a, const struct tg_line *b) {
    return line_intersects_line(a, b, SI_INTERSECTS);
}

bool tg_line_touches_line(const struct tg_line *a,const struct tg_line *b) {    
    return line_intersects_line(a, b, SI_TOUCHES);
}

/// Tests whether a line fully contains a polygon
/// @see LineFuncs
bool tg_line_covers_poly(const struct tg_line *line,
    const struct tg_poly *poly)
{
    if (tg_line_empty(line) || tg_poly_empty(poly)) return false;
    struct tg_rect rect = tg_poly_rect(poly);
    if (rect.min.x != rect.max.x && rect.min.y != rect.max.y) return false;
    
    // polygon can fit in a straight (vertial or horizontal) line
    struct tg_segment seg = { rect.min, rect.max };
    struct tg_ring *other = stack_ring();
    segment_to_ring(seg, other);
    rect_to_ring(rect, other);
    return tg_line_covers_line(line, (struct tg_line*)(other));
}

bool tg_line_contains_poly(const struct tg_line *line,
    const struct tg_poly *poly)
{
    // not possible
    (void)line; (void)poly;
    return false;
}

/// Tests whether a line intersects a polygon
/// @see LineFuncs
bool tg_line_intersects_poly(const struct tg_line *line,
    const struct tg_poly *poly)
{
    return tg_poly_intersects_line(poly, line);
}

bool tg_line_touches_poly(const struct tg_line *a, const struct tg_poly *b) {
    return tg_poly_touches_line(b, a);
}

struct tg_line *tg_line_move(const struct tg_line *line,
    double delta_x, double delta_y)
{
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return (struct tg_line*)series_move(ring, false, delta_x, delta_y);
}

/// Returns true if winding order is clockwise. 
/// @param line Input line
/// @return True if clockwise
/// @return False if counter-clockwise
/// @see LineFuncs
bool tg_line_clockwise(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_clockwise(ring);
}

/// Clones a line
/// @param line Input line, caller retains ownership.
/// @return A duplicate of the provided line. 
/// @note The caller is responsible for freeing with tg_line_free().
/// @note This method of cloning uses implicit sharing through an atomic 
/// reference counter.
/// @see LineFuncs
struct tg_line *tg_line_clone(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return (struct tg_line*)tg_ring_clone(ring);
}

/// Returns the underlying point array of a line.
/// @param line Input line
/// @return Array or points
/// @see tg_line_num_points()
/// @see LineFuncs
const struct tg_point *tg_line_points(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_points(ring);
}

/// Returns the allocation size of the line. 
/// @param line Input line
/// @return Size of line in bytes
/// @see LineFuncs
size_t tg_line_memsize(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_memsize(ring);
}

/// Returns the indexing spread for a line.
/// 
/// The "spread" is the number of segments or rectangles that are grouped 
/// together to produce a unioned rectangle that is stored at a higher level.
/// 
/// For a tree based structure, this would be the number of items per node.
///
/// @param line Input line
/// @return The spread, default is 16
/// @return Zero if line has no indexing
/// @see tg_line_index_num_levels()
/// @see tg_line_index_level_num_rects()
/// @see tg_line_index_level_rect()
/// @see LineFuncs
int tg_line_index_spread(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_index_spread(ring);
}

/// Returns the number of levels.
/// @param line Input line
/// @return The number of levels
/// @return Zero if line has no indexing
/// @see tg_line_index_spread()
/// @see tg_line_index_level_num_rects()
/// @see tg_line_index_level_rect()
/// @see LineFuncs
int tg_line_index_num_levels(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_index_num_levels(ring);
}

/// Returns the number of rectangles at level.
/// @param line Input line
/// @param levelidx The index of level
/// @return The number of index levels
/// @return Zero if line has no indexing or levelidx is out of bounds.
/// @see tg_line_index_spread()
/// @see tg_line_index_num_levels()
/// @see tg_line_index_level_rect()
/// @see LineFuncs
int tg_line_index_level_num_rects(const struct tg_line *line, int levelidx) {
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_index_level_num_rects(ring, levelidx);
}

/// Returns a specific level rectangle.
/// @param line Input line
/// @param levelidx The index of level
/// @param rectidx The index of rectangle
/// @return The rectangle
/// @return Empty rectangle if line has no indexing, or levelidx or rectidx
/// is out of bounds.
/// @see tg_line_index_spread()
/// @see tg_line_index_num_levels()
/// @see tg_line_index_level_num_rects()
/// @see LineFuncs
struct tg_rect tg_line_index_level_rect(const struct tg_line *line, 
    int levelidx, int rectidx)
{
    const struct tg_ring *ring = (const struct tg_ring *)line;
    return tg_ring_index_level_rect(ring, levelidx, rectidx);
}

////////////////////
// poly
////////////////////

/// Creates a polygon.
/// @param exterior Exterior ring
/// @param holes Array of interior rings that are holes
/// @param nholes Number of holes in array
/// @return A newly allocated polygon
/// @return NULL if out of memory
/// @return NULL if exterior or any holes are NULL
/// @note A tg_poly can be safely upcasted to a tg_geom. `(struct tg_geom*)poly`
/// @see PolyFuncs
struct tg_poly *tg_poly_new(const struct tg_ring *exterior, 
    const struct tg_ring *const holes[], int nholes) 
{
    if (!exterior) {
        return NULL;
    }
    if (nholes == 0) {
        // When the user creates a new poly without holes then we can avoid
        // the extra allocations by upcasting the base tg_ring to a tg_poly.
        return (struct tg_poly *)tg_ring_clone(exterior);
    }
    struct tg_poly *poly = tg_malloc(sizeof(struct tg_poly));
    if (!poly) {
        goto fail;
    }
    memset(poly, 0, sizeof(struct tg_poly));
    rc_init(&poly->head.rc);
    rc_retain(&poly->head.rc);
    poly->head.base = BASE_POLY;
    poly->head.type = TG_POLYGON;
    poly->exterior = tg_ring_clone(exterior);
    if (nholes > 0) {
        poly->holes = tg_malloc(sizeof(struct tg_ring*)*nholes);
        if (!poly->holes) {
            goto fail;
        }
        poly->nholes = nholes;
        memset(poly->holes, 0, sizeof(struct tg_ring*)*poly->nholes);
        for (int i = 0; i < poly->nholes; i++) {
            poly->holes[i] = tg_ring_clone(holes[i]);
        }
    }
    return poly;
fail:
    tg_poly_free(poly);
    return NULL;
}

/// Releases the memory associated with a polygon.
/// @param poly Input polygon
/// @see PolyFuncs
void tg_poly_free(struct tg_poly *poly) {
    if (!poly) return;
    if (poly->head.base == BASE_RING) {
        tg_ring_free((struct tg_ring*)poly);
        return;
    }
    if (poly->head.noheap || !rc_release(&poly->head.rc)) return;
    if (poly->exterior) tg_ring_free(poly->exterior);
    if (poly->holes) {
        for (int i = 0; i < poly->nholes; i++) {
            if (poly->holes[i]) tg_ring_free(poly->holes[i]);
        }
        tg_free(poly->holes);
    }
    tg_free(poly);
}

/// Clones a polygon.
/// @param poly Input polygon, caller retains ownership.
/// @return A duplicate of the provided polygon. 
/// @note The caller is responsible for freeing with tg_poly_free().
/// @note This method of cloning uses implicit sharing through an atomic 
/// reference counter.
/// @see PolyFuncs
struct tg_poly *tg_poly_clone(const struct tg_poly *poly) {
    if (!poly || poly->head.noheap) {
        return tg_poly_copy(poly);
    }
    struct tg_poly *poly_mut = (struct tg_poly*)poly;
    rc_retain(&poly_mut->head.rc);
    return poly_mut;
}

/// Returns the exterior ring.
/// @param poly Input polygon
/// @return Exterior ring
/// @note The polygon maintains ownership of the exterior ring.
/// @see PolyFuncs
const struct tg_ring *tg_poly_exterior(const struct tg_poly *poly) {
    if (!poly) return NULL;
    if (poly->head.base == BASE_RING) {
        return (struct tg_ring*)poly;
    }
    return poly->exterior;
}

/// Returns the number of interior holes.
/// @param poly Input polygon
/// @return Number of holes
/// @see tg_poly_hole_at()
/// @see PolyFuncs
int tg_poly_num_holes(const struct tg_poly *poly) {
    if (!poly || poly->head.base == BASE_RING) return 0;
    return poly->nholes;
}

/// Returns an interior hole.
/// @param poly Input polygon
/// @param index Index of hole
/// @return Ring hole
/// @see tg_poly_num_holes()
/// @see PolyFuncs
const struct tg_ring *tg_poly_hole_at(const struct tg_poly *poly, int index) {
    if (!poly || poly->head.base == BASE_RING) return NULL;
    if (index < 0 || index >= poly->nholes) return NULL;
    return poly->holes[index];
}

/// Returns true if winding order is clockwise.
/// @param poly Input polygon
/// @return True if clockwise
/// @return False if counter-clockwise
/// @see PolyFuncs
bool tg_poly_clockwise(const struct tg_poly *poly) {
    return tg_ring_clockwise(tg_poly_exterior(poly));
}

/// Returns true if polygon is empty.
/// @see PolyFuncs
bool tg_poly_empty(const struct tg_poly *poly) {
    return tg_ring_empty(tg_poly_exterior(poly));
}

/// Returns the minimum bounding rectangle of a polygon.
/// @see PolyFuncs
struct tg_rect tg_poly_rect(const struct tg_poly *poly) {
    return tg_ring_rect(tg_poly_exterior(poly));
}

static bool poly_contains_point(const struct tg_poly *poly, 
    struct tg_point point, bool allow_on_edge)
{
    if (poly && poly->head.base == BASE_RING) {
        // downcast fast path
        return tg_ring_contains_point((struct tg_ring*)poly, point, 
            allow_on_edge).hit;
    }
    // standard path
    if (tg_poly_empty(poly)) {
        return false;
    }
    if (!tg_ring_contains_point(poly->exterior, point, allow_on_edge).hit) {
        return false;
    }
    bool covers = true;
    for (int i = 0; i < poly->nholes; i++) {
        if (tg_ring_contains_point(poly->holes[i], point, !allow_on_edge).hit) {
            covers = false;
            break;
        }
    }
    return covers;
}

/// Tests whether a polygon fully contains a point.
/// @see PolyFuncs
bool tg_poly_covers_point(const struct tg_poly *poly, struct tg_point point) {
    return poly_contains_point(poly, point, true);
}

bool tg_poly_contains_point(const struct tg_poly *poly, struct tg_point point) {
    return poly_contains_point(poly, point, false);
}

/// Tests whether a polygon fully contains a point using xy coordinates.
/// @see PolyFuncs
bool tg_poly_covers_xy(const struct tg_poly *poly, double x, double y) {
    return tg_poly_covers_point(poly, (struct tg_point){ .x = x, .y = y });
}

/// Tests whether a polygon intersects a point.
/// @see PolyFuncs
bool tg_poly_intersects_point(const struct tg_poly *poly, struct tg_point point)
{
    return tg_poly_covers_point(poly, point);
}

bool tg_poly_touches_point(const struct tg_poly *poly, struct tg_point point) {
    return tg_point_touches_poly(point, poly);
}


/// Tests whether a polygon fully contains a rectangle.
/// @see PolyFuncs
bool tg_poly_covers_rect(const struct tg_poly *poly, struct tg_rect rect) {
    // Convert rect into a poly
    struct tg_ring *other = stack_ring();
    rect_to_ring(rect, other);
    return tg_poly_covers_poly(poly, (struct tg_poly*)other);
}

/// Tests whether a polygon intersects a rectangle.
/// @see PolyFuncs
bool tg_poly_intersects_rect(const struct tg_poly *poly, struct tg_rect rect) {
    // convert rect into a poly
    struct tg_ring *other = stack_ring();
    rect_to_ring(rect, other);
    return tg_poly_intersects_poly(poly, (struct tg_poly*)other);
}

/// Tests whether a polygon covers (fully contains) a line.
/// @see PolyFuncs
bool tg_poly_covers_line(const struct tg_poly *a, const struct tg_line *b) {
    if (tg_poly_empty(a) || tg_line_empty(b)) {
        return false;
    }
    if (!tg_ring_contains_line(tg_poly_exterior(a), b, true, false)) {
        return false;
    }
    int nholes = tg_poly_num_holes(a);
    for (int i = 0; i < nholes; i++) {
        if (tg_ring_intersects_line(tg_poly_hole_at(a, i), b, false)) {
            return false;
        }
    }
    return true;
}

bool tg_poly_contains_line(const struct tg_poly *a, const struct tg_line *b) {
    if (tg_poly_empty(a) || tg_line_empty(b)) {
        return false;
    }
    if (!tg_ring_contains_line(tg_poly_exterior(a), b, false, true)) {
        return false;
    }
    int nholes = tg_poly_num_holes(a);
    for (int i = 0; i < nholes; i++) {
        if (tg_ring_intersects_line(tg_poly_hole_at(a, i), b, false)) {
            return false;
        }
    }
    return true;
}

/// Tests whether a polygon intersects a line.
/// @see PolyFuncs
bool tg_poly_intersects_line(const struct tg_poly *poly,
    const struct tg_line *line)
{
    if (poly && poly->head.base == BASE_RING) {
        // downcast fast path
        return tg_ring_intersects_line((struct tg_ring*)poly, line, true);
    }
    // standard path
    if (tg_poly_empty(poly) || tg_line_empty(line)) {
        return false;
    }
    if (!tg_ring_intersects_line(poly->exterior, line, true)) {
        return false;
    }
    for (int i = 0; i < poly->nholes; i++) {
        if (tg_ring_contains_line(poly->holes[i], line, false, false)) {
            return false;
        }
    }
    return true;
}

/// Tests whether a polygon fully contains another polygon.
/// @see PolyFuncs
bool tg_poly_covers_poly(const struct tg_poly *a, const struct tg_poly *b) {
    if (a && a->head.base == BASE_RING && 
        b && b->head.base == BASE_RING)
    {
        // downcast fast path
        return tg_ring_contains_ring((struct tg_ring*)a, (struct tg_ring*)b,
            true);
    }
    // standard path
    if (tg_poly_empty(a) || tg_poly_empty(b)) {
        return false;
    }

    const struct tg_ring *a_exterior = tg_poly_exterior(a);
    const struct tg_ring *b_exterior = tg_poly_exterior(b);
    int a_nholes = tg_poly_num_holes(a);
    int b_nholes = tg_poly_num_holes(b);
    struct tg_ring **a_holes = NULL;
    if (a->head.base == BASE_POLY) {
        a_holes = a->holes;
    }
    struct tg_ring **b_holes = NULL;
    if (b->head.base == BASE_POLY) {
        b_holes = b->holes;
    }
    
    // 1) other exterior must be fully contained inside of the poly exterior.
    if (!tg_ring_contains_ring(a_exterior, b_exterior, true)) {
        return false;
    }
    // 2) ring cannot intersect poly holes
    bool covers = true;
    for (int i = 0; i < a_nholes; i++) {
        if (tg_ring_intersects_ring(a_holes[i], b_exterior, false)) {
            covers = false;
            // 3) unless the poly hole is contain inside of a other hole
            for (int j = 0; j < b_nholes; j++) {
                if (tg_ring_contains_ring(b_holes[j], a_holes[i], 
                    true))
                {
                    covers = true;
                    break;
                }
            }
            if (!covers) {
                break;
            }
        }
    }
    return covers;
}

bool tg_poly_contains_poly(const struct tg_poly *a, const struct tg_poly *b) {
    return tg_poly_covers_poly(a, b);
}

/// Tests whether a polygon intesects a polygon.
/// @see PolyFuncs
bool tg_poly_intersects_poly(const struct tg_poly *poly, 
    const struct tg_poly *other)
{
    if (poly && poly->head.base == BASE_RING && 
        other && other->head.base == BASE_RING)
    {
        // downcast fast path
        return tg_ring_intersects_ring((struct tg_ring*)poly,
            (struct tg_ring*)other, true);
    }
    // standard path
    if (tg_poly_empty(poly) || tg_poly_empty(other)) return false;

    const struct tg_ring *poly_exterior = tg_poly_exterior(poly);
    const struct tg_ring *other_exterior = tg_poly_exterior(other);
    int poly_nholes = tg_poly_num_holes(poly);
    int other_nholes = tg_poly_num_holes(other);
    struct tg_ring **poly_holes = NULL;
    if (poly->head.base == BASE_POLY) poly_holes = poly->holes;
    struct tg_ring **other_holes = NULL;
    if (other->head.base == BASE_POLY) other_holes = other->holes;

    if (!tg_ring_intersects_ring(other_exterior, poly_exterior, true)) {
        return false;
    }
    for (int i = 0; i < poly_nholes; i++) {
        if (tg_ring_contains_ring(poly_holes[i], other_exterior, false)) {
            return false;
        }
    }
    for (int i = 0; i < other_nholes; i++) {
        if (tg_ring_contains_ring(other_holes[i], poly_exterior, false)) {
            return false;
        }
    }
    return true;
}

bool tg_poly_touches_line(const struct tg_poly *a, const struct tg_line *b) {
    if (!tg_rect_intersects_rect(tg_poly_rect(a), tg_line_rect(b))) {
        return false;
    }

    // Check if the line is inside any of the polygon holes
    int npoints = tg_line_num_points(b);
    int nholes = tg_poly_num_holes(a);
    for (int i = 0; i < nholes; i++) {
        const struct tg_ring *hole = tg_poly_hole_at(a, i);
        if (tg_ring_contains_line(hole, b, true, false)) {
            // Yes, now check if any of the points touch the hole boundary.
            for (int j = 0; j < npoints; j++) {
                struct tg_point point = tg_line_point_at(b, j);
                if (tg_line_covers_point((struct tg_line*)hole, point)) {
                    return true;
                }
            }
            return false;
        }
    }

    // Check if at least one line point touches the polygon exterior.
    const struct tg_ring *ring = tg_poly_exterior(a);
    bool touches = false;
    for (int i = 0; i < npoints; i++) {
        struct tg_point point = tg_line_point_at(b, i);
        // Cast the exterior ring to a polygon to avoid holes.
        if (tg_poly_touches_point((struct tg_poly*)ring, point)) {
            touches = true;
            break;
        }
    }
    if (!touches) {
        return false;
    }
    int nsegs = tg_line_num_segments(b);
    for (int i = 0; i < nsegs; i++) {
        struct tg_segment seg = tg_line_segment_at(b, i);
        if (tg_ring_intersects_segment(ring, seg, false)) {
            return false;
        }
    }
    return true;
}

bool tg_poly_touches_poly(const struct tg_poly *a, const struct tg_poly *b) {
    if (!tg_rect_intersects_rect(tg_poly_rect(a), tg_poly_rect(b))) {
        return false;
    }
    
    const struct tg_ring *aext = tg_poly_exterior(a);
    const struct tg_ring *bext = tg_poly_exterior(b);

    // Check if one polygon is fully inside a hole of the other and touching
    // the hole boundary.
    for (int ii = 0; ii < 2; ii++) {
        const struct tg_poly *poly = ii == 0 ? a : b; 
        const struct tg_ring *ring = ii == 0 ? bext : aext; 
        int nholes = tg_poly_num_holes(poly);
        for (int i = 0; i < nholes; i++) {
            const struct tg_ring *hole = tg_poly_hole_at(poly, i);
            if (tg_ring_contains_ring(hole, ring, true)) {
                // Yes, now check if any exterior points are on the other 
                // hole boundary.
                int npoints = tg_ring_num_points(ring);
                for (int j = 0; j < npoints; j++) {
                    struct tg_point point = tg_ring_point_at(ring, j);
                    if (tg_line_covers_point((struct tg_line*)hole, point)) {
                        // Touching
                        return true;
                    }
                }
                // Not touching and full enclosed in a hole.
                return false;
            }
        }
    }
    
    // Now we can work with the exterior rings only.
    // Check if one polygon is touching the other
    int ansegs = tg_ring_num_segments(aext);
    int bnsegs = tg_ring_num_segments(bext);
    int atouches = 0;
    int btouches = 0;
    for (int ii = 0; ii < 2; ii++) {
        const struct tg_ring *a = ii == 0 ? aext : bext; 
        const struct tg_ring *b = ii == 0 ? bext : aext; 
        int nsegs = ii == 0 ? bnsegs : ansegs;
        int touches = 0;
        for (int i = 0; i < nsegs; i++) {
            struct tg_segment seg = tg_ring_segment_at(b, i);
            bool isects0 = tg_ring_intersects_segment(a, seg, true);
            bool isects1 = tg_ring_intersects_segment(a, seg, false);
            if (isects0 && !isects1) {
                touches++;
            } else if (isects0 || isects1) {
                return false;
            }
        }
        if (ii == 0) {
            btouches = touches;
        } else {
            atouches = touches;
        }
    }
    if (atouches > 0 || btouches > 0) {
        return !(atouches == ansegs && btouches == bnsegs);
    }
    return false;
}

struct tg_poly *tg_poly_move(const struct tg_poly *poly, double delta_x, 
    double delta_y)
{
    if (!poly) return NULL;
    if (poly->head.base == BASE_RING) {
        return (struct tg_poly*)tg_ring_move((struct tg_ring*)poly, delta_x, 
            delta_y);
    }

    struct tg_poly *final = NULL;
    struct tg_ring *exterior = NULL;
    struct tg_ring **holes = NULL;

    if (poly->exterior) {
        exterior = tg_ring_move(poly->exterior, delta_x, delta_y);
        if (!exterior) goto done;
    }
    if (poly->nholes > 0) {
        holes = tg_malloc(sizeof(struct tg_ring*)*poly->nholes);
        if (!holes) goto done;
        memset(holes, 0, sizeof(struct tg_ring*)*poly->nholes);
        for (int i = 0; i < poly->nholes; i++) {
            holes[i] = tg_ring_move(poly->holes[i], delta_x, delta_y);
            if (!holes[i]) goto done;
        }
    }
    final = tg_poly_new(exterior, (const struct tg_ring**)holes, poly->nholes);
done:
    if (exterior) tg_ring_free(exterior);
    if (holes) {
        for (int i = 0; i < poly->nholes; i++) {
            if (holes[i]) tg_ring_free(holes[i]);
        }
        tg_free(holes);
    }
    return final;
}

/// Returns the allocation size of the polygon. 
/// @param poly Input polygon
/// @return Size of polygon in bytes
/// @see PolyFuncs
size_t tg_poly_memsize(const struct tg_poly *poly) {
    if (!poly) return 0;
    if (poly->head.base == BASE_RING) {
        return tg_ring_memsize((struct tg_ring*)poly);
    }
    size_t size = sizeof(struct tg_poly);
    if (poly->exterior) {
        size += tg_ring_memsize(poly->exterior);
    }
    size += poly->nholes*sizeof(struct tg_ring);
    for (int i = 0; i < poly->nholes; i++) {
        size += tg_ring_memsize(poly->holes[i]);
    }
    return size;
}

////////////////////
// geom
////////////////////

static struct tg_geom *geom_new(enum tg_geom_type type) {
    struct tg_geom *geom = tg_malloc(sizeof(struct tg_geom));
    if (!geom) return NULL;
    memset(geom, 0, sizeof(struct tg_geom));
    rc_init(&geom->head.rc);
    rc_retain(&geom->head.rc);
    geom->head.base = BASE_GEOM;
    geom->head.type = type;
    return geom;
}

static struct tg_geom *geom_new_empty(enum tg_geom_type type) {
    struct tg_geom *geom = geom_new(type);
    if (!geom) return NULL;
    geom->head.flags = IS_EMPTY;
    return geom;
}

/// Creates a Point geometry.
/// @param point Input point
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_point(struct tg_point point) {
    struct boxed_point *geom = tg_malloc(sizeof(struct boxed_point));
    if (!geom) return NULL;
    memset(geom, 0, sizeof(struct boxed_point));
    rc_init(&geom->head.rc);
    rc_retain(&geom->head.rc);
    geom->head.base = BASE_POINT;
    geom->head.type = TG_POINT;
    geom->point = point;
    return (struct tg_geom*)geom;
}

static void boxed_point_free(struct boxed_point *point) {
    if (point->head.noheap || !rc_release(&point->head.rc)) return;
    tg_free(point);
}

/// Creates a Point geometry that includes a Z coordinate.
/// @param point Input point
/// @param z The Z coordinate
/// @return A newly allocated geometry, or NULL if system is out of 
/// memory. The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_point_z(struct tg_point point, double z) {
    struct tg_geom *geom = geom_new(TG_POINT);
    if (!geom) return NULL;
    geom->head.flags = HAS_Z;
    geom->point = point;
    geom->z = z;
    return geom;
}

/// Creates a Point geometry that includes an M coordinate.
/// @param point Input point
/// @param m The M coordinate
/// @return A newly allocated geometry, or NULL if system is out of 
/// memory. The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_point_m(struct tg_point point, double m) {
    struct tg_geom *geom = geom_new(TG_POINT);
    if (!geom) return NULL;
    geom->head.flags = HAS_M;
    geom->point = point;
    geom->m = m;
    return geom;
}

/// Creates a Point geometry that includes a Z and M coordinates.
/// @param point Input point
/// @param z The Z coordinate
/// @param m The M coordinate
/// @return A newly allocated geometry, or NULL if system is out of 
/// memory. The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_point_zm(struct tg_point point, double z, double m)
{
    struct tg_geom *geom = geom_new(TG_POINT);
    if (!geom) return NULL;
    geom->head.flags = HAS_Z | HAS_M;
    geom->point = point;
    geom->z = z;
    geom->m = m;
    return geom;
}

/// Creates an empty Point geometry.
/// @return A newly allocated geometry, or NULL if system is out of 
/// memory. The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_point_empty(void) {
    return geom_new_empty(TG_POINT);
}

/// Creates a LineString geometry.
/// @param line Input line, caller retains ownership.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_linestring(const struct tg_line *line) {
    return (struct tg_geom*)tg_line_clone(line);
}

/// Creates an empty LineString geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_linestring_empty(void) {
    return geom_new_empty(TG_LINESTRING);
}

/// Creates a Polygon geometry.
/// @param poly Input polygon, caller retains ownership.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_polygon(const struct tg_poly *poly) {
    return (struct tg_geom*)tg_poly_clone(poly);
}

/// Creates an empty Polygon geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_polygon_empty(void) {
    return geom_new_empty(TG_POLYGON);
}

static struct tg_geom *geom_new_multi(enum tg_geom_type type, int ngeoms) {
    ngeoms = ngeoms < 0 ? 0 : ngeoms;
    struct tg_geom *geom = geom_new(type);
    if (!geom) return NULL;
    geom->multi = tg_malloc(sizeof(struct multi));
    if (!geom->multi) {
        tg_free(geom);
        return NULL;
    }
    memset(geom->multi, 0, sizeof(struct multi));
    geom->multi->geoms = tg_malloc(ngeoms*sizeof(struct tg_geom*));
    if (!geom->multi->geoms) {
        tg_free(geom->multi);
        tg_free(geom);
        return NULL;
    }
    memset(geom->multi->geoms, 0, ngeoms*sizeof(struct tg_geom*));
    geom->multi->ngeoms = ngeoms;

    const int spread = 32;
    if (ngeoms >= spread*2) {
        int nlevels;
        size_t ixsize = calc_index_size(spread, ngeoms, &nlevels);
        geom->multi->index = tg_malloc(ixsize);
        geom->multi->ixgeoms = tg_malloc(ngeoms*sizeof(int));
        if (!geom->multi->index || !geom->multi->ixgeoms) {
            if (geom->multi->index) tg_free(geom->multi->index);
            if (geom->multi->ixgeoms) tg_free(geom->multi->ixgeoms);
            tg_free(geom->multi->geoms);
            tg_free(geom->multi);
            tg_free(geom);
            return NULL;
        }
        fill_index_struct(geom->multi->index, nlevels, ngeoms, spread, ixsize);
    }

    return geom;
}

// Fast 2D hilbert curve
// https://github.com/rawrunprotected/hilbert_curves
// Public Domain
static uint32_t hilbert_xy_to_index(uint32_t x, uint32_t y) {
    uint32_t A, B, C, D;
    uint32_t a, b, c, d;
    // Round (1) Initial prefix scan round, prime with x and y
    a = x ^ y;
    b = 0xFFFF ^ a;
    c = 0xFFFF ^ (x | y);
    d = x & (y ^ 0xFFFF);
    A = a | (b >> 1);
    B = (a >> 1) ^ a;
    C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
    D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;
    // Round (2) 
    a = A; b = B; c = C; d = D;
    A = ((a & (a >> 2)) ^ (b & (b >> 2)));
    B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));
    C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
    D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));
    // Round (3) 
    a = A; b = B; c = C; d = D;
    A = ((a & (a >> 4)) ^ (b & (b >> 4)));
    B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));
    C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
    D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));
    // Round (4) Final round and projection
    a = A; b = B; c = C; d = D;
    C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
    D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));
    // Undo transformation prefix scan
    a = C ^ (C >> 1);
    b = D ^ (D >> 1);
    // Recover index bits
    uint32_t i0 = x ^ y;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));
    // Interleave (i0)
    i0 = (i0 | (i0 << 8)) & 0x00FF00FF;
    i0 = (i0 | (i0 << 4)) & 0x0F0F0F0F;
    i0 = (i0 | (i0 << 2)) & 0x33333333;
    i0 = (i0 | (i0 << 1)) & 0x55555555;
    // Interleave (i1)
    i1 = (i1 | (i1 << 8)) & 0x00FF00FF;
    i1 = (i1 | (i1 << 4)) & 0x0F0F0F0F;
    i1 = (i1 | (i1 << 2)) & 0x33333333;
    i1 = (i1 | (i1 << 1)) & 0x55555555;
    return (i1 << 1) | i0;
}

uint32_t tg_point_hilbert(struct tg_point point, struct tg_rect rect) {
    uint32_t ix = ((point.x - rect.min.x) / (rect.max.x - rect.min.x)) * 0xFFFF;
    uint32_t iy = ((point.y - rect.min.y) / (rect.max.y - rect.min.y)) * 0xFFFF;
    return hilbert_xy_to_index(ix, iy);
}

struct hildex {
    uint32_t hilbert;
    int index;
};

static int hilsort(const void *a, const void *b) {
    const struct hildex *ha = a;
    const struct hildex *hb = b;
    return ha->hilbert < hb->hilbert ? -1 : ha->hilbert > hb->hilbert;
}

static bool multi_geom_inflate_index(struct multi *multi) {
    // inflate multi index base level and mbr in one pass
    struct index *index = multi->index;
    int nlevels = index->nlevels;
    int spread = index->spread;

    // fill the hilbert indexes
    struct hildex *hildexes = tg_malloc(sizeof(struct hildex)*multi->ngeoms);
    if (!hildexes) {
        return false;
    }
    for (int i = 0; i < multi->ngeoms; i++) {
        struct tg_point center = tg_rect_center(tg_geom_rect(multi->geoms[i]));
        hildexes[i].index = i;
        hildexes[i].hilbert = tg_point_hilbert(center, multi->rect);
    }
    qsort(hildexes, multi->ngeoms, sizeof(struct hildex), hilsort);
    for (int i = 0; i < multi->ngeoms; i++) {
        multi->ixgeoms[i] = hildexes[i].index;
    }
    tg_free(hildexes);

    struct ixrect ixrect;
    struct tg_rect rect0 = tg_geom_rect(multi->geoms[multi->ixgeoms[0]]);
    tg_rect_to_ixrect(&rect0, &ixrect);
    int j = 1;
    int k = 0;
    for (int i = 1; i < multi->ngeoms; i++) {
        struct tg_rect rect = tg_geom_rect(multi->geoms[multi->ixgeoms[i]]);
        multi->rect = tg_rect_expand(multi->rect, rect);
        struct ixrect ixrect2;
        tg_rect_to_ixrect(&rect, &ixrect2);
        if (j == spread) {
            index->levels[nlevels-1].rects[k] = ixrect;
            k++;
            j = 1;
            ixrect = ixrect2;
        } else {
            ixrect_expand(&ixrect, &ixrect2);
            j++;
        }
    }
    if (k < index->levels[nlevels-1].nrects) {
        index->levels[nlevels-1].rects[k] = ixrect;
        k++;
    }
    for (int lvl = nlevels-1; lvl > 0; lvl--) {
        struct level *level = &index->levels[lvl];
        struct ixrect ixrect = level->rects[0];
        int j = 1;
        int k = 0;
        for (int i = 1; i < level->nrects; i++) {
            if (j == spread) {
                index->levels[lvl-1].rects[k] = ixrect;
                k++;
                j = 1;
                ixrect = index->levels[lvl].rects[i];
            } else {
                ixrect_expand(&ixrect, &index->levels[lvl].rects[i]);
                j++;
            }
        }
        if (k < index->levels[lvl-1].nrects) {
            index->levels[lvl-1].rects[k] = ixrect;
            k++;
        }
    }
    return true;
}

static struct tg_geom *multi_geom_inflate_rect(struct tg_geom *geom) {
    if (geom->multi->ngeoms == 0) {
        geom->multi->rect = (struct tg_rect){ 0 };
        return geom;
    }
    geom->multi->rect = tg_geom_rect(geom->multi->geoms[0]);
    for (int i = 1; i < geom->multi->ngeoms; i++) {
        struct tg_rect rect = tg_geom_rect(geom->multi->geoms[i]);
        geom->multi->rect = tg_rect_expand(geom->multi->rect, rect);
    }
    if (geom->multi->index) {
        if (!multi_geom_inflate_index(geom->multi)) {
            tg_geom_free(geom);
            return NULL;
        }
    }
    return geom;
}

static const struct multi *geom_multi(const struct tg_geom *geom) {
    if (geom && geom->head.base == BASE_GEOM && (
        geom->head.type == TG_MULTIPOINT ||
        geom->head.type == TG_MULTILINESTRING ||
        geom->head.type == TG_MULTIPOLYGON ||
        geom->head.type == TG_GEOMETRYCOLLECTION))
    {
        return geom->multi;
    }
    return NULL;
}

static const struct index *geom_multi_index(const struct tg_geom *geom) {
    const struct multi *multi = geom_multi(geom);
    return multi ? multi->index : NULL;
}

int tg_geom_multi_index_spread(const struct tg_geom *geom) {
    const struct index *index = geom_multi_index(geom);
    return index_spread(index);
}

int tg_geom_multi_index_num_levels(const struct tg_geom *geom) {
    const struct index *index = geom_multi_index(geom);
    return index_num_levels(index);
}

int tg_geom_multi_index_level_num_rects(const struct tg_geom *geom, 
    int levelidx)
{
    const struct index *index = geom_multi_index(geom);
    return index_level_num_rects(index, levelidx);
}

struct tg_rect tg_geom_multi_index_level_rect(const struct tg_geom *geom, 
    int levelidx, int rectidx)
{
    const struct index *index = geom_multi_index(geom);
    return index_level_rect(index, levelidx, rectidx);
}

/// Creates a MultiPoint geometry.
/// @param points An array of points, caller retains ownership.
/// @param npoints The number of points in array
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_multipoint(const struct tg_point *points,
    int npoints)
{
    struct tg_geom *geom = geom_new_multi(TG_MULTIPOINT, npoints);
    if (!geom) return NULL;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        geom->multi->geoms[i] = tg_geom_new_point(points[i]);
        if (!geom->multi->geoms[i]) {
            tg_geom_free(geom);
            return NULL;
        }
    }
    return multi_geom_inflate_rect(geom);
}

/// Creates an empty MultiPoint geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipoint_empty(void) {
    return geom_new_empty(TG_MULTIPOINT);
}

/// Creates a MultiLineString geometry.
/// @param lines An array of lines, caller retains ownership.
/// @param nlines The number of lines in array
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_multilinestring(const struct tg_line *const lines[],
    int nlines)
{
    struct tg_geom *geom = geom_new_multi(TG_MULTILINESTRING, nlines);
    if (!geom) return NULL;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        geom->multi->geoms[i] = (struct tg_geom*)tg_line_clone(lines[i]);
    }
    return multi_geom_inflate_rect(geom);
}

/// Creates an empty MultiLineString geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multilinestring_empty(void) {
    return geom_new_empty(TG_MULTILINESTRING);
}

/// Creates a MultiPolygon geometry.
/// @param polys An array of polygons, caller retains ownership.
/// @param npolys The number of polygons in array
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_multipolygon(const struct tg_poly *const polys[], 
    int npolys)
{
    struct tg_geom *geom = geom_new_multi(TG_MULTIPOLYGON, npolys);
    if (!geom) return NULL;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        geom->multi->geoms[i] = (struct tg_geom*)tg_poly_clone(polys[i]);
    }
    return multi_geom_inflate_rect(geom);
}

/// Creates an empty MultiPolygon geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipolygon_empty(void) {
    return geom_new_empty(TG_MULTIPOLYGON);
}

/// Creates a GeometryCollection geometry.
/// @param geoms An array of geometries, caller retains ownership.
/// @param ngeoms The number of geometries in array
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructors
struct tg_geom *tg_geom_new_geometrycollection(
    const struct tg_geom *const geoms[], int ngeoms)
{
    struct tg_geom *geom = geom_new_multi(TG_GEOMETRYCOLLECTION, ngeoms);
    if (!geom) return NULL;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        geom->multi->geoms[i] = tg_geom_clone(geoms[i]);
    }
    return multi_geom_inflate_rect(geom);
}

/// Creates an empty GeometryCollection geometry.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_geometrycollection_empty(void) {
    return geom_new_empty(TG_GEOMETRYCOLLECTION);
}

static struct tg_geom *fill_extra_coords(struct tg_geom *geom,
    const double *coords, int ncoords, enum flags flags)
{
    ncoords = ncoords < 0 ? 0 : ncoords;
    // if (!geom) return NULL; // already checked
    geom->head.flags = flags;
    geom->ncoords = ncoords;
    if (ncoords == 0) {
        geom->coords = NULL;
    } else {
        geom->coords = tg_malloc(ncoords*sizeof(double));
        if (!geom->coords) {
            tg_geom_free(geom);
            return NULL;
        }
        memcpy(geom->coords, coords, ncoords*sizeof(double));
    }
    return geom;
}

/// Creates a LineString geometry that includes Z coordinates.
/// @param line Input line, caller retains ownership.
/// @param coords Array of doubles representing each Z coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_linestring_z(const struct tg_line *line, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_LINESTRING);
    if (!geom) return NULL;
    geom->line = tg_line_clone(line);
    return fill_extra_coords(geom, coords, ncoords, HAS_Z);
}

/// Creates a LineString geometry that includes M coordinates.
/// @param line Input line, caller retains ownership.
/// @param coords Array of doubles representing each M coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_linestring_m(const struct tg_line *line, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_LINESTRING);
    if (!geom) return NULL;
    geom->line = tg_line_clone(line);
    return fill_extra_coords(geom, coords, ncoords, HAS_M);
}

/// Creates a LineString geometry that includes ZM coordinates.
/// @param line Input line, caller retains ownership.
/// @param coords Array of doubles representing each Z and M coordinate, 
/// interleaved. Caller retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_linestring_zm(const struct tg_line *line, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_LINESTRING);
    if (!geom) return NULL;
    geom->line = tg_line_clone(line);
    return fill_extra_coords(geom, coords, ncoords, HAS_Z|HAS_M);
}

/// Creates a Polygon geometry that includes Z coordinates.
/// @param poly Input polygon, caller retains ownership.
/// @param coords Array of doubles representing each Z coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_polygon_z(const struct tg_poly *poly, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_POLYGON);
    if (!geom) return NULL;
    geom->poly = tg_poly_clone(poly);
    return fill_extra_coords(geom, coords, ncoords, HAS_Z);
}

/// Creates a Polygon geometry that includes M coordinates.
/// @param poly Input polygon, caller retains ownership.
/// @param coords Array of doubles representing each M coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_polygon_m(const struct tg_poly *poly, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_POLYGON);
    if (!geom) return NULL;
    geom->poly = tg_poly_clone(poly);
    return fill_extra_coords(geom, coords, ncoords, HAS_M);
}

/// Creates a Polygon geometry that includes ZM coordinates.
/// @param poly Input polygon, caller retains ownership.
/// @param coords Array of doubles representing each Z and M coordinate, 
/// interleaved. Caller retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_polygon_zm(const struct tg_poly *poly, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = geom_new(TG_POLYGON);
    if (!geom) return NULL;
    geom->poly = tg_poly_clone(poly);
    return fill_extra_coords(geom, coords, ncoords, HAS_Z|HAS_M);
}

/// Creates a MultiPoint geometry that includes Z coordinates.
/// @param points An array of points, caller retains ownership.
/// @param npoints The number of points in array
/// @param coords Array of doubles representing each Z coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipoint_z(const struct tg_point *points, 
    int npoints, const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipoint(points, npoints);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z);
}

/// Creates a MultiPoint geometry that includes M coordinates.
/// @param points An array of points, caller retains ownership.
/// @param npoints The number of points in array
/// @param coords Array of doubles representing each M coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipoint_m(const struct tg_point *points, 
    int npoints, const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipoint(points, npoints);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_M);
}

/// Creates a MultiPoint geometry that includes ZM coordinates.
/// @param points An array of points, caller retains ownership.
/// @param npoints The number of points in array
/// @param coords Array of doubles representing each Z and M coordinate, 
/// interleaved. Caller retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipoint_zm(const struct tg_point *points, 
    int npoints, const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipoint(points, npoints);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z|HAS_M);
}

/// Creates a MultiLineString geometry that includes Z coordinates.
/// @param lines An array of lines, caller retains ownership.
/// @param nlines The number of lines in array
/// @param coords Array of doubles representing each Z coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multilinestring_z(
    const struct tg_line *const lines[], int nlines, const double *coords,
    int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multilinestring(lines, nlines);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z);
}

/// Creates a MultiLineString geometry that includes M coordinates.
/// @param lines An array of lines, caller retains ownership.
/// @param nlines The number of lines in array
/// @param coords Array of doubles representing each M coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multilinestring_m(
    const struct tg_line *const lines[], int nlines,
    const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multilinestring(lines, nlines);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_M);
}

/// Creates a MultiLineString geometry that includes ZM coordinates.
/// @param lines An array of lines, caller retains ownership.
/// @param nlines The number of lines in array
/// @param coords Array of doubles representing each Z and M coordinate, 
/// interleaved. Caller retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multilinestring_zm(
    const struct tg_line *const lines[], int nlines, 
    const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multilinestring(lines, nlines);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z|HAS_M);
}

/// Creates a MultiPolygon geometry that includes Z coordinates.
/// @param polys An array of polygons, caller retains ownership.
/// @param npolys The number of polygons in array
/// @param coords Array of doubles representing each Z coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipolygon_z(
    const struct tg_poly *const polys[], int npolys,
    const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipolygon(polys, npolys);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z);
}

/// Creates a MultiPolygon geometry that includes M coordinates.
/// @param polys An array of polygons, caller retains ownership.
/// @param npolys The number of polygons in array
/// @param coords Array of doubles representing each M coordinate, caller
/// retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipolygon_m(
    const struct tg_poly *const polys[], int npolys,
    const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipolygon(polys, npolys);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_M);
}

/// Creates a MultiPolygon geometry that includes ZM coordinates.
/// @param polys An array of polygons, caller retains ownership.
/// @param npolys The number of polygons in array
/// @param coords Array of doubles representing each Z and M coordinate, 
/// interleaved. Caller retains ownership.
/// @param ncoords Number of doubles in array.
/// @return A newly allocated geometry.
/// @return NULL if system is out of memory. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @see GeometryConstructorsEx
struct tg_geom *tg_geom_new_multipolygon_zm(
    const struct tg_poly *const polys[], int npolys,
    const double *coords, int ncoords)
{
    struct tg_geom *geom = tg_geom_new_multipolygon(polys, npolys);
    if (!geom) return NULL;
    return fill_extra_coords(geom, coords, ncoords, HAS_Z|HAS_M);
}

/// Clones a geometry
/// @param geom Input geometry, caller retains ownership.
/// @return A duplicate of the provided geometry. 
/// @note The caller is responsible for freeing with tg_geom_free().
/// @note This method of cloning uses implicit sharing through an atomic 
/// reference counter.
/// @see GeometryConstructors
struct tg_geom *tg_geom_clone(const struct tg_geom *geom) {
    if (!geom || geom->head.noheap) {
        return tg_geom_copy(geom);
    }
    struct tg_geom *geom_mut = (struct tg_geom*)geom;
    rc_retain(&geom_mut->head.rc);
    return geom_mut;
}

static void geom_free(struct tg_geom *geom) {
    if (geom->head.noheap || !rc_release(&geom->head.rc)) return;
    switch (geom->head.type) {
    case TG_POINT:
        break;
    case TG_LINESTRING:
        tg_line_free(geom->line);
        break;
    case TG_POLYGON:
        tg_poly_free(geom->poly);
        break;
    case TG_MULTIPOINT:
    case TG_MULTILINESTRING:
    case TG_MULTIPOLYGON:
    case TG_GEOMETRYCOLLECTION:
        if (geom->multi) {
            if (geom->multi->geoms) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    tg_geom_free(geom->multi->geoms[i]);
                }
                tg_free(geom->multi->geoms);
            }
            if (geom->multi->index) {
                tg_free(geom->multi->index);
            }
            if (geom->multi->ixgeoms) {
                tg_free(geom->multi->ixgeoms);
            }
            tg_free(geom->multi);
        }
        break;
    }
    if (geom->head.type != TG_POINT && geom->coords) {
        tg_free(geom->coords);
    }
    if (geom->error) {
        // error and xjson share the same memory, so this copy covers both.
        tg_free(geom->error);
    }
    tg_free(geom);
}


/// Releases the memory associated with a geometry.
/// @param geom Input geometry
/// @see GeometryConstructors
void tg_geom_free(struct tg_geom *geom) {
    if (!geom) {
        return;
    }
    switch (geom->head.base) {
    case BASE_GEOM:
        geom_free(geom);
        break;
    case BASE_POINT: 
        boxed_point_free((struct boxed_point*)geom);
        break;
    case BASE_LINE:
        tg_line_free((struct tg_line*)geom);
        break;
    case BASE_RING:
        tg_ring_free((struct tg_ring*)geom);
        break;
    case BASE_POLY:
        tg_poly_free((struct tg_poly*)geom);
        break;
    }
}

/// Returns the geometry type. e.g. TG_POINT, TG_POLYGON, TG_LINESTRING
/// @param geom Input geometry
/// @return The geometry type
/// @see [tg_geom_type](.#tg_geom_type)
/// @see tg_geom_type_string()
/// @see GeometryAccessors
enum tg_geom_type tg_geom_typeof(const struct tg_geom *geom) {
    if (!geom) return 0;
    return geom->head.type;
}

/// Returns true if the geometry is a GeoJSON Feature.
/// @param geom Input geometry
/// @return True or false
/// @see GeometryAccessors
bool tg_geom_is_feature(const struct tg_geom *geom) {
    return geom && (geom->head.flags&IS_FEATURE) == IS_FEATURE;
}

/// Returns true if the geometry is a GeoJSON FeatureCollection.
/// @param geom Input geometry
/// @return True or false
/// @see GeometryAccessors
bool tg_geom_is_featurecollection(const struct tg_geom *geom) {
    return geom && (geom->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL;
}

static struct tg_rect geom_rect(const struct tg_geom *geom) {
    struct tg_rect rect = { 0 };
    switch (geom->head.type) {
    case TG_POINT:
        return tg_point_rect(geom->point);
    case TG_LINESTRING: 
        return tg_line_rect(geom->line);
    case TG_POLYGON:
        return tg_poly_rect(geom->poly);
    case TG_MULTIPOINT:
    case TG_MULTILINESTRING:
    case TG_MULTIPOLYGON:
    case TG_GEOMETRYCOLLECTION:
        if (geom->multi) {
            rect = geom->multi->rect;
        }
    }
    return rect;
}

/// Returns the minimum bounding rectangle of a geometry.
/// @param geom Input geometry
/// @return Minumum bounding rectangle
/// @see tg_rect
/// @see GeometryAccessors
struct tg_rect tg_geom_rect(const struct tg_geom *geom) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return geom_rect(geom);
        case BASE_POINT: 
            return tg_point_rect(((struct boxed_point*)geom)->point);
        case BASE_LINE:
            return tg_line_rect((struct tg_line*)geom);
        case BASE_RING:
            return tg_ring_rect((struct tg_ring*)geom);
        case BASE_POLY:
            return tg_poly_rect((struct tg_poly*)geom);
        }
    }
    return (struct tg_rect) { 0 };
}

/// Returns the underlying point for the provided geometry.
/// @param geom Input geometry
/// @return For a TG_POINT geometry, returns the point.
/// @return For everything else returns the center of the geometry's bounding
/// rectangle.
/// @see tg_point
/// @see GeometryAccessors
struct tg_point tg_geom_point(const struct tg_geom *geom) {
    if (!geom) return (struct tg_point) { 0 };
    if (geom->head.base == BASE_POINT) {
        return ((struct boxed_point*)geom)->point;
    }
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_POINT) {
        return geom->point;
    }
    struct tg_rect rect = tg_geom_rect(geom);
    return (struct tg_point) { 
        (rect.min.x + rect.max.x) / 2, 
        (rect.min.y + rect.max.y) / 2,
    };
}

static size_t geom_memsize(const struct tg_geom *geom) {
    size_t size = sizeof(struct tg_geom);
    switch (geom->head.type) {
    case TG_POINT:
        break;
    case TG_LINESTRING:
        size += tg_line_memsize(geom->line);
        break;
    case TG_POLYGON: 
        size += tg_poly_memsize(geom->poly);
        break;
    case TG_MULTIPOINT: 
    case TG_MULTILINESTRING: 
    case TG_MULTIPOLYGON:
    case TG_GEOMETRYCOLLECTION: 
        if (geom->multi) {
            size += sizeof(struct multi);
            size += geom->multi->ngeoms*sizeof(struct tg_geom*);
            for (int i = 0; i < geom->multi->ngeoms; i++) {
                size += tg_geom_memsize(geom->multi->geoms[i]);
            }
            if (geom->multi->index) {
                size += geom->multi->index->memsz;
            }
            if (geom->multi->ixgeoms) {
                size += geom->multi->ngeoms*sizeof(int);
            }
        }
        break;
    }
    if (geom->head.type != TG_POINT && geom->coords) {
        size += geom->ncoords*sizeof(double);
    }
    if (geom->xjson) {
        // geom->error shares the same memory stored as a C string.
        size += strlen(geom->xjson)+1;
    }
    return size;
}

/// Returns the allocation size of the geometry.
/// @param geom Input geometry
/// @return Size of geometry in bytes
size_t tg_geom_memsize(const struct tg_geom *geom) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return geom_memsize(geom);
        case BASE_POINT: 
            return sizeof(struct boxed_point);
        case BASE_LINE:
            return tg_line_memsize((struct tg_line*)geom);
        case BASE_RING:
            return tg_ring_memsize((struct tg_ring*)geom);
        case BASE_POLY:
            return tg_poly_memsize((struct tg_poly*)geom);
        }
    }
    return 0;
}

/// Returns the underlying line for the provided geometry.
/// @param geom Input geometry
/// @return For a TG_LINESTRING geometry, returns the line.
/// @return For everything else returns NULL.
/// @see tg_line
/// @see GeometryAccessors
const struct tg_line *tg_geom_line(const struct tg_geom *geom) {
    if (!geom) return NULL;
    if (geom->head.base == BASE_LINE) return (struct tg_line*)geom;
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_LINESTRING) {
        return geom->line;
    }
    return NULL;
}

/// Returns the underlying polygon for the provided geometry.
/// @param geom Input geometry
/// @return For a TG_POLYGON geometry, returns the polygon.
/// @return For everything else returns NULL.
/// @see tg_poly
/// @see GeometryAccessors
const struct tg_poly *tg_geom_poly(const struct tg_geom *geom) {
    if (!geom) return NULL;
    if (geom->head.base == BASE_RING) return (struct tg_poly*)geom;
    if (geom->head.base == BASE_POLY) return (struct tg_poly*)geom;
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_POLYGON) {
        return geom->poly;
    }
    return NULL;
}

/// Returns the number of points in a MultiPoint geometry.
/// @param geom Input geometry
/// @return For a TG_MULTIPOINT geometry, returns the number of points.
/// @return For everything else returns zero.
/// @see tg_geom_point_at()
/// @see GeometryAccessors
int tg_geom_num_points(const struct tg_geom *geom) {
    if (!geom) return 0;
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_MULTIPOINT &&
        geom->multi)
    {
        return geom->multi->ngeoms;
    }
    return 0;
}

/// Returns the number of lines in a MultiLineString geometry.
/// @param geom Input geometry
/// @return For a TG_MULTILINESTRING geometry, returns the number of lines.
/// @return For everything else returns zero.
/// @see tg_geom_line_at()
/// @see GeometryAccessors
int tg_geom_num_lines(const struct tg_geom *geom) {
    if (!geom) return 0;
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_MULTILINESTRING &&
        geom->multi)
    {
        return geom->multi->ngeoms;
    }
    return 0;
}

/// Returns the number of polygons in a MultiPolygon geometry.
/// @param geom Input geometry
/// @return For a TG_MULTIPOLYGON geometry, returns the number of polygons.
/// @return For everything else returns zero.
/// @see tg_geom_poly_at()
/// @see GeometryAccessors
int tg_geom_num_polys(const struct tg_geom *geom) {
    if (!geom) return 0;
    if (geom->head.base == BASE_GEOM && geom->head.type == TG_MULTIPOLYGON &&
        geom->multi)
    {
        return geom->multi->ngeoms;
    }
    return 0;
}

/// Returns the number of geometries in a GeometryCollection geometry.
/// @param geom Input geometry
/// @return For a TG_MULTIGEOMETRY geometry, returns the number of geometries.
/// @return For everything else returns zero.
/// @note A geometry that is a GeoJSON FeatureCollection can use this function
/// to get number features in its collection.
/// @see tg_geom_geometry_at()
/// @see tg_geom_is_featurecollection()
/// @see GeometryAccessors
int tg_geom_num_geometries(const struct tg_geom *geom) {
    if (!geom) return 0;
    if (geom->head.base == BASE_GEOM && 
        geom->head.type == TG_GEOMETRYCOLLECTION && geom->multi)
    {
        return geom->multi->ngeoms;
    }
    return 0;
}

/// Returns the point at index for a MultiPoint geometry.
/// @param geom Input geometry
/// @param index Index of point
/// @return The point at index. Returns an empty point if the geometry type is
/// not TG_MULTIPOINT or when the provided index is out of range.
/// @see tg_geom_num_points()
/// @see GeometryAccessors
struct tg_point tg_geom_point_at(const struct tg_geom *geom, int index) {
    if (geom && geom->head.base == BASE_GEOM && 
        geom->head.type == TG_MULTIPOINT && 
        geom->multi &&index >= 0 && index <= geom->multi->ngeoms)
    {
        return ((struct boxed_point*)geom->multi->geoms[index])->point;
    }
    return (struct tg_point) { 0 };
}

/// Returns the line at index for a MultiLineString geometry.
/// @param geom Input geometry
/// @param index Index of line
/// @return The line at index. Returns NULL if the geometry type is not 
/// TG_MULTILINE or when the provided index is out of range.
/// @see tg_geom_num_lines()
/// @see GeometryAccessors
const struct tg_line *tg_geom_line_at(const struct tg_geom *geom, int index) {
    if (geom && geom->head.base == BASE_GEOM && 
        geom->head.type == TG_MULTILINESTRING && 
        geom->multi &&index >= 0 && index <= geom->multi->ngeoms)
    {
        return (struct tg_line*)geom->multi->geoms[index];
    }
    return NULL;
}

/// Returns the polygon at index for a MultiPolygon geometry.
/// @param geom Input geometry
/// @param index Index of polygon
/// @return The polygon at index. Returns NULL if the geometry type is not 
/// TG_MULTIPOLYGON or when the provided index is out of range.
/// @see tg_geom_num_polys()
/// @see GeometryAccessors
const struct tg_poly *tg_geom_poly_at(const struct tg_geom *geom, int index) {
    if (geom && geom->head.base == BASE_GEOM && 
        geom->head.type == TG_MULTIPOLYGON && 
        geom->multi && index >= 0 && index <= geom->multi->ngeoms)
    {
        return (struct tg_poly *)geom->multi->geoms[index];
    }
    return NULL;
}

/// Returns the geometry at index for a GeometryCollection geometry.
/// @param geom Input geometry
/// @param index Index of geometry
/// @return For a TG_MULTIGEOMETRY geometry, returns the number of geometries.
/// @return For everything else returns zero.
/// @note A geometry that is a GeoJSON FeatureCollection can use this
/// function to get number features in its collection.
/// @see tg_geom_geometry_at()
/// @see tg_geom_is_featurecollection()
/// @see GeometryAccessors
const struct tg_geom *tg_geom_geometry_at(const struct tg_geom *geom, 
    int index)
{
    if (geom && geom->head.base == BASE_GEOM && 
        geom->head.type == TG_GEOMETRYCOLLECTION && 
        geom->multi && index >= 0 && index <= geom->multi->ngeoms)
    {
        return geom->multi->geoms[index];
    }
    return NULL;
}

static bool geom_foreach(const struct tg_geom *geom,
    bool(*iter)(const struct tg_geom *geom, void *udata),
    void *udata)
{
    if (!geom) {
        return true;
    }
    if (geom->head.base == BASE_GEOM) {
        switch (geom->head.type) {
        case TG_MULTIPOINT:
        case TG_MULTILINESTRING:
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION:
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (!iter(geom->multi->geoms[i], udata)) return false;
                }
            }
            return true;
        default:
            break;
        }
    }
    return iter(geom, udata);
}

/// tg_geom_foreach flattens a Multi* or GeometryCollection, iterating over
/// every child (or child of child in case of nested geometry collection).
/// If the provided geometry is not a Multi* or GeometryCollection then only 
/// that geometry is returned by the iter.
/// Empty geometries are not returned.
void tg_geom_foreach(const struct tg_geom *geom,
    bool(*iter)(const struct tg_geom *geom, void *udata),
    void *udata)
{
    geom_foreach(geom, iter, udata);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////  Intersects
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool point_intersects_geom(struct tg_point point,
    const struct tg_geom *geom);

static bool point_intersects_base_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_point_intersects_point(point, geom->point); 
        case TG_LINESTRING: 
            return tg_point_intersects_line(point, geom->line);
        case TG_POLYGON: 
            return tg_point_intersects_poly(point, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION:
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (point_intersects_geom(point, geom->multi->geoms[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

static bool point_intersects_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return point_intersects_base_geom(point, geom);
        case BASE_POINT:
            return tg_point_intersects_point(point, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_point_intersects_line(point, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_point_intersects_poly(point, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_point_intersects_poly(point, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool line_intersects_geom(struct tg_line *line,
    const struct tg_geom *geom);

static bool line_intersects_base_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_line_intersects_point(line, geom->point); 
        case TG_LINESTRING: 
            return tg_line_intersects_line(line, geom->line);
        case TG_POLYGON: 
            return tg_line_intersects_poly(line, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (line_intersects_geom(line, geom->multi->geoms[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

static bool line_intersects_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return line_intersects_base_geom(line, geom);
        case BASE_POINT:
            return tg_line_intersects_point(line, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_line_intersects_line(line, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_line_intersects_poly(line, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_line_intersects_poly(line, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool poly_intersects_geom(struct tg_poly *poly, 
    const struct tg_geom *geom);

static bool poly_intersects_base_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_poly_intersects_point(poly, geom->point); 
        case TG_LINESTRING: 
            return tg_poly_intersects_line(poly, geom->line);
        case TG_POLYGON: 
            return tg_poly_intersects_poly(poly, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (poly_intersects_geom(poly, geom->multi->geoms[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

static bool poly_intersects_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return poly_intersects_base_geom(poly, geom);
        case BASE_POINT:
            return tg_poly_intersects_point(poly, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_poly_intersects_line(poly, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_poly_intersects_poly(poly, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_poly_intersects_poly(poly, ((struct tg_poly*)geom));
        }
    }
    return false;
}

struct multiiterctx {
    bool isect;
    const struct tg_geom *other;
};

static bool multiiter(const struct tg_geom *geom, int index, void *udata) {
    (void)index;
    struct multiiterctx *ctx = udata;
    if (tg_geom_intersects(geom, ctx->other)) {
        ctx->isect = true;
        return false;
    }
    return true;
}

static bool base_geom_intersects_geom(const struct tg_geom *geom, 
    const struct tg_geom *other)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return point_intersects_geom(geom->point, other);
        case TG_LINESTRING: 
            return line_intersects_geom(geom->line, other);
        case TG_POLYGON: 
            return poly_intersects_geom(geom->poly, other);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            struct multiiterctx ctx = { .other = other };
            tg_geom_search(geom, tg_geom_rect(geom), multiiter, &ctx);
            return ctx.isect;
        }}
    }
    return false;
}

/// Tests whether two geometries intersect.
/// @see GeometryPredicates
bool tg_geom_intersects(const struct tg_geom *geom, 
    const struct tg_geom *other)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return base_geom_intersects_geom(geom, other);
        case BASE_POINT: 
            return point_intersects_geom(((struct boxed_point*)geom)->point, 
                other);
        case BASE_LINE:
            return line_intersects_geom((struct tg_line*)geom, other);
        case BASE_RING:
            return poly_intersects_geom((struct tg_poly*)geom, other);
        case BASE_POLY:
            return poly_intersects_geom((struct tg_poly*)geom, other);
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////  Covers
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool point_covers_geom(struct tg_point point,
    const struct tg_geom *geom);

static bool point_covers_base_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_point_covers_point(point, geom->point); 
        case TG_LINESTRING: 
            return tg_point_covers_line(point, geom->line);
        case TG_POLYGON: 
            return tg_point_covers_poly(point, geom->poly);
        case TG_MULTIPOINT:
        case TG_MULTILINESTRING:
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (!geom->multi || geom->multi->ngeoms == 0) return false;
            for (int i = 0; i < geom->multi->ngeoms; i++) {
                if (!point_covers_geom(point, geom->multi->geoms[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static bool point_covers_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return point_covers_base_geom(point, geom);
        case BASE_POINT:
            return tg_point_covers_point(point, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_point_covers_line(point, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_point_covers_poly(point, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_point_covers_poly(point, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool line_covers_geom(struct tg_line *line,
    const struct tg_geom *geom);

static bool line_covers_base_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_line_covers_point(line, geom->point); 
        case TG_LINESTRING: 
            return tg_line_covers_line(line, geom->line);
        case TG_POLYGON: 
            return tg_line_covers_poly(line, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (!geom->multi || geom->multi->ngeoms == 0) return false;
            for (int i = 0; i < geom->multi->ngeoms; i++) {
                if (!line_covers_geom(line, geom->multi->geoms[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static bool line_covers_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return line_covers_base_geom(line, geom);
        case BASE_POINT:
            return tg_line_covers_point(line, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_line_covers_line(line, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_line_covers_poly(line, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_line_covers_poly(line, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool poly_covers_geom(struct tg_poly *poly, 
    const struct tg_geom *geom);

static bool poly_covers_base_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_poly_covers_point(poly, geom->point); 
        case TG_LINESTRING: 
            return tg_poly_covers_line(poly, geom->line);
        case TG_POLYGON: 
            return tg_poly_covers_poly(poly, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (!geom->multi || geom->multi->ngeoms == 0) return false;
            for (int i = 0; i < geom->multi->ngeoms; i++) {
                if (!poly_covers_geom(poly, geom->multi->geoms[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

static bool poly_covers_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return poly_covers_base_geom(poly, geom);
        case BASE_POINT:
            return tg_poly_covers_point(poly, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_poly_covers_line(poly, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_poly_covers_poly(poly, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_poly_covers_poly(poly, ((struct tg_poly*)geom));
        }
    }
    return false;
}

struct geom_covers_iter_ctx {
    const struct tg_geom *geom;
    bool result;
};

static bool geom_covers_iter0(const struct tg_geom *geom, void *udata) {
    struct geom_covers_iter_ctx *ctx = udata;
    if (tg_geom_covers(geom, ctx->geom)) {
        // found a child object that contains geom, end inner loop
        ctx->result = true;
        return false;
    }
    return true;
}

bool tg_geom_is_empty(const struct tg_geom *geom);

static bool base_geom_deep_empty(const struct tg_geom *geom) {
    switch (geom->head.type) {
    case TG_POINT:
        return false;
    case TG_LINESTRING:
        return tg_line_empty(geom->line);
    case TG_POLYGON:
        return tg_poly_empty(geom->poly);
    case TG_MULTIPOINT:
    case TG_MULTILINESTRING:
    case TG_MULTIPOLYGON:
    case TG_GEOMETRYCOLLECTION:
        if (geom->multi) {
            for (int i = 0; i < geom->multi->ngeoms; i++) {
                if (!tg_geom_is_empty(geom->multi->geoms[i])) {
                    return false;
                }
            }
        }
    }
    return true;
}

/// Tests whether a geometry is empty. An empty geometry is one that has no
/// interior boundary.
/// @param geom Input geometry
/// @return True or false
bool tg_geom_is_empty(const struct tg_geom *geom) {
    if (geom) {
        if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) return true;
        switch (geom->head.base) {
        case BASE_GEOM:
            return base_geom_deep_empty(geom);
        case BASE_POINT:
            return false;
        case BASE_LINE:
            return tg_line_empty((struct tg_line*)geom);
        case BASE_RING:
            return tg_poly_empty((struct tg_poly*)geom);
        case BASE_POLY:
            return tg_poly_empty((struct tg_poly*)geom);
        }
    }
    return true;
}

static bool geom_covers_iter(const struct tg_geom *geom, void *udata) {
    struct geom_covers_iter_ctx *ctx = udata;
    // skip empty geometries
    if (tg_geom_is_empty(geom)) {
        return true;
    }
    struct geom_covers_iter_ctx ctx0 = { .geom = geom };
    tg_geom_foreach(ctx->geom, geom_covers_iter0, &ctx0);
    if (!ctx0.result) {
        // unmark and quit the loop
        ctx->result = false;
        return false;
    }
    // mark that at least one geom is contained
    ctx->result = true;
    return true;
}

static bool base_geom_covers_geom(const struct tg_geom *geom, 
    const struct tg_geom *other)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT:
            return point_covers_geom(geom->point, other);
        case TG_LINESTRING:
            return line_covers_geom(geom->line, other);
        case TG_POLYGON:
            return poly_covers_geom(geom->poly, other);
        case TG_MULTIPOINT:
        case TG_MULTILINESTRING:
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            // all children of 'other' must be fully within 'geom'
            struct geom_covers_iter_ctx ctx = { .geom = geom };
            tg_geom_foreach(other, geom_covers_iter, &ctx);
            return ctx.result;
        }
        }
    }
    return false;
}

/// Tests whether a geometry 'a' fully contains geometry 'b'.
/// @see GeometryPredicates
bool tg_geom_covers(const struct tg_geom *geom, const struct tg_geom *other) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return base_geom_covers_geom(geom, other);
        case BASE_POINT: 
            return point_covers_geom(((struct boxed_point*)geom)->point,
                other);
        case BASE_LINE:
            return line_covers_geom((struct tg_line*)geom, other);
        case BASE_RING:
            return poly_covers_geom((struct tg_poly*)geom, other);
        case BASE_POLY:
            return poly_covers_geom((struct tg_poly*)geom, other);
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////  Contains
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool poly_contains_geom(struct tg_poly *poly, 
    const struct tg_geom *geom);

static bool poly_contains_base_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_poly_contains_point(poly, geom->point); 
        case TG_LINESTRING: 
            return tg_poly_contains_line(poly, geom->line);
        case TG_POLYGON: 
            return tg_poly_contains_poly(poly, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            bool contains = false;
            if (geom->multi && geom->multi->ngeoms > 0) {
                contains = true;
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (!poly_contains_geom(poly, geom->multi->geoms[i])) {
                        contains = false;
                        break;
                    }
                }
            }
            return contains;
        }}
    }
    return false;
}

static bool poly_contains_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return poly_contains_base_geom(poly, geom);
        case BASE_POINT:
            return tg_poly_contains_point(poly, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_poly_contains_line(poly, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_poly_contains_poly(poly, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_poly_contains_poly(poly, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool line_contains_geom(struct tg_line *line,
    const struct tg_geom *geom);

static bool line_contains_base_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_line_contains_point(line, geom->point); 
        case TG_LINESTRING: 
            return tg_line_contains_line(line, geom->line);
        case TG_POLYGON: 
            return tg_line_contains_poly(line, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            bool contains = false;
            if (geom->multi && geom->multi->ngeoms > 0) {
                contains = true;
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (!line_contains_geom(line, geom->multi->geoms[i])) {
                        contains = false;
                        break;
                    }
                }
            }
            return contains;
        }}
    }
    return false;
}

static bool line_contains_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return line_contains_base_geom(line, geom);
        case BASE_POINT:
            return tg_line_contains_point(line, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_line_contains_line(line, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_line_contains_poly(line, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_line_contains_poly(line, ((struct tg_poly*)geom));
        }
    }
    return false;
}


static bool point_contains_geom(struct tg_point point,
    const struct tg_geom *geom);

static bool point_contains_base_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_point_contains_point(point, geom->point); 
        case TG_LINESTRING: 
            return tg_point_contains_line(point, geom->line);
        case TG_POLYGON: 
            return tg_point_contains_poly(point, geom->poly);
        case TG_MULTIPOINT:
        case TG_MULTILINESTRING:
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            bool contains = false;
            if (geom->multi && geom->multi->ngeoms > 0) {
                contains = true;
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (!point_contains_geom(point, geom->multi->geoms[i])) {
                        contains = false;
                        break;
                    }
                }
            }
            return contains;
        }}
    }
    return false;
}

struct geom_contains_iter_ctx {
    const struct tg_geom *geom;
    bool result;
};


static bool geom_contains_iter0(const struct tg_geom *geom, void *udata) {
    struct geom_contains_iter_ctx *ctx = udata;
    if (tg_geom_contains(geom, ctx->geom)) {
        // found a child object that contains geom, end inner loop
        ctx->result = true;
        return false;
    }
    return true;
}

static bool geom_contains_iter(const struct tg_geom *geom, void *udata) {
    struct geom_contains_iter_ctx *ctx = udata;
    // skip empty geometries
    if (!tg_geom_is_empty(geom)) {
        struct geom_contains_iter_ctx ctx0 = { .geom = geom };
        tg_geom_foreach(ctx->geom, geom_contains_iter0, &ctx0);
        if (!ctx0.result) {
            // unmark and quit the loop
            ctx->result = false;
            return false;
        }
        // mark that at least one geom is contained
        ctx->result = true;
    }
    return true;
}

static bool base_geom_contains_geom(const struct tg_geom *geom, 
    const struct tg_geom *other)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT:
            return point_contains_geom(geom->point, other);
        case TG_LINESTRING:
            return line_contains_geom(geom->line, other);
        case TG_POLYGON:
            return poly_contains_geom(geom->poly, other);
        case TG_MULTIPOINT:
        case TG_MULTILINESTRING:
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            // all children of 'other' must be fully within 'geom'
            struct geom_contains_iter_ctx ctx = { .geom = geom };
            tg_geom_foreach(other, geom_contains_iter, &ctx);
            return ctx.result;
        }
        }
    }
    return false;
}

static bool point_contains_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return point_contains_base_geom(point, geom);
        case BASE_POINT:
            return tg_point_contains_point(point, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_point_contains_line(point, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_point_contains_poly(point, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_point_contains_poly(point, ((struct tg_poly*)geom));
        }
    }
    return false;
}

/// Tests whether 'a' contains 'b', and 'b' is not touching the boundary of 'a'.
/// @note Works the same as `tg_geom_within(b, a)`
/// @warning This predicate returns **false** when geometry 'b' is *on* or
/// *touching* the boundary of geometry 'a'. Such as when a point is on the
/// edge of a polygon.  
/// For full coverage, consider using @ref tg_geom_covers.
/// @see GeometryPredicates
bool tg_geom_contains(const struct tg_geom *geom, const struct tg_geom *other) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return base_geom_contains_geom(geom, other);
        case BASE_POINT: 
            return point_contains_geom(((struct boxed_point*)geom)->point,
                other);
        case BASE_LINE:
            return line_contains_geom((struct tg_line*)geom, other);
        case BASE_RING:
            return poly_contains_geom((struct tg_poly*)geom, other);
        case BASE_POLY:
            return poly_contains_geom((struct tg_poly*)geom, other);
        }
    }
    return false;
}

bool tg_poly_contains_geom(struct tg_poly *a, const struct tg_geom *b) {
    return poly_contains_geom(a, b);
}

bool tg_line_contains_geom(struct tg_line *a, const struct tg_geom *b) {
    return line_contains_geom(a, b);
}

bool tg_point_contains_geom(struct tg_point a, const struct tg_geom *b) {
    return point_contains_geom(a, b);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/////  Touches
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static bool point_touches_geom(struct tg_point point,
    const struct tg_geom *geom);

static bool point_touches_base_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_point_touches_point(point, geom->point); 
        case TG_LINESTRING: 
            return tg_point_touches_line(point, geom->line);
        case TG_POLYGON: 
            return tg_point_touches_poly(point, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION:
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (point_touches_geom(point, geom->multi->geoms[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

static bool point_touches_geom(struct tg_point point,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return point_touches_base_geom(point, geom);
        case BASE_POINT:
            return tg_point_touches_point(point, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_point_touches_line(point, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_point_touches_poly(point, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_point_touches_poly(point, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool line_touches_geom(struct tg_line *line,
    const struct tg_geom *geom);

static bool line_touches_base_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_line_touches_point(line, geom->point); 
        case TG_LINESTRING: 
            return tg_line_touches_line(line, geom->line);
        case TG_POLYGON: 
            return tg_line_touches_poly(line, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: 
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    if (line_touches_geom(line, geom->multi->geoms[i])) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

static bool line_touches_geom(struct tg_line *line,
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return line_touches_base_geom(line, geom);
        case BASE_POINT:
            return tg_line_touches_point(line, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_line_touches_line(line, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_line_touches_poly(line, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_line_touches_poly(line, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool poly_touches_geom(struct tg_poly *poly, 
    const struct tg_geom *geom);

static bool poly_touches_base_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return tg_poly_touches_point(poly, geom->point); 
        case TG_LINESTRING: 
            return tg_poly_touches_line(poly, geom->line);
        case TG_POLYGON: 
            return tg_poly_touches_poly(poly, geom->poly);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            bool touches = false;
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    const struct tg_geom *child = geom->multi->geoms[i];
                    if (poly_touches_geom(poly, child)) {
                        touches = true;
                    } else if (poly_intersects_geom(poly, child)) {
                        return false;
                    }
                }
            }
            return touches;
        }}
    }
    return false;
}

static bool poly_touches_geom(struct tg_poly *poly, 
    const struct tg_geom *geom)
{
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return poly_touches_base_geom(poly, geom);
        case BASE_POINT:
            return tg_poly_touches_point(poly, 
                ((struct boxed_point*)geom)->point); 
        case BASE_LINE:
            return tg_poly_touches_line(poly, ((struct tg_line*)geom));
        case BASE_RING:
            return tg_poly_touches_poly(poly, ((struct tg_poly*)geom));
        case BASE_POLY:
            return tg_poly_touches_poly(poly, ((struct tg_poly*)geom));
        }
    }
    return false;
}

static bool base_geom_touches_geom(const struct tg_geom *geom, 
    const struct tg_geom *other)
{
    if ((geom->head.flags&IS_EMPTY) != IS_EMPTY) {
        switch (geom->head.type) {
        case TG_POINT: 
            return point_touches_geom(geom->point, other);
        case TG_LINESTRING: 
            return line_touches_geom(geom->line, other);
        case TG_POLYGON: 
            return poly_touches_geom(geom->poly, other);
        case TG_MULTIPOINT: 
        case TG_MULTILINESTRING: 
        case TG_MULTIPOLYGON:
        case TG_GEOMETRYCOLLECTION: {
            bool touches = false;
            if (geom->multi) {
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    const struct tg_geom *child = geom->multi->geoms[i];
                    if (tg_geom_touches(child, other)) {
                        touches = true;
                    } else if (tg_geom_intersects(child, other)) {
                        return false;
                    }
                }
            }
            return touches;
         }}
    }
    return false;
}

/// Tests whether a geometry 'a' touches 'b'. 
/// They have at least one point in common, but their interiors do not
/// intersect.
/// @see GeometryPredicates
bool tg_geom_touches(const struct tg_geom *geom, const struct tg_geom *other) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return base_geom_touches_geom(geom, other);
        case BASE_POINT: 
            return point_touches_geom(((struct boxed_point*)geom)->point,
                other);
        case BASE_LINE:
            return line_touches_geom((struct tg_line*)geom, other);
        case BASE_RING:
            return poly_touches_geom((struct tg_poly*)geom, other);
        case BASE_POLY:
            return poly_touches_geom((struct tg_poly*)geom, other);
        }
    }
    return false;
}

bool tg_poly_touches_geom(struct tg_poly *a, const struct tg_geom *b) {
    return poly_touches_geom(a, b);
}

bool tg_line_touches_geom(struct tg_line *a, const struct tg_geom *b) {
    return line_touches_geom(a, b);
}

bool tg_point_touches_geom(struct tg_point a, const struct tg_geom *b) {
    return point_touches_geom(a, b);
}

/// Tests whether a geometry 'a' fully contains point 'b'.
/// @see GeometryPredicates
bool tg_geom_covers_point(const struct tg_geom *a, struct tg_point b) {
    struct boxed_point bpoint = {
        .head = { .base = BASE_POINT, .type = TG_POINT },
        .point = b,
    };
    return tg_geom_covers(a, (struct tg_geom*)&bpoint);
}

/// Tests whether a geometry fully contains a point using xy coordinates.
/// @see GeometryPredicates
bool tg_geom_covers_xy(const struct tg_geom *a, double x, double y) {
    return tg_geom_covers_point(a, (struct tg_point){ .x = x, .y = y });
}

/// Tests whether a geometry 'a' intersects point 'b'.
/// @see GeometryPredicates
bool tg_geom_intersects_point(const struct tg_geom *a, struct tg_point b) {
    struct boxed_point bpoint = {
        .head = { .base = BASE_POINT, .type = TG_POINT },
        .point = b,
    };
    return tg_geom_intersects(a, (struct tg_geom*)&bpoint);
}

/// Tests whether a geometry intersects a point using xy coordinates.
/// @see GeometryPredicates
bool tg_geom_intersects_xy(const struct tg_geom *a, double x, double y) {
    return tg_geom_intersects_point(a, (struct tg_point){ .x = x, .y = y });
}

/// Get the extra coordinates for a geometry.
/// @param geom Input geometry
/// @return Array of coordinates
/// @return NULL if there are no extra coordinates 
/// @note These are the raw coodinates provided by a constructor like 
/// tg_geom_new_polygon_z() or from a parsed source like WKT "POLYGON Z ...".
/// @see tg_geom_num_extra_coords()
const double *tg_geom_extra_coords(const struct tg_geom *geom) {
    if (!geom || geom->head.base != BASE_GEOM || geom->head.type == TG_POINT) {
        return NULL;
    }
    return geom->coords;
}

/// Get the number of extra coordinates for a geometry
/// @param geom Input geometry
/// @return The number of extra coordinates, or zero if none.
/// @see tg_geom_extra_coords()
int tg_geom_num_extra_coords(const struct tg_geom *geom) {
    if (!geom || geom->head.base != BASE_GEOM || geom->head.type == TG_POINT) {
        return 0;
    }
    return geom->ncoords;
}

/// Get the number of dimensions for a geometry.
/// @param geom Input geometry
/// @return 2 for standard geometries
/// @return 3 when geometry has Z or M coordinates
/// @return 4 when geometry has Z and M coordinates
/// @return 0 when input is NULL
int tg_geom_dims(const struct tg_geom *geom) {
    if (!geom) return 0;
    int dims = 2;
    if ((geom->head.flags&HAS_Z) == HAS_Z) dims++;
    if ((geom->head.flags&HAS_M) == HAS_M) dims++;
    return dims;
}

/// Tests whether a geometry has Z coordinates.
/// @param geom Input geometry
/// @return True or false
bool tg_geom_has_z(const struct tg_geom *geom) {
    return (geom && (geom->head.flags&HAS_Z) == HAS_Z);
}

/// Tests whether a geometry has M coordinates.
/// @param geom Input geometry
/// @return True or false
bool tg_geom_has_m(const struct tg_geom *geom) {
    return (geom && (geom->head.flags&HAS_M) == HAS_M);
}

/// Get the Z coordinate of a Point geometry.
/// @param geom Input geometry
/// @return For a TG_POINT geometry, returns the Z coodinate.
/// @return For everything else returns zero.
double tg_geom_z(const struct tg_geom *geom) {
    if (!geom || geom->head.base != BASE_GEOM || geom->head.type != TG_POINT) {
        return 0;
    }
    return geom->z;
}

/// Get the M coordinate of a Point geometry.
/// @param geom Input geometry
/// @return For a TG_POINT geometry, returns the M coodinate.
/// @return For everything else returns zero.
double tg_geom_m(const struct tg_geom *geom) {
    if (!geom || geom->head.base != BASE_GEOM || geom->head.type != TG_POINT) {
        return 0;
    }
    return geom->m;
}

/// Returns the indexing spread for a ring.
/// 
/// The "spread" is the number of segments or rectangles that are grouped 
/// together to produce a unioned rectangle that is stored at a higher level.
/// 
/// For a tree based structure, this would be the number of items per node.
///
/// @param ring Input ring
/// @return The spread, default is 16
/// @return Zero if ring has no indexing
/// @see tg_ring_index_num_levels()
/// @see tg_ring_index_level_num_rects()
/// @see tg_ring_index_level_rect()
/// @see RingFuncs
int tg_ring_index_spread(const struct tg_ring *ring) {
    struct index *index = ring ? ring->index : NULL;
    return index_spread(index);
}

/// Returns the number of levels.
/// @param ring Input ring
/// @return The number of levels
/// @return Zero if ring has no indexing
/// @see tg_ring_index_spread()
/// @see tg_ring_index_level_num_rects()
/// @see tg_ring_index_level_rect()
/// @see RingFuncs
int tg_ring_index_num_levels(const struct tg_ring *ring) {
    struct index *index = ring ? ring->index : NULL;
    return index_num_levels(index);
}

/// Returns the number of rectangles at level.
/// @param ring Input ring
/// @param levelidx The index of level
/// @return The number of index levels
/// @return Zero if ring has no indexing or levelidx is out of bounds.
/// @see tg_ring_index_spread()
/// @see tg_ring_index_num_levels()
/// @see tg_ring_index_level_rect()
/// @see RingFuncs
int tg_ring_index_level_num_rects(const struct tg_ring *ring, int levelidx) {
    struct index *index = ring ? ring->index : NULL;
    return index_level_num_rects(index, levelidx);
}

/// Returns a specific level rectangle.
/// @param ring Input ring
/// @param levelidx The index of level
/// @param rectidx The index of rectangle
/// @return The rectangle
/// @return Empty rectangle if ring has no indexing, or levelidx or rectidx
/// is out of bounds.
/// @see tg_ring_index_spread()
/// @see tg_ring_index_num_levels()
/// @see tg_ring_index_level_num_rects()
/// @see RingFuncs
struct tg_rect tg_ring_index_level_rect(const struct tg_ring *ring, 
    int levelidx, int rectidx)
{
    struct index *index = ring ? ring->index : NULL;
    return index_level_rect(index, levelidx, rectidx);
}

/// Get the string representation of a geometry type. 
/// e.g. "Point", "Polygon", "LineString".
/// @param type Input geometry type
/// @return A string representing the type
/// @note The returned string does not need to be freed.
/// @see tg_geom_typeof()
/// @see GeometryAccessors
const char *tg_geom_type_string(enum tg_geom_type type) {
    switch (type) {
    case TG_POINT: return "Point";
    case TG_LINESTRING: return "LineString";
    case TG_POLYGON: return "Polygon";
    case TG_MULTIPOINT: return "MultiPoint";
    case TG_MULTILINESTRING: return "MultiLineString";
    case TG_MULTIPOLYGON: return "MultiPolygon";
    case TG_GEOMETRYCOLLECTION: return "GeometryCollection";
    default: return "Unknown";
    }
}

/////////////////////////////////////////////
// Formats -- GeoJSON, WKT, WKB, HEX
/////////////////////////////////////////////

// maximum depth for all recursive input formats such as wkt, wkb, and json.
#define MAXDEPTH 1024

#ifndef JSON_MAXDEPTH
#define JSON_MAXDEPTH MAXDEPTH
#endif

#ifdef TG_NOAMALGA

#include "deps/json.h"

#else

#define JSON_STATIC

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// BEGIN json.c
// https://github.com/tidwall/json.c
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef JSON_STATIC
#include "json.h"
#else
enum json_type { 
    JSON_NULL,
    JSON_FALSE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_TRUE,
    JSON_ARRAY,
    JSON_OBJECT,
};
struct json { void *priv[4]; };

struct json_valid {
    bool valid;
    size_t pos;
};

#define JSON_EXTERN static
#endif

#ifndef JSON_EXTERN
#define JSON_EXTERN
#endif

#ifndef JSON_MAXDEPTH
#define JSON_MAXDEPTH 1024
#endif

struct vutf8res { int n; uint32_t cp; };

// parse and validate a single utf8 codepoint.
// The first byte has already been checked from the vstring function.
static inline struct vutf8res vutf8(const uint8_t data[], int64_t len) {
    uint32_t cp;
    int n = 0;
    if (data[0]>>4 == 14) {
        if (len < 3) goto fail;
        if (((data[1]>>6)|(data[2]>>6<<2)) != 10) goto fail;
        cp = ((uint32_t)(data[0]&15)<<12)|((uint32_t)(data[1]&63)<<6)|
             ((uint32_t)(data[2]&63));
        n = 3;
    } else if (data[0]>>3 == 30) {
        if (len < 4) goto fail;
        if (((data[1]>>6)|(data[2]>>6<<2)|(data[3]>>6<<4)) != 42) goto fail;
        cp = ((uint32_t)(data[0]&7)<<18)|((uint32_t)(data[1]&63)<<12)|
             ((uint32_t)(data[2]&63)<<6)|((uint32_t)(data[3]&63));
        n = 4;
    } else if (data[0]>>5 == 6) {
        if (len < 2) goto fail;
        if (data[1]>>6 != 2) goto fail;
        cp = ((uint32_t)(data[0]&31)<<6)|((uint32_t)(data[1]&63));
        n = 2;
    } else {
        goto fail;
    }
    if (cp < 128) goto fail; // don't allow multibyte ascii characters
    if (cp >= 0x10FFFF) goto fail; // restricted to utf-16
    if (cp >= 0xD800 && cp <= 0xDFFF) goto fail; // needs surrogate pairs
    return (struct vutf8res) { .n = n, .cp = cp };
fail:
    return (struct vutf8res) { 0 };
}

static inline int64_t vesc(const uint8_t *json, int64_t jlen, int64_t i) {
    // The first byte has already been checked from the vstring function.
    i += 1;
    if (i == jlen) return -(i+1);
    switch (json[i]) {
    case '"': case '\\': case '/': case 'b': case 'f': case 'n': 
    case 'r': case 't': return i+1;
    case 'u':
        for (int j = 0; j < 4; j++) {
            i++;
            if (i == jlen) return -(i+1);
            if (!((json[i] >= '0' && json[i] <= '9') ||
                  (json[i] >= 'a' && json[i] <= 'f') ||
                  (json[i] >= 'A' && json[i] <= 'F'))) return -(i+1);
        }
        return i+1;
    }
    return -(i+1);
}

#ifndef ludo
#define ludo
#define ludo1(i,f) f; i++;
#define ludo2(i,f) ludo1(i,f); ludo1(i,f);
#define ludo4(i,f) ludo2(i,f); ludo2(i,f);
#define ludo8(i,f) ludo4(i,f); ludo4(i,f);
#define ludo16(i,f) ludo8(i,f); ludo8(i,f);
#define ludo32(i,f) ludo16(i,f); ludo16(i,f);
#define ludo64(i,f) ludo32(i,f); ludo32(i,f);
#define for1(i,n,f) while(i+1<=(n)) { ludo1(i,f); }
#define for2(i,n,f) while(i+2<=(n)) { ludo2(i,f); } for1(i,n,f);
#define for4(i,n,f) while(i+4<=(n)) { ludo4(i,f); } for1(i,n,f);
#define for8(i,n,f) while(i+8<=(n)) { ludo8(i,f); } for1(i,n,f);
#define for16(i,n,f) while(i+16<=(n)) { ludo16(i,f); } for1(i,n,f);
#define for32(i,n,f) while(i+32<=(n)) { ludo32(i,f); } for1(i,n,f);
#define for64(i,n,f) while(i+64<=(n)) { ludo64(i,f); } for1(i,n,f);
#endif

static const uint8_t strtoksu[256] = {
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
#ifndef JSON_NOVALIDATEUTF8
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,
    //0=ascii, 1=quote, 2=escape, 3=utf82, 4=utf83, 5=utf84, 6=error
#endif
};

static int64_t vstring(const uint8_t *json, int64_t jlen, int64_t i) {
    while (1) {
        for8(i, jlen, { if (strtoksu[json[i]]) goto tok; })
        break;
    tok:
        if (json[i] == '"') {
            return i+1;
#ifndef JSON_NOVALIDATEUTF8
        } else if (json[i] > 127) {
            struct vutf8res res = vutf8(json+i, jlen-i);
            if (res.n == 0) break;
            i += res.n;
#endif
        } else if (json[i] == '\\') {
            if ((i = vesc(json, jlen, i)) < 0) break;
        } else {
            break;
        }
    } 
    return -(i+1);
}

static int64_t vnumber(const uint8_t *data, int64_t dlen, int64_t i) {
    i--;
    // sign
    if (data[i] == '-') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
    }
    // int 
    if (data[i] == '0') {
        i++;
    } else {
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    // frac
    if (i == dlen) return i;
    if (data[i] == '.') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    // exp
    if (i == dlen) return i;
    if (data[i] == 'e' || data[i] == 'E') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] == '+' || data[i] == '-') i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    return i;
}

static int64_t vnull(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+3 <= dlen && data[i] == 'u' && data[i+1] == 'l' &&
        data[i+2] == 'l' ? i+3 : -(i+1);
}

static int64_t vtrue(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+3 <= dlen && data[i] == 'r' && data[i+1] == 'u' &&
        data[i+2] == 'e' ? i+3 : -(i+1);
}

static int64_t vfalse(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+4 <= dlen && data[i] == 'a' && data[i+1] == 'l' &&
        data[i+2] == 's' && data[i+3] == 'e' ? i+4 : -(i+1);
}

static int64_t vcolon(const uint8_t *json, int64_t len, int64_t i) {
    if (i == len) return -(i+1);
    if (json[i] == ':') return i+1;
    do {
        switch (json[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ':': return i+1;
        default: return -(i+1);
        }
    } while (++i < len);
    return -(i+1);
}

static int64_t vcomma(const uint8_t *json, int64_t len, int64_t i, uint8_t end)
{
    if (i == len) return -(i+1);
    if (json[i] == ',') return i;
    do {
        switch (json[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ',': return i;
        default: return json[i] == end ? i : -(i+1);
        }
    } while (++i < len);
    return -(i+1);
}

static int64_t vany(const uint8_t *data, int64_t dlen, int64_t i, int depth);

static int64_t varray(const uint8_t *data, int64_t dlen, int64_t i, int depth) {
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ']': return i+1;
        default:
            for (; i < dlen; i++) {
                if ((i = vany(data, dlen, i, depth+1)) < 0) return i;
                if ((i = vcomma(data, dlen, i, ']')) < 0) return i;
                if (data[i] == ']') return i+1;
            }
        }
    }
    return -(i+1);
}

static int64_t vkey(const uint8_t *json, int64_t len, int64_t i) {
    for16(i, len, { if (strtoksu[json[i]]) goto tok; })
    return -(i+1);
tok:
    if (json[i] == '"') return i+1;
    return vstring(json, len, i);
}

static int64_t vobject(const uint8_t *data, int64_t dlen, int64_t i, int depth)
{
    for (; i < dlen; i++) {
        switch (data[i]) {
        case '"':
        key:
            if ((i = vkey(data, dlen, i+1)) < 0) return i;
            if ((i = vcolon(data, dlen, i)) < 0) return i;
            if ((i = vany(data, dlen, i, depth+1)) < 0) return i;
            if ((i = vcomma(data, dlen, i, '}')) < 0) return i;
            if (data[i] == '}') return i+1;
            i++;
            for (; i < dlen; i++) {
                switch (data[i]) {
                case ' ': case '\t': case '\n': case '\r': continue;
                case '"': goto key;
                default: return -(i+1);
                }
            }
            return -(i+1);
        case ' ': case '\t': case '\n': case '\r': continue;
        case '}': return i+1;
        default:
            return -(i+1);
        }
    }
    return -(i+1);
}

static int64_t vany(const uint8_t *data, int64_t dlen, int64_t i, int depth) {
    if (depth > JSON_MAXDEPTH) return -(i+1);
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case '{': return vobject(data, dlen, i+1, depth);
        case '[': return varray(data, dlen, i+1, depth);
        case '"': return vstring(data, dlen, i+1);
        case 't': return vtrue(data, dlen, i+1);
        case 'f': return vfalse(data, dlen, i+1);
        case 'n': return vnull(data, dlen, i+1);
        case '-': case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
            return vnumber(data, dlen, i+1);
        }
        break;
    }
    return -(i+1);
}

static int64_t vpayload(const uint8_t *data, int64_t dlen, int64_t i) {
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        default:
            if ((i = vany(data, dlen, i, 1)) < 0) return i;
            for (; i < dlen; i++) {
                switch (data[i]) {
                case ' ': case '\t': case '\n': case '\r': continue;
                default: return -(i+1);
                }
            }
            return i;
        }
    }
    return -(i+1);
}

JSON_EXTERN
struct json_valid json_validn_ex(const char *json_str, size_t len, int opts) {
    (void)opts; // for future use
    int64_t ilen = len;
    if (ilen < 0) return (struct json_valid) { 0 };
    int64_t pos = vpayload((uint8_t*)json_str, len, 0);
    if (pos > 0) return (struct json_valid) { .valid = true };
    return (struct json_valid) { .pos = (-pos)-1 };
}

JSON_EXTERN struct json_valid json_valid_ex(const char *json_str, int opts) {
    return json_validn_ex(json_str, json_str?strlen(json_str):0, opts);
}

JSON_EXTERN bool json_validn(const char *json_str, size_t len) {
    return json_validn_ex(json_str, len, 0).valid;
}

JSON_EXTERN bool json_valid(const char *json_str) {
    return json_validn(json_str, json_str?strlen(json_str):0);
}

// don't changes these flags without changing the numtoks table too.
enum iflags { IESC = 1, IDOT = 2, ISCI = 4, ISIGN = 8 };

#define jmake(info, raw, end, len) ((struct json) { .priv = { \
    (void*)(uintptr_t)(info), (void*)(uintptr_t)(raw), \
    (void*)(uintptr_t)(end), (void*)(uintptr_t)(len) } })
#define jinfo(json) ((int)(uintptr_t)((json).priv[0]))
#define jraw(json) ((uint8_t*)(uintptr_t)((json).priv[1]))
#define jend(json) ((uint8_t*)(uintptr_t)((json).priv[2]))
#define jlen(json) ((size_t)(uintptr_t)((json).priv[3]))

static const uint8_t strtoksa[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
};

static inline size_t count_string(uint8_t *raw, uint8_t *end, int *infoout) {
    size_t len = end-raw;
    size_t i = 1;
    int info = 0;
    bool e = false;
    while (1) {
        for8(i, len, {
            if (strtoksa[raw[i]]) goto tok;
            e = false;
        });
        break;
    tok:
        if (raw[i] == '"') {
            i++;
            if (!e) {
                break;
            }
            e = false;
            continue;
        }
        if (raw[i] == '\\') {
            info |= IESC;
            e = !e;
        }
        i++;
    }
    *infoout = info;
    return i;
}

static struct json take_string(uint8_t *raw, uint8_t *end) {
    int info = 0;
    size_t i = count_string(raw, end, &info);
    return jmake(info, raw, end, i);
}

static const uint8_t numtoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,1,0,1,3,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // don't changes these flags without changing enum iflags too.
};

static struct json take_number(uint8_t *raw, uint8_t *end) {
    int64_t len = end-raw;
    int info = raw[0] == '-' ? ISIGN : 0;
    int64_t i = 1;
    for16(i, len, {
        if (!numtoks[raw[i]]) goto done;
        info |= (numtoks[raw[i]]-1);
    });
done:
    return jmake(info, raw, end, i);
}

static const uint8_t nesttoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,0,
};

static size_t count_nested(uint8_t *raw, uint8_t *end) {
    size_t len = end-raw;
    size_t i = 1;
    int depth = 1;
    int kind = 0;
    if (i >= len) return i;
    while (depth) {
        for16(i, len, { if (nesttoks[raw[i]]) goto tok0; });
        break;
    tok0:
        kind = nesttoks[raw[i]];
        i++;
        if (kind-1) {
            depth += kind-3;
        } else {
            while (1) {
                for16(i, len, { if (raw[i]=='"') goto tok1; });
                break;
            tok1:
                i++;
                if (raw[i-2] == '\\') {
                    size_t j = i-3;
                    size_t e = 1;
                    while (j > 0 && raw[j] == '\\') {
                        e = (e+1)&1;
                        j--;
                    }
                    if (e) continue;
                }
                break;
            }
        }
    }
    return i;
}

static struct json take_literal(uint8_t *raw, uint8_t *end, size_t litlen) {
    size_t rlen = end-raw;
    return jmake(0, raw, end, rlen < litlen ? rlen : litlen);
}

static struct json peek_any(uint8_t *raw, uint8_t *end) {
    while (raw < end) {
        switch (*raw){
        case '}': case ']': return (struct json){ 0 };
        case '{': case '[': return jmake(0, raw, end, 0);
        case '"': return take_string(raw, end);
        case 'n': return take_literal(raw, end, 4);
        case 't': return take_literal(raw, end, 4);
        case 'f': return take_literal(raw, end, 5);
        case '-': case '0': case '1': case '2': case '3': case '4': case '5': 
        case '6': case '7': case '8': case '9': return take_number(raw, end);
        }
        raw++;
    }
    return (struct json){ 0 };
}

JSON_EXTERN struct json json_first(struct json json) {
    uint8_t *raw = jraw(json);
    uint8_t *end = jend(json);
    if (end <= raw || (*raw != '{' &&  *raw != '[')) return (struct json){0};
    return peek_any(raw+1, end);
}

JSON_EXTERN struct json json_next(struct json json) {
    uint8_t *raw = jraw(json);
    uint8_t *end = jend(json);
    if (end <= raw) return (struct json){ 0 };
    raw += jlen(json) == 0 ? count_nested(raw, end): jlen(json);
    return peek_any(raw, end);
}

JSON_EXTERN struct json json_parsen(const char *json_str, size_t len) {
    if (len > 0 && (json_str[0] == '[' || json_str[0] == '{')) {
        return jmake(0, json_str, json_str+len, 0);
    }
    if (len == 0) return (struct json){ 0 };
    return peek_any((uint8_t*)json_str, (uint8_t*)json_str+len);
}

JSON_EXTERN struct json json_parse(const char *json_str) {
    return json_parsen(json_str, json_str?strlen(json_str):0);
}

JSON_EXTERN bool json_exists(struct json json) {
    return jraw(json) && jend(json);
}

JSON_EXTERN const char *json_raw(struct json json) {
    return (char*)jraw(json);
}

JSON_EXTERN size_t json_raw_length(struct json json) {
    if (jlen(json)) return jlen(json);
    if (jraw(json) < jend(json)) return count_nested(jraw(json), jend(json));
    return 0;
}

static const uint8_t typetoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,3,0,0,0,0,0,0,0,0,0,0,2,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,6,0,0,0,0,
};

JSON_EXTERN enum json_type json_type(struct json json) {
    return jraw(json) < jend(json) ? typetoks[*jraw(json)] : JSON_NULL;
}

JSON_EXTERN struct json json_ensure(struct json json) {
    return jmake(jinfo(json), jraw(json), jend(json), json_raw_length(json));
}

static int strcmpn(const char *a, size_t alen, const char *b, size_t blen) {
    size_t n = alen < blen ? alen : blen;
    int cmp = strncmp(a, b, n);
    if (cmp == 0) {
        cmp = alen < blen ? -1 : alen > blen ? 1 : 0;
    }
    return cmp;
}

static const uint8_t hextoks[256] = { 
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static uint32_t decode_hex(const uint8_t *str) {
    return (((int)hextoks[str[0]])<<12) | (((int)hextoks[str[1]])<<8) |
           (((int)hextoks[str[2]])<<4) | (((int)hextoks[str[3]])<<0);
}

static bool is_surrogate(uint32_t cp) {
    return cp > 55296 && cp < 57344;
}

static uint32_t decode_codepoint(uint32_t cp1, uint32_t cp2) {
    return cp1 > 55296  && cp1 < 56320 && cp2 > 56320 && cp2 < 57344 ?
        ((cp1 - 55296) << 10) | ((cp2 - 56320) + 65536) :
        65533;
}

static inline int encode_codepoint(uint8_t dst[], uint32_t cp) {
    if (cp < 128) {
        dst[0] = cp;
        return 1;
    } else if (cp < 2048) {
        dst[0] = 192 | (cp >> 6);
        dst[1] = 128 | (cp & 63);
        return 2;
    } else if (cp > 1114111 || is_surrogate(cp)) {
        cp = 65533; // error codepoint
    }
    if (cp < 65536) {
        dst[0] = 224 | (cp >> 12);
        dst[1] = 128 | ((cp >> 6) & 63);
        dst[2] = 128 | (cp & 63);
        return 3;
    }
    dst[0] = 240 | (cp >> 18);
    dst[1] = 128 | ((cp >> 12) & 63);
    dst[2] = 128 | ((cp >> 6) & 63);
    dst[3] = 128 | (cp & 63);
    return 4;
}

// for_each_utf8 iterates over each UTF-8 bytes in jstr, unescaping along the
// way. 'f' is a loop expression that will make available the 'ch' char which 
// is just a single byte in a UTF-8 series.
#define for_each_utf8(jstr, len, f) { \
    size_t nn = (len); \
    int ch = 0; \
    (void)ch; \
    for (size_t ii = 0; ii < nn; ii++) { \
        if ((jstr)[ii] != '\\') { \
            ch = (jstr)[ii]; \
            if (1) f \
            continue; \
        }; \
        ii++; \
        if (ii == nn) break; \
        switch  ((jstr)[ii]) { \
        case '\\': ch = '\\'; break; \
        case '/' : ch = '/';  break; \
        case 'b' : ch = '\b'; break; \
        case 'f' : ch = '\f'; break; \
        case 'n' : ch = '\n'; break; \
        case 'r' : ch = '\r'; break; \
        case 't' : ch = '\t'; break; \
        case '"' : ch = '"';  break; \
        case 'u' : \
            if (ii+5 > nn) { nn = 0; continue; }; \
            uint32_t cp = decode_hex((jstr)+ii+1); \
            ii += 5; \
            if (is_surrogate(cp)) { \
                if (nn-ii >= 6 && (jstr)[ii] == '\\' && (jstr)[ii+1] == 'u') { \
                    cp = decode_codepoint(cp, decode_hex((jstr)+ii+2)); \
                    ii += 6; \
                } \
            } \
            uint8_t _bytes[4]; \
            int _n = encode_codepoint(_bytes, cp); \
            for (int _j = 0; _j < _n; _j++) { \
                ch = _bytes[_j]; \
                if (1) f \
            } \
            ii--; \
            continue; \
        default: \
            continue; \
        }; \
        if (1) f \
    } \
}

JSON_EXTERN 
int json_raw_comparen(struct json json, const char *str, size_t len) {
    char *raw = (char*)jraw(json);
    if (!raw) raw = "";
    size_t rlen = json_raw_length(json);
    return strcmpn(raw, rlen, str, len);
}

JSON_EXTERN int json_raw_compare(struct json json, const char *str) {
    return json_raw_comparen(json, str, strlen(str));
}

JSON_EXTERN size_t json_string_length(struct json json) {
    size_t len = json_raw_length(json);
    if (json_type(json) != JSON_STRING) {
        return len;
    }
    len = len < 2 ? 0 : len - 2;
    if ((jinfo(json)&IESC) != IESC) {
        return len;
    }
    uint8_t *raw = jraw(json)+1;
    size_t count = 0;
    for_each_utf8(raw, len, { count++; });
    return count;
}

JSON_EXTERN 
int json_string_comparen(struct json json, const char *str, size_t slen) {
    if (json_type(json) != JSON_STRING) {
        return json_raw_comparen(json, str, slen);
    }
    uint8_t *raw = jraw(json);
    size_t rlen = json_raw_length(json);
    raw++;
    rlen = rlen < 2 ? 0 : rlen - 2;
    if ((jinfo(json)&IESC) != IESC) {
        return strcmpn((char*)raw, rlen, str, slen);
    }
    int cmp = 0;
    uint8_t *sp = (uint8_t*)(str ? str : "");
    for_each_utf8(raw, rlen, {
        if (!*sp || ch > *sp) {
            cmp = 1;
            goto done;
        } else if (ch < *sp) {
            cmp = -1;
            goto done;
        }
        sp++;
    });
done:
    if (cmp == 0 && *sp) cmp = -1;
    return cmp;
}

JSON_EXTERN 
int json_string_compare(struct json json, const char *str) {
    return json_string_comparen(json, str, str?strlen(str):0);
}

JSON_EXTERN size_t json_string_copy(struct json json, char *str, size_t n) {
    size_t len = json_raw_length(json);
    uint8_t *raw = jraw(json);
    bool isjsonstr = json_type(json) == JSON_STRING;
    bool isesc = false;
    if (isjsonstr) {
        raw++;
        len = len < 2 ? 0 : len - 2;
        isesc = (jinfo(json)&IESC) == IESC;
    }
    if (!isesc) {
        if (n == 0) return len;
        n = n-1 < len ? n-1 : len;
        memcpy(str, raw, n);
        str[n] = '\0';
        return len;
    }
    size_t count = 0;
    for_each_utf8(raw, len, {
        if (count < n) str[count] = ch;
        count++;
    });
    if (n > count) str[count] = '\0';
    else if (n > 0) str[n-1] = '\0';
    return count;
}

JSON_EXTERN size_t json_array_count(struct json json) {
    size_t count = 0;
    if (json_type(json) == JSON_ARRAY) {
        json = json_first(json);
        while (json_exists(json)) {
            count++;
            json = json_next(json);
        }
    }
    return count;
}

JSON_EXTERN struct json json_array_get(struct json json, size_t index) {
    if (json_type(json) == JSON_ARRAY) {
        json = json_first(json);
        while (json_exists(json)) {
            if (index == 0) return json;
            json = json_next(json);
            index--;
        }
    }
    return (struct json) { 0 };
}

JSON_EXTERN
struct json json_object_getn(struct json json, const char *key, size_t len) {
    if (json_type(json) == JSON_OBJECT) {
        json = json_first(json);
        while (json_exists(json)) {
            if (json_string_comparen(json, key, len) == 0) {
                return json_next(json);
            }
            json = json_next(json_next(json));
        }
    }
    return (struct json) { 0 };
}

JSON_EXTERN struct json json_object_get(struct json json, const char *key) {
    return json_object_getn(json, key, key?strlen(key):0);
}

static double stod(const uint8_t *str, size_t len, char *buf) {
    memcpy(buf, str, len);
    buf[len] = '\0';
    char *ptr;
    double x = strtod(buf, &ptr);
    return (size_t)(ptr-buf) == len ? x : 0;
}

static double parse_double_big(const uint8_t *str, size_t len) {
    char buf[512];
    if (len >= sizeof(buf)) return 0;
    return stod(str, len, buf);
}

static double parse_double(const uint8_t *str, size_t len) {
    char buf[32];
    if (len >= sizeof(buf)) return parse_double_big(str, len);
    return stod(str, len, buf);
}

static int64_t parse_int64(const uint8_t *s, size_t len) {
    char buf[21];
    double y;
    if (len == 0) return 0;
    if (len < sizeof(buf) && sizeof(long long) == sizeof(int64_t)) {
        memcpy(buf, s, len);
        buf[len] = '\0';
        char *ptr = NULL;
        int64_t x = strtoll(buf, &ptr, 10);
        if ((size_t)(ptr-buf) == len) return x;
        y = strtod(buf, &ptr);
        if ((size_t)(ptr-buf) == len) goto clamp;
    }
    y = parse_double(s, len);
clamp:
    if (y < (double)INT64_MIN) return INT64_MIN;
    if (y > (double)INT64_MAX) return INT64_MAX;
    return y;
}

static uint64_t parse_uint64(const uint8_t *s, size_t len) {
    char buf[21];
    double y;
    if (len == 0) return 0;
    if (len < sizeof(buf) && sizeof(long long) == sizeof(uint64_t) &&
        s[0] != '-')
    {
        memcpy(buf, s, len);
        buf[len] = '\0';
        char *ptr = NULL;
        uint64_t x = strtoull(buf, &ptr, 10);
        if ((size_t)(ptr-buf) == len) return x;
        y = strtod(buf, &ptr);
        if ((size_t)(ptr-buf) == len) goto clamp;
    }
    y = parse_double(s, len);
clamp:
    if (y < 0) return 0;
    if (y > (double)UINT64_MAX) return UINT64_MAX;
    return y;
}

JSON_EXTERN double json_double(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 3) return 0.0;
        return parse_double(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_double(jraw(json), jlen(json));
    default:
        return 0.0;
    }
}

JSON_EXTERN int64_t json_int64(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 2) return 0;
        return parse_int64(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_int64(jraw(json), jlen(json));
    default:
        return 0;
    }
}

JSON_EXTERN int json_int(struct json json) {
    int64_t x = json_int64(json);
    if (x < (int64_t)INT_MIN) return INT_MIN;
    if (x > (int64_t)INT_MAX) return INT_MAX;
    return x;
}

JSON_EXTERN uint64_t json_uint64(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 2) return 0;
        return parse_uint64(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_uint64(jraw(json), jlen(json));
    default:
        return 0;
    }
}

JSON_EXTERN bool json_bool(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return true;
    case JSON_NUMBER:
         return json_double(json) != 0.0; 
    case JSON_STRING: {
        char *trues[] = { "1", "t", "T", "true", "TRUE", "True" };
        for (size_t i = 0; i < sizeof(trues)/sizeof(char*); i++) {
            if (json_string_compare(json, trues[i]) == 0) return true;
        }
        return false;
    }
    default:
        return false;
    }
}

struct jesc_buf { 
    uint8_t *esc;
    size_t esclen;
    size_t count;
};

static void jesc_append(struct jesc_buf *buf, uint8_t ch) {
    if (buf->esclen > 1) { 
        *(buf->esc++) = ch;
        buf->esclen--; 
    }
    buf->count++;
}
static void jesc_append2(struct jesc_buf *buf, uint8_t c1, uint8_t c2) {
    jesc_append(buf, c1);
    jesc_append(buf, c2);
}

static const uint8_t hexchars[] = "0123456789abcdef";

static void 
jesc_append_ux(struct jesc_buf *buf, uint8_t c1, uint8_t c2, uint16_t x) {
    jesc_append2(buf, c1, c2);
    jesc_append2(buf, hexchars[x>>12&0xF], hexchars[x>>8&0xF]);
    jesc_append2(buf, hexchars[x>>4&0xF], hexchars[x>>0&0xF]);
}

JSON_EXTERN
size_t json_escapen(const char *str, size_t len, char *esc, size_t n) {
    uint8_t cpbuf[4];
    struct jesc_buf buf  = { .esc = (uint8_t*)esc, .esclen = n };
    jesc_append(&buf, '"');
    for (size_t i = 0; i < len; i++) {
        uint32_t c = (uint8_t)str[i];
        if (c < ' ') {
            switch (c) {
            case '\n': jesc_append2(&buf, '\\', 'n'); break;
            case '\b': jesc_append2(&buf, '\\', 'b'); break;
            case '\f': jesc_append2(&buf, '\\', 'f'); break;
            case '\r': jesc_append2(&buf, '\\', 'r'); break;
            case '\t': jesc_append2(&buf, '\\', 't'); break;
            default: jesc_append_ux(&buf, '\\', 'u', c);
            }
        } else if (c == '>' || c == '<' || c == '&') {
            // make web safe
            jesc_append_ux(&buf, '\\', 'u', c);
        } else if (c == '\\') {
            jesc_append2(&buf, '\\', '\\');
        } else if (c == '"') {
            jesc_append2(&buf, '\\', '"');
        } else if (c > 127) {
            struct vutf8res res = vutf8((uint8_t*)(str+i), len-i);
            if (res.n == 0) {
                res.n = 1;
                res.cp = 0xfffd;
            }
            int cpn = encode_codepoint(cpbuf, res.cp);
            for (int j = 0; j < cpn; j++) {
                jesc_append(&buf, cpbuf[j]);
            }
            i = i + res.n - 1;
        } else {
            jesc_append(&buf, str[i]);
        }
    }
    jesc_append(&buf, '"');
    if (buf.esclen > 0) {
        // add to null terminator
        *(buf.esc++) = '\0';
        buf.esclen--;
    }
    return buf.count;
}

JSON_EXTERN size_t json_escape(const char *str, char *esc, size_t n) {
    return json_escapen(str, str?strlen(str):0, esc, n);
}

JSON_EXTERN
struct json json_getn(const char *json_str, size_t len, const char *path) {
    if (!path) return (struct json) { 0 };
    struct json json = json_parsen(json_str, len);
    int i = 0;
    bool end = false;
    char *p = (char*)path;
    for (; !end && json_exists(json); i++) {
        // get the next component
        const char *key = p;
        while (*p && *p != '.') p++;
        size_t klen = p-key;
        if (*p == '.') p++;
        else if (!*p) end = true;
        enum json_type type = json_type(json);
        if (type == JSON_OBJECT) {
            json = json_object_getn(json, key, klen);
        } else if (type == JSON_ARRAY) {
            if (klen == 0) { i = 0; break; }
            char *end;
            size_t index = strtol(key, &end, 10);
            if (*end && *end != '.') { i = 0; break; }
            json = json_array_get(json, index);
        } else {
            i = 0; 
            break;
        }
    }
    return i == 0 ? (struct json) { 0 } : json;
}

JSON_EXTERN struct json json_get(const char *json_str, const char *path) {
    return json_getn(json_str, json_str?strlen(json_str):0, path);
}

JSON_EXTERN bool json_string_is_escaped(struct json json) {
    return (jinfo(json)&IESC) == IESC;
}
// END json.c

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif

static struct tg_geom *make_parse_error(const char *format, ...) {
    char *error = NULL;
    va_list args;
    va_start(args, format);
    int n = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (n >= 0) {
        error = tg_malloc(n+1);
        if (error) {
            va_start(args, format);
            vsnprintf(error, n+1, format, args);
            va_end(args);
        }
    }
    if (!error) return NULL;
    struct tg_geom *geom = geom_new_empty(TG_POINT);
    if (!geom) {
        tg_free(error);
        return NULL;
    }
    geom->head.flags |= IS_ERROR;
    geom->error = error;
    return geom;
}

/// Return a parsing error.
///
/// Parsing functions, such as tg_parse_geojson(), may fail due to invalid
/// input data.
///
/// It's important to **always** check for errors after parsing.
///
/// **Example**
///
/// ```
/// struct tg_geom *geom = tg_parse_geojson(input);
/// if (tg_geom_error(geom)) {
///     // The parsing failed due to invalid input or out of memory.
///
///     // Get the error message.
///     const char *err = tg_geom_error(geom);
///
///     // Do something with the error, such as log it.
///     printf("[err] %s\n", err);
///
///     // Make sure to free the error geom and it's resources.
///     tg_geom_free(geom);
///
///     // !!
///     // DO NOT use the return value of tg_geom_error() after calling 
///     // tg_geom_free(). If you need to hang onto the error for a longer
///     // period of time then you must copy it before freeing.
///     // !!
///
///     return;
/// } else {
///     // ... Parsing succeeded 
/// }
/// ```
///
/// @return A string describing the error
/// @return NULL if there was no error
/// @see tg_geom_free()
/// @see tg_parse_geojson()
/// @see tg_parse_wkt()
/// @see tg_parse_wkb()
/// @see tg_parse_hex()
/// @see GeometryParsing
const char *tg_geom_error(const struct tg_geom *geom) {
    if (!geom) return "no memory";
    return (geom->head.flags&IS_ERROR) == IS_ERROR ? geom->error : NULL;
}

static bool buf_append_json_pair(struct buf *buf, struct json key, 
    struct json val)
{
    size_t len = buf->len;
    if (!buf_append_byte(buf, buf->len == 0 ? '{' : ',') ||
        !buf_append_bytes(buf, (uint8_t*)json_raw(key), json_raw_length(key)) || 
        !buf_append_byte(buf, ':') || 
        !buf_append_bytes(buf, (uint8_t*)json_raw(val), json_raw_length(val)))
    {
        buf->len = len;
        return false;
    }
    return true;
}

// returns an error message constant if there's an error. Do not free.
static const char *take_basic_geojson(struct json json, 
    const char *target_name, struct json *targetout, 
    enum flags *flagsout, char **extraout, bool *okout
) {
    enum flags flags = 0;
    bool is_feat = strcmp(target_name, "geometry") == 0;
    const char *err = NULL;
    bool ok = false;
    bool has_props = false;
    bool has_id = false;
    struct buf extra = { 0 };
    struct json target = { 0 };
    struct json key = json_first(json);
    struct json val = json_next(key);
    while (json_exists(key)) {
        if (json_string_compare(key, "type") == 0) {
            // .. do nothing, the caller should already know its type ...
        } else if (json_string_compare(key, target_name) == 0) {
            val = json_ensure(val);
            target = val;
        } else {
            bool skip_val = false;
            if (is_feat) {
                if (json_string_compare(key, "properties") == 0) {
                    if (!has_props) {
                        if (json_type(val) == JSON_NULL) {
                            flags |= HAS_NULL_PROPS;
                            skip_val = true;
                        } else if (json_type(val) == JSON_OBJECT) {
                            skip_val = !json_exists(json_first(val));
                        } else {
                            err = "'properties' must be an object or null";
                            goto fail;
                        }
                    } else {
                        skip_val = true;
                    }
                    has_props = true;
                } else if (json_string_compare(key, "id") == 0) {
                    if (has_id) {
                        skip_val = true;
                    } else {
                        if (json_type(val) != JSON_STRING && 
                            json_type(val) != JSON_NUMBER)
                        {
                            err = "'id' must be a string or number";
                            goto fail;
                        }
                    }
                    has_id = true;
                }
            }
            if (!skip_val) {
                if (!buf_append_json_pair(&extra, key, val)) goto fail;
            }
        }
        key = json_next(val);
        val = json_next(key);
    }
    if (!json_exists(target)) {
        if (strcmp(target_name, "geometry") == 0) {
            err = "missing 'geometry'";
        } else if (strcmp(target_name, "geometries") == 0) {
            err = "missing 'geometries'";
        } else if (strcmp(target_name, "features") == 0) {
            err = "missing 'features'";
        } else { // "coordinates"
            err = "missing 'coordinates'";
        }
        goto fail;
    }
    enum json_type ttype = json_type(target);
    if (is_feat) {
        if (ttype != JSON_OBJECT) {
            if (ttype == JSON_NULL) {
                // unlocated
                // https://www.rfc-editor.org/rfc/rfc7946#section-3.2
                flags |= IS_EMPTY;
                flags |= IS_UNLOCATED;
            } else {
                err = "'geometry' must be an object or null";
                goto fail;
            }
        }
        #ifdef GEOJSON_REQ_PROPERTIES
        if (!has_props) {
            err = "missing 'properties'";
            goto fail;
        }
        #endif
    } else if (ttype != JSON_ARRAY) {
        if (strcmp(target_name, "geometries") == 0) {
            err = "'geometries' must be an array";
        } else if (strcmp(target_name, "features") == 0) {
            err = "'features' must be an array";
        } else { // "coordinates"
            err = "'coordinates' must be an array";
        }
        goto fail;
    } else if (!json_exists(json_first(target))) {
        // null object
        // https://www.rfc-editor.org/rfc/rfc7946#section-3.1
        flags |= IS_EMPTY;
    }
    if (extra.len > 0) {
        if (!buf_append_byte(&extra, '}')) goto fail;
        if (!buf_append_byte(&extra, '\0')) goto fail;
        if (!buf_trunc(&extra)) goto fail;
    }
    ok = true;
fail:
    if (!ok) {
        if (extra.data) tg_free(extra.data);
        *extraout = NULL;
    } else {
        *extraout = (char*)extra.data;
    }
    *targetout = target;
    *okout = ok;
    *flagsout = flags;
    return err;
}

#define def_vec(name, type, append_name, start_cap) \
name { \
    type *data; \
    size_t len; \
    size_t cap; \
}; \
static bool append_name(name *vec, type f) { \
    if (vec->len == vec->cap) { \
        size_t cap = vec->cap; \
        cap = grow_cap(cap, start_cap); \
        type *data = tg_realloc(vec->data, cap*sizeof(type)); \
        if (!data) return false; \
        vec->data = data; \
        vec->cap = cap; \
    } \
    vec->data[vec->len++] = f; \
    return true; \
} \

// some vectors are better than no vectors, i guess.
def_vec(struct dvec, double,          dvec_append, 8)
def_vec(struct rvec, struct tg_ring*, rvec_append, 1)
def_vec(struct lvec, struct tg_line*, lvec_append, 1)
def_vec(struct pvec, struct tg_poly*, pvec_append, 1)
def_vec(struct gvec, struct tg_geom*, gvec_append, 1)

#define PARSE_GEOJSON_BASIC_HEAD(target_name) \
    struct tg_geom *gerr = NULL; \
    struct tg_geom *geom = NULL; \
    struct json target; \
    enum flags flags; \
    char *extra; \
    bool ok; \
    const char *err_ = take_basic_geojson(json, target_name, \
        &target, &flags, &extra, &ok); \
    if (!ok) { \
        gerr = err_ ? make_parse_error("%s", err_) : NULL; \
        goto fail; \
    }

#define PARSE_GEOJSON_BASIC_TAIL(cleanup) \
    goto done; \
done: \
    if (!geom) goto fail; \
    geom->head.flags |= flags; \
    if (extra) geom->xjson = extra; \
    cleanup; \
    return geom; \
fail: \
    tg_geom_free(geom); \
    if (extra) tg_free(extra); \
    cleanup; \
    return gerr;

static const char *err_for_geojson_depth(int depth) {
    if (depth == 1) {
        return "'coordinates' must be an array of positions";
    } else if (depth == 2) {
        return "'coordinates' must be a two deep nested array of positions";
    } else {
        return "'coordinates' must be a three deep nested array of positions";
    }
}

static struct tg_geom *parse_geojson_point(struct json json, bool req_geom,
    enum tg_index ix)
{
    (void)ix;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    if ((flags&IS_EMPTY) == IS_EMPTY) {
        geom = tg_geom_new_point_empty();
        if (!geom) goto fail;
        goto done;
    }
    double posn[4];
    int dims = 0;
    struct json val = json_first(target);
    while (json_exists(val)) {
        if (json_type(val) != JSON_NUMBER) {
            gerr = make_parse_error("'coordinates' must only contain numbers");
            goto fail;
        }
        if (dims < 4) {
            // i don't care about more than 4 dimensions
            posn[dims] = json_double(val);
            dims++;
        }
        val = json_next(val);
    }
    if (dims < 2) {
        gerr = make_parse_error("'coordinates' must have two or more numbers");
        goto fail;
    }
    struct tg_point xy = { posn[0], posn[1] };
    double z = posn[2];
    double m = posn[3];
    if (!req_geom && !extra && dims == 2) {
        geom = tg_geom_new_point(xy);
    } else {
        switch (dims) {
        case 2: 
            geom = geom_new(TG_POINT);
            if (!geom) goto fail;
            geom->point = xy;
            break;
        case 3: 
            geom = tg_geom_new_point_z(xy, z);
            break;
        default: 
            geom = tg_geom_new_point_zm(xy, z, m);
            break;
        }
    }
    PARSE_GEOJSON_BASIC_TAIL()
}

static bool check_parse_posns(enum base base, double *posns, int nposns,
    const char **err)
{
    // nposns must be an even number.
    const struct tg_point *points = (void*)posns;
    int npoints = nposns / 2;
    if (base == BASE_LINE) {
        if (npoints < 2) {
            *err = "lines must have two or more positions";
            return false;
        }
    } else if (base == BASE_RING) {
        if (npoints < 3) {
            *err = "rings must have three or more positions";
            return false;
        }
        struct tg_point p0 = points[0];
        struct tg_point p1 = points[npoints-1];
        if (!(p0.x == p1.x && p0.y == p1.y)) {
            *err = "rings must have matching first and last positions";
            return false;
        }
    }
    return true;
}

// return the dims or -1 if error
static int parse_geojson_posns(enum base base, int dims, int depth, 
    struct json coords, struct dvec *posns, struct dvec *xcoords, 
    const char **err)
{
    struct json val0 = json_first(coords);
    while (json_exists(val0)) {
        if (json_type(val0) != JSON_ARRAY) {
            *err = err_for_geojson_depth(depth);
            return -1;
        }
        struct json val1 = json_first(val0);
        double posn[4];
        int pdims = 0;
        while (json_exists(val1)) {
            if (json_type(val1) != JSON_NUMBER) {
                *err = "each element in a position must be a number";
                return -1;
            }
            if (pdims < 4) {
                // we don't care more that 4
                posn[pdims] = json_double(val1);
                pdims++;
            }
            val1 = json_next(val1);
        }
        if (dims == 0) {
            dims = pdims;
        }
        if (pdims < 2) {
            *err = "each position must have two or more numbers";
            return -1;
        } else if (pdims != dims) {
            *err = "each position must have the same number of dimensions";
            return -1;
        }
        if (!dvec_append(posns, posn[0]) || !dvec_append(posns, posn[1])) {
            return -1;
        }
        for (int i = 2; i < dims; i++) {
            if (i >= pdims || !dvec_append(xcoords, posn[i])) return -1;
            // if (!dvec_append(xcoords, i < pdims ? posn[i] : 0)) return -1;
        }
        val0 = json_next(val0);
    }
    if (!check_parse_posns(base, posns->data, posns->len, err)) return -1;
    return dims;
}

static struct tg_geom *parse_geojson_linestring(struct json json, 
    bool req_geom, enum tg_index ix)
{
    struct tg_line *line = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    const char *err = NULL;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    if ((flags&IS_EMPTY) == IS_EMPTY) {
        geom = tg_geom_new_linestring_empty();
        goto done;
    }
    int dims = parse_geojson_posns(BASE_LINE, 0, 1, target, &posns, &xcoords,
        &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    line = tg_line_new_ix((struct tg_point*)posns.data, posns.len / 2, ix);
    if (!line) goto fail;
    if (!req_geom && !extra && dims == 2) {
        geom = tg_geom_new_linestring(line);
    } else {
        switch (dims) {
        case 2: 
            geom = geom_new(TG_LINESTRING);
            if (geom) geom->line = tg_line_clone(line);
            break;
        case 3: 
            geom = tg_geom_new_linestring_z(line, xcoords.data, xcoords.len);
            break;
        default: 
            geom = tg_geom_new_linestring_zm(line, xcoords.data, xcoords.len);
            break;
        }
    }
    PARSE_GEOJSON_BASIC_TAIL({
        if (posns.data) tg_free(posns.data);
        if (xcoords.data) tg_free(xcoords.data);
        if (line) tg_line_free(line);
    })
}

// returns the dims or -1 if error
static int parse_geojson_multi_posns(enum base base, int dims, int depth, 
    struct json coords, struct dvec *posns, struct rvec *rings,  
    struct tg_poly **poly, struct dvec *xcoords, enum tg_index ix,
    const char **err)
{
    struct json val0 = json_first(coords);
    while (json_exists(val0)) {
        if (json_type(val0) != JSON_ARRAY) {
            *err = "'coordinates' must be a nested array";
            return -1;
        }
        posns->len = 0;
        dims = parse_geojson_posns(base, dims, depth, val0, posns, 
            xcoords, err);
        if (dims == -1) return -1;
        struct tg_ring *ring = tg_ring_new_ix((struct tg_point*)posns->data, 
            posns->len / 2, ix);
        if (!ring) return -1;
        if (!rvec_append(rings, ring)) {
            tg_ring_free(ring);
            return -1;
        }
        val0 = json_next(val0);
    }
    if (rings->len == 0) {
        *err = "polygons must have one or more rings";
        return -1;
    }
    *poly = tg_poly_new(rings->data[0], 
        (struct tg_ring const*const*)rings->data+1, rings->len-1);
    if (!*poly) return -1;
    for (size_t i = 0; i < rings->len; i++) {
        tg_ring_free(rings->data[i]);
    }
    rings->len = 0;
    return dims;
}

static struct tg_geom *parse_geojson_polygon(struct json json, bool req_geom,
    enum tg_index ix)
{
    struct tg_poly *poly = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct rvec rings = { 0 };
    const char *err = NULL;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    if ((flags&IS_EMPTY) == IS_EMPTY) {
        geom = tg_geom_new_polygon_empty();
        goto done;
    }
    int dims = parse_geojson_multi_posns(BASE_RING, 0, 2, target, &posns, 
        &rings, &poly, &xcoords, ix, &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    if (!req_geom && !extra && dims == 2) {
        geom = tg_geom_new_polygon(poly);
    } else {
        switch (dims) {
        case 2: 
            geom = geom_new(TG_POLYGON);
            if (geom) geom->poly = tg_poly_clone(poly);
            break;
        case 3: 
            geom = tg_geom_new_polygon_z(poly, xcoords.data, xcoords.len);
            break;
        default: 
            geom = tg_geom_new_polygon_zm(poly, xcoords.data, xcoords.len);
            break;
        }
    }
    PARSE_GEOJSON_BASIC_TAIL({
        if (posns.data) tg_free(posns.data);
        if (xcoords.data) tg_free(xcoords.data);
        if (rings.data) {
            for (size_t i = 0; i < rings.len; i++) {
                tg_ring_free(rings.data[i]);
            }
            tg_free(rings.data);
        }
        if (poly) tg_poly_free(poly);
    })
}

static struct tg_geom *parse_geojson_multipoint(struct json json,
    enum tg_index ix)
{
    (void)ix;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    const char *err = NULL;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    int dims = parse_geojson_posns(BASE_POINT, 0, 1, target, &posns, &xcoords,
        &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    const struct tg_point *points = (struct tg_point*)posns.data;
    int npoints = posns.len/2;
    switch (dims) {
    case 2: 
        geom = tg_geom_new_multipoint(points, npoints);
        break;
    case 3: 
        geom = tg_geom_new_multipoint_z(points, npoints, 
            xcoords.data, xcoords.len);
        break;
    default: 
        geom = tg_geom_new_multipoint_zm(points, npoints, 
            xcoords.data, xcoords.len);
        break;
    }
    PARSE_GEOJSON_BASIC_TAIL({
        if (posns.data) tg_free(posns.data);
        if (xcoords.data) tg_free(xcoords.data);
    })
}

static struct tg_geom *parse_geojson_multilinestring(struct json json,
    enum tg_index ix)
{
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct lvec lines = { 0 };
    const char *err = NULL;
    int dims = 0;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    struct json val0 = json_first(target);
    while (json_exists(val0)) {
        if (json_type(val0) != JSON_ARRAY) {
            gerr = make_parse_error("%s", err_for_geojson_depth(2));
            goto fail;
        }
        posns.len = 0;
        dims = parse_geojson_posns(BASE_LINE, dims, 2, val0, &posns, &xcoords,
            &err);
        if (dims == -1) {
            gerr = err ? make_parse_error("%s", err) : NULL;
            goto fail;
        }
        struct tg_line *line = tg_line_new_ix((struct tg_point*)posns.data, 
            posns.len / 2, ix);
        if (!line) goto fail;
        if (!lvec_append(&lines, line)) {
            tg_line_free(line);
            goto fail;
        }
        val0 = json_next(val0);
    }
    switch (dims) {
    case 2: 
        geom = tg_geom_new_multilinestring(
                (struct tg_line const*const*)lines.data, lines.len);
        break;
    case 3: 
        geom = tg_geom_new_multilinestring_z(
            (struct tg_line const*const*)lines.data, lines.len,
            xcoords.data, xcoords.len);
        break;
    default: 
        geom = tg_geom_new_multilinestring_zm(
            (struct tg_line const*const*)lines.data, lines.len,
            xcoords.data, xcoords.len);
        break;
    }
    PARSE_GEOJSON_BASIC_TAIL({
        if (posns.data) tg_free(posns.data);
        if (xcoords.data) tg_free(xcoords.data);
        if (lines.data) {
            for (size_t i = 0; i < lines.len; i++) {
                tg_line_free(lines.data[i]);
            }
            tg_free(lines.data);
        }
    })
}

static struct tg_geom *parse_geojson_multipolygon(struct json json,
    enum tg_index ix)
{
    struct tg_poly *poly = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct rvec rings = { 0 };
    struct pvec polys = { 0 };
    const char *err = NULL;
    int dims = 0;
    PARSE_GEOJSON_BASIC_HEAD("coordinates")
    struct json val0 = json_first(target);
    while (json_exists(val0)) {
        if (json_type(val0) != JSON_ARRAY) {
            gerr = make_parse_error("%s", err_for_geojson_depth(3));
            goto fail;
        }
        posns.len = 0;
        rings.len = 0;
        dims = parse_geojson_multi_posns(BASE_RING, dims, 3, val0, &posns, 
            &rings, &poly, &xcoords, ix, &err);
        if (dims == -1) {
            gerr = err ? make_parse_error("%s", err) : NULL;
            goto fail;
        }
        if (!pvec_append(&polys, poly)) {
            tg_poly_free(poly);
            goto fail;
        }
        val0 = json_next(val0);
    }
    switch (dims) {
    case 2: 
        geom = tg_geom_new_multipolygon(
                (struct tg_poly const*const*)polys.data, polys.len);
        break;
    case 3: 
        geom = tg_geom_new_multipolygon_z(
            (struct tg_poly const*const*)polys.data, polys.len,
            xcoords.data, xcoords.len);
        break;
    default: 
        geom = tg_geom_new_multipolygon_zm(
            (struct tg_poly const*const*)polys.data, polys.len,
            xcoords.data, xcoords.len);
        break;
    }
    PARSE_GEOJSON_BASIC_TAIL({
        if (posns.data) tg_free(posns.data);
        if (xcoords.data) tg_free(xcoords.data);
        if (rings.data) {
            for (size_t i = 0; i < rings.len; i++) {
                tg_ring_free(rings.data[i]);
            }
            tg_free(rings.data);
        }
        if (polys.data) {
            for (size_t i = 0; i < polys.len; i++) {
                tg_poly_free(polys.data[i]);
            }
            tg_free(polys.data);
        }
    })
}

static struct tg_geom *parse_geojson(struct json json, bool req_geom, 
    enum tg_index ix);

static struct tg_geom *parse_geojson_geometrycollection(struct json json,
    enum tg_index ix) 
{
    struct gvec geoms = { 0 };
    PARSE_GEOJSON_BASIC_HEAD("geometries")
    struct json val0 = json_first(target);
    while (json_exists(val0)) {
        struct tg_geom *child = parse_geojson(val0, false, ix);
        if (!child) goto fail;
        if (tg_geom_error(child)) {
            gerr = child; 
            child = NULL;
            goto fail;
        }
        if ((child->head.flags&IS_FEATURE) == IS_FEATURE ||
            (child->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL)
        {
            gerr = make_parse_error("'geometries' must only contain objects "
                    "with the 'type' of Point, LineString, Polygon, "
                    "MultiPoint, MultiLineString, MultiPolygon, or "
                    "GeometryCollection");
            tg_geom_free(child);
            goto fail;
        }
        if (!gvec_append(&geoms, child)) {
            tg_geom_free(child);
            goto fail;
        }
        val0 = json_next(val0);
    }
    geom = tg_geom_new_geometrycollection(
        (struct tg_geom const*const*)geoms.data, geoms.len);
    PARSE_GEOJSON_BASIC_TAIL({
        if (geoms.data) {
            for (size_t i = 0; i < geoms.len; i++) {
                tg_geom_free(geoms.data[i]);
            }
            tg_free(geoms.data);
        }
    })
}

static struct tg_geom *parse_geojson_feature(struct json json, enum tg_index ix)
{
    struct buf combined = { 0 };
    PARSE_GEOJSON_BASIC_HEAD("geometry")
    if ((flags&IS_EMPTY) == IS_EMPTY) {
        geom = tg_geom_new_point_empty();
    } else {
        geom = parse_geojson(target, extra != NULL, ix);
    }
    if (!geom) goto fail;
    if (tg_geom_error(geom)) {
        gerr = geom;
        geom = NULL;
        goto fail;
    }
    if ((geom->head.flags&IS_FEATURE) == IS_FEATURE ||
        (geom->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL)
    {
        gerr = make_parse_error("'geometry' must only contain an object with "
            "the 'type' of Point, LineString, Polygon, MultiPoint, "
            "MultiLineString, MultiPolygon, or GeometryCollection");
        goto fail;
    }
    geom->head.flags |= IS_FEATURE;
    if (geom->head.base == BASE_GEOM && geom->xjson) {
        // combine the two together as '[feature-extra,geometry-extra]'
        size_t xn0 = extra ? strlen(extra) : 0;
        size_t xn1 = strlen(geom->xjson);
        if (!buf_append_byte(&combined, '[') || 
            !buf_append_bytes(&combined, 
                (uint8_t*)(xn0 ? extra : "{}"), (xn0 ? xn0 : 2)) ||
            !buf_append_byte(&combined, ',') || 
            !buf_append_bytes(&combined, (uint8_t*)geom->xjson, xn1) ||
            !buf_append_byte(&combined, ']') ||
            !buf_append_byte(&combined, '\0'))
        { goto fail; }
        if (!buf_trunc(&combined)) goto fail;
        if (geom->xjson) tg_free(geom->xjson);
        geom->xjson = NULL;
        if (extra) tg_free(extra);
        extra = (char*)combined.data;
        combined = (struct buf) { 0 };
    }
        
    PARSE_GEOJSON_BASIC_TAIL({
        if (combined.data) tg_free(combined.data);
    })
}

static struct tg_geom *parse_geojson_featurecollection(struct json json,
    enum tg_index ix)
{
    struct gvec geoms = { 0 };
    PARSE_GEOJSON_BASIC_HEAD("features")
    struct json val0 = json_first(target);
    while (json_exists(val0)) {
        struct tg_geom *child = parse_geojson(val0, false, ix);
        if (!child) goto fail;
        if (tg_geom_error(child)) {
            gerr = child;
            goto fail;
        }
        if ((child->head.flags&IS_FEATURE) != IS_FEATURE) {
            gerr = make_parse_error("'features' must only contain objects "
                "with the 'type' of Feature");
            tg_geom_free(child);
            goto fail;
        }
        if (!gvec_append(&geoms, child)) {
            tg_geom_free(child);
            goto fail;
        }
        val0 = json_next(val0);
    }
    geom = tg_geom_new_geometrycollection(
        (struct tg_geom const*const*)geoms.data, geoms.len);
    if (geom) geom->head.flags |= IS_FEATURE_COL;
    PARSE_GEOJSON_BASIC_TAIL({
        if (geoms.data) {
            for (size_t i = 0; i < geoms.len; i++) {
                tg_geom_free(geoms.data[i]);
            }
            tg_free(geoms.data);
        }
    })
}

static struct tg_geom *parse_geojson(struct json json, bool req_geom,
    enum tg_index ix)
{
    if (json_type(json) != JSON_OBJECT) {
        return make_parse_error("expected an object");
    }
    struct json jtype = json_object_get(json, "type");
    if (!json_exists(jtype)) {
        return make_parse_error("'type' is required");
    }
    char type[24];
    json_string_copy(jtype, type, sizeof(type));
    struct tg_geom *geom = NULL;
    if (strcmp(type, "Point") == 0) {
        geom = parse_geojson_point(json, req_geom, ix);
    } else if (strcmp(type, "LineString") == 0) {
        geom = parse_geojson_linestring(json, req_geom, ix);
    } else if (strcmp(type, "Polygon") == 0) {
        geom = parse_geojson_polygon(json, req_geom, ix);
    } else if (strcmp(type, "MultiPoint") == 0) {
        geom = parse_geojson_multipoint(json, ix);
    } else if (strcmp(type, "MultiLineString") == 0) {
        geom = parse_geojson_multilinestring(json, ix);
    } else if (strcmp(type, "MultiPolygon") == 0) {
        geom = parse_geojson_multipolygon(json, ix);
    } else if (strcmp(type, "GeometryCollection") == 0) {
        geom = parse_geojson_geometrycollection(json, ix);
    } else if (strcmp(type, "Feature") == 0) {
        geom = parse_geojson_feature(json, ix);
    } else if (strcmp(type, "FeatureCollection") == 0) {
        geom = parse_geojson_featurecollection(json, ix);
    } else {
        geom = make_parse_error("unknown type '%s'", type);
    }
    return geom;
}

/// Parse geojson with an included data length.
/// @param geojson Geojson data. Must be UTF8.
/// @param len Length of data
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_geojson()
/// @see GeometryParsing
struct tg_geom *tg_parse_geojsonn(const char *geojson, size_t len) {
    return tg_parse_geojsonn_ix(geojson, len, TG_DEFAULT);
}

/// Parse geojson.
///
/// Supports [GeoJSON](https://datatracker.ietf.org/doc/html/rfc7946) standard,
/// including Features, FeaturesCollection, ZM coordinates, properties, and
/// arbritary JSON members.
/// @param geojson A geojson string. Must be UTF8 and null-terminated.
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_geojsonn()
/// @see tg_parse_geojson_ix()
/// @see tg_parse_geojsonn_ix()
/// @see tg_geom_error()
/// @see tg_geom_geojson()
/// @see GeometryParsing
struct tg_geom *tg_parse_geojson(const char *geojson) {
    return tg_parse_geojsonn_ix(geojson, geojson?strlen(geojson):0, TG_DEFAULT);
}

/// Parse geojson using provided indexing option.
/// @param geojson A geojson string. Must be UTF8 and null-terminated.
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see [tg_index](.#tg_index)
/// @see tg_parse_geojson()
/// @see tg_parse_geojsonn_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_geojson_ix(const char *geojson, enum tg_index ix) {
    return tg_parse_geojsonn_ix(geojson, geojson?strlen(geojson):0, ix);
}

/// Parse geojson using provided indexing option. 
/// @param geojson Geojson data. Must be UTF8.
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see [tg_index](.#tg_index)
/// @see tg_parse_geojson()
/// @see tg_parse_geojson_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_geojsonn_ix(const char *geojson, size_t len, 
    enum tg_index ix)
{
    struct tg_geom *geom = NULL;
    struct json_valid is = json_validn_ex(geojson, len, 0);
    if (!is.valid) {
        geom = make_parse_error("invalid json");
    } else {
        struct json json = json_parsen(geojson, len);
        geom = parse_geojson(json, false, ix);
    }
    if (!geom) return NULL;
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        struct tg_geom *gerr = make_parse_error("ParseError: %s", geom->error);
        tg_geom_free(geom);
        return gerr;
    }
    return geom;
}

struct writer {
    uint8_t *dst;
    size_t n;
    size_t count;
};

union raw_double {
    uint64_t u;
    double d;
};

static void write_nullterm(struct writer *wr) {
    if (wr->n > wr->count) {
        wr->dst[wr->count] = '\0';
    } else if (wr->n > 0) {
        wr->dst[wr->n-1] = '\0';
    }
}

static void write_byte(struct writer *wr, uint8_t b) {
    if (wr->count < wr->n) {
        wr->dst[wr->count] = b;
    }
    wr->count++;
}

static void write_char(struct writer *wr, char ch) {
    write_byte(wr, ch);
}

static void write_uint32le(struct writer *wr, uint32_t x) {
    for (int i = 0; i < 4; i++) {
        write_byte(wr, x>>(i*8));
    }
}

static void write_uint64le(struct writer *wr, uint64_t x) {
    for (int i = 0; i < 8; i++) {
        write_byte(wr, x>>(i*8));
    }
}

static void write_doublele(struct writer *wr, double x) {
    write_uint64le(wr, ((union raw_double){.d=x}).u);
}

static void write_string(struct writer *wr, const char *str) {
    const char *p = str;
    while (*p) write_char(wr, *(p++));
}

static void write_stringn(struct writer *wr, const char *str, size_t n) {
    for (size_t i = 0; i < n; i++) {
        write_char(wr, str[i]);
    }
}

#ifdef TG_NOAMALGA

#include "deps/fp.h"

#else

#define FP_STATIC
#define FP_NOWRITER

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// BEGIN fp.c
// END fp.c

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif

static void write_string_double(struct writer *wr, double f) {
    if (!isnormal(f)) {
        write_char(wr, '0');
        return;
    }
    size_t dstsz = wr->count < wr->n ? wr->n - wr->count : 0;
    char *dst = wr->dst ? (char*)wr->dst+wr->count : 0;
    if (print_fixed_floats) {
        wr->count += fp_dtoa(f, 'f', dst, dstsz);
    } else {
        wr->count += fp_dtoa(f, 'g', dst, dstsz);
    }
}

static void write_posn_geojson(struct writer *wr, struct tg_point posn) {
    write_char(wr, '[');
    write_string_double(wr, posn.x);
    write_char(wr, ',');
    write_string_double(wr, posn.y);
    write_char(wr, ']');
}

static void write_posn_geojson_3(struct writer *wr, struct tg_point posn, 
    double z)
{
    write_char(wr, '[');
    write_string_double(wr, posn.x);
    write_char(wr, ',');
    write_string_double(wr, posn.y);
    write_char(wr, ',');
    write_string_double(wr, z);
    write_char(wr, ']');
}
static void write_posn_geojson_4(struct writer *wr, struct tg_point posn, 
    double z, double m)
{
    write_char(wr, '[');
    write_string_double(wr, posn.x);
    write_char(wr, ',');
    write_string_double(wr, posn.y);
    write_char(wr, ',');
    write_string_double(wr, z);
    write_char(wr, ',');
    write_string_double(wr, m);
    write_char(wr, ']');
}

static int write_ring_points_geojson(struct writer *wr,
    const struct tg_ring *ring)
{
    write_char(wr, '[');
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        write_posn_geojson(wr, ring->points[i]);
    }
    write_char(wr, ']');
    return ring->npoints;
}

static int write_ring_points_geojson_3(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    double z;
    write_char(wr, '[');
    int j = 0;
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        z = (j < ncoords) ? coords[j++] : 0;
        write_posn_geojson_3(wr, ring->points[i], z);
    }
    write_char(wr, ']');
    return ring->npoints;
}

static int write_ring_points_geojson_4(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    double z, m;
    write_char(wr, '[');
    int j = 0;
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        z = (j < ncoords) ? coords[j++] : 0;
        m = (j < ncoords) ? coords[j++] : 0;
        write_posn_geojson_4(wr, ring->points[i], z, m);
    }
    write_char(wr, ']');
    return ring->npoints;
}

static int write_poly_points_geojson(struct writer *wr, 
    const struct tg_poly *poly)
{
    int count = 0;
    write_char(wr, '[');
    write_ring_points_geojson(wr, tg_poly_exterior(poly));
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        count += write_ring_points_geojson(wr, tg_poly_hole_at(poly, i));
    }
    write_char(wr, ']');
    return count;
}

static int write_poly_points_geojson_3(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    write_char(wr, '[');
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    int n = write_ring_points_geojson_3(wr, exterior, pcoords, ncoords);
    count += n;
    ncoords -= n;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+n;
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        int n = write_ring_points_geojson_3(wr, hole, pcoords, ncoords);
        count += n;
        ncoords -= n;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+n;
    }
    write_char(wr, ']');
    return count;
}

static int write_poly_points_geojson_4(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    write_char(wr, '[');
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    int n = write_ring_points_geojson_4(wr, exterior, pcoords, ncoords);
    count += n;
    ncoords -= n*2;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        int n = write_ring_points_geojson_4(wr, hole, pcoords, ncoords);
        count += n;
        ncoords -= n*2;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    }
    write_char(wr, ']');
    return count;
}

static void write_geom_point_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"Point\",\"coordinates\":");
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, "[]");
    } else {
        if ((geom->head.flags&HAS_Z) == HAS_Z) {
            if ((geom->head.flags&HAS_M) == HAS_M) {
                write_posn_geojson_4(wr, geom->point, geom->z, geom->m);
            } else {
                write_posn_geojson_3(wr, geom->point, geom->z);
            }
        } else if ((geom->head.flags&HAS_M) == HAS_M) {
            write_posn_geojson_3(wr, geom->point, geom->m);
        } else {
            write_posn_geojson(wr, geom->point);
        }
    }
    write_char(wr, '}');
}

static void write_geom_linestring_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"LineString\",\"coordinates\":");
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, "[]");
    } else {
        switch (tg_geom_dims(geom)) {
        case 3:
            write_ring_points_geojson_3(wr, (struct tg_ring*)geom->line, 
                geom->coords, geom->ncoords);
            break;
        case 4:
            write_ring_points_geojson_4(wr, (struct tg_ring*)geom->line, 
                geom->coords, geom->ncoords);
            break;
        default:
            write_ring_points_geojson(wr, (struct tg_ring*)geom->line);
            break;
        }
    }
    write_char(wr, '}');
}

static void write_geom_polygon_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"Polygon\",\"coordinates\":");
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, "[]");
    } else {
        switch (tg_geom_dims(geom)) {
        case 3:
            write_poly_points_geojson_3(wr, geom->poly, 
                geom->coords, geom->ncoords);
            break;
        case 4:
            write_poly_points_geojson_4(wr, geom->poly, 
                geom->coords, geom->ncoords);
            break;
        default: // 2
            write_poly_points_geojson(wr, geom->poly);
            break;
        }
    }
    write_char(wr, '}');
}

static void write_geom_multipoint_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"MultiPoint\",\"coordinates\":[");
    if (geom->multi) {
        int dims = tg_geom_dims(geom);
        double z, m;
        double *coords = (double *)geom->coords;
        int ncoords = geom->ncoords;
        int j = 0;
        for (int i = 0; i < geom->multi->ngeoms; i++) {
            struct tg_point point = tg_geom_point(geom->multi->geoms[i]);
            if (i > 0) write_char(wr, ',');
            switch (dims) {
            case 3:
                z = (j < ncoords) ? coords[j++] : 0;
                write_posn_geojson_3(wr, point, z);
                break;
            case 4:
                z = (j < ncoords) ? coords[j++] : 0;
                m = (j < ncoords) ? coords[j++] : 0;
                write_posn_geojson_4(wr, point, z, m);
                break;
            default: // 2
                write_posn_geojson(wr, point);
                break;
            }
        }
    }
    write_char(wr, ']');
    write_char(wr, '}');
}

static void write_geom_multilinestring_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"MultiLineString\",\"coordinates\":[");
    if (geom->multi) {
        double *pcoords = (double *)geom->coords;
        int ncoords = geom->ncoords;
        int n;
        for (int i = 0; i < geom->multi->ngeoms; i++) {
            const struct tg_line *line = tg_geom_line(geom->multi->geoms[i]);
            const struct tg_ring *ring = (struct tg_ring*)line;
            if (i > 0) write_char(wr, ',');
            switch (tg_geom_dims(geom)) {
            case 3:
                n = write_ring_points_geojson_3(wr, ring, pcoords, ncoords);
                ncoords -= n;
                if (ncoords < 0) ncoords = 0;
                pcoords = ncoords == 0 ? NULL : pcoords+n;
                break;
            case 4:
                n = write_ring_points_geojson_4(wr, ring, pcoords, ncoords);
                ncoords -= n*2;
                if (ncoords < 0) ncoords = 0;
                pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
                break;
            default: // 2
                write_ring_points_geojson(wr, ring);
                break;
            }
        }
    }
    write_char(wr, ']');
    write_char(wr, '}');
}

static void write_geom_multipolygon_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "{\"type\":\"MultiPolygon\",\"coordinates\":[");
    if (geom->multi) {
        double *pcoords = (double *)geom->coords;
        int ncoords = geom->ncoords;
        int n;
        for (int i = 0; i < geom->multi->ngeoms; i++) {
            const struct tg_poly *poly = tg_geom_poly(geom->multi->geoms[i]);
            if (i > 0) write_char(wr, ',');
            switch (tg_geom_dims(geom)) {
            case 3:
                n = write_poly_points_geojson_3(wr, poly, pcoords, ncoords);
                ncoords -= n;
                if (ncoords < 0) ncoords = 0;
                pcoords = ncoords == 0 ? NULL : pcoords+n;
                break;
            case 4:
                n = write_poly_points_geojson_4(wr, poly, pcoords, ncoords);
                ncoords -= n*2;
                if (ncoords < 0) ncoords = 0;
                pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
                break;
            default: // 2
                write_poly_points_geojson(wr, poly);
                break;
            }
        }
    }
    write_char(wr, ']');
    write_char(wr, '}');
}

static void write_geom_geojson(const struct tg_geom *geom, struct writer *wr);

static void write_geom_geometrycollection_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    if ((geom->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL) {
        write_string(wr, "{\"type\":\"FeatureCollection\",\"features\":[");
    } else {
        write_string(wr, "{\"type\":\"GeometryCollection\",\"geometries\":[");
    }
    int ngeoms = tg_geom_num_geometries(geom);
    for (int i = 0; i < ngeoms; i++) {
        if (i > 0) write_char(wr, ',');
        write_geom_geojson(tg_geom_geometry_at(geom, i), wr);
    }
    write_char(wr, ']');
    write_char(wr, '}');
}

static void write_base_geom_geojson(const struct tg_geom *geom,
    struct writer *wr)
{
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        // sigh, just write us an empty point ...
        write_string(wr, "{\"type\":\"Point\",\"coordinates\":[]}");
        return;
    }
    bool is_feat = (geom->head.flags&IS_FEATURE) == IS_FEATURE;
    struct json fjson = { 0 };
    struct json gjson = { 0 };
    const char *xjson = geom->head.base == BASE_GEOM ? geom->xjson : NULL;
    if (is_feat) {
        if (xjson) {
            struct json json = json_parse(xjson);
            if (json_type(json) == JSON_ARRAY) {
                fjson = json_ensure(json_first(json));
                gjson = json_ensure(json_next(fjson));
            } else if (json_type(json) == JSON_OBJECT) {
                fjson = json_ensure(json);
            }
        }
        write_string(wr, "{\"type\":\"Feature\",");
        struct json id = json_object_get(fjson, "id");
        if (json_exists(id)) {
            write_string(wr, "\"id\":");
            write_stringn(wr, json_raw(id), json_raw_length(id));
            write_char(wr, ',');
        }
        write_string(wr, "\"geometry\":");
    } else {
        if (xjson) {
            gjson = json_ensure(json_parse(xjson));
        }
    }
    if (is_feat && (geom->head.flags&IS_UNLOCATED) == IS_UNLOCATED) {
        write_string(wr, "null");
    } else {
        switch (geom->head.type) {
        case TG_POINT:
            write_geom_point_geojson(geom, wr);
            break;
        case TG_LINESTRING:
            write_geom_linestring_geojson(geom, wr);
            break;
        case TG_POLYGON:
            write_geom_polygon_geojson(geom, wr);
            break;
        case TG_MULTIPOINT:
            write_geom_multipoint_geojson(geom, wr);
            break;
        case TG_MULTILINESTRING:
            write_geom_multilinestring_geojson(geom, wr);
            break;
        case TG_MULTIPOLYGON:
            write_geom_multipolygon_geojson(geom, wr);
            break;
        case TG_GEOMETRYCOLLECTION:
            write_geom_geometrycollection_geojson(geom, wr);
            break;
        }
    }
    if (json_type(gjson) == JSON_OBJECT) {
        if (json_exists(json_first(gjson))) {
            long len = json_raw_length(gjson)-1;
            if (len > 1) {
                // rewind one byte
                wr->count--;
                write_char(wr, ',');
                write_stringn(wr, (char*)(json_raw(gjson)+1), len);
            }
        }
    }
    if (is_feat) {
        bool wrote_props = false;
        if (json_type(fjson) == JSON_OBJECT) {
            struct json key = json_first(fjson);
            struct json val = json_next(key);
            while (json_exists(key)) {
                if (json_raw_compare(key, "\"id\"") != 0) {
                    write_char(wr, ',');
                    write_stringn(wr, json_raw(key), json_raw_length(key));
                    write_char(wr, ':');
                    write_stringn(wr, json_raw(val), json_raw_length(val));
                    if (!wrote_props && 
                        json_raw_compare(key, "\"properties\"") == 0)
                    {
                        wrote_props = true;
                    }
                }
                key = json_next(val);
                val = json_next(key);
            }
        }
        if (!wrote_props) {
            write_string(wr, ",\"properties\":");
            if ((geom->head.flags&HAS_NULL_PROPS)== HAS_NULL_PROPS){
                write_string(wr, "null");
            } else {
                write_string(wr, "{}");
            }
        }
        write_char(wr, '}');
    }
}

static void write_point_geojson(const struct boxed_point *point,
    struct writer *wr)
{
    if ((point->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, "{\"type\":\"Feature\",\"geometry\":");
    }
    write_string(wr, "{\"type\":\"Point\",\"coordinates\":");
    write_posn_geojson(wr, point->point);
    write_char(wr, '}');
    if ((point->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, ",\"properties\":");
        if ((point->head.flags&HAS_NULL_PROPS)== HAS_NULL_PROPS){
            write_string(wr, "null}");
        } else {
            write_string(wr, "{}}");
        }
    }
}

static void write_line_geojson(const struct tg_line *line, struct writer *wr) {
    struct tg_ring *ring = (struct tg_ring*)line;
    if ((ring->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, "{\"type\":\"Feature\",\"geometry\":");
    }
    write_string(wr, "{\"type\":\"LineString\",\"coordinates\":");
    write_ring_points_geojson(wr, ring);
    write_char(wr, '}');
    if ((ring->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, ",\"properties\":");
        if ((ring->head.flags&HAS_NULL_PROPS)== HAS_NULL_PROPS){
            write_string(wr, "null}");
        } else {
            write_string(wr, "{}}");
        }
    }
}

static void write_ring_geojson(const struct tg_ring *ring, struct writer *wr) {
    if ((ring->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, "{\"type\":\"Feature\",\"geometry\":");
    }
    write_string(wr, "{\"type\":\"Polygon\",\"coordinates\":[");
    write_ring_points_geojson(wr, ring);
    write_string(wr, "]}");
    if ((ring->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, ",\"properties\":");
        if ((ring->head.flags&HAS_NULL_PROPS)== HAS_NULL_PROPS){
            write_string(wr, "null}");
        } else {
            write_string(wr, "{}}");
        }
    }
}

static void write_poly_geojson(const struct tg_poly *poly, struct writer *wr) {
    if ((poly->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, "{\"type\":\"Feature\",\"geometry\":");
    }
    write_string(wr, "{\"type\":\"Polygon\",\"coordinates\":");
    write_poly_points_geojson(wr, poly);
    write_char(wr, '}');
    if ((poly->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_string(wr, ",\"properties\":");
        if ((poly->head.flags&HAS_NULL_PROPS)== HAS_NULL_PROPS){
            write_string(wr, "null}");
        } else {
            write_string(wr, "{}}");
        }
    }
}

static void write_geom_geojson(const struct tg_geom *geom, struct writer *wr) {
    switch (geom->head.base) {
    case BASE_GEOM:
        write_base_geom_geojson(geom, wr);
        break;
    case BASE_POINT:
        write_point_geojson((struct boxed_point*)geom, wr);
        break;
    case BASE_LINE:
        write_line_geojson((struct tg_line*)geom, wr);
        break;
    case BASE_RING:
        write_ring_geojson((struct tg_ring*)geom, wr);
        break;
    case BASE_POLY:
        write_poly_geojson((struct tg_poly*)geom, wr);
        break;
    }
}

/// Writes a GeoJSON representation of a geometry.
///
/// The content is stored as a C string in the buffer pointed to by dst.
/// A terminating null character is automatically appended after the
/// content written.
///
/// @param geom Input geometry
/// @param dst Buffer where the resulting content is stored.
/// @param n Maximum number of bytes to be used in the buffer.
/// @return  The number of characters, not including the null-terminator, 
/// needed to store the content into the C string buffer.
/// If the returned length is greater than n-1, then only a parital copy
/// occurred, for example:
///
/// ```
/// char str[64];
/// size_t len = tg_geom_geojson(geom, str, sizeof(str));
/// if (len > sizeof(str)-1) {
///     // ... write did not complete ...
/// }
/// ```
///
/// @see tg_geom_wkt()
/// @see tg_geom_wkb()
/// @see tg_geom_hex()
/// @see GeometryWriting
size_t tg_geom_geojson(const struct tg_geom *geom, char *dst, size_t n) {
    if (!geom) return 0;
    struct writer wr = { .dst = (uint8_t*)dst, .n = n };
    write_geom_geojson(geom, &wr);
    write_nullterm(&wr);
    return wr.count;
}

/// Returns a string that represents any extra JSON from a parsed GeoJSON
/// geometry. Such as the "id" or "properties" fields.
/// @param geom Input geometry
/// @return Returns a valid JSON object as a string, or NULL if the geometry
/// did not come from GeoJSON or there is no extra JSON.
/// @note The returned string does not need to be freed.
/// @see tg_parse_geojson()
const char *tg_geom_extra_json(const struct tg_geom *geom) {
    return geom && geom->head.base == BASE_GEOM &&
          (geom->head.flags&IS_ERROR) != IS_ERROR ? geom->xjson : NULL;
}

//////////////////
// wkt
//////////////////

static const char *wkt_invalid_err(const char *inner) {
    (void)inner;
    return "invalid text";
}

static bool isws(char c) {
    return c <= ' ' && (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static bool isnum(char c) {
    return c == '-' || (c >= '0' && c <= '9');
}

static long wkt_trim_ws(const char *wkt, long len, long i) {
    for (; i < len; i++) {
        if (!isws(wkt[i])) break;
    }
    return i;
}

// returns -1 for invalid 'Z', 'M', 'ZM' or 'EMPTY'
static enum tg_geom_type wkt2type(const char *wkt, long len, 
    bool *z, bool *m, bool *empty)
{
    *z = false;
    *m = false;
    *empty = false;
    char name[32];
    long i = 0;
    long j = 0;
    int nws = 0;
    for (; i < len; i++) {
        if (j == sizeof(name)-1) {
            goto bad_type;
        }
        if (isws(wkt[i])) {
            if (j > 0 && name[j-1] == ' ') continue;
            name[j] = ' ';
            nws++;
            if (nws > 2) return -1;
        } else if (wkt[i] >= 'a' && wkt[i] <= 'z') {
            name[j] = wkt[i]-32;
        } else {
            name[j] = wkt[i];
        }
        j++;
    }
    name[j] = '\0';
    if (j > 6) {
        // look for " EMPTY" suffix
        if (strcmp(name+j-6, " EMPTY") == 0) {
            j -= 6;
            name[j] = '\0';
            *empty = true;
        }
    }
    if (j > 3) {
        // look for " ZM", " Z", " M" and smash it
        if (name[j-2] == 'Z' && name[j-1] == 'M') {
            j -= 2;
            *z = true;
            *m = true;
        } else if (name[j-1] == 'Z') {
            j -= 1;
            *z = true;
        } else if (name[j-1] == 'M') {
            j -= 1;
            *m = true;
        }
        if (name[j-1] == ' ') {
            j -= 1;
        }
        name[j] = '\0';
    }
    if (j == 0) return 0;
    if (!strcmp(name, "POINT")) return TG_POINT;
    if (!strcmp(name, "LINESTRING")) return TG_LINESTRING;
    if (!strcmp(name, "POLYGON")) return TG_POLYGON;
    if (!strcmp(name, "MULTIPOINT")) return TG_MULTIPOINT;
    if (!strcmp(name, "MULTILINESTRING")) return TG_MULTILINESTRING;
    if (!strcmp(name, "MULTIPOLYGON")) return TG_MULTIPOLYGON;
    if (!strcmp(name, "GEOMETRYCOLLECTION")) return TG_GEOMETRYCOLLECTION;
    if (strchr(name, ' ')) return -1;
bad_type:
    // determine the length of the bad type
    i = 0;
    for (; i < len; i++) {
        char c = wkt[i];
        if (isws(c)) break;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 'A' || c > 'Z') break;
    }
    return -(i+1);
}

static long wkt_balance_coords(const char *wkt, long len, long i) {
    i++; // first '(' already checked by caller
    long depth = 1;
    long maxdepth = 1;
    for (; i < len; i++) {
        if (wkt[i] == '(') {
            depth++;
            maxdepth++;
        } else if (wkt[i] == ')') {
            depth--;
            if (depth == 0) {
                if (maxdepth > MAXDEPTH) {
                    return -(i+1); 
                }
                return i+1;
            }
        }
    }
    return -(i+1); 
}

static long wkt_vnumber(const char *data, long dlen, long i) {
    // sign
    if (data[i] == '-') {
        i++;
        if (i == dlen) return -(i+1);
    }
    // int
    if ((data[i] < '0' || data[i] > '9') && data[i] != '.') return -(i+1);
    for (; i < dlen; i++) {
        if (data[i] >= '0' && data[i] <= '9') continue;
        break;
    }
    // frac
    if (i == dlen) return i;
    if (data[i] == '.') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    // exp
    if (i == dlen) return i;
    if (data[i] == 'e' || data[i] == 'E') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] == '+' || data[i] == '-') i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    return i;
}

static const char *err_for_wkt_posn(int dims) {
    if (dims == 2) {
        return "each position must have two numbers";
    } else if (dims == 3) {
        return "each position must have three numbers";
    } else if (dims == 4) {
        return "each position must have four numbers";
    } else {
        return "each position must have two to four numbers";
    }
}

static struct tg_geom *parse_wkt_point(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    (void)ix;
    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    long i = 0;
    double posn[4] = { 0 };
    int pdims = 0;
    i = wkt_trim_ws(wkt, len, i);
    if (i == len) goto bad_dims;
    while (1) {
        long s = i;
        if ((i = wkt_vnumber(wkt, len, i)) < 0) {
            return make_parse_error(wkt_invalid_err("invalid number"));
        }
        if (pdims < 4) {
            posn[pdims++] = strtod(wkt+s, NULL);
        } else goto bad_dims;
        if (i == len) break;
        if (isws(wkt[i])) {
            i = wkt_trim_ws(wkt, len, i);
            if (i == len) break;
        } else {
            return make_parse_error(wkt_invalid_err("invalid number"));
        }
    }
    enum flags flags = 0;
    if (dims == 0) {
        if (pdims < 2) goto bad_dims;
    } else {
        if (pdims != dims) goto bad_dims;
    }
    dims = pdims;
    struct tg_point pt = { posn[0], posn[1] };
    struct tg_geom *geom = NULL;
    if (dims == 2) {
        geom = tg_geom_new_point(pt);
    } else if (dims == 3) {
        if (m) {
            geom = tg_geom_new_point_m(pt, posn[2]);
        } else {
            geom = tg_geom_new_point_z(pt, posn[2]);
        }
    } else {
        geom = tg_geom_new_point_zm(pt, posn[2], posn[3]);
    }
    if (geom) geom->head.flags |= flags;
    return geom;
bad_dims:
    return make_parse_error("%s", err_for_wkt_posn(dims));
}

static int parse_wkt_posns(enum base base, int dims, int depth, const char *wkt, 
    long len, struct dvec *posns, struct dvec *xcoords, const char **err)
{
    (void)depth; // TODO: return correct depth errors
    double posn[4] = { 0 };
    int pdims = 0;
    long i = 0;
    i = wkt_trim_ws(wkt, len, i);
    if (i == len) {
        // err: expected numbers
        goto exp_nums;
    }
    bool xparens = false;
    if (base == BASE_POINT && wkt[i] == '(') {
        // The multipoint is using the format 'MULTIPOINT ((1 2),(3 4))'.
        // While not standard, it must be supported.
        xparens = true;
    }
    while (i < len) {
        if (xparens) {
            if (i == len || wkt[i] != '(') {
                // err: expected '('
                *err = wkt_invalid_err("expected '('");
                return -1;
            }
            i = wkt_trim_ws(wkt, len, i+1);
        }
        // read each number, delimted by whitespace
        while (i < len) {
            double num;
            if (isnum(wkt[i])) {
                long s = i;
                i = wkt_vnumber(wkt, len, i);
                if (i < 0) {
                    *err = wkt_invalid_err("invalid number");
                    return -1;
                }
                num = strtod(wkt+s, NULL);
            } else if (wkt[i] == ')') {
                // err: expected a number, got ')'
                *err = wkt_invalid_err("expected number, got '('");
                return -1;
            } else if (wkt[i] == ',') {
                *err = wkt_invalid_err("expected number, got ','");
                return -1;
            } else {
                *err = wkt_invalid_err("expected a number");
                return -1;
            }
            if (pdims == 4) {
                *err = err_for_wkt_posn(dims);
                return -1;
            }
            posn[pdims++] = num;
            if (i == len || !isws(wkt[i])) break;
            if ((i = wkt_trim_ws(wkt, len, i+1)) == len) break;
            if (wkt[i] == ')' || wkt[i] == ',') break;
        }
        if (xparens) {
            if (i == len || wkt[i] != ')') {
                *err = wkt_invalid_err("expected ')'");
                return -1;
            }
            i = wkt_trim_ws(wkt, len, i+1);
        }
        if (i < len) {
            if (wkt[i] != ',') {
                *err = wkt_invalid_err("expected ','");
                return -1;
            }
            i = wkt_trim_ws(wkt, len, i+1);
            if (i == len) {
                *err = wkt_invalid_err("expected position, got end of stream");
                return -1;
            }
        }
        if (dims != pdims) {
            if (dims == 0 && pdims >= 2) {
                dims = pdims;
            } else {
                *err = err_for_wkt_posn(dims);
                return -1;
            }
        }
        if (!dvec_append(posns, posn[0]) || !dvec_append(posns, posn[1])) {
            return -1;
        }
        for (int i = 2; i < dims; i++) {
            if (i >= pdims || !dvec_append(xcoords, posn[i])) return -1;
        }
        pdims = 0;
    }
exp_nums:
    if (!check_parse_posns(base, posns->data, posns->len, err)) return -1;
    return dims;
}

static int parse_wkt_multi_posns(enum base base, int dims, int depth, 
    const char *wkt, long len, struct dvec *posns, struct rvec *rings,  
    struct tg_poly **poly, struct dvec *xcoords, enum tg_index ix,
    const char **err)
{
    long i = 0;
    i = wkt_trim_ws(wkt, len, i);
    while (i < len) {
        if (wkt[i] != '(') {
            *err = wkt_invalid_err("expected '('");
            return -1;
        }
        long j = wkt_balance_coords(wkt, len, i);
        const char *grp_wkt = wkt+i+1;
        long grp_len = j-i-2;
        i = j;
        posns->len = 0;
        dims = parse_wkt_posns(base, dims, depth, grp_wkt, grp_len, posns, 
            xcoords, err);
        if (dims == -1) return -1;
        struct tg_ring *ring = tg_ring_new_ix((struct tg_point*)posns->data, 
            posns->len / 2, ix);
        if (!ring) return -1;
        if (!rvec_append(rings, ring)) {
            tg_ring_free(ring);
            return -1;
        }
        i = wkt_trim_ws(wkt, len, i);
        if (i == len) break;
        if (wkt[i] != ',') {
            *err = wkt_invalid_err("expected ','");
            return -1;
        }
        i = wkt_trim_ws(wkt, len, i+1);
        if (i == len) {
            *err = wkt_invalid_err("expected '(', got end of stream");
            return -1;
        }
    }
    if (rings->len == 0) {
        *err = "polygons must have one or more rings";
        return -1;
    }
    *poly = tg_poly_new(rings->data[0], 
        (struct tg_ring const*const*)rings->data+1, rings->len-1);
    if (!*poly) return -1;
    for (size_t i = 0; i < rings->len; i++) {
        tg_ring_free(rings->data[i]);
    }
    rings->len = 0;
    return dims;
}

static struct tg_geom *parse_wkt_linestring(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct tg_line *line = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    const char *err = NULL;

    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    dims = parse_wkt_posns(BASE_LINE, dims, 1, wkt, len, &posns, &xcoords,
        &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    line = tg_line_new_ix((struct tg_point*)posns.data, posns.len / 2, ix);
    if (!line) goto fail;
    switch (dims) {
    case 2: 
        geom = tg_geom_new_linestring(line);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_linestring_m(line, xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_linestring_z(line, xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_linestring_zm(line, xcoords.data, xcoords.len);
        break;
    }
cleanup:
    tg_line_free(line);
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static struct tg_geom *parse_wkt_polygon(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct tg_poly *poly = NULL;
    struct rvec rings = { 0 };
    const char *err = NULL;

    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    dims = parse_wkt_multi_posns(BASE_RING, dims, 2, wkt, len, &posns, 
        &rings, &poly, &xcoords, ix, &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    switch (dims) {
    case 2: 
        geom = tg_geom_new_polygon(poly);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_polygon_m(poly, xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_polygon_z(poly, xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_polygon_zm(poly, xcoords.data, xcoords.len);
        break;
    }
cleanup:
    tg_poly_free(poly);
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    if (rings.data) {
        for (size_t i = 0; i < rings.len; i++) {
            tg_ring_free(rings.data[i]);
        }
        tg_free(rings.data);
    }
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static struct tg_geom *parse_wkt_multipoint(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    (void)ix;
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    const char *err = NULL;

    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    dims = parse_wkt_posns(BASE_POINT, dims, 1, wkt, len, &posns, &xcoords,
        &err);
    if (dims == -1) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    const struct tg_point *points = (struct tg_point*)posns.data;
    int npoints = posns.len/2;
    switch (dims) {
    case 2: 
        geom = tg_geom_new_multipoint(points, npoints);
        break;
    case 3: 
        if (m) {
            geom = tg_geom_new_multipoint_m(points, npoints, 
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multipoint_z(points, npoints, 
                xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_multipoint_zm(points, npoints, 
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static struct tg_geom *parse_wkt_multilinestring(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct lvec lines = { 0 };
    const char *err = NULL;
    long i = wkt_trim_ws(wkt, len, 0);
    while (i < len) {
        if (wkt[i] != '(') {
            gerr = make_parse_error(wkt_invalid_err("expected '('"));
            goto fail;
        }
        long j = wkt_balance_coords(wkt, len, i);
        const char *grp_wkt = wkt+i+1;
        long grp_len = j-i-2;
        i = j;
        posns.len = 0;
        dims = parse_wkt_posns(BASE_LINE, dims, 2, grp_wkt, grp_len, &posns,
            &xcoords, &err);
        if (dims == -1) {
            gerr = err ? make_parse_error("%s", err) : NULL;
            goto fail;
        }
        struct tg_line *line = tg_line_new_ix((struct tg_point*)posns.data, 
            posns.len / 2, ix);
        if (!line) goto fail;
        if (!lvec_append(&lines, line)) {
            tg_line_free(line);
            goto fail;
        }
        i = wkt_trim_ws(wkt, len, i);
        if (i == len) break;
        if (wkt[i] != ',') {
            gerr = make_parse_error(wkt_invalid_err("expected ','"));
            goto fail;
        }
        i = wkt_trim_ws(wkt, len, i+1);
        if (i == len) {
            gerr = make_parse_error(wkt_invalid_err("expected '('"));
            goto fail;
        }
    }
    switch (dims) {
    case 2:
        geom = tg_geom_new_multilinestring(
                (struct tg_line const*const*)lines.data, lines.len);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_multilinestring_m(
                (struct tg_line const*const*)lines.data, lines.len,
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multilinestring_z(
                (struct tg_line const*const*)lines.data, lines.len,
                xcoords.data, xcoords.len);
        }
        break;
    default:
        geom = tg_geom_new_multilinestring_zm(
            (struct tg_line const*const*)lines.data, lines.len,
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    if (lines.data) {
        for (size_t i = 0; i < lines.len; i++) {
            tg_line_free(lines.data[i]);
        }
        tg_free(lines.data);
    }
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static struct tg_geom *parse_wkt_multipolygon(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    int dims = z ? m ? 4 : 3 : m ? 3 : 0;
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct tg_poly *poly = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct rvec rings = { 0 };
    struct pvec polys = { 0 };
    const char *err = NULL;
    long i = wkt_trim_ws(wkt, len, 0);
    while (i < len) {
        if (wkt[i] != '(') {
            gerr = make_parse_error(wkt_invalid_err("expected '('"));
            goto fail;
        }
        long j = wkt_balance_coords(wkt, len, i);
        const char *grp_wkt = wkt+i+1;
        long grp_len = j-i-2;
        i = j;
        posns.len = 0;
        rings.len = 0;
        dims = parse_wkt_multi_posns(BASE_RING, dims, 3, grp_wkt, grp_len,
            &posns, &rings, &poly, &xcoords, ix, &err);
        if (dims == -1) {
            gerr = err ? make_parse_error("%s", err) : NULL;
            goto fail;
        }
        if (!pvec_append(&polys, poly)) {
            tg_poly_free(poly);
            goto fail;
        }
        i = wkt_trim_ws(wkt, len, i);
        if (i == len) break;
        if (wkt[i] != ',') {
            gerr = make_parse_error(wkt_invalid_err("expected ','"));
            goto fail;
        }
        i = wkt_trim_ws(wkt, len, i+1);
        if (i == len) {
            gerr = make_parse_error(wkt_invalid_err("expected '('"));
            goto fail;
        }
    }
    switch (dims) {
    case 2:
        geom = tg_geom_new_multipolygon(
                (struct tg_poly const*const*)polys.data, polys.len);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_multipolygon_m(
                (struct tg_poly const*const*)polys.data, polys.len,
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multipolygon_z(
                (struct tg_poly const*const*)polys.data, polys.len,
                xcoords.data, xcoords.len);
        }
        break;
    default:
        geom = tg_geom_new_multipolygon_zm(
            (struct tg_poly const*const*)polys.data, polys.len,
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    if (polys.data) {
        for (size_t i = 0; i < polys.len; i++) {
            tg_poly_free(polys.data[i]);
        }
        tg_free(polys.data);
    }
    if (rings.data) {
        for (size_t i = 0; i < rings.len; i++) {
            tg_ring_free(rings.data[i]);
        }
        tg_free(rings.data);
    }
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static long wkt_next_geometry(const char *wkt, long len, long i) {
    for (;i < len;i++) {
        if (wkt[i] == ',') break;
        if (wkt[i] == '(') {
            return wkt_balance_coords(wkt, len, i);
        }
    }
    return i;
}

static struct tg_geom *parse_wkt(const char *wkt, long len, enum tg_index ix);

static struct tg_geom *parse_wkt_geometrycollection(const char *wkt, long len, 
    bool z, bool m, enum tg_index ix)
{
    (void)z; (void)m; // not used
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct gvec geoms = { 0 };
    long i = 0;
    size_t commas = 0;
    while (i < len) {
        long s = i;
        i = wkt_next_geometry(wkt, len, i);
        if (i-s > 0) {
            struct tg_geom *child = parse_wkt(wkt+s, i-s, ix);
            if (!child) goto fail;
            if (tg_geom_error(child)) {
                gerr = child; 
                child = NULL;
                goto fail;
            }
            if (!gvec_append(&geoms, child)) {
                tg_geom_free(child);
                goto fail;
            }
        }
        i = wkt_trim_ws(wkt, len, i);
        if (i == len) break;
        if (wkt[i] != ',') {
            gerr = make_parse_error(wkt_invalid_err("expected ','"));
            goto fail;
        }
        i = wkt_trim_ws(wkt, len, i);
        commas++;
        i++;
    }
    if (commas+1 != geoms.len) {
        // err: missing last geometry
        gerr = make_parse_error("missing type");
        goto fail;
    }
    geom = tg_geom_new_geometrycollection(
        (struct tg_geom const*const*)geoms.data, geoms.len);
cleanup:
    if (geoms.data) {
        for (size_t i = 0; i < geoms.len; i++) {
            tg_geom_free(geoms.data[i]);
        }
        tg_free(geoms.data);
    }
    return geom;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static struct tg_geom *parse_wkt(const char *wkt, long len, enum tg_index ix) {
    if (len == 0) {
        return make_parse_error("missing type");
    }
    long i = wkt_trim_ws(wkt, len, 0);
    long s = i;
    for (; i < len; i++) if (wkt[i] == '(') break;
    long e = i;
    while (e-1 > s && isws(wkt[e-1])) e--;
    bool z, m, is_empty;
    enum tg_geom_type type = wkt2type(wkt+s, e-s, &z, &m, &is_empty);
    if ((int)type <= 0) {
        int n = (int)type;
        if (n == 0) {
            return make_parse_error("missing type");
        } else if (n == -1) {
            return make_parse_error("invalid type specifier, "
                "expected 'Z', 'M', 'ZM', or 'EMPTY'");
        } else {
            n = (-n)-1;
            return make_parse_error("unknown type '%.*s'", n, wkt+s);
        }
    }
    if (is_empty) {
        switch (type) {
        case TG_POINT: return tg_geom_new_point_empty();
        case TG_LINESTRING: return tg_geom_new_linestring_empty();
        case TG_POLYGON: return tg_geom_new_polygon_empty();
        case TG_MULTIPOINT: return tg_geom_new_multipoint_empty();
        case TG_MULTILINESTRING: return tg_geom_new_multilinestring_empty();
        case TG_MULTIPOLYGON: return tg_geom_new_multipolygon_empty();
        default: return tg_geom_new_geometrycollection_empty();
        }
    }
    if (i == len || wkt[i] != '(') {
        return make_parse_error(wkt_invalid_err("expected '('"));
    }
    long j = wkt_balance_coords(wkt, len, i);
    if (j <= 0) {
        return make_parse_error(wkt_invalid_err("unbalanced '()'"));
    }
    long k = j;
    for (; k < len; k++) {
        if (!isws(wkt[k])) {
            return make_parse_error(
                wkt_invalid_err("too much data after last ')'"));
        }
    }
    // Only use the inner parts of the group. Do not include the parens.
    wkt = wkt+i+1;
    len = j-i-2;
    switch (type) {
    case TG_POINT: return parse_wkt_point(wkt, len, z, m, ix);
    case TG_LINESTRING: return parse_wkt_linestring(wkt, len, z, m, ix);
    case TG_POLYGON: return parse_wkt_polygon(wkt, len, z, m, ix);
    case TG_MULTIPOINT: return parse_wkt_multipoint(wkt, len, z, m, ix);
    case TG_MULTILINESTRING: 
        return parse_wkt_multilinestring(wkt, len, z, m, ix);
    case TG_MULTIPOLYGON: return parse_wkt_multipolygon(wkt, len, z, m, ix);
    default: return parse_wkt_geometrycollection(wkt, len, z, m, ix);
    }
}

/// Parse Well-known text (WKT) with an included data length.
/// @param wkt WKT data
/// @param len Length of data
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wkt()
/// @see GeometryParsing
struct tg_geom *tg_parse_wktn(const char *wkt, size_t len) {
    return tg_parse_wktn_ix(wkt, len, TG_DEFAULT);
}

/// Parse Well-known text (WKT).
/// @param wkt A WKT string. Must be null-terminated
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wktn()
/// @see tg_parse_wkt_ix()
/// @see tg_parse_wktn_ix()
/// @see tg_geom_error()
/// @see tg_geom_wkt()
/// @see GeometryParsing
struct tg_geom *tg_parse_wkt(const char *wkt) {
    return tg_parse_wktn_ix(wkt, wkt?strlen(wkt):0, TG_DEFAULT);
}

/// Parse Well-known text (WKT) using provided indexing option.
/// @param wkt A WKT string. Must be null-terminated
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wkt()
/// @see tg_parse_wktn_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_wkt_ix(const char *wkt, enum tg_index ix) {
    return tg_parse_wktn_ix(wkt, wkt?strlen(wkt):0, ix);
}

/// Parse Well-known text (WKT) using provided indexing option. 
/// @param wkt WKT data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wkt()
/// @see tg_parse_wkt_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_wktn_ix(const char *wkt, size_t len, 
    enum tg_index ix)
{
    struct tg_geom *geom = parse_wkt(wkt, len, ix);
    if (!geom) return NULL;
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        struct tg_geom *gerr = make_parse_error("ParseError: %s", geom->error);
        tg_geom_free(geom);
        return gerr;
    }
    return geom;
}

static void write_posn_wkt(struct writer *wr, struct tg_point posn) {
    write_string_double(wr, posn.x);
    write_char(wr, ' ');
    write_string_double(wr, posn.y);
}

static void write_posn_wkt_3(struct writer *wr, struct tg_point posn, 
    double z)
{
    write_string_double(wr, posn.x);
    write_char(wr, ' ');
    write_string_double(wr, posn.y);
    write_char(wr, ' ');
    write_string_double(wr, z);
}

static void write_posn_wkt_4(struct writer *wr, struct tg_point posn, 
    double z, double m)
{
    write_string_double(wr, posn.x);
    write_char(wr, ' ');
    write_string_double(wr, posn.y);
    write_char(wr, ' ');
    write_string_double(wr, z);
    write_char(wr, ' ');
    write_string_double(wr, m);
}

static void write_point_wkt(const struct boxed_point *point, 
    struct writer *wr)
{
    write_string(wr, "POINT(");
    write_posn_wkt(wr, point->point);
    write_char(wr, ')');
}

static int write_ring_points_wkt(struct writer *wr, const struct tg_ring *ring)
{
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        write_posn_wkt(wr, ring->points[i]);
    }
    return ring->npoints;
}


static int write_ring_points_wkt_3(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    double z;
    int j = 0;
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        z = (j < ncoords) ? coords[j++] : 0;
        write_posn_wkt_3(wr, ring->points[i], z);
    }
    return ring->npoints;
}

static int write_ring_points_wkt_4(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    double z, m;
    int j = 0;
    for (int i = 0 ; i < ring->npoints; i++) {
        if (i > 0) write_char(wr, ',');
        z = (j < ncoords) ? coords[j++] : 0;
        m = (j < ncoords) ? coords[j++] : 0;
        write_posn_wkt_4(wr, ring->points[i], z, m);
    }
    return ring->npoints;
}

static void write_line_wkt(const struct tg_line *line, struct writer *wr) {
    struct tg_ring *ring = (struct tg_ring*)line;
    write_string(wr, "LINESTRING(");
    write_ring_points_wkt(wr, ring);
    write_char(wr, ')');
}

static void write_ring_wkt(const struct tg_ring *ring, struct writer *wr) {
    write_string(wr, "POLYGON((");
    write_ring_points_wkt(wr, ring);
    write_string(wr, "))");
}


static int write_poly_points_wkt(struct writer *wr, 
    const struct tg_poly *poly)
{
    int count = 0;
    write_char(wr, '(');
    write_ring_points_wkt(wr, tg_poly_exterior(poly));
    write_char(wr, ')');
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        write_char(wr, '(');
        count += write_ring_points_wkt(wr, tg_poly_hole_at(poly, i));
        write_char(wr, ')');
    }
    
    return count;
}

static int write_poly_points_wkt_3(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    write_char(wr, '(');
    int n = write_ring_points_wkt_3(wr, exterior, pcoords, ncoords);
    write_char(wr, ')');
    count += n;
    ncoords -= n;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+n;
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        write_char(wr, '(');
        int n = write_ring_points_wkt_3(wr, hole, pcoords, ncoords);
        write_char(wr, ')');
        count += n;
        ncoords -= n;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+n;
    }
    return count;
}

static int write_poly_points_wkt_4(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    write_char(wr, '(');
    int n = write_ring_points_wkt_4(wr, exterior, pcoords, ncoords);
    write_char(wr, ')');
    count += n;
    ncoords -= n*2;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    int nholes = tg_poly_num_holes(poly);
    for (int i = 0 ; i < nholes; i++) {
        write_char(wr, ',');
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        write_char(wr, '(');
        int n = write_ring_points_wkt_4(wr, hole, pcoords, ncoords);
        write_char(wr, ')');
        count += n;
        ncoords -= n*2;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    }
    return count;
}

static void write_poly_wkt(const struct tg_poly *poly, struct writer *wr) {
    write_string(wr, "POLYGON(");
    write_poly_points_wkt(wr, poly);
    write_char(wr, ')');
}

static void write_zm_def_wkt(struct writer *wr, const struct tg_geom *geom) {
    if ((geom->head.flags&HAS_M) == HAS_M && 
               (geom->head.flags&HAS_Z) != HAS_Z)
    {
        write_string(wr, " M");
    }
}

static void write_geom_point_wkt(const struct tg_geom *geom, 
    struct writer *wr)
{
    write_string(wr, "POINT");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, " EMPTY");
    } else {
        write_char(wr, '(');
        if ((geom->head.flags&HAS_Z) == HAS_Z) {
            if ((geom->head.flags&HAS_M) == HAS_M) {
                write_posn_wkt_4(wr, geom->point, geom->z, geom->m);
            } else {
                write_posn_wkt_3(wr, geom->point, geom->z);
            }
        } else if ((geom->head.flags&HAS_M) == HAS_M) {
            write_posn_wkt_3(wr, geom->point, geom->m);
        } else {
            write_posn_wkt(wr, geom->point);
        }
        write_char(wr, ')');
    }
}

static void write_geom_linestring_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "LINESTRING");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    switch (tg_geom_dims(geom)) {
    case 3:
        write_ring_points_wkt_3(wr, (struct tg_ring*)geom->line, 
            geom->coords, geom->ncoords);
        break;
    case 4:
        write_ring_points_wkt_4(wr, (struct tg_ring*)geom->line, 
            geom->coords, geom->ncoords);
        break;
    default:
        write_ring_points_wkt(wr, (struct tg_ring*)geom->line);
        break;
    }
    write_char(wr, ')');
}

static void write_geom_polygon_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "POLYGON");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    switch (tg_geom_dims(geom)) {
    case 3:
        write_poly_points_wkt_3(wr, geom->poly, 
            geom->coords, geom->ncoords);
        break;
    case 4:
        write_poly_points_wkt_4(wr, geom->poly, 
            geom->coords, geom->ncoords);
        break;
    default: // 2
        write_poly_points_wkt(wr, geom->poly);
        break;
    }
    write_char(wr, ')');
}

static void write_geom_multipoint_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "MULTIPOINT");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY || !geom->multi ||
        !geom->multi->ngeoms)
    {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    int dims = tg_geom_dims(geom);
    double z, m;
    double *coords = (double *)geom->coords;
    int ncoords = geom->ncoords;
    int j = 0;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        struct tg_point point = tg_geom_point(geom->multi->geoms[i]);
        if (i > 0) write_char(wr, ',');
        switch (dims) {
        case 3:
            z = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkt_3(wr, point, z);
            break;
        case 4:
            z = (j < ncoords) ? coords[j++] : 0;
            m = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkt_4(wr, point, z, m);
            break;
        default: // 2
            write_posn_wkt(wr, point);
            break;
        }
    }
    write_char(wr, ')');
}

static void write_geom_multilinestring_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "MULTILINESTRING");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY || !geom->multi ||
        !geom->multi->ngeoms)
    {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    double *pcoords = (double *)geom->coords;
    int ncoords = geom->ncoords;
    int n;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        const struct tg_line *line = tg_geom_line(
                geom->multi->geoms[i]);
        const struct tg_ring *ring = (struct tg_ring*)line;
        if (i > 0) write_char(wr, ',');
        write_char(wr, '(');
        switch (tg_geom_dims(geom)) {
        case 3:
            n = write_ring_points_wkt_3(wr, ring, pcoords, ncoords);
            ncoords -= n;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+n;
            break;
        case 4:
            n = write_ring_points_wkt_4(wr, ring, pcoords, ncoords);
            ncoords -= n*2;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
            break;
        default: // 2
            write_ring_points_wkt(wr, ring);
            break;
        }
        write_char(wr, ')');
    }
    write_char(wr, ')');
}

static void write_geom_multipolygon_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "MULTIPOLYGON");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY || !geom->multi ||
        !geom->multi->ngeoms)
    {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    double *pcoords = (double *)geom->coords;
    int ncoords = geom->ncoords;
    int n;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        const struct tg_poly *poly = tg_geom_poly(
                geom->multi->geoms[i]);
        if (i > 0) write_char(wr, ',');
        write_char(wr, '(');
        switch (tg_geom_dims(geom)) {
        case 3:
            n = write_poly_points_wkt_3(wr, poly, pcoords, ncoords);
            ncoords -= n;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+n;
            break;
        case 4:
            n = write_poly_points_wkt_4(wr, poly, pcoords, ncoords);
            ncoords -= n*2;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
            break;
        default: // 2
            write_poly_points_wkt(wr, poly);
            break;
        }
        write_char(wr, ')');
    }
    write_char(wr, ')');
}

static void write_geom_wkt(const struct tg_geom *geom, struct writer *wr);

static void write_geom_geometrycollection_wkt(const struct tg_geom *geom,
    struct writer *wr)
{
    write_string(wr, "GEOMETRYCOLLECTION");
    write_zm_def_wkt(wr, geom);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY || !geom->multi ||
        !geom->multi->ngeoms)
    {
        write_string(wr, " EMPTY");
        return;
    }
    write_char(wr, '(');
    int ngeoms = tg_geom_num_geometries(geom);
    for (int i = 0; i < ngeoms; i++) {
        if (i > 0) write_char(wr, ',');
        write_geom_wkt(tg_geom_geometry_at(geom, i), wr);
    }
    write_char(wr, ')');
}

static void write_base_geom_wkt(const struct tg_geom *geom, struct writer *wr) {
    switch (geom->head.type) {
    case TG_POINT:
        write_geom_point_wkt(geom, wr);
        break;
    case TG_LINESTRING:
        write_geom_linestring_wkt(geom, wr);
        break;
    case TG_POLYGON:
        write_geom_polygon_wkt(geom, wr);
        break;
    case TG_MULTIPOINT:
        write_geom_multipoint_wkt(geom, wr);
        break;
    case TG_MULTILINESTRING:
        write_geom_multilinestring_wkt(geom, wr);
        break;
    case TG_MULTIPOLYGON:
        write_geom_multipolygon_wkt(geom, wr);
        break;
    case TG_GEOMETRYCOLLECTION:
        write_geom_geometrycollection_wkt(geom, wr);
        break;
    }
}

static void write_geom_wkt(const struct tg_geom *geom, struct writer *wr) {
    switch (geom->head.base) {
    case BASE_GEOM:
        write_base_geom_wkt(geom, wr);
        break;
    case BASE_POINT:
        write_point_wkt((struct boxed_point*)geom, wr);
        break;
    case BASE_LINE:
        write_line_wkt((struct tg_line*)geom, wr);
        break;
    case BASE_RING:
        write_ring_wkt((struct tg_ring*)geom, wr);
        break;
    case BASE_POLY:
        write_poly_wkt((struct tg_poly*)geom, wr);
        break;
    }
}

/// Writes a Well-known text (WKT) representation of a geometry.
///
/// The content is stored as a C string in the buffer pointed to by dst.
/// A terminating null character is automatically appended after the
/// content written.
///
/// @param geom Input geometry
/// @param dst Buffer where the resulting content is stored.
/// @param n Maximum number of bytes to be used in the buffer.
/// @return  The number of characters, not including the null-terminator, 
/// needed to store the content into the C string buffer.
/// If the returned length is greater than n-1, then only a parital copy
/// occurred, for example:
///
/// ```
/// char str[64];
/// size_t len = tg_geom_wkt(geom, str, sizeof(str));
/// if (len > sizeof(str)-1) {
///     // ... write did not complete ...
/// }
/// ```
///
/// @see tg_geom_geojson()
/// @see tg_geom_wkb()
/// @see tg_geom_hex()
/// @see GeometryWriting
size_t tg_geom_wkt(const struct tg_geom *geom, char *dst, size_t n) {
    if (!geom) return 0;
    struct writer wr = { .dst = (uint8_t*)dst, .n = n };
    write_geom_wkt(geom, &wr);
    write_nullterm(&wr);
    return wr.count;
}

// wkb

static const char *wkb_invalid_child_type(void) {
    return "invalid child type";
}

static uint32_t read_uint32(const uint8_t *data, bool swap) {
    uint32_t x = (((uint32_t)data[0])<<0)|(((uint32_t)data[1])<<8)|
                 (((uint32_t)data[2])<<16)|(((uint32_t)data[3])<<24);
    return swap ? __builtin_bswap32(x) : x;
}

static uint64_t read_uint64(const uint8_t *data, bool swap) {
    uint64_t x = (((uint64_t)data[0])<<0)|(((uint64_t)data[1])<<8)|
                 (((uint64_t)data[2])<<16)|(((uint64_t)data[3])<<24)|
                 (((uint64_t)data[4])<<32)|(((uint64_t)data[5])<<40)|
                 (((uint64_t)data[6])<<48)|(((uint64_t)data[7])<<56);
    return swap ? __builtin_bswap64(x) : x;
}

static double read_double(const uint8_t *data, bool le) {
    return ((union raw_double){.u=read_uint64(data, le)}).d;
}

#define read_uint32(name) { \
    if (i+4 > len) goto invalid; \
    uint32_t _val_ = read_uint32(wkb+i, swap); \
    i += 4; \
    (name) = _val_; \
}

#define read_posn(posn) { \
    if (i+(8*dims) > len) goto invalid; \
    for (int j = 0; j < dims; j++) { \
        (posn)[j] = read_double(wkb+i, swap); \
        i += 8; \
    } \
}

static const char *wkb_invalid_err(void) {
    return "invalid binary";
}

#define PARSE_FAIL SIZE_MAX

// returns the updated wkb index.
static size_t parse_wkb_posns(enum base base, int dims, 
    const uint8_t *wkb, size_t len, size_t i, bool swap,
    struct dvec *posns, struct dvec *xcoords,
    struct tg_point **points, int *npoints,
    const char **err)
{
    *err = NULL;
    double posn[4];
    uint32_t count;
    read_uint32(count);
    if (count == 0) return i;
    if (dims == 2 && !swap && len-i >= count*2*8) {
        // Use the point data directly. No allocations. 
        *points = (void*)(wkb+i);
        *npoints = count;
        i += count*2*8; 
    } else {
        for (uint32_t j = 0 ; j < count; j++) {
            read_posn(posn);
            for (int i = 0; i < 2; i++) {
                if (!dvec_append(posns, posn[i])) {
                    return PARSE_FAIL;
                }
            }
            for (int i = 2; i < dims; i++) {
                if (!dvec_append(xcoords, posn[i])) {
                    return PARSE_FAIL;
                }
            }
        }
        *points = (void*)posns->data;
        *npoints = posns->len / 2;
    }
    if (!check_parse_posns(base, (void*)(*points), (*npoints) * 2, err)) {
        return PARSE_FAIL;
    }
    return i;
invalid:
    *err = wkb_invalid_err();
    return PARSE_FAIL;
}

static size_t parse_wkb_multi_posns(enum base base, int dims, 
    const uint8_t *wkb, size_t len, size_t i, bool swap, struct dvec *posns, 
    struct rvec *rings,  struct tg_poly **poly, struct dvec *xcoords, 
    enum tg_index ix, const char **err)
{
    *err = NULL;
    uint32_t count;
    read_uint32(count);
    if (count == 0) return i;
    for (uint32_t j = 0 ; j < count; j++) {
        struct tg_point *points = NULL;
        int npoints = 0;
        posns->len = 0;
        i = parse_wkb_posns(base, dims, wkb, len, i, swap, posns, xcoords, 
            &points, &npoints, err);
        if (i == PARSE_FAIL) return PARSE_FAIL;
        struct tg_ring *ring = tg_ring_new_ix(points, npoints, ix);
        if (!ring) return PARSE_FAIL;
        if (!rvec_append(rings, ring)) {
            tg_ring_free(ring);
            return PARSE_FAIL;
        }
    }
    *poly = tg_poly_new(rings->data[0], 
        (struct tg_ring const*const*)rings->data+1, rings->len-1);
    if (!*poly) return PARSE_FAIL;
    for (size_t i = 0; i < rings->len; i++) {
        tg_ring_free(rings->data[i]);
    }
    rings->len = 0;
    return i;
invalid:
    *err = wkb_invalid_err();
    return PARSE_FAIL;
}

static size_t parse_wkb_point(const uint8_t *wkb, size_t len, size_t i,
    bool swap, bool z, bool m, int depth, enum tg_index ix, 
    struct tg_geom **gout)
{
    (void)depth; (void)ix;
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;
    double posn[4];
    read_posn(posn);
    struct tg_geom *geom = NULL;
    if (isnan(posn[0])) {
        bool empty = true;
        for (int i = 1; i < dims; i++) {
            if (!isnan(posn[i])) {
                empty = false;
                break;
            }
        }
        if (empty) {
            geom = tg_geom_new_point_empty();
            *gout = geom;
            return i;
        }
    }
    struct tg_point pt = { posn[0], posn[1] };
    if (dims == 2) {
        geom = tg_geom_new_point(pt);
    } else if (dims == 3) {
        if (m) {
            geom = tg_geom_new_point_m(pt, posn[2]);
        } else {
            geom = tg_geom_new_point_z(pt, posn[2]);
        }
    } else {
        geom = tg_geom_new_point_zm(pt, posn[2], posn[3]);
    }
    *gout = geom;
    return i;
invalid:
    *gout = make_parse_error(wkb_invalid_err());
    return PARSE_FAIL;
}

static size_t parse_wkb_linestring(const uint8_t *wkb, size_t len, size_t i, 
    bool swap, bool z, bool m, int depth, enum tg_index ix, 
    struct tg_geom **gout)
{
    (void)depth;
    struct tg_geom *gerr = NULL;
    struct tg_geom *geom = NULL;
    struct tg_line *line = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct tg_point *points = NULL;
    int npoints = 0;
    const char *err = NULL;
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;
    i = parse_wkb_posns(BASE_LINE, dims, wkb, len, i, swap, &posns, &xcoords, 
        &points, &npoints, &err);
    if (i == PARSE_FAIL) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    if (npoints == 0) {
        geom = tg_geom_new_linestring_empty();
        goto cleanup;
    }
    line = tg_line_new_ix(points, npoints, ix);
    if (!line) goto fail;
    switch (dims) {
    case 2: 
        geom = tg_geom_new_linestring(line);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_linestring_m(line, xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_linestring_z(line, xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_linestring_zm(line, xcoords.data, xcoords.len);
        break;
    }
cleanup:
    tg_line_free(line);
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static size_t parse_wkb_polygon(const uint8_t *wkb, size_t len,
    size_t i, bool swap, bool z, bool m, int depth, enum tg_index ix,
    struct tg_geom **gout)
{
    (void)depth;
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct tg_poly *poly = NULL;
    struct rvec rings = { 0 };
    const char *err = NULL;
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;
    i = parse_wkb_multi_posns(BASE_RING, dims, wkb, len, i, swap, &posns, 
        &rings, &poly, &xcoords, ix, &err);
    if (i == PARSE_FAIL) {
        gerr = err ? make_parse_error("%s", err) : NULL;
        goto fail;
    }
    if (!poly) {
        geom = tg_geom_new_polygon_empty();
        goto cleanup;
    }
    switch (dims) {
    case 2: 
        geom = tg_geom_new_polygon(poly);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_polygon_m(poly, xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_polygon_z(poly, xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_polygon_zm(poly, xcoords.data, xcoords.len);
        break;
    }
cleanup:
    tg_poly_free(poly);
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    if (rings.data) {
        for (size_t i = 0; i < rings.len; i++) {
            tg_ring_free(rings.data[i]);
        }
        tg_free(rings.data);
    }
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
}

static size_t parse_wkb(const uint8_t *wkb, size_t len, size_t i, int depth,
    enum tg_index ix, struct tg_geom **g);

static bool wkb_type_match(const struct tg_geom *child, enum tg_geom_type type, 
    bool z, bool m)
{
    bool child_has_z = (child->head.flags&HAS_Z) == HAS_Z;
    bool child_has_m = (child->head.flags&HAS_M) == HAS_M;
    return child->head.type == type && child_has_z == z && child_has_m == m;
}

static size_t parse_wkb_multipoint(const uint8_t *wkb, size_t len, size_t i,
    bool swap, bool z, bool m, int depth, enum tg_index ix, 
    struct tg_geom **gout)
{
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;

    struct dvec posns = { 0 };
    struct dvec xcoords = { 0 };
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct tg_geom *point = NULL;

    uint32_t count;
    read_uint32(count);
    for (size_t j = 0; j < count; j++) {
        i = parse_wkb(wkb, len, i, depth+1, ix, &point);
        if (!point || i == PARSE_FAIL ||  tg_geom_error(point)) {
            gerr = point;
            point = NULL;
            goto fail;
        }
        if (!wkb_type_match(point, TG_POINT, z, m)) {
            gerr = make_parse_error(wkb_invalid_child_type());
            goto fail;
        }
        struct tg_point pt = tg_geom_point(point);
        if (!dvec_append(&posns, pt.x) || !dvec_append(&posns, pt.y)) goto fail;
        if (z || m) {
            if (z && m) {
                if (!dvec_append(&xcoords, tg_geom_z(point))||
                    !dvec_append(&xcoords, tg_geom_m(point))) {
                        goto fail;
                    }
            } else if (z) {
                if (!dvec_append(&xcoords, tg_geom_z(point))) goto fail;
            } else if (m) {
                if (!dvec_append(&xcoords, tg_geom_m(point))) goto fail;
            }
        }
        tg_geom_free(point);
        point = NULL;
    }

    const struct tg_point *points = (struct tg_point*)posns.data;
    int npoints = posns.len/2;
    switch (dims) {
    case 2: 
        geom = tg_geom_new_multipoint(points, npoints);
        break;
    case 3: 
        if (m) {
            geom = tg_geom_new_multipoint_m(points, npoints, 
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multipoint_z(points, npoints, 
                xcoords.data, xcoords.len);
        }
        break;
    default: 
        geom = tg_geom_new_multipoint_zm(points, npoints, 
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (point) tg_geom_free(point);
    if (posns.data) tg_free(posns.data);
    if (xcoords.data) tg_free(xcoords.data);
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
invalid:
    gerr = make_parse_error(wkb_invalid_err());
    goto fail;
}

static size_t parse_wkb_multilinestring(const uint8_t *wkb, size_t len,
    size_t i, bool swap, bool z, bool m, int depth, enum tg_index ix,
    struct tg_geom **gout)
{
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;

    struct dvec xcoords = { 0 };
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct tg_geom *child = NULL;
    struct lvec lines = { 0 };

    uint32_t count;
    read_uint32(count);
    for (size_t j = 0; j < count; j++) {
        i = parse_wkb(wkb, len, i, depth+1, ix, &child);
        if (!child || i == PARSE_FAIL || tg_geom_error(child)) {
            gerr = child;
            child = NULL;
            goto fail;
        }
        if (!wkb_type_match(child, TG_LINESTRING, z, m)) {
            gerr = make_parse_error(wkb_invalid_child_type());
            goto fail;
        }
        struct tg_line *line = tg_line_clone(tg_geom_line(child));
        if (!lvec_append(&lines, line)) {
            tg_line_free(line);
            goto fail;
        }
        const double *coords = tg_geom_extra_coords(child);
        int ncoords = tg_geom_num_extra_coords(child);
        for (int i = 0; i < ncoords; i++) {
            if (!dvec_append(&xcoords, coords[i])) goto fail;
        }
        tg_geom_free(child);
        child = NULL;
    }
    switch (dims) {
    case 2:
        geom = tg_geom_new_multilinestring(
                (struct tg_line const*const*)lines.data, lines.len);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_multilinestring_m(
                (struct tg_line const*const*)lines.data, lines.len,
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multilinestring_z(
                (struct tg_line const*const*)lines.data, lines.len,
                xcoords.data, xcoords.len);
        }
        break;
    default:
        geom = tg_geom_new_multilinestring_zm(
            (struct tg_line const*const*)lines.data, lines.len,
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (child) tg_geom_free(child);
    if (xcoords.data) tg_free(xcoords.data);
    if (lines.data) {
        for (size_t i = 0; i < lines.len; i++) {
            tg_line_free(lines.data[i]);
        }
        tg_free(lines.data);
    }
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
invalid:
    gerr = make_parse_error(wkb_invalid_err());
    goto fail;
}

static size_t parse_wkb_multipolygon(const uint8_t *wkb, size_t len,
    size_t i, bool swap, bool z, bool m, int depth, enum tg_index ix, 
    struct tg_geom **gout)
{
    int dims = z ? m ? 4 : 3 : m ? 3 : 2;

    struct dvec xcoords = { 0 };
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct tg_geom *child = NULL;
    struct pvec polys = { 0 };

    uint32_t count;
    read_uint32(count);
    for (size_t j = 0; j < count; j++) {
        i = parse_wkb(wkb, len, i, depth+1, ix, &child);
        if (!child || i == PARSE_FAIL || tg_geom_error(child)) {
            gerr = child;
            child = NULL;
            goto fail;
        }
        if (!wkb_type_match(child, TG_POLYGON, z, m)) {
            gerr = make_parse_error(wkb_invalid_child_type());
            goto fail;
        }
        struct tg_poly *poly = tg_poly_clone(tg_geom_poly(child));
        if (!pvec_append(&polys, poly)) {
            tg_poly_free(poly);
            goto fail;
        }
        const double *coords = tg_geom_extra_coords(child);
        int ncoords = tg_geom_num_extra_coords(child);
        for (int i = 0; i < ncoords; i++) {
            if (!dvec_append(&xcoords, coords[i])) goto fail;
        }
        tg_geom_free(child);
        child = NULL;
    }
    switch (dims) {
    case 2:
        geom = tg_geom_new_multipolygon(
                (struct tg_poly const*const*)polys.data, polys.len);
        break;
    case 3:
        if (m) {
            geom = tg_geom_new_multipolygon_m(
                (struct tg_poly const*const*)polys.data, polys.len,
                xcoords.data, xcoords.len);
        } else {
            geom = tg_geom_new_multipolygon_z(
                (struct tg_poly const*const*)polys.data, polys.len,
                xcoords.data, xcoords.len);
        }
        break;
    default:
        geom = tg_geom_new_multipolygon_zm(
            (struct tg_poly const*const*)polys.data, polys.len,
            xcoords.data, xcoords.len);
        break;
    }
cleanup:
    if (child) tg_geom_free(child);
    if (xcoords.data) tg_free(xcoords.data);
    if (polys.data) {
        for (size_t i = 0; i < polys.len; i++) {
            tg_poly_free(polys.data[i]);
        }
        tg_free(polys.data);
    }
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
invalid:
    gerr = make_parse_error(wkb_invalid_err());
    goto fail;
}

static size_t parse_wkb_geometrycollection(const uint8_t *wkb, size_t len, 
    size_t i, bool swap, bool z, bool m, int depth, enum tg_index ix,
    struct tg_geom **gout)
{
    (void)z; (void)m; // not used
    struct tg_geom *geom = NULL;
    struct tg_geom *gerr = NULL;
    struct gvec geoms = { 0 };

    uint32_t count;
    read_uint32(count);
    for (size_t j = 0; j < count; j++) {
        struct tg_geom *child = NULL;
        i = parse_wkb(wkb, len, i, depth+1, ix, &child);
        if (!child || i == PARSE_FAIL || tg_geom_error(child)) {
            gerr = child;
            goto fail;
        }
        if (!gvec_append(&geoms, child)) {
            tg_geom_free(child);
            goto fail;
        }
    }
    geom = tg_geom_new_geometrycollection(
        (struct tg_geom const*const*)geoms.data, geoms.len);
cleanup:
    if (geoms.data) {
        for (size_t i = 0; i < geoms.len; i++) {
            tg_geom_free(geoms.data[i]);
        }
        tg_free(geoms.data);
    }
    *gout = geom;
    return i;
fail:
    tg_geom_free(geom);
    geom = gerr;
    gerr = NULL;
    goto cleanup;
invalid:
    gerr = make_parse_error(wkb_invalid_err());
    goto fail;
}

static size_t parse_wkb(const uint8_t *wkb, size_t len, size_t i, int depth,
    enum tg_index ix, struct tg_geom **g)
{
    if (i == len) goto invalid;
    if (wkb[i] >> 1) goto invalid; // not 1 or 0
    
    int d = depth;
    if (d > MAXDEPTH) goto invalid;

    // Set the 'swap' bool which indicates that the wkb numbers need swapping
    // to match the host endianness.
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    bool swap = wkb[i] == 1;
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    bool swap = wkb[i] == 0;
#else
    #error "cannot determine byte order"
#endif
    i++;

    uint32_t type;
    read_uint32(type);

    bool has_srid = !!(type & 0x20000000);
    type &= 0xFFFF;
    int srid = 0;
    if (has_srid) {
        // Read the SRID from the extended wkb format.
        uint32_t usrid;
        read_uint32(usrid);
        srid = (int)usrid;
    }
    (void)srid; // Now throw it away.

    bool s = swap;
    switch (type) {
    case    1: return parse_wkb_point(wkb, len, i, s, 0, 0, d, ix, g);
    case 1001: return parse_wkb_point(wkb, len, i, s, 1, 0, d, ix, g);
    case 2001: return parse_wkb_point(wkb, len, i, s, 0, 1, d, ix, g);
    case 3001: return parse_wkb_point(wkb, len, i, s, 1, 1, d, ix, g);
    case    2: return parse_wkb_linestring(wkb, len, i, s, 0, 0, d, ix, g);
    case 1002: return parse_wkb_linestring(wkb, len, i, s, 1, 0, d, ix, g);
    case 2002: return parse_wkb_linestring(wkb, len, i, s, 0, 1, d, ix, g);
    case 3002: return parse_wkb_linestring(wkb, len, i, s, 1, 1, d, ix, g);
    case    3: return parse_wkb_polygon(wkb, len, i, s, 0, 0, d, ix, g); 
    case 1003: return parse_wkb_polygon(wkb, len, i, s, 1, 0, d, ix, g); 
    case 2003: return parse_wkb_polygon(wkb, len, i, s, 0, 1, d, ix, g); 
    case 3003: return parse_wkb_polygon(wkb, len, i, s, 1, 1, d, ix, g); 
    case    4: return parse_wkb_multipoint(wkb, len, i, s, 0, 0, d, ix, g);
    case 1004: return parse_wkb_multipoint(wkb, len, i, s, 1, 0, d, ix, g);
    case 2004: return parse_wkb_multipoint(wkb, len, i, s, 0, 1, d, ix, g);
    case 3004: return parse_wkb_multipoint(wkb, len, i, s, 1, 1, d, ix, g);
    case    5: return parse_wkb_multilinestring(wkb, len, i, s, 0, 0, d, ix, g);
    case 1005: return parse_wkb_multilinestring(wkb, len, i, s, 1, 0, d, ix, g);
    case 2005: return parse_wkb_multilinestring(wkb, len, i, s, 0, 1, d, ix, g);
    case 3005: return parse_wkb_multilinestring(wkb, len, i, s, 1, 1, d, ix, g);
    case    6: return parse_wkb_multipolygon(wkb, len, i, s, 0, 0, d, ix, g);
    case 1006: return parse_wkb_multipolygon(wkb, len, i, s, 1, 0, d, ix, g);
    case 2006: return parse_wkb_multipolygon(wkb, len, i, s, 0, 1, d, ix, g);
    case 3006: return parse_wkb_multipolygon(wkb, len, i, s, 1, 1, d, ix, g);
    case    7: case 1007: case 2007: case 3007: 
        return parse_wkb_geometrycollection(wkb, len, i, s, 0, 0, d, ix, g);
    default: 
        *g = make_parse_error("invalid type");
        return PARSE_FAIL;
    }
invalid:
    *g = make_parse_error("invalid binary");
    return PARSE_FAIL;
}

/// Parse Well-known binary (WKB).
/// @param wkb WKB data
/// @param len Length of data
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wkb_ix()
/// @see tg_geom_error()
/// @see tg_geom_wkb()
/// @see GeometryParsing
struct tg_geom *tg_parse_wkb(const uint8_t *wkb, size_t len) {
    return tg_parse_wkb_ix(wkb, len, 0);
}

/// Parse Well-known binary (WKB) using provided indexing option.
/// @param wkb WKB data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_wkb()
struct tg_geom *tg_parse_wkb_ix(const uint8_t *wkb, size_t len, 
    enum tg_index ix)
{
    struct tg_geom *geom = NULL;
    parse_wkb(wkb, len, 0, 0, ix, &geom);
    if (!geom) return NULL;
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        struct tg_geom *gerr = make_parse_error("ParseError: %s", geom->error);
        tg_geom_free(geom);
        return gerr;
    }
    return geom;
}

static void write_wkb_type(struct writer *wr, const struct head *head) {
    uint32_t type = head->type;
    if ((head->flags&HAS_Z) == HAS_Z) {
        if ((head->flags&HAS_M) == HAS_M) {
            type += 3000;
        } else {
            type += 1000;
        }
    } else if ((head->flags&HAS_M) == HAS_M) {
        type += 2000;
    }
    write_byte(wr, 1);
    write_uint32le(wr, type);
}

static void write_posn_wkb(struct writer *wr, struct tg_point posn) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (wr->count+16 < wr->n) {
        memcpy(wr->dst+wr->count, &posn, 16);
        wr->count += 16;
        return;
    }
#endif
    write_doublele(wr, posn.x);
    write_doublele(wr, posn.y);
}

static void write_posn_wkb_3(struct writer *wr, struct tg_point posn, double z)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (wr->count+24 < wr->n) {
        memcpy(wr->dst+wr->count, ((double[3]){posn.x, posn.y, z}), 24);
        wr->count += 24;
        return;
    }
#endif
    write_doublele(wr, posn.x);
    write_doublele(wr, posn.y);
    write_doublele(wr, z);
}

static void write_posn_wkb_4(struct writer *wr, struct tg_point posn, 
    double z, double m)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (wr->count+32 < wr->n) {
        memcpy(wr->dst+wr->count, ((double[4]){posn.x, posn.y, z, m}), 32);
        wr->count += 32;
        return;
    }
#endif
    write_doublele(wr, posn.x);
    write_doublele(wr, posn.y);
    write_doublele(wr, z);
    write_doublele(wr, m);
}

static int write_ring_points_wkb(struct writer *wr, const struct tg_ring *ring)
{
    write_uint32le(wr, ring->npoints);
    size_t needed = ring->npoints*16;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if (wr->count+needed <= wr->n) {
        memcpy(wr->dst+wr->count, ring->points, needed);
        wr->count += needed;
        return ring->npoints;
    }
#endif
    if (wr->count >= wr->n) {
        wr->count += needed;
    } else {
        for (int i = 0 ; i < ring->npoints; i++) {
            write_posn_wkb(wr, ring->points[i]);
        }
    }
    return ring->npoints;
}

static int write_ring_points_wkb_3(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    write_uint32le(wr, ring->npoints);
    size_t needed = ring->npoints*24;
    if (wr->count >= wr->n) {
        wr->count += needed;
    } else {
        double z;
        int j = 0;
        for (int i = 0 ; i < ring->npoints; i++) {
            z = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkb_3(wr, ring->points[i], z);
        }
    }
    return ring->npoints;
}

static int write_ring_points_wkb_4(struct writer *wr, 
    const struct tg_ring *ring, const double *coords, int ncoords)
{
    write_uint32le(wr, ring->npoints);
    size_t needed = ring->npoints*32;
    if (wr->count >= wr->n) {
        wr->count += needed;
    } else {
        double z, m;
        int j = 0;
        for (int i = 0 ; i < ring->npoints; i++) {
            z = (j < ncoords) ? coords[j++] : 0;
            m = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkb_4(wr, ring->points[i], z, m);
        }
    }
    return ring->npoints;
}

static int write_poly_points_wkb(struct writer *wr, 
    const struct tg_poly *poly)
{
    int count = 0;
    int nholes = tg_poly_num_holes(poly);
    write_uint32le(wr, 1+nholes);
    write_ring_points_wkb(wr, tg_poly_exterior(poly));
    for (int i = 0 ; i < nholes; i++) {
        count += write_ring_points_wkb(wr, tg_poly_hole_at(poly, i));
    }    
    return count;
}

static int write_poly_points_wkb_3(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    int nholes = tg_poly_num_holes(poly);
    write_uint32le(wr, 1+nholes);
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    int n = write_ring_points_wkb_3(wr, exterior, pcoords, ncoords);
    count += n;
    ncoords -= n;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+n;
    for (int i = 0 ; i < nholes; i++) {
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        int n = write_ring_points_wkb_3(wr, hole, pcoords, ncoords);
        count += n;
        ncoords -= n;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+n;
    }
    return count;
}

static int write_poly_points_wkb_4(struct writer *wr, 
    const struct tg_poly *poly, const double *coords, int ncoords)
{
    int count = 0;
    double *pcoords = (double*)coords;
    int nholes = tg_poly_num_holes(poly);
    write_uint32le(wr, 1+nholes);
    const struct tg_ring *exterior = tg_poly_exterior(poly);
    int n = write_ring_points_wkb_4(wr, exterior, pcoords, ncoords);
    count += n;
    ncoords -= n*2;
    if (ncoords < 0) ncoords = 0;
    pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    for (int i = 0 ; i < nholes; i++) {
        const struct tg_ring *hole = tg_poly_hole_at(poly, i);
        int n = write_ring_points_wkb_4(wr, hole, pcoords, ncoords);
        count += n;
        ncoords -= n*2;
        if (ncoords < 0) ncoords = 0;
        pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
    }
    return count;
}

static void write_point_wkb(struct boxed_point *point, struct writer *wr) {
    write_wkb_type(wr, &point->head);
    write_posn_wkb(wr, point->point);
}

static void write_line_wkb(struct tg_line *line, struct writer *wr) {
    struct tg_ring *ring = (struct tg_ring*)line;
    write_wkb_type(wr, &ring->head);
    write_ring_points_wkb(wr, ring);
}

static void write_ring_wkb(struct tg_ring *ring, struct writer *wr) {
    (void)ring; (void)wr;
    write_wkb_type(wr, &ring->head);
    write_uint32le(wr, 1);
    write_ring_points_wkb(wr, ring);
}

static void write_poly_wkb(struct tg_poly *poly, struct writer *wr) {
    (void)poly; (void)wr;
    write_wkb_type(wr, &poly->head);
    write_poly_points_wkb(wr, poly);
}

static void write_geom_point_wkb(const struct tg_geom *geom, struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_posn_wkb(wr, (struct tg_point){ NAN, NAN });
    } else {
        if ((geom->head.flags&HAS_Z) == HAS_Z) {
            if ((geom->head.flags&HAS_M) == HAS_M) {
                write_posn_wkb_4(wr, geom->point, geom->z, geom->m);
            } else {
                write_posn_wkb_3(wr, geom->point, geom->z);
            }
        } else if ((geom->head.flags&HAS_M) == HAS_M) {
            write_posn_wkb_3(wr, geom->point, geom->m);
        } else {
            write_posn_wkb(wr, geom->point);
        }
    }
}

static void write_geom_linestring_wkb(const struct tg_geom *geom,
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    switch (tg_geom_dims(geom)) {
    case 3:
        write_ring_points_wkb_3(wr, (struct tg_ring*)geom->line, 
            geom->coords, geom->ncoords);
        break;
    case 4:
        write_ring_points_wkb_4(wr, (struct tg_ring*)geom->line, 
            geom->coords, geom->ncoords);
        break;
    default:
        write_ring_points_wkb(wr, (struct tg_ring*)geom->line);
        break;
    }
}

static void write_geom_polygon_wkb(const struct tg_geom *geom,
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if ((geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    switch (tg_geom_dims(geom)) {
    case 3:
        write_poly_points_wkb_3(wr, geom->poly, 
            geom->coords, geom->ncoords);
        break;
    case 4:
        write_poly_points_wkb_4(wr, geom->poly, 
            geom->coords, geom->ncoords);
        break;
    default: // 2
        write_poly_points_wkb(wr, geom->poly);
        break;
    }
}

static void write_geom_multipoint_wkb(const struct tg_geom *geom,
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if (!geom->multi || (geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    int dims = tg_geom_dims(geom);
    double z, m;
    double *coords = (double *)geom->coords;
    int ncoords = geom->ncoords;
    int j = 0;
    write_uint32le(wr, geom->multi->ngeoms);
    struct head head = { 
        .type = TG_POINT, 
        .flags = (geom->head.flags&(HAS_Z|HAS_M)),
    };
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        struct tg_point point = tg_geom_point(geom->multi->geoms[i]);
        write_wkb_type(wr, &head);
        switch (dims) {
        case 3:
            z = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkb_3(wr, point, z);
            break;
        case 4:
            z = (j < ncoords) ? coords[j++] : 0;
            m = (j < ncoords) ? coords[j++] : 0;
            write_posn_wkb_4(wr, point, z, m);
            break;
        default: // 2
            write_posn_wkb(wr, point);
            break;
        }
    }
}

static void write_geom_multilinestring_wkb(const struct tg_geom *geom, 
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if (!geom->multi || (geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    int dims = tg_geom_dims(geom);
    write_uint32le(wr, geom->multi->ngeoms);
    struct head head = { 
        .type = TG_LINESTRING, 
        .flags = (geom->head.flags&(HAS_Z|HAS_M)),
    };
    const double *pcoords = geom->coords;
    int ncoords = geom->ncoords;
    int n;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        const struct tg_line *line = tg_geom_line(geom->multi->geoms[i]);
        const struct tg_ring *ring = (struct tg_ring*)line;
        write_wkb_type(wr, &head);
        switch (dims) {
        case 3:
            n = write_ring_points_wkb_3(wr, ring, pcoords, ncoords);
            ncoords -= n;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+n;
            break;
        case 4:
            n = write_ring_points_wkb_4(wr, ring, pcoords, ncoords);
            ncoords -= n*2;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
            break;
        default: // 2
            write_ring_points_wkb(wr, ring);
            break;
        }
    }
}

static void write_geom_multipolygon_wkb(const struct tg_geom *geom,
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if (!geom->multi || (geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    int dims = tg_geom_dims(geom);
    write_uint32le(wr, geom->multi->ngeoms);
    struct head head = { 
        .type = TG_POLYGON, 
        .flags = (geom->head.flags&(HAS_Z|HAS_M)),
    };
    const double *pcoords = geom->coords;
    int ncoords = geom->ncoords;
    int n;
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        const struct tg_poly *poly = tg_geom_poly(geom->multi->geoms[i]);
        write_wkb_type(wr, &head);
        switch (dims) {
        case 3:
            n = write_poly_points_wkb_3(wr, poly, pcoords, ncoords);
            ncoords -= n;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+n;
            break;
        case 4:
            n = write_poly_points_wkb_4(wr, poly, pcoords, ncoords);
            ncoords -= n*2;
            if (ncoords < 0) ncoords = 0;
            pcoords = ncoords == 0 ? NULL : pcoords+(n*2);
            break;
        default: // 2
            write_poly_points_wkb(wr, poly);
            break;
        }
    }
}

static void write_geom_wkb(const struct tg_geom *geom, struct writer *wr);

static void write_geom_geometrycollection_wkb(const struct tg_geom *geom, 
    struct writer *wr)
{
    write_wkb_type(wr, &geom->head);
    if (!geom->multi || (geom->head.flags&IS_EMPTY) == IS_EMPTY) {
        write_uint32le(wr, 0);
        return;
    }
    write_uint32le(wr, geom->multi->ngeoms);
    for (int i = 0; i < geom->multi->ngeoms; i++) {
        write_geom_wkb(geom->multi->geoms[i], wr);
    }
}

static void write_base_geom_wkb(const struct tg_geom *geom, struct writer *wr) {
    switch (geom->head.type) {
    case TG_POINT:
        write_geom_point_wkb(geom, wr);
        break;
    case TG_LINESTRING:
        write_geom_linestring_wkb(geom, wr);
        break;
    case TG_POLYGON:
        write_geom_polygon_wkb(geom, wr);
        break;
    case TG_MULTIPOINT:
        write_geom_multipoint_wkb(geom, wr);
        break;
    case TG_MULTILINESTRING:
        write_geom_multilinestring_wkb(geom, wr);
        break;
    case TG_MULTIPOLYGON:
        write_geom_multipolygon_wkb(geom, wr);
        break;
    case TG_GEOMETRYCOLLECTION:
        write_geom_geometrycollection_wkb(geom, wr);
        break;
    }
}

static void write_geom_wkb(const struct tg_geom *geom, struct writer *wr) {
    switch (geom->head.base) {
    case BASE_GEOM:
        write_base_geom_wkb(geom, wr);
        break;
    case BASE_POINT:
        write_point_wkb((struct boxed_point*)geom, wr);
        break;
    case BASE_LINE:
        write_line_wkb((struct tg_line*)geom, wr);
        break;
    case BASE_RING:
        write_ring_wkb((struct tg_ring*)geom, wr);
        break;
    case BASE_POLY:
        write_poly_wkb((struct tg_poly*)geom, wr);
        break;
    }
}

/// Writes a Well-known binary (WKB) representation of a geometry.
///
/// The content is stored in the buffer pointed by dst.
///
/// @param geom Input geometry
/// @param dst Buffer where the resulting content is stored.
/// @param n Maximum number of bytes to be used in the buffer.
/// @return  The number of characters needed to store the content into the
/// buffer.
/// If the returned length is greater than n, then only a parital copy
/// occurred, for example:
///
/// ```
/// uint8_t buf[64];
/// size_t len = tg_geom_wkb(geom, buf, sizeof(buf));
/// if (len > sizeof(buf)) {
///     // ... write did not complete ...
/// }
/// ```
///
/// @see tg_geom_geojson()
/// @see tg_geom_wkt()
/// @see tg_geom_hex()
/// @see GeometryWriting
size_t tg_geom_wkb(const struct tg_geom *geom, uint8_t *dst, size_t n) {
    if (!geom) return 0;
    struct writer wr = { .dst = dst, .n = n };
    write_geom_wkb(geom, &wr);
    return wr.count;
}

/// Writes a hex encoded Well-known binary (WKB) representation of a geometry.
///
/// The content is stored as a C string in the buffer pointed to by dst.
/// A terminating null character is automatically appended after the
/// content written.
///
/// @param geom Input geometry
/// @param dst Buffer where the resulting content is stored.
/// @param n Maximum number of bytes to be used in the buffer.
/// @return  The number of characters, not including the null-terminator, 
/// needed to store the content into the C string buffer.
/// If the returned length is greater than n-1, then only a parital copy
/// occurred, for example:
///
/// ```
/// char str[64];
/// size_t len = tg_geom_hex(geom, str, sizeof(str));
/// if (len > sizeof(str)-1) {
///     // ... write did not complete ...
/// }
/// ```
///
/// @see tg_geom_geojson()
/// @see tg_geom_wkt()
/// @see tg_geom_wkb()
/// @see GeometryWriting
size_t tg_geom_hex(const struct tg_geom *geom, char *dst, size_t n) {
    // Geom to hex is done by first writing as wkb, then rewrite as hex.
    // This is done by scanning the wkb in reverse, overwriting the data
    // along the way.
    static const uint8_t hexchars[] = "0123456789ABCDEF";
    size_t count = tg_geom_wkb(geom, (uint8_t*)dst, n);
    if (count == 0) {
        if (n > 0) dst[0] = '\0';
        return 0;
    }
    size_t i = count - 1;
    size_t j = count*2 - 1;
    while (1) {
        if (i < n) {
            uint8_t ch = dst[i];
            if (j < n) dst[j] = hexchars[ch&15];
            if (j-1 < n) dst[j-1] = hexchars[(ch>>4)&15];
        }
        if (i == 0) break;
        i -= 1;
        j -= 2;
    }
    if (count*2 < n) dst[count*2] = '\0';
    else if (n > 0) dst[n-1] = '\0';
    return count*2;
}

static size_t parse_geobin(const uint8_t *geobin, size_t len, size_t i, 
    size_t depth, enum tg_index ix, struct tg_geom **g);

static struct tg_geom *parse_hex(const char *hex, size_t len, enum tg_index ix)
{
    const uint8_t _ = 0;
    static const uint8_t hextoks[256] = { 
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,01,2,3,4,5,6,7,8,9,10,_,_,_,_,_,
        _,_,11,12,13,14,15,16,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
        _,_,_,_,_,11,12,13,14,15,16,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,_,
    };
    uint8_t *dst = NULL;
    bool must_free = false;
    if (len == 0 || (len&1) == 1) goto invalid;
    uint8_t smallfry[128];
    if (len/2 > sizeof(smallfry)) {
        dst = tg_malloc(len/2);
        if (!dst) return NULL;
        must_free = true;
    } else {
        dst = smallfry;
    }
    size_t j = 0;
    for (size_t i = 0; i < len; i += 2) {
        uint8_t b0 = hextoks[(uint8_t)hex[i+0]];
        uint8_t b1 = hextoks[(uint8_t)hex[i+1]];
        if (b0 == _ || b1 == _) goto invalid;
        dst[j] = ((b0-1)<<4)|(b1-1);
        j++;
    }
    struct tg_geom *geom;
    len /= 2;
    size_t n;
    if (len > 0 && dst[0] >= 0x2 && dst[0] <= 0x4) {
        n = parse_geobin(dst, len, 0, 0, ix, &geom);
    } else {
        n = parse_wkb(dst, len, 0, 0, ix, &geom);
    }
    (void)n;
    if (must_free) tg_free(dst);
    return geom;
invalid:
    if (must_free) tg_free(dst);
    return make_parse_error(wkb_invalid_err());
}

/// Parse hex encoded Well-known binary (WKB) or GeoBIN using provided indexing
/// option.
/// @param hex Hex data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_hex()
/// @see tg_parse_hex_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_hexn_ix(const char *hex, size_t len, 
    enum tg_index ix)
{
    struct tg_geom *geom = parse_hex(hex, len, ix);
    if (!geom) return NULL;
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        struct tg_geom *gerr = make_parse_error("ParseError: %s", geom->error);
        tg_geom_free(geom);
        return gerr;
    }
    return geom;
}

/// Parse hex encoded Well-known binary (WKB) or GeoBIN using provided indexing
/// option.
/// @param hex Hex string. Must be null-terminated
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_hex()
/// @see tg_parse_hexn_ix()
/// @see GeometryParsing
struct tg_geom *tg_parse_hex_ix(const char *hex, enum tg_index ix) {
    return tg_parse_hexn_ix(hex, hex?strlen(hex):0, ix);
}

/// Parse hex encoded Well-known binary (WKB) or GeoBIN with an included data 
/// length.
/// @param hex Hex data
/// @param len Length of data
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_hex()
/// @see GeometryParsing
struct tg_geom *tg_parse_hexn(const char *hex, size_t len) {
    return tg_parse_hexn_ix(hex, len, TG_DEFAULT);
}

/// Parse hex encoded Well-known binary (WKB) or GeoBIN.
/// @param hex A hex string. Must be null-terminated
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_hexn()
/// @see tg_parse_hex_ix()
/// @see tg_parse_hexn_ix()
/// @see tg_geom_error()
/// @see tg_geom_hex()
/// @see GeometryParsing
struct tg_geom *tg_parse_hex(const char *hex) {
    return tg_parse_hexn_ix(hex, hex?strlen(hex):0, TG_DEFAULT);
}

/// Calculate the area of a ring.
double tg_ring_area(const struct tg_ring *ring) {
    if (tg_ring_empty(ring)) return 0;
    // The ring area has already been calculated by process_points.
    return ring->area;
}

/// Calculate the perimeter length of a ring.
double tg_ring_perimeter(const struct tg_ring *ring) {
    if (tg_ring_empty(ring)) return 0;
    int nsegs = tg_ring_num_segments(ring);
    double perim = 0;
    for (int i = 0; i < nsegs; i++) {
        struct tg_point a = ring->points[i];
        struct tg_point b = ring->points[i+1];
        perim += length(a.x, a.y, b.x, b.y);
    }
    return perim;
}

double tg_ring_polsby_popper_score(const struct tg_ring *ring) {
    // Calculate the polsby-popper score for the ring or line
    // https://en.wikipedia.org/wiki/Polsby–Popper_test 
    //
    // The score is calculated by multiplying the polygon area by 4pi
    // and dividing by the perimeter squared. A perfect circle has a score of 1
    // and all other shapes will be smaller. Itty bitty scores mean the
    // polygon is really something nuts or has bad data.
    double score = 0.0;
    double perim = tg_ring_perimeter(ring);
    double area = tg_ring_area(ring);
    if (perim > 0) {
        score = (area * M_PI * 4) / (perim * perim);
    }
    return score;
}

double tg_line_polsby_popper_score(const struct tg_line *line) {
    const struct tg_ring *ring = (const struct tg_ring*)line;
    return tg_ring_polsby_popper_score(ring);    
}

struct tg_ring *tg_circle_new_ix(struct tg_point center, double radius, 
    int steps, enum tg_index ix)
{
    steps--; 
    radius = radius < 0 ? 0 : radius;
    steps = steps < 3 ? 3 : steps;
    struct tg_ring *ring = NULL;
    struct tg_point *points = tg_malloc(sizeof(struct tg_point)*(steps+1));
    if (points) {
        int i = 0;
        for (double th = 0.0; th <= 360.0; th += 360.0 / (double)steps) {
            double radians = (M_PI / 180.0) * th;
            double x = center.x + radius * cos(radians);
            double y = center.y + radius * sin(radians);
            points[i] = (struct tg_point) { x, y };
            i++;
        }
        // add last connecting point, make a total of steps
        points[steps] = points[0];
        ring = tg_ring_new_ix(points, steps+1, ix);
        tg_free(points);
    }
    return ring;
}

struct tg_ring *tg_circle_new(struct tg_point center, double radius, int steps)
{
    return tg_circle_new_ix(center, radius, steps, 0);
}

double tg_point_distance_segment(struct tg_point p, struct tg_segment s) {
    double a = p.x - s.a.x;
    double b = p.y - s.a.y;
    double c = s.b.x - s.a.x;
    double d = s.b.y - s.a.y;
    double e = c * c + d * d;
    double f = e ? (a * c + b * d) / e : 0.0;
    double g = fclamp0(f, 0, 1);
    double dx = p.x - (s.a.x + g * c);
    double dy = p.y - (s.a.y + g * d);
    return sqrt(dx * dx + dy * dy);
}

double tg_rect_distance_rect(struct tg_rect a, struct tg_rect b) {
    double dx = fmax0(fmax0(a.min.x, b.min.x) - fmin0(a.max.x, b.max.x), 0);
    double dy = fmax0(fmax0(a.min.y, b.min.y) - fmin0(a.max.y, b.max.y), 0);
    return sqrt(dx * dx + dy * dy);
}

double tg_point_distance_rect(struct tg_point p, struct tg_rect r) {
    return tg_rect_distance_rect((struct tg_rect) { p, p }, r);
}

double tg_point_distance_point(struct tg_point a, struct tg_point b) {
    return sqrt((a.x-b.x) * (a.x-b.x) + (a.y-b.y) * (a.y-b.y));
}

enum nqentry_kind {
    NQUEUE_KIND_SEGMENT,
    NQUEUE_KIND_RECT
};

struct nqentry {
    double dist;
    enum nqentry_kind kind:8;
    int rect_level:8;
    int more;
    union {
        int seg_index;
        int rect_index;
    };
};

// nqueue is a priority queue using a binary heap type structure for ordering
// segments needed by the tg_ring_nearest function.
struct nqueue {
    bool oom;    // nqueue operation failure
    bool onheap; // items array is on the heap
    struct nqentry spare;
    struct nqentry *items;
    size_t len;
    size_t cap;
    size_t maxlen;
};

#define NQUEUE_INIT(queue, init_cap) \
    struct nqentry init_items[init_cap]; \
    queue = (struct nqueue) { \
        .cap = (init_cap), \
        .items = init_items \
    }; \

#define NQUEUE_DESTROY(queue) \
    if (queue.onheap) { \
        tg_free(queue.items); \
    } \

static void nqswap(struct nqueue *queue, size_t i, size_t j) {
    queue->spare = queue->items[i];
    queue->items[i] = queue->items[j];
    queue->items[j] = queue->spare;
}

static int nqcompare(struct nqueue *queue, size_t i, size_t j) {
    const struct nqentry *a = &queue->items[i];
    const struct nqentry *b = &queue->items[j];
    return a->dist < b->dist ? -1 : a->dist > b->dist;
}

static void nqueue_push(struct nqueue *queue, const struct nqentry *item) {
    if (queue->oom) {
        return;
    }
    size_t elsize = sizeof(struct nqentry);
    if (queue->len == queue->cap) {
        size_t cap = queue->cap < 1000 ? queue->cap*2 : queue->cap*1.25;
        struct nqentry *items;
        if (!queue->onheap) {
            items = tg_malloc(elsize*cap);
            if (!items) goto oom;
            memcpy(items, queue->items, elsize*queue->len);
        } else {
            items = tg_realloc(queue->items, elsize*cap);
            if (!items) goto oom;
        }
        queue->items = items;
        queue->cap = cap;
        queue->onheap = true;
    }
    queue->items[queue->len++] = *item;
    size_t i = queue->len - 1;
    while (i != 0) {
        size_t parent = (i - 1) / 2;
        if (!(nqcompare(queue, parent, i) > 0)) break;
        nqswap(queue, parent, i);
        i = parent;
    }
    queue->maxlen = fmax0(queue->maxlen, queue->len);
    return;
oom:
    queue->oom = true;
}

static const struct nqentry *nqueue_pop(struct nqueue *queue) {
    if (queue->len == 0 || queue->oom) {
        return NULL;
    }
    nqswap(queue, 0, queue->len-1);
    queue->len--;
    const struct nqentry *item = &queue->items[queue->len];
    size_t i = 0;
    while (1) {
        size_t smallest = i;
        size_t left = i * 2 + 1;
        size_t right = i * 2 + 2;
        if (left < queue->len && nqcompare(queue, left, smallest) <= 0) {
            smallest = left;
        }
        if (right < queue->len && nqcompare(queue, right, smallest) <= 0) {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        nqswap(queue, smallest, i);
        i = smallest;
    }
    return item;
}

/// Iterates over segments from nearest to farthest.
///
/// This is a kNN operation.
/// The caller must provide their own "rect_dist" and "seg_dist" callbacks to
/// do the actual distance calculations.
///
/// @param ring Input ring
/// @param rect_dist Callback that returns the distance to a tg_rect.
/// @param seg_dist Callback that returns the distance to a tg_segment.
/// @param iter Callback that returns each segment in the ring in order of
/// nearest to farthest. Caller must return true to continue to the next
/// segment, or return false to stop iterating.
/// @param udata User-defined data
/// @return True if operation succeeded, false if out of memory.
/// @note Though not typical, this operation may need to allocate memory.
/// It's recommended to check the return value for success.
/// @note The `*more` argument is an optional ref-value that is used for
/// performing partial step-based or probability-based calculations. A detailed
/// description of its use is outside the scope of this document. Ignoring it 
/// altogether is the preferred behavior.
/// @see RingFuncs
bool tg_ring_nearest_segment(const struct tg_ring *ring, 
    double (*rect_dist)(struct tg_rect rect, int *more, void *udata),
    double (*seg_dist)(struct tg_segment seg, int *more, void *udata),
    bool (*iter)(struct tg_segment seg, double dist, int index, void *udata),
    void *udata)
{
    if (!ring || !seg_dist) return true;
    struct nqueue queue;
    NQUEUE_INIT(queue, 256);
    struct index *ix = ring->index;
    int ixspread = ix ? ring->index->spread : 0;
    if (rect_dist && ix) {
        // Gather root rectangles
        for (int i = 0; i < ix->levels[0].nrects; i++) {
            int more = 0;
            struct tg_rect rect;
            ixrect_to_tg_rect(&ix->levels[0].rects[i], &rect);
            double dist = rect_dist(rect, &more, udata);
            struct nqentry entry = {
                .kind = NQUEUE_KIND_RECT,
                .dist = dist,
                .more = more,
                .rect_level = 0,
                .rect_index = i,
            };
            nqueue_push(&queue, &entry);
        }
    } else {
        // Gather all segments
        for (int i = 0; i < ring->nsegs; i++) {
            struct tg_segment seg = { 
                ring->points[i+0],
                ring->points[i+1]
            };
            int more = 0;
            double dist = seg_dist(seg, &more, udata);
            struct nqentry entry = {
                .kind = NQUEUE_KIND_SEGMENT,
                .dist = dist,
                .more = more,
                .seg_index = i,
            };
            nqueue_push(&queue, &entry);
        }
    }
    while (1) {
        const struct nqentry *ientry = nqueue_pop(&queue);
        if (!ientry) break;
        if (ientry->kind == NQUEUE_KIND_SEGMENT) {
            struct tg_segment seg = { 
                ring->points[ientry->seg_index+0], 
                ring->points[ientry->seg_index+1],
            };
            if (ientry->more) {
                // Reinsert the segment
                struct nqentry entry = *ientry;
                entry.dist = seg_dist(seg, &entry.more, udata);
                nqueue_push(&queue, &entry);
            } else {
                // Segments are sent back to the caller.
                if (!iter(seg, ientry->dist, ientry->seg_index, udata)) {
                    break;
                }
            }
            continue;
        }
        if (ientry->more) {
            // Reinsert the rectangle
            struct tg_rect rect;
            ixrect_to_tg_rect(&ix->levels[ientry->rect_level]
                .rects[ientry->rect_index], &rect);
            struct nqentry entry = *ientry;
            entry.dist = rect_dist(rect, &entry.more, udata);
            nqueue_push(&queue, &entry);
            continue;
        }
        int lvl = ientry->rect_level + 1;
        int start = ientry->rect_index*ixspread;
        ientry = NULL; // no longer need this
        if (lvl == ix->nlevels) {
            // Gather leaf segments
            int nsegs = ring->nsegs;
            int i = start;
            int e = i+ixspread;
            if (e > nsegs) e = nsegs;
            for (; i < e; i++) {
                struct tg_segment seg = {
                    ring->points[i+0],
                    ring->points[i+1]
                };
                int more = 0;
                double dist = seg_dist(seg, &more, udata);
                struct nqentry entry = {
                    .more = more,
                    .dist = dist,
                    .kind = NQUEUE_KIND_SEGMENT,
                    .seg_index = i,
                };
                nqueue_push(&queue, &entry);
            };
        } else {
            // Gather branch rectangles
            const struct level *level = &ix->levels[lvl];
            int i = start;
            int e = i+ixspread;
            if (e > level->nrects) e = level->nrects;
            for (; i < e; i++) {
                int more = 0;
                struct tg_rect rect;
                ixrect_to_tg_rect(&level->rects[i], &rect);
                double dist = rect_dist(rect, &more, udata);
                struct nqentry entry = {
                    .more = more,
                    .dist = dist,
                    .kind = NQUEUE_KIND_RECT,
                    .rect_level = lvl,
                    .rect_index = i,
                };
                nqueue_push(&queue, &entry);
            };
        }
    }
    bool oom = queue.oom;
    NQUEUE_DESTROY(queue);
    return !oom;
}

/// Iterates over segments from nearest to farthest.
/// @see tg_ring_nearest_segment(), which shares the same interface, for a 
/// detailed description.
/// @see LineFuncs
bool tg_line_nearest_segment(const struct tg_line *line, 
    double (*rect_dist)(struct tg_rect rect, int *more, void *udata),
    double (*seg_dist)(struct tg_segment seg, int *more, void *udata),
    bool (*iter)(struct tg_segment seg, double dist, int index, void *udata),
    void *udata)
{
    struct tg_ring *ring = (struct tg_ring *)line;
    return tg_ring_nearest_segment(ring, rect_dist, seg_dist, iter, udata);
}

////////////////////////////////////////////////////////////////////////////////
//  Spatial predicates
////////////////////////////////////////////////////////////////////////////////

/// Tests whether two geometries are topologically equal.
/// @see GeometryPredicates
bool tg_geom_equals(const struct tg_geom *a, const struct tg_geom *b) {
    return tg_geom_within(a, b) && tg_geom_contains(a, b);
}

/// Tests whether 'a' is fully contained inside of 'b'.
/// @note Works the same as `tg_geom_covers(b, a)`
/// @see GeometryPredicates
bool tg_geom_coveredby(const struct tg_geom *a, const struct tg_geom *b) {
    return tg_geom_covers(b, a);
}

/// Tests whether 'a' and 'b' have no point in common, and are fully
/// disconnected geometries.
/// @note Works the same as `!tg_geom_intersects(a, b)`
/// @see GeometryPredicates
bool tg_geom_disjoint(const struct tg_geom *a, const struct tg_geom *b) {
    return !tg_geom_intersects(a, b);
}

/// Tests whether 'a' is contained inside of 'b' and not touching the boundary
/// of 'b'.
/// @note Works the same as `tg_geom_contains(b, a)`
/// @warning This predicate returns **false** when geometry 'a' is *on* or
/// *touching* the boundary of geometry 'b'. Such as when a point is on the
/// edge of a polygon.  
/// For full coverage, consider using @ref tg_geom_coveredby.
/// @see GeometryPredicates
bool tg_geom_within(const struct tg_geom *a, const struct tg_geom *b) {
    return tg_geom_contains(b, a);
}

bool tg_geom_crosses(const struct tg_geom *a, const struct tg_geom *b) {
    (void)a; (void)b;
    // unsupported
    return false;
}

bool tg_geom_overlaps(const struct tg_geom *a, const struct tg_geom *b) {
    (void)a; (void)b;
    // unsupported
    return false;
}

int tg_geom_de9im_dims(const struct tg_geom *geom) {
    int dims = -1;
    if (geom) {
        switch (geom->head.base) {
        case BASE_POINT: return 0;
        case BASE_LINE:  return 1;
        case BASE_RING:  return 2;
        case BASE_POLY:  return 2;
        case BASE_GEOM:
            switch (geom->head.type) {
            case TG_POINT:           return 0;
            case TG_LINESTRING:      return 1;
            case TG_POLYGON:         return 2;
            case TG_MULTIPOINT:      return 0;
            case TG_MULTILINESTRING: return 1;
            case TG_MULTIPOLYGON:    return 2;
            case TG_GEOMETRYCOLLECTION: {
                int ngeoms = tg_geom_num_geometries(geom);
                for (int i = 0; i < ngeoms; i++) {
                    const struct tg_geom *child = tg_geom_geometry_at(geom, i);
                    int child_dims = tg_geom_de9im_dims(child);
                    if (child_dims > dims) {
                        dims = child_dims;
                    }
                }
            }}
        }
    }
    return dims;
}

/// Copies a ring
/// @param ring Input ring, caller retains ownership.
/// @return A duplicate of the provided ring. 
/// @return NULL if out of memory
/// @note The caller is responsible for freeing with tg_ring_free().
/// @note This method performs a deep copy of the entire geometry to new memory.
/// @see RingFuncs
struct tg_ring *tg_ring_copy(const struct tg_ring *ring) {
    if (!ring) {
        return NULL;
    }
    size_t size = ring_alloc_size(ring);
    struct tg_ring *ring2 = tg_malloc(size);
    if (!ring2) {
        return NULL;
    }
    memcpy(ring2, ring, size);
    rc_init(&ring2->head.rc);
    rc_retain(&ring2->head.rc);
    ring2->head.noheap = 0;
    if (ring->ystripes) {
        ring2->ystripes = tg_malloc(ring->ystripes->memsz);
        if (!ring2->ystripes) {
            tg_free(ring2);
            return NULL;
        }
        memcpy(ring2->ystripes, ring->ystripes, ring->ystripes->memsz);
    }
    return ring2;
}

/// Copies a line
/// @param line Input line, caller retains ownership.
/// @return A duplicate of the provided line. 
/// @return NULL if out of memory
/// @note The caller is responsible for freeing with tg_line_free().
/// @note This method performs a deep copy of the entire geometry to new memory.
/// @see LineFuncs
struct tg_line *tg_line_copy(const struct tg_line *line) {
    return (struct tg_line*)tg_ring_copy((struct tg_ring*)line);
}

/// Copies a polygon.
/// @param poly Input polygon, caller retains ownership.
/// @return A duplicate of the provided polygon. 
/// @return NULL if out of memory
/// @note The caller is responsible for freeing with tg_poly_free().
/// @note This method performs a deep copy of the entire geometry to new memory.
/// @see PolyFuncs
struct tg_poly *tg_poly_copy(const struct tg_poly *poly) {
    if (!poly) {
        return NULL;
    }
    if (poly->head.base == BASE_RING) {
        return (struct tg_poly*)tg_ring_copy((struct tg_ring*)poly);
    }
    struct tg_poly *poly2 = tg_malloc(sizeof(struct tg_poly));
    if (!poly2) {
        goto fail;
    }
    memset(poly2, 0, sizeof(struct tg_poly));
    memcpy(&poly2->head, &poly->head, sizeof(struct head));
    rc_init(&poly2->head.rc);
    rc_retain(&poly2->head.rc);
    poly2->head.noheap = 0;
    poly2->exterior = tg_ring_copy(poly->exterior);
    if (!poly2->exterior) {
        goto fail;
    }
    if (poly->nholes > 0) {
        poly2->holes = tg_malloc(sizeof(struct tg_ring*)*poly->nholes);
        if (!poly2->holes) {
            goto fail;
        }
        poly2->nholes = poly->nholes;
        memset(poly2->holes, 0, sizeof(struct tg_ring*)*poly2->nholes);
        for (int i = 0; i < poly2->nholes; i++) {
            poly2->holes[i] = tg_ring_copy(poly->holes[i]);
            if (!poly2->holes[i]) {
                goto fail;
            }
        }
    }
    return poly2;
fail:
    tg_poly_free(poly2);
    return NULL;
}

static struct tg_geom *geom_copy(const struct tg_geom *geom) {
    struct tg_geom *geom2 = tg_malloc(sizeof(struct tg_geom));
    if (!geom2) {
        return NULL;
    }
    memset(geom2, 0, sizeof(struct tg_geom));
    memcpy(&geom2->head, &geom->head, sizeof(struct head));
    rc_init(&geom2->head.rc);
    rc_retain(&geom2->head.rc);
    geom2->head.noheap = 0;
    switch (geom->head.type) {
    case TG_POINT:
        geom2->point.x = geom->point.x;
        geom2->point.y = geom->point.y;
        geom2->z = geom->z;
        geom2->m = geom->m;
        break;
    case TG_LINESTRING:
        geom2->line = tg_line_copy(geom->line);
        if (!geom2->line) {
            goto fail;
        }
        break;
    case TG_POLYGON:
        geom2->poly = tg_poly_copy(geom->poly);
        if (!geom2->poly) {
            goto fail;
        }
        break;
    case TG_MULTIPOINT:
    case TG_MULTILINESTRING:
    case TG_MULTIPOLYGON:
    case TG_GEOMETRYCOLLECTION:
        if (geom->multi) {
            geom2->multi = tg_malloc(sizeof(struct multi));
            if (!geom2->multi) {
                goto fail;
            }
            memset(geom2->multi, 0, sizeof(struct multi));
            geom2->multi->rect = geom->multi->rect;
            if (geom->multi->geoms) {
                size_t gsize = sizeof(struct tg_geom*)*geom->multi->ngeoms;
                geom2->multi->geoms = tg_malloc(gsize);
                if (!geom2->multi->geoms) {
                    goto fail;
                }
                memset(geom2->multi->geoms, 0, gsize);
                geom2->multi->ngeoms = geom->multi->ngeoms;
                for (int i = 0; i < geom->multi->ngeoms; i++) {
                    const struct tg_geom *child = geom->multi->geoms[i];
                    geom2->multi->geoms[i] = tg_geom_copy(child);
                    if (!geom2->multi->geoms[i]) {
                        goto fail;
                    }
                }
            }
            if (geom->multi->index) {
                geom2->multi->index = tg_malloc(geom->multi->index->memsz);
                if (!geom2->multi->index) {
                    goto fail;
                }
                memcpy(geom2->multi->index, geom->multi->index, 
                    geom->multi->index->memsz);
            }
            if (geom->multi->ixgeoms) {
                geom2->multi->ixgeoms = tg_malloc(
                    geom->multi->ngeoms*sizeof(int));
                if (!geom2->multi->ixgeoms) {
                    goto fail;
                }
                memcpy(geom2->multi->ixgeoms, geom->multi->ixgeoms,
                    geom->multi->ngeoms*sizeof(int));
            }
        }
        break;
    }
    if (geom->head.type != TG_POINT && geom->coords) {
        geom2->coords = tg_malloc(sizeof(double)*geom->ncoords);
        if (!geom2->coords) {
            goto fail;
        }
        geom2->ncoords = geom->ncoords;
        memcpy(geom2->coords, geom->coords, sizeof(double)*geom->ncoords);
    }
    if (geom->error) {
        // error and xjson share the same memory, so this copy covers both.
        size_t esize = strlen(geom->error)+1;
        geom2->error = tg_malloc(esize);
        if (!geom2->error) {
            goto fail;
        }
        memcpy(geom2->error, geom->error, esize);
    }
    return geom2;
fail:
    tg_geom_free(geom2);
    return NULL;
}

static struct boxed_point *boxed_point_copy(const struct boxed_point *point) {
    struct boxed_point *point2 = tg_malloc(sizeof(struct boxed_point));
    if (!point2) {
        return NULL;
    }
    memcpy(point2, point, sizeof(struct boxed_point));
    rc_init(&point2->head.rc);
    rc_retain(&point2->head.rc);
    point2->head.noheap = 0;
    return point2;
}

/// Copies a geometry
/// @param geom Input geometry, caller retains ownership.
/// @return A duplicate of the provided geometry.
/// @return NULL if out of memory
/// @note The caller is responsible for freeing with tg_geom_free().
/// @note This method performs a deep copy of the entire geometry to new memory.
/// @see GeometryConstructors
struct tg_geom *tg_geom_copy(const struct tg_geom *geom) {
    if (geom) {
        switch (geom->head.base) {
        case BASE_GEOM:
            return geom_copy(geom);
        case BASE_POINT:
            return (struct tg_geom*)boxed_point_copy((struct boxed_point*)geom);
        case BASE_LINE:
            return (struct tg_geom*)tg_line_copy((struct tg_line*)geom);
        case BASE_RING:
            return (struct tg_geom*)tg_ring_copy((struct tg_ring*)geom);
        case BASE_POLY:
            return (struct tg_geom*)tg_poly_copy((struct tg_poly*)geom);
        }
    }
    return NULL;
}

/// Tests whether a geometry intersects a rect.
/// @see GeometryPredicates
bool tg_geom_intersects_rect(const struct tg_geom *a, struct tg_rect b) {
    struct tg_ring *ring = stack_ring();
    rect_to_ring(b, ring);
    return tg_geom_intersects(a, (struct tg_geom*)ring);
}

static bool multi_index_search(const struct multi *multi, struct tg_rect rect,
    int levelidx, int startidx,
    bool (*iter)(const struct tg_geom *geom, int index, void *udata),
    void *udata)
{
    const struct index *index = multi->index;
    if (levelidx == index->nlevels) {
        // leaf
        int s = startidx;
        int e = s+index->spread;
        if (e > multi->ngeoms) {
            e = multi->ngeoms;
        }
        for (int i = s; i < e; i++) {
            int index = multi->ixgeoms[i];
            const struct tg_geom *child = multi->geoms[index];
            if (tg_rect_intersects_rect(tg_geom_rect(child), rect)) {
                if (!iter(child, index, udata)) {
                    return false;
                }
            }
        }
    } else {
        // branch
        const struct level *level = &index->levels[levelidx];
        int s = startidx;
        int e = s+index->spread;
        if (e > level->nrects) {
            e = level->nrects;
        }
        for (int i = s; i < e; i++) {
            struct tg_rect brect;
            ixrect_to_tg_rect(&level->rects[i], &brect);
            if (tg_rect_intersects_rect(brect, rect)) {
                if (!multi_index_search(multi, rect, levelidx+1, 
                    i*index->spread, iter, udata))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

/// Iterates over all child geometries in geom that intersect rect
/// @note Only iterates over collection types: TG_MULTIPOINT, 
/// TG_MULTILINESTRING, TG_MULTIPOLYGON, and TG_GEOMETRYCOLLECTION.
/// @note A GeoJSON FeatureCollection works as well.
/// @see tg_geom_typeof()
/// @see GeometryAccessors
void tg_geom_search(const struct tg_geom *geom, struct tg_rect rect,
    bool (*iter)(const struct tg_geom *geom, int index, void *udata),
    void *udata)
{
    const struct multi *multi = geom_multi(geom);
    if (!iter || !multi) return;
    if (!tg_rect_intersects_rect(tg_geom_rect(geom), rect)) {
        return;
    }
    if (!multi->index) {
        // sequential search
        for (int i = 0; i < multi->ngeoms; i++) {
            const struct tg_geom *child = multi->geoms[i];
            if (tg_rect_intersects_rect(tg_geom_rect(child), rect)) {
                if (!iter(child, i, udata)) {
                    return;
                }
            } 
        }
    } else {
        // indexed search
        multi_index_search(multi, rect, 0, 0, iter, udata);
    }
}

/// Calculate the length of a line.
double tg_line_length(const struct tg_line *line) {
    return tg_ring_perimeter((struct tg_ring*)line);
}

/// Parse data into a geometry by auto detecting the input type.
/// The input data can be WKB, WKT, Hex, or GeoJSON.
/// @param data Data
/// @param len Length of data
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors.
/// @see tg_parse_ix()
/// @see tg_geom_error()
/// @see GeometryParsing
struct tg_geom *tg_parse(const void *data, size_t len) {
    return tg_parse_ix(data, len, 0);
}

/// Parse data using provided indexing option.
/// @param data Data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors.
/// @see tg_parse()
struct tg_geom *tg_parse_ix(const void *data, size_t len, enum tg_index ix) {
    if (!data || len == 0) {
        return 0;
    }
    const char *src = data;
    if (src[0] == '{') {
        goto geojson;
    }
    if (isspace(src[0])) {
        for (size_t i = 1; i < len; i++) {
            if (isspace(src[i])) {
                continue;
            } else if (src[i] == '{') {
                goto geojson;
            } else {
                break;
            }
        }
        goto wkt;
    }
    if (isalpha(src[0]) || isxdigit(src[0])) {
        bool ishex = true;
        for (size_t i = 0; i < len && i < 16; i++) {
            if (!isxdigit(src[i])) {
                ishex = false;
                break;
            }
        }
        if (ishex) {
            goto hex;
        }
        goto wkt;
    }
    if (src[0] == 0x00 || src[0] == 0x01) {
        goto wkb;
    }
    goto geobin;
geojson:
    return tg_parse_geojsonn_ix(src, len, ix);
wkt:
    return tg_parse_wktn_ix(src, len, ix);
hex:
    return tg_parse_hexn_ix(src, len, ix);
wkb:
    return tg_parse_wkb_ix((uint8_t*)src, len, ix);
geobin:
    return tg_parse_geobin_ix((uint8_t*)src, len, ix);
}

/// Utility for returning an error message wrapped in a geometry.
/// This operation does not return a real geometry, only an error message,
/// which may be useful for generating custom errors from operations 
/// outside of the TG library.
struct tg_geom *tg_geom_new_error(const char *error) {
    return error?make_parse_error("%s", error):0;
}

/// Set the noheap property to true and the reference counter to zero.
/// _undocumented_
void tg_geom_setnoheap(struct tg_geom *geom) {
    geom->head.rc = 0;
    geom->head.noheap = 1;
}

/// Parse GeoBIN binary using provided indexing option.
/// @param geobin GeoBIN data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_geobin_ix()
/// @see tg_geom_error()
/// @see tg_geom_geobin()
/// @see https://github.com/tidwall/tg/blob/main/docs/GeoBIN.md
/// @see GeometryParsing
struct tg_geom *tg_parse_geobin(const uint8_t *geobin, size_t len) {
    return tg_parse_geobin_ix(geobin, len, 0);
}

static size_t parse_geobin(const uint8_t *geobin, size_t len, size_t i, 
    size_t depth, enum tg_index ix, struct tg_geom **g)
{
    if (i == len) goto invalid;
    if (depth > MAXDEPTH) goto invalid;
    int head = geobin[i];
    if (head == 0x01) {
        return parse_wkb(geobin, len, i, depth, ix, g);
    }
    i++;
    if (head < 0x02 || head > 0x04) {
        goto invalid;
    }
    if (i == len) {
        goto invalid;
    }
    int dims = geobin[i++];
    if (dims && (dims < 2 || dims > 4)) {
        goto invalid;
    }
    if (dims) {
        i += 8*dims*2;
        if (i >= len) {
            goto invalid;
        }
    }
    size_t xjsonlen = 0;
    const char *xjson = (const char*)(geobin+i);
    for (; i < len; i++) {
        if (geobin[i] == '\0') {
            i++;
            break;
        }
        xjsonlen++;
    }
    if (i == len) {
        goto invalid;
    }
    if (xjsonlen > 0 && !json_validn(xjson, xjsonlen)) {
        goto invalid;
    }
    struct tg_geom *geom;
    if (head == 0x04) {
        // FeatureCollection
        if (i+4 > len) {
            goto invalid;
        }
        uint32_t nfeats;
        memcpy(&nfeats, geobin+i, 4);
        i += 4;
        struct tg_geom **feats = tg_malloc(nfeats*sizeof(struct tg_geom*));
        if (!feats) {
            return 0;
        }
        memset(feats, 0, nfeats*sizeof(struct tg_geom*));
        struct tg_geom *feat = 0;
        uint32_t j = 0;
        for (; j < nfeats; j++) {
            i = parse_geobin(geobin, len, i, depth+1, ix, &feat);
            if (i == PARSE_FAIL) {
                break;
            }
            feats[j] = feat;
        }
        if (j == nfeats) {
            geom = tg_geom_new_geometrycollection((void*)feats, nfeats);
        }
        for (uint32_t k = 0; k < j; k++) {
            tg_geom_free(feats[k]);
        }
        tg_free(feats);
        if (j < nfeats) {
            *g = feat; // return the last failed feature
            return PARSE_FAIL;
        }
        if (!geom) {
            *g = 0;
            return PARSE_FAIL;
        }
        geom->head.flags |= IS_FEATURE_COL;
    } else {
        i = parse_wkb(geobin, len, i, depth, ix, &geom);
    }
    if (i == PARSE_FAIL || !geom) {
        *g = geom;
        return PARSE_FAIL;
    }
    if ((xjsonlen > 0 || head == 0x03) && geom->head.base != BASE_GEOM) {
        // Wrap base in tg_geom
        struct tg_geom *g2 = geom_new(geom->head.type);
        if (!g2) {
            tg_geom_free(geom);
            *g = 0;
            return PARSE_FAIL;
        }
        if (geom->head.base == BASE_POINT) {
            g2->point = ((struct boxed_point*)geom)->point;
            boxed_point_free((struct boxed_point*)geom);
        } else {
            g2->line = (struct tg_line*)geom;
        }
        geom = g2;
    }
    if (head == 0x03) {
        geom->head.flags |= IS_FEATURE;
    }
    if (xjsonlen > 0) {
        geom->xjson = tg_malloc(xjsonlen+1);
        if (!geom->xjson) {
            tg_geom_free(geom);
            *g = 0;
            return PARSE_FAIL;
        }
        memcpy(geom->xjson, xjson, xjsonlen+1);
    }
    *g = geom;
    return i;
invalid:
    *g = make_parse_error("invalid binary");
    return PARSE_FAIL;
    
}

/// Parse GeoBIN binary using provided indexing option.
/// @param geobin GeoBIN data
/// @param len Length of data
/// @param ix Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES
/// @returns A geometry or an error. Use tg_geom_error() after parsing to check
/// for errors. 
/// @see tg_parse_geobin()
struct tg_geom *tg_parse_geobin_ix(const uint8_t *geobin, size_t len,
    enum tg_index ix)
{
    struct tg_geom *geom = NULL;
    parse_geobin(geobin, len, 0, 0, ix, &geom);
    if (!geom) return NULL;
    if ((geom->head.flags&IS_ERROR) == IS_ERROR) {
        struct tg_geom *gerr = make_parse_error("ParseError: %s", geom->error);
        tg_geom_free(geom);
        return gerr;
    }
    return geom;
}

static void write_geom_geobin(const struct tg_geom *geom, struct writer *wr);

static void write_base_geom_geobin(const struct tg_geom *geom,
    struct writer *wr)
{
    // extra json section
    const char *xjson = tg_geom_extra_json(geom);
    
    // write head byte
    if ((geom->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL) {
        write_byte(wr, 0x04);
    } else if ((geom->head.flags&IS_FEATURE) == IS_FEATURE) {
        write_byte(wr, 0x03);
    } else if (geom->head.type == TG_POINT && !xjson) {
        write_geom_point_wkb(geom, wr);
        return;
    } else {
        write_byte(wr, 0x02);
    }
    
    // mbr section
    double min[4], max[4];
    int dims = tg_geom_fullrect(geom, min, max);
    write_byte(wr, dims);
    for (int i = 0; i < dims; i++) {
        write_doublele(wr, min[i]);
    }
    for (int i = 0; i < dims; i++) {
        write_doublele(wr, max[i]);
    }
    
    if (xjson) {
        write_string(wr, xjson);
    }
    write_byte(wr, 0);
    
    if ((geom->head.flags&IS_FEATURE_COL) == IS_FEATURE_COL) {
        // write feature collection
        int ngeoms = tg_geom_num_geometries(geom);
        write_uint32le(wr, (uint32_t)ngeoms);
        for (int i = 0; i < ngeoms; i++) {
            const struct tg_geom *g2 = tg_geom_geometry_at(geom, i);
            write_geom_geobin(g2, wr);
        }
    } else {
        // write wkb
        write_geom_wkb(geom, wr);
    }
}

static void write_point_geobin(struct boxed_point *point, struct writer *wr) {
    write_point_wkb(point, wr);
}

static void write_geobin_rect(struct writer *wr, struct tg_rect rect) {
    write_byte(wr, 2); // dims
    write_doublele(wr, rect.min.x);
    write_doublele(wr, rect.min.y);
    write_doublele(wr, rect.max.x);
    write_doublele(wr, rect.max.y);
}

static void write_line_geobin(struct tg_line *line, struct writer *wr) {
    write_byte(wr, 0x02);
    write_geobin_rect(wr, ((struct tg_ring*)line)->rect);
    write_byte(wr, 0);
    write_line_wkb(line, wr);
}

static void write_ring_geobin(struct tg_ring *ring, struct writer *wr) {
    write_byte(wr, 0x02);
    write_geobin_rect(wr, ring->rect);
    write_byte(wr, 0);
    write_ring_wkb(ring, wr);
}

static void write_poly_geobin(struct tg_poly *poly, struct writer *wr) {
    write_byte(wr, 0x02);
    write_geobin_rect(wr, poly->exterior->rect);
    write_byte(wr, 0);
    write_poly_wkb(poly, wr);
}

static void write_geom_geobin(const struct tg_geom *geom, struct writer *wr) {
    if ((geom->head.flags&IS_FEATURE) == IS_FEATURE) {
        goto base_geom;
    }
    switch (geom->head.base) {
    case BASE_GEOM:
    base_geom:
        write_base_geom_geobin(geom, wr);
        break;
    case BASE_POINT:
        write_point_geobin((struct boxed_point*)geom, wr);
        break;
    case BASE_LINE:
        write_line_geobin((struct tg_line*)geom, wr);
        break;
    case BASE_RING:
        write_ring_geobin((struct tg_ring*)geom, wr);
        break;
    case BASE_POLY:
        write_poly_geobin((struct tg_poly*)geom, wr);
        break;
    }
}

/// Writes a GeoBIN representation of a geometry.
///
/// The content is stored in the buffer pointed by dst.
///
/// @param geom Input geometry
/// @param dst Buffer where the resulting content is stored.
/// @param n Maximum number of bytes to be used in the buffer.
/// @return  The number of characters needed to store the content into the
/// buffer.
/// If the returned length is greater than n, then only a parital copy
/// occurred, for example:
///
/// ```
/// uint8_t buf[64];
/// size_t len = tg_geom_geobin(geom, buf, sizeof(buf));
/// if (len > sizeof(buf)) {
///     // ... write did not complete ...
/// }
/// ```
///
/// @see tg_geom_geojson()
/// @see tg_geom_wkt()
/// @see tg_geom_wkb()
/// @see tg_geom_hex()
/// @see GeometryWriting
size_t tg_geom_geobin(const struct tg_geom *geom, uint8_t *dst, size_t n) {
    if (!geom) return 0;
    struct writer wr = { .dst = dst, .n = n };
    write_geom_geobin(geom, &wr);
    return wr.count;
}

/// Returns the minimum bounding rectangle of a geometry on all dimensions.
/// @param geom Input geometry
/// @param min min values, must have room for 4 dimensions
/// @param max max values, must have room for 4 dimensions
/// @return number of dimensions, or zero if invalid geom.
/// @see tg_geom_rect()
int tg_geom_fullrect(const struct tg_geom *geom, double min[4], double max[4]) {
    if (!geom) {
        return 0;
    }
    struct tg_rect rect = tg_geom_rect(geom);
    min[0] = rect.min.x;
    min[1] = rect.min.y;
    min[2] = 0;
    min[3] = 0;
    max[0] = rect.max.x;
    max[1] = rect.max.y;
    max[2] = 0;
    max[3] = 0;
    int dims = 2;
    if (geom->head.base == BASE_GEOM) {
        if (geom->head.type == TG_POINT) {
            // Point
            if ((geom->head.flags&HAS_Z) == HAS_Z) {
                min[dims] = geom->z;
                max[dims] = geom->z;
                dims++;
            }
            if ((geom->head.flags&HAS_M) == HAS_M) {
                min[dims] = geom->m;
                max[dims] = geom->m;
                dims++;
            }
        } else if (geom->head.type == TG_GEOMETRYCOLLECTION && geom->multi) {
            // GeometryCollection. Expand all child geometries
            struct tg_geom **geoms = geom->multi->geoms;
            int ngeoms = geom->multi->ngeoms;
            double gmin[4], gmax[4];
            for (int i = 0; i < ngeoms; i++) {
                int gdims = tg_geom_fullrect(geoms[i], gmin, gmax);
                if (gdims >= 3) {
                    if (dims == 2) {
                        min[2] = gmin[2];
                        max[2] = gmax[2];
                        dims++;
                    } else {
                        min[2] = fmin0(min[2], gmin[2]);
                        max[2] = fmax0(max[2], gmax[2]);
                    }
                }
                if (gdims >= 4) {
                    if (dims == 3) {
                        min[3] = gmin[3];
                        max[3] = gmax[3];
                        dims++;
                    } else {
                        min[3] = fmin0(min[3], gmin[3]);
                        max[3] = fmax0(max[3], gmax[3]);
                    }
                }
            }
        } else {
            // Other geometries
            if ((geom->head.flags&HAS_Z) == HAS_Z) dims++;
            if ((geom->head.flags&HAS_M) == HAS_M) dims++;
            if (dims == 3 && geom->ncoords > 0) {
                min[2] = geom->coords[0];
                max[2] = geom->coords[0];
                for (int i = 1; i < geom->ncoords; i++) {
                    min[2] = fmin0(min[2], geom->coords[i]);
                    max[2] = fmax0(max[2], geom->coords[i]);
                }
            } else if (dims == 4 && geom->ncoords > 1) {
                min[2] = geom->coords[0];
                min[3] = geom->coords[1];
                max[2] = geom->coords[0];
                max[3] = geom->coords[1];
                for (int i = 2; i < geom->ncoords-1; i+=2) {
                    min[2] = fmin0(min[2], geom->coords[i]);
                    min[3] = fmin0(min[3], geom->coords[i+1]);
                    max[2] = fmax0(max[2], geom->coords[i]);
                    max[3] = fmax0(max[3], geom->coords[i+1]);
                }
            }
        }
    }
    return dims;
}

/// Returns the minimum bounding rectangle of GeoBIN data.
/// @param geobin GeoBIN data
/// @param len Length of data
/// @param min min values, must have room for 4 dimensions
/// @param max max values, must have room for 4 dimensions
/// @return number of dimensions, or zero if rect cannot be determined.
/// @see tg_geom_fullrect()
/// @see tg_geom_rect()
int tg_geobin_fullrect(const uint8_t *geobin, size_t len, double min[4],
    double max[4])
{
    size_t dims = 0;
    if (geobin && len > 2 && geobin[0] >= 0x01 && geobin[0] <= 0x04) {
        if (geobin[0] == 0x01 && len >= 5) {
            // Read Point
            uint32_t type;
            memcpy(&type, geobin+1, 4);
            switch (type) {
            case    1: dims = 2; break;
            case 1001: dims = 3; break;
            case 2001: dims = 3; break;
            case 3001: dims = 4; break;
            }
            if (dims > 0 && len >= 5+8*dims) {
                memcpy(min, geobin+5, 8*dims);
                memcpy(max, geobin+5, 8*dims);
            }
        } else if (geobin[0] != 0x01 && len >= 2+8*(size_t)geobin[1]*2){
            // Read MBR
            dims = geobin[1];
            memcpy(min, geobin+2, 8*dims);
            memcpy(max, geobin+2+8*dims, 8*dims);
        }
    }
    return dims;
}

/// Returns the minimum bounding rectangle of GeoBIN data.
/// @param geobin GeoBIN data
/// @param len Length of data
/// @return the rectangle
struct tg_rect tg_geobin_rect(const uint8_t *geobin, size_t len) {
    struct tg_rect rect = { 0 };
    if (geobin && len > 2 && geobin[0] >= 0x01 && geobin[0] <= 0x04) {
        if (geobin[0] == 0x01 && len >= 21) {
            // Read Point
            uint32_t type;
            memcpy(&type, geobin+1, 4);
            if (type == 1 || type == 1001 || type == 2001 || type == 3001) {
                memcpy(&rect.min.x, geobin+5, 8);
                memcpy(&rect.min.y, geobin+5+8, 8);
                rect.max.x = rect.min.x;
                rect.max.y = rect.min.y;
            }
        } else if (geobin[0] != 0x01 && len >= 2+8*(size_t)geobin[1]*2){
            // Read MBR
            int dims = geobin[1];
            if (dims >= 2) {
                memcpy(&rect.min.x, geobin+2, 8);
                memcpy(&rect.min.y, geobin+2+8, 8);
                memcpy(&rect.max.x, geobin+2+8*dims, 8);
                memcpy(&rect.max.y, geobin+2+8*dims+8, 8);
            }
        }
    }
    return rect;
}

/// Returns the center point of GeoBIN data.
/// @param geobin GeoBIN data
/// @param len Length of data
/// @return the center point
struct tg_point tg_geobin_point(const uint8_t *geobin, size_t len) {
    struct tg_point point = { 0 };
    if (geobin && len > 2 && geobin[0] >= 0x01 && geobin[0] <= 0x04) {
        if (geobin[0] == 0x01 && len >= 21) {
            // Read Point
            uint32_t type;
            memcpy(&type, geobin+1, 4);
            if (type == 1 || type == 1001 || type == 2001 || type == 3001) {
                memcpy(&point.x, geobin+5, 8);
                memcpy(&point.y, geobin+5+8, 8);
            }
        } else if (geobin[0] != 0x01 && len >= 2+8*(size_t)geobin[1]*2){
            // Read MBR
            struct tg_rect rect = { 0 };
            int dims = geobin[1];
            if (dims >= 2) {
                memcpy(&rect.min.x, geobin+2, 8);
                memcpy(&rect.min.y, geobin+2+8, 8);
                memcpy(&rect.max.x, geobin+2+8*dims, 8);
                memcpy(&rect.max.y, geobin+2+8*dims+8, 8);
            }
            point = tg_rect_center(rect);
        }
    }
    return point;
}
