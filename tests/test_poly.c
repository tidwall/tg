#include "tests.h"

void test_poly_rect() {
    DUAL_POLY_TEST(POLY(RING(octagon)), {
        assert(recteq(tg_poly_rect(poly), R(0, 0, 10, 10)));
    });
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        assert(recteq(tg_poly_rect(poly), R(0, 0, 10, 10)));
    });
}

void test_poly_move() {
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        struct tg_poly *poly2 = tg_poly_move_gc(poly, 5, 8);
        assert(recteq(tg_poly_rect(poly2), R(5, 8, 15, 18)));
    });
    struct tg_poly *poly = POLY(RR(0, 0, 10, 10), RR(2, 2, 8, 8));
    struct tg_poly *poly2 = tg_poly_move_gc(poly, 5, 8);
    assert(recteq(tg_poly_rect(poly2), R(5, 8, 15, 18)));
    assert(tg_poly_num_holes(poly2) == 1);
    assert(recteq(tg_ring_rect(tg_poly_hole_at(poly2, 0)), R(7, 10, 13, 16)));
    assert(tg_poly_move_gc(NULL, 0, 0) == NULL);
    assert(tg_poly_move_gc(POLY(NULL), 0, 0) == NULL);
}

void test_poly_covers_point() {
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        assert(!tg_poly_covers_point(poly, P(0, 0)));
        assert(tg_poly_covers_point(poly, P(0, 5)));
        assert(tg_poly_covers_point(poly, P(3, 5)));
        assert(!tg_poly_covers_point(poly, P(5, 5)));
        assert(!tg_poly_covers_xy(poly, 0, 0));
        assert(tg_poly_covers_xy(poly, 0, 5));
        assert(tg_poly_covers_xy(poly, 3, 5));
        assert(!tg_poly_covers_xy(poly, 5, 5));
    });
}

void test_poly_intersects_point() {
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        assert(!tg_poly_intersects_point(poly, P(0, 0)));
        assert(tg_poly_intersects_point(poly, P(0, 5)));
        assert(tg_poly_intersects_point(poly, P(3, 5)));
        assert(!tg_poly_intersects_point(poly, P(5, 5)));
    });
    assert(!tg_poly_intersects_point(NULL, P(0, 0)));
    assert(!tg_poly_intersects_point(POLY(NULL), P(0, 0)));
}

void test_poly_covers_rect() {
    struct tg_ring *ring = RING({0, 0}, {10, 0}, {10, 10}, {0, 10}, {0, 0});
    struct tg_ring *hole = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(ring, hole), {
        assert(tg_poly_covers_rect(poly, R(0, 0, 4, 4)));
        assert(!tg_poly_covers_rect(poly, R(0, 0, 5, 5)));
        assert(!tg_poly_covers_rect(poly, R(2, 2, 6, 6)));
        assert(!tg_poly_covers_rect(poly, R(4.1, 4.1, 5.9, 5.9)));
        assert(!tg_poly_covers_rect(poly, R(4.1, 4.1, 5.9, 5.9)));
    });
    assert(!tg_poly_covers_rect(NULL, R(0, 0, 0, 0)));
    assert(!tg_poly_covers_rect(POLY(NULL), R(0, 0, 0, 0)));
}

void test_poly_intersects_rect() {
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        assert(tg_poly_intersects_rect(poly, R(0, 4, 4, 6)));
        assert(tg_poly_intersects_rect(poly, R(-1, 4, 4, 6)));
        assert(tg_poly_intersects_rect(poly, R(4, 4, 6, 6)));
        assert(!tg_poly_intersects_rect(poly, R(4.1, 4.1, 5.9, 5.9)));
        assert(!tg_poly_intersects_rect(poly, R(0, 0, 1.4, 1.4)));
        assert(tg_poly_intersects_rect(poly, R(0, 0, 1.5, 1.5)));
        assert(!tg_poly_intersects_rect(poly, tg_rect_move(R(0, 0, 10, 10), 11, 0)));
    });
    assert(!tg_poly_intersects_rect(NULL, R(0, 0, 0, 0)));
    assert(!tg_poly_intersects_rect(POLY(NULL), R(0, 0, 0, 0)));
}

void test_poly_covers_line() {
    struct tg_ring *small = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), small), {
        assert( tg_poly_covers_line(poly, LINE(P(3, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert(!tg_poly_covers_line(poly, LINE(P(-1, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert( tg_poly_covers_line(poly, LINE(P(4, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert(!tg_poly_covers_line(poly, LINE(P(5, 3), P(3, 7), P(7, 7), P(7, 3))));
    });
}

void test_poly_intersects_line() {
    struct tg_ring *hole = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    DUAL_POLY_TEST(POLY(RING(octagon), hole), {
        assert( tg_poly_intersects_line(poly, LINE(P(3, 3), P(4, 4))));
        assert( tg_poly_intersects_line(poly, LINE(P(-1, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert( tg_poly_intersects_line(poly, LINE(P(4, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert( tg_poly_intersects_line(poly, LINE(P(5, 3), P(3, 7), P(7, 7), P(7, 3))));
        assert(!tg_poly_intersects_line(poly, tg_line_move_gc(LINE(P(5, 3), P(3, 7), P(7, 7), P(7, 3)), 11, 0)));
    });
}

void test_poly_covers_poly() {
    struct tg_ring *hole1 = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    struct tg_ring *hole2 = RING({5, 4}, {7, 4}, {7, 6}, {5, 6}, {5, 4});
    struct tg_poly *poly1 = POLY(RING(octagon), hole1);
    struct tg_poly *poly2 = POLY(RING(octagon), hole2);

    assert(!tg_poly_covers_poly(poly1, POLY(hole2)));
    assert(!tg_poly_covers_poly(poly1, poly2));

    DUAL_POLY_TEST(POLY(RING(octagon), hole1), {
        assert(tg_poly_covers_poly(poly, poly1));
        assert(!tg_poly_covers_poly(poly, tg_poly_move_gc(poly1, 1, 0)));
        assert(tg_poly_covers_poly(poly, POLY(hole1)));
        assert(!tg_poly_covers_poly(poly,POLY(hole2)));
    });
}

void test_poly_clockwise() {
    assert(!tg_poly_clockwise(POLY(RING(bowtie))));
    assert(!tg_poly_clockwise(NULL));
}

void test_poly_various() {
    assert(tg_poly_clone(NULL) == NULL);
    assert(tg_poly_exterior(NULL) == NULL);
    assert(tg_poly_num_holes(NULL) == 0);
    assert(tg_poly_hole_at(NULL, 0) == NULL);
    assert(recteq(tg_poly_rect(NULL), R(0, 0, 0, 0)));
    struct tg_ring *exterior = RING(octagon);
    struct tg_ring *hole = RING({4, 4}, {6, 4}, {6, 6}, {4, 6}, {4, 4});
    struct tg_poly *poly = POLY(exterior, hole);
    struct tg_poly *poly2 = tg_poly_clone(poly);
    struct tg_poly *poly3 = tg_poly_clone(poly2);
    assert(poly == poly2);
    assert(poly2 == poly3);
    tg_poly_free(poly3);


    assert(recteq(tg_poly_rect(poly), tg_poly_rect(poly2)));
    assert(tg_poly_exterior(poly) == tg_poly_exterior(poly2));
    assert(tg_poly_num_holes(poly) == tg_poly_num_holes(poly2));
    assert(tg_poly_num_holes(poly) == 1);
    assert(tg_poly_hole_at(poly, -1) == NULL);
    assert(tg_poly_hole_at(poly, 0) == hole);
    assert(tg_poly_hole_at(poly, 1) == NULL);
    tg_poly_free(poly2);

    assert(tg_poly_contains_line(poly, 0) == 0);
    assert(tg_poly_contains_line(0, 0) == 0);

    assert(tg_poly_touches_line(poly, 0) == 0);
    assert(tg_poly_touches_line(0, 0) == 0);
}

void test_poly_casting() {
    struct tg_point points[] = { az };
    int npoints = sizeof(points)/sizeof(struct tg_point);
    struct tg_ring *exterior = tg_ring_new_ix(points, npoints, TG_YSTRIPES|TG_NATURAL);
    assert(exterior);

    struct tg_poly *poly1 = tg_poly_new(exterior, 0, 0);
    assert(poly1);


    struct tg_point hpoints[] = { P(-112, 33), P(-111, 33), P(-111, 34), P(-112, 34), P(-112, 33) };
    int hnpoints = sizeof(hpoints)/sizeof(struct tg_point);
    struct tg_ring *hole = tg_ring_new(hpoints, hnpoints);
    assert(hole);

    struct tg_poly *poly2 = tg_poly_new(exterior, (const struct tg_ring **)&hole, 1);
    assert(poly2);

    struct tg_poly *poly3 = tg_poly_new(hole, 0, 0);
    assert(poly3);

    assert((void*)poly1 == (void*)exterior);
    assert((void*)poly2 != (void*)exterior);
    assert((void*)poly3 == (void*)hole);

    assert(tg_poly_memsize(NULL) == 0);
    assert(tg_poly_memsize(poly1) == tg_ring_memsize(exterior));
    assert(tg_poly_memsize(poly2) > tg_ring_memsize(exterior) + 
        tg_ring_memsize(hole));

    struct tg_line *line = tg_line_new((struct tg_point[]) { 
        P(-112+0.1, 33+0.1), P(-111-0.1, 34-0.1),
    }, 2);
    assert(line);

    assert(tg_ring_contains_line(exterior, line, true, 0));
    assert(tg_poly_covers_line(poly1, line));
    assert(tg_ring_contains_line(hole, line, true, 0));
    assert(!tg_poly_covers_line(poly2, line));
    assert(!tg_poly_covers_line(poly2, NULL));

    assert(tg_ring_intersects_line(exterior, line, true));
    assert(tg_poly_intersects_line(poly1, line));
    assert(tg_ring_intersects_line(hole, line, true));
    assert(!tg_poly_intersects_line(poly2, line));
    assert(!tg_poly_intersects_line(poly2, NULL));

    assert(tg_poly_covers_poly(poly1, poly2));
    assert(tg_poly_covers_poly(poly2, poly3));
    assert(tg_poly_covers_poly(poly1, poly3));
    assert(!tg_poly_covers_poly(poly2, poly1));
    assert(!tg_poly_covers_poly(poly3, poly1));

    assert(tg_poly_intersects_poly(poly1, poly1));
    assert(tg_poly_intersects_poly(poly1, poly2));
    assert(tg_poly_intersects_poly(poly1, poly3));
    assert(tg_poly_intersects_poly(poly2, poly1));
    assert(tg_poly_intersects_poly(poly2, poly2));
    assert(tg_poly_intersects_poly(poly2, poly3));
    assert(tg_poly_intersects_poly(poly3, poly1));
    assert(tg_poly_intersects_poly(poly3, poly2));
    assert(tg_poly_intersects_poly(poly3, poly3));

    struct tg_poly *poly4 = tg_poly_clone(poly1);
    assert(poly4);
    struct tg_poly *poly5 = tg_poly_clone(poly2);
    assert(poly5);
    struct tg_poly *poly6 = tg_poly_clone(poly3);
    assert(poly6);

    assert((void*)poly4 == (void*)exterior);
    assert((void*)poly5 == (void*)poly2);
    assert((void*)poly6 == (void*)hole);

    struct tg_ring *exterior2 = tg_ring_new_ix(points, npoints, TG_YSTRIPES|TG_NATURAL);
    assert(exterior2);

    struct tg_ring *exteriorB = tg_ring_move(exterior2, -200, -200);
    assert(exteriorB);
    struct tg_line *lineB = tg_line_move(line, -200, -200);
    assert(lineB);
    struct tg_ring *holeB = tg_ring_move(hole, -200, -200);
    assert(holeB);
    struct tg_poly *poly1B = tg_poly_move(poly1, -200, -200);
    assert(poly1B);
    struct tg_poly *poly2B = tg_poly_move(poly2, -200, -200);
    assert(poly2B);

    assert(tg_ring_contains_line(exteriorB, lineB, true, 0));
    assert(tg_poly_covers_line(poly1B, lineB));
    assert(tg_ring_contains_line(holeB, lineB, true, 0));
    assert(!tg_poly_covers_line(poly2B, lineB));
    assert(!tg_poly_covers_line(poly2B, NULL));

    tg_ring_free(exteriorB);
    tg_line_free(lineB);
    tg_ring_free(holeB);
    tg_poly_free(poly1B);
    tg_poly_free(poly2B);

    tg_ring_free(exterior2);
    tg_ring_free(exterior);
    tg_poly_free(poly1);
    tg_poly_free(poly2);
    tg_poly_free(poly3);
    tg_ring_free(hole);
    tg_line_free(line);
    tg_poly_free(poly4);
    tg_poly_free(poly5);
    tg_poly_free(poly6);
}

void test_poly_copy(void) {
    struct tg_geom *geom = tg_parse_wkt("POLYGON ("
        "(1.16 1.92, 1.62 1.92, 1.62 1.52, 1.16 1.52, 1.16 1.92), "
        "(1.2 1.8, 1.3 1.8, 1.3 1.7, 1.2 1.7, 1.2 1.8), "
        "(1.4 1.7, 1.504 1.7, 1.504 1.604, 1.4 1.604, 1.4 1.7))");
    const struct tg_poly *poly = tg_geom_poly(geom);
    assert(poly);

    struct tg_poly *poly2 = tg_poly_copy(poly);
    assert(poly2);

    assert(!tg_geom_intersects_xy((struct tg_geom*)poly2, 1.25, 1.77));
    assert(!tg_geom_intersects_xy((struct tg_geom*)poly2, 1.44, 1.68));
    assert(tg_geom_intersects_xy((struct tg_geom*)poly2, 1.32, 1.82));

    struct tg_poly *poly3 = tg_poly_copy((struct tg_poly*)tg_poly_exterior(poly2));
    assert(poly3);
    assert(tg_poly_num_holes(poly3) == 0);

    assert(tg_geom_intersects_xy((struct tg_geom*)poly3, 1.25, 1.77));
    assert(tg_geom_intersects_xy((struct tg_geom*)poly3, 1.44, 1.68));
    assert(tg_geom_intersects_xy((struct tg_geom*)poly3, 1.32, 1.82));


    tg_poly_free(poly3);
    tg_poly_free(poly2);
    tg_geom_free(geom);

    assert(tg_poly_copy(0) == 0);
}

void test_poly_chaos(void) {
    struct tg_poly *poly = NULL;
    int must_fail = 0;
    
    // copy chaos
    while (!poly) {
        struct tg_point points[] = { az };
        struct tg_geom *geom = tg_parse_wkt("POLYGON ("
            "(1.16 1.92, 1.62 1.92, 1.62 1.52, 1.16 1.52, 1.16 1.92), "
            "(1.2 1.8, 1.3 1.8, 1.3 1.7, 1.2 1.7, 1.2 1.8), "
            "(1.4 1.7, 1.504 1.7, 1.504 1.604, 1.4 1.604, 1.4 1.7))");
        poly = tg_poly_clone(tg_geom_poly(geom));
        tg_geom_free(geom);
    }


    struct tg_poly *poly2;
    must_fail = 100;
    while (must_fail > 0) {
        poly2 = tg_poly_copy(poly);
        if (!poly2) {
            must_fail--;
            continue;
        }
        tg_poly_free(poly2);
        poly2 = NULL;
    }
    tg_poly_free(poly);

}

int main(int argc, char **argv) {
    do_test(test_poly_rect);
    do_test(test_poly_move);
    do_test(test_poly_covers_point);
    do_test(test_poly_intersects_point);
    do_test(test_poly_covers_rect);
    do_test(test_poly_intersects_rect);
    do_test(test_poly_covers_line);
    do_test(test_poly_intersects_line);
    do_test(test_poly_covers_poly);
    do_test(test_poly_copy);
    do_test(test_poly_clockwise);
    do_test(test_poly_casting);
    do_test(test_poly_various);
    do_chaos_test(test_poly_chaos);
}
