#include "tests.h"

void test_parse_autodetect() {
    char *input;
    input = "POINT(10 20 30 40)";
    struct tg_geom *g1 = tg_parse(input, strlen(input));
    input = " {\"type\":\"Point\",\"coordinates\":[10,20,30,40]}";
    struct tg_geom *g2 = tg_parse(input, strlen(input));
    input = "  POINT(10 20 30 40)";
    struct tg_geom *g3 = tg_parse(input, strlen(input));
    input = "{\"type\":\"Point\",\"coordinates\":[10,20,30,40]}";
    struct tg_geom *g4 = tg_parse(input, strlen(input));
    input = "01B90B0000000000000000244000000000000034400000000000003E400000000000004440";
    size_t hexlen = strlen(input);
    struct tg_geom *g5 = tg_parse(input, hexlen);
    input = (char*)((uint8_t[]) {
        0x01, 0xB9, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24,
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x34, 0x40, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x3E, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 
        0x40
    });
    struct tg_geom *g6 = tg_parse(input, hexlen/2);
    struct tg_geom *g7 = tg_parse(input, 0);
    struct tg_geom *g8 = tg_parse(0, 0);

    assert(tg_geom_equals(g1, g2));
    assert(tg_geom_equals(g2, g3));
    assert(tg_geom_equals(g3, g4));
    assert(tg_geom_equals(g4, g5));
    assert(tg_geom_equals(g5, g6));
    assert(tg_geom_error(g7));
    assert(tg_geom_error(g8));

    tg_geom_free(g1);
    tg_geom_free(g2);
    tg_geom_free(g3);
    tg_geom_free(g4);
    tg_geom_free(g5);
    tg_geom_free(g6);
    tg_geom_free(g7);
    tg_geom_free(g8);
}

int main(int argc, char **argv) {
    seedrand();
    do_test(test_parse_autodetect);
    return 0;
}
