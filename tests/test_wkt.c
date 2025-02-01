#include "tests.h"


void __geom_wkt_match(int line, const struct tg_geom *geom, const char *wstr0) {
    assert(geom);
    const char *err = tg_geom_error(geom);
    if (err) {
        printf("line %d: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    size_t len0 = strlen(wstr0);
    if (1) {
        size_t cap = 64;
        while (1) {
            char *wstr1 = malloc(cap);
            assert(wstr1);
            size_t len1 = tg_geom_wkt(geom, wstr1, cap);
            if (len1 > strlen(wstr1)) {
                cap *= 2;
                free(wstr1);
                continue;
            }
            // printf("%s\n", wstr1);
            if (strcmp(wstr0, wstr1)) {
                printf("line %d: expected:\n\t%s\ngot:\n\t%s\n\n", line, wstr0, wstr1);
                exit(1);
            }
            free(wstr1);
            break;
        }
    }
    for (size_t i = 0; i < len0; i++) {
        char *wstr1 = malloc(i);
        assert(wstr1);
        size_t len2 = tg_geom_wkt(geom, wstr1, i);
        assert(len2 == len0);
        if (i > 0) {
            size_t len1 = strlen(wstr1);
            if (len0 != len1) {
                assert(len0 > len1);
                assert(len1 == i-1);
            }
            assert(memcmp(wstr0, wstr1, len1) == 0);
        }
        free(wstr1);
    }
    // printf("%d\n", tg_geom_memsize(geom));
}

void __wkt_match_2(int line, const char *input, const char *wstr0) {
    struct tg_geom *geom = tg_parse_wkt(input);
    __geom_wkt_match(line, geom, wstr0);
    tg_geom_free(geom);
}

void __wkt_match(int line, const char *wstr0) {
    __wkt_match_2(line, wstr0, wstr0);
}



void __wkt_match_err(int line, const char *wstr0, const char *experr) {
    struct tg_geom *geom = tg_parse_wkt(wstr0);
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
#define wkt_match_2(...) __wkt_match_2(__LINE__,__VA_ARGS__)
#define wkt_match_err(...) __wkt_match_err(__LINE__,__VA_ARGS__)
#define geom_wkt_match(...) __geom_wkt_match(__LINE__,__VA_ARGS__)


void test_wkt_basic_syntax() {
    wkt_match_2("POINT ZM EMPTY", "POINT EMPTY"); 
    
    wkt_match_err(" POINT ", "invalid text"); 
    wkt_match_err("POINT", "invalid text"); 
    wkt_match_err("POINT EMPTY", ""); 
    wkt_match_err("POINT Z EMPTY", ""); 
    wkt_match_err("POINT ZM EMPTY", ""); 
    wkt_match_err("POINT ZM () asdf", "invalid text"); 
    wkt_match_err("POINT ZM (()", "invalid text"); 
    wkt_match_err("POINT ZM", "invalid text"); 
    wkt_match_err("POINT ZM ()", "each position must have four numbers"); 
    wkt_match_err("POINT Z ()", "each position must have three numbers"); 
    wkt_match_err("POINT()", "each position must have two to four numbers"); 
    wkt_match_err("POINT(1v)", "invalid text"); 
    wkt_match_err("POINT(1et)", "invalid text"); 
    wkt_match_2(" POINT ( -.5 -000 )", "POINT(-0.5 0)"); 
    wkt_match_err("POINT(-0.9)", "each position must have two to four numbers"); 
    wkt_match_err("POINT(-0.9 1)", ""); 
    wkt_match_err("POINT(-0.9 1,)", "invalid text"); 
    wkt_match_err("POINT(-0.9 1 ,)", "invalid text"); 
    wkt_match_err("POINT(-0.9 1 , )", "invalid text"); 
    wkt_match_err("POINT(-0.9 1. , )", "invalid text"); 
    wkt_match_err("POINT(-0.9 .1 , )", "invalid text"); 
    wkt_match_err("POINT( -0.9 .1 , )", "invalid text"); 
    wkt_match_err("POINT( -0.9 .1 , 1)", "invalid text"); 
    wkt_match_err("POINT( -0.9 1 3)", ""); 
    wkt_match_err("POINT( -0.9 1 3 )", ""); 
    wkt_match_err("POINT( -0.9 1 3 4)", ""); 
    wkt_match_err("POINT( -0.9\t1\r3 4)", ""); 
    wkt_match_2("POINT( 5 \t 1 \r 3 \n 4 )", "POINT(5 1 3 4)"); 
    wkt_match_err("POINT Z( 5 1 3 4)", "each position must have three numbers"); 
    wkt_match_err("POINT ZM( 5 1 3 4)", "");
    wkt_match_err("POINT ZM( 5 1 3 4 5)", "each position must have four numbers");
    wkt_match_err("POINT M( 5 1 3 4 5)", "each position must have three numbers");
    wkt_match_err("POINT M( 5 1 3)", "");

    wkt_match_err(" LINESTRING ", "invalid text");
    wkt_match_err(" LINESTRING ZM", "invalid text");
    wkt_match_2(" LINESTRINGZM EMPTY", "LINESTRING EMPTY");
    wkt_match_err(" LINESTRING ZM EMPTY", "");
    wkt_match_err(" LINESTRING ZM ()", "lines must have two or more positions");
    wkt_match_err(" LINESTRING ZM ()  ", "lines must have two or more positions");
    wkt_match_err(" LINESTRING ZM ()  z", "invalid text");
    wkt_match_err(" LINESTRING ZM (()  z", "invalid text");
    wkt_match_err(" LINESTRING ZM (1)", "each position must have four numbers");
    wkt_match_err(" LINESTRING ZM (1v)", "invalid text");
    wkt_match_err(" LINESTRING ZM (1 1 2 3)", "lines must have two or more positions");
    wkt_match_err(" LINESTRING ZM (1 1 2 3,3 4 5 5)", "");
    wkt_match_err(" LINESTRING ZM (1 1,3 4 5 5)", "each position must have four numbers");
    wkt_match_err(" LINESTRING (1 1,3 4 5 5)", "each position must have two numbers");
    wkt_match_err(" LINESTRING Z (1 2 3,3 4 5 5)", "each position must have three numbers");
    wkt_match_err(" LINESTRING M (1 2 3,3 4 5 5)", "each position must have three numbers");
    wkt_match_err(" LINESTRING (1 2,3 4)", "");
    wkt_match_err(" LINESTRING (1 2 3,3 4 5)", "");
    wkt_match_err(" LINESTRING M (1 2 3,3 4 5)", "");
    wkt_match_err(" LINESTRING (1 2 3 4,3 4 5 6)", "");
    
    wkt_match_err(" LINESTRING (1 2 3 4,)", "invalid text");
    wkt_match_err(" LINESTRING (1 2 3 4 , )", "invalid text");
    wkt_match_err(" LINESTRING (1 2 3 4 , ())", "invalid text");
    wkt_match_err(" LINESTRING (1 2 3 4 , ( ) )", "invalid text");

    wkt_match_2(" LINESTRINGM (1 \t 2 \r 3,3 \n 4 5)", "LINESTRING M(1 2 3,3 4 5)");
    wkt_match_err(" LINESTRING M (1 2 3,3 4 5 6 7)", "each position must have three numbers");

    wkt_match_2(" MULTIPOINT ( ( 1  2 ) , ( 3  4 ) ) ", "MULTIPOINT(1 2,3 4)");
    wkt_match_2(" MULTIPOINT ( ) ", "MULTIPOINT EMPTY" );
    wkt_match_2(" MULTIPOINT ( 1 2 3 ) ", "MULTIPOINT(1 2 3)" );
    wkt_match_err(" MULTIPOINT ( ( 1  2 ) , 3  4 ) ", "invalid text");
    wkt_match_err(" MULTIPOINT (-.4) ", "each position must have two to four numbers");
    wkt_match_err(" MULTIPOINT (-) ", "invalid text");
    wkt_match_2(" MULTIPOINT (-.5 000.5e4) ", "MULTIPOINT(-0.5 5e3)");
    wkt_match_err(" MULTIPOINT (()) ", "invalid text");
    wkt_match_err(" MULTIPOINT ((,)) ", "invalid text");
    wkt_match_err(" MULTIPOINT ((-,)) ", "invalid text");
    wkt_match_2(" MULTIPOINT ((-1 -1)) ", "MULTIPOINT(-1 -1)");
    wkt_match_err(" MULTIPOINT (-1.) ", "invalid text");
    wkt_match_err(" MULTIPOINT (-1.000v) ", "invalid text");
    wkt_match_2(" MULTIPOINT (1e5 2000e-2) ", "MULTIPOINT(1e5 20)");
    wkt_match_err(" MULTIPOINT (1e) ", "invalid text");
    wkt_match_err(" MULTIPOINT (1e-) ", "invalid text");
    wkt_match_2(" MULTIPOINT (500000000000000e-10 1) ", "MULTIPOINT(5e4 1)");

    wkt_match_2(" MULTIPOINT ( (1 2) , (2 3)) ", "MULTIPOINT(1 2,2 3)");
    wkt_match_err(" MULTIPOINT ( (1 2v ) , (2 3)) ", "invalid text");

    wkt_match_2(" POLYGON ( (1 2, 2 3, 1 2) , (2 3, 4 5, 2 3)) ", 
        "POLYGON((1 2,2 3,1 2),(2 3,4 5,2 3))");
    wkt_match_err(" POLYGON ( (1 2, 2 3) , (2 3, 4 5, 2 3)) ", 
        "rings must have three or more positions");
    wkt_match_err(" POLYGON ( (1 2, 2 3, 4 5) , (2 3, 4 5, 2 3)) ", 
        "rings must have matching first and last positions");


    wkt_match_2(" POLYGON ( (1 2 3, 2 3 4, 1 2 3) , (2 3 4, 4 5 6, 2 3 4)) ", 
        "POLYGON((1 2 3,2 3 4,1 2 3),(2 3 4,4 5 6,2 3 4))");

    wkt_match_2(" POLYGON Z( (1 2 3, 2 3 4, 1 2 3) , (2 3 4, 4 5 6, 2 3 4)) ", 
        "POLYGON((1 2 3,2 3 4,1 2 3),(2 3 4,4 5 6,2 3 4))");

    wkt_match_2(" POLYGON M( (1 2 3, 2 3 4, 1 2 3) , (2 3 4, 4 5 6, 2 3 4)) ", 
        "POLYGON M((1 2 3,2 3 4,1 2 3),(2 3 4,4 5 6,2 3 4))");


    wkt_match_2(" POLYGON ( (1 2 3 4, 2 3 4 5, 1 2 3 4) , (2 3 4 5, 4 5 6 7, 2 3 4 5)) ", 
        "POLYGON((1 2 3 4,2 3 4 5,1 2 3 4),(2 3 4 5,4 5 6 7,2 3 4 5))");

    wkt_match_2(" POLYGON ZM( (1 2 3 4, 2 3 4 5, 1 2 3 4) , (2 3 4 5, 4 5 6 7, 2 3 4 5)) ", 
        "POLYGON((1 2 3 4,2 3 4 5,1 2 3 4),(2 3 4 5,4 5 6 7,2 3 4 5))");

    wkt_match_err(" POLYGON ZM( (1 2 3 4, 2 3 4 5, 1 2 3 4) , (2 3 4 5, 4 5 6 7, 2 3 4 5v)) ", 
        "invalid text");

    wkt_match_err(" POLYGON ( xxx ) ", "invalid text");
    wkt_match_err(" POLYGON ( ( 1 2, 3 4, 1 2) x ) ", "invalid text");
    wkt_match_err(" POLYGON ( ( 1 2, 3 4, 1 2) , ) ", "invalid text");
    wkt_match_err(" POLYGON (  ) ", "polygons must have one or more rings");

    wkt_match_2(" MULTIPOINT (1 2) ", "MULTIPOINT(1 2)");
    wkt_match_2(" MULTIPOINT (1 2 3) ", "MULTIPOINT(1 2 3)");
    wkt_match_2(" MULTIPOINT (1 2 3) ", "MULTIPOINT(1 2 3)");
    wkt_match_2(" MULTIPOINT Z (1 2 3) ", "MULTIPOINT(1 2 3)");
    wkt_match_2(" MULTIPOINT M (1 2 3) ", "MULTIPOINT M(1 2 3)");
    wkt_match_2(" MULTIPOINT (1 2 3 4) ", "MULTIPOINT(1 2 3 4)");
    wkt_match_2(" MULTIPOINT ZM (1 2 3 4) ", "MULTIPOINT(1 2 3 4)");

    wkt_match("MULTILINESTRING((1 2,5 6))");
    wkt_match("MULTILINESTRING((1 2 3,5 6 7))");
    wkt_match("MULTILINESTRING((1 2 3,5 6 7))");
    wkt_match("MULTILINESTRING M((1 2 3,5 6 7))");
    wkt_match("MULTILINESTRING((1 2 3 4,5 6 7 8))");
    wkt_match("MULTILINESTRING((1 2 3 4,5 6 7 8))");
    wkt_match_err(" MULTILINESTRING ( ( 1 2, 5 6 ) ,  x)", "invalid text");
    wkt_match_err(" MULTILINESTRING ( ( 1v 2, 5 6 ) ,  x)", "invalid text");
    wkt_match_err(" MULTILINESTRING ( ( 1 2, 5 6 ) x )", "invalid text");
    wkt_match_err(" MULTILINESTRING ( ( 1 2, 5 6 ) , )", "invalid text");

    wkt_match("MULTIPOLYGON(((1 2,2 3,1 2)))");
    wkt_match("MULTIPOLYGON(((1 2 3,2 3 4,1 2 3)))");
    wkt_match("MULTIPOLYGON(((1 2 3,2 3 4,1 2 3)))");
    wkt_match("MULTIPOLYGON M(((1 2 3,2 3 4,1 2 3)))");
    wkt_match("MULTIPOLYGON(((1 2 3 4,2 3 4 5,1 2 3 4)))");
    wkt_match("MULTIPOLYGON(((1 2 3 4,2 3 4 5,1 2 3 4)))");
    
    wkt_match_err("MULTIPOLYGON(((1 2,2 3,1 2)) , x)", "invalid text");
    wkt_match_err("MULTIPOLYGON(((1 2v ,2 3,1 2)) , x)", "invalid text");

    wkt_match_err("MULTIPOLYGON(((1 2 ,2 3,1 2)) x)", "invalid text");
    wkt_match_err("MULTIPOLYGON(((1 2 ,2 3,1 2)) ,)", "invalid text");
    wkt_match_err("MULTIPOLYGON(((1 2 ,2 3,1 2),(1 2 ,2 3v,1 2)))", "invalid text");

    wkt_match_err("GEOMETRYCOLLECTION()", "missing type");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match("GEOMETRYCOLLECTION(POINT M(1 2 3))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3 4))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3 4))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2 3 4))");
    wkt_match_err("GEOMETRYCOLLECTION(POINTV(1 2))", "unknown type 'POINTV'");
    wkt_match_2("GEOMETRYCOLLECTION ZM(POINT(1 2 3))", "GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2) x )", "invalid text");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2) , X )", "unknown type 'X'");

    wkt_match_err("", "missing type");
    wkt_match_err(" ", "missing type");
    wkt_match_err(" POINT ZZ ", "invalid type specifier, expected 'Z', 'M', 'ZM', or 'EMPTY'");

    wkt_match("POINT EMPTY");
    wkt_match("LINESTRING EMPTY");
    wkt_match("POLYGON EMPTY");
    wkt_match("MULTIPOINT EMPTY");
    wkt_match("MULTILINESTRING EMPTY");
    wkt_match("MULTIPOLYGON EMPTY");
    wkt_match("GEOMETRYCOLLECTION EMPTY");

    wkt_match_2("PoinT Zm eMpTy", "POINT EMPTY");
    wkt_match_2("lInEsTrInG zM EmPtY", "LINESTRING EMPTY");


    wkt_match_err(" JE11O", "unknown type 'JE'");
    wkt_match_err(" je11o", "unknown type 'je'");

    wkt_match_err(" TOOBIGTOKNOWTOOBIGTOKNOWTOOBIGTOKNOW", 
        "unknown type 'TOOBIGTOKNOWTOOBIGTOKNOWTOOBIGTOKNOW'");

    wkt_match_err(" JELLO  ZM   ZM  EMPTY  ZM", 
        "invalid type specifier, expected 'Z', 'M', 'ZM', or 'EMPTY'");

    wkt_match("POINT(1 2)");
    wkt_match("POINT(1 2 3)");
    wkt_match("POINT(1 2 3 4)");
    wkt_match_2("POINTZ(1 2 3)", "POINT(1 2 3)");
    wkt_match_2("POINT Z(1 2 3)", "POINT(1 2 3)");
    wkt_match_2("POINTM(1 2 3)", "POINT M(1 2 3)");
    wkt_match("POINT M(1 2 3)");
    wkt_match_2("POINTZM(1 2 3 4)", "POINT(1 2 3 4)");
    wkt_match_2("POINT ZM(1 2 3 4)", "POINT(1 2 3 4)");
    wkt_match_2("POINT EMPTY", "POINT EMPTY");
    wkt_match_2("POINT Z EMPTY", "POINT EMPTY");
    wkt_match_2("POINT M EMPTY", "POINT EMPTY");
    wkt_match_2("POINT ZM EMPTY", "POINT EMPTY");

    wkt_match("LINESTRING(1 2,3 4)");
    wkt_match_2("LINESTRING Z(1 2 4,5 6 7)", "LINESTRING(1 2 4,5 6 7)");
    wkt_match("LINESTRING M(1 2 4,5 6 7)");
    wkt_match_2("LINESTRING ZM(1 2 4 5,6 7 8 9)", "LINESTRING(1 2 4 5,6 7 8 9)");
    wkt_match_2("LINESTRING EMPTY", "LINESTRING EMPTY");
    wkt_match_2("LINESTRING Z EMPTY", "LINESTRING EMPTY");
    wkt_match_2("LINESTRING M EMPTY", "LINESTRING EMPTY");
    wkt_match_2("LINESTRING ZM EMPTY", "LINESTRING EMPTY");
    wkt_match_2("LINESTRINGZM EMPTY", "LINESTRING EMPTY");

    wkt_match("POLYGON((1 2,3 4,1 2))");
    wkt_match("POLYGON((1 2 3,3 4 5,1 2 3))");
    wkt_match("POLYGON((1 2,3 4,1 2),(5 6,7 8,5 6))");
    wkt_match("POLYGON M((1 2 3,3 4 5,1 2 3),(5 6 7,7 8 9,5 6 7))");
    wkt_match_2("POLYGON Z((1 2 3,3 4 5,1 2 3),(5 6 7,7 8 9,5 6 7))", "POLYGON((1 2 3,3 4 5,1 2 3),(5 6 7,7 8 9,5 6 7))");
    wkt_match("POLYGON((1 2 3 4,3 4 5 6,1 2 3 4),(5 6 7 8,7 8 9 10,5 6 7 8))");
    wkt_match_2("POLYGON EMPTY", "POLYGON EMPTY");
    wkt_match_2("POLYGON Z EMPTY", "POLYGON EMPTY");
    wkt_match_2("POLYGON M EMPTY", "POLYGON EMPTY");
    wkt_match_2("POLYGON ZM EMPTY", "POLYGON EMPTY");

    wkt_match("MULTIPOINT(1 2,3 4,1 2)");
    wkt_match("MULTIPOINT(1 2 6,3 4 7,1 2 8)");
    wkt_match("MULTIPOINT(1 2 6 7,3 4 7 8,1 2 8 9)");
    wkt_match_2("MULTIPOINT EMPTY", "MULTIPOINT EMPTY");
    wkt_match_2("MULTIPOINT Z EMPTY", "MULTIPOINT EMPTY");
    wkt_match_2("MULTIPOINT M EMPTY", "MULTIPOINT EMPTY");
    wkt_match_2("MULTIPOINT ZM EMPTY", "MULTIPOINT EMPTY");

    wkt_match("MULTILINESTRING((1 2,3 4,1 2))");
    wkt_match_2("MULTILINESTRING EMPTY", "MULTILINESTRING EMPTY");
    wkt_match_2("MULTILINESTRING Z EMPTY", "MULTILINESTRING EMPTY");
    wkt_match_2("MULTILINESTRING M EMPTY", "MULTILINESTRING EMPTY");
    wkt_match_2("MULTILINESTRING ZM EMPTY", "MULTILINESTRING EMPTY");

    wkt_match_2("MULTILINESTRING Z((1 2 4,6 7 8))", "MULTILINESTRING((1 2 4,6 7 8))");
    wkt_match_2("MULTILINESTRING ZM((1 2 4 5,6 7 8 9))", "MULTILINESTRING((1 2 4 5,6 7 8 9))");

    wkt_match("MULTIPOLYGON(((1 2,3 4,1 2)))");
    wkt_match("MULTIPOLYGON(((1 2,3 4,1 2)),((4 5,3 4,4 5)))");
    wkt_match("MULTIPOLYGON(((1 2 3,3 4 5,1 2 3)))");
    wkt_match("MULTIPOLYGON M(((1 2 3,3 4 5,1 2 3)))");
    wkt_match_2("MULTIPOLYGON Z(((1 2 3,3 4 5,1 2 3)))", "MULTIPOLYGON(((1 2 3,3 4 5,1 2 3)))");
    wkt_match("MULTIPOLYGON(((1 2 3 3,3 4 5 5,1 2 3 3)))");
    wkt_match_2("MULTIPOLYGON ZM(((1 2 3 3,3 4 5 5,1 2 3 3)))", "MULTIPOLYGON(((1 2 3 3,3 4 5 5,1 2 3 3)))");
    
    wkt_match_2("MULTIPOLYGON EMPTY", "MULTIPOLYGON EMPTY");
    wkt_match_2("MULTIPOLYGON Z EMPTY", "MULTIPOLYGON EMPTY");
    wkt_match_2("MULTIPOLYGON M EMPTY", "MULTIPOLYGON EMPTY");
    wkt_match_2("MULTIPOLYGON ZM EMPTY", "MULTIPOLYGON EMPTY");

    wkt_match("GEOMETRYCOLLECTION(POINT(1 2))");
    wkt_match("GEOMETRYCOLLECTION(POINT(1 2),LINESTRING EMPTY)");
    wkt_match("GEOMETRYCOLLECTION EMPTY");
    wkt_match_2("GEOMETRYCOLLECTION Z EMPTY", "GEOMETRYCOLLECTION EMPTY");
    wkt_match_2("GEOMETRYCOLLECTION M EMPTY", "GEOMETRYCOLLECTION EMPTY");
    wkt_match_2("GEOMETRYCOLLECTION ZM EMPTY", "GEOMETRYCOLLECTION EMPTY");
    wkt_match_2("GEOMETRYCOLLECTION Z(POINT(1 2 3))", "GEOMETRYCOLLECTION(POINT(1 2 3))");
    wkt_match_2("GEOMETRYCOLLECTION Z(POINT(1 2 3),LINESTRING Z EMPTY)", "GEOMETRYCOLLECTION(POINT(1 2 3),LINESTRING EMPTY)");
    wkt_match_2("GEOMETRYCOLLECTION M(POINT M(1 2 3))", "GEOMETRYCOLLECTION(POINT M(1 2 3))");
    wkt_match_2("GEOMETRYCOLLECTION M(POINT M(1 2 3),LINESTRING M EMPTY)", "GEOMETRYCOLLECTION(POINT M(1 2 3),LINESTRING EMPTY)");
    wkt_match_2("GEOMETRYCOLLECTION ZM(POINT(1 2 3 4))", "GEOMETRYCOLLECTION(POINT(1 2 3 4))");
    wkt_match_2("GEOMETRYCOLLECTION ZM(POINT(1 2 3 4),LINESTRING ZM EMPTY)", "GEOMETRYCOLLECTION(POINT(1 2 3 4),LINESTRING EMPTY)");
    wkt_match_2(
        "GEOMETRYCOLLECTION( POINT(1 2) , POINT(1 2) )",
        "GEOMETRYCOLLECTION(POINT(1 2),POINT(1 2))");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 0, 
            (double[]) {}, 0
        )),
        "MULTIPOINT EMPTY");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            NULL, 0, 
            (double[]) {1,2,3}, 3
        )),
        "MULTIPOINT EMPTY");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {1,2,3}, 3
        )),
        "MULTIPOINT(1 2 1 2,3 4 3 0)");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "MULTIPOINT(1 2 0 0,3 4 0 0)");
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "MULTIPOINT(1 2 0 0,3 4 0 0)");
    
    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "MULTIPOINT(1 2 0,3 4 0)");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "MULTIPOINT(1 2 0,3 4 0)");

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

    struct tg_poly *mpoly[] = {
        POLY(RING(P(1,2),P(3,4),P(5,6)),
                RING(P(7,8),P(9,10),P(11,12)),
                RING(P(13,14),P(15,16),P(17,18))),
        POLY(RING(P(19,20),P(21,22),P(23,24)),
                RING(P(25,26),P(27,28),P(29,30)),
                RING(P(31,32),P(33,34),P(35,36))),
        POLY(RING(P(37,38),P(39,40),P(41,42)),
                RING(P(43,44),P(45,46),P(47,48)),
                RING(P(49,50),P(51,52),P(53,54))),
    };

    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 0,3 4 0,5 6 0),"
                "(7 8 0,9 10 0,11 12 0),"
                "(13 14 0,15 16 0,17 18 0)"
            ")"
        ")");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 11
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 7,3 4 6,5 6 5),"
                "(7 8 4,9 10 3,11 12 2),"
                "(13 14 1,15 16 9,17 18 5)"
            ")"
        ")");

    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,5,6,4,1,5,7,8}, 6
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 7 6,3 4 5 4,5 6 3 2),"
                "(7 8 0 0,9 10 0 0,11 12 0 0),"
                "(13 14 0 0,15 16 0 0,17 18 0 0)"
            ")"
        ")");
 

    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,5,6,4,1,5,7,8}, 19
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 7 6,3 4 5 4,5 6 3 2),"
                "(7 8 1 9,9 10 5 6,11 12 8 9),"
                "(13 14 5 6,15 16 4 1,17 18 5 7)"
            ")"
        ")");


    geom_wkt_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,5,6,4,1,5,7,8}, 5
        )),
        "MULTIPOLYGON("
            "("
                "(1 2 7 6,3 4 5 4,5 6 3 0),"
                "(7 8 0 0,9 10 0 0,11 12 0 0),"
                "(13 14 0 0,15 16 0 0,17 18 0 0)"
            ")"
        ")");

    geom_wkt_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            POLY(RING(P(1,2),P(3,4))) }, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 1)),
        "MULTIPOLYGON(((1 2 7,3 4 0)))"
    );

    geom_wkt_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            POLY(RING(P(1,2))) }, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 1)),
        "MULTIPOLYGON(((1 2 7)))"
    );

    geom_wkt_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            POLY(RING()) }, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0)),
        "MULTIPOLYGON((()))"
    );

    wkt_match_err("POINT", "invalid text");
    wkt_match_err("", "missing type");
    wkt_match_err("  POINT  ", "invalid text");
    wkt_match_err("  POINT ZZ  ", "invalid type specifier, expected 'Z', 'M', 'ZM', or 'EMPTY'");
    wkt_match_err("  POINTZ ZM  ", "unknown type 'POINTZ'");
    wkt_match_err("  POINT ZM (( ", "invalid text");
    wkt_match_err("  POINT ZM (())z", "invalid text");
    wkt_match_err("  POINT ZM (()) z", "invalid text");
    wkt_match_err("GEOMETRYCOLLECTION(POINTX(1 2))", "unknown type 'POINTX'");
    wkt_match_2("GEOMETRYCOLLECTION Z(POINT(1 2))", "GEOMETRYCOLLECTION(POINT(1 2))");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2)v)", "invalid text");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2),)", "missing type");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2) , )", "missing type");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2)  ,  ,  POINT(1 2))", "missing type");
    wkt_match_err("GEOMETRYCOLLECTION(POINT(1 2),,POINT(1 2))", "missing type");

    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2)),v((1 2,3 4,1 2))"
        ")", 
        "invalid text");
    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2)),((1v 2,3 4,1 2))"
        ")", 
        "invalid text");

    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2)) , ((1v 2,3 4,1 2))"
        ")", 
        "invalid text");

    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2)) b ((1v 2,3 4,1 2))"
        ")", 
        "invalid text");

    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2)) ,"
        ")", 
        "invalid text");

    wkt_match_err("MULTIPOLYGON("
            "((1 2,3 4,1 2),(1 2,3 4,1 2 v )) , "
        ")", 
        "invalid text");

    assert(tg_geom_wkt(NULL, NULL, 0) == 0);

    geom_wkt_match(gc_geom(tg_parse_geojson(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,2],[3,4],[1,2]]"
        "],\"hello\":[1,2,3]}")),
        "POLYGON((1 2,3 4,1 2))");

    geom_wkt_match(gc_geom(tg_parse_geojson(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2],[3,4],[1,2]"
        "],\"hello\":[1,2,3]}")),
        "LINESTRING(1 2,3 4,1 2)");

    geom_wkt_match(gc_geom(tg_geom_new_point_m(P(1,2),3)),
        "POINT M(1 2 3)");

    struct tg_geom *g1;
    
    g1 = tg_parse_wktn("POINT(1 2)", 10);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_wktn_ix("POINT(1 2)", 10, 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

    g1 = tg_parse_wkt("POINT(1 2)");
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_wkt_ix("POINT(1 2)", 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

}

char *make_deep_wkt(int depth) {
    char gcname[] = "GEOMETRYCOLLECTION()";
    char ptname[] = "POINT EMPTY";
    char *wkt = malloc((strlen(gcname)*depth)+strlen(ptname)+1);
    assert(wkt);
    int len = 0;
    for (int i = 0;i < depth; i++) {
        memcpy(wkt+len, gcname, strlen(gcname));
        len += strlen(gcname)-1;
    }
    memcpy(wkt+len, ptname, strlen(ptname));
    len += strlen(ptname);
    for (int i = 0;i < depth; i++) {
        wkt[len++] = ')'; 
    }
    wkt[len] = '\0';
    return wkt;
}

void test_wkt_max_depth() {
    // nested geometry collections
    char *wkt;
    struct tg_geom *geom;
    
    int depths[] = { 1, 100, 1000, 1023, 1024, 1025, 2000 };

    for (size_t i = 0; i < sizeof(depths)/sizeof(int); i++) {
        wkt = make_deep_wkt(depths[i]);
        geom = tg_parse_wkt(wkt);
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
    struct tg_geom *geom = tg_parse_wkt((wkt)); \
    if (geom) { \
        if (tg_geom_error(geom)) { \
            printf("line %d: expected '(null)', got '%s'\n", __LINE__, \
                tg_geom_error(geom)); \
            exit(1); \
        } \
        tg_geom_free(geom); \
    } \
}

void test_wkt_chaos() {
    double secs = 2.0;
    double start = now();
    char buf[512];
    while (now()-start < secs) {
        ok_or_oom("POINT(1 2)");
        ok_or_oom("GeometryCollection(Point(0 0),Point(0 0))");
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
    }
}

void test_wkt_various() {
    struct tg_geom *geom;
    geom = tg_parse_wkt(0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: missing type") == 0);
    tg_geom_free(geom);
    geom = tg_parse_wkt_ix(0, 0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: missing type") == 0);
    tg_geom_free(geom);

    geom = tg_parse_geojson("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[10,20]},\"properties\":{\"a\":\"b\"}}");
    char output[100];
    size_t len = tg_geom_wkt(geom, output, sizeof(output));
    assert(strcmp(output, "POINT(10 20)") == 0);
    tg_geom_free(geom);

}

void test_wkt_geometrycollection() {
    struct tg_geom *g1 = tg_parse_wkt("POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))");
    assert(!tg_geom_error(g1));
    struct tg_geom *g2 = tg_parse_wkt("POLYGON ((300 100, 400 400, 200 400, 100 200, 300 100))");
    assert(!tg_geom_error(g2));
    struct tg_geom *collection = tg_geom_new_geometrycollection((const struct tg_geom*const[]) {g1, g2}, 2);
    assert(!tg_geom_error(collection));
    char dst1[1024];
    char dst2[1024];
    tg_geom_wkt(tg_geom_geometry_at(collection, 1), dst1, sizeof(dst1));
    struct tg_geom *g3 = tg_parse_wkt(dst1);
    tg_geom_wkt(g3, dst2, sizeof(dst2));
    assert(strcmp(dst1, dst2) == 0);
    tg_geom_free(g1);
    tg_geom_free(g2);
    tg_geom_free(collection);
    tg_geom_free(g3);
}

int main(int argc, char **argv) {
    seedrand();
    do_test(test_wkt_basic_syntax);
    do_test(test_wkt_max_depth);
    do_chaos_test(test_wkt_chaos);
    do_test(test_wkt_various);
    do_test(test_wkt_geometrycollection);
    return 0;
}
