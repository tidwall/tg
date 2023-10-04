#include "tests.h"

void test_ring_contains_point() {
    struct tg_ring *ring;
    
    ring = RING(octagon);
    assert(!tg_ring_contains_point(ring, P(0, 0), true).hit);
    assert(tg_ring_contains_point(ring, P(0, 5), true).hit);
    assert(!tg_ring_contains_point(ring, P(0, 5), false).hit);
    assert(tg_ring_contains_point(ring, P(4, 4), true).hit);
    assert(!tg_ring_contains_point(ring, P(1.4, 1.4), true).hit);
    assert(tg_ring_contains_point(ring, P(1.5, 1.5), true).hit);
    assert(!tg_ring_contains_point(ring, P(1.5, 1.5), false).hit);

    ring = RING({0, 0}, {4, 0}, {4, 3}, {3, 4}, {1, 4}, {0, 3}, {0, 0});
    assert(!tg_ring_contains_point(ring, P(0, 3.5), true).hit);
    assert(!tg_ring_contains_point(ring, P(0.4, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(0.5, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(0.6, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(1, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(3, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(3.4, 3.5), true).hit);
    assert(tg_ring_contains_point(ring, P(3.5, 3.5), true).hit);
    assert(!tg_ring_contains_point(ring, P(3.9, 3.5), true).hit);
    assert(!tg_ring_contains_point(ring, P(4, 3.5), true).hit);
    assert(!tg_ring_contains_point(ring, P(4.1, 3.5), true).hit);
}

void test_ring_intersects_segment() {
    struct tg_ring *ring;

    // TestRingXIntersectsSegment/Cases
    ring = RING(P(0, 0), P(4, 0), P(4, 4), P(0, 4), P(0, 0));
    assert(tg_ring_intersects_segment(ring, S(2, 2, 4, 4), true));
    assert(tg_ring_intersects_segment(ring, S(2, 2, 4, 4), false));
    assert(tg_ring_intersects_segment(ring, S(2, 0, 4, 2), true));
    assert(tg_ring_intersects_segment(ring, S(2, 0, 4, 2), false));
    assert(tg_ring_intersects_segment(ring, S(1, 0, 3, 0), true));
    assert(!tg_ring_intersects_segment(ring, S(1, 0, 3, 0), false));
    assert(tg_ring_intersects_segment(ring, S(2, 4, 2, 0), true));
    assert(tg_ring_intersects_segment(ring, S(2, 4, 2, 0), false));
    assert(tg_ring_intersects_segment(ring, S(2, 0, 2, -3), true));
    assert(!tg_ring_intersects_segment(ring, S(2, 0, 2, -3), false));
    assert(!tg_ring_intersects_segment(ring, S(2, -1, 2, -3), true));
    assert(!tg_ring_intersects_segment(ring, S(2, -1, 2, -3), false));
    assert(tg_ring_intersects_segment(ring, S(-1, 2, 3, 2), true));
    assert(tg_ring_intersects_segment(ring, S(-1, 2, 3, 2), false));
    assert(tg_ring_intersects_segment(ring, S(-1, 3, 2, 1), true));
    assert(tg_ring_intersects_segment(ring, S(-1, 3, 2, 1), false));
    assert(tg_ring_intersects_segment(ring, S(-1, 2, 5, 1), true));
    assert(tg_ring_intersects_segment(ring, S(-1, 2, 5, 1), false));
    assert(tg_ring_intersects_segment(ring, S(2, 5, 1, -1), true));
    assert(tg_ring_intersects_segment(ring, S(2, 5, 1, -1), false));
    assert(tg_ring_intersects_segment(ring, S(0, 4, 4, 0), true));
    assert(tg_ring_intersects_segment(ring, S(0, 4, 4, 0), false));

    ring = RING({0, 0}, {2, 0}, {4, 0}, {4, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(1, 0, 3, 0), true));
    assert(!tg_ring_intersects_segment(ring, S(1, 0, 3, 0), false));

    ring = RING({0, 0}, {4, 0}, {4, 4}, {2, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(0, 4, 4, 4), true));
    assert(!tg_ring_intersects_segment(ring, S(0, 4, 4, 4), false));

    ring = RING({0, 0}, {4, 0}, {4, 4}, {2, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(-1, -2, 0, 0), true));
    assert(!tg_ring_intersects_segment(ring, S(-1, -2, 0, 0), false));

    ring = RING({0, 0}, {4, 0}, {4, 4}, {2, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(0, 4, 5, 4), true));
    assert(!tg_ring_intersects_segment(ring, S(0, 4, 5, 4), false));

    ring = RING({0, 0}, {4, 0}, {4, 4}, {2, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(1, 4, 5, 4), true));
    assert(!tg_ring_intersects_segment(ring, S(1, 4, 5, 4), false));

    ring = RING({0, 0}, {4, 0}, {4, 4}, {2, 4}, {0, 4}, {0, 0});
    assert(tg_ring_intersects_segment(ring, S(1, 4, 4, 4), true));
    assert(!tg_ring_intersects_segment(ring, S(1, 4, 4, 4), false));

    // convex shape
    // TestRingXIntersectsSegment/Octagon
    ring = RING(octagon);
    { // TestRingXIntersectsSegment/Octagon/Outside
        // test coming off of the edges
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 5, -5, 5), -1, 0),           // left
            tg_segment_move(S(1.5, 1.5, -3.5, -3.5), -1, -1), // bottom-left corner
            tg_segment_move(S(5, 0, 5, -5), 0, -1),           // bottom
            tg_segment_move(S(8.5, 1.5, 13.5, -3.5), 1, -1),  // bottom-right corner
            tg_segment_move(S(10, 5, 15, 5), 1, 0),           // right
            tg_segment_move(S(8.5, 8.5, 13.5, 13.5), 1, 1),   // top-right corner
            tg_segment_move(S(5, 10, 5, 15), 0, 5),           // top
            tg_segment_move(S(1.5, 8.5, -3.5, 13.5), -1, 1),  // top-left corner
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(!tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(!tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/OutsideEdgeTouch
        // test coming off of the edges
        struct tg_segment segs[] = {
            S(0, 5, -5, 5),          // left
            S(1.5, 1.5, -3.5, -3.5), // bottom-left corner
            S(5, 0, 5, -5),          // bottom
            S(8.5, 1.5, 13.5, -3.5), // bottom-right corner
            S(10, 5, 15, 5),         // right
            S(8.5, 8.5, 13.5, 13.5), // top-right corner
            S(5, 10, 5, 15),         // top
            S(1.5, 8.5, -3.5, 13.5), // top-left corner
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/EdgeCross
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 5, -5, 5), 2.5, 0),              // left
            tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 2.5, 2.5),   // bottom-left corner
            tg_segment_move(S(5, 0, 5, -5), 0, 2.5),              // bottom
            tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -2.5, 2.5),  // bottom-right corner
            tg_segment_move(S(10, 5, 15, 5), -2.5, 0),            // right
            tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -2.5, -2.5), // top-right corner
            tg_segment_move(S(5, 10, 5, 15), 0, -2.5),            // top
            tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 2.5, -2.5),  // top-left corner
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/InsideEdgeTouch
       struct tg_segment segs[] = {
            tg_segment_move(S(0, 5, -5, 5), 5, 0),            // left
            tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 5, 5),   // bottom-left corner
            tg_segment_move(S(5, 0, 5, -5), 0, 5),            // bottom
            tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -5, 5),  // bottom-right corner
            tg_segment_move(S(10, 5, 15, 5), -5, 0),          // right
            tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -5, -5), // top-right corner
            tg_segment_move(S(5, 10, 5, 15), 0, -5),          // top
            tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 5, -5),  // top-left corner
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/Inside
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 5, -5, 5), 6, 0),            // left
            tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 6, 6),   // bottom-left corner
            tg_segment_move(S(5, 0, 5, -5), 0, 6),            // bottom
            tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -6, 6),  // bottom-right corner
            tg_segment_move(S(10, 5, 15, 5), -6, 0),          // right
            tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -6, -6), // top-right corner
            tg_segment_move(S(5, 10, 5, 15), 0, -6),          // top
            tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 6, -6),  // top-left corner
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/InsideEdgeToEdge
        struct tg_segment segs[] = {
            S(0, 5, 10, 5),        // left to right
            S(1.5, 1.5, 8.5, 8.5), // bottom-left to top-right
            S(5, 0, 5, 10),        // bottom to top
            S(8.5, 1.5, 1.5, 8.5), // bottom-right to top-left
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/PassoverEdgeToEdge
        struct tg_segment segs[] = {
            S(-1, 5, 11, 5),       // left to right
            S(0.5, 0.5, 9.5, 9.5), // bottom-left to top-right
            S(5, -1, 5, 11),       // bottom to top
            S(9.5, 0.5, 0.5, 9.5), // bottom-right to top-left
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }

    // Concave
    ring = RING(concave1);
    { // TestRingXIntersectsSegment/Concave/Outside
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 7.5, -5, 7.5), -1, 0), // left
            tg_segment_move(S(2.5, 5, 2.5, 0), 0, -1),  // bottom-left-1
            tg_segment_move(S(5, 2.5, 0, 2.5), -1, 0),  // bottom-left-2
            tg_segment_move(S(7.5, 0, 7.5, -5), 0, -1), // bottom
            tg_segment_move(S(10, 5, 15, 5), 1, 0),     // right
            tg_segment_move(S(5, 10, 5, 15), 0, 1),     // top
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(!tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(!tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
        }
    }

    { // TestRingXIntersectsSegment/Octagon/OutsideEdgeTouch
        struct tg_segment segs[] = {
            S(0, 7.5, -5, 7.5), // left
            S(2.5, 5, 2.5, 0),  // bottom-left-1
            S(5, 2.5, 0, 2.5),  // bottom-left-2
            S(7.5, 0, 7.5, -5), // bottom
            S(10, 5, 15, 5),    // right
            S(5, 10, 5, 15),    // top
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(!tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/EdgeCross
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 7.5, -5, 7.5), 2.5, 0), // left
            tg_segment_move(S(2.5, 5, 2.5, 0), 0, 2.5),  // bottom-left-1
            tg_segment_move(S(5, 2.5, 0, 2.5), 2.5, 0),  // bottom-left-2
            tg_segment_move(S(7.5, 0, 7.5, -5), 0, 2.5), // bottom
            tg_segment_move(S(10, 5, 15, 5), -2.5, 0),   // right
            tg_segment_move(S(5, 10, 5, 15), 0, -2.5),   // top
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/InsideEdgeTouch
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 7.5, -3, 7.5), 3, 0), // 0:left
            tg_segment_move(S(2.5, 5, 2.5, 2), 0, 3),  // 1:bottom-left-1
            tg_segment_move(S(5, 2.5, 2, 2.5), 3, 0),  // 2:bottom-left-2
            tg_segment_move(S(7.5, 0, 7.5, -3), 0, 3), // 3:bottom
            tg_segment_move(S(10, 5, 13, 5), -3, 0),   // 4:right
            tg_segment_move(S(5, 10, 5, 13), 0, -3),   // 5:top
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/Inside
        struct tg_segment segs[] = {
            tg_segment_move(S(0, 7.5, -3, 7.5), 4, 0), // 0:left
            tg_segment_move(S(2.5, 5, 2.5, 2), 0, 4),  // 1:bottom-left-1
            tg_segment_move(S(5, 2.5, 2, 2.5), 4, 0),  // 2:bottom-left-2
            tg_segment_move(S(7.5, 0, 7.5, -3), 0, 4), // 3:bottom
            tg_segment_move(S(10, 5, 13, 5), -4, 0),   // 4:right
            tg_segment_move(S(5, 10, 5, 13), 0, -4),   // 5:top
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/InsideEdgeToEdge
        struct tg_segment segs[] = {
            S(0, 7.5, 10, 7.5), // 0:left
            S(2.5, 5, 2.5, 10), // 1:bottom-left-1
            S(5, 2.5, 10, 2.5), // 2:bottom-left-2
            S(7.5, 0, 7.5, 10), // 3:bottom
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
    { // TestRingXIntersectsSegment/Octagon/PassoverEdgeToEdge
        struct tg_segment segs[] = {
            S(-1, 7.5, 11, 7.5), // 0:left
            S(2.5, 4, 2.5, 11),  // 1:bottom-left-1
            S(4, 2.5, 11, 2.5),  // 2:bottom-left-2
            S(7.5, -1, 7.5, 11), // 3:bottom
        };
        for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
            struct tg_segment seg = segs[i];
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
            struct tg_point tmp = seg.a;
            seg.a = seg.b;
            seg.b = tmp;
            assert(tg_ring_intersects_segment(ring, seg, true));
            assert(tg_ring_intersects_segment(ring, seg, false));
        }
    }
}

void test_ring_intersects_segment_parallel(void) {
    // struct tg_segment slong = S(1.57,1.61,1.57,1.92); 
    struct tg_segment slong = S(1,4,1,1); 
    struct tg_segment sshort = S(1,3,1,2); 
    struct tg_ring *big = (struct tg_ring*)tg_parse_wkt(
        // "POLYGON((1.57 1.92,1.89 1.92,1.89 1.61,1.57 1.61,1.57 1.92))");
        "POLYGON((1 1,4 1,4 4,1 4,1 2.5,1 1))");
    struct tg_ring *small = (struct tg_ring*)tg_parse_wkt(
        // "POLYGON((1.44 1.84,1.57 1.84,1.57 1.72,1.44 1.72,1.44 1.84))");
        "POLYGON((0 2, 1 2,1 2.5,1 3, 0 3, 0 2))");
    bool isects0, isects1;

    isects0 = tg_ring_intersects_segment(big, sshort, true);
    isects1 = tg_ring_intersects_segment(big, sshort, false);
    // printf("big/short:   isects0: %d, isects1: %d\n", isects0, isects1);
    assert(isects0 && !isects1);

    isects0 = tg_ring_intersects_segment(big, slong, true);
    isects1 = tg_ring_intersects_segment(big, slong, false);
    // printf("big/long:    isects0: %d, isects1: %d\n", isects0, isects1);
    assert(isects0 && !isects1);

    isects0 = tg_ring_intersects_segment(small, sshort, true);
    isects1 = tg_ring_intersects_segment(small, sshort, false);
    // printf("small/short: isects0: %d, isects1: %d\n", isects0, isects1);
    assert(isects0 && !isects1);
    
    isects0 = tg_ring_intersects_segment(small, slong, true);
    isects1 = tg_ring_intersects_segment(small, slong, false);
    // printf("small/long:  isects0: %d, isects1: %d\n", isects0, isects1);
    assert(isects0 && !isects1);

    tg_ring_free(big);
    tg_ring_free(small);

}

void test_ring_covers_segment() {
    assert(tg_ring_contains_segment(RR(0, 0, 10, 10), S(5, 5, 5, 5), true));
    assert(tg_ring_contains_segment(RR(0, 0, 10, 10), S(5, 5, 5, 5), false));
    assert(tg_ring_contains_segment(RR(0, 0, 10, 10), S(0, 0, 0, 0), true));
    assert(!tg_ring_contains_segment(RR(0, 0, 10, 10), S(0, 0, 0, 0), false));
    assert(tg_ring_contains_segment(RR(0, 0, 10, 10), S(10, 10, 10, 10), true));
    assert(!tg_ring_contains_segment(RR(0, 0, 10, 10), S(10, 10, 10, 10), false));

    // convex shape
    { // Octagon
        struct tg_ring *ring = RING(octagon);
        { // Outside
            // test coming off of the edges
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 5, -5, 5), -1, 0),           // left
                tg_segment_move(S(1.5, 1.5, -3.5, -3.5), -1, -1), // bottom-left corner
                tg_segment_move(S(5, 0, 5, -5), 0, -1),           // bottom
                tg_segment_move(S(8.5, 1.5, 13.5, -3.5), 1, -1),  // bottom-right corner
                tg_segment_move(S(10, 5, 15, 5), 1, 0),           // right
                tg_segment_move(S(8.5, 8.5, 13.5, 13.5), 1, 1),   // top-right corner
                tg_segment_move(S(5, 10, 5, 15), 0, 5),           // top
                tg_segment_move(S(1.5, 8.5, -3.5, 13.5), -1, 1),  // top-left corner
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // OutsideEdgeTouch
            // test coming off of the edges
            struct tg_segment segs[] = {
                S(0, 5, -5, 5),          // left
                S(1.5, 1.5, -3.5, -3.5), // bottom-left corner
                S(5, 0, 5, -5),          // bottom
                S(8.5, 1.5, 13.5, -3.5), // bottom-right corner
                S(10, 5, 15, 5),         // right
                S(8.5, 8.5, 13.5, 13.5), // top-right corner
                S(5, 10, 5, 15),         // top
                S(1.5, 8.5, -3.5, 13.5), // top-left corner
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // EdgeCross
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 5, -5, 5), 2.5, 0),              // left
                tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 2.5, 2.5),   // bottom-left corner
                tg_segment_move(S(5, 0, 5, -5), 0, 2.5),              // bottom
                tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -2.5, 2.5),  // bottom-right corner
                tg_segment_move(S(10, 5, 15, 5), -2.5, 0),            // right
                tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -2.5, -2.5), // top-right corner
                tg_segment_move(S(5, 10, 5, 15), 0, -2.5),            // top
                tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 2.5, -2.5),  // top-left corner
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // InsideEdgeTouch
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 5, -5, 5), 5, 0),            // left
                tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 5, 5),   // bottom-left corner
                tg_segment_move(S(5, 0, 5, -5), 0, 5),            // bottom
                tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -5, 5),  // bottom-right corner
                tg_segment_move(S(10, 5, 15, 5), -5, 0),          // right
                tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -5, -5), // top-right corner
                tg_segment_move(S(5, 10, 5, 15), 0, -5),          // top
                tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 5, -5),  // top-left corner
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // Inside
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 5, -5, 5), 6, 0),            // left
                tg_segment_move(S(1.5, 1.5, -3.5, -3.5), 6, 6),   // bottom-left corner
                tg_segment_move(S(5, 0, 5, -5), 0, 6),            // bottom
                tg_segment_move(S(8.5, 1.5, 13.5, -3.5), -6, 6),  // bottom-right corner
                tg_segment_move(S(10, 5, 15, 5), -6, 0),          // right
                tg_segment_move(S(8.5, 8.5, 13.5, 13.5), -6, -6), // top-right corner
                tg_segment_move(S(5, 10, 5, 15), 0, -6),          // top
                tg_segment_move(S(1.5, 8.5, -3.5, 13.5), 6, -6),  // top-left corner
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // InsideEdgeToEdge
            struct tg_segment segs[] = {
                S(0, 5, 10, 5),        // left to right
                S(1.5, 1.5, 8.5, 8.5), // bottom-left to top-right
                S(5, 0, 5, 10),        // bottom to top
                S(8.5, 1.5, 1.5, 8.5), // bottom-right to top-left
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // PassoverEdgeToEdge
            struct tg_segment segs[] = {
                S(-1, 5, 11, 5),       // left to right
                S(0.5, 0.5, 9.5, 9.5), // bottom-left to top-right
                S(5, -1, 5, 11),       // bottom to top
                S(9.5, 0.5, 0.5, 9.5), // bottom-right to top-left
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
    }

    // concave shape
    { // Concave
        struct tg_ring *ring = RING(concave1);
        { // Outside
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 7.5, -5, 7.5), -1, 0), // left
                tg_segment_move(S(2.5, 5, 2.5, 0), 0, -1),  // bottom-left-1
                tg_segment_move(S(5, 2.5, 0, 2.5), -1, 0),  // bottom-left-2
                tg_segment_move(S(7.5, 0, 7.5, -5), 0, -1), // bottom
                tg_segment_move(S(10, 5, 15, 5), 1, 0),     // right
                tg_segment_move(S(5, 10, 5, 15), 0, 1),     // top
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // OutsideEdgeTouch
            struct tg_segment segs[] = {
                S(0, 7.5, -5, 7.5), // left
                S(2.5, 5, 2.5, 0),  // bottom-left-1
                S(5, 2.5, 0, 2.5),  // bottom-left-2
                S(7.5, 0, 7.5, -5), // bottom
                S(10, 5, 15, 5),    // right
                S(5, 10, 5, 15),    // top
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // EdgeCross
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 7.5, -5, 7.5), 2.5, 0), // left
                tg_segment_move(S(2.5, 5, 2.5, 0), 0, 2.5),  // bottom-left-1
                tg_segment_move(S(5, 2.5, 0, 2.5), 2.5, 0),  // bottom-left-2
                tg_segment_move(S(7.5, 0, 7.5, -5), 0, 2.5), // bottom
                tg_segment_move(S(10, 5, 15, 5), -2.5, 0),   // right
                tg_segment_move(S(5, 10, 5, 15), 0, -2.5),   // top
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // InsideEdgeTouch
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 7.5, -3, 7.5), 3, 0), // 0:left
                tg_segment_move(S(2.5, 5, 2.5, 2), 0, 3),  // 1:bottom-left-1
                tg_segment_move(S(5, 2.5, 2, 2.5), 3, 0),  // 2:bottom-left-2
                tg_segment_move(S(7.5, 0, 7.5, -3), 0, 3), // 3:bottom
                tg_segment_move(S(10, 5, 13, 5), -3, 0),   // 4:right
                tg_segment_move(S(5, 10, 5, 13), 0, -3),   // 5:top
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // Inside
            struct tg_segment segs[] = {
                tg_segment_move(S(0, 7.5, -3, 7.5), 4, 0), // 0:left
                tg_segment_move(S(2.5, 5, 2.5, 2), 0, 4),  // 1:bottom-left-1
                tg_segment_move(S(5, 2.5, 2, 2.5), 4, 0),  // 2:bottom-left-2
                tg_segment_move(S(7.5, 0, 7.5, -3), 0, 4), // 3:bottom
                tg_segment_move(S(10, 5, 13, 5), -4, 0),   // 4:right
                tg_segment_move(S(5, 10, 5, 13), 0, -4),   // 5:top
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // InsideEdgeToEdge
            struct tg_segment segs[] = {
                S(0, 7.5, 10, 7.5), // 0:left
                S(2.5, 5, 2.5, 10), // 1:bottom-left-1
                S(5, 2.5, 10, 2.5), // 2:bottom-left-2
                S(7.5, 0, 7.5, 10), // 3:bottom
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
        { // PassoverEdgeToEdge
            struct tg_segment segs[] = {
                S(-1, 7.5, 11, 7.5), // 0:left
                S(2.5, 4, 2.5, 11),  // 1:bottom-left-1
                S(4, 2.5, 11, 2.5),  // 2:bottom-left-2
                S(7.5, -1, 7.5, 11), // 3:bottom
            };
            for (size_t i = 0; i < sizeof(segs)/sizeof(struct tg_segment); i++) {
                struct tg_segment seg = segs[i];
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
                struct tg_point tmp = seg.a;
                seg.a = seg.b;
                seg.b = tmp;
                assert(!tg_ring_contains_segment(ring, seg, true));
                assert(!tg_ring_contains_segment(ring, seg, false));
            }
        }
    }


    { // Cases
        struct tg_ring *ring = RING(P(0, 0), P(4, 0), P(4, 4), P(3, 4), P(2, 3), P(1, 4), P(0, 4), P(0, 0));
        { // 1
            assert(!tg_ring_contains_segment(ring, S(1.5, 3.5, 2.5, 3.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.5, 3.5, 2.5, 3.5), false));
        }
        { // 2
            assert(!tg_ring_contains_segment(ring, S(1.0, 3.5, 2.5, 3.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.0, 3.5, 2.5, 3.5), false));
        }
        { // 3
            assert(tg_ring_contains_segment(ring, S(1.25, 3.75, 1.75, 3.25), true));
            assert(!tg_ring_contains_segment(ring, S(1.25, 3.75, 1.75, 3.25), false));
        }
        { // 4
            assert(!tg_ring_contains_segment(ring, S(1.5, 3.5, 3, 3.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.5, 3.5, 3, 3.5), false));
        }
        { // 5
            assert(!tg_ring_contains_segment(ring, S(1.0, 3.5, 3, 3.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.0, 3.5, 3, 3.5), false));
        }
        { // 6
            struct tg_ring *ring = RING(
                P(0, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2, 5), P(1, 4), P(0, 4), P(0, 0),
            );
            assert(tg_ring_contains_segment(ring, S(1.5, 4.5, 2.5, 4.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.5, 4.5, 2.5, 4.5), false));
        }
        { // 7
            struct tg_ring *ring = RING(
                P(0, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2.5, 3), P(2, 4), P(1.5, 3),
                P(1, 4), P(0, 4), P(0, 0),
            );
            assert(!tg_ring_contains_segment(ring, S(1.25, 3.5, 2.75, 3.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.25, 3.5, 2.75, 3.5), false));
        }
        { // 8
            struct tg_ring *ring = RING(
                P(0, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2.5, 5), P(2, 4), P(1.5, 5),
                P(1, 4), P(0, 4), P(0, 0),
            );
            assert(!tg_ring_contains_segment(ring, S(1.25, 4.5, 2.75, 4.5), true));
            assert(!tg_ring_contains_segment(ring, S(1.25, 4.5, 2.75, 4.5), false));
        }
        { // 9
            assert(tg_ring_contains_segment(ring, S(1, 2, 3, 2), true));
            assert(tg_ring_contains_segment(ring, S(1, 2, 3, 2), false));
        }
        { // 0
            assert(tg_ring_contains_segment(ring, S(1, 3, 3, 2), true));
            assert(tg_ring_contains_segment(ring, S(1, 3, 3, 2), false));
        }
        { // 1
            assert(tg_ring_contains_segment(ring, S(1, 2, 3, 3), true));
            assert(tg_ring_contains_segment(ring, S(1, 2, 3, 3), false));
        }
        { // 2
            assert(tg_ring_contains_segment(ring, S(1.5, 3.5, 1.5, 2), true));
            assert(!tg_ring_contains_segment(ring, S(1.5, 3.5, 1.5, 2), false));
        }
        { // 3
            struct tg_ring *ring = RING(
                P(0, 0), P(2, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2, 3), P(1, 4), P(0, 4), P(0, 0),
            );
            assert(tg_ring_contains_segment(ring, S(1, 0, 3, 0), true));
            assert(!tg_ring_contains_segment(ring, S(1, 0, 3, 0), false));
        }
        { // 4
            struct tg_ring *ring = RING(
                P(0, 0), P(4, 0), P(2, 2), P(0, 4), P(0, 0),
            );
            assert(tg_ring_contains_segment(ring, S(1, 3, 3, 1), true));
            assert(tg_ring_contains_segment(ring, S(3, 1, 1, 3), true));
            assert(!tg_ring_contains_segment(ring, S(1, 3, 3, 1), false));
            assert(!tg_ring_contains_segment(ring, S(3, 1, 1, 3), false));
        }
        { // 5
            assert(tg_ring_contains_segment(ring, S(1, 3, 3, 3), true));
            assert(!tg_ring_contains_segment(ring, S(1, 3, 3, 3), false));
        }
        { // 6
            assert(tg_ring_contains_segment(ring, S(1, 3, 2, 3), true));
            assert(!tg_ring_contains_segment(ring, S(1, 3, 2, 3), false));
        }
    }
}

void test_ring_contains_ring() {
    { // Empty
        assert(!tg_ring_contains_ring(NULL, RR(0, 0, 1, 1), true));
        assert(!tg_ring_contains_ring(RR(0, 0, 1, 1), NULL, true));
    }
    { // Exact
        assert(tg_ring_contains_ring(RR(0, 0, 1, 1), RR(0, 0, 1, 1), true));
        assert(!tg_ring_contains_ring(RR(0, 0, 1, 1), RR(0, 0, 1, 1), false));
        assert(tg_ring_contains_ring(RING(concave1), RING(concave1), true));
        assert(!tg_ring_contains_ring(RING(concave1), RING(concave1), false));
        assert(tg_ring_contains_ring(RING(octagon), RING(octagon), true));
        assert(!tg_ring_contains_ring(RING(octagon), RING(octagon), false));
    }
    { // Cases
        // concave
        struct tg_ring *ring = RING(
            P(0, 0), P(4, 0), P(4, 4), P(3, 4),
            P(2, 3), P(1, 4), P(0, 4), P(0, 0),
        );
        { // 1
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), true));
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), false));
        }
        { // 2
            assert(tg_ring_contains_ring(ring, RR(0, 0, 2, 1), true));
            assert(!tg_ring_contains_ring(ring, RR(0, 0, 2, 1), false));
        }
        { // 3
            assert(!tg_ring_contains_ring(ring, RR(-1.5, 1, 1.5, 2), true));
            assert(!tg_ring_contains_ring(ring, RR(-1.5, 1, 1.5, 2), false));
        }
        { // 4
            assert(!tg_ring_contains_ring(ring, RR(1, 2.5, 3, 3.5), true));
            assert(!tg_ring_contains_ring(ring, RR(1, 2.5, 3, 3.5), false));
        }
        { // 5
            assert(tg_ring_contains_ring(ring, RR(1, 2, 3, 3), true));
            assert(!tg_ring_contains_ring(ring, RR(1, 2, 3, 3), false));
        }
        // convex
        ring = RING(
            P(0, 0), P(4, 0), P(4, 4),
            P(3, 4), P(2, 5), P(1, 4),
            P(0, 4), P(0, 0),
        );
        { // 6
            assert(tg_ring_contains_ring(ring, RR(1, 2, 3, 3), true));
            assert(tg_ring_contains_ring(ring, RR(1, 2, 3, 3), false));
        }
        { // 7
            assert(tg_ring_contains_ring(ring, RR(1, 3, 3, 4), true));
            assert(!tg_ring_contains_ring(ring, RR(1, 3, 3, 4), false));
        }
        { // 8
            assert(!tg_ring_contains_ring(ring, RR(1, 3.5, 3, 4.5), true));
            assert(!tg_ring_contains_ring(ring, RR(1, 3.5, 3, 4.5), false));
        }
        { // 9
            ring = RING(
                P(0, 0), P(2, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2, 5), P(1, 4), P(0, 4), P(0, 0),
            );
            assert(tg_ring_contains_ring(ring, RR(1, 0, 3, 1), true));
            assert(!tg_ring_contains_ring(ring, RR(1, 0, 3, 1), false));
        }
        { // 10
            ring = RING(
                P(0, 0), P(4, 0), P(4, 3), P(2, 4),
                P(0, 3), P(0, 0),
            );
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), true));
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), false));
        }
        ring = RING(
            P(0, 0), P(4, 0), P(4, 3), P(3, 4), P(1, 4), P(0, 3), P(0, 0),
        );
        { // 11
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), true));
            assert(tg_ring_contains_ring(ring, RR(1, 1, 3, 2), false));
        }
        { // 12
            assert(tg_ring_contains_ring(ring, RR(0, 1, 2, 2), true));
            assert(!tg_ring_contains_ring(ring, RR(0, 1, 2, 2), false));
        }
        { // 13
            assert(!tg_ring_contains_ring(ring, RR(-1, 1, 1, 2), true));
            assert(!tg_ring_contains_ring(ring, RR(-1, 1, 1, 2), false));
        }
        { // 14
            assert(tg_ring_contains_ring(ring, RR(0.5, 2.5, 2.5, 3.5), true));
            assert(!tg_ring_contains_ring(ring, RR(0.5, 2.5, 2.5, 3.5), false));
        }
        { // 15
            assert(!tg_ring_contains_ring(ring, RR(0.25, 2.75, 2.25, 3.75), true));
            assert(!tg_ring_contains_ring(ring, RR(0.25, 2.75, 2.25, 3.75), false));
        }
        { // 16
            assert(!tg_ring_contains_ring(ring, RR(-2, 1, -1, 2), true));
            assert(!tg_ring_contains_ring(ring, RR(-2, 1, -1, 2), false));
        }
        { // 17
            assert(!tg_ring_contains_ring(ring, RR(-0.5, 3.5, 0.5, 4.5), true));
            assert(!tg_ring_contains_ring(ring, RR(-0.5, 3.5, 0.5, 4.5), false));
        }
        { // 18
            assert(!tg_ring_contains_ring(ring, RR(-0.75, 3.75, 0.25, 4.75), true));
            assert(!tg_ring_contains_ring(ring, RR(-0.74, 3.75, 0.25, 4.75), false));
        }
        { // 19
            assert(!tg_ring_contains_ring(ring, RR(-1, -1, 5, 5), true));
            assert(!tg_ring_contains_ring(ring, RR(-1, -1, 5, 5), false));
        }
    }
    { // Identical
        struct tg_ring *rings[] = {
            RR(0, 0, 10, 10),
            RING(octagon),
            RING(concave1),
            RING(concave3),
            RING(ri),
        };
        for (size_t i = 0; i < sizeof(rings)/sizeof(struct tg_ring*); i++) {
            assert(tg_ring_contains_ring(rings[i], rings[i], true));
        }
    }
    { // Big
        // use rhode island
        struct tg_ring *ring = RING(ri);
        assert(tg_ring_contains_ring(rect_to_ring_test(tg_ring_rect(ring)), ring, true));
        assert(tg_ring_contains_ring(ring, ring, true));
    }
}

bool intersect_both_ways(struct tg_ring *ring_a, struct tg_ring *ring_b, bool allow_on_edge) {
    bool t1 = tg_ring_intersects_ring(ring_a, ring_b, allow_on_edge);
    bool t2 = tg_ring_intersects_ring(ring_b, ring_a, allow_on_edge);
    assert(t1 == t2);
    return t1;
}

void test_ring_intersects_ring() {
    { // Empty
        assert(!intersect_both_ways(NULL, RR(0, 0, 1, 1), true));
        assert(!intersect_both_ways(RR(0, 0, 1, 1), NULL, true));
    }
    { // Cases
        // concave
        struct tg_ring *ring = RING(
            P(0, 0), P(4, 0), P(4, 4), P(3, 4),
            P(2, 3), P(1, 4), P(0, 4), P(0, 0),
        );
        { // 1
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), true));
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), false));
        }
        { // 2
            assert(intersect_both_ways(ring, RR(0, 0, 2, 1), true));
            assert(intersect_both_ways(ring, RR(0, 0, 2, 1), false));
        }
        { // 3
            assert(intersect_both_ways(ring, RR(-1.5, 1, 1.5, 2), true));
            assert(intersect_both_ways(ring, RR(-1.5, 1, 1.5, 2), false));
        }
        { // 4
            assert(intersect_both_ways(ring, RR(1, 2.5, 3, 3.5), true));
            assert(intersect_both_ways(ring, RR(1, 2.5, 3, 3.5), false));
        }
        { // 5
            assert(intersect_both_ways(ring, RR(1, 2, 3, 3), true));
            assert(intersect_both_ways(ring, RR(1, 2, 3, 3), false));
        }
        // convex
        ring = RING(
            P(0, 0), P(4, 0), P(4, 4),
            P(3, 4), P(2, 5), P(1, 4),
            P(0, 4), P(0, 0),
        );
        { // 6
            assert(intersect_both_ways(ring, RR(1, 2, 3, 3), true));
            assert(intersect_both_ways(ring, RR(1, 2, 3, 3), false));
        }
        { // 7
            assert(intersect_both_ways(ring, RR(1, 3, 3, 4), true));
            assert(intersect_both_ways(ring, RR(1, 3, 3, 4), false));
        }
        { // 8
            assert(intersect_both_ways(ring, RR(1, 3.5, 3, 4.5), true));
            assert(intersect_both_ways(ring, RR(1, 3.5, 3, 4.5), false));
        }
        { // 9
            ring = RING(
                P(0, 0), P(2, 0), P(4, 0), P(4, 4), P(3, 4),
                P(2, 5), P(1, 4), P(0, 4), P(0, 0),
            );
            assert(intersect_both_ways(ring, RR(1, 0, 3, 1), true));
            assert(intersect_both_ways(ring, RR(1, 0, 3, 1), false));
        }
        { // 10
            ring = RING(
                P(0, 0), P(4, 0), P(4, 3), P(2, 4),
                P(0, 3), P(0, 0),
            );
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), true));
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), false));
        }
        ring = RING(
            P(0, 0), P(4, 0), P(4, 3), P(3, 4), P(1, 4), P(0, 3), P(0, 0),
        );
        { // 11
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), true));
            assert(intersect_both_ways(ring, RR(1, 1, 3, 2), false));
        }
        { // 12
            assert(intersect_both_ways(ring, RR(0, 1, 2, 2), true));
            assert(intersect_both_ways(ring, RR(0, 1, 2, 2), false));
        }
        { // 13
            assert(intersect_both_ways(ring, RR(-1, 1, 1, 2), true));
            assert(intersect_both_ways(ring, RR(-1, 1, 1, 2), false));
        }
        { // 14
            assert(intersect_both_ways(ring, RR(0.5, 2.5, 2.5, 3.5), true));
            assert(intersect_both_ways(ring, RR(0.5, 2.5, 2.5, 3.5), false));
        }
        { // 15
            assert(intersect_both_ways(ring, RR(0.25, 2.75, 2.25, 3.75), true));
            assert(intersect_both_ways(ring, RR(0.25, 2.75, 2.25, 3.75), false));
        }
        { // 16
            assert(!intersect_both_ways(ring, RR(-2, 1, -1, 2), true));
            assert(!intersect_both_ways(ring, RR(-2, 1, -1, 2), false));
        }
        { // 17
            assert(intersect_both_ways(ring, RR(-0.5, 3.5, 0.5, 4.5), true));
            assert(!intersect_both_ways(ring, RR(-0.5, 3.5, 0.5, 4.5), false));
        }
        { // 18
            assert(!intersect_both_ways(ring, RR(-0.75, 3.75, 0.25, 4.75), true));
            assert(!intersect_both_ways(ring, RR(-0.74, 3.75, 0.25, 4.75), false));
        }
        { // 19
            assert(tg_ring_intersects_ring(ring, RR(-1, -1, 5, 5), true));
            assert(tg_ring_intersects_ring(ring, RR(-1, -1, 5, 5), false));
        }
    }
}

struct tg_line *make_line(struct tg_point start) {
    return LINE(
        start, 
        tg_point_move(start, 0, 1), 
        tg_point_move(start, 1, 1),
        tg_point_move(start, 1, 2), 
        tg_point_move(start, 2, 2),
    );
}

void test_ring_covers_line() {
    { //Cases
        // convex
        struct tg_ring *ring = RING(
            P(0, 0), P(4, 0), P(4, 3),
            P(3, 4), P(1, 4),
            P(0, 3), P(0, 0),
        );
        { //1
            assert(tg_ring_contains_line(ring, make_line(P(1, 1)), true, 0));
            assert(tg_ring_contains_line(ring, make_line(P(1, 1)), false, 0));
        }
        { //2
            assert(tg_ring_contains_line(ring, make_line(P(0, 1)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(0, 1)), false, 0));
        }
        { //3
            assert(!tg_ring_contains_line(ring, make_line(P(-0.5, 1)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(-0.5, 1)), false, 0));
        }
        { //4
            assert(tg_ring_contains_line(ring, make_line(P(0, 2)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(0, 2)), false, 0));
        }
        { //5
            assert(!tg_ring_contains_line(ring, make_line(P(0, 2.5)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(0, 2.5)), false, 0));
        }
        { //6
            assert(!tg_ring_contains_line(ring, make_line(P(2, 2)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(2, 2)), false, 0));
        }
        { //7
            assert(!tg_ring_contains_line(ring, make_line(P(2, 1.5)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(2, 1.5)), false, 0));
        }
        { //8
            assert(tg_ring_contains_line(ring, make_line(P(2, 1)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(2, 1)), false, 0));
        }
        { //9
            assert(tg_ring_contains_line(ring, make_line(P(2, 0)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(2, 0)), false, 0));
        }
        { //10
            assert(tg_ring_contains_line(ring, make_line(P(1.5, 0)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(1.5, 0)), false, 0));
        }
        { //11
            assert(!tg_ring_contains_line(ring, make_line(P(1, -1)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(1, -1)), false, 0));
        }
        { //12
            assert(!tg_ring_contains_line(ring, make_line(P(1.5, -0.5)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(1.5, -0.5)), false, 0));
        }
        { //13
            assert(tg_ring_contains_line(ring, make_line(P(0, 0)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(0, 0)), false, 0));
        }
        { //14
            assert(!tg_ring_contains_line(ring, make_line(P(3, 1)), true, 0));
            assert(!tg_ring_contains_line(ring, make_line(P(3, 1)), false, 0));
        }
        struct tg_line *line = NULL;
        { //15
            line = LINE(P(-1, -1), P(-1, 2), P(2, 2), P(2, 5), P(5, 5));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //16
            line = LINE(P(0, -1), P(0, 1), P(3, 3), P(5, 3));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //17
            line = LINE(P(-1, 2), P(2, 1), P(5, 2));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //18
            line = LINE(P(1, -1), P(3, 2), P(2, 5));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //19
            line = LINE(P(3.5, 4.5), P(3.5, 3.5), P(5, 3.5), P(5, 2.5));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //20
            line = LINE(P(4, 5), P(4, 4), P(5, 4), P(5, 3));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
        { //21
            line = LINE(P(1, -1), P(1, -2), P(2, -2), P(2, -3));
            assert(!tg_ring_contains_line(ring, line, true, 0));
            assert(!tg_ring_contains_line(ring, line, false, 0));
        }
    }
}

void test_ring_intersects_line() {
    { // Various
        assert(!tg_ring_intersects_line(RR(0, 0, 0, 0), LINE(), true));
        assert(!tg_ring_intersects_line(RR(0, 0, 10, 10),
            LINE(P(-1, -1), P(11, -1), P(11, 11), P(-1, 11)), 
            true));
        assert(tg_ring_intersects_line(RR(0, 0, 10, 10), 
            LINE(P(-1, -1), P(11, -1), P(11, 11), P(-1, 11), P(5, -1)), 
            true));
    }
    { // Cases
        // convex
        struct tg_ring *ring = RING(
            P(0, 0), P(4, 0), P(4, 3),
            P(3, 4), P(1, 4),
            P(0, 3), P(0, 0),
        );
        { // 1
            assert(tg_ring_intersects_line(ring, make_line(P(1, 1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(1, 1)), false));
        }
        { // 2
            assert(tg_ring_intersects_line(ring, make_line(P(0, 1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(0, 1)), false));
        }
        { // 3
            assert(tg_ring_intersects_line(ring, make_line(P(-0.5, 1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(-0.5, 1)), false));
        }
        { // 4
            assert(tg_ring_intersects_line(ring, make_line(P(0, 2)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(0, 2)), false));
        }
        { // 5
            assert(tg_ring_intersects_line(ring, make_line(P(0, 2.5)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(0, 2.5)), false));
        }
        { // 6
            assert(tg_ring_intersects_line(ring, make_line(P(2, 2)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(2, 2)), false));
        }
        { // 7
            assert(tg_ring_intersects_line(ring, make_line(P(2, 1.5)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(2, 1.5)), false));
        }
        { // 8
            assert(tg_ring_intersects_line(ring, make_line(P(2, 1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(2, 1)), false));
        }
        { // 9
            assert(tg_ring_intersects_line(ring, make_line(P(2, 0)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(2, 0)), false));
        }
        { // 10
            assert(tg_ring_intersects_line(ring, make_line(P(1.5, 0)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(1.5, 0)), false));
        }
        { // 11
            assert(tg_ring_intersects_line(ring, make_line(P(1, -1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(1, -1)), false));
        }
        { // 12
            assert(tg_ring_intersects_line(ring, make_line(P(1.5, -0.5)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(1.5, -0.5)), false));
        }
        { // 13
            assert(tg_ring_intersects_line(ring, make_line(P(0, 0)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(0, 0)), false));
        }
        { // 14
            assert(tg_ring_intersects_line(ring, make_line(P(3, 1)), true));
            assert(tg_ring_intersects_line(ring, make_line(P(3, 1)), false));
        }
        struct tg_line *line = NULL;
        { // 15
            line = LINE(P(-1, -1), P(-1, 2), P(2, 2), P(2, 5), P(5, 5));
            assert(tg_ring_intersects_line(ring, line, true));
            assert(tg_ring_intersects_line(ring, line, false));
        }
        { // 16
            line = LINE(P(0, -1), P(0, 1), P(3, 3), P(5, 3));
            assert(tg_ring_intersects_line(ring, line, true));
            assert(tg_ring_intersects_line(ring, line, false));
        }
        { // 17
            line = LINE(P(-1, 2), P(2, 1), P(5, 2));
            assert(tg_ring_intersects_line(ring, line, true));
            assert(tg_ring_intersects_line(ring, line, false));
        }
        { // 18
            line = LINE(P(1, -1), P(3, 2), P(2, 5));
            assert(tg_ring_intersects_line(ring, line, true));
            assert(tg_ring_intersects_line(ring, line, false));
        }
        { // 19
            line = LINE(P(3.5, 4.5), P(3.5, 3.5), P(5, 3.5), P(5, 2.5));
            assert(tg_ring_intersects_line(ring, line, true));
            assert(!tg_ring_intersects_line(ring, line, false));
        }
        { // 20
            line = LINE(P(4, 5), P(4, 4), P(5, 4), P(5, 3));
            assert(!tg_ring_intersects_line(ring, line, true));
            assert(!tg_ring_intersects_line(ring, line, false));
        }
        { // 21
            line = LINE(P(1, -1), P(1, -2), P(2, -2), P(2, -3));
            assert(!tg_ring_intersects_line(ring, line, true));
            assert(!tg_ring_intersects_line(ring, line, false));
        }
    }
}

void test_ring_not_closed() {
    struct tg_ring *ring = RING(P(1,1), P(2,1), P(2,2), P(1,2));
    assert(tg_ring_contains_point(ring, P(1.5, 1.5), true).hit);
}

void test_ring_points() {
    struct tg_ring *ring = RING(octagon);
    for (int i = 0; i < tg_ring_num_points(ring); i++) {
        struct tg_point p1 = tg_ring_point_at(ring, i);
        struct tg_point p2 = tg_ring_points(ring)[i];
        assert(pointeq(p1, p2));
    }
    assert(tg_ring_points(NULL) == NULL);
}

void test_ring_various() {
    struct tg_ring *ring = NULL;
    tg_ring_free(NULL);
    assert(!tg_ring_convex(NULL));
    assert(!tg_ring_clockwise(NULL));
    assert(tg_ring_clone(NULL) == NULL);
    assert(tg_ring_memsize(NULL) == 0);
    assert(tg_ring_num_points(NULL) == 0);
    struct tg_rect rect1 = tg_ring_rect(NULL);
    struct tg_rect rect2 = { 0 };
    assert(memcmp(&rect1, &rect2, sizeof(struct tg_rect)) == 0);
    assert(tg_ring_point_at(NULL, 0).x == 0);
    assert(tg_ring_point_at(NULL, 0).y == 0);
    assert(tg_ring_num_segments(NULL) == 0);
    struct tg_segment seg1 = tg_ring_segment_at(NULL, 0);
    struct tg_segment seg2 = { 0 };
    assert(memcmp(&seg1, &seg2, sizeof(struct tg_segment)) == 0);
    ring = RING_YSTRIPES(az);
    assert(!tg_ring_convex(ring));
    struct tg_ring *ring2 = tg_ring_clone(ring);
    assert(ring == ring2);
    tg_ring_free(ring2);
    ring2 = RING(az);
    assert(tg_ring_memsize(ring) > tg_ring_memsize(ring2));

    ring = RING({0, 0}, {4, 0}, {4, 3}, {3, 4}, {1, 4}, {0, 3}, {0, 0});
    assert(!tg_ring_clockwise(ring));
    assert(tg_ring_convex(ring));

    ring = RING(P(1,2),P(3,4));
    assert(tg_ring_num_points(ring) == 2);
    assert(pointeq( tg_ring_point_at(ring, 0), P(1, 2)));
    assert(pointeq( tg_ring_point_at(ring, 1), P(3, 4)));

    ring = RING(P(1,2));
    assert(tg_ring_num_points(ring) == 1);
    assert(pointeq( tg_ring_point_at(ring, 0), P(1, 2)));

    ring = RING();
    assert(tg_ring_num_points(ring) == 0);


    ring = tg_circle_new_ix((struct tg_point) { -112, 33 }, -1, 20, TG_NONE);
    assert(ring);
    assert(tg_ring_num_points(ring) == 20);
    tg_ring_free(ring);

    ring = tg_circle_new_ix((struct tg_point) { -112, 33 }, -1, -1, TG_NONE);
    assert(ring);
    assert(tg_ring_num_points(ring) == 4);
    tg_ring_free(ring);


    tg_ring_search(RING(az), R(0,0,0,0), 0, 0);
    tg_ring_search(0, R(0,0,0,0), 0, 0);

    tg_ring_ring_search(RING(az), 0, 0, 0);
    tg_ring_ring_search(0, RING(az), 0, 0);
    tg_ring_ring_search(0, 0, 0, 0);

    assert(tg_ring_contains_line(RING(az), 0, 0, 0) == 0);
    assert(tg_ring_contains_line(0, (struct tg_line*)RING(az), 0, 0) == 0);
    assert(tg_ring_contains_line(0, 0, 0, 0) == 0);

    assert(tg_ring_perimeter(RING(u1)) == 40.0);
    assert(tg_ring_area(RING(u1)) == 100.0);
}

void test_ring_pip(void) {
    struct tg_ring *ring1 = RING_NONE(az);
    struct tg_ring *ring2 = RING_NATURAL(az);
    struct tg_ring *ring3 = RING_YSTRIPES(az);
    double minx = tg_ring_rect(ring1).min.x;
    double miny = tg_ring_rect(ring1).min.y;
    double maxx = tg_ring_rect(ring1).max.x;
    double maxy = tg_ring_rect(ring1).max.y;
    int N = 100;
    for (int i = 0; i < N; i++) {
        double x = (((maxx-minx)/(double)N)*(double)i)+minx;
        for (int j = 0; j < N; j++) {
            double y = (((maxy-miny)/(double)N)*(double)j)+miny;
            bool h1 = tg_ring_contains_point(ring1, (struct tg_point) { x, y }, true).hit;
            bool h2 = tg_ring_contains_point(ring2, (struct tg_point) { x, y }, true).hit;
            bool h3 = tg_ring_contains_point(ring3, (struct tg_point) { x, y }, true).hit;
            assert( h2 == h1 );
            assert( h3 == h2 );
        }
    }
}

void test_ring_polsby_popper(void) {
    assert(tg_ring_polsby_popper_score(RING_NONE(az)) > 0.5);
    assert(tg_line_polsby_popper_score((struct tg_line*)RING_NONE(az)) > 0.5);
}

void test_ring_chaos(void) {
    struct tg_ring *ring;
    int must_fail;
    
    // circle chaos
    must_fail = 100;
    while (must_fail > 0) {
        ring = tg_circle_new((struct tg_point) { -112, 33 }, 600, 600);
        if (!ring) {
            must_fail--;
            continue;
        }
        tg_ring_free(ring);
        ring = NULL;
    }

    // copy chaos
    while (!ring) {
        struct tg_point points[] = { az };
        ring = tg_ring_new_ix(points, sizeof(points)/sizeof(struct tg_point), TG_YSTRIPES);
    }

    struct tg_ring *ring2;
    must_fail = 100;
    while (must_fail > 0) {
        ring2 = tg_ring_copy(ring);
        if (!ring2) {
            must_fail--;
            continue;
        }
        tg_ring_free(ring2);
        ring2 = NULL;
    }
    tg_ring_free(ring);
}

struct rrres {
    int aidx;
    int bidx;
};

int rrres_cmp(const void *va, const void *vb) {
    struct rrres *a = (struct rrres*)va;
    struct rrres *b = (struct rrres*)vb;
    return a->aidx < b->aidx ? -1 : a->aidx > b->aidx ? 1 :
           a->bidx < b->bidx ? -1 : a->bidx > b->bidx;
}

struct rrctx {
    struct rrres *all;
    int len;
    int cap;
    int max;
    struct tg_ring *aring;
    struct tg_ring *bring;
};

bool rriter(struct tg_segment seg_a, int index_a, struct tg_segment seg_b, 
    int index_b, void *udata)
{
    struct rrctx *ctx = udata;
    if (ctx->max == ctx->len) {
        return false;
    }
    struct tg_segment aseg = tg_ring_segment_at(ctx->aring, index_a);
    struct tg_segment bseg = tg_ring_segment_at(ctx->bring, index_b);
    assert(memcmp(&seg_a, &aseg, sizeof(struct tg_segment)) == 0);
    assert(memcmp(&seg_b, &bseg, sizeof(struct tg_segment)) == 0);
    if (ctx->len == ctx->cap) {
        ctx->cap = !ctx->cap?1024:ctx->cap*2;
        ctx->all = xrealloc(ctx->all, ctx->cap * sizeof(struct rrres));
        assert(ctx->all);
    }
    ctx->all[ctx->len].aidx = index_a;
    ctx->all[ctx->len].bidx = index_b;
    ctx->len++;
    return true;
}

void test_ring_ring_search(void) {
    const bool printout = false;
    uint64_t seed = mkrandseed();
    // seed = 23528185193229915;
    if (printout) printf("SEED=%llu\n", (unsigned long long) seed);
    srand(seed);

    double start = now();
    int h = 0;
    while (h < 3 || (h >= 3 && now()-start < 5)) {
        int npoints1 = rand_double()*1000+10;
        int npoints2 = rand_double()*1000+10;
        struct tg_point *points1 = make_rand_polygon(npoints1);
        assert(points1);
        struct tg_point *points2 = make_rand_polygon(npoints2);
        assert(points2);
        struct rrctx ctx1 = { 0 };
        struct rrctx ctx2 = { 0 };
        double start2 = now();
        int max = rand_double()*npoints1 + 10;;
        int i = 0;
        // printf("====\n");
        while (i < 5 || (i >= 5 && now()-start2 < 0.1)) {
            enum tg_index ix1 = tg_index_with_spread((rand()%5==0)?TG_NONE:TG_NATURAL, 
                rand_double()*20);
            enum tg_index ix2 = tg_index_with_spread((rand()%5==0)?TG_NONE:TG_NATURAL, 
                rand_double()*20);
            struct tg_ring *ring1 = tg_ring_new_ix(points1, npoints1, ix1);
            struct tg_ring *ring2 = tg_ring_new_ix(points2, npoints2, ix2);
    
            struct rrctx ctx = ctx1;
            ctx.max = max;
            ctx.len = 0;
            ctx.aring = ring1;
            ctx.bring = ring2;
            
            double start = now();
            switch (rand()%3) {
            case 0:
                tg_ring_ring_search(ring1, ring2, rriter, &ctx);
                break;
            case 1:
                tg_ring_line_search(ring1, (struct tg_line*)ring2, rriter, &ctx);
                break;
            case 2:
                tg_line_line_search((struct tg_line*)ring1, (struct tg_line*)ring2, rriter, &ctx);
                break;
            }
            double elapsed = now()-start;
            qsort(ctx.all, ctx.len, sizeof(struct rrres), rrres_cmp);
            if (printout) printf("npoints: %4d/%4d, ",npoints1, npoints2);
            if (printout) printf("hits: %4d, ", ctx.len);
            if (printout) printf("ix: %d/%d, ", tg_ring_index_num_levels(ring1)!=0, tg_ring_index_num_levels(ring2)!=0);
            if (printout) printf("spread: %2d/%2d, ", tg_ring_index_spread(ring1), tg_ring_index_spread(ring2));
            if (printout) printf("elapsed: %.4f secs ", elapsed);
            ctx1 = ctx2;
            ctx2 = ctx;
            if (i > 0) {
                bool ok = ctx1.len == ctx2.len;
                if (ok) {
                    for (int j = 0; j < ctx1.len; j++) {
                        if (rrres_cmp(&ctx1.all[j], &ctx2.all[j])) {
                            ok = false;
                            break;
                        }
                    }
                }
                if (ok) {
                    if (printout) printf("(OK)");
                } else {
                    if (printout) printf("(MISMATCH)");
                }
            } else {
                if (printout) printf("(OK)");
            }
            if (printout) printf("\n");
            i++;
            tg_ring_free(ring1);
            tg_ring_free(ring2);
        }
        if (ctx1.all) {
            xfree(ctx1.all);
        }
        if (ctx2.all) {
            xfree(ctx2.all);
        }
        free(points1);
        free(points2);
        h++;
    }
}

struct txiterctx {
    int count;
    int hits;
};

bool txiter(struct tg_segment seg_a, int index_a, struct tg_segment seg_b, 
    int index_b, void *udata)
{
    (void)seg_a; (void)seg_b; (void)index_a; (void)index_b;
    struct txiterctx *ctx = udata;
    if (ctx->count == ctx->hits) {
        return false;
    }
    ctx->hits++;
    return true;
}

void test_ring_ring_search_stop(void) {
    for (int i = 0; i < 100; i++) {
        struct tg_ring *ring0 = RANDOM_INDEX((rand()%5==0)?TG_NONE:TG_NATURAL, rand_double()*1000+300);
        assert(ring0);
        struct tg_ring *ring1 = RANDOM_INDEX((rand()%5==0)?TG_NONE:TG_NATURAL, rand_double()*1000+300);
        assert(ring1);
        struct txiterctx ctx = { .count = i };
        tg_ring_ring_search(ring0, ring1, txiter, &ctx);
        assert(ctx.hits == i);
        tg_ring_free(ring0);
        tg_ring_free(ring1);
    }
}

void test_ring_copy(void) {
    struct tg_ring *ring = RING_YSTRIPES(az);
    struct tg_ring *ring2 = tg_ring_copy(ring);
    assert(tg_geom_intersects_xy((struct tg_geom*)ring, -112, 33));
    tg_ring_free(ring2);
    assert(!tg_ring_copy(0));
}

int main(int argc, char **argv) {
    do_test(test_ring_contains_point);
    do_test(test_ring_intersects_segment);
    do_test(test_ring_intersects_segment_parallel);
    do_test(test_ring_covers_segment);
    do_test(test_ring_contains_ring);
    do_test(test_ring_intersects_ring);
    do_test(test_ring_covers_line);
    do_test(test_ring_intersects_line);
    do_test(test_ring_not_closed);
    do_test(test_ring_points);
    do_test(test_ring_various);
    do_test(test_ring_copy)
    do_test(test_ring_pip);
    do_test(test_ring_polsby_popper);
    do_test(test_ring_ring_search);
    do_test(test_ring_ring_search_stop);
    do_chaos_test(test_ring_chaos);
    return 0;
}
