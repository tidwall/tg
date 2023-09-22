#ifdef GEOS_BENCH
#include <geos_c.h>
#endif
#include "tests.h"

size_t bmalloc_heap_size();
size_t bmalloc_num_allocs();
void *bmalloc(size_t size);
void bfree(void *ptr);
void *brealloc(void *ptr, size_t size);

#define MBR_PAD            0.10  // 0.10 = 10%
#define NUM_RAND_POINTS    10000  // number of random points, Point-in-polygon
#define NUM_RAND_SEGMENTS  10000  // number of random segments, Line intersect

bool markdown = false;

void print_start_bold() {
    printf("\n");
    if (markdown) {
        printf("<b>");
    } else {
        printf("\e[1m");
    }
}

void print_end_bold() {
    if (markdown) {
        printf("</b>");
    } else {
        printf("\e[0m");
    }
    printf("\n");
}

static int64_t *cinv = NULL;
static int64_t xval = 0;
static void invalidate_cache() {
    const int size = 64*1024*1024;
    if (!cinv) {
        xval = mkrandseed();
        cinv = malloc(size);
        assert(cinv);
    }
    for (int j = 0; j < size; j += 8) {
        cinv[j/8] = xval;
        xval++;
    }
    int64_t total = mkrandseed();
    for (int j = 0; j < size; j += 8) {
        total *= cinv[j/8];
    }
    xval = total;
}

static void bench_step_tg(
    enum tg_index opts,
    const struct tg_point points[], 
    int npoints,
    struct tg_point *rpoints,
    struct tg_segment *rsegs,
    int N,
    double *create_secs_out,
    int *memsize_out,
    double *bench_secs_out,
    int *hits_out
) {
    struct tg_line **rlines = NULL;
    if (rsegs) {
        rlines = malloc(N*sizeof(struct tg_line*));
        assert(rlines);
        for (int i = 0; i < N; i++) {
            struct tg_point lpts[2] = { rsegs[i].a, rsegs[i].b };
            rlines[i] = tg_line_new(lpts, 2);
        }
    }

    double start = clock_now();
    size_t heap_start = bmalloc_heap_size();
    struct tg_ring *ring = tg_ring_new_ix(points, npoints, opts);
    assert(ring);
    struct tg_poly *poly = tg_poly_new(ring, NULL, 0);
    assert(poly);
    size_t heap_end = bmalloc_heap_size();
    *create_secs_out = clock_now()-start;
    *memsize_out = heap_end-heap_start; 
    // *memsize_out = tg_poly_memsize(poly);
    char label[64];
    int hits = 0;
    start = clock_now();
    for (int i = 0; i < N; i++) {
        if (rpoints) {
            if (tg_poly_intersects_point(poly, rpoints[i])) {
                hits++;
            }
        } else {
            if (tg_poly_intersects_line(poly, rlines[i])) {
                hits++;
            }
        }
    }
    *bench_secs_out = clock_now()-start;
    *hits_out = hits;
    tg_poly_free(poly);
    tg_ring_free(ring);
    if (rlines) {
        for (int i = 0; i < N; i++) {
            tg_line_free(rlines[i]);
        }
        free(rlines);
    }
}

#ifdef GEOS_BENCH
static void bench_step_geos(
    bool prepared,
    const struct tg_point points[], 
    int npoints,
    struct tg_point *rpoints,
    struct tg_segment *rsegs,
    int N,
    double *create_secs_out,
    int *memsize_out,
    double *bench_secs_out,
    int *hits_out
) {

    GEOSContextHandle_t handle = GEOS_init_r();

    // convers rand tg points into geos points
    GEOSGeometry **rgeoms = malloc(N*sizeof(GEOSGeometry *));
    assert(rgeoms);
    memset(rgeoms, 0, N*sizeof(GEOSGeometry *));
    if (rpoints) {
        rgeoms = malloc(N*sizeof(GEOSGeometry *));
        for (int i = 0; i < N; i++) {
            rgeoms[i] = GEOSGeom_createPointFromXY_r(handle, rpoints[i].x, rpoints[i].y);
            assert(rgeoms[i]);
        }
    } else if (rsegs) {
        for (int i = 0; i < N; i++) {
            GEOSCoordSequence *lseq = GEOSCoordSeq_create_r(handle, 2, 2);
            assert(lseq);
            GEOSCoordSeq_setXY_r(handle, lseq, 0, rsegs[i].a.x, rsegs[i].a.y); 
            GEOSCoordSeq_setXY_r(handle, lseq, 1, rsegs[i].b.x, rsegs[i].b.y);
            rgeoms[i] = GEOSGeom_createLineString_r(handle, lseq);
            assert(rgeoms[i]);
        }
    }

    // create the geos polygon
    
    double start = clock_now();
    size_t heap_start = bmalloc_heap_size();
    GEOSCoordSequence *seq = GEOSCoordSeq_copyFromBuffer_r(handle, 
        (double*)points, npoints, 0, 0);
    assert(seq);
    GEOSGeometry *geos_ring = GEOSGeom_createLinearRing_r(handle, seq);
    assert(geos_ring);
    GEOSGeometry *geos_poly = GEOSGeom_createPolygon_r(handle, geos_ring, NULL, 0);

    // double area = 0;
    // double perim = 0;
    // GEOSArea_r(handle, geos_poly, &area);
    // GEOSLength_r(handle, geos_poly, &perim);
    // double complexity = (area * M_PI * 4) / (perim * perim);
    // printf("=============================\n");
    // printf(">> B area:       %f\n", area);
    // printf(">> B perimeter:  %f\n", perim);
    // printf(">> B complexity: %f\n", complexity);
    // printf("=============================\n");

    const GEOSPreparedGeometry *prep_poly;
    if (prepared) {
        prep_poly = GEOSPrepare_r(handle, geos_poly);
        assert(prep_poly);

        double xmin, xmax, ymin, ymax;
        assert(GEOSGeom_getXMin_r(handle, geos_poly, &xmin));
        assert(GEOSGeom_getYMin_r(handle, geos_poly, &ymin));
        assert(GEOSGeom_getXMax_r(handle, geos_poly, &xmax));
        assert(GEOSGeom_getYMax_r(handle, geos_poly, &ymax));
        double x = (xmin+xmax)/2;
        double y = (ymin+ymax)/2;


        // It appears that the indexing for a prepared GEOS geometry is not
        // ready immediately after the GEOSPrepare_r call. It seems to take
        // a couple of runs for it to kick in.
        // Let's do two point-in-polygon operations on the MBR center point.
        assert(GEOSPreparedIntersectsXY_r(handle, prep_poly, x, y));
        assert(GEOSPreparedIntersectsXY_r(handle, prep_poly, x, y));
    }
    size_t heap_end = bmalloc_heap_size();

    *create_secs_out = clock_now()-start;
    *memsize_out = heap_end-heap_start;
    // do benchmarking
    int hits = 0;
    start = clock_now();
    if (prepared) {
        for (int i = 0; i < N; i++) {
            if (GEOSPreparedIntersects_r(handle, prep_poly, rgeoms[i])) {
                hits++;
            }
        }
    } else {
         for (int i = 0; i < N; i++) {
            if (GEOSIntersects_r(handle, geos_poly, rgeoms[i])) {
                hits++;
            }
         }
    }

    *bench_secs_out = clock_now()-start;
    *hits_out = hits;

    // cleanup
    if (prepared) {
        GEOSPreparedGeom_destroy_r(handle, prep_poly);
    }
    GEOSGeom_destroy_r(handle, geos_poly);
    
    for (int i = 0; i < N; i++) {
        GEOSGeom_destroy_r(handle, rgeoms[i]);
    }
    free(rgeoms);
    GEOS_finish_r(handle);
}
#endif

#define HEADER_FIELDS "%11s %8s %7s %5s %11s %10s"
#define ROW_FIELDS    "%11s %8.0f %7d %5d %8.2f µs %10s"
#define HEADER_FIELDS_ENCDEC "%11s %8s %7s"
#define ROW_FIELDS_ENCDEC    "%11s %8.0f %7.0f"

void print_header(const char *name) {
    print_start_bold();
    printf("%-15s " HEADER_FIELDS, name, "ops/sec", "ns/op", "points", "hits", "built", "bytes");
    print_end_bold();
}

void print_header_io(const char *name) {
    print_start_bold();
    printf("%-15s " HEADER_FIELDS_ENCDEC, name, "ops/sec", "ns/op", "MB/sec");
    print_end_bold();
}

void bench_run(int runs, const char *name, char *libname, char *ixname,
    const struct tg_point points[], int npoints,
    struct tg_point *rpoints, struct tg_segment *rsegs, int N
    )
{
    enum tg_index opts = { 0 };
    if (strcmp(libname, "tg") == 0) {
        if (strcmp(ixname, "none") == 0) {
            opts = TG_NONE;
        } else if (strcmp(ixname, "natural") == 0) {
            opts = TG_NATURAL;
        } else if (strcmp(ixname, "ystripes") == 0) {
            opts = TG_YSTRIPES;
        }
    }

    double create_secs_sum = 0;
    double create_secs = 0;
    double bench_secs = 0;
    int memsize = 0;
    int hits = 0;

    char label[64];
    // snprintf(label, sizeof(label), "%s/%s/%s", name, libname, ixname);
    snprintf(label, sizeof(label), "%s/%s", libname, ixname);
    for (int i = 0; i < runs; i++) {
        invalidate_cache();
        if (!markdown) {
            printf("\r%-21s %2d/%d ", label, i+1, runs); 
        }
        fflush(stdout);
        double create_secs0 = 0;
        double bench_secs0 = 0;
        int memsize0 = 0;
        int hits0 = 0;
    #ifdef GEOS_BENCH
        if (strcmp(libname, "geos") == 0) {
            bool prepared = strcmp(ixname, "prepared") == 0;
            bench_step_geos(prepared, points, npoints, rpoints, rsegs, N, 
                &create_secs0, &memsize0, &bench_secs0, &hits0);
        } else
    #endif
        {
            bench_step_tg(opts, points, npoints, rpoints, rsegs, N, 
                &create_secs0, &memsize0, &bench_secs0, &hits0);
        }
        create_secs_sum += create_secs0;
        if (i == 0) {
            create_secs = create_secs0;
            bench_secs = bench_secs0;
            memsize = memsize0;
            hits = hits0;
        } else {
            if (create_secs0 < create_secs && create_secs0 > 0.0) {
                create_secs = create_secs0;
            }
            if (bench_secs0 < bench_secs) {
                bench_secs = bench_secs0;
            }
            assert(memsize0 == memsize);
            assert(hits0 == hits);
        }
    }
    if (create_secs*1e6 < 1.0) {
        create_secs = create_secs_sum / (double)runs;
    }
    if (!markdown) {
        printf("\r");
    }
    printf("%-15s ", label); 
    printf(ROW_FIELDS,
        commaize(N/bench_secs), bench_secs/N*1e9, 
        npoints, hits, create_secs*1e6, commaize(memsize));
    printf("\n");
}

struct tg_rect rect_from_points(const struct tg_point points[], int npoints) {
    struct tg_rect rect = { 0 };
    for (int i = 0; i < npoints; i++) {
        if (i == 0) {
            rect = tg_point_rect(points[i]);
        } else {
            if (points[i].x < rect.min.x) rect.min.x = points[i].x;
            else if (points[i].x > rect.max.x) rect.max.x = points[i].x;
            if (points[i].y < rect.min.y) rect.min.y = points[i].y;
            else if (points[i].y > rect.max.y) rect.max.y = points[i].y;
        }
    }
    return rect;
}

struct tg_point *make_random_points(struct tg_rect rect, int npoints) {
    struct tg_point *rpoints = malloc(npoints*sizeof(struct tg_point));
    assert(rpoints);
    
    double w = rect.max.x - rect.min.x;
    double h = rect.max.y - rect.min.y;

    double perc = MBR_PAD; // percent

    rect.min.x -= w * perc;
    rect.min.y -= h * perc;
    rect.max.x += w * perc;
    rect.max.y += h * perc;

    for (int i = 0; i < npoints; i++) {
        rpoints[i] = rand_point(rect);
    }
    return rpoints;
}

struct tg_segment *make_random_segments(struct tg_rect rect, int nsegs) {
    struct tg_segment *rand_segs = malloc(nsegs*sizeof(struct tg_segment));
    assert(rand_segs);
    for (int i = 0; i < nsegs; i++) {
        rand_segs[i] = rand_segment(rect);
    }
    return rand_segs;
}

void pip_bench(const char *name, int runs, const struct tg_point points[], int npoints) {
    int N = NUM_RAND_POINTS;
 
    // create a rect from the incoming points.
    struct tg_rect rect = rect_from_points(points, npoints);

    // create random points that intersect the rect.
    struct tg_point *rpoints = make_random_points(rect, N);

    // perform benchmarks
    bench_run(runs, name, "tg", "none", points, npoints, rpoints, 0, N);
    bench_run(runs, name, "tg", "natural", points, npoints, rpoints, 0, N);
    bench_run(runs, name, "tg", "ystripes", points, npoints, rpoints, 0, N);
#ifdef GEOS_BENCH
    bench_run(runs, name, "geos", "none", points, npoints, rpoints, 0, N);
    bench_run(runs, name, "geos", "prepared", points, npoints, rpoints, 0, N);
#endif
    free(rpoints);
}

void intersects_bench(const char *name, int runs, const struct tg_point points[], int npoints) {
    int N = NUM_RAND_SEGMENTS;

    // create a rect from the incoming points.
    struct tg_rect rect = rect_from_points(points, npoints);

    // create random segments that are conatined in or intersecting the rect.
    struct tg_segment *rsegs = make_random_segments(rect, N);

    bench_run(runs, name, "tg", "none", points, npoints, 0, rsegs, N);
    bench_run(runs, name, "tg", "natural", points, npoints, 0, rsegs, N);
    bench_run(runs, name, "tg", "ystripes", points, npoints, 0, rsegs, N);
#ifdef GEOS_BENCH
    bench_run(runs, name, "geos", "none", points, npoints, 0, rsegs, N);
    bench_run(runs, name, "geos", "prepared", points, npoints, 0, rsegs, N);
#endif
    free(rsegs);
}

void test_pip_bench_arizona(int runs) {
    struct tg_point points[] = {az};
    pip_bench("az", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_pip_bench_texas(int runs) {
    struct tg_point points[] = {tx};
    pip_bench("tx", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_pip_bench_brazil(int runs) {
    struct tg_geom *geom = load_geom("br", TG_NONE);
    const struct tg_ring *ring = tg_poly_exterior(tg_geom_poly(geom));
    const struct tg_point *points = tg_ring_points(ring);
    int npoints = tg_ring_num_points(ring);
    pip_bench("br", runs, points, npoints);
    tg_geom_free(geom);
}

void test_pip_bench_british_columbia(int runs) {
    struct tg_geom *geom = load_geom("bc", TG_NONE);
    const struct tg_ring *ring = tg_poly_exterior(tg_geom_poly(geom));
    const struct tg_point *points = tg_ring_points(ring);
    int npoints = tg_ring_num_points(ring);
    pip_bench("bc", runs, points, npoints);
    tg_geom_free(geom);
}

void test_pip_bench_rhode_island(int runs) {
    struct tg_point points[] = {ri};
    pip_bench("ri", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_pip_bench_rand_polygon(int runs, int npoints) {
    double s = 0.7;
    double c = 0.7;
    double x = -111.9575;
    double y = 33.4329;
    double r = 0.002;
    struct tg_point *points = (void*)make_polygon(npoints, x, y, r, c, s);
    assert(points);
    pip_bench("rp", runs, points, npoints);
    free(points);
}

void test_pip_bench_circle(int runs, int npoints) {
    struct tg_ring *ring = tg_circle_new_ix((struct tg_point) { -112, 33 }, 5, npoints, TG_NONE);
    assert(ring);
    const struct tg_point *points = tg_ring_points(ring);
    npoints = tg_ring_num_points(ring);
    pip_bench("cp", runs, points, npoints);
    tg_ring_free(ring);
}


void test_intersects_bench_circle(int runs, int npoints) {
    struct tg_ring *ring = tg_circle_new_ix((struct tg_point) { -112, 33 }, 5, npoints, TG_NONE);
    assert(ring);
    const struct tg_point *points = tg_ring_points(ring);
    npoints = tg_ring_num_points(ring);
    intersects_bench("cp", runs, points, npoints);
    tg_ring_free(ring);
}


void test_intersects_bench_brazil(int runs) {
    struct tg_geom *geom = load_geom("br", TG_NONE);
    const struct tg_ring *ring = tg_poly_exterior(tg_geom_poly(geom));
    const struct tg_point *points = tg_ring_points(ring);
    int npoints = tg_ring_num_points(ring);
    intersects_bench("br", runs, points, npoints);
    tg_geom_free(geom);
}

void test_intersects_bench_british_columbia(int runs) {
    struct tg_geom *geom = load_geom("bc", TG_NONE);
    const struct tg_ring *ring = tg_poly_exterior(tg_geom_poly(geom));
    const struct tg_point *points = tg_ring_points(ring);
    int npoints = tg_ring_num_points(ring);
    intersects_bench("bc", runs, points, npoints);
    tg_geom_free(geom);
}

void test_intersects_bench_arizona(int runs) {
    struct tg_point points[] = {az};
    intersects_bench("az", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_intersects_bench_texas(int runs) {
    struct tg_point points[] = {tx};
    intersects_bench("tx", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_intersects_bench_rhode_island(int runs) {
    struct tg_point points[] = {ri};
    intersects_bench("ri", runs, points, sizeof(points)/sizeof(struct tg_point));
}

void test_intersects_bench_rand_polygon(int runs, int npoints) {
    struct tg_point *points = make_rand_polygon(npoints);
    pip_bench("rp", runs, points, npoints);
}

void main_pip_bench(uint64_t seed, int runs, bool simple) {
    srand(seed);
    print_start_bold();
    printf("== Point-in-polygon ==");
    print_end_bold();
    printf("Benchmark point-in-polygon operation for %dK random points that\n"
           "are each within the polygon's MBR with an extra %.0f%% padding.\n",
           NUM_RAND_POINTS/1000, MBR_PAD*100.0);
    printf("Performs %d run%s and chooses the best results.\n", runs, runs!=0?"s":"");
    print_header("Brazil");
    test_pip_bench_brazil(runs);
    print_header("Texas");
    test_pip_bench_texas(runs);
    print_header("Arizona");
    test_pip_bench_arizona(runs);
    print_header("Br Columbia");
    test_pip_bench_british_columbia(runs);
    print_header("Rhode Island");
    test_pip_bench_rhode_island(runs);

    if (simple) return;

    // print_header("Circle-10");
    // test_pip_bench_circle(runs, 10);
    print_header("Circle-50");
    test_pip_bench_circle(runs, 50);
    print_header("Circle-100");
    test_pip_bench_circle(runs, 100);
    print_header("Circle-1000");
    test_pip_bench_circle(runs, 1000);
    print_header("Circle-10000");
    test_pip_bench_circle(runs, 10000);

    // print_header("Random-10");
    // test_pip_bench_rand_polygon(runs, 10);
    print_header("Random-50");
    test_pip_bench_rand_polygon(runs, 50);
    print_header("Random-100");
    test_pip_bench_rand_polygon(runs, 100);
    print_header("Random-1000");
    test_pip_bench_rand_polygon(runs, 1000);
    print_header("Random-10000");
    test_pip_bench_rand_polygon(runs, 10000);
}

void main_intersects_bench(uint64_t seed, int runs) {
    srand(seed); 
    print_start_bold();
    printf("== Line intersect ==");
    print_end_bold();
    printf("Benchmark line interecting polygon operation for 10K random line\n"
           "segments that are each no larger than %d%% the width of the polygon\n"
           "and are within the polygon's MBR plus an extra %.0f%% padding.\n",
           NUM_RAND_SEGMENTS/1000, MBR_PAD*100.0);
    printf("Performs %d run%s and chooses the best results.\n", runs, runs!=0?"s":"");
    print_header("Brazil");
    test_intersects_bench_brazil(runs);
    print_header("Texas");
    test_intersects_bench_texas(runs);
    print_header("Arizona");
    test_intersects_bench_arizona(runs);
    print_header("Br Columbia");
    test_intersects_bench_british_columbia(runs);
    print_header("Rhode Island");
    test_intersects_bench_rhode_island(runs);

    print_header("Circle-100");
    test_intersects_bench_circle(runs, 100);
    print_header("Circle-1000");
    test_intersects_bench_circle(runs, 1000);
    print_header("Circle-10000");
    test_intersects_bench_circle(runs, 10000);

    print_header("Random-100");
    test_intersects_bench_rand_polygon(runs, 100);
    print_header("Random-1000");
    test_intersects_bench_rand_polygon(runs, 1000);
    print_header("Random-10000");
    test_intersects_bench_rand_polygon(runs, 10000);
}


void geos_error(const char *message, void *userdata) {
    printf("\n\n   == %s ==\n\n", message);
} 

void read_bench_run(int runs, char *name, char *libname, char *kind, char *data, size_t len) {
    char label[64];
    // snprintf(label, sizeof(label), "%s/%s/%s/%s", name, libname, kind, "read");
    snprintf(label, sizeof(label), "%s/%s/%s", libname, kind, "read");
    double bench_secs;
    double start;

    int subruns = 10;
    for (int i = 0; i < runs; i++) {
        invalidate_cache();
        if (!markdown) {
            printf("\r%-21s %2d/%d ", label, i+1, runs); 
        }
        // printf("\r%-18s %2d ", label, i+1); 
        fflush(stdout);
        double nsecs;
        if (strcmp(libname, "tg") == 0) {
            if (strcmp(kind, "wkb") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    struct tg_geom *geom = tg_parse_wkb_ix((uint8_t*)data, len, TG_NONE);
                    assert(!tg_geom_error(geom));
                    tg_geom_free(geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "hex") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    struct tg_geom *geom = tg_parse_hex_ix(data, TG_NONE);
                    assert(!tg_geom_error(geom));
                    tg_geom_free(geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "wkt") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    struct tg_geom *geom = tg_parse_wkt_ix(data, TG_NONE);
                    assert(!tg_geom_error(geom));
                    tg_geom_free(geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "json") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    struct tg_geom *geom = tg_parse_geojson_ix(data, TG_NONE);
                    assert(!tg_geom_error(geom));
                    tg_geom_free(geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else {
                fprintf(stderr, "invalid kind '%s'\n", kind);
                abort();
            }
        #ifdef GEOS_BENCH
        } else if (strcmp(libname, "geos") == 0) {
            GEOSContextHandle_t handle = GEOS_init_r();
            GEOSContext_setErrorMessageHandler_r(handle, geos_error, 0);
            if (strcmp(kind, "wkb") == 0) {
                GEOSWKBReader *reader = GEOSWKBReader_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    GEOSGeometry *geom = GEOSWKBReader_read_r(handle, reader, (unsigned char*)data, len);
                    assert(geom);
                    GEOSGeom_destroy_r(handle, geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKBReader_destroy_r(handle, reader);
            } else if (strcmp(kind, "hex") == 0) {
                GEOSWKBReader *reader = GEOSWKBReader_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    GEOSGeometry *geom = GEOSWKBReader_readHEX_r(handle, reader, (unsigned char*)data, len);
                    assert(geom);
                    GEOSGeom_destroy_r(handle, geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKBReader_destroy_r(handle, reader);
            } else if (strcmp(kind, "wkt") == 0) {
                GEOSWKTReader *reader = GEOSWKTReader_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    GEOSGeometry *geom = GEOSWKTReader_read_r(handle, reader, data);
                    assert(geom);
                    GEOSGeom_destroy_r(handle, geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKTReader_destroy_r(handle, reader);
            } else if (strcmp(kind, "json") == 0) {
                GEOSGeoJSONReader *reader = GEOSGeoJSONReader_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    GEOSGeometry *geom = GEOSGeoJSONReader_readGeometry_r(handle, reader, data);
                    assert(geom);
                    GEOSGeom_destroy_r(handle, geom);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSGeoJSONReader_destroy_r(handle, reader);
            } else {
                fprintf(stderr, "invalid kind '%s'\n", kind);
                abort();
            }
            GEOS_finish_r(handle);
        #endif
        } else {
            fprintf(stderr, "invalid libname '%s'\n", libname);
            abort();
        }

        if (i == 0 || (nsecs < bench_secs && nsecs > 0)) {
            bench_secs = nsecs;
        }
    }
    if (!markdown) {
        printf("\r");
    }
    printf("%-15s ", label); 
    printf(ROW_FIELDS_ENCDEC, commaize(1/bench_secs), bench_secs/1*1e9, 
        ((double)len)/bench_secs/1024.0/1024.0);
    printf("\n");
}



void write_bench_run(int runs, char *name, char *libname, char *kind, char *data, size_t len) {
    char label[64];
    // snprintf(label, sizeof(label), "%s/%s/%s/%s", name, libname, kind, "write");
    snprintf(label, sizeof(label), "%s/%s/%s", libname, kind, "write");
    double bench_secs;
    double start;

    int subruns = 10;
    for (int i = 0; i < runs; i++) {
        invalidate_cache();
        if (!markdown) {
            printf("\r%-21s %2d/%d ", label, i+1, runs); 
        }
        // printf("\r%-18s %2d ", label, i+1); 
        fflush(stdout);
        double nsecs;
        if (strcmp(libname, "tg") == 0) {
            struct tg_geom *geom = tg_parse_wkb_ix((uint8_t*)data, len, TG_NONE);
            if (strcmp(kind, "wkb") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t len2 = tg_geom_wkb(geom, 0, 0);
                    uint8_t *data2 = malloc(len2);
                    assert(data2);
                    tg_geom_wkb(geom, data2, len2);
                    free(data2);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "hex") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t len2 = tg_geom_hex(geom, 0, 0);
                    char *data2 = malloc(len2+1);
                    assert(data2);
                    tg_geom_hex(geom, data2, len2+1);
                    free(data2);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "wkt") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t size = tg_geom_wkt(geom, 0, 0);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else if (strcmp(kind, "json") == 0) { 
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t size = tg_geom_geojson(geom, 0, 0);
                }
                nsecs = (clock_now()-start)/(double)subruns;
            } else {
                fprintf(stderr, "invalid kind '%s'\n", kind);
                abort();
            }
        #ifdef GEOS_BENCH
        } else if (strcmp(libname, "geos") == 0) {
            GEOSContextHandle_t handle = GEOS_init_r();
            GEOSContext_setErrorMessageHandler_r(handle, geos_error, 0);
            GEOSWKBReader *reader = GEOSWKBReader_create_r(handle);
            GEOSGeometry *geom = GEOSWKBReader_read_r(handle, reader, (unsigned char*)data, len);
            assert(geom);
            if (strcmp(kind, "wkb") == 0) {
                GEOSWKBWriter *writer = GEOSWKBWriter_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t size;
                    void *data = GEOSWKBWriter_write_r(handle, writer, geom, &size);
                    free(data);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKBWriter_destroy_r(handle, writer);
            } else if (strcmp(kind, "hex") == 0) {
                GEOSWKBWriter *writer = GEOSWKBWriter_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    size_t size;
                    void *data = GEOSWKBWriter_writeHEX_r(handle, writer, geom, &size);
                    free(data);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKBWriter_destroy_r(handle, writer);
            } else if (strcmp(kind, "wkt") == 0) {
                GEOSWKTWriter *writer = GEOSWKTWriter_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    void *data = GEOSWKTWriter_write_r(handle, writer, geom);
                    free(data);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSWKTWriter_destroy_r(handle, writer);
            } else if (strcmp(kind, "json") == 0) {
                GEOSGeoJSONWriter *writer = GEOSGeoJSONWriter_create_r(handle);
                double start = clock_now();
                for (int j = 0; j < subruns; j++) {
                    void *data = GEOSGeoJSONWriter_writeGeometry_r(handle, writer, geom, 0);
                    free(data);
                }
                nsecs = (clock_now()-start)/(double)subruns;
                GEOSGeoJSONWriter_destroy_r(handle, writer);
            } else {
                fprintf(stderr, "invalid kind '%s'\n", kind);
                abort();
            }
            GEOSGeom_destroy_r(handle, geom);
            GEOSWKBReader_destroy_r(handle, reader);
            GEOS_finish_r(handle);
        #endif
        } else {
            fprintf(stderr, "invalid libname '%s'\n", libname);
            abort();
        }

        if (i == 0 || (nsecs < bench_secs && nsecs > 0)) {
            bench_secs = nsecs;
        }
    }
    if (!markdown) {
        printf("\r");
    }
    printf("%-15s ", label);
    printf(ROW_FIELDS_ENCDEC, commaize(1/bench_secs), bench_secs/1*1e9, 
        ((double)len)/bench_secs/1024.0/1024.0);
    printf("\n");
}


void test_io_bench(int runs, char *name) {
    struct tg_geom *geom = load_geom(name, TG_NONE);
    char *wkb, *json, *wkt, *hex;
    size_t wkbsz, jsonsz, wktsz, hexsz;

    wkbsz = tg_geom_wkb(geom, 0, 0);
    wkb = malloc(wkbsz);
    assert(wkb);
    tg_geom_wkb(geom, (uint8_t*)wkb, wkbsz);

    jsonsz = tg_geom_geojson(geom, 0, 0);
    json = malloc(jsonsz+1);
    assert(json);
    tg_geom_geojson(geom, json, jsonsz+1);

    wktsz = tg_geom_wkt(geom, 0, 0);
    wkt = malloc(wktsz+1);
    assert(wkt);
    tg_geom_wkt(geom, wkt, wktsz+1);

    hexsz = tg_geom_hex(geom, 0, 0);
    hex = malloc(hexsz+1);
    assert(hex);
    tg_geom_hex(geom, hex, hexsz+1);



    read_bench_run(runs, name, "tg", "wkb", wkb, wkbsz);
    write_bench_run(runs, name, "tg", "wkb", wkb, wkbsz);
    read_bench_run(runs, name, "tg", "hex", hex, hexsz);
    write_bench_run(runs, name, "tg", "hex", wkb, wkbsz);
    read_bench_run(runs, name, "tg", "wkt", wkt, wktsz);
    write_bench_run(runs, name, "tg", "wkt", wkb, wkbsz);
    read_bench_run(runs, name, "tg", "json", json, jsonsz);
    write_bench_run(runs, name, "tg", "json", wkb, wkbsz);
#ifdef GEOS_BENCH
    read_bench_run(runs, name, "geos", "wkb", wkb, wkbsz);
    write_bench_run(runs, name, "geos", "wkb", wkb, wkbsz);
    read_bench_run(runs, name, "geos", "hex", hex, hexsz);
    write_bench_run(runs, name, "geos", "hex", wkb, wkbsz);
    read_bench_run(runs, name, "geos", "wkt", wkt, wktsz);
    write_bench_run(runs, name, "geos", "wkt", wkb, wkbsz);
    read_bench_run(runs, name, "geos", "json", json, jsonsz);
    write_bench_run(runs, name, "geos", "json", wkb, wkbsz);
#endif

    free(hex);
    free(wkt);
    free(wkb);
    free(json);
    tg_geom_free(geom);
}


void main_io_bench(uint64_t seed, int runs) {
    print_start_bold();
    printf("== I/O ==");
    print_end_bold();
    printf("Benchmark reading and writing polygons from various formats.\n");
    printf("Performs %d run%s and chooses the average.\n", runs, runs!=0?"s":"");

    print_header_io("Brazil");
    test_io_bench(runs, "br");

    print_header_io("Texas");
    test_io_bench(runs, "tx");

    print_header_io("Arizona");
    test_io_bench(runs, "az");

    print_header_io("Br Columbia");
    test_io_bench(runs, "bc");

    print_header_io("Rhode Island");
    test_io_bench(runs, "ri");
}

int main(int argc, char **argv) {
    tg_env_set_allocator(bmalloc, brealloc, bfree);
    markdown = atoi(getenv("MARKDOWN")?getenv("MARKDOWN"):"0");
    uint64_t seed = strtoull(getenv("SEED")?getenv("SEED"):"0", NULL, 10);
    if (!seed) seed = mkrandseed();

    int runs = atoi(getenv("RUNS")?getenv("RUNS"):"0");
    if (runs <= 0) runs = 10;

    printf("SEED=%llu\n", (unsigned long long)seed);
    printf("RUNS=%llu\n", (unsigned long long)runs);

    if (argc == 2) {
        // run all tests
        main_pip_bench(seed, runs, false);
        main_intersects_bench(seed, runs);
        main_io_bench(seed, runs);
    } else {
        // run specific tests
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "io") == 0 || strcmp(argv[i], "i/o") == 0) {
                main_io_bench(seed, runs);
            }
            if (strcmp(argv[i], "pip") == 0) {
                main_pip_bench(seed, runs, false);
            }
            if (strcmp(argv[i], "pip_simple") == 0) {
                main_pip_bench(seed, runs, true);
            }
            if (strcmp(argv[i], "intersects") == 0) {
                main_intersects_bench(seed, runs);
            }
        }
    }
    // Do a quick exit to avoid any thread-local and c++ stack unrolling.
    _Exit(0);
}