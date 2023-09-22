#include "tests.h"

void test_memory_random_generation(double secs) {
    int count = 0;
    int fails = 0;
    struct tg_rect rect = R(-180, -90, 180, 90);
    double start = now();
    while (now()-start < secs) {
        enum tg_index index = 0;
        switch (rand()%6) {
        case 0: index = TG_NONE; break;
        case 1: index = TG_NATURAL; break;
        case 2: index = TG_YSTRIPES; break;
        }
        int npoints = rand()%200;
        struct tg_point *points = malloc(npoints*sizeof(struct tg_point));
        assert(points);
        for (int i = 0; i < npoints; i++) {
            points[i] = rand_point(rect);
        }
        struct tg_ring *ring = NULL;
        while (1) {
            ring = tg_ring_new_ix(points, npoints, index);
            if (ring) break;
            fails++;
        }
        struct tg_line *line = NULL;
        while (1) {
            line = tg_line_new_ix(points, npoints, index);
            if (line) break;
            fails++;
        }
        struct tg_poly *poly = NULL;
        while (1) {
            poly = tg_poly_new(ring, 0, 0);
            if (poly) break;
            fails++;
        }

        int hnpoints = rand()%20;
        struct tg_point *hpoints = malloc(hnpoints*sizeof(struct tg_point));
        assert(hpoints);
        for (int i = 0; i < hnpoints; i++) {
            hpoints[i] = rand_point(rect);
        }

        struct tg_ring *hole = NULL;
        while (1) {
            hole = tg_ring_new(hpoints, hnpoints);
            if (hole) break;
            fails++;
        }


        struct tg_poly *hpoly = NULL;
        while (1) {
            hpoly = tg_poly_new(ring, (const struct tg_ring**)&hole, 1);
            if (hpoly) break;
            fails++;
        }


        while (1) {
            struct tg_ring *ring2 = tg_ring_move(ring, 10, 10);
            if (ring2) {
                tg_ring_free(ring2);
                break;
            }
            fails++;
        }

        while (1) {
            struct tg_line *line2 = tg_line_move(line, 10, 10);
            if (line2) {
                tg_line_free(line2);
                break;
            }
            fails++;
        }

        while (1) {
            struct tg_poly *poly2 = tg_poly_move(poly, 10, 10);
            if (poly2) {
                tg_poly_free(poly2);
                break;
            }
            fails++;
        }


        while (1) {
            struct tg_poly *hpoly2 = tg_poly_move(hpoly, 10, 10);
            if (hpoly2) {
                tg_poly_free(hpoly2);
                break;
            }
            fails++;
        }


        #define try_again(expr) while(1) { \
            struct tg_geom *g = (expr); \
            if (g) { \
                tg_geom_free(tg_geom_clone(g)); \
                tg_geom_free(g); \
                break; \
            } \
            fails++; \
        }

        try_again(tg_geom_new_point(P(10,10)));
        try_again(tg_geom_new_point_empty());
        try_again(tg_geom_new_point_z(P(0,0),0));
        try_again(tg_geom_new_point_m(P(0,0),0));
        try_again(tg_geom_new_point_zm(P(0,0),0,0));
        try_again(tg_geom_new_linestring_z(NULL,0,0));
        try_again(tg_geom_new_linestring_m(NULL,0,0));
        try_again(tg_geom_new_linestring_zm(NULL,0,0));
        try_again(tg_geom_new_polygon_z(NULL,0,0));
        try_again(tg_geom_new_polygon_m(NULL,0,0));
        try_again(tg_geom_new_polygon_zm(NULL,0,0));
        try_again(tg_geom_new_multipoint((struct tg_point[]){
            P(1,2),P(2,3),}, 2));
        double coords[] = { 1,2,3,4,5,6,7,8 };
        int ncoords = sizeof(coords)/sizeof(double);
        try_again(tg_geom_new_multipoint_z(0, 0, coords, ncoords));
        try_again(tg_geom_new_multipoint_m(0, 0, coords, ncoords));
        try_again(tg_geom_new_multipoint_zm(0, 0, coords, ncoords));
        try_again(tg_geom_new_multilinestring_z(0, 0, coords, ncoords));
        try_again(tg_geom_new_multilinestring_m(0, 0, coords, ncoords));
        try_again(tg_geom_new_multilinestring_zm(0, 0, coords, ncoords));
        try_again(tg_geom_new_multilinestring_z(
            (const struct tg_line *const[]){NULL,NULL,}, 2, coords, ncoords));
        try_again(tg_geom_new_multilinestring_z(0, 0, coords, ncoords));
        try_again(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const[]){NULL,NULL}, 2, coords, ncoords));
        try_again(tg_geom_new_multipolygon_m(0, 0, coords, ncoords));
        try_again(tg_geom_new_multipolygon_zm(0, 0, coords, ncoords));
        try_again(tg_geom_new_geometrycollection(
            (const struct tg_geom *const[]){NULL,NULL,}, 2));
        try_again(tg_geom_new_geometrycollection(0, 0));

        tg_ring_free(hole);
        tg_poly_free(hpoly);
        free(hpoints);

        tg_poly_free(poly);
        tg_line_free(line);
        tg_ring_free(ring);
        free(points);

        count++;
    }
    (void)count;
    (void)fails;
}

void test_memory_stdlib() {
    tg_env_set_allocator(NULL, NULL, NULL);

    char *mem = tg_malloc(100);
    assert(mem);
    memset(mem, 'c', 100);
    for (int i = 0; i < 100; i++) {
        assert(mem[i] == 'c');
    }
    
    mem = tg_realloc(mem, 200);
    assert(mem);
    memset(mem+100, 'd', 100);
    for (int i = 0; i < 100; i++) {
        assert(mem[i] == 'c');
    }
    for (int i = 100; i < 200; i++) {
        assert(mem[i] == 'd');
    }
    tg_free(mem);


    test_memory_random_generation(0.5);
}

void test_memory_chaos() {
    test_memory_random_generation(2);
}

int main(int argc, char **argv) {
    seedrand();
    do_test(test_memory_stdlib);
    do_chaos_test(test_memory_chaos);
    return 0;
}
