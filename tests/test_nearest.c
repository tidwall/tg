#include "tests.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(a, min, max) ((a) < (min) ? (min) : (a) > (max) ? (max) : (a))


struct nearest_ctx {
    const struct tg_ring *ring;
    int *iter_segs;
    struct tg_point point;
    int stop_at;
    bool stopped;
    double last_dist;
    int count;
};

double rect_dist(struct tg_rect rect, int *more, void *udata) {
    struct nearest_ctx *ctx = udata;
    assert(!ctx->stopped);
    double dist = tg_point_distance_rect(ctx->point, rect);
    *more = *more == 0 ? 3 : *more - 1;
    return dist;
}

double seg_dist(struct tg_segment seg, int *more, void *udata) {
    struct nearest_ctx *ctx = udata;
    assert(!ctx->stopped);
    double dist = tg_point_distance_segment(ctx->point, seg);
    *more = *more == 0 ? 3 : *more - 1;
    return dist;
}

bool iter(struct tg_segment seg, double dist, int index, void *udata) {
    (void)seg;
    struct nearest_ctx *ctx = udata;
    assert(!ctx->stopped);
    if (ctx->stop_at >= 0 && ctx->count == ctx->stop_at) {
        ctx->stopped = true;
        return false;
    }
    assert(index >= 0 && index < tg_ring_num_segments(ctx->ring));
    assert(ctx->count == 0 || !(dist < ctx->last_dist));
    assert(ctx->iter_segs[index] == 0);
    struct tg_segment seg_org = tg_ring_segment_at(ctx->ring, index);
    assert(memcmp(&seg, &seg_org, sizeof(struct tg_segment)) == 0);
    ctx->iter_segs[index] = 1;
    ctx->last_dist = dist;
    ctx->count++;
    return true;
}

bool ring_nearby_ordered(const char *name, enum tg_index ix, int stop_at) {
    struct nearest_ctx ctx = {
        .point = (struct tg_point) { 110, -35 },
        .stop_at = stop_at,
    };
    struct tg_ring *ring = (struct tg_ring *)load_geom(name, ix);
    if (!ring) return false;
    ctx.ring = ring;
    int nsegs = tg_ring_num_segments(ring);
    ctx.iter_segs = xmalloc(nsegs*sizeof(int));
    if (!ctx.iter_segs) {
        tg_ring_free(ring);
        return false;
    }
    memset(ctx.iter_segs, 0, nsegs*sizeof(int));
    // Use tg_line_nearest_segment because it's a lightweight wrapper around
    // tg_ring_nearest_segment and this will ensure both are tested at the
    // same time.
    bool ok = tg_line_nearest_segment((struct tg_line*)ring, rect_dist, 
        seg_dist, iter, &ctx);
    if (ok) {
        int count = 0;
        for (int i = 0; i < nsegs; i++) {
            if (ctx.iter_segs[i] == 1) {
                count++;
            }
        }
        assert(count == ctx.count);
        if (stop_at == -1) {
            assert(count == nsegs);
        } else {
            assert(count == stop_at);
        }
    }
    xfree(ctx.iter_segs);
    tg_ring_free(ring);
    return ok;
}

void test_nearby_group(const char *name, bool chaos, int count) {
    for (int i = 0; i < count; i++) {
        while (!ring_nearby_ordered(name, TG_NATURAL, 0)) assert(chaos);
        while (!ring_nearby_ordered(name, TG_NATURAL, 50)) assert(chaos);
        while (!ring_nearby_ordered(name, TG_NATURAL, -1)) assert(chaos);
        while (!ring_nearby_ordered(name, TG_NONE, 0)) assert(chaos);
        while (!ring_nearby_ordered(name, TG_NONE, 50)) assert(chaos);
        while (!ring_nearby_ordered(name, TG_NONE, -1)) assert(chaos);
    }
}

void test_nearest_basic() {
    assert(tg_ring_nearest_segment((void*)0, 0, 0, 0, 0));
    assert(tg_ring_nearest_segment((void*)1, 0, 0, 0, 0));
    test_nearby_group("rd.1000", false, 1);
    test_nearby_group("ri", false, 1);
    test_nearby_group("bc", false, 1);
    test_nearby_group("az", false, 1);
    test_nearby_group("tx", false, 1);
    test_nearby_group("br", false, 1);
}

void test_nearest_chaos() {
    test_nearby_group("rd.1000", true, 3);
    test_nearby_group("rd.100", true, 3);
    test_nearby_group("ri", true, 3);
    test_nearby_group("bc", true, 3);
    test_nearby_group("az", true, 3);
    test_nearby_group("tx", true, 3);
    // test_nearby_group("br", true, 1);
}

int main(int argc, char **argv) {
    do_test(test_nearest_basic);
    do_chaos_test(test_nearest_chaos);
    return 0;
}
