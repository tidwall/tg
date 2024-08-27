#include "tests.h"

void __geom_wkt_match(int line, const struct tg_geom *geom, const char *wstr0) {
    assert(geom);
    const char *err = tg_geom_error(geom);
    if (err) {
        printf("line %d: A: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    char wstr1[4096];
    tg_geom_hex(geom, wstr1, sizeof(wstr1));
    struct tg_geom *geom2 = tg_parse_hex(wstr1);
    assert(geom2);
    err = tg_geom_error(geom2);
    if (err) {
        printf("line %d: B: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }

    tg_geom_wkt(geom2, wstr1, sizeof(wstr1));

    if (strcmp(wstr0, wstr1)) {
        printf("line %d: C: expected:\n\t%s\ngot:\n\t%s\n\n", line, wstr0, wstr1);
        exit(1);
    }

    size_t n = tg_geom_wkb(geom, (uint8_t*)wstr1, sizeof(wstr1));
    tg_geom_free(geom2);

    geom2 = tg_parse_wkb((uint8_t*)wstr1, n);
    assert(geom2);
    err = tg_geom_error(geom2);
    if (err) {
        printf("line %d: D: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    tg_geom_wkt(geom2, wstr1, sizeof(wstr1));

    if (strcmp(wstr0, wstr1)) {
        printf("line %d: E: expected:\n\t%s\ngot:\n\t%s\n\n", line, wstr0, wstr1);
        exit(1);
    }
    tg_geom_free(geom2);
}

struct tg_geom *__any_parse(int line, const char *input, bool oom) {
    bool json = false;
    bool hex = true;
    size_t n = strlen(input);
    for (size_t i = 0; i < n; i++) {
        if (hex && isxdigit(input[i])) continue;
        hex = false;
        if (input[i] <= ' ') continue;
        json = input[i] == '{';
        break;
    }
    struct tg_geom *geom = NULL;
    if (hex) { 
        geom = tg_parse_hex(input);
    } else if (json) {
        geom = tg_parse_geojson(input);
    } else {
        geom = tg_parse_wkt(input);
    }
    if (!geom) {
        if (oom) return NULL;
    }
    assert(geom);
    const char *err = tg_geom_error(geom);
    if (err) {
        printf("line %d: 1: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    char buf1[4096];
    char buf2[4096];
    tg_geom_hex(geom, buf1, sizeof(buf1));
    tg_geom_free(geom);
    geom = tg_parse_hex(buf1);
    if (!geom) {
        if (oom) return NULL;
    }
    assert(geom);
    err = tg_geom_error(geom);
    if (err) {
        printf("line %d: 2: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    tg_geom_hex(geom, buf2, sizeof(buf2));
    if (strcmp(buf1, buf2) != 0) {
        printf("line %d: hex reencode mismatch: expected:\n\t%s\n"
            "got:\n\t%s\n\n", line, buf1, buf2);
        abort();
    }
    return geom;
}

void __any_match_2(int line, const char *input, const char *wstr0) {
    struct tg_geom *geom = __any_parse(line, input, false);
    __geom_wkt_match(line, geom, wstr0);
    tg_geom_free(geom);
}

void __wkt_match(int line, const char *wstr0) {
    __any_match_2(line, wstr0, wstr0);
}

void __hex_match_err(int line, const char *wstr0, const char *experr) {
    struct tg_geom *geom = tg_parse_hex(wstr0);
    assert(geom);
    const char *err = tg_geom_error(geom);
    if (!err) err = "";
    if (strcmp(err, experr)) {
        bool failed = true;
        if (strstr(err, "ParseError: ") == err) {
            if (strcmp(err+strlen("ParseError: "), experr) == 0) {
                failed = false;
            }
        }
        if (failed) {
            printf("line %d: expected '%s', got '%s'\n", line, experr, err);
            exit(1);
        }
    }
    tg_geom_free(geom);
}

#define wkt_match(...) __wkt_match(__LINE__,__VA_ARGS__)
#define any_match_2(...) __any_match_2(__LINE__,__VA_ARGS__)
#define hex_match_err(...) __hex_match_err(__LINE__,__VA_ARGS__)
#define geom_wkt_match(...) __geom_wkt_match(__LINE__,__VA_ARGS__)

void test_wkb_basic_syntax() {
    // these test a passing WKT to HEX to WKB back to WKT
    wkt_match("POINT EMPTY");
    wkt_match("POINT(1 2)");
    wkt_match("POINT(1 2 3)");
    wkt_match("POINT M(1 2 3)");
    wkt_match("POINT(1 2 3 4)");
    any_match_2("POINT Z EMPTY", "POINT EMPTY");
    any_match_2("POINT M EMPTY", "POINT EMPTY");

    wkt_match("LINESTRING EMPTY");
    wkt_match("LINESTRING(1 2,3 4)");
    wkt_match("LINESTRING(1 2 3,4 5 6)");
    wkt_match("LINESTRING M(1 2 3,4 5 6)");
    wkt_match("LINESTRING(1 2 3 4,5 6 7 8)");
    any_match_2("LINESTRING Z EMPTY", "LINESTRING EMPTY");
    any_match_2("LINESTRING M EMPTY", "LINESTRING EMPTY");

    wkt_match("POLYGON EMPTY");
    wkt_match("POLYGON((1 2,3 4,1 2))");
    wkt_match("POLYGON((1 2,3 4,1 2),(2 3,4 5,2 3))");
    wkt_match("POLYGON((1 2 3,3 4 5,1 2 3))");
    wkt_match("POLYGON M((1 2 3,3 4 5,1 2 3))");
    wkt_match("POLYGON((1 2 3 4,3 4 5 6,1 2 3 4))");
    any_match_2("POLYGON Z EMPTY", "POLYGON EMPTY");
    any_match_2("POLYGON M EMPTY", "POLYGON EMPTY");

    wkt_match("MULTIPOINT EMPTY");
    wkt_match("MULTIPOINT(1 2)");
    wkt_match("MULTIPOINT(1 2 3)");
    wkt_match("MULTIPOINT M(1 2 3)");
    wkt_match("MULTIPOINT(1 2 3 4)");
    any_match_2("MULTIPOINT Z EMPTY", "MULTIPOINT EMPTY");
    any_match_2("MULTIPOINT M EMPTY", "MULTIPOINT EMPTY");

    wkt_match("MULTILINESTRING EMPTY");
    wkt_match("MULTILINESTRING((1 2,3 4))");
    wkt_match("MULTILINESTRING((1 2 3,3 4 5))");
    wkt_match("MULTILINESTRING M((1 2 3,3 4 5))");
    wkt_match("MULTILINESTRING((1 2 3 4,3 4 5 6))");
    any_match_2("MULTILINESTRING Z EMPTY", "MULTILINESTRING EMPTY");
    any_match_2("MULTILINESTRING M EMPTY", "MULTILINESTRING EMPTY");

    wkt_match("MULTIPOLYGON EMPTY");
    wkt_match("MULTIPOLYGON(((1 2,3 4,1 2)))");
    wkt_match("MULTIPOLYGON(((1 2 3,3 4 5,1 2 3)))");
    wkt_match("MULTIPOLYGON M(((1 2 3,3 4 5,1 2 3)))");
    wkt_match("MULTIPOLYGON(((1 2 3 4,3 4 5 6,1 2 3 4)))");
    any_match_2("MULTIPOLYGON Z EMPTY", "MULTIPOLYGON EMPTY");
    any_match_2("MULTIPOLYGON M EMPTY", "MULTIPOLYGON EMPTY");

    wkt_match("GEOMETRYCOLLECTION EMPTY");
    wkt_match("GEOMETRYCOLLECTION(POINT EMPTY)");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match("GEOMETRYCOLLECTION(POINT M(1 2 3))");
    any_match_2("GEOMETRYCOLLECTION M(POINT(1 2 3))", "GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match("GEOMETRYCOLLECTION(MULTIPOINT(1 2 3 4))");
    any_match_2("GEOMETRYCOLLECTION Z EMPTY", "GEOMETRYCOLLECTION EMPTY");
    any_match_2("GEOMETRYCOLLECTION M EMPTY", "GEOMETRYCOLLECTION EMPTY");
    any_match_2(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33,3],[-111,33,4],[-111,32,5],[-112,32,6],[-112,33,3]],"
            "[[1,1,0],[1,2,37],[2,2,1],[1,2,10],[1,1,0]]"
        "],\"hello\":[1,2,3]}",
        "POLYGON("
            "(-112 33 3,-111 33 4,-111 32 5,-112 32 6,-112 33 3),"
            "(1 1 0,1 2 37,2 2 1,1 2 10,1 1 0)"
        ")"
    );
    any_match_2(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33],[-111,33],[-111,32],[-112,32],[-112,33]],"
            "[[1,1],[1,2],[2,2],[1,2],[1,1]]"
        "],\"hello\":[1,2,3]}",
        "POLYGON("
            "(-112 33,-111 33,-111 32,-112 32,-112 33),"
            "(1 1,1 2,2 2,1 2,1 1)"
        ")"
    );
    any_match_2(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33,3,4],[-111,33,4,5],[-111,32,5,6],[-112,32,6,7],[-112,33,3,4]],"
            "[[1,1,0,1],[1,2,37,38],[2,2,1,2],[1,2,10,11],[1,1,0,1]]"
        "],\"hello\":[1,2,3]}",
        "POLYGON("
            "(-112 33 3 4,-111 33 4 5,-111 32 5 6,-112 32 6 7,-112 33 3 4),"
            "(1 1 0 1,1 2 37 38,2 2 1 2,1 2 10 11,1 1 0 1)"
        ")"
    );

    any_match_2(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2],[3,4],[1,2]"
        "],\"hello\":[1,2,3]}",
        "LINESTRING(1 2,3 4,1 2)");
    any_match_2(
        "{\"type\":\"Point\",\"coordinates\":[1,2],\"a\":1}",
        "POINT(1 2)");
    any_match_2(
        "{\"type\":\"Point\",\"coordinates\":[1,2,3],\"a\":1}",
        "POINT(1 2 3)");
    any_match_2(
        "{\"type\":\"Point\",\"coordinates\":[1,2,3,4],\"a\":1}",
        "POINT(1 2 3 4)");
}

void test_wkb_contrived() {
    hex_match_err("0199999", "invalid binary");
    hex_match_err("0110000000", "invalid type");
    hex_match_err("0101000000", "invalid binary");
    hex_match_err("01010000", "invalid binary");

    // simple big-endian
    any_match_2("00000000013FF00000000000004000000000000000", "POINT(1 2)");
    hex_match_err("00000000AA3FF00000000000004000000000000000", "invalid type");
    
    //01030000000100000003000000000000000000F03F000000000000004000000000000008400000000000001040000000000000F03F0000000000000040 POLYGON((1 2,3 4,1 2))
    //010200000002000000000000000000F03F000000000000004000000000000000400000000000000840 LINESTRING(1 2,2 3)
    //0101000000000000000000F03F0000000000000040 POINT(1 2)
    //010100000000000000000008400000000000001040 POINT(3 4)

    hex_match_err(
        "010200000001000000000000000000F03F000000000000004000000000000000400000000000000840",
        "lines must have two or more positions");
    hex_match_err(
        "01030000000100000002000000000000000000F03F000000000000004000000000000008400000000000001040000000000000F03F0000000000000040",
        "rings must have three or more positions");

    hex_match_err(
        "01030000000100000003000000000000000000F03F000000000000004000000000000008400000000000001040000000000000F03F0000000000000041",
        "rings must have matching first and last positions");

    hex_match_err(
       //001111111122222222
        "010400000001000000010200000002000000000000000000F03F000000000000004000000000000000400000000000000840",
        "invalid child type"
    );
    hex_match_err(
       //001111111122222222
        "0105000000010000000101000000000000000000F03F0000000000000040",
        "invalid child type"
    );
    hex_match_err(
       //001111111122222222
        "0106000000010000000101000000000000000000F03F0000000000000040",
        "invalid child type"
    );
    hex_match_err(
       //001111111122222222
        "",
        "invalid binary"
    );
    hex_match_err(
       //001111111122222222
        "02",
        "invalid binary"
    );
    hex_match_err(
       //001111111122222222
        "010600000001000000",
        "invalid binary"
    );

    


    struct tg_geom *geom = tg_parse_wkb((uint8_t[]){ 
        0x01,0x01,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    }, 21);
    assert(!tg_geom_error(geom));
    tg_geom_free(geom);
    
    geom = tg_parse_wkb((uint8_t[]){ 
        0x02,0x01,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    }, 21);
    assert(tg_geom_error(geom));
    tg_geom_free(geom);


    char buf[8] = { 0 };
    
    memset(buf, 'a', sizeof(buf)-1);
    assert(tg_geom_wkb(NULL, (uint8_t*)buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "aaaaaaa") == 0);
    
    memset(buf, 'a', sizeof(buf)-1);
    assert(tg_geom_hex(NULL, buf, 1) == 0);
    assert(strcmp(buf, "") == 0);
    
    memset(buf, 'a', sizeof(buf)-1);
    assert(tg_geom_hex(NULL, buf, 0) == 0);
    assert(strcmp(buf, "aaaaaaa") == 0);

    geom = tg_parse_hex("00000000013FF00000000000004000000000000000");
    assert(geom);

    memset(buf, 'a', sizeof(buf)-1);
    size_t n = tg_geom_hex(geom, buf, sizeof(buf));
    assert(n == 42);
    assert(strcmp(buf, "0101000") == 0);
    tg_geom_free(geom);
    

    const char *hex = 
        "010300000002000000050000000000000000005CC000000000008040400000000000"
        "C05BC000000000008040400000000000C05BC000000000000040400000000000005C"
        "C000000000000040400000000000005CC00000000000804040050000000000000000"
        "00F03F000000000000F03F000000000000F03F000000000000004000000000000000"
        "400000000000000040000000000000F03F0000000000000040000000000000F03F00"
        "0000000000F03F";
    for (size_t i = 0 ; i < strlen(hex)-1; i++) {
        geom = tg_parse_hexn(hex, i);
        assert(strcmp(tg_geom_error(geom), "ParseError: invalid binary") == 0);
        tg_geom_free(geom); 
    }

    hex = // GEOMETRYCOLLECTION(POINT(1 2 3),LINESTRING(1 2,3 4))
        "0107000000020000000101000000000000000000F03F000000000000004001020000"
        "0002000000000000000000F03F000000000000004000000000000008400000000000"
        "001040";
    hex_match_err(hex, "");

    

    // multipolygon - OK
    hex_match_err(
        "010300000002000000050000000000000000005CC000000000008040400000000000"
        "C05BC000000000008040400000000000C05BC000000000000040400000000000005C"
        "C000000000000040400000000000005CC00000000000804040050000000000000000"
        "00F03F000000000000F03F000000000000F03F000000000000004000000000000000"
        "400000000000000040000000000000F03F0000000000000040000000000000F03F00"
        "0000000000F03F",
        "");

    // bad hex
    hex_match_err(
        "0103000MM002000000050000000000000000005CC000000000008040400000000000"
        "C05BC000000000008040400000000000C05BC000000000000040400000000000005C"
        "C000000000000040400000000000005CC00000000000804040050000000000000000"
        "00F03F000000000000F03F000000000000F03F000000000000004000000000000000"
        "400000000000000040000000000000F03F0000000000000040000000000000F03F00"
        "0000000000F03F",
        "invalid binary");
    hex_match_err(
        "010300MM0002000000050000000000000000005CC000000000008040400000000000"
        "C05BC000000000008040400000000000C05BC000000000000040400000000000005C"
        "C000000000000040400000000000005CC00000000000804040050000000000000000"
        "00F03F000000000000F03F000000000000F03F000000000000004000000000000000"
        "400000000000000040000000000000F03F0000000000000040000000000000F03F00"
        "0000000000F03F",
        "invalid binary");


    hex_match_err("0107000000020000", "invalid binary");
    hex_match_err("0106000000020000", "invalid binary");
    hex_match_err("0105000000020000", "invalid binary");
    hex_match_err("0104000000020000", "invalid binary");
    // hex_match_err("010300000000000000", "polygons must have one or more rings");

    
    geom = tg_parse_hex(NULL);
    assert(strcmp(tg_geom_error(geom), "ParseError: invalid binary") == 0);
    tg_geom_free(geom);

    struct tg_poly *mpoly[] = {
        POLY(RING(P(1,2),P(3,4),P(1,2)),
             RING(P(7,8),P(9,10),P(7,8)),
             RING(P(13,14),P(15,16),P(13,14))),
        POLY(RING(P(19,20),P(21,22),P(19,20)),
             RING(P(25,26),P(27,28),P(25,26)),
             RING(P(31,32),P(33,34),P(31,32))),
        POLY(RING(P(37,38),P(39,40),P(37,38)),
             RING(P(43,44),P(45,46),P(43,44)),
             RING(P(49,50),P(51,52),P(49,50))),
    };

    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 0,3 4 0,1 2 0),"
                "(7 8 0,9 10 0,7 8 0),"
                "(13 14 0,15 16 0,13 14 0)"
            ")"
        ")");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 2, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 0,3 4 0,1 2 0),"
                "(7 8 0,9 10 0,7 8 0),"
                "(13 14 0,15 16 0,13 14 0)"
            "),("
                "(19 20 0,21 22 0,19 20 0),"
                "(25 26 0,27 28 0,25 26 0),"
                "(31 32 0,33 34 0,31 32 0)"
            ")"
        ")");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 0,3 4 0,1 2 0),"
                "(7 8 0,9 10 0,7 8 0),"
                "(13 14 0,15 16 0,13 14 0)"
            "),("
                "(19 20 0,21 22 0,19 20 0),"
                "(25 26 0,27 28 0,25 26 0),"
                "(31 32 0,33 34 0,31 32 0)"
            "),("
                "(37 38 0,39 40 0,37 38 0),"
                "(43 44 0,45 46 0,43 44 0),"
                "(49 50 0,51 52 0,49 50 0)"
            ")"
        ")");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {1,2,3,4,5,6,7,8,9,1,2,3,4,5,6,7,8,9}, 10
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 1,3 4 2,1 2 3),"
                "(7 8 4,9 10 5,7 8 6),"
                "(13 14 7,15 16 8,13 14 9)"
            "),("
                "(19 20 1,21 22 0,19 20 0),"
                "(25 26 0,27 28 0,25 26 0),"
                "(31 32 0,33 34 0,31 32 0)"
            "),("
                "(37 38 0,39 40 0,37 38 0),"
                "(43 44 0,45 46 0,43 44 0),"
                "(49 50 0,51 52 0,49 50 0)"
            ")"
        ")");


    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {1,2,3,4,5,6,7,8,9,
                        1,2,3,4,5,6,7,8,9,
                        1,2,3,4,5,6,7,8,9}, 10
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 1 2,3 4 3 4,1 2 5 6),"
                "(7 8 7 8,9 10 9 1,7 8 0 0),"
                "(13 14 0 0,15 16 0 0,13 14 0 0)"
            ")"
        ")");


    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {1,2,3,4,5,6,7,8,9,
                        1,2,3,4,5,6,7,8,9,
                        1,2,3,4,5,6,7,8,9}, 19
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 1 2,3 4 3 4,1 2 5 6),"
                "(7 8 7 8,9 10 9 1,7 8 2 3),"
                "(13 14 4 5,15 16 6 7,13 14 8 9)"
            "),("
                "(19 20 1 0,21 22 0 0,19 20 0 0),"
                "(25 26 0 0,27 28 0 0,25 26 0 0),"
                "(31 32 0 0,33 34 0 0,31 32 0 0)"
            "),("
                "(37 38 0 0,39 40 0 0,37 38 0 0),"
                "(43 44 0 0,45 46 0 0,43 44 0 0),"
                "(49 50 0 0,51 52 0 0,49 50 0 0)"
            ")"
        ")");

    struct tg_line *mline[] = {
        LINE(P(1,2),P(3,4),P(5,6)),
        LINE(P(7,8),P(9,10),P(11,12)),
        LINE(P(13,14),P(15,16),P(17,18)),
    };
    geom_wkt_match(
        gc_geom(tg_geom_new_multilinestring_z(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "MULTILINESTRING("
            "(1 2 0,3 4 0,5 6 0),"
            "(7 8 0,9 10 0,11 12 0),"
            "(13 14 0,15 16 0,17 18 0)"
        ")");
    geom_wkt_match(
        gc_geom(tg_geom_new_multilinestring_z(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 5
        )),
        "MULTILINESTRING("
            "(1 2 7,3 4 6,5 6 5),"
            "(7 8 4,9 10 3,11 12 0),"
            "(13 14 0,15 16 0,17 18 0)"
        ")");

    geom_wkt_match(
        gc_geom(tg_geom_new_multilinestring_zm(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 8
        )),
        "MULTILINESTRING("
            "(1 2 7 6,3 4 5 4,5 6 3 2),"
            "(7 8 1 9,9 10 0 0,11 12 0 0),"
            "(13 14 0 0,15 16 0 0,17 18 0 0)"
        ")");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4),P(5,6) }, 3, 
            (double[]) {1,2,3}, 3
        )),
        "MULTIPOINT(1 2 1,3 4 2,5 6 3)");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4),P(5,6) }, 3, 
            (double[]) {1,2,3}, 2
        )),
        "MULTIPOINT(1 2 1,3 4 2,5 6 0)");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4),P(5,6) }, 3, 
            (double[]) {1,2,3}, 1
        )),
        "MULTIPOINT(1 2 1,3 4 0,5 6 0)");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {1,2,3}, 3
        )),
        "MULTIPOINT(1 2 1 2,3 4 3 0)");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {1,2,3}, 2
        )),
        "MULTIPOINT(1 2 1 2,3 4 0 0)");

    struct tg_geom *g1;
    
    g1 = tg_parse_hexn("0101000000000000000000F03F0000000000000040", 42);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_hexn_ix("0101000000000000000000F03F0000000000000040", 42, 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

    g1 = tg_parse_hex("0101000000000000000000F03F0000000000000040");
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_hex_ix("0101000000000000000000F03F0000000000000040", 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

    g1 = tg_parse_wkb((uint8_t[]){ 
        0x01,0x01,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    }, 21);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_wkb_ix((uint8_t[]){ 
        0x01,0x01,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
    }, 21, 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

}


char *make_deep_wkb(int depth) {
    char gcname[] = "010700000001000000"; // GEOMETRYCOLLECTION() one child
    char ptname[] = "0101000000000000000000F87F000000000000F87F"; // POINT EMPTY
    char *wkt = malloc((strlen(gcname)*depth)+strlen(ptname)+1);
    assert(wkt);
    int len = 0;
    for (int i = 0;i < depth; i++) {
        memcpy(wkt+len, gcname, strlen(gcname));
        len += strlen(gcname);
    }
    memcpy(wkt+len, ptname, strlen(ptname));
    len += strlen(ptname);
    wkt[len] = '\0';
    return wkt;
}

void test_wkb_max_depth() {
    // nested geometry collections
    char *wkt;
    struct tg_geom *geom;
    
    int depths[] = { 1, 100, 1000, 1023, 1024, 1025, 2000 };

    for (size_t i = 0; i < sizeof(depths)/sizeof(int); i++) {
        wkt = make_deep_wkb(depths[i]);
        geom = tg_parse_hex(wkt);
        if (depths[i] > 1024) {
            assert(tg_geom_error(geom));
        } else {
            assert(!tg_geom_error(geom));
        }
        tg_geom_free(geom);
        free(wkt);
    }
}



#define ok_or_oom(wkt) { \
    struct tg_geom *geom = __any_parse(__LINE__, wkt, true); \
    if (geom) { \
        if (tg_geom_error(geom)) { \
            printf("line %d: expected '(null)', got '%s'\n", __LINE__, \
                tg_geom_error(geom)); \
            exit(1); \
        } \
        tg_geom_free(geom); \
    } \
}

#define ok_or_oom_wkb(wkb, sz) { \
    struct tg_geom *geom = tg_parse_wkb(wkb, sz); \
    if (geom) { \
        if (tg_geom_error(geom)) { \
            printf("line %d: expected '(null)', got '%s'\n", __LINE__, \
                tg_geom_error(geom)); \
            exit(1); \
        } \
        tg_geom_free(geom); \
    } \
}

void test_wkb_chaos() {
    double secs = 2.0;
    double start = now();
    char buf[512];
    while (now()-start < secs) {
        ok_or_oom("POINT(1 2)");
        ok_or_oom("GeometryCollection(Point(0 0),Point(1 2),POINT EMPTY)");
        ok_or_oom("MultiPolygon("
            "("
                "(1 1 4 5,1 2 7 6,2 2 6 7,1 2 5 8,1 1 4 5),"
                "(1 1 4 5,1 2 7 6,2 2 6 7,1 2 5 8,1 1 4 5)"
            "),("
                "(10 10 40 50,10 20 70 60,20 20 60 70,10 20 50 80,10 10 40 50),"
                "(10 10 40 50,10 20 70 60,20 20 60 70,10 20 50 80,10 10 40 50)"
            ")"
        ")");
        ok_or_oom("LineString("
            "1 2 22 222,3 4 44 444,5 6 66 666"
        ")");
        ok_or_oom("MultiLineString("
            "(1 2 22 222,3 4 44 444,5 6 66 666),"
            "(10 20 220 2220,30 40 440 4440,50 60 660 6660)"
        ")");
        ok_or_oom("MultiPoint("
                "1 2 22 222,3 4 44 444,5 6 66 666,10 20 220 2220,"
                "30 40 440 4440,50 60 660 6660)");

        ok_or_oom("MultiPoint M("
                "1 2 22,3 4 44,5 6 66,10 20 220,"
                "30 40 440,50 60 660)");

        ok_or_oom("MultiPoint Z("
                "1 2 22,3 4 44,5 6 66,10 20 220,"
                "30 40 440,50 60 660)");

        ok_or_oom_wkb(((uint8_t[]){ 
            0x01,0x01,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0xF0,0x3F,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
        }), 21);

        ok_or_oom_wkb(((uint8_t[]){ 
            0x01,0x01,0x00,0x00,0x00,
            0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x7F,
            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,
        }), 21);

    }
}

void wkb_partial_writes_for_wkt(char *wkt) {
    struct tg_geom *geom = tg_parse_wkt(wkt);
    assert(!tg_geom_error(geom));
    uint8_t wkb[1000];
    size_t wkb_size = tg_geom_wkb(geom, wkb, sizeof(wkb));
    for (size_t i = 0; i < sizeof(wkb); i++) {
        void *data = malloc(i);
        assert(data);
        size_t wkb_size2 = tg_geom_wkb(geom, data, i);
        // printf("%d %d\n", wkb_size, wkb_size2);
        assert(wkb_size2 == wkb_size);
        assert(memcmp(wkb, data, i > wkb_size ? wkb_size: i) == 0);
        free(data);
    }
    tg_geom_free(geom);
}

void test_wkb_partial_writes() {
    wkb_partial_writes_for_wkt("POLYGON((1 2,3 4,5 6,7 8,9 0,1 2))");
    wkb_partial_writes_for_wkt("POLYGON((1 2 1,3 4 2,5 6 3,7 8 4,9 0 5,1 2 1))");
    wkb_partial_writes_for_wkt("POLYGON((1 2 1 2,3 4 2 3,5 6 3 4,7 8 4 5,9 0 5 6,1 2 1 2))");
}

void test_wkb_with_srid() {
    struct tg_geom *geom0 = tg_parse_hex("01020000200912000007000000642F25DC75A24CC0E4DE5740FCB34840A7CEFE9B72A24CC09DA85B2CFBB34840B5519D0E64A24CC091FAA188FBB34840FA449E245DA24CC054C2137AFDB34840F4ACFFCE51A24CC09FEB562A03B448405328C1D144A24CC09A3DD00A0CB44840404C10C03CA24CC0EA07FE6910B44840");
    assert(!tg_geom_error(geom0));
    struct tg_geom *geom1 = tg_parse_hex("0102000020091200000E00000004BE47A23CA24CC098A1F14410B448409871AEBC3FA24CC078341F2114B448400858AB764DA24CC09D0546031DB448406BFD3E2D50A24CC0BEDDEDD522B4484004824AA654A24CC02DC9A60128B44840EE377FB850A24CC0FA18BD642DB44840CCAF8B474EA24CC02CCCE78134B44840D7158E7B4EA24CC01D7C17A53AB44840ACFA01B452A24CC02688BA0F40B44840DB508C8752A24CC006CDF80846B44840A1F31ABB44A24CC0C891730756B44840009AF7EE45A24CC06B7649415CB448408C2ECAC749A24CC0CE57248161B44840A74302A150A24CC07DD00E1368B44840");
    assert(!tg_geom_error(geom1));
    assert(tg_geom_intersects(geom0, geom1));
    tg_geom_free(geom0);
    tg_geom_free(geom1);
}

void test_wkb_various() {
    struct tg_geom *geom;
    geom = tg_parse_hex(0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: invalid binary") == 0);
    tg_geom_free(geom);
    geom = tg_parse_hex_ix(0, 0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: invalid binary") == 0);
    tg_geom_free(geom);
}

void test_wkb_big_shapes() {
    struct tg_geom *geom = load_geom("bc", TG_NONE);
    size_t n = tg_geom_wkb(geom, 0, 0);
    uint8_t *buf = malloc(n+1);
    assert(buf);
    size_t n2 = tg_geom_wkb(geom, buf, n+1);
    assert(n2 == n); 
    struct tg_geom *geom2 = tg_parse_wkb_ix(buf, n2, TG_NONE);
    free(buf);
    assert(tg_geom_equals(geom, geom2));
    tg_geom_free(geom);
    tg_geom_free(geom2);
}

void test_wkb_geometrycollection() {
    struct tg_geom *g1 = tg_parse_wkt("POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))");
    assert(!tg_geom_error(g1));
    struct tg_geom *g2 = tg_parse_wkt("POLYGON ((300 100, 400 400, 200 400, 100 200, 300 100))");
    assert(!tg_geom_error(g2));
    struct tg_geom *collection = tg_geom_new_geometrycollection((const struct tg_geom*const[]) {g1, g2}, 2);
    assert(!tg_geom_error(collection));
    uint8_t dst1[1024];
    uint8_t dst2[1024];
    size_t sz1 = tg_geom_wkb(tg_geom_geometry_at(collection, 1), dst1, sizeof(dst1));
    struct tg_geom *g3 = tg_parse_wkb(dst1, sz1);
    size_t sz2 = tg_geom_wkb(g3, dst2, sizeof(dst2));
    assert(sz1 == sz2 && memcmp(dst1, dst2, sz1) == 0);
    tg_geom_free(g1);
    tg_geom_free(g2);
    tg_geom_free(collection);
    tg_geom_free(g3);
}


int main(int argc, char **argv) {
    do_test(test_wkb_basic_syntax);
    do_test(test_wkb_max_depth);
    do_test(test_wkb_contrived);
    do_test(test_wkb_partial_writes);
    do_chaos_test(test_wkb_chaos);
    do_test(test_wkb_with_srid);
    do_test(test_wkb_various);
    do_test(test_wkb_big_shapes);
    do_test(test_wkb_geometrycollection);
    return 0;
}
