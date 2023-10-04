#include "tests.h"

void test_line_new() {
    struct tg_line *line = LINE(u1);
    assert(!tg_line_empty(line));
}

void test_line_move() {
    struct tg_line *ln1 = LINE(P(0, 1), P(2, 3), P(4, 5));
    struct tg_line *ln2 = tg_line_move_gc(ln1, 7, 8);
    assert(tg_line_num_points(ln1) == tg_line_num_points(ln2));
    for (int i = 0 ;i < tg_line_num_points(ln2); i++) {
        pointeq(tg_line_point_at(ln2, i), tg_point_move(tg_line_point_at(ln1, i), 7, 8));
    }
    assert(tg_line_move_gc(NULL, 0, 0) == NULL);
    assert(tg_line_move_gc(tg_line_new_gc(NULL, 0), 0, 0) != NULL);
}

void test_line_clockwise() {
    assert(tg_line_clockwise(LINE(P(0, 0), P(0, 10), P(10, 10), P(10, 0), P(0, 0))));
    assert(!tg_line_clockwise(LINE(P(0, 0), P(10, 0), P(10, 10), P(0, 10), P(0, 0))));
    assert(tg_line_clockwise(LINE(P(0, 0), P(0, 10), P(10, 10))));
    assert(!tg_line_clockwise(LINE(P(0, 0), P(10, 0), P(10, 10))));
}

void test_line_covers_point() {
    struct tg_line *line = LINE(u1);
    assert(tg_line_covers_point(line, P(0, 0)));
    assert(tg_line_covers_point(line, P(10, 10)));
    assert(tg_line_covers_point(line, P(0, 5)));
    assert(!tg_line_covers_point(line, P(5, 5)));
    line = LINE(v1);
    assert( tg_line_covers_point(line, P(0, 10)));
    assert(!tg_line_covers_point(line, P(0, 0)));
    assert( tg_line_covers_point(line, P(5, 0)));
    assert( tg_line_covers_point(line, P(2.5, 5)));
    assert(!tg_line_covers_point(NULL, P(0, 0)));
    assert(!tg_line_covers_point(tg_line_new_gc(NULL, 0), P(0, 0)));
}

void test_line_intersects_point() {
    struct tg_line *line = LINE(v1);
    assert( tg_line_intersects_point(line, P(0, 10)));
    assert(!tg_line_intersects_point(line, P(0, 0)));
    assert( tg_line_intersects_point(line, P(5, 0)));
    assert( tg_line_intersects_point(line, P(2.5, 5)));
    assert(!tg_line_intersects_point(NULL, P(0, 0)));
    assert(!tg_line_intersects_point(tg_line_new_gc(NULL, 0), P(0, 0)));
}

void test_line_covers_rect() {
    struct tg_line *line = LINE(v1);
    assert(!tg_line_covers_rect(line, R(0, 0, 10, 10)));
    assert(tg_line_covers_rect(line, R(0, 10, 0, 10)));
    line = LINE(u1);
    assert(tg_line_covers_rect(line, R(0, 0, 0, 10)));
    assert(!tg_line_covers_rect(NULL, R(0, 0, 0, 0)));
    assert(!tg_line_covers_rect(tg_line_new_gc(NULL, 0), R(0, 0, 0, 0)));
}

void test_line_intersects_rect() {
    struct tg_line *line = LINE(v1);
    assert( tg_line_intersects_rect(line, R(0, 0, 10, 10)));
    assert( tg_line_intersects_rect(line, R(0, 0, 2.5, 5)));
    assert(!tg_line_intersects_rect(line, R(0, 0, 2.4, 5)));
    assert(!tg_line_intersects_rect(NULL, R(0, 0, 0, 0)));
    assert(!tg_line_intersects_rect(tg_line_new_gc(NULL, 0), R(0, 0, 0, 0)));
}

void test_line_covers_line() {
    struct tg_line *ln1 = LINE(P(5, 0), P(5, 5), P(10, 5), P(10, 10), P(15, 10), P(15, 15));
    struct tg_line *lns[] = {
        LINE(P(7, 5), P(10, 5), P(10, 10), P(12, 10)),
        LINE(P(7, 5), P(8, 5), P(10, 5), P(10, 10), P(12, 10)),
        LINE(P(7, 5), P(8, 5), P(6, 5), P(10, 5), P(10, 8), P(10, 5), P(5, 5),
            P(10, 5), P(10, 10), P(12, 10)),
    };
    for (size_t i = 0; i < sizeof(lns)/sizeof(struct tg_line*); i++) {
        assert(tg_line_covers_line(ln1, lns[i]));
    }
    assert(!tg_line_covers_line(ln1, LINE(P(5, -1), P(5, 5), P(10, 5))));
    assert(!tg_line_covers_line(ln1, LINE(P(5, 0), P(5, 5), P(5, 0), P(10, 0))));
    assert(!tg_line_covers_line(ln1, LINE(P(5, 0), P(5, 5), P(10, 5), P(10, 10),P(15, 10), P(15, 15), P(20, 20))));
    assert(!tg_line_covers_line(ln1, LINE()));
    assert(!tg_line_covers_line(LINE(), LINE(P(5, 0))));
    assert(!tg_line_covers_line(LINE(P(5, 0), P(10, 0)), LINE(P(5, 0))));
}

void test_line_intersects_line() {
    struct tg_line *lns[] = {LINE(u1), LINE(u2), LINE(u3), LINE(u4), LINE(v1), LINE(v2), LINE(v3), LINE(v4)};
    for (size_t i = 0; i < sizeof(lns)/sizeof(struct tg_line*); i++) {
        for (size_t j = 0; j < sizeof(lns)/sizeof(struct tg_line*); j++) {
            assert(tg_line_intersects_line(lns[i], lns[j]));
        }
    }
    struct tg_line *line = LINE(u1);
    assert(!tg_line_intersects_line(line, LINE()));
    assert(!tg_line_intersects_line(line, NULL));
    assert(!tg_line_intersects_line(LINE(), LINE()));
    assert(!tg_line_intersects_line(LINE(), line));
    assert(!tg_line_intersects_line(NULL, LINE()));
    assert(!tg_line_intersects_line(NULL, NULL));
    assert(!tg_line_intersects_line(NULL, line));
    assert(tg_line_intersects_line(line, tg_line_move_gc(line, 5, 0)));
    assert(tg_line_intersects_line(line, tg_line_move_gc(line, 10, 0)));
    assert(!tg_line_intersects_line(line, tg_line_move_gc(line, 11, 0)));
    assert(!tg_line_intersects_line(LINE(v1), tg_line_move_gc(LINE(v1), 0, 1)));
    assert(!tg_line_intersects_line(LINE(v1), tg_line_move_gc(LINE(v1), 0, -1)));
}

void test_line_covers_poly() {
    struct tg_line *line = LINE(u1);
    struct tg_poly *poly = POLY(RING(octagon));
    assert(!tg_line_covers_poly(line, poly));
    assert( tg_line_covers_poly(line, POLY(RING(P(0, 10), P(0, 0), P(0, 10)))));
    assert( tg_line_covers_poly(line, POLY(RING(P(0, 0), P(10, 0), P(0, 0)))));
    assert(!tg_line_covers_poly(LINE(), POLY(RING(P(0, 0), P(10, 0), P(0, 0)))));
    assert(!tg_line_covers_poly(NULL, POLY(RING(P(0, 0), P(10, 0), P(0, 0)))));
    assert(!tg_line_covers_poly(line, POLY(NULL)));
    assert(!tg_line_covers_poly(line, NULL));
    assert(!tg_line_covers_poly(NULL, NULL));
}

void test_line_intersects_poly() {
    struct tg_line *line = LINE(u1);
    struct tg_poly *poly = POLY(RING(octagon));
    assert(tg_line_intersects_poly(line, poly));
    assert(tg_line_intersects_poly(line, tg_poly_move_gc(poly, 5, 0)));
    assert(tg_line_intersects_poly(line, tg_poly_move_gc(poly, 10, 0)));
    assert(!tg_line_intersects_poly(line, tg_poly_move_gc(poly, 11, 0)));
    assert(!tg_line_intersects_poly(line, tg_poly_move_gc(poly, 15, 0)));
}

void test_line_various() {
    struct tg_line *line = LINE(u1);
    struct tg_line *line2 = tg_line_clone(line);
    assert(line == line2);

    assert(tg_point_touches_line((struct tg_point) { 10, 10 }, line));
    assert(tg_point_touches_line((struct tg_point) { 10, 10 }, NULL) == false);

    assert(tg_line_points(line) == tg_line_points(line2));
    assert(tg_line_memsize(line) == tg_line_memsize(line2));
    assert(tg_line_memsize(NULL) == 0);
    tg_line_free(line2);
    assert(!tg_line_copy(0));
    assert(tg_line_length(LINE(u1)) == 30.0);
}

int main(int argc, char **argv) {
    // assert(tg_segment_covers_point(S(0,0,0,10), P(0, 5)));
    // assert(tg_line_covers_point(line, P(0, 5)));
// return 0;

    do_test(test_line_new);
    do_test(test_line_clockwise);
    do_test(test_line_move);
    do_test(test_line_covers_point);
    do_test(test_line_intersects_point);
    do_test(test_line_covers_rect);
    do_test(test_line_intersects_rect);
    do_test(test_line_covers_line);
    do_test(test_line_intersects_line);
    do_test(test_line_covers_poly);
    do_test(test_line_intersects_poly);
    do_test(test_line_various);
}
