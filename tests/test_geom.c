#include "tests.h"

void test_geom_point() {
    struct tg_geom *p = tg_geom_new_point(P(5, 6));
    struct tg_geom *g2 = tg_geom_new_polygon(POLY(RING(rectangle)));
    size_t mz = tg_geom_memsize(p);

    assert(recteq(tg_geom_rect(p), R(5,6,5,6)));
    assert(tg_geom_covers(g2, p));
    assert(tg_geom_covers(p, p));
    assert(!tg_geom_covers(NULL, p));
    assert(!tg_geom_covers(p, NULL));
    assert(!tg_geom_covers(p, g2));
    assert(mz == 24);
    assert(pointeq(tg_geom_point(p), P(5, 6)));

    assert(tg_geom_intersects(g2, p));
    assert(!tg_geom_intersects(p, NULL));
    assert(!tg_geom_intersects(g2, NULL));
    assert(!tg_geom_intersects(NULL, p));
    assert(!tg_geom_intersects(NULL, g2));

    tg_geom_free(p);
    tg_geom_free(g2);

    struct tg_geom *g;

    g = tg_geom_new_point_z(P(5, 6), 7);
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_z(g) == 7);
    assert(pointeq(tg_geom_point(g), P(5, 6)));
    tg_geom_free(g);

    g = tg_geom_new_point_m(P(5, 6), 7);
    assert(tg_geom_m(g) == 7);
    tg_geom_free(g);

    g = tg_geom_new_point_zm(P(5, 6), 7, 8);
    assert(tg_geom_z(g) == 7);
    assert(tg_geom_m(g) == 8);
    assert(recteq(tg_geom_rect(g), R(5,6,5,6)));

    struct tg_geom *pt = tg_geom_new_point(P(5, 6));
    struct tg_geom *pt2 = tg_geom_new_point_zm(P(5, 6),0,0);
    struct tg_geom *ln = tg_geom_new_linestring(LINE(P(4, 6),P(5,6)));
    struct tg_geom *ln2 = tg_geom_new_linestring_zm(LINE(P(4, 6),P(5,6)),0,0);
    struct tg_geom *pl = tg_geom_new_polygon(POLY(RR(4,4,6,6)));
    struct tg_geom *pl2 = tg_geom_new_polygon(POLY(RR(4,4,6,6), RR(4,4,4,4)));
    struct tg_geom *pl3 = tg_geom_new_polygon_zm(POLY(RR(4,4,6,6)),0,0);
    struct tg_geom *mpt = tg_geom_new_multipoint((struct tg_point[]) {
        P(5,6),
    }, 1);
    struct tg_geom *mpt2 = tg_geom_new_multipoint((struct tg_point[]) {
        P(50,60),
    }, 1);
    struct tg_geom *mln = tg_geom_new_multilinestring(
        (const struct tg_line*const[]) {
            LINE(P(4, 6),P(5,6))
        }, 1);
    struct tg_geom *mln2 = tg_geom_new_multilinestring((const struct tg_line*const[]) {
        LINE(P(40, 60),P(50,60))
    }, 1);
    struct tg_geom *mpl = tg_geom_new_multipolygon((const struct tg_poly*const[]) {
        POLY(RR(4,4,6,6))
    }, 1);
    struct tg_geom *mpl2 = tg_geom_new_multipolygon((const struct tg_poly*const[]) {
        POLY(RR(40,40,60,60))
    }, 1);
    struct tg_geom *mgc = tg_geom_new_geometrycollection((const struct tg_geom*const[]) {
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(4,4,6,6)),0,0)),
    }, 1);
    struct tg_geom *mgc2 = tg_geom_new_geometrycollection((const struct tg_geom*const[]) {
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(40,40,60,60)),0,0)),
    }, 1);

    assert(tg_geom_covers(g, pt));
    assert(tg_geom_intersects(g, pt));
    assert(tg_geom_intersects(g, pt2));
    assert(tg_geom_covers(g, pt2));
    assert(tg_geom_intersects(g, ln));
    assert(!tg_geom_covers(g, ln));
    assert(!tg_geom_covers(g, ln2));
    assert(tg_geom_intersects(g, ln2));
    assert(tg_geom_intersects(g, pl));
    assert(tg_geom_intersects(g, pl2));
    assert(tg_geom_intersects(g, pl3));
    assert(!tg_geom_covers(g, pl));
    assert(!tg_geom_covers(g, pl2));
    assert(!tg_geom_covers(g, pl3));
    assert(tg_geom_intersects(g, mpt));
    assert(tg_geom_covers(g, mpt));
    assert(!tg_geom_intersects(g, mpt2));
    assert(tg_geom_intersects(g, mln));
    assert(!tg_geom_intersects(g, mln2));
    assert(tg_geom_intersects(g, mpl));
    assert(!tg_geom_intersects(g, mpl2));
    assert(tg_geom_intersects(g, mgc));
    assert(!tg_geom_intersects(g, mgc2));
    assert(!tg_geom_covers(g, gc_geom(
        tg_geom_new_geometrycollection_empty()
    )));
    assert(!tg_geom_covers(g, gc_geom(
        tg_geom_new_geometrycollection(0,0)
    )));
    assert(!tg_geom_covers(g, gc_geom(
        tg_geom_new_geometrycollection((const struct tg_geom*const[]) {
            gc_geom(tg_geom_new_polygon_zm(POLY(RR(40,40,60,60)),0,0)),
        }, 1)
    )));

    tg_geom_free(pt);
    tg_geom_free(pt2);
    tg_geom_free(ln);
    tg_geom_free(ln2);
    tg_geom_free(pl);
    tg_geom_free(pl2);
    tg_geom_free(pl3);
    tg_geom_free(mpt);
    tg_geom_free(mpt2);
    tg_geom_free(mln);
    tg_geom_free(mln2);
    tg_geom_free(mpl);
    tg_geom_free(mpl2);
    tg_geom_free(mgc);
    tg_geom_free(mgc2);
    tg_geom_free(g);
}

void test_geom_linestring() {
    struct tg_geom *lil1 = tg_geom_new_polygon(POLY(RR(4, 5, 6, 7)));
    struct tg_geom *p = tg_geom_new_point(P(5, 5));
    struct tg_geom *pt2 = tg_geom_new_point_zm(P(2,2),0,0);
    struct tg_geom *pl = tg_geom_new_polygon(POLY(RR(0,2,4,3),RR(2.1,2.1,2.2,2.2)));
    struct tg_line *line = LINE(P(2, 2), P(8, 8));
    struct tg_geom *l = tg_geom_new_linestring(line);
    struct tg_geom *g2 = tg_geom_new_polygon(POLY(RING(rectangle)));
    struct tg_geom *l2 = tg_geom_new_linestring_zm(line, 0, 0);
    size_t mz = tg_geom_memsize(l);

    assert(tg_geom_poly(l) == NULL);
    assert(tg_geom_covers(l, l));
    assert(tg_geom_covers(l, l2));
    assert(tg_geom_covers(l, pt2));
    assert(!tg_geom_covers(NULL, l));
    assert(!tg_geom_covers(l, NULL));
    assert(!tg_geom_covers(l, lil1));
    assert(!tg_geom_covers(l, pl));
    assert(tg_geom_line(l) == line);
    assert(tg_geom_covers(l, p));
    assert(tg_geom_covers(g2, p));
    assert(tg_geom_covers(g2, l));
    assert(tg_geom_intersects(l, g2));
    assert(tg_geom_intersects(g2, l));
    assert(tg_geom_intersects(l, lil1));

    assert(pointeq(tg_geom_point(l), P(5, 5)));

    tg_geom_free(p);
    tg_geom_free(l);
    tg_geom_free(l2);
    tg_geom_free(g2);
    tg_geom_free(pl);

    struct tg_geom *g;

    g = tg_geom_new_linestring_z(LINE(P(2, 2), P(8, 8)),
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    assert(pointeq(tg_geom_point(g), P(5, 5)));
    assert(tg_geom_intersects(g, lil1));
    tg_geom_free(g);

    g = tg_geom_new_linestring_z(LINE(P(2, 2), P(8, 8)), 0, 0);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_linestring_z(LINE(P(2, 2), P(8, 8)), 0, -100);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_linestring_m(LINE(P(2, 2), P(8, 8)),
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    tg_geom_free(g);

    struct tg_geom *pt = tg_geom_new_point(P(2,2));
    struct tg_geom *pt3 = tg_geom_new_point_empty();
    struct tg_geom *ln = tg_geom_new_linestring(LINE(P(2, 2), P(8, 8)));
    struct tg_geom *ln2 = tg_geom_new_linestring_zm(LINE(P(2, 2), P(8, 8)),0,0);
    pl = tg_geom_new_polygon(POLY(RR(0,2,4,3),RR(2.1,2.1,2.2,2.2)));
    struct tg_geom *pl2 = tg_geom_new_polygon_zm(POLY(RR(0,2,4,3)),0,0);
    struct tg_geom *mpt = tg_geom_new_multipoint((struct tg_point[]) {
        P(2,2),
    }, 1);
    struct tg_geom *mpt2 = tg_geom_new_multipoint((struct tg_point[]) {
        P(20,20),
    }, 1);

    struct tg_geom *mln = tg_geom_new_multilinestring((const struct tg_line*const []) {
        LINE(P(2, 2), P(8, 8))
    }, 1);
    struct tg_geom *mln2 = tg_geom_new_multilinestring((const struct tg_line*const []) {
        LINE(P(20, 20), P(80, 80))
    }, 1);

    struct tg_geom *mpl = tg_geom_new_multipolygon((const struct tg_poly*const []) {
        POLY(RR(0,2,4,3))
    }, 1);
    struct tg_geom *mpl2 = tg_geom_new_multipolygon((const struct tg_poly*const []) {
        POLY(RR(20,22,24,23))
    }, 1);
    struct tg_geom *mgc = tg_geom_new_geometrycollection((const struct tg_geom*const []) {
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(0,2,4,3)),0,0)),
    }, 1);
    struct tg_geom *mgc2 = tg_geom_new_geometrycollection((const struct tg_geom*const []) {
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(20,22,24,23)),0,0)),
    }, 1);
    g = tg_geom_new_linestring_zm(LINE(P(2, 2), P(8, 8)),
        (double[]){1,2,3,4,5,6,7,8}, 8
    );

    assert(tg_geom_dims(g) == 4);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    assert(recteq(tg_geom_rect(g), tg_line_rect(LINE(P(2, 2), P(8, 8)))));

    assert(tg_geom_intersects(g, pt));
    assert(tg_geom_covers(g, pt));
    assert(tg_geom_intersects(g, pt2));
    assert(!tg_geom_intersects(g, pt3));
    assert(tg_geom_intersects(g, ln));
    assert(tg_geom_intersects(g, ln2));
    assert(tg_geom_intersects(g, pl));
    assert(tg_geom_intersects(g, pl2));
    assert(tg_geom_intersects(g, mpt));
    assert(tg_geom_covers(g, mpt));
    assert(!tg_geom_intersects(g, mpt2));
    assert(tg_geom_intersects(g, mln));
    assert(!tg_geom_intersects(g, mln2));
    assert(tg_geom_intersects(g, mpl));
    assert(!tg_geom_intersects(g, mpl2));
    assert(!tg_geom_covers(g, mgc));
    assert(tg_geom_intersects(g, mgc));
    assert(!tg_geom_intersects(g, mgc2));
    assert(!tg_geom_covers(g, gc_geom(tg_geom_new_point_empty())));
    assert(!tg_geom_covers(g, gc_geom(tg_geom_new_multilinestring(0,0))));

    tg_geom_free(pt);
    tg_geom_free(pt2);
    tg_geom_free(pt3);
    tg_geom_free(ln);
    tg_geom_free(ln2);
    tg_geom_free(pl);
    tg_geom_free(pl2);
    tg_geom_free(mpt);
    tg_geom_free(mpt2);
    tg_geom_free(mln);
    tg_geom_free(mln2);
    tg_geom_free(mpl);
    tg_geom_free(mpl2);
    tg_geom_free(mgc);
    tg_geom_free(mgc2);
    tg_geom_free(g);
    tg_geom_free(lil1);
}

void test_geom_polygon() {
    struct tg_ring *lil1 = RR(2,2,3,3);
    struct tg_geom *p = tg_geom_new_point(P(5, 5));
    struct tg_poly *poly = POLY(RING(rectangle));
    struct tg_geom *g2 = tg_geom_new_polygon(poly);
    size_t mz = tg_geom_memsize(g2);

    assert(tg_geom_poly(g2) == poly);
    assert(tg_geom_line(g2) == NULL);
    assert(tg_geom_num_lines(g2) == 0);
    assert(tg_geom_num_polys(g2) == 0);
    assert(tg_geom_num_points(g2) == 0);
    assert(tg_geom_num_geometries(g2) == 0);
    assert(tg_geom_covers(g2, p));
    assert(tg_geom_covers(p, p));
    assert(!tg_geom_covers(p, g2));
    assert(tg_geom_memsize(p) == 24);
    

    tg_geom_free(p);
    tg_geom_free(g2);
    
    struct tg_geom *g;

    g = tg_geom_new_polygon_z(POLY(RING(rectangle)), 0, 0);
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    g2 = tg_geom_clone(g);
    assert(g2 == g);
    tg_geom_free(g2);
    tg_geom_free(g);

    g = tg_geom_new_polygon_z(POLY(RING(rectangle)), 0, -100);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    g2 = tg_geom_clone(g);
    assert(g2 == g);
    tg_geom_free(g2);
    tg_geom_free(g);

    g = tg_geom_new_polygon_m(POLY(RING(rectangle)),
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    g2 = tg_geom_clone(g);
    assert(g2 == g);
    tg_geom_free(g2);
    tg_geom_free(g);

    g = tg_geom_new_polygon_zm(POLY(RING(rectangle)),
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(!tg_geom_covers(g, NULL));
    assert(tg_geom_intersects(g, (struct tg_geom*)lil1));
    assert(tg_geom_dims(g) == 4);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    assert(recteq(tg_geom_rect(g), tg_ring_rect(RING(rectangle))));
    g2 = tg_geom_clone(g);
    assert(g2 == g);
    tg_geom_free(g2);
    tg_geom_free(g);

    struct tg_geom *pt = tg_geom_new_point_zm(P(2,2),0,0);
    struct tg_geom *pt2 = tg_geom_new_point_empty();
    struct tg_geom *ln = tg_geom_new_linestring_zm(LINE(P(2,2),P(3,3)),0,0);
    struct tg_geom *mpt = tg_geom_new_multipoint((struct tg_point[]){
        P(2,2), P(3,3),
    }, 2);
    struct tg_geom *mpt2 = tg_geom_new_multipoint((struct tg_point[]){
        P(20,20), P(30,30),
    }, 2);
    struct tg_geom *mln = tg_geom_new_multilinestring((const struct tg_line*const []){
        LINE(P(2,2),P(3,3)),
    }, 1);
    struct tg_geom *mln2 = tg_geom_new_multilinestring((const struct tg_line*const []){
        LINE(P(20,20),P(30,30)),
    }, 1);
    struct tg_geom *mpl = tg_geom_new_multipolygon((const struct tg_poly*const []){
        POLY(RR(2,2,3,3)),
    }, 1);
    struct tg_geom *mpl2 = tg_geom_new_multipolygon((const struct tg_poly*const []){
        POLY(RR(20,20,30,30)),
    }, 1);
    struct tg_geom *mgc = tg_geom_new_geometrycollection((const struct tg_geom*const []){
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(2,2,3,3)),0,0)),
    }, 1);
    struct tg_geom *mgc2 = tg_geom_new_geometrycollection((const struct tg_geom*const []){
        gc_geom(tg_geom_new_polygon_zm(POLY(RR(20,20,30,30)),0,0)),
    }, 1);
    struct tg_geom *mgc3 = tg_geom_new_geometrycollection(0,0);

    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    struct tg_poly *poly2 = POLY(RING(octagon), small);
    g = tg_geom_new_polygon(poly2);
    size_t mz2 = tg_geom_memsize(g);
    assert(recteq(tg_geom_rect(g), tg_ring_rect(RING(octagon))));
    assert(tg_geom_covers(g, g));
    assert(tg_geom_intersects(g, (struct tg_geom*)lil1));
    assert(tg_geom_intersects(g, g));
    assert(tg_geom_intersects(g, pt));
    assert(tg_geom_covers(g, pt));
    assert(!tg_geom_intersects(g, pt2));
    assert(tg_geom_intersects(g, ln));
    assert(tg_geom_covers(g, ln));
    assert(tg_geom_intersects(g, mpt));
    assert(!tg_geom_intersects(g, mpt2));
    assert(tg_geom_covers(g, mpt));
    assert(tg_geom_intersects(g, mln));
    assert(!tg_geom_intersects(g, mln2));
    assert(tg_geom_intersects(g, mpl));
    assert(!tg_geom_intersects(g, mpl2));
    assert(tg_geom_intersects(g, mgc));
    assert(!tg_geom_intersects(g, mgc2));
    assert(tg_geom_covers(g, mgc));
    assert(!tg_geom_covers(mgc, g));
    assert(!tg_geom_covers(g, mgc2));
    assert(!tg_geom_covers(mgc, NULL));
    assert(!tg_geom_covers(g, mgc3));
    assert(!tg_geom_covers(g, gc_geom(tg_geom_new_point_empty())));

    assert(tg_geom_poly(g) == poly2);
    assert(mz2 > mz);
    tg_geom_free(g);
    tg_geom_free(pt);
    tg_geom_free(pt2);
    tg_geom_free(mpt);
    tg_geom_free(mpt2);
    tg_geom_free(mln);
    tg_geom_free(mln2);
    tg_geom_free(mpl);
    tg_geom_free(mpl2);
    tg_geom_free(mgc);
    tg_geom_free(mgc2);
    tg_geom_free(mgc3);
    tg_geom_free(ln);


    struct tg_geom *g1 = (struct tg_geom*)RING(P(1,1),P(2,1),P(2,2),P(1,2),P(1,1));
    assert(!tg_geom_intersects_rect(g1, R(5,5,6,6)));
    assert(tg_geom_intersects_rect(g1, R(1.5,1.5,2.5,2.5)));

}

void test_geom_multipoint() {
    struct tg_geom *lil1 = tg_geom_new_polygon_zm(POLY(RR(-180, -90, 180, 90)), 0, 0);
    struct tg_point points[] = { az };
    int npoints = sizeof(points)/sizeof(struct tg_point);
    
    struct tg_geom *g = tg_geom_new_multipoint(points, npoints);
    size_t mz = tg_geom_memsize(g);

    assert(tg_geom_intersects(g, lil1));
    assert(tg_geom_num_points(g) == npoints);
    assert(pointeq(tg_geom_point_at(g, 100), points[100]));

    tg_geom_free(g);

    g = tg_geom_new_multipoint(points, -100);
    assert(!tg_geom_covers(g, lil1));
    assert(!tg_geom_intersects(g, lil1));
    assert(tg_geom_memsize(g) < mz);
    assert(tg_geom_num_points(g) == 0);
    assert(pointeq(tg_geom_point_at(g, 100), (struct tg_point){ 0 }));
    tg_geom_free(g);


    g = tg_geom_new_multipoint_z(points, npoints, 0, 0);
    assert(!tg_geom_covers(g, lil1));
    assert(tg_geom_covers(g, g));
    assert(tg_geom_intersects(g, lil1));
    assert(!tg_geom_intersects(g, NULL));
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multipoint_z(points, npoints, 0, -100);
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multipoint_m(points, npoints,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    tg_geom_free(g);

    g = tg_geom_new_multipoint_zm(points, npoints,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_dims(g) == 4);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    assert(recteq(tg_geom_rect(g), tg_ring_rect(RING(az))));


    struct tg_rect shell = tg_point_rect(points[0]);
    for (int i = 0; i < npoints; i++) {
        shell = tg_rect_expand_point(shell, points[i]);
    }
    assert(recteq(tg_geom_rect(g), shell));

    tg_geom_free(g);
    tg_geom_free(lil1);


}

void test_geom_multilinestring() {
    struct tg_geom *lil1 = tg_geom_new_polygon(POLY(RR(5, 5, 20, 20)));
    struct tg_line const *lines[] = { 
        LINE(P(10, 10), P(20, 20)),
        LINE(P(40, 40), P(50, 50)),
    };
    int nlines = sizeof(lines)/sizeof(struct tg_line*);
    struct tg_geom *g = tg_geom_new_multilinestring(lines, nlines);
    size_t mz = tg_geom_memsize(g);

    assert(tg_geom_intersects(g, lil1));
    assert(!tg_geom_intersects(g, NULL));
    assert(tg_geom_num_lines(g) == nlines);
    assert(tg_geom_line_at(g, 1) == lines[1]);
    tg_geom_free(g);

    g = tg_geom_new_multilinestring(lines, -100);
    assert(tg_geom_memsize(g) < mz);
    assert(tg_geom_num_points(g) == 0);
    assert(pointeq(tg_geom_point_at(g, 100), (struct tg_point){ 0 }));
    tg_geom_free(g);

    g = tg_geom_new_multilinestring_z(lines, nlines, 0, 0);
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multilinestring_z(lines, nlines, 0, -100);
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multilinestring_m(lines, nlines,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    tg_geom_free(g);

    g = tg_geom_new_multilinestring_zm(lines, nlines,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );

    assert(tg_geom_dims(g) == 4);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    assert(recteq(tg_geom_rect(g), R(10,10,50,50)));
    tg_geom_free(g);

    tg_geom_free(lil1);

}

void test_geom_multipolygon() {
    struct tg_geom *lil1 = tg_geom_new_polygon_zm(POLY(RR(-180, -90, 180, 90)), 0, 0);
    struct tg_poly *p1 = POLY(RING(az));
    struct tg_poly *p2 = POLY(RING(tx));
    struct tg_poly *p3 = POLY(RING(ri));
    struct tg_poly const *polys[] = {  p1, p2, p3 };
    int npolys = sizeof(polys)/sizeof(struct tg_poly*);
    struct tg_geom *g = tg_geom_new_multipolygon(polys, npolys);
    size_t mz = tg_geom_memsize(g);

    assert(tg_geom_intersects(g, lil1));
    assert(!tg_geom_intersects(g, NULL));
    assert(tg_geom_num_polys(g) == npolys);
    assert(tg_geom_poly_at(g, 0) == polys[0]);
    assert(tg_geom_poly_at(g, 1) == polys[1]);
    assert(tg_geom_poly_at(g, 2) == polys[2]);
    tg_geom_free(g);

    g = tg_geom_new_multipolygon(polys, -100);
    assert(tg_geom_memsize(g) < mz);
    assert(tg_geom_num_points(g) == 0);
    assert(pointeq(tg_geom_point_at(g, 100), (struct tg_point){ 0 }));
    tg_geom_free(g);


    g = tg_geom_new_multipolygon_z(polys, npolys, 0, 0);
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multipolygon_z(polys, npolys, 0, -100);
    assert(tg_geom_memsize(g) == mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multipolygon_m(polys, npolys,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_memsize(g) > mz);
    assert(tg_geom_dims(g) == 3);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    tg_geom_free(g);

    g = tg_geom_new_multipolygon_zm(polys, npolys,
        (double[]){1,2,3,4,5,6,7,8}, 8
    );
    assert(tg_geom_dims(g) == 4);
    assert(tg_geom_num_extra_coords(g) == 8);
    assert(tg_geom_extra_coords(g)[1] == 2);
    
    struct tg_rect shell = tg_poly_rect(p1);
    shell = tg_rect_expand(shell, tg_poly_rect(p2));
    shell = tg_rect_expand(shell, tg_poly_rect(p3));

    assert(recteq(tg_geom_rect(g), shell));
    tg_geom_free(g);
    tg_geom_free(lil1);
}

void test_geom_geometrycollection() {
    struct tg_geom *lil1 = tg_geom_new_polygon_zm(POLY(RR(-180, -90, 180, 90)), 0, 0);
    struct tg_geom const *geoms[] = { 
        gc_geom(tg_geom_new_polygon(POLY(RING(az)))),
        gc_geom(tg_geom_new_linestring(LINE(P(10, 10), P(20, 20)))),
    };
    int ngeoms = sizeof(geoms)/sizeof(struct tg_geom*);
    struct tg_geom *g = tg_geom_new_geometrycollection(geoms, ngeoms);
    size_t mz = tg_geom_memsize(g);
    assert(tg_geom_intersects(g, lil1));
    assert(tg_geom_covers(lil1, g));
    assert(!tg_geom_covers(g, lil1));
    assert(!tg_geom_intersects(g, NULL));
    assert(tg_geom_num_geometries(g) == ngeoms);
    assert(tg_geom_geometry_at(g, 0) == geoms[0]);
    assert(tg_geom_geometry_at(g, 1) == geoms[1]);
    assert(tg_geom_z(g) == 0);
    assert(tg_geom_m(g) == 0);

    assert(tg_geom_covers(
        gc_geom(tg_geom_new_geometrycollection((const struct tg_geom*const []){
            gc_geom(tg_geom_new_polygon_zm(POLY(RR(20,20,30,30)),0,0)),
        }, 1)),
        gc_geom(tg_geom_new_geometrycollection((const struct tg_geom*const []){
            gc_geom(tg_geom_new_point_empty()),
            gc_geom(tg_geom_new_polygon_zm(POLY(RR(20,20,30,30)),0,0)),
        }, 2))
    ));

    assert(mz > 0);
    tg_geom_free(g);
    tg_geom_free(lil1);
}

void test_geom_various() {
    tg_geom_free(NULL);
    assert(tg_geom_is_empty(NULL) == true);
    assert(tg_geom_intersects(NULL, NULL) == false);
    assert(tg_geom_covers(NULL, NULL) == false);
    assert(tg_geom_dims(NULL) == 0);
    assert(tg_geom_has_z(NULL) == false);
    assert(tg_geom_has_m(NULL) == false);
    assert(tg_geom_is_empty(NULL) == true);
    assert(tg_geom_extra_coords(NULL) == NULL);
    assert(tg_geom_num_extra_coords(NULL) == 0);
    assert(tg_geom_memsize(NULL) == 0);
    assert(tg_geom_typeof(NULL) == 0);
    assert(tg_geom_line(NULL) == NULL);
    assert(recteq(tg_geom_rect(NULL), R(0,0,0,0)));
    assert(pointeq(tg_geom_point(NULL), P(0, 0)));
    assert(tg_geom_poly(NULL) == NULL);
    assert(tg_geom_clone(NULL) == NULL);
    assert(tg_geom_num_points(NULL) == 0);
    assert(tg_geom_num_lines(NULL) == 0);
    assert(tg_geom_num_polys(NULL) == 0);
    assert(tg_geom_num_geometries(NULL) == 0);
    assert(pointeq(tg_geom_point_at(NULL, 0), P(0,0)));
    assert(tg_geom_line_at(NULL, 0) == NULL);
    assert(tg_geom_poly_at(NULL, 0) == NULL);
    assert(tg_geom_geometry_at(NULL, 0) == NULL);
    struct tg_geom *p = tg_geom_new_point(P(10, 10));
    struct tg_geom *p2 = tg_geom_clone(p);
    tg_geom_free(p);
    tg_geom_free(p2);



    struct tg_geom *mpt = tg_geom_new_multipoint(
        (const struct tg_point[]) {
            P(10, 10),
        }, 1);
    assert(mpt != NULL);
    assert(tg_geom_num_points(mpt) == 1);
    assert(tg_geom_is_empty(mpt) == false);
    tg_geom_free(mpt);



    struct tg_geom *mln = tg_geom_new_multilinestring(
        (const struct tg_line *const[]) {
            LINE(P(4, 5),P(6, 7)),
            NULL,
            LINE(P(8, 9),P(10, 11)),
        }, 3);
    assert(mln != NULL);
    assert(tg_geom_num_lines(mln) == 3);
    assert(tg_geom_is_empty(mln) == false);
    tg_geom_free(mln);

    mln = tg_geom_new_multilinestring(
        (const struct tg_line *const[]) {
            NULL,
            (struct tg_line*)gc_geom(tg_geom_new_linestring_zm(0,0,0)),
            NULL,
            LINE(P(8, 9),P(10, 11)),
        }, 4);
    assert(mln != NULL);
    assert(tg_geom_num_lines(mln) == 4);
    assert(tg_geom_is_empty(mln) == false);
    tg_geom_free(mln);


    struct tg_geom *mpl = tg_geom_new_multipolygon(
        (const struct tg_poly *const[]) {
            NULL,
            POLY(RING(az)),
            NULL,
            POLY(RR(1,1,2,2)),
        }, 4);
    assert(mpl != NULL);
    assert(tg_geom_num_polys(mpl) == 4);
    assert(tg_geom_poly_at(mpl, 0) == NULL);
    assert(tg_geom_poly_at(mpl, 1) != NULL);
    assert(tg_geom_poly_at(mpl, 2) == NULL);
    assert(tg_geom_is_empty(mpl) == false);
    tg_geom_free(mpl);

    mpl = tg_geom_new_multipolygon(NULL, 0);
    assert(mpl != NULL);
    assert(tg_geom_num_polys(mpl) == 0);
    assert(tg_geom_is_empty(mpl) == true);
    tg_geom_free(mpl);

    mpl = tg_geom_new_multipolygon_empty();
    assert(mpl != NULL);
    assert(tg_geom_num_polys(mpl) == 0);
    assert(tg_geom_is_empty(mpl) == true);
    tg_geom_free(mpl);
    struct tg_geom *gc = tg_geom_new_geometrycollection(
        (const struct tg_geom *const[]) {
            NULL,
            gc_geom(tg_geom_new_point_empty()),
            gc_geom(tg_geom_new_point_zm(P(10,10),20,30)),
            NULL,
        }, 4);
    assert(gc != NULL);
    assert(tg_geom_num_geometries(gc) == 4);
    assert(tg_geom_is_empty(gc) == false);
    tg_geom_free(gc);

    assert(tg_geom_covers_point(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        P(15, 15)));
    assert(tg_geom_covers_xy(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        15, 15));
    assert(!tg_geom_covers_point(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        P(30, 15)));
    assert(!tg_geom_covers_xy(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        30, 15));

    assert(tg_geom_intersects_point(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        P(15, 15)));
    assert(tg_geom_intersects_xy(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        15, 15));
    assert(!tg_geom_intersects_point(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        P(30, 15)));
    assert(!tg_geom_intersects_xy(
        gc_geom(tg_geom_new_polygon(POLY(RR(10,10,20,20)))),
        30, 15));
}

void test_geom_empty() {
    assert(tg_geom_is_empty(NULL));
    struct tg_geom *g = tg_geom_new_point_empty();
    struct tg_geom *g2 = tg_geom_new_point_z(P(10, 10), 1);
    assert(tg_geom_typeof(g) == TG_POINT);
    assert(tg_geom_is_empty(g));
    assert(!tg_geom_intersects(g, NULL));
    assert(!tg_geom_intersects(g, g));
    assert(!tg_geom_intersects(g2, g));
    assert(!tg_geom_intersects(g, g2));
    assert(!tg_geom_covers(g, g2));
    assert(!tg_geom_covers(g2, g));
    assert(!tg_geom_has_z(g));
    assert(!tg_geom_has_m(g));
    tg_geom_free(g);
    tg_geom_free(g2);

    g = tg_geom_new_linestring_empty();
    assert(tg_geom_typeof(g) == TG_LINESTRING);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_line(g) == NULL);
    tg_geom_free(g);

    g = tg_geom_new_polygon_empty();
    assert(tg_geom_typeof(g) == TG_POLYGON);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_poly(g) == NULL);
    tg_geom_free(g);

    g = tg_geom_new_multipoint_empty();
    assert(tg_geom_typeof(g) == TG_MULTIPOINT);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_num_points(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multilinestring_empty();
    assert(tg_geom_typeof(g) == TG_MULTILINESTRING);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_num_lines(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_multipolygon_empty();
    assert(tg_geom_typeof(g) == TG_MULTIPOLYGON);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_num_polys(g) == 0);
    tg_geom_free(g);

    g = tg_geom_new_geometrycollection_empty();
    assert(tg_geom_typeof(g) == TG_GEOMETRYCOLLECTION);
    assert(tg_geom_is_empty(g));
    assert(tg_geom_num_geometries(g) == 0);
    tg_geom_free(g);
}

void test_geom_type_string() {
    assert(strcmp(tg_geom_type_string(0), "Unknown") == 0);
    assert(strcmp(tg_geom_type_string(TG_POINT-1), "Unknown") == 0);
    assert(strcmp(tg_geom_type_string(TG_POINT), "Point") == 0);
    assert(strcmp(tg_geom_type_string(TG_LINESTRING), "LineString") == 0);
    assert(strcmp(tg_geom_type_string(TG_POLYGON), "Polygon") == 0);
    assert(strcmp(tg_geom_type_string(TG_MULTIPOINT), "MultiPoint") == 0);
    assert(strcmp(tg_geom_type_string(TG_MULTILINESTRING), "MultiLineString") == 0);
    assert(strcmp(tg_geom_type_string(TG_MULTIPOLYGON), "MultiPolygon") == 0);
    assert(strcmp(tg_geom_type_string(TG_GEOMETRYCOLLECTION), "GeometryCollection") == 0);
    assert(strcmp(tg_geom_type_string(TG_GEOMETRYCOLLECTION+1), "Unknown") == 0);
}

void test_geom_extra_coords() {
    struct tg_geom *g;
    for (int i = 0; i < 20; i++) {
        g = tg_geom_new_linestring_z(
            LINE(P(2, 2), P(8, 8)),
            (double[]){1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20}, i
        );
        assert(g);
        assert(tg_geom_num_extra_coords(g) == i);
        for (int j = 0; j < i; j++) {
            assert(tg_geom_extra_coords(g)[j] == j+1);
        }
        tg_geom_free(g);
    }
}

void test_geom_copy(void) {
    struct tg_geom *geom;
    struct tg_geom *geom2;
    geom = tg_parse_wkt("POINT(1 2)");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(tg_geom_covers_xy(geom, 1, 2));
    tg_geom_free(geom2);
    tg_geom_free(geom);


    geom = tg_parse_wkt("LINESTRING(1 1,2 2)");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(tg_geom_covers_xy(geom, 1.5, 1.5));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("LINESTRING(1 1 3,2 2 4)");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(tg_geom_covers_xy(geom, 1.5, 1.5));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("POLYGON((1 1, 2 1, 2 2, 1 2, 1 1))");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(tg_geom_covers_xy(geom, 1.3, 1.3));
    assert(tg_geom_covers_xy(geom, 1.5, 1.5));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("POLYGON((1 1 3, 2 1 4, 2 2 5, 1 2 6, 1 1 7))");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(tg_geom_covers_xy(geom, 1.3, 1.3));
    assert(tg_geom_covers_xy(geom, 1.5, 1.5));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("POLYGON ((1 1, 2 1, 2 2, 1 2, 1 1), (1.2 1.2, 1.4 1.2, 1.4 1.4, 1.2 1.4, 1.2 1.2))");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(!tg_geom_covers_xy(geom, 1.3, 1.3));
    assert(tg_geom_covers_xy(geom, 1.5, 1.5));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("MULTIPOINT((0.91 1.49), (1.21 1.66), (1.37 1.42), (1.21 1.27), (1.11 1.51))");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(!tg_geom_covers_xy(geom, 1.20, 1.65));
    assert(tg_geom_covers_xy(geom, 1.21, 1.66));
    tg_geom_free(geom2);
    tg_geom_free(geom);

    geom = tg_parse_wkt("MULTIPOINT((0.91 1.49 3), (1.21 1.66 4), (1.37 1.42 5))");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(!tg_geom_covers_xy(geom, 1.20, 1.65));
    assert(tg_geom_covers_xy(geom, 1.21, 1.66));
    tg_geom_free(geom2);
    tg_geom_free(geom);


    geom = tg_parse_wkt("INTENTIONAL ERROR");
    assert(geom);
    geom2 = tg_geom_copy(geom);
    assert(geom2);
    assert(!tg_geom_covers_xy(geom, 1.20, 1.65));
    assert(!tg_geom_covers_xy(geom, 1.21, 1.66));
    tg_geom_free(geom2);
    tg_geom_free(geom);



    assert(!tg_geom_copy(0));
}

void chaos_parse_copy(const char *wkt) {
    struct tg_geom *geom = NULL;
    int must_fail = 0;
    // copy chaos
    while (!geom) {
        geom = tg_parse_wkt(wkt);
    }
    struct tg_geom *geom2;
    must_fail = 100;
    while (must_fail > 0) {
        geom2 = tg_geom_copy(geom);
        if (!geom2) {
            must_fail--;
            continue;
        }
        tg_geom_free(geom2);
        geom2 = NULL;
    }
    tg_geom_free(geom);

}

void test_geom_chaos(void) {
    chaos_parse_copy("POINT(1 2)");
    chaos_parse_copy("LINESTRING(1 1,2 2)");
    chaos_parse_copy("LINESTRING(1 1 3,2 2 4)");
    chaos_parse_copy("POLYGON((1 1, 2 1, 2 2, 1 2, 1 1))");
    chaos_parse_copy("POLYGON((1 1 3, 2 1 4, 2 2 5, 1 2 6, 1 1 7))");
    chaos_parse_copy("POLYGON ((1 1, 2 1, 2 2, 1 2, 1 1), (1.2 1.2, 1.4 1.2, 1.4 1.4, 1.2 1.4, 1.2 1.2))");
    chaos_parse_copy("MULTIPOINT((0.91 1.49), (1.21 1.66), (1.37 1.42), (1.21 1.27), (1.11 1.51))");
    chaos_parse_copy("MULTIPOINT((0.91 1.49 3), (1.21 1.66 4), (1.37 1.42 5))");
    chaos_parse_copy("INTENTIONAL ERROR");

}

void test_geom_error(void) {
    struct tg_geom *geom;
    geom = tg_geom_new_error(0);
    assert(strcmp(tg_geom_error(geom), "no memory") == 0);
    tg_geom_free(geom);
    
    geom = tg_geom_new_error("");
    assert(strcmp(tg_geom_error(geom), "") == 0);
    tg_geom_free(geom);

    geom = tg_geom_new_error("hello");
    assert(strcmp(tg_geom_error(geom), "hello") == 0);
    tg_geom_free(geom);

}

int main(int argc, char **argv) {
    do_test(test_geom_point);
    do_test(test_geom_linestring);
    do_test(test_geom_polygon);
    do_test(test_geom_multipoint);
    do_test(test_geom_multilinestring);
    do_test(test_geom_multipolygon);
    do_test(test_geom_geometrycollection);
    do_test(test_geom_type_string);
    do_test(test_geom_empty);
    do_test(test_geom_extra_coords);
    do_test(test_geom_various);
    do_test(test_geom_copy);
    do_chaos_test(test_geom_chaos);
    do_test(test_geom_error);
    return 0;
}
