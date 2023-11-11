// https://github.com/tidwall/tg
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by a license 
// that can be found in the LICENSE file.

#ifndef TG_H
#define TG_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/// The base point type used for all geometries.
/// @see PointFuncs
struct tg_point {
    double x;
    double y;
};

/// The base segment type used in tg_line and tg_ring for joining two vertices.
/// @see SegmentFuncs
struct tg_segment {
    struct tg_point a;
    struct tg_point b;
};

/// A rectangle defined by a minimum and maximum coordinates.
/// Returned by the tg_geom_rect(), tg_ring_rect(), and other \*_rect() 
/// functions for getting a geometry's minumum bounding rectangle.
/// Also used internally for geometry indexing.
/// @see RectFuncs
struct tg_rect {
    struct tg_point min;
    struct tg_point max;
};

struct tg_line;  ///< Find the description in the tg.c file.
struct tg_ring;  ///< Find the description in the tg.c file.
struct tg_poly;  ///< Find the description in the tg.c file.
struct tg_geom;  ///< Find the description in the tg.c file.

/// Geometry types.
///
/// All tg_geom are one of the following underlying types.
///
/// @see tg_geom_typeof()
/// @see tg_geom_type_string()
/// @see GeometryAccessors
enum tg_geom_type {
    TG_POINT              = 1, ///< Point
    TG_LINESTRING         = 2, ///< LineString
    TG_POLYGON            = 3, ///< Polygon
    TG_MULTIPOINT         = 4, ///< MultiPoint, collection of points
    TG_MULTILINESTRING    = 5, ///< MultiLineString, collection of linestrings
    TG_MULTIPOLYGON       = 6, ///< MultiPolygon, collection of polygons
    TG_GEOMETRYCOLLECTION = 7, ///< GeometryCollection, collection of geometries
};

/// Geometry indexing options.
///
/// Used for polygons, rings, and lines to make the point-in-polygon and
/// geometry intersection operations fast.
///
/// An index can also be used for efficiently traversing, searching, and 
/// performing nearest-neighbor (kNN) queries on the segment using 
/// tg_ring_index_*() and tg_ring_nearest() functions.
enum tg_index { 
    TG_DEFAULT,  ///< default is TG_NATURAL or tg_env_set_default_index().
    TG_NONE,     ///< no indexing available, or disabled.
    TG_NATURAL,  ///< indexing with natural ring order, for rings/lines
    TG_YSTRIPES, ///< indexing using segment striping, rings only
};

/// @defgroup GeometryConstructors Geometry constructors
/// Functions for creating and freeing geometries. 
/// @{
struct tg_geom *tg_geom_new_point(struct tg_point point);
struct tg_geom *tg_geom_new_linestring(const struct tg_line *line);
struct tg_geom *tg_geom_new_polygon(const struct tg_poly *poly);
struct tg_geom *tg_geom_new_multipoint(const struct tg_point *points, int npoints);
struct tg_geom *tg_geom_new_multilinestring(const struct tg_line *const lines[], int nlines);
struct tg_geom *tg_geom_new_multipolygon(const struct tg_poly *const polys[], int npolys);
struct tg_geom *tg_geom_new_geometrycollection(const struct tg_geom *const geoms[], int ngeoms);
struct tg_geom *tg_geom_clone(const struct tg_geom *geom);
struct tg_geom *tg_geom_copy(const struct tg_geom *geom);
void tg_geom_free(struct tg_geom *geom);
/// @}

/// @defgroup GeometryAccessors Geometry accessors
/// Functions for accessing various information about geometries, such as
/// getting the geometry type or extracting underlying components or
/// coordinates.
/// @{
enum tg_geom_type tg_geom_typeof(const struct tg_geom *geom);
const char *tg_geom_type_string(enum tg_geom_type type);
struct tg_rect tg_geom_rect(const struct tg_geom *geom);
bool tg_geom_is_feature(const struct tg_geom *geom);
bool tg_geom_is_featurecollection(const struct tg_geom *geom);
struct tg_point tg_geom_point(const struct tg_geom *geom);
const struct tg_line *tg_geom_line(const struct tg_geom *geom);
const struct tg_poly *tg_geom_poly(const struct tg_geom *geom);
int tg_geom_num_points(const struct tg_geom *geom);
struct tg_point tg_geom_point_at(const struct tg_geom *geom, int index);
int tg_geom_num_lines(const struct tg_geom *geom);
const struct tg_line *tg_geom_line_at(const struct tg_geom *geom, int index);
int tg_geom_num_polys(const struct tg_geom *geom);
const struct tg_poly *tg_geom_poly_at(const struct tg_geom *geom, int index);
int tg_geom_num_geometries(const struct tg_geom *geom);
const struct tg_geom *tg_geom_geometry_at(const struct tg_geom *geom, int index);
const char *tg_geom_extra_json(const struct tg_geom *geom);
bool tg_geom_is_empty(const struct tg_geom *geom);
int tg_geom_dims(const struct tg_geom *geom);
bool tg_geom_has_z(const struct tg_geom *geom);
bool tg_geom_has_m(const struct tg_geom *geom);
double tg_geom_z(const struct tg_geom *geom);
double tg_geom_m(const struct tg_geom *geom);
const double *tg_geom_extra_coords(const struct tg_geom *geom);
int tg_geom_num_extra_coords(const struct tg_geom *geom);
size_t tg_geom_memsize(const struct tg_geom *geom);
void tg_geom_search(const struct tg_geom *geom, struct tg_rect rect,
    bool (*iter)(const struct tg_geom *geom, int index, void *udata),
    void *udata);
/// @}

/// @defgroup GeometryPredicates Geometry predicates
/// Functions for testing the spatial relations of two geometries.
/// @{
bool tg_geom_equals(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_intersects(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_disjoint(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_contains(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_within(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_covers(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_coveredby(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_touches(const struct tg_geom *a, const struct tg_geom *b);
bool tg_geom_intersects_rect(const struct tg_geom *a, struct tg_rect b);
bool tg_geom_intersects_xy(const struct tg_geom *a, double x, double y);
/// @}

/// @defgroup GeometryParsing Geometry parsing
/// Functions for parsing geometries from external data representations.
/// It's recommended to use tg_geom_error() after parsing to check for errors.
/// @{
struct tg_geom *tg_parse_geojson(const char *geojson);
struct tg_geom *tg_parse_geojsonn(const char *geojson, size_t len);
struct tg_geom *tg_parse_geojson_ix(const char *geojson, enum tg_index ix);
struct tg_geom *tg_parse_geojsonn_ix(const char *geojson, size_t len, enum tg_index ix);
struct tg_geom *tg_parse_wkt(const char *wkt);
struct tg_geom *tg_parse_wktn(const char *wkt, size_t len);
struct tg_geom *tg_parse_wkt_ix(const char *wkt, enum tg_index ix);
struct tg_geom *tg_parse_wktn_ix(const char *wkt, size_t len, enum tg_index ix);
struct tg_geom *tg_parse_wkb(const uint8_t *wkb, size_t len);
struct tg_geom *tg_parse_wkb_ix(const uint8_t *wkb, size_t len, enum tg_index ix);
struct tg_geom *tg_parse_hex(const char *hex);
struct tg_geom *tg_parse_hexn(const char *hex, size_t len);
struct tg_geom *tg_parse_hex_ix(const char *hex, enum tg_index ix);
struct tg_geom *tg_parse_hexn_ix(const char *hex, size_t len, enum tg_index ix);
const char *tg_geom_error(const struct tg_geom *geom);
/// @}

/// @defgroup GeometryWriting Geometry writing
/// Functions for writing geometries as external data representations.
/// @{
size_t tg_geom_geojson(const struct tg_geom *geom, char *dst, size_t n);
size_t tg_geom_wkt(const struct tg_geom *geom, char *dst, size_t n);
size_t tg_geom_wkb(const struct tg_geom *geom, uint8_t *dst, size_t n);
size_t tg_geom_hex(const struct tg_geom *geom, char *dst, size_t n);
/// @}

/// @defgroup GeometryConstructorsEx Geometry with alternative dimensions
/// Functions for working with geometries that have more than two dimensions or
/// are empty. The extra dimensional coordinates contained within these
/// geometries are only carried along and serve no other purpose than to be
/// available for when it's desired to export to an output representation such
/// as GeoJSON, WKT, or WKB.
/// @{
struct tg_geom *tg_geom_new_point_z(struct tg_point point, double z);
struct tg_geom *tg_geom_new_point_m(struct tg_point point, double m);
struct tg_geom *tg_geom_new_point_zm(struct tg_point point, double z, double m);
struct tg_geom *tg_geom_new_point_empty(void);
struct tg_geom *tg_geom_new_linestring_z(const struct tg_line *line, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_linestring_m(const struct tg_line *line, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_linestring_zm(const struct tg_line *line, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_linestring_empty(void);
struct tg_geom *tg_geom_new_polygon_z(const struct tg_poly *poly, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_polygon_m(const struct tg_poly *poly, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_polygon_zm(const struct tg_poly *poly, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_polygon_empty(void);
struct tg_geom *tg_geom_new_multipoint_z(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipoint_m(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipoint_zm(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipoint_empty(void);
struct tg_geom *tg_geom_new_multilinestring_z(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multilinestring_m(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multilinestring_zm(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multilinestring_empty(void);
struct tg_geom *tg_geom_new_multipolygon_z(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipolygon_m(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipolygon_zm(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
struct tg_geom *tg_geom_new_multipolygon_empty(void);
struct tg_geom *tg_geom_new_geometrycollection_empty(void);
/// @}

/// @defgroup PointFuncs Point functions
/// Functions for working directly with the tg_point type.
/// @{
struct tg_rect tg_point_rect(struct tg_point point);
bool tg_point_intersects_rect(struct tg_point a, struct tg_rect b);
/// @}

/// @defgroup SegmentFuncs Segment functions
/// Functions for working directly with the tg_segment type.
/// @{
struct tg_rect tg_segment_rect(struct tg_segment s);
bool tg_segment_intersects_segment(struct tg_segment a, struct tg_segment b);
/// @}

/// @defgroup RectFuncs Rectangle functions
/// Functions for working directly with the tg_rect type.
/// @{
struct tg_rect tg_rect_expand(struct tg_rect rect, struct tg_rect other);
struct tg_rect tg_rect_expand_point(struct tg_rect rect, struct tg_point point);
struct tg_point tg_rect_center(struct tg_rect rect);
bool tg_rect_intersects_rect(struct tg_rect a, struct tg_rect b);
bool tg_rect_intersects_point(struct tg_rect a, struct tg_point b);
/// @}

/// @defgroup RingFuncs Ring functions
/// Functions for working directly with the tg_ring type.
///
/// There are no direct spatial predicates for tg_ring.
/// If you want to perform operations like "intersects" or "covers" then you 
/// must upcast the ring to a tg_geom, like such:
///
/// ```
/// tg_geom_intersects((struct tg_geom*)ring, geom);
/// ```
/// @{
struct tg_ring *tg_ring_new(const struct tg_point *points, int npoints);
struct tg_ring *tg_ring_new_ix(const struct tg_point *points, int npoints, enum tg_index ix);
void tg_ring_free(struct tg_ring *ring);
struct tg_ring *tg_ring_clone(const struct tg_ring *ring);
struct tg_ring *tg_ring_copy(const struct tg_ring *ring);
size_t tg_ring_memsize(const struct tg_ring *ring);
struct tg_rect tg_ring_rect(const struct tg_ring *ring);
int tg_ring_num_points(const struct tg_ring *ring);
struct tg_point tg_ring_point_at(const struct tg_ring *ring, int index);
const struct tg_point *tg_ring_points(const struct tg_ring *ring);
int tg_ring_num_segments(const struct tg_ring *ring);
struct tg_segment tg_ring_segment_at(const struct tg_ring *ring, int index);
bool tg_ring_convex(const struct tg_ring *ring);
bool tg_ring_clockwise(const struct tg_ring *ring);
int tg_ring_index_spread(const struct tg_ring *ring);
int tg_ring_index_num_levels(const struct tg_ring *ring);
int tg_ring_index_level_num_rects(const struct tg_ring *ring, int levelidx);
struct tg_rect tg_ring_index_level_rect(const struct tg_ring *ring, int levelidx, int rectidx);
bool tg_ring_nearest_segment(const struct tg_ring *ring, 
    double (*rect_dist)(struct tg_rect rect, int *more, void *udata),
    double (*seg_dist)(struct tg_segment seg, int *more, void *udata),
    bool (*iter)(struct tg_segment seg, double dist, int index, void *udata),
    void *udata);
void tg_ring_line_search(const struct tg_ring *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata);
void tg_ring_ring_search(const struct tg_ring *a, const struct tg_ring *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata);
double tg_ring_area(const struct tg_ring *ring);
double tg_ring_perimeter(const struct tg_ring *ring);
/// @}

/// @defgroup LineFuncs Line functions
/// Functions for working directly with the tg_line type.
///
/// There are no direct spatial predicates for tg_line.
/// If you want to perform operations like "intersects" or "covers" then you 
/// must upcast the line to a tg_geom, like such:
///
/// ```
/// tg_geom_intersects((struct tg_geom*)line, geom);
/// ```
/// @{
struct tg_line *tg_line_new(const struct tg_point *points, int npoints);
struct tg_line *tg_line_new_ix(const struct tg_point *points, int npoints, enum tg_index ix);
void tg_line_free(struct tg_line *line);
struct tg_line *tg_line_clone(const struct tg_line *line);
struct tg_line *tg_line_copy(const struct tg_line *line);
size_t tg_line_memsize(const struct tg_line *line);
struct tg_rect tg_line_rect(const struct tg_line *line);
int tg_line_num_points(const struct tg_line *line);
const struct tg_point *tg_line_points(const struct tg_line *line);
struct tg_point tg_line_point_at(const struct tg_line *line, int index);
int tg_line_num_segments(const struct tg_line *line);
struct tg_segment tg_line_segment_at(const struct tg_line *line, int index);
bool tg_line_clockwise(const struct tg_line *line);
int tg_line_index_spread(const struct tg_line *line);
int tg_line_index_num_levels(const struct tg_line *line);
int tg_line_index_level_num_rects(const struct tg_line *line, int levelidx);
struct tg_rect tg_line_index_level_rect(const struct tg_line *line, int levelidx, int rectidx);
bool tg_line_nearest_segment(const struct tg_line *line, 
    double (*rect_dist)(struct tg_rect rect, int *more, void *udata),
    double (*seg_dist)(struct tg_segment seg, int *more, void *udata),
    bool (*iter)(struct tg_segment seg, double dist, int index, void *udata),
    void *udata);
void tg_line_line_search(const struct tg_line *a, const struct tg_line *b, 
    bool (*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, 
        int bidx, void *udata),
    void *udata);
double tg_line_length(const struct tg_line *line);
/// @}

/// @defgroup PolyFuncs Polygon functions
/// Functions for working directly with the tg_poly type.
///
/// There are no direct spatial predicates for tg_poly.
/// If you want to perform operations like "intersects" or "covers" then you 
/// must upcast the poly to a tg_geom, like such:
///
/// ```
/// tg_geom_intersects((struct tg_geom*)poly, geom);
/// ```
/// @{
struct tg_poly *tg_poly_new(const struct tg_ring *exterior, const struct tg_ring *const holes[], int nholes);
void tg_poly_free(struct tg_poly *poly);
struct tg_poly *tg_poly_clone(const struct tg_poly *poly);
struct tg_poly *tg_poly_copy(const struct tg_poly *poly);
size_t tg_poly_memsize(const struct tg_poly *poly);
const struct tg_ring *tg_poly_exterior(const struct tg_poly *poly);
int tg_poly_num_holes(const struct tg_poly *poly);
const struct tg_ring *tg_poly_hole_at(const struct tg_poly *poly, int index);
struct tg_rect tg_poly_rect(const struct tg_poly *poly);
bool tg_poly_clockwise(const struct tg_poly *poly);
/// @}

/// @defgroup GlobalFuncs Global environment
/// Functions for optionally setting the behavior of the TG environment.
/// These, if desired, should be called only once at program start up and prior
/// to calling any other tg_*() functions.
/// @{
void tg_env_set_allocator(void *(*malloc)(size_t), void *(*realloc)(void*, size_t), void (*free)(void*));
void tg_env_set_index(enum tg_index ix);
void tg_env_set_index_spread(int spread);
/// @}


#endif // TG_H
