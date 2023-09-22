/*
A simple utility for getting a "relate" result in JSON using the GEOS library.

$ cc -O3 -Ilibgeos/build/install/include -o relate relate.c \
     libgeos/build/install/lib/libgeos_c.a \
     libgeos/build/install/lib/libgeos.a -lm -pthread -lstdc++
$ ./relate <geom-a> <geom-b>
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <geos_c.h>

GEOSContextHandle_t handle;

static void geos_error(const char *message, void *userdata) {
    fprintf(stderr, "error: %s\n", message);
}

char predicate0(GEOSGeometry *a, GEOSGeometry *b, const char *name) {
    int res;
    if (strcmp(name, "equals") == 0) {
        res = GEOSEquals_r(handle, a, b);
    } else if (strcmp(name, "intersects") == 0) {
        res = GEOSIntersects_r(handle, a, b);
    } else if (strcmp(name, "disjoint") == 0) {
        res = GEOSDisjoint_r(handle, a, b);
    } else if (strcmp(name, "contains") == 0) {
        res = GEOSContains_r(handle, a, b);
    } else if (strcmp(name, "within") == 0) {
        res = GEOSWithin_r(handle, a, b);
    } else if (strcmp(name, "covers") == 0) {
        res = GEOSCovers_r(handle, a, b);
    } else if (strcmp(name, "coveredby") == 0) {
        res = GEOSCovers_r(handle, b, a);
    } else if (strcmp(name, "crosses") == 0) {
        res = GEOSCrosses_r(handle, a, b);
    } else if (strcmp(name, "overlaps") == 0) {
        res = GEOSOverlaps_r(handle, a, b);
    } else if (strcmp(name, "touches") == 0) {
        res = GEOSTouches_r(handle, a, b);
    } else {
        abort();
    }
    if (res == 0) {
        return 'F';
    }
    if (res == 1) {
        return 'T';
    }
    abort();
}

char *predicate(GEOSGeometry *a, GEOSGeometry *b, const char *name) {
    char *out = malloc(128);
    if (!out) abort();
    char t1 = predicate0(a, b, name);
    char t2 = predicate0(b, a, name);
    snprintf(out, 128, "\"%s\": %.*s[\"%c\", \"%c\"]", name, 
        (int)(10-strlen(name)), "          ", t1, t2);
    return out;
}

bool isjson(const char *str) {
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

GEOSGeometry *read_json(const char *json) {
    GEOSGeoJSONReader *reader = GEOSGeoJSONReader_create_r(handle);
    GEOSGeometry *geom = GEOSGeoJSONReader_readGeometry_r(handle, reader, json);
    return geom;
}

GEOSGeometry *read_wkt(const char *wkt) {
    GEOSWKTReader *reader = GEOSWKTReader_create_r(handle);
    GEOSGeometry *geom = GEOSWKTReader_read_r(handle, reader, wkt);
    return geom;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: relate <geom-a> <geom-b>\n");
        exit(1);
    }
    handle = GEOS_init_r();

    GEOSContext_setErrorMessageHandler_r(handle, geos_error, 0);
    bool ajson = isjson(argv[1]);
    bool bjson = isjson(argv[2]);
    GEOSGeometry *a = ajson ? read_json(argv[1]) : read_wkt(argv[1]);
    GEOSGeometry *b = bjson ? read_json(argv[2]) : read_wkt(argv[2]);
    if (!a || !b) exit(1);
    char *ab = GEOSRelate_r(handle, a, b);
    char *ba = GEOSRelate_r(handle, b, a);
    if (!ab || !ba) abort();

    int da = GEOSGeom_getDimensions(a);
    int db = GEOSGeom_getDimensions(b);

    printf("{\n");
    printf("  \"geoms\": [\n");
    if (ajson) {
        printf("    %s,\n",argv[1]);
    } else {
        printf("    \"%s\",\n",argv[1]);
    }
    if (bjson) {
        printf("    %s\n", argv[2]);
    } else {
        printf("    \"%s\"\n",argv[2]);
    }
    printf("  ],\n");
    printf("  \"dims\": [ %d, %d ],\n", da, db);
    printf("  \"relate\": [ \"%s\", \"%s\" ],\n", ab, ba);
    printf("  \"predicates\": {\n");
    printf("    %s,\n", predicate(a, b, "equals"));
    printf("    %s,\n", predicate(a, b, "intersects"));
    printf("    %s,\n", predicate(a, b, "disjoint"));
    printf("    %s,\n", predicate(a, b, "contains"));
    printf("    %s,\n", predicate(a, b, "within"));
    printf("    %s,\n", predicate(a, b, "covers"));
    printf("    %s,\n", predicate(a, b, "coveredby"));
    printf("    %s,\n", predicate(a, b, "crosses"));
    printf("    %s,\n", predicate(a, b, "overlaps"));
    printf("    %s\n",  predicate(a, b, "touches"));
    printf("  }\n");
    printf("}\n");
}
