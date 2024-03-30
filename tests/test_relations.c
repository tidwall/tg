#include "tests.h"
#include <dirent.h>
#include "../deps/json.h"

/*
•-----------------------------------------------------•
|                Predicate checklist                  |
|-----------------------------------------------------|
| combo       | e | i , d | c , w | c , b | c , o , t |
|-----------------------------------------------------|
| point/point | * | * , * | * , * | * , * | * , * , * |
| point/line  | * | * , * | * , * | * , * | * , * , * |
| point/poly  | * | * , * | * , * | * , * | * , * , * |
| line/point  | * | * , * | * , * | * , * | * , * , * |
| line/line   | * | * , * | * , * | * , * |   ,   ,   |
| line/poly   | * | * , * | * , * | * , * |   ,   ,   |
| poly/point  | * | * , * | * , * | * , * | * , * , * |
| poly/line   | * | * , * | * , * | * , * |   ,   ,   |
| poly/poly   | * | * , * | * , * | * , * |   ,   , * |
•-----------------------------------------------------•
*/

/*
  {
    "case": "case name",
    "geoms": [
      "POINT(0 0)",
      "POINT(0 0)"
    ],
    "relate": [ "0FFFFFFF2", "0FFFFFFF2" ],
    "predicates": [
      { "equals":     ["T", "T"] },
      { "intersects": ["T", "T"] },
      { "disjoint":   ["F", "F"] },
      { "contains":   ["T", "T"] },
      { "within":     ["T", "T"] },
      { "covers":     ["T", "T"] },
      { "coveredby":  ["T", "T"] },
      { "crosses":    ["F", "F"] },
      { "overlaps":   ["F", "F"] },
      { "touches":    ["F", "F"] }
    ]
  },
*/

// #define ENABLE_RELATE
#define ENABLE_EQUALS
#define ENABLE_INTERSECTS
#define ENABLE_DISJOINT
#define ENABLE_CONTAINS
#define ENABLE_WITHIN
#define ENABLE_COVERS
#define ENABLE_COVEREDBY
// #define ENABLE_CROSSES
// #define ENABLE_OVERLAPS
#define ENABLE_TOUCHES

static const char *only_file = "all";
static const char *only_index = "all";

size_t json_strip_jsonc(char *src, size_t len) {
    size_t len_dst = 0;
    char *dst = src;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '/') {
            if (i < len-1) {
                if (src[i+1] == '/') {
                    dst[len_dst++] = ' ';
                    dst[len_dst++] = ' ';
                    i += 2;
                    for (; i < len; i++) {
                        if (src[i] == '\n') {
                            dst[len_dst++] = '\n';
                            break;
                        } else if (src[i] == '\t' || src[i] == '\r') {
                            dst[len_dst++] = src[i];
                        } else {
                            dst[len_dst++] = ' ';
                        }
                    }
                    continue;
                }
                if (src[i+1] == '*') {
                    dst[len_dst++] = ' ';
                    dst[len_dst++] = ' ';
                    i += 2;
                    for (; i < len-1; i++) {
                        if (src[i] == '*' && src[i+1] == '/') {
                            dst[len_dst++] = ' ';
                            dst[len_dst++] = ' ';
                            i++;
                            break;
                        } else if (src[i] == '\n' || src[i] == '\t' ||
                            src[i] == '\r')
                        {
                            dst[len_dst++] = src[i];
                        } else {
                            dst[len_dst++] = ' ';
                        }
                    }
                    continue;
                }
            }
        }
        dst[len_dst++] = src[i];
        if (src[i] == '"') {
            for (i = i + 1; i < len; i++) {
                dst[len_dst++] = src[i];
                if (src[i] == '"') {
                    size_t j = i - 1;
                    for (; ; j--) {
                        if (src[j] != '\\') {
                            break;
                        }
                    }
                    if ((j-i)%2 != 0) {
                        break;
                    }
                }
            }
        } else if (src[i] == '}' || src[i] == ']') {
            for (size_t j = len_dst - 2; ; j--) {
                if (dst[j] <= ' ') {
                    if (j == 0) {
                        break;
                    }
                    continue;
                }
                if (dst[j] == ',') {
                    dst[j] = ' ';
                }
                break;
            }
        }
    }
    if (len_dst < len) {
        dst[len_dst] = '\0';
    }
    return len_dst;
}


static char *wkt_from_json_string(struct json json) {
    size_t len = json_string_length(json);
    char *str = malloc(len+1);
    assert(str);
    json_string_copy(json, str, len+1);
    return str;
}

static int json_get_line_number(const char *org, size_t org_len, struct json json) {
    const char *raw = json_raw(json);
    size_t raw_len = json_raw_length(json);
    if (raw < org && raw >= org+org_len) {
        return 0;
    }
    size_t n = raw-org;
    int line = 1;
    for (size_t i = 0; i < n; i++) {
        if (org[i] == '\n') {
            line++;
        }
    }
    return line;
}

static bool isjson(const char *str) {
    char *p = (char *)str;
    while (*p) {
        if (*p == '{') {
            return true;
        }
        if (!isspace(*p)) {
            break;
        }
        p++;
    }
    return false;
}

static struct tg_geom *geom_from_json(const char *path, const char *org_json,
    size_t org_len, struct json json, enum tg_index ix)
{
    char *text = wkt_from_json_string(json);
    struct tg_geom *geom;
    if (isjson(text)) {
        geom = tg_parse_geojson_ix(text, ix);
    } else {
        geom = tg_parse_wkt_ix(text, ix);
    }
    if (tg_geom_error(geom)) {
        int line = json_get_line_number(org_json, org_len, json);
        fprintf(stderr, "%s:%d: %s\n\t%s\n", 
            path, line, tg_geom_error(geom), text);
        abort();
    }
    free(text);
    return geom;
}

const char *ixstr4idx(int idx) {
    switch (idx) {
    case 0: return "none";
    case 1: return "natural";
    case 2: return "ystripes";
    default: return "unknown";
    }
}

static bool test_case_predicate(const char *path, const char *org_json, 
    size_t org_len, struct json json, const char *predicate, 
    struct tg_geom **a_arr, struct tg_geom **b_arr, 
    bool ab, bool ba, const char *relate_ab, const char *relate_ba)
{
    bool ok = true;
    for (int i = 0; a_arr[i]; i++) {
        if (strcmp(only_index, ixstr4idx(i)) != 0 && strcmp(only_index, "all") != 0) {
            continue;
        }
        for (int j = 0; b_arr[j]; j++) {
            if (strcmp(only_index, ixstr4idx(j)) != 0 && strcmp(only_index, "all") != 0) {
                continue;
            }
            struct tg_geom *a = a_arr[i];
            struct tg_geom *b = b_arr[j];
            char relate_ab0[64] = { 0 };
            char relate_ba0[64] = { 0 };
            bool ab0, ba0;
            bool relate = false;
            if (0) {
#ifdef ENABLE_RELATE
            } else if (strcmp(predicate, "relate") == 0) {
                relate = true;
                if (relate_ab && relate_ab[0]){
                    tg_geom_relate(a, b, relate_ab0, sizeof(relate_ab0));
                }
                if (relate_ba && relate_ba[0]){
                    tg_geom_relate(b, a, relate_ba0, sizeof(relate_ba0));
                }
#endif
#ifdef ENABLE_EQUALS
            } else if (strcmp(predicate, "equals") == 0) {
                ab0 = tg_geom_equals(a, b);
                ba0 = tg_geom_equals(b, a);
#endif
#ifdef ENABLE_INTERSECTS
            } else if (strcmp(predicate, "intersects") == 0) {
                ab0 = tg_geom_intersects(a, b);
                ba0 = tg_geom_intersects(b, a);
#endif
#ifdef ENABLE_DISJOINT
            } else if (strcmp(predicate, "disjoint") == 0) {
                ab0 = tg_geom_disjoint(a, b);
                ba0 = tg_geom_disjoint(b, a);
#endif
#ifdef ENABLE_CONTAINS
            } else if (strcmp(predicate, "contains") == 0) {
                ab0 = tg_geom_contains(a, b);
                ba0 = tg_geom_contains(b, a);
#endif
#ifdef ENABLE_WITHIN
            } else if (strcmp(predicate, "within") == 0) {
                ab0 = tg_geom_within(a, b);
                ba0 = tg_geom_within(b, a);
#endif
#ifdef ENABLE_COVERS
            } else if (strcmp(predicate, "covers") == 0) {
                ab0 = tg_geom_covers(a, b);
                ba0 = tg_geom_covers(b, a);
#endif
#ifdef ENABLE_COVEREDBY
            } else if (strcmp(predicate, "coveredby") == 0) {
                ab0 = tg_geom_coveredby(a, b);
                ba0 = tg_geom_coveredby(b, a);
#endif
#ifdef ENABLE_CROSSES
            } else if (strcmp(predicate, "crosses") == 0) {
                ab0 = tg_geom_crosses(a, b);
                ba0 = tg_geom_crosses(b, a);
#endif
#ifdef ENABLE_OVERLAPS
            } else if (strcmp(predicate, "overlaps") == 0) {
                ab0 = tg_geom_overlaps(a, b);
                ba0 = tg_geom_overlaps(b, a);
#endif
#ifdef ENABLE_TOUCHES
            } else if (strcmp(predicate, "touches") == 0) {
                ab0 = tg_geom_touches(a, b);
                ba0 = tg_geom_touches(b, a);
#endif
            } else {
                // skip unknown predicates
                continue;
            }
            if (!relate && (ab0 != ab || ba0 != ba)) {
                int line = json_get_line_number(org_json, org_len, json);
                fprintf(stderr, "%s:%d '%s' "
                    "[\"%s\",\"%s\"] != [\"%s\",\"%s\"] "
                    "(ix:%s/%s)\n", 
                        path, line, predicate, 
                        ab0?"T":"F", ba0?"T":"F",
                        ab?"T":"F", ba?"T":"F",
                    ixstr4idx(i), ixstr4idx(j));
                ok = false;
            } else if (relate) {
                if (((relate_ab && relate_ab[0]) && strcmp(relate_ab, relate_ab0) != 0) || 
                    ((relate_ba && relate_ba[0]) && strcmp(relate_ba, relate_ba0) != 0))
                {
                    int line = json_get_line_number(org_json, org_len, json);
                    fprintf(stderr, "%s:%d '%s' "
                        "[\"%s\",\"%s\"] != [\"%s\",\"%s\"] "
                        "(aix:%s, bix:%s)\n",
                        path, line, "relate",
                        relate_ab0, relate_ba0,
                        relate_ab, relate_ba,
                        ixstr4idx(i), ixstr4idx(j));
                }
            }
        }
    }
    return ok;
}

static bool test_case(const char *path, const char *org_json, 
    size_t org_len, struct json json)
{
    bool ok = true;

    struct json geoms = json_object_get(json, "geoms");
    struct json ga = json_array_get(geoms, 0);
    struct json gb = json_array_get(geoms, 1);
    struct json relate = json_object_get(json, "relate");
    struct json ab = json_array_get(relate, 0);
    struct json ba = json_array_get(relate, 1);
    struct json dims = json_object_get(json, "dims");
    struct json da = json_array_get(dims, 0);
    struct json db = json_array_get(dims, 1);
    struct json pvals = json_first(json_object_get(json, "predicates"));

    struct tg_geom *a_noix = geom_from_json(path, org_json, org_len, ga, TG_NONE);
    struct tg_geom *a_ixn = geom_from_json(path, org_json, org_len, ga, TG_NATURAL);
    struct tg_geom *a_ixy = geom_from_json(path, org_json, org_len, ga, TG_YSTRIPES);
    struct tg_geom *b_noix = geom_from_json(path, org_json, org_len, gb, TG_NONE);
    struct tg_geom *b_ixn = geom_from_json(path, org_json, org_len, gb, TG_NATURAL);
    struct tg_geom *b_ixy = geom_from_json(path, org_json, org_len, gb, TG_YSTRIPES);

    struct tg_geom *a_arr[] = { a_noix, a_ixn, a_ixy, NULL };
    struct tg_geom *b_arr[] = { b_noix, b_ixn, b_ixy, NULL };

    while (json_exists(pvals)) {
        int line = 0;
        struct json key = pvals;
        struct json val = json_next(key);
        char predicate[64];
        json_string_copy(key, predicate, sizeof(predicate));
        bool abval;
        if (json_string_compare(json_array_get(val, 0), "T") == 0) {
            abval = true;
        } else if (json_string_compare(json_array_get(val, 0), "F") == 0) {
            abval = false;
        } else {
            fprintf(stderr, "%s:%d: invalid predicate: '%s'\n",
                path, line, predicate);
            abort();
        }
        bool baval;
        if (json_string_compare(json_array_get(val, 1), "T") == 0) {
            baval = true;
        } else if (json_string_compare(json_array_get(val, 1), "F") == 0) {
            baval = false;
        } else {
            fprintf(stderr, "%s:%d: invalid predicate: '%s'\n",
                path, line, predicate);
            abort();
        }
        if (!test_case_predicate(path, org_json, org_len, key, 
            predicate, a_arr, b_arr,  abval, baval, NULL, NULL))
        {
            ok = false;
        }
        pvals = json_next(val);
    }

    if (json_exists(ab) || json_exists(ba)) {
        char ab_str[64];
        json_string_copy(ab, ab_str, sizeof(ab_str));
        char ba_str[64];
        json_string_copy(ba, ba_str, sizeof(ba_str));
        if (!test_case_predicate(path, org_json, org_len, relate, 
            "relate", a_arr, b_arr, false, false, ab_str, ba_str))
        {
            ok = false;
        }
    }

    
    if (json_exists(da) || json_exists(da)) {
        int da0 = tg_geom_de9im_dims(a_noix);
        int db0 = tg_geom_de9im_dims(b_noix);
        if (da0 != json_int(da) || db0 != json_int(db)) {
            int line = json_get_line_number(org_json, org_len, dims);
            fprintf(stderr, "%s:%d '%s' [%d,%d] != [%d,%d]\n",
                path, line, "dims", da0, db0, json_int(da), json_int(db));
        }
    }
    tg_geom_free(a_noix);
    tg_geom_free(a_ixy);
    tg_geom_free(a_ixn);
    tg_geom_free(b_noix);
    tg_geom_free(b_ixy);
    tg_geom_free(b_ixn);
    return ok;
}

static bool test_relation_file(struct relation *rel) {
    bool ok = true;
    char *jsrc = malloc(strlen(rel->data)+1);
    assert(jsrc);
    strcpy(jsrc, rel->data);
    json_strip_jsonc(jsrc, strlen(jsrc));
    if (!json_valid(jsrc)) {
        fprintf(stderr, "invalid json: %s\n", rel->name);
        abort();
    }
    struct json json = json_parse(jsrc);
    json = json_first(json);
    while (json_exists(json)) {
        if (!test_case(rel->name, jsrc, strlen(jsrc), json)) {
            ok = false;
        }
        json = json_next(json);
    }
    free(jsrc);
    return ok;
}

void test_relations_cases(void) {
    bool ok = true;
    int nrelations = sizeof(relations) / sizeof(struct relation);
    for (int i = 0; i < nrelations; i++) {
        struct relation *rel = &relations[i];
        if (strstr(rel->name, only_file) || strcmp(only_file, "all") == 0)
        {
            if (!test_relation_file(rel)) {
                ok = false;
            }
        }
    }
    if (!ok) {
        exit(1);
    }
}

void simple_test(const char *awkt, const char *bwkt, 
    bool equals, 
    bool intersects, 
    bool disjoint, 
    bool contains, 
    bool within, 
    bool covers,
    bool coveredby,
    bool crosses,
    bool overlaps,
    bool touches
) {
    struct tg_geom *a = tg_parse_wkt(awkt);
    if (tg_geom_error(a)) {
        fprintf(stderr, "awkt: %s\n", tg_geom_error(a));
        assert(0);
    }
    struct tg_geom *b = tg_parse_wkt(bwkt);
    if (tg_geom_error(b)) {
        fprintf(stderr, "awkt: %s\n", tg_geom_error(b));
        assert(0);
    }
    bool ok = true;
    if (tg_geom_equals(a, b) != equals) {
        fprintf(stderr, "equals: %s != %s\n", !equals?"T":"F", equals?"T":"F");
        ok = false;
    }
    if (tg_geom_intersects(a, b) != intersects) {
        fprintf(stderr, "intersects: %s != %s\n", !intersects?"T":"F", intersects?"T":"F");
        ok = false;
    }
    if (tg_geom_disjoint(a, b) != disjoint) {
        fprintf(stderr, "disjoint: %s != %s\n", !disjoint?"T":"F", disjoint?"T":"F");
        ok = false;
    }
    if (tg_geom_contains(a, b) != contains) {
        fprintf(stderr, "contains: %s != %s\n", !contains?"T":"F", contains?"T":"F");
        ok = false;
    }
    if (tg_geom_within(a, b) != within) {
        fprintf(stderr, "within: %s != %s\n", !within?"T":"F", within?"T":"F");
        ok = false;
    }
    if (tg_geom_covers(a, b) != covers) {
        fprintf(stderr, "covers: %s != %s\n", !covers?"T":"F", covers?"T":"F");
        ok = false;
    }
    if (tg_geom_coveredby(a, b) != coveredby) {
        fprintf(stderr, "coveredby: %s != %s\n", !coveredby?"T":"F", coveredby?"T":"F");
        ok = false;
    }
    if (tg_geom_crosses(a, b) != crosses) {
        fprintf(stderr, "crosses: %s != %s\n", !crosses?"T":"F", crosses?"T":"F");
        ok = false;
    }
    if (tg_geom_overlaps(a, b) != overlaps) {
        fprintf(stderr, "overlaps: %s != %s\n", !overlaps?"T":"F", overlaps?"T":"F");
        ok = false;
    }
    if (tg_geom_touches(a, b) != touches) {
        fprintf(stderr, "touches: %s != %s\n", !touches?"T":"F", touches?"T":"F");
        ok = false;
    }
    if (!ok) {
        abort();
    }
    tg_geom_free(a);
    tg_geom_free(b);
}

void test_relations_unsupported(void) {
    // Telations that are unsupported, but available. 
    // These always return false, until true supported is added.
    assert(tg_geom_overlaps(
        (struct tg_geom*)RING(P(1,1),P(2,1),P(2,2),P(1,2),P(1,1)), 
        (struct tg_geom*)RING(P(1.5,1.5),P(2.5,1.5),P(2.5,2.5),P(1.5,2.5),P(1.5,1.5))
    ) == 0);
    assert(tg_geom_crosses(
        (struct tg_geom*)LINE(P(1,1),P(2,2)), 
        (struct tg_geom*)LINE(P(1,2),P(2,1))
    ) == 0);
}

void test_relations_various(void) {
    assert(tg_segment_covers_point(S(1.31,1.88,1.65,1.8), P(1.65,1.8)));
    assert(tg_segment_covers_point(S(1.31,1.88,1.65,1.8), P(1.31,1.88)));
    assert(tg_segment_covers_segment(S(1.31,1.88,1.65,1.8), S(1.31,1.88,1.65,1.8)));
// return;
    assert(tg_geom_touches(0, 0) == 0);
    assert(tg_geom_contains(0, 0) == 0);
    assert(tg_geom_de9im_dims(0) == -1);

    assert(tg_poly_contains_geom(0, 0) == 0);
    assert(tg_line_contains_geom(0, 0) == 0);
    assert(tg_point_contains_geom(P(0,0), 0) == 0);

    assert(tg_poly_touches_geom(0, 0) == 0);
    assert(tg_line_touches_geom(0, 0) == 0);
    assert(tg_point_touches_geom(P(0, 0), 0) == 0);

    struct tg_geom *geom = tg_parse_wkt("MULTILINESTRING ((1.1 1.39, 1.31 1.88, 1.65 1.8, 1.58 1.66, 1.9 1.7), (1.18 1.26, 1.32 1.79, 1.68 1.48, 2.16 1.73, 2.01 1.89, 1.71 1.73, 1.68 1.88, 1.35 1.96))");
    assert(tg_geom_equals(geom, geom));
    assert(geom);
    tg_geom_free(geom);
}

int main(int argc, char **argv) {
    // return 0;
    // $ tests/run.sh test_relation [only_file] [only_index]
    // example:
    // $ tests/run.sh test_relation tmp.jsonc
    // $ tests/run.sh test_relation tmp.jsonc natural
    // $ tests/run.sh test_relation all natural
    if (argc > 2) {
        only_file = argv[2];
    }
    if (argc > 3) {
        only_index = argv[3];
    }

    do_test(test_relations_cases);
    do_test(test_relations_unsupported);
    do_test(test_relations_various);
    return 0;
}
