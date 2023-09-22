#include "tests.h"

void test_point_rect() {
    assert(recteq(tg_point_rect(P(5, 5)), R(5, 5, 5, 5)));
}

void test_point_move() {
    assert(pointeq(tg_point_move(P(5, 6), 10, 10), P(15, 16)));
}

void test_point_covers_point() {
    assert(tg_point_covers_point(P(5, 5), P(5, 5)));
    assert(!tg_point_covers_point(P(5, 5), P(6, 5)));
}

void test_point_intersects_point() {
    assert(tg_point_intersects_point(P(5, 5), P(5, 5)));
    assert(!tg_point_intersects_point(P(5, 5), P(6, 5)));
}

void test_point_covers_rect() {
    assert(tg_point_covers_rect(P(5, 5), R(5, 5, 5, 5)));
    assert(!tg_point_covers_rect(P(5, 5), R(0, 0, 10, 10)));
}

void test_point_intersects_rect() {
    assert(tg_point_intersects_rect(P(5, 5), R(5, 5, 5, 5)));
    assert(tg_point_intersects_rect(P(5, 5), R(0, 0, 10, 10)));
    assert(tg_point_intersects_rect(P(0, 0), R(0, 0, 10, 10)));
    assert(!tg_point_intersects_rect(P(-1, 0), R(0, 0, 10, 10)));
}

void test_point_covers_line() {
    assert(!tg_point_covers_line(P(5, 5), LINE()));
    assert(!tg_point_covers_line(P(5, 5), LINE(P(5, 5))));
    assert(tg_point_covers_line(P(5, 5), LINE(P(5, 5), P(5, 5))));
    assert(!tg_point_covers_line(P(5, 5), LINE(P(5, 5), P(10, 10))));
    assert(!tg_point_covers_line(P(15, 15), LINE(P(15, 0), P(15, 15), P(30, 15))));
    assert(!tg_point_covers_line(P(15, 15), LINE()));
    assert(!tg_point_covers_line(P(15, 15), LINE(P(15, 15))));
    assert(tg_point_covers_line(P(15, 15), LINE(P(15, 15), P(15, 15))));
}

void test_point_intersects_line() {
    assert(!tg_point_intersects_line(P(5, 5), LINE()));
    assert(!tg_point_intersects_line(P(5, 5), LINE(P(5, 5))));
    assert(tg_point_intersects_line(P(5, 5), LINE(P(5, 5), P(5, 5))));
    assert(tg_point_intersects_line(P(5, 5), LINE(P(0, 0), P(10, 10))));
    assert(!tg_point_intersects_line(P(6, 5), LINE(P(0, 0), P(10, 10))));
}

void test_point_covers_poly() {
    assert(!tg_point_covers_poly(P(5, 5), NULL));
    assert(!tg_point_covers_poly(P(5, 5), tg_poly_new_gc(RING(P(0, 0), P(10, 0)), NULL, 0)));
    assert(!tg_point_covers_poly(P(5, 5), tg_poly_new_gc(RR(0, 0, 10, 10), NULL, 0)));
    assert(tg_point_covers_poly(P(5, 5), tg_poly_new_gc(RR(5, 5, 5, 5), NULL, 0)));
}

void test_point_intersects_poly() {
    struct tg_poly *octa = tg_poly_new_gc(RING(octagon), NULL, 0);
    struct tg_poly *concav1 = tg_poly_new_gc(RING(concave1), NULL, 0);
    assert(!tg_point_intersects_poly(P(5, 5), NULL));
    assert(!tg_point_intersects_poly(P(5, 5), tg_poly_new_gc(RING(P(0, 0), P(10, 0)), NULL, 0)));
    assert( tg_point_intersects_poly(P(5, 5), octa));
    assert( tg_point_intersects_poly(P(0, 5), octa));
    assert(!tg_point_intersects_poly(P(1, 1), octa));
    assert(!tg_point_intersects_poly(P(4, 4), concav1));
    assert( tg_point_intersects_poly(P(5, 5), concav1));
    assert( tg_point_intersects_poly(P(6, 6), concav1));
}

void test_point_distance() {
    assert(tg_point_distance_point(P(10,10), P(10,20)) == 10);
    assert(tg_point_distance_point(P(10,10), P(20,10)) == 10);
    assert(tg_point_distance_point(P(10,10), P(10,10)) == 0);
    assert(tg_point_distance_segment(P(5,5), S(0,0,10,0)) == 5);
    assert(tg_point_distance_segment(P(0,0), S(0,0,10,0)) == 0);
    assert(tg_point_distance_segment(P(5,0), S(0,0,10,0)) == 0);
    assert(tg_point_distance_segment(P(-1,0), S(0,0,10,0)) == 1);
    assert(tg_point_distance_segment(P(11,0), S(0,0,10,0)) == 1);
    assert(tg_point_distance_segment(P(0,0), S(0,0,0,0)) == 0);
    assert(tg_point_distance_rect(P(5,5), R(0,0,10,10)) == 0);
    assert(tg_point_distance_rect(P(11,10), R(0,0,10,10)) == 1);
}

int main(int argc, char **argv) {
    do_test(test_point_rect);
    do_test(test_point_move);
    do_test(test_point_covers_point);
    do_test(test_point_intersects_point);
    do_test(test_point_covers_rect);
    do_test(test_point_intersects_rect);
    do_test(test_point_covers_line);
    do_test(test_point_intersects_line);
    do_test(test_point_covers_poly);
    do_test(test_point_intersects_poly);
    do_test(test_point_distance);
    return 0;
}
