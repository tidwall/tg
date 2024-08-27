#include "tests.h"

// #define __assert assert
// #define 

bool json_valid(const char *json_str);

void __geom_geojson_match(int line, const struct tg_geom *geom, const char *jstr0) {
    assert(geom);
    const char *err = tg_geom_error(geom);
    if (err) {
        printf("line %d: expected '(null)', got '%s'\n", line, err);
        exit(1);
    }
    size_t len0 = strlen(jstr0);
    if (1) {
        size_t cap = 64;
        while (1) {
            char *jstr1 = malloc(cap);
            assert(jstr1);
            size_t len1 = tg_geom_geojson(geom, jstr1, cap);
            if (len1 > strlen(jstr1)) {
                cap *= 2;
                free(jstr1);
                continue;
            }
            // printf("%s\n", jstr1);
            if (strcmp(jstr0, jstr1)) {
                printf("line %d: expected:\n\t%s\ngot:\n\t%s\n\n", line, jstr0, jstr1);
                exit(1);
            }
            assert(json_valid(jstr1));
            free(jstr1);
            break;
        }
    }
    for (size_t i = 0; i < len0; i++) {
        char *jstr1 = malloc(i);
        assert(jstr1);
        size_t len2 = tg_geom_geojson(geom, jstr1, i);
        assert(len2 == len0);
        if (i > 0) {
            size_t len1 = strlen(jstr1);
            if (len0 != len1) {
                assert(len0 > len1);
                assert(len1 == i-1);
            }
            assert(memcmp(jstr0, jstr1, len1) == 0);
        }
        free(jstr1);
    }
    // printf("%d\n", tg_geom_memsize(geom));
}

void __geojson_match_2(int line, const char *input, const char *jstr0) {
    struct tg_geom *geom = tg_parse_geojson(input);
    __geom_geojson_match(line, geom, jstr0);
    tg_geom_free(geom);
}

void __geojson_match(int line, const char *jstr0) {
    __geojson_match_2(line, jstr0, jstr0);
}

void __geojson_match_err(int line, const char *jstr0, const char *experr) {
    struct tg_geom *geom = tg_parse_geojson(jstr0);
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

#define geojson_match(...) __geojson_match(__LINE__,__VA_ARGS__)
#define geojson_match_2(...) __geojson_match_2(__LINE__,__VA_ARGS__)
#define geojson_match_err(...) __geojson_match_err(__LINE__,__VA_ARGS__)
#define geom_geojson_match(...) __geom_geojson_match(__LINE__,__VA_ARGS__)

void test_geojson_basic_syntax(void) {
    geojson_match("{\"type\":\"Point\",\"coordinates\":[1,2]}");
    geojson_match("{\"type\":\"Point\",\"coordinates\":[1,2,3]}");
    geojson_match("{\"type\":\"Point\",\"coordinates\":[1,2,3,4]}");
    geojson_match("{\"type\":\"Feature\",\"geometry\":"
                    "{\"type\":\"Point\",\"coordinates\":[1,2]}"
                  ",\"properties\":{}}");
    geojson_match("{\"type\":\"Feature\",\"geometry\":"
                    "{\"type\":\"Point\",\"coordinates\":[1,2,3,4]}"
                  ",\"properties\":{}}");
    geojson_match("{\"type\":\"Feature\",\"id\":123,\"geometry\":"
                    "{\"type\":\"Point\",\"coordinates\":[1,2,3,4]}"
                  ",\"properties\":{}}");
    geojson_match("{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
                    "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
                  ",\"properties\":{}}");

    geojson_match("{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
                    "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
                  ",\"properties\":null}");

    geojson_match_2(
        "{\"type\":\"Feature\",\"id\":\"123\",\"id\":true,\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
        ",\"properties\":null,\"properties\":false}",
        
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
        ",\"properties\":null}"
    );

    geojson_match("{\"type\":\"Feature\",\"geometry\":null,\"properties\":{}}");
    geojson_match("{\"type\":\"Feature\",\"id\":123,\"geometry\":null,\"properties\":{}}");


    geojson_match("{\"type\":\"Point\",\"coordinates\":[]}");
    geojson_match("{\"type\":\"Point\",\"coordinates\":[1,2],\"a\":1}");

    geojson_match_err(
        "{\"type\":\"Point\",\"coordinates\":[null,2]}",
        "'coordinates' must only contain numbers");
    geojson_match_err(
        "{\"type\":\"Point\",\"coordinates\":[2]}",
        "'coordinates' must have two or more numbers");
    geojson_match_err(
        "{\"type\":\"Point\",\"coordinates\":null}",
        "'coordinates' must be an array");
    geojson_match_err(
        "{\"type\":\"GeometryCollection\",\"geometries\":null}",
        "'geometries' must be an array");
    geojson_match_err(
        "{\"type\":\"FeatureCollection\",\"features\":null}",
        "'features' must be an array");

    geojson_match_err(
        "{\"type\":\"Feature\",\"geometry\":true,\"properties\":{}}",
        "'geometry' must be an object or null");

    // geojson_match_err(
    //     "{\"type\":\"Feature\",\"geometry\":null}",
    //     "missing 'properties'");

    geojson_match_err(
        "\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1,2,3,4]}"
        ",\"properties\":{}}",
        "invalid json");
    geojson_match_err(
        "{\"type\":\"Feature\",\"id\":true,\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
        ",\"properties\":{}}",
        "'id' must be a string or number"
    );
    geojson_match_err(
        "{\"type\":\"Feature\",\"id\":1,\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
        ",\"properties\":true}",
        "'properties' must be an object or null");
    geojson_match_err("{\"type\":\"Point\"}", "missing 'coordinates'");
    geojson_match_err("{\"type\":\"Feature\"}", "missing 'geometry'");
    geojson_match_err("{\"type\":\"FeatureCollection\"}", "missing 'features'");
    geojson_match_err("{\"type\":\"GeometryCollection\"}", "missing 'geometries'");


    geojson_match("{\"type\":\"MultiPoint\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2]]}");
    geojson_match(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2],[3,4]]}");
    geojson_match(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3]]}");
    geojson_match(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3,4]]}");
    geojson_match(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3,4]],\"1\":2}");
    geojson_match_2(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3,4,5]]}",
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3,4]]}");
    geojson_match_err(
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,3,4,false]],\"1\":2}",
        "each element in a position must be a number");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2],[3,4],[5,6]"
        "]}");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2],[3,4],[5,6]"
        "],\"a\":1}");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2,22],[3,4,44],[5,6,66]"
        "]}");
    geojson_match(
        "{\"type\":\"LineString\",\"coordinates\":["
            "[1,2,22,222],[3,4,44,444],[5,6,66,666]"
        "]}");
    geojson_match(
        "{\"type\":\"Feature\",\"id\":1,\"geometry\":"
            "{\"type\":\"LineString\",\"coordinates\":["
                "[1,2,22,222],[3,4,44,444],[5,6,66,666]"
            "],\"hello\":\"world\"}"
        ",\"properties\":{\"1\":\"2\"}}");
    geojson_match_err(
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,22]]}",
        "lines must have two or more positions");

    geojson_match(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2],[3,4],[5,6]]"
        "]}");
    geojson_match(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2],[3,4],[5,6]],"
            "[[7,8],[9,10],[11,12]]"
        "]}");
    geojson_match(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,22],[3,4,44],[5,6,66]]"
        "]}");
    geojson_match(
        "{\"type\":\"MultiLineString\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,22,222],[3,4,44,444],[5,6,66,666]],"
            "[[10,20,220,2220],[30,40,440,4440],[50,60,660,6660]]"
        "]}");
    geojson_match_err(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[1,2,22,222],[3,4,44,444],[5,6,66,666],"
            "[[10,20,220,2220],[30,40,440,4440],[50,60,660,6660]]"
        "]}",
        "'coordinates' must be a two deep nested array of positions");
     geojson_match_err(
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "1,2,22,222,"
            "[[10,20,220,2220],[30,40,440,4440],[50,60,660,6660]]"
        "]}",
        "'coordinates' must be a two deep nested array of positions");

    geojson_match_err(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33,3,3.3],[-111,33,4,4.4],[-111,32,5],[-112,32,6],[-112,33,7]],"
            "[[1,1],[1,2],[2,2],[1,2,10],[1,1,0,11]]"
        "],\"hello\":[1,2,3]}",
        "each position must have the same number of dimensions");

    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33,3,3],[-111,33,4,4],[-111,32,5,4],[-112,32,6,65],[-112,33,3,3]],"
            "[[1,1,0,11],[1,2,37,33],[2,2,1,9],[1,2,10,0],[1,1,0,11]]"
        "],\"hello\":[1,2,3]}");

    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":[],\"a\":1}");

    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,1],[1,2],[2,2],[1,2],[1,1]]"
        "]}");
    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,1],[1,2],[2,2],[1,2],[1,1]]"
        "],\"a\":1}");

    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,1,3],[1,2,4],[2,2,5],[1,2,6],[1,1,3]]"
        "]}");


    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,1],[1,2],[2,2],[1,2],[1,1]]"
        "]}");

    // geojson_match_err(
    //     "{\"type\":\"Polygon\",\"coordinates\":["
    //         "[[-112,33,3,2],[-111,33,4,4],[-111,32,5,4],[-112,32,6,65],[-112,33,3,3]],"
    //         "[[1,1,0,11],[1,2,37,33],[2,2,1,9],[1,2,10,0],[1,1,0,11]]"
    //     "],\"hello\":[1,2,3]}",
    //     NULL);

    geojson_match_err(
        "{\"type\":\"Polygon\",\"coordinates\":[false]}",
        "'coordinates' must be a nested array");


    geojson_match_err(
        "{\"type\":\"MultiPolygon\",\"coordinates\":[[]]}",
        "polygons must have one or more rings");

    geojson_match(
        "{\"type\":\"MultiPolygon\",\"coordinates\":[]}");
    geojson_match(
        "{\"type\":\"MultiPolygon\",\"coordinates\":[],\"a\":1}");

    geojson_match_err(
        "{\"type\":\"MultiPolygon\",\"coordinates\":[true]}",
        "'coordinates' must be a three deep nested array of positions");

    geojson_match(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1],[1,2],[2,2],[1,2],[1,1]]]"
        "]}");
    geojson_match_err(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1],[1,1]]]"
        "]}",
        "rings must have three or more positions");
    geojson_match_err(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1],[1,2,3],[2,2],[1,2],[1,1]]]"
        "]}",
        "each position must have the same number of dimensions");

    geojson_match_err(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1],[1],[2,2],[1,2],[1,1]]]"
        "]}",
        "each position must have two or more numbers");

    geojson_match(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1,4],[1,2,7],[2,2,6],[1,2,5],[1,1,4]]]"
        "]}");
    geojson_match(
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1,4,5],[1,2,7,6],[2,2,6,7],[1,2,5,8],[1,1,4,5]]],"
            "[[[10,10,40,50],[10,20,70,60],[20,20,60,70],[10,20,50,80],[10,10,40,50]]]"
        "]}");

    geojson_match_err(
        "{\"type\":\"GeometryCollection\",\"geometries\":["
            "{\"type\":\"Point\",\"coordinates\":[]},"
            "{\"type\":\"Point\",\"coordinates\":[1,2]},"
            "{\"type\":\"LineString\",\"coordinates\":[[1,2]]}"
        "]}",
        "lines must have two or more positions");
    
    geojson_match(
        "{\"type\":\"GeometryCollection\",\"geometries\":["
            "{\"type\":\"Point\",\"coordinates\":[]},"
            "{\"type\":\"Point\",\"coordinates\":[1,2]}"
        "]}");

    geojson_match(
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[]}"
        ",\"properties\":null}");

    geojson_match_err(
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[true]}"
        ",\"properties\":null}",
        "'coordinates' must only contain numbers");

    geojson_match_err(
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Feature\",\"geometry\":null,\"properties\":{}}"
        ",\"properties\":null}",
        "'geometry' must only contain an object with the 'type' of Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, or GeometryCollection");

    geojson_match_err(
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"FeatureCollection\",\"features\":[]}"
        ",\"properties\":null}",
        "'geometry' must only contain an object with the 'type' of Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, or GeometryCollection");
    
    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
        ",\"properties\":null}");

    geojson_match_err(
        "{\"type\":\"FeatureCollection\",\"geometry\":"
            "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
        "}",
        "missing 'features'");
    geojson_match_err(
        "{\"type\":\"FeatureCollection\",\"features\":"
            "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
        "}",
        "'features' must be an array");
    geojson_match(
        "{\"type\":\"FeatureCollection\",\"features\":[]}");
    geojson_match(
        "{\"type\":\"FeatureCollection\",\"features\":[],\"a\":1}");
    geojson_match_err(
        "{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
        "],\"a\":1}",
        "'features' must only contain objects with the 'type' of Feature");
    geojson_match_err(
        "{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"MultiPoint\",\"coordinates\":[true],\"a\":1}"
        "],\"a\":1}",
        "'coordinates' must be an array of positions");
    geojson_match(
        "{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"a\":1},\"b\":2},"
            "{\"type\":\"Feature\",\"geometry\":"
                "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
            ",\"b\":2,\"properties\":null}"
        "],\"a\":1}");
    geojson_match_err(
        "[]",
        "expected an object"
    );
    geojson_match_err(
        "{}",
        "'type' is required"
    );
    geojson_match_err(
        "{\"type\":true}",
        "unknown type 'true'"
    );
    geojson_match_err(
        "{\"type\":true}",
        "unknown type 'true'"
    );

    geojson_match_err("{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,31,3],[-111,33,4],[-111,32,5],[-112,32,6],[-112,33,3]],"
            "[[1,1,0],[1,2,37],[2,2,1],[1,2,10],[1,1,0]]"
        "],\"hello\":[1,2,3]}",
        "rings must have matching first and last positions");


    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33,3],[-111,33,4],[-111,32,5],[-112,32,6],[-112,33,3]],"
            "[[1,1,0],[1,2,37],[2,2,1],[1,2,10],[1,1,0]]"
        "],\"hello\":[1,2,3]}");
    geojson_match(
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[-112,33],[-111,33],[-111,32],[-112,32],[-112,33]],"
            "[[1,1],[1,2],[2,2],[1,2],[1,1]]"
        "]}");

    geojson_match_err(
        "{\"type\":\"GeometryCollection\",\"geometries\":["
            "{\"type\":\"Feature\",\"geometry\":null,\"properties\":{}}"
        "]}",
        "'geometries' must only contain objects with the 'type' of Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, or GeometryCollection");

    geom_geojson_match(
        gc_geom(tg_geom_new_point((struct tg_point) { 1, 2 })),
        "{\"type\":\"Point\",\"coordinates\":[1,2]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_point_z((struct tg_point) { 1, 2 }, 3)),
        "{\"type\":\"Point\",\"coordinates\":[1,2,3]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_point_m((struct tg_point) { 1, 2 }, 3)),
        "{\"type\":\"Point\",\"coordinates\":[1,2,3]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_point_zm((struct tg_point) { 1, 2 }, 3, 4)),
        "{\"type\":\"Point\",\"coordinates\":[1,2,3,4]}");

    struct tg_geom *gerr = gc_geom(tg_parse_geojson("{}"));
    assert(gerr);
    char buf[100];
    tg_geom_geojson(gerr, buf, sizeof(buf));
    assert(strcmp(buf, "{\"type\":\"Point\",\"coordinates\":[]}") == 0);

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1,2]}"
        ",\"properties\":null}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"LineString\",\"coordinates\":[[1,2],[3,4]]}"
        ",\"properties\":null}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"LineString\",\"coordinates\":[[1,2],[3,4]]}"
        ",\"properties\":{}}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"LineString\",\"coordinates\":[[1,2],[3,4]],\"b\":2}"
        ",\"properties\":{\"a\":1}}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":[[[1,2],[3,4],[1,2]]]}"
        ",\"properties\":null}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":[[[1,2],[3,4],[1,2]]]}"
        ",\"properties\":{}}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":[[[1,2],[3,4],[1,2]]],\"b\":2}"
        ",\"properties\":{\"a\":1}}");

    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":["
                "[[1,2],[3,4],[1,2]],"
                "[[1,2],[3,4],[1,2]]"
            "]}"
        ",\"properties\":{}}");
    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":["
                "[[1,2],[3,4],[1,2]],"
                "[[1,2],[3,4],[1,2]]"
            "]}"
        ",\"properties\":null}");
    geojson_match(
        "{\"type\":\"Feature\",\"geometry\":"
            "{\"type\":\"Polygon\",\"coordinates\":["
                "[[1,2],[3,4],[1,2]],"
                "[[1,2],[3,4],[1,2]]"
            "],\"b\":2}"
        ",\"properties\":{\"a\":1}}");

    assert(tg_geom_geojson(0, 0, 0) == 0);

    geom_geojson_match(
        gc_geom(tg_geom_new_point_zm((struct tg_point) { NAN, +INFINITY }, -INFINITY, -0.0)),
        "{\"type\":\"Point\",\"coordinates\":[0,0,0,0]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 0, 
            (double[]) {}, 0
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_zm(
            NULL, 0, 
            (double[]) {1,2,3}, 3
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {1,2,3}, 3
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,1,2],[3,4,3,0]]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,0,0],[3,4,0,0]]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_zm(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,0,0],[3,4,0,0]]}");
    
    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,0],[3,4,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_multipoint_z(
            (struct tg_point[]) { P(1,2),P(3,4) }, 2, 
            (double[]) {}, 0
        )),
        "{\"type\":\"MultiPoint\",\"coordinates\":[[1,2,0],[3,4,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_linestring_z(
            LINE(P(1,2),P(3,4),P(5,6)), 
            (double[]) {}, 0
        )),
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,0],[3,4,0],[5,6,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_linestring_z(
            LINE(P(1,2),P(3,4),P(5,6)), 
            (double[]) {7,6}, 2
        )),
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,7],[3,4,6],[5,6,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_linestring_z(
            LINE(P(1,2),P(3,4),P(5,6)), 
            (double[]) {7,6,5,4,3}, 5
        )),
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,7],[3,4,6],[5,6,5]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_linestring_zm(
            LINE(P(1,2),P(3,4),P(5,6)), 
            (double[]) {7,6,5,4,3}, 5
        )),
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,7,6],[3,4,5,4],[5,6,3,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_linestring_zm(
            LINE(P(1,2),P(3,4),P(5,6)), 
            (double[]) {7,6,5,4,3}, 2
        )),
        "{\"type\":\"LineString\",\"coordinates\":[[1,2,7,6],[3,4,0,0],[5,6,0,0]]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_polygon_z(
            POLY(RING(P(1,2),P(3,4),P(5,6)),
                 RING(P(7,8),P(9,10),P(11,12))), 
            (double[]) {7,6,5,4,3}, 5
        )),
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,2,7],[3,4,6],[5,6,5]],"
            "[[7,8,4],[9,10,3],[11,12,0]]"
        "]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_polygon_z(
            POLY(RING(P(1,2),P(3,4),P(5,6)),
                 RING(P(7,8),P(9,10),P(11,12))), 
            (double[]) {7,6,5,4,3}, 2
        )),
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,2,7],[3,4,6],[5,6,0]],"
            "[[7,8,0],[9,10,0],[11,12,0]]"
        "]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_polygon_z(
            POLY(RING(P(1,2),P(3,4),P(5,6)),
                 RING(P(7,8),P(9,10),P(11,12)),
                 RING(P(13,14),P(15,16),P(17,18))), 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 7
        )),
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,2,7],[3,4,6],[5,6,5]],"
            "[[7,8,4],[9,10,3],[11,12,2]],"
            "[[13,14,1],[15,16,0],[17,18,0]]"
        "]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_polygon_z(
            POLY(RING(P(1,2),P(3,4),P(5,6)),
                 RING(P(7,8),P(9,10),P(11,12)),
                 RING(P(13,14),P(15,16),P(17,18))), 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 7
        )),
        "{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,2,7],[3,4,6],[5,6,5]],"
            "[[7,8,4],[9,10,3],[11,12,2]],"
            "[[13,14,1],[15,16,0],[17,18,0]]"
        "]}");



    struct tg_line *mline[] = {
        LINE(P(1,2),P(3,4),P(5,6)),
        LINE(P(7,8),P(9,10),P(11,12)),
        LINE(P(13,14),P(15,16),P(17,18)),
    };
    geom_geojson_match(
        gc_geom(tg_geom_new_multilinestring_z(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,0],[3,4,0],[5,6,0]],"
            "[[7,8,0],[9,10,0],[11,12,0]],"
            "[[13,14,0],[15,16,0],[17,18,0]]"
        "]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multilinestring_z(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 5
        )),
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,7],[3,4,6],[5,6,5]],"
            "[[7,8,4],[9,10,3],[11,12,0]],"
            "[[13,14,0],[15,16,0],[17,18,0]]"
        "]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_multilinestring_zm(
            (const struct tg_line *const*)mline, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 8
        )),
        "{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,7,6],[3,4,5,4],[5,6,3,2]],"
            "[[7,8,1,9],[9,10,0,0],[11,12,0,0]],"
            "[[13,14,0,0],[15,16,0,0],[17,18,0,0]]"
        "]}");








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

    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,0],[3,4,0],[5,6,0]],"
                "[[7,8,0],[9,10,0],[11,12,0]],"
                "[[13,14,0],[15,16,0],[17,18,0]]"
            "]"
        "]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 2, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,0],[3,4,0],[5,6,0]],"
                "[[7,8,0],[9,10,0],[11,12,0]],"
                "[[13,14,0],[15,16,0],[17,18,0]]"
            "],["
                "[[19,20,0],[21,22,0],[23,24,0]],"
                "[[25,26,0],[27,28,0],[29,30,0]],"
                "[[31,32,0],[33,34,0],[35,36,0]]"
            "]"
        "]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8}, 0
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,0],[3,4,0],[5,6,0]],"
                "[[7,8,0],[9,10,0],[11,12,0]],"
                "[[13,14,0],[15,16,0],[17,18,0]]"
            "],["
                "[[19,20,0],[21,22,0],[23,24,0]],"
                "[[25,26,0],[27,28,0],[29,30,0]],"
                "[[31,32,0],[33,34,0],[35,36,0]]"
            "],["
                "[[37,38,0],[39,40,0],[41,42,0]],"
                "[[43,44,0],[45,46,0],[47,48,0]],"
                "[[49,50,0],[51,52,0],[53,54,0]]"
            "]"
        "]}");
    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,10,11,12,5,6,3,1}, 10
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,7],[3,4,6],[5,6,5]],"
                "[[7,8,4],[9,10,3],[11,12,2]],"
                "[[13,14,1],[15,16,9],[17,18,5]]"
            "],["
                "[[19,20,6],[21,22,0],[23,24,0]],"
                "[[25,26,0],[27,28,0],[29,30,0]],"
                "[[31,32,0],[33,34,0],[35,36,0]]"
            "],["
                "[[37,38,0],[39,40,0],[41,42,0]],"
                "[[43,44,0],[45,46,0],[47,48,0]],"
                "[[49,50,0],[51,52,0],[53,54,0]]"
            "]"
        "]}");
        
    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_z(
            (const struct tg_poly *const*)mpoly, 1, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,10,11,12,5,6,3,1}, 10
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,7],[3,4,6],[5,6,5]],"
                "[[7,8,4],[9,10,3],[11,12,2]],"
                "[[13,14,1],[15,16,9],[17,18,5]]"
            "]"
        "]}");

    geom_geojson_match(
        gc_geom(tg_geom_new_multipolygon_zm(
            (const struct tg_poly *const*)mpoly, 3, 
            (double[]) {7,6,5,4,3,2,1,9,5,6,8,9,10,11,12, 5,6,3,1,8}, 20
        )),
        "{\"type\":\"MultiPolygon\",\"coordinates\":["
            "["
                "[[1,2,7,6],[3,4,5,4],[5,6,3,2]],"
                "[[7,8,1,9],[9,10,5,6],[11,12,8,9]],"
                "[[13,14,10,11],[15,16,12,5],[17,18,6,3]]"
            "],["
                "[[19,20,1,8],[21,22,0,0],[23,24,0,0]],"
                "[[25,26,0,0],[27,28,0,0],[29,30,0,0]],"
                "[[31,32,0,0],[33,34,0,0],[35,36,0,0]]"
            "],["
                "[[37,38,0,0],[39,40,0,0],[41,42,0,0]],"
                "[[43,44,0,0],[45,46,0,0],[47,48,0,0]],"
                "[[49,50,0,0],[51,52,0,0],[53,54,0,0]]"
            "]"
        "]}");

    assert(strcmp(tg_geom_error(NULL), "no memory")== 0);

    struct tg_geom *geom;
    geom = tg_parse_geojson("{\"type\":\"GeometryCollection\",\"geometries\":[]}");
    recteq(tg_geom_rect(geom), R(0,0,0,0));
    tg_geom_free(geom);

    geom = tg_parse_geojson("{\"type\":\"GeometryCollection\",}");
    assert(tg_geom_memsize(geom) > 0);
    tg_geom_free(geom);

    struct tg_poly *pp = POLY(RING(P(1,2),P(3,4)));
    geom_geojson_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            pp }, 1, NULL, 0)),
        "{\"type\":\"MultiPolygon\",\"coordinates\":[[[[1,2,0],[3,4,0]]]]}"
    );

    geom_geojson_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            POLY(RING(P(1,2))) }, 1, NULL, 0)),
        "{\"type\":\"MultiPolygon\",\"coordinates\":[[[[1,2,0]]]]}"
    );

    geom_geojson_match(
       gc_geom(tg_geom_new_multipolygon_z((const struct tg_poly *const[]) {
            POLY(RING()) }, 1, NULL, 0)),
        "{\"type\":\"MultiPolygon\",\"coordinates\":[[[]]]}"
    );

    struct tg_geom *g1;
    
    g1 = tg_parse_geojsonn("{\"type\":\"Point\",\"coordinates\":[1,2]}", 36);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_geojsonn_ix("{\"type\":\"Point\",\"coordinates\":[1,2]}", 36, 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);

    g1 = tg_parse_geojson("{\"type\":\"Point\",\"coordinates\":[1,2]}");
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);
    
    g1 = tg_parse_geojson_ix("{\"type\":\"Point\",\"coordinates\":[1,2]}", 0);
    assert(g1);
    assert(pointeq( tg_geom_point(g1), P(1, 2)));
    tg_geom_free(g1);


    geom = tg_parse_geojson(
        "{\"type\":\"Feature\",\"id\":\"hello\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1,2]}"
        ",\"properties\":{\"a\":1}}");
    assert(geom);
    assert(strcmp(tg_geom_extra_json(geom), "{\"id\":\"hello\",\"properties\":{\"a\":1}}") == 0);
    tg_geom_free(geom);
    

}

#define ok_or_oom(json) { \
    struct tg_geom *geom = tg_parse_geojson((json)); \
    if (geom) { \
        if (tg_geom_error(geom)) { \
            printf("line %d: expected '(null)', got '%s'\n", __LINE__, \
                tg_geom_error(geom)); \
            exit(1); \
        } \
        tg_geom_free(geom); \
    } \
}

void test_geojson_chaos() {
    double secs = 2.0;
    double start = now();
    char buf[512];
    while (now()-start < secs) {
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2]},\"properties\":{}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2],\"a\":1},\"properties\":{}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[1,2,3,4],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"id\":1,\"geometry\":null,\"properties\":null}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"MultiPoint\",\"coordinates\":[[0,0],[0,0]],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0],[0,0]],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"Feature\",\"geometry\":{\"type\":\"LineString\",\"coordinates\":[[0,0,0,0],[0,0,0,0]],\"a\":1},\"properties\":{\"b\":1}}");
        ok_or_oom("{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"Feature\",\"geometry\":"
                "{\"type\":\"Point\",\"coordinates\":[0,0]}"
            ",\"properties\":{}},"
            "{\"type\":\"Feature\",\"geometry\":"
                "{\"type\":\"Point\",\"coordinates\":[0,0]}"
            ",\"properties\":{}}"
        "],\"a\":1}");
        ok_or_oom("{\"type\":\"GeometryCollection\",\"geometries\":["
                "{\"type\":\"Point\",\"coordinates\":[0,0]},"
                "{\"type\":\"Point\",\"coordinates\":[0,0]}"
        "],\"a\":1}");
        ok_or_oom("{\"type\":\"MultiPolygon\",\"coordinates\":["
            "[[[1,1,4,5],[1,2,7,6],[2,2,6,7],[1,2,5,8],[1,1,4,5]],[[1,1,4,5],[1,2,7,6],[2,2,6,7],[1,2,5,8],[1,1,4,5]]],"
            "[[[10,10,40,50],[10,20,70,60],[20,20,60,70],[10,20,50,80],[10,10,40,50]],[[10,10,40,50],[10,20,70,60],[20,20,60,70],[10,20,50,80],[10,10,40,50]]]"
        "],\"a\":1}");
        ok_or_oom("{\"type\":\"Polygon\",\"coordinates\":["
            "[[1,1,4,5],[1,2,7,6],[2,2,6,7],[1,2,5,8],[1,1,4,5]]"
        "],\"a\":1}");
        ok_or_oom("{\"type\":\"LineString\",\"coordinates\":["
            "[1,2,22,222],[3,4,44,444],[5,6,66,666]"
        "],\"a\":1}");
        ok_or_oom("{\"type\":\"MultiLineString\",\"coordinates\":["
            "[[1,2,22,222],[3,4,44,444],[5,6,66,666]],"
            "[[10,20,220,2220],[30,40,440,4440],[50,60,660,6660]]"
        "],\"a\":1}");

        strcpy(buf, "{\"type\":\"Point\",\"coordinates\":[1,2],\"");
        rand_str(buf+strlen(buf), rand()%64);
        strcat(buf, "\":1}");
        ok_or_oom(buf);

        rand_str(buf, rand()%128);
        tg_geom_free(tg_parse_geojson(buf));

    }
}

void test_geojson_feature() {
    struct tg_geom *geom;

    geom = gc_geom(tg_parse_geojson(
        "{\"type\":\"Feature\",\"id\":\"123\",\"geometry\":"
            "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
        ",\"properties\":null}"
    ));
    assert(geom);
    assert(tg_geom_is_feature(geom));
    assert(!tg_geom_is_featurecollection(geom));

    geom = gc_geom(tg_parse_geojson(
        "{\"type\":\"Point\",\"coordinates\":[1.5,2,3,4]}"
    ));
    assert(geom);
    assert(!tg_geom_is_feature(geom));
    assert(!tg_geom_is_featurecollection(geom));

    geom = gc_geom(tg_parse_geojson(
        "{\"type\":\"FeatureCollection\",\"features\":["
            "{\"type\":\"Feature\",\"geometry\":null,\"properties\":{\"a\":1},\"b\":2},"
            "{\"type\":\"Feature\",\"geometry\":"
                "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
            ",\"b\":2,\"properties\":null}"
        "],\"a\":1}"
    ));
    assert(geom);
    assert(!tg_geom_is_feature(geom));
    assert(tg_geom_is_featurecollection(geom));


    geom = gc_geom(tg_parse_geojson(
        "{\"type\":\"GeometryCollection\",\"features\":["
            "{\"type\":\"MultiPoint\",\"coordinates\":[],\"a\":1}"
        "],\"a\":1}"
    ));
    assert(geom);
    assert(!tg_geom_is_feature(geom));
    assert(!tg_geom_is_featurecollection(geom));
}

void test_geojson_various() {
    struct tg_geom *geom;
    geom = tg_parse_geojson(0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: invalid json") == 0);
    tg_geom_free(geom);
    geom = tg_parse_geojson_ix(0, 0);
    assert(tg_geom_error(geom));
    assert(strcmp(tg_geom_error(geom), "ParseError: invalid json") == 0);
    tg_geom_free(geom);
}

void test_geojson_big_shapes() {
    struct tg_geom *geom = load_geom("bc", TG_NONE);
    size_t n = tg_geom_geojson(geom, 0, 0);
    char *buf = malloc(n+1);
    assert(buf);
    size_t n2 = tg_geom_geojson(geom, buf, n+1);
    assert(n2 == n); 
    struct tg_geom *geom2 = tg_parse_geojson_ix(buf, TG_NONE);
    free(buf);
    assert(tg_geom_equals(geom, geom2));
    tg_geom_free(geom);
    tg_geom_free(geom2);
}

void test_geojson_geometrycollection() {
    struct tg_geom *g1 = tg_parse_wkt("POLYGON ((30 10, 40 40, 20 40, 10 20, 30 10))");
    assert(!tg_geom_error(g1));
    struct tg_geom *g2 = tg_parse_wkt("POLYGON ((300 100, 400 400, 200 400, 100 200, 300 100))");
    assert(!tg_geom_error(g2));
    struct tg_geom *collection = tg_geom_new_geometrycollection((const struct tg_geom*const[]) {g1, g2}, 2);
    assert(!tg_geom_error(collection));
    char dst1[1024];
    char dst2[1024];
    tg_geom_geojson(tg_geom_geometry_at(collection, 1), dst1, sizeof(dst1));
    struct tg_geom *g3 = tg_parse_geojson(dst1);
    tg_geom_geojson(g3, dst2, sizeof(dst2));
    assert(strcmp(dst1, dst2) == 0);
    tg_geom_free(g1);
    tg_geom_free(g2);
    tg_geom_free(collection);
    tg_geom_free(g3);
}

int main(int argc, char **argv) {
    seedrand();
    do_test(test_geojson_basic_syntax);
    do_test(test_geojson_feature);
    do_test(test_geojson_various);
    do_test(test_geojson_big_shapes);
    do_chaos_test(test_geojson_chaos);
    do_test(test_geojson_geometrycollection);
    return 0;
}
