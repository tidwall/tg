#include "tests.h"

void test_rect_center() {
    assert(pointeq(tg_rect_center(R(0, 0, 10, 10)), P(5, 5)));
    assert(pointeq(tg_rect_center(R(0, 0, 0, 0)), P(0, 0)));
}

void test_rect_move() {
    assert(recteq(tg_rect_move(R(1, 2, 3, 4), 5, 6), R(6, 8, 8, 10)));
}

void test_rect_num_points() {
    assert(tg_rect_num_points(R(0, 0, 10, 10)) == 5);
}

void test_rect_num_segments() {
    assert(tg_rect_num_segments(R(0, 0, 10, 10)) == 4);
}

void test_rect_point_at() {
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 0), P(0, 0)));
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 1), P(10, 0)));
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 2), P(10, 10)));
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 3), P(0, 10)));
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 4), P(0, 0)));
    assert(pointeq(tg_rect_point_at(R(0, 0, 10, 10), 5), P(0, 0)));
}

void test_rect_segment_at() {
    assert(segeq(tg_rect_segment_at(R(0, 0, 10, 10), 0), S(0, 0, 10, 0)));
    assert(segeq(tg_rect_segment_at(R(0, 0, 10, 10), 1), S(10, 0, 10, 10)));
    assert(segeq(tg_rect_segment_at(R(0, 0, 10, 10), 2), S(10, 10, 0, 10)));
    assert(segeq(tg_rect_segment_at(R(0, 0, 10, 10), 3), S(0, 10, 0, 0)));
    assert(segeq(tg_rect_segment_at(R(0, 0, 10, 10), 4), S(0, 0, 0, 0)));
}

struct rsearchctx {
    int count;
    struct tg_rect rect;
};

bool rsearchiter(struct tg_segment seg, int idx, void *udata) {
    struct rsearchctx *ctx = udata;
    assert(pointeq(tg_rect_point_at(ctx->rect, idx), seg.a));
    ctx->count++;
    return true;
}

bool rsearchiter2(struct tg_segment seg, int idx, void *udata) {
    struct rsearchctx *ctx = udata;
    assert(pointeq(tg_rect_point_at(ctx->rect, idx), seg.a));
    ctx->count++;
    return false;
}

void test_rect_search() {
    struct tg_rect rect = R(0, 0, 10, 10);
    struct rsearchctx ctx = { .rect = rect, .count = 0 };
    tg_rect_search(rect, R(0, 0, 10, 10), rsearchiter, &ctx);
    assert(ctx.count == 4);
    ctx.count = 0;
    tg_rect_search(rect, R(0, 4, 10, 5), rsearchiter, &ctx);
    assert(ctx.count == 2);
    ctx.count = 0;
    tg_rect_search(rect, R(0, 4, 10, 5), rsearchiter2, &ctx);
    assert(ctx.count == 1);
}

void test_rect_covers_point() {
    for (double x = 0; x <= 10; x += 1) {
        for (double y = 0; y <= 10; y += 1) {
            assert(tg_rect_covers_point(R(0, 0, 10, 10), P(x, y)));
            assert(tg_rect_covers_xy(R(0, 0, 10, 10), x, y));
        }
    }
     assert(!tg_rect_covers_point(R(0, 0, 10, 10), P(-15, -15)));
     assert(!tg_rect_covers_point(R(0, 0, 10, 10), P(-15, 5)));
     assert(!tg_rect_covers_point(R(0, 0, 10, 10), P(-15, 15)));
     assert(!tg_rect_covers_point(R(0, 0, 10, 10), P(0, -15)));
}

void test_rect_intersects_point() {
    assert(tg_rect_intersects_point(R(0, 0, 10, 10), P(5, 5)));
    assert(!tg_rect_intersects_point(R(0, 0, 10, 10), P(15, 15)));
}

void test_rect_covers_rect() {
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(0, 0, 10, 10)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(2, 2, 10, 10)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(2, 2, 8, 8)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(0, 0, 8, 8)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(0, 2, 8, 8)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(2, 0, 8, 8)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(2, 2, 10, 8)));
    assert(tg_rect_covers_rect(R(0, 0, 10, 10), R(2, 2, 8, 10)));
    assert(!tg_rect_covers_rect(R(0, 0, 10, 10), R(-1, 0, 10, 10)));
    assert(!tg_rect_covers_rect(R(0, 0, 10, 10), R(0, -1, 10, 10)));
    assert(!tg_rect_covers_rect(R(0, 0, 10, 10), R(0, 0, 11, 10)));
    assert(!tg_rect_covers_rect(R(0, 0, 10, 10), R(0, 0, 10, 11)));
}

void test_rect_intersects_rect() {
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(0, 0, 10, 10)));
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(2, 2, 8, 8)));
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(-1, 0, 10, 10)));
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(0, -1, 10, 10)));
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(0, 0, 11, 10)));
    assert(tg_rect_intersects_rect(R(0, 0, 10, 10), R(0, 0, 10, 11)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(11, 0, 21, 10)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(0, 11, 10, 21)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(11, 0, 21, 10)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(11, 11, 21, 21)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(-11, 11, 1, 21)));
    assert(!tg_rect_intersects_rect(R(0, 0, 10, 10), R(-11, -11, -1, -1)));
}

void test_rect_covers_line() {
    assert(tg_rect_covers_line(R(0, 0, 10, 10), LINE(P(0, 0), P(10, 10))));
    assert(!tg_rect_covers_line(R(0, 0, 10, 10), LINE(P(0, 0), P(11, 11))));
    assert(!tg_rect_covers_line(R(0, 0, 10, 10), LINE()));
    assert(tg_rect_covers_line(R(0, 0, 30, 30), LINE(P(15, 0), P(15, 15), P(30, 15))));;
    assert(!tg_rect_covers_line(R(0, 0, 30, 30), LINE()));;
    assert(!tg_rect_covers_line(R(0, 0, 20, 20), LINE(P(15, 0), P(15, 15), P(30, 15))));;
    assert( tg_rect_covers_line(R(0, 0, 30, 30), LINE(P(15, 0), P(15, 15), P(30, 15))));
    assert(!tg_rect_covers_line(R(0, 0, 30, 30), LINE()));
}

void test_rect_intersects_line() {
    assert(tg_rect_intersects_line(R(0, 0, 10, 10), LINE(P(0, 0), P(10, 10))));
    assert(tg_rect_intersects_line(R(0, 0, 10, 10), LINE(P(0, 0), P(11, 11))));
    assert(!tg_rect_intersects_line(R(0, 0, 10, 10), LINE()));
    assert(!tg_rect_intersects_line(R(0, 0, 10, 10), LINE(P(11, 11), P(12, 12))));
}

void test_rect_covers_poly() {
    struct tg_poly *oct = tg_poly_new_gc(RING(octagon), NULL, 0); 
    assert(tg_rect_covers_poly(R(0, 0, 10, 10), oct));
    assert(!tg_rect_covers_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 1, 0)));
    assert(!tg_rect_covers_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 1, 1)));
    assert(!tg_rect_covers_poly(R(0, 0, 10, 10), tg_poly_new_gc(NULL, NULL, 0)));
}

void test_rect_intersects_poly() {
    struct tg_poly *oct = tg_poly_new_gc(RING(octagon), NULL, 0); 
    assert(tg_rect_intersects_poly(R(0, 0, 10, 10), oct));
    assert(tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 1, 0)));
    assert(tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 0, 1)));
    assert(!tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 10, 10)));
    assert(!tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, 11, 10)));
    assert(!tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_move_gc(oct, -11, 0)));
    assert(!tg_rect_intersects_poly(R(0, 0, 10, 10), tg_poly_new_gc(NULL, NULL, 0)));
}

void test_rect_distance() {
    assert(tg_rect_distance_rect(R(5,5,5,5), R(0,0,10,10)) == 0);
    assert(tg_rect_distance_rect(R(0,0,10,10), R(0,0,10,10)) == 0);
    assert(tg_rect_distance_rect(R(10,10,20,20), R(0,0,10,10)) == 0);
    assert(tg_rect_distance_rect(R(11,10,20,20), R(0,0,10,10)) == 1);
    assert(tg_rect_distance_rect(R(11,10,11,10), R(0,0,10,10)) == 1);
}


void fullrectmatch(const char *wkt, int dims, double min[4], double max[4]) {
    struct tg_geom *g = tg_parse_wkt(wkt);
    if (tg_geom_error(g)) {
        printf("%s\n", tg_geom_error(g));
        assert(0);
    }

    double min2[4], max2[4];
    int dims2 = tg_geom_fullrect(g, min2, max2);
    tg_geom_free(g);
    cmpfullrect(dims, min, max, dims2, min2, max2);
}

void test_rect_fullrect() {
    assert(tg_geom_fullrect(0, 0, 0) == 0);
    fullrectmatch("POINT(10 20)", 2, (double[4]){10, 20}, (double[4]){10, 20});
    fullrectmatch("POINT(10 20 30)", 3, (double[4]){10, 20, 30}, (double[4]){10, 20, 30});
    fullrectmatch("POINT(10 20 30 40)", 4, (double[4]){10, 20, 30, 40}, (double[4]){10, 20, 30, 40});
    fullrectmatch("LINESTRING(10 20,30 40)", 2,(double[4]){10,20},(double[4]){30,40});
    fullrectmatch("LINESTRING(10 20 30,40 50 60)", 3,(double[4]){10,20,30},(double[4]){40,50,60});
    fullrectmatch("LINESTRING(40 50 60,10 20 30)", 3,(double[4]){10,20,30},(double[4]){40,50,60});
    fullrectmatch("LINESTRING(10 20 30 40,50 60 70 80)", 4,(double[4]){10,20,30,40},(double[4]){50,60,70,80});
    fullrectmatch("LINESTRING(50 60 70 80,10 20 30 40)", 4,(double[4]){10,20,30,40},(double[4]){50,60,70,80});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20)"
    ")", 2, (double[4]){10, 20}, (double[4]){10, 20});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20),"
        "POINT(30 40)"
    ")", 2, (double[4]){10, 20}, (double[4]){30, 40});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20 30),"
        "POINT(40 50 60)"
    ")", 3, (double[4]){10, 20, 30}, (double[4]){40, 50, 60});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20 30 40),"
        "POINT(50 60 70 80)"
    ")", 4, (double[4]){10, 20, 30, 40}, (double[4]){50, 60, 70, 80});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(40 50 60),"
        "POINT(10 20 30)"
    ")", 3, (double[4]){10, 20, 30}, (double[4]){40, 50, 60});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(50 60 70 80),"
        "POINT(10 20 30 40)"
    ")", 4, (double[4]){10, 20, 30, 40}, (double[4]){50, 60, 70, 80});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20),"
        "POINT(30 40 50)"
    ")", 3, (double[4]){10, 20, 50}, (double[4]){30, 40, 50});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20),"
        "POINT(30 40 50 60)"
    ")", 4, (double[4]){10, 20, 50, 60}, (double[4]){30, 40, 50, 60});
    fullrectmatch("GEOMETRYCOLLECTION("
        "POINT(10 20),"
        "POINT(30 40 50 60),"
        "POINT(70 80 90)"
    ")", 4, (double[4]){10, 20, 50, 60}, (double[4]){70, 80, 90, 60});
}

int main(int argc, char **argv) {
    do_test(test_rect_center);
    do_test(test_rect_move);
    do_test(test_rect_num_points);
    do_test(test_rect_num_segments);
    do_test(test_rect_point_at);
    do_test(test_rect_segment_at);
    do_test(test_rect_search);
    do_test(test_rect_covers_point);
    do_test(test_rect_intersects_point);
    do_test(test_rect_covers_rect);
    do_test(test_rect_intersects_rect);
    do_test(test_rect_covers_line);
    do_test(test_rect_intersects_line);
    do_test(test_rect_covers_poly);
    do_test(test_rect_intersects_poly);
    do_test(test_rect_distance);
    do_test(test_rect_fullrect);
}
