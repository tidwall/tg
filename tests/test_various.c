#include "tests.h"

// https://github.com/tidwall/geojson/issues/14
void test_various_issue_14() {
    { // original
        struct tg_ring *exterior = RING(P(0, 0),P(6000, 0),P(10, 10.1),P(0, 10),P(0, 0));
        struct tg_poly *polygon = tg_poly_new_gc(exterior, NULL, 0);
        struct tg_rect rect = R(2560, 0, 5120, 2560);

        bool flag_covers = tg_poly_covers_rect(polygon, rect);
        bool flag_intersects = tg_rect_intersects_poly(rect, polygon);
        bool flag_intersects2 = tg_poly_intersects_rect(polygon, rect);
        assert(!(!flag_intersects || flag_covers || !flag_intersects2));
    }
    { // modified
        struct tg_ring *exterior = RING(P(0, 0), P(5, 0), P(0, 1), P(0, 0));
        struct tg_poly *polygon = tg_poly_new_gc(exterior, NULL, 0);
        struct tg_rect rect = R(2, -1, 4.5, 1);
        bool flag_intersects = tg_poly_intersects_rect(polygon, rect);
        bool flag_covers = tg_poly_covers_rect(polygon, rect);
        bool flag_intersects2 = tg_poly_intersects_rect(polygon, rect);
        assert(!(!flag_intersects || flag_covers || !flag_intersects2));
    }
}

// https://github.com/tidwall/tile38/issues/369
void test_various_issue_369() {
    struct tg_poly *poly_holes = POLY(
        RING(
            {-122.44154334068298, 37.73179457567642},
            {-122.43935465812682, 37.73179457567642},
            {-122.43935465812682, 37.7343740514423},
            {-122.44154334068298, 37.7343740514423},
            {-122.44154334068298, 37.73179457567642}
        ), 
        RING(
            {-122.44104981422423, 37.73286371140448},
            {-122.44104981422423, 37.73424677678513},
            {-122.43990182876587, 37.73424677678513},
            {-122.43990182876587, 37.73286371140448},
            {-122.44104981422423, 37.73286371140448},
        ),
        RING(
            {-122.44109272956847, 37.731870943026074},
            {-122.43976235389708, 37.731870943026074},
            {-122.43976235389708, 37.7326855231885},
            {-122.44109272956847, 37.7326855231885},
            {-122.44109272956847, 37.731870943026074},
        ));
    struct tg_poly *a = POLY(
        RING(
            {-122.4408378, 37.7341129},
            {-122.4408378, 37.733},
            {-122.44, 37.733},
            {-122.44, 37.7343129},
            {-122.4408378, 37.7341129},
        ));
    struct tg_poly *b = POLY(
        RING(
            {-122.44091033935547, 37.731981251280985},
            {-122.43994474411011, 37.731981251280985},
            {-122.43994474411011, 37.73254976045042},
            {-122.44091033935547, 37.73254976045042},
            {-122.44091033935547, 37.731981251280985},
        ));
    struct tg_poly *c = POLY(
        RING(
            {-122.4408378, 37.7341129},
            {-122.4408378, 37.733},
            {-122.44, 37.733},
            {-122.44, 37.7341129},
            {-122.4408378, 37.7341129},
        ));
    assert( tg_poly_intersects_poly(poly_holes, a));
    assert(!tg_poly_intersects_poly(poly_holes, b));
    assert(!tg_poly_intersects_poly(poly_holes, c));
    assert( tg_poly_intersects_poly(a, poly_holes));
    assert(!tg_poly_intersects_poly(b, poly_holes));
    assert(!tg_poly_intersects_poly(c, poly_holes));
}

void test_various_unit_tests() {
    assert(tg_aligned_size(0) == 0);
    assert(tg_aligned_size(1) == 8);
    assert(tg_aligned_size(8) == 8);
    assert(tg_aligned_size(9) == 16);
    assert(tg_ring_polsby_popper_score(NULL) == 0);
}


#define parse_wkt_check(wkt) ({ \
    struct tg_geom *geom = tg_parse_wkt((wkt)); \
    assert(!tg_geom_error(geom)); \
    assert(geom); \
    gc_geom(geom); \
    geom; \
})

void test_various_imported_tests() {
    // struct tg_geom *geom = tg_parse_wkt("MULTIPOINT((5 5),EMPTY)");
    // printf("%s\n", tg_geom_error(geom));

    // assert(geom);
    
    struct tg_geom *a;
    struct tg_geom *b;

    // a = parse_wkt_check("LINESTRING(0 0, 10 10)");
    // b = parse_wkt_check("MULTIPOINT((5 5),EMPTY)");
    // assert(tg_geom_intersects(a, b) == true);
    // // assert(tg_geom_intersects(b, a) == true);
    // return;

    a = parse_wkt_check("POINT(-23.1094689600055 50.5195368635957)");
    b = parse_wkt_check("LINESTRING(-23.122057005539 50.5201976774794,-23.1153476966995 50.5133404815199,-23.1094689600055 50.5223376452201,-23.1094689600055 50.5169177629559,-23.0961967920942 50.5330464848094,-23.0887991006034 50.5258515213185,-23.0852302622362 50.5264582238409)");
    assert(tg_geom_intersects(a, b) == true);
    a = parse_wkt_check("POLYGON((0 0, 80 0, 80 80, 0 80, 0 0))");
    b = parse_wkt_check("POLYGON((100 200, 100 140, 180 140, 180 200, 100 200))");
    assert(tg_geom_intersects(a, b) == false);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((0 0, 140 0, 140 140, 0 140, 0 0))");
    b = parse_wkt_check("POLYGON((140 0, 0 0, 0 140, 140 140, 140 0))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == true);
    a = parse_wkt_check("POLYGON((40 60, 360 60, 360 300, 40 300, 40 60))");
    b = parse_wkt_check("POLYGON((120 100, 280 100, 280 240, 120 240, 120 100))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == true);
    a = parse_wkt_check("POLYGON((40 60, 420 60, 420 320, 40 320, 40 60), (200 140, 160 220, 260 200, 200 140))");
    b = parse_wkt_check("POLYGON((80 100, 360 100, 360 280, 80 280, 80 100))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((0 280, 0 0, 260 0, 260 280, 0 280), (220 240, 40 240, 40 40, 220 40, 220 240))");
    b = parse_wkt_check("POLYGON((20 260, 240 260, 240 20, 20 20, 20 260), (160 180, 80 180, 120 120, 160 180))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((60 80, 200 80, 200 220, 60 220, 60 80))");
    b = parse_wkt_check("POLYGON((120 140, 260 140, 260 260, 120 260, 120 140))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((60 220, 220 220, 140 140, 60 220))");
    b = parse_wkt_check("POLYGON((100 180, 180 180, 180 100, 100 100, 100 180))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((40 40, 180 40, 180 180, 40 180, 40 40))");
    b = parse_wkt_check("POLYGON((180 40, 40 180, 160 280, 300 140, 180 40))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((100 60, 140 100, 100 140, 60 100, 100 60))");
    b = parse_wkt_check("MULTIPOLYGON(((80 40, 120 40, 120 80, 80 80, 80 40)), ((120 80, 160 80, 160 120, 120 120, 120 80)), ((80 120, 120 120, 120 160, 80 160, 80 120)), ((40 80, 80 80, 80 120, 40 120, 40 80)))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((40 280, 200 280, 200 100, 40 100, 40 280), (100 220, 120 220, 120 200, 100 180, 100 220))");
    b = parse_wkt_check("POLYGON((40 280, 180 260, 180 120, 60 120, 40 280))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((0 200, 0 0, 200 0, 200 200, 0 200), (20 180, 130 180, 130 30, 20 30, 20 180))");
    b = parse_wkt_check("POLYGON((60 90, 130 90, 130 30, 60 30, 60 90))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((150 150, 410 150, 280 20, 20 20, 150 150), (170 120, 330 120, 260 50, 100 50, 170 120))");
    b = parse_wkt_check("POLYGON((270 90, 200 50, 150 80, 210 120, 270 90))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON ((0 0, 20 80, 120 80, -20 120, 0 0))");
    b = parse_wkt_check("POLYGON ((60 180, -100 120, -140 60, -40 20, -100 -80, 40 -20, 140 -100, 140 40, 260 160, 80 120, 60 180))");
    assert(tg_geom_intersects(a, b) == true);
    assert(tg_geom_covers(a, b) == false);
    a = parse_wkt_check("POLYGON((100 60, 140 100, 100 140, 60 100, 100 60))");
    b = parse_wkt_check("MULTIPOLYGON(((80 40, 120 40, 120 80, 80 80, 80 40)), ((120 80, 160 80, 160 120, 120 120, 120 80)), ((80 120, 120 120, 120 160, 80 160, 80 120)), ((40 80, 80 80, 80 120, 40 120, 40 80)))");
    assert(tg_geom_intersects(a, b) == true);
    a = parse_wkt_check("LINESTRING(-123456789 -40, 381039468754763 123456789)");
    b = parse_wkt_check("POINT(0 0)");
    assert(tg_geom_intersects(a, b) == false);
    a = parse_wkt_check("POLYGON((0.04745459120333707 7.998990511152711, 10.222919749090828 34.08521670606894, 10 10, 0.04745459120333707 7.998990511152711))");
    b = parse_wkt_check("POLYGON((15 5, 0.010044793132693013 7.903085268568247, 10.260329547338191 34.181121949106455, 15 5))");
    assert(tg_geom_intersects(a, b) == true);
    a = parse_wkt_check("POLYGON((0.04745459120333707 7.998990511152711, 10 10, 10.222919749090828 34.08521670606894, 0.04745459120333707 7.998990511152711))");
    b = parse_wkt_check("POLYGON((15 5, 0.010044793132693013 7.903085268568247, 10.260329547338191 34.181121949106455, 15 5))");
    assert(tg_geom_intersects(a, b) == true);
    a = parse_wkt_check("POLYGON((0.04745459120333707 7.998990511152711, 10.222919749090828 34.08521670606894, 10 10, 0.04745459120333707 7.998990511152711))");
    b = parse_wkt_check("POLYGON((15 5, 10.260329547338191 34.181121949106455, 0.010044793132693013 7.903085268568247, 15 5))");
    assert(tg_geom_intersects(a, b) == true);
    a = parse_wkt_check("POLYGON((0.04745459120333707 7.998990511152711, 10 10, 10.222919749090828 34.08521670606894, 0.04745459120333707 7.998990511152711))");
    b = parse_wkt_check("POLYGON((15 5, 10.260329547338191 34.181121949106455, 0.010044793132693013 7.903085268568247, 15 5))");
    assert(tg_geom_intersects(a, b) == true);
    // a = parse_wkt_check("LINESTRING(0 0, 10 10)");
    // b = parse_wkt_check("MULTIPOINT((5 5),EMPTY)");
    // assert(tg_geom_intersects(a, b) == true);
    // assert(tg_geom_intersects(b, a) == true);
    // a = parse_wkt_check("GEOMETRYCOLLECTION (LINESTRING (0 0, 1 1), POINT (2 2))");
    // b = parse_wkt_check("LINESTRING (2 2, 3 3)");
    // assert(tg_geom_intersects(a, b) == true);
    // assert(tg_geom_intersects(b, a) == true);
    // a = parse_wkt_check("POLYGON ((26639.240191093646 6039.3615818717535, 26639.240191093646 5889.361620883223,28000.000095100608 5889.362081553552, 28000.000095100608 6039.361620882992, 28700.00019021402 6039.361620882992, 28700.00019021402 5889.361822800367, 29899.538842431968 5889.362160452064,32465.59665091549 5889.362882757903, 32969.2837182586 -1313.697771558439, 31715.832811969216 -1489.87008918589, 31681.039836323587 -1242.3030298361555, 32279.3890331618 -1158.210534269224, 32237.63710287376 -861.1301136466199, 32682.89764107368 -802.0828534499739, 32247.445200905553 5439.292852892075, 31797.06861513178 5439.292852892075, 31797.06861513178 5639.36178850523, 29899.538849750803 5639.361268079038, 26167.69458275995 5639.3602445643955, 26379.03654594742 2617.0293071870683, 26778.062167926924 2644.9318977193907, 26792.01346261031 2445.419086759444, 26193.472956813417 2403.5650586598513, 25939.238114175267 6039.361685403233, 26639.240191093646 6039.3615818717535), (32682.89764107368 -802.0828534499738, 32682.89764107378 -802.0828534499669, 32247.445200905655 5439.292852892082, 32247.445200905553 5439.292852892075, 32682.89764107368 -802.0828534499738))");
    // b = parse_wkt_check("POLYGON ((32450.100392347143 5889.362314133216, 32050.1049555691 5891.272957209961, 32100.021071878822 16341.272221116333, 32500.016508656867 16339.361578039587, 32450.100392347143 5889.362314133216))");
    // assert(tg_geom_intersects(a, b) == true);
    // assert(tg_geom_intersects(b, a) == true);
}

void tg_geom_setnoheap(struct tg_geom *geom);

void test_various_noheap(void) {
    struct tg_geom *g1 = tg_parse("POINT(1 1)", 10);
    struct tg_geom *g2 = tg_geom_clone(g1);


    size_t size = tg_geom_memsize(g1);
    struct tg_geom *g3 = alloca(size);
    memcpy(g3, g1, size);
    tg_geom_setnoheap(g3);
    struct tg_geom *g4 = tg_geom_clone(g3);


    assert(tg_geom_equals(g1, g2));
    assert(tg_geom_equals(g2, g3));
    assert(tg_geom_equals(g3, g4));

    assert(g1 == g2);
    assert(g2 != g3);
    assert(g3 != g4);

    tg_geom_free(g4);
    tg_geom_free(g3);
    tg_geom_free(g2);
    tg_geom_free(g1);
    
}

void test_various_fixed_floating_points(void) {
    char a[] = "POINT(9000000 1000000)";
    char b[] = "POINT(9e6 1e6)";
    struct tg_geom *g = tg_parse(a, strlen(a));
    assert(g);
    char buf[64];
    tg_geom_wkt(g, buf, sizeof(buf));
    assert(strcmp(buf, b) == 0);
    tg_geom_wkt(g, buf, sizeof(buf));
    tg_env_set_print_fixed_floats(true);
    tg_geom_wkt(g, buf, sizeof(buf));
    assert(strcmp(buf, a) == 0);
    tg_env_set_print_fixed_floats(false);
    tg_geom_free(g);

    
}

int main(int argc, char **argv) {
    do_test(test_various_imported_tests);
    do_test(test_various_issue_14);
    do_test(test_various_issue_369);
    do_test(test_various_unit_tests);
    do_test(test_various_noheap);
    do_test(test_various_fixed_floating_points);
    return 0;
}


