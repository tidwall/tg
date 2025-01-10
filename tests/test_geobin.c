#include "tests.h"

void perfect_match(const char *geojson_or_wkt, const char *expect, int dims, double min[4], double max[4]) {
    struct tg_geom *g1;
    if (geojson_or_wkt[0] == '{') {
        g1 = tg_parse_geojson(geojson_or_wkt);
    } else {
        g1 = tg_parse_wkt(geojson_or_wkt);
    }
    if (tg_geom_error(g1)) {
        printf("%s\n", tg_geom_error(g1));
        assert(0);
    }

    double gmin[4], gmax[4];
    int gdims = tg_geom_fullrect(g1, gmin, gmax);
    cmpfullrect(gdims, gmin, gmax, dims, min, max);
    uint8_t geobin[100000];
    size_t geobinlen = tg_geom_geobin(g1, geobin, sizeof(geobin));
    assert(geobinlen <= sizeof(geobin));
    struct tg_geom *g2 = tg_parse_geobin(geobin, geobinlen);
    if (tg_geom_error(g2)) {
        printf("%s\n", tg_geom_error(g2));
        assert(0);
    }


    // printf("0x%02X\n", geobin[0]);
    gdims = tg_geobin_fullrect(geobin, geobinlen, gmin, gmax);

    cmpfullrect(gdims, gmin, gmax, dims, min, max);

    struct tg_rect rect = tg_geobin_rect(geobin, geobinlen);
    gdims = 2;
    gmin[0] = rect.min.x;
    gmin[1] = rect.min.y;
    gmax[0] = rect.max.x;
    gmax[1] = rect.max.y;
    cmpfullrect(gdims, gmin, gmax, 2, min, max);

    struct tg_point point = tg_geobin_point(geobin, geobinlen);
    gdims = 2;
    gmin[0] = point.x;
    gmin[1] = point.y;
    gmax[0] = point.x;
    gmax[1] = point.y;

    double min2[4], max2[4];
    min2[0] = (gmin[0]+gmax[0])/2;
    min2[1] = (gmin[1]+gmax[1])/2;
    max2[0] = min2[0];
    max2[1] = min2[1];
    cmpfullrect(gdims, gmin, gmax, 2, min2, max2);



    char geojson_or_wkt2[100000];
    size_t geojson_or_wkt2len;
    if (geojson_or_wkt[0] == '{') {
        geojson_or_wkt2len = tg_geom_geojson(g2, geojson_or_wkt2, sizeof(geojson_or_wkt2));
    } else {
        geojson_or_wkt2len = tg_geom_wkt(g2, geojson_or_wkt2, sizeof(geojson_or_wkt2));
    }
    assert(geojson_or_wkt2len < sizeof(geojson_or_wkt2));
    
    if (!expect) {
        expect = geojson_or_wkt;
    }
    if (strcmp(geojson_or_wkt2, expect) != 0) {
        printf("expected:\n\t%s\ngot\n\t%s\n", expect, geojson_or_wkt2); 
        assert(0);
    }

    tg_geom_free(g1);
    tg_geom_free(g2);
}

void test_geobin_basic_syntax(void) {
    assert(tg_geom_geobin(0, 0, 0) == 0);
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44]}", 0, 2, (double[4]){55, 44}, (double[4]){55, 44});
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44,33]}", 0, 3, (double[4]){55, 44, 33}, (double[4]){55, 44, 33});
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44,33,22]}", 0, 4, (double[4]){55, 44, 33, 22}, (double[4]){55, 44, 33, 22});
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44],\"a\":{\"b\":\"c\"}}", 0, 2, (double[4]){55, 44}, (double[4]){55, 44});
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44,33],\"a\":{\"b\":\"c\"}}", 0, 3, (double[4]){55, 44, 33}, (double[4]){55, 44, 33});
    perfect_match("{\"type\":\"Point\",\"coordinates\":[55,44,33,22],\"a\":{\"b\":\"c\"}}", 0, 4, (double[4]){55, 44, 33, 22}, (double[4]){55, 44, 33, 22});
    perfect_match("POINT(55 44)", 0, 2, (double[4]){55, 44}, (double[4]){55, 44});
    perfect_match("POINT(55 44 33)", 0, 3, (double[4]){55, 44, 33}, (double[4]){55, 44, 33});
    perfect_match("POINT(55 44 33 22)", 0, 4, (double[4]){55, 44, 33, 22}, (double[4]){55, 44, 33, 22});
    perfect_match("POINT(55 44)", 0, 2, (double[4]){55, 44}, (double[4]){55, 44});
    perfect_match("POINTZ(55 44 33)", "POINT(55 44 33)", 3, (double[4]){55, 44, 33}, (double[4]){55, 44, 33});
    perfect_match("POINTM(55 44 33)", "POINT M(55 44 33)", 3, (double[4]){55, 44, 33}, (double[4]){55, 44, 33});
    perfect_match("POINT(55 44 33 22)", 0, 4, (double[4]){55, 44, 33, 22}, (double[4]){55, 44, 33, 22});
    perfect_match("{\"type\":\"LineString\",\"coordinates\":[[55,44],[33,22]]}", 0, 2, (double[4]){33, 22}, (double[4]){55, 44});
    perfect_match("{\"type\":\"LineString\",\"coordinates\":[[55,44,11],[33,22,66]]}", 0, 3, (double[4]){33, 22,11}, (double[4]){55, 44,66});
    perfect_match("{\"type\":\"LineString\",\"coordinates\":[[55,44,11,99],[33,22,66,77]]}", 0, 4, (double[4]){33, 22, 11, 77}, (double[4]){55, 44, 66, 99});
    perfect_match("{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[33,44]]]}", 0, 2, (double[4]){33, 44}, (double[4]){55, 66});
    perfect_match("{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]}", 0, 2, (double[4]){33, 44}, (double[4]){55, 66});
    perfect_match(
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]}}", 
        "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{}}", 
        2, (double[4]){33, 44}, (double[4]){55, 66});
    perfect_match(
        "{\"type\":\"Feature\",\"id\":123,\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{\"a\":\"b\"}}", 
        0, 2, (double[4]){33, 44}, (double[4]){55, 66});
    perfect_match(
        "{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"Feature\",\"id\":123,\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{\"a\":\"b\"}},"
            "{\"type\":\"Feature\",\"id\":123,\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{\"a\":\"b\"}}"
        "]}",
        0, 2, (double[4]){33, 44}, (double[4]){55, 66});
}

void test_geobin_fail() {
    assert(tg_geom_error(gc_geom(tg_parse_geobin(0, 0))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x00},1))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x05},1))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02}, 1))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02,2}, 2))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02,1}, 2))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02,5}, 2))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02,0}, 2))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x02,0,0,0x1}, 4))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x04,0,0,0x1,0x0,0x0}, 6))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x04,0,0,0x1,0x0,0x0,0x0}, 7))));
    assert(tg_geom_error(gc_geom(tg_parse((uint8_t[]){0x04,0,0,0x1,0x0,0x0,0x0,0x2,0x0,0x0}, 10))));
    // invalid extra json. just the '{' character
    assert(tg_geom_error(gc_geom(tg_parse_hex("02007B000101000000000000000000F87F000000000000F87F"))));
}

void test_geobin_chaos() {
    uint8_t geobin[5000];
    size_t len;
    struct tg_geom *g;
    while (1) {
        g = tg_parse_geojson(
            "{\"type\":\"FeatureCollection\",\"features\":["
                "{\"type\":\"Feature\",\"id\":123,\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{\"a\":\"b\"}},"
                "{\"type\":\"Feature\",\"id\":123,\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[[[33,44],[55,66],[44,66],[33,44]],[[42,61],[46,61],[46,64],[42,61]]]},\"properties\":{\"a\":\"b\"}}"
            "]}");
        len = tg_geom_geobin(g, geobin, sizeof(geobin));
        if (g) break;
    }
    double secs = 3.0;
    double start = now();
    while (now()-start < secs) {
        struct tg_geom *g2 = tg_parse_geobin(geobin, len);
        tg_geom_free(g2);
    }
    tg_geom_free(g);
}

char *make_deep_geobin_hex(int depth) {
    char gcname[] = "04000001000000"; // FeatureCollection() one child
    char ptname[] = "0101000000000000000000F87F000000000000F87F"; // POINT EMPTY
    char *hex = malloc((strlen(gcname)*depth)+strlen(ptname)+1);
    assert(hex);
    int len = 0;
    // memcpy(hex+len, gbin, strlen(gbin));
    // len += strlen(gbin);
    for (int i = 0;i < depth; i++) {
        memcpy(hex+len, gcname, strlen(gcname));
        len += strlen(gcname);
    }
    memcpy(hex+len, ptname, strlen(ptname));
    len += strlen(ptname);
    hex[len] = '\0';
    return hex;
}

void test_geobin_max_depth() {
    // nested geometry collections
    char *hex;
    struct tg_geom *geom;
    
    int depths[] = { 1, 100, 1000, 1023, 1024, 1025, 2000 };

    for (size_t i = 0; i < sizeof(depths)/sizeof(int); i++) {
        hex = make_deep_geobin_hex(depths[i]);
        geom = tg_parse_hex(hex);
        if (depths[i] > 1024) {
            assert(tg_geom_error(geom));
        } else {
            assert(!tg_geom_error(geom));
        }
        tg_geom_free(geom);
        free(hex);
    }
}


int main(int argc, char **argv) {
    do_test(test_geobin_basic_syntax);
    do_test(test_geobin_fail);
    do_test(test_geobin_max_depth);
    do_chaos_test(test_geobin_chaos);
    do_test(test_geobin_basic_syntax);
    return 0;
}
