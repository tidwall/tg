#include <assert.h>
#include <string.h>

#include "../tg.h"

static void test_parse_and_write(void) {
    struct tg_geom *geom = tg_parse_wkt("POINT(1 2)");
    assert(geom);
    assert(!tg_geom_error(geom));

    char wkt[32];
    assert(tg_geom_wkt(geom, wkt, sizeof(wkt)) == strlen("POINT(1 2)"));
    assert(strcmp(wkt, "POINT(1 2)") == 0);
    tg_geom_free(geom);
}

static void test_big_endian_wkb(void) {
    const char *hex = "00000000013FF00000000000004000000000000000";
    struct tg_geom *geom = tg_parse_hex(hex);
    assert(geom);
    assert(!tg_geom_error(geom));
    assert(tg_geom_point(geom).x == 1);
    assert(tg_geom_point(geom).y == 2);
    tg_geom_free(geom);
}

static void test_atomic_reference_counting(void) {
    struct tg_geom *geom = tg_parse_wkt("LINESTRING(0 0,1 1)");
    assert(geom);
    struct tg_geom *clone = tg_geom_clone(geom);
    assert(clone == geom);
    tg_geom_free(geom);
    char wkt[32];
    tg_geom_wkt(clone, wkt, sizeof(wkt));
    assert(strcmp(wkt, "LINESTRING(0 0,1 1)") == 0);
    tg_geom_free(clone);
}

int main(void) {
    test_parse_and_write();
    test_big_endian_wkb();
    test_atomic_reference_counting();
    return 0;
}
