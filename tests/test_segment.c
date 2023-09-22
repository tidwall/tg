#include "tests.h"

void test_segment_raycast() {
    // This is full coverage raycast test. It uses a 7x7 grid where the each
    // point is checked for a total of 49 tests per segment. There are 16
    // segments at 0º,30º,45º,90º,180º in both directions for a total of 784
    // tests.
    // The '-' character is OUT
    // The '+' character is IN
    // The '*' character is ON
    void *segs[] = {
        SA(1, 1, 5, 5), "A",
        SA(5, 5, 1, 1), "B",
            "-------",
            "-----*-",
            "++++*--",
            "+++*---",
            "++*----",
            "+*-----",
            "-------",
        SA(1, 5, 5, 1), "C",
        SA(5, 1, 1, 5), "D",
            "-------",
            "-*-----",
            "++*----",
            "+++*---",
            "++++*--",
            "+++++*-",
            "-------",
        SA(1, 3, 5, 3), "E",
        SA(5, 3, 1, 3), "F",
            "-------",
            "-------",
            "-------",
            "-*****-",
            "-------",
            "-------",
            "-------",
        SA(3, 5, 3, 1), "G",
        SA(3, 1, 3, 5), "H",
            "-------",
            "---*---",
            "+++*---",
            "+++*---",
            "+++*---",
            "+++*---",
            "-------",
        SA(1, 2, 5, 4), "I",
        SA(5, 4, 1, 2), "J",
            "-------",
            "-------",
            "-----*-",
            "+++*---",
            "+*-----",
            "-------",
            "-------",
        SA(1, 4, 5, 2), "K",
        SA(5, 2, 1, 4), "L",
            "-------",
            "-------",
            "-*-----",
            "+++*---",
            "+++++*-",
            "-------",
            "-------",
        SA(2, 1, 4, 5), "M",
        SA(4, 5, 2, 1), "N",
            "-------",
            "----*--",
            "++++---",
            "+++*---",
            "+++----",
            "++*----",
            "-------",
        SA(2, 5, 4, 1), "O",
        SA(4, 1, 2, 5), "P",
            "-------",
            "--*----",
            "+++----",
            "+++*---",
            "++++---",
            "++++*--",
            "-------",
        SA(3, 3, 3, 3), "Q",
        SA(3, 3, 3, 3), "R",
            "-------",
            "-------",
            "-------",
            "---*---",
            "-------",
            "-------",
            "-------",
    };
    bool ok = true;
    for (size_t i = 0; i < sizeof(segs)/sizeof(void*); i += 11) {
        void *segs2[] = {
            segs[i], segs[i+1], &segs[i+4],
            segs[i+2], segs[i+3], &segs[i+4],
        };
        for (size_t j = 0; j < sizeof(segs2)/sizeof(void*); j += 3) {
            double *seg = segs2[j];
            char *label = segs2[j+1];
            char **grid = segs2[j+2];
            char *ngrid[7];
            int y = 0;
            int sy = 6;
            for (;y < 7;) {
                char *nline = malloc(8);
                assert(nline);
                for (int x = 0; x < 7; x++) {
                    struct tg_segment lseg = { 
                        {seg[0], seg[1]},
                        {seg[2], seg[3]},
                    };
                    struct tg_point lpt = { x, y };
                    enum tg_raycast_result res = tg_raycast(lseg, lpt);
                    if (res == TG_IN) {
                        nline[x] = '+';
                    } else if (res == TG_ON) {
                        nline[x] = '*';
                    } else {
                        nline[x] = '-';
                    }
                }
                nline[7] = '\0';
                ngrid[sy] = nline;
                y++;
                sy--;
            }
            for (int k = 0; k < 7; k++) {
                if (strcmp(grid[k], ngrid[k]) != 0) {
                    fprintf(stderr, 
                        "MISMATCH (%s) SEGMENT: (%.0f, %.0f, %.0f, %.0f)\n", 
                        label, seg[0], seg[1], seg[2], seg[3]);
                    fprintf(stderr, "EXPECTED\n");
                    for (int k = 0; k < 7; k++) {
                        fprintf(stderr, "%s\n", grid[k]);
                    }
                    fprintf(stderr, "GOT\n");
                    for (int k = 0; k < 7; k++) {
                        fprintf(stderr, "%s\n", ngrid[k]);
                    }
                    ok = false;
                    break;
                }
            }
            for (int k = 0; k < 7; k++) {
                free(ngrid[k]);
            }
        }
    }
    if (!ok) {
        fprintf(stderr, "raycast test failed\n");
        exit(1);
    }
}

static struct tg_segment flip(struct tg_segment seg){
    return (struct tg_segment){ .a = seg.b, .b = seg.a };
}

void test_segment_intersects_segment() {
    #define V0 (void*)0
    #define V1 (void*)1
    void *vals[] = {
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "----A----", V1,
        "----A----", "----A----", V1, "----A----", "----*----", V1,
        "----*----", "----*----", V1, "----*----", "----B----", V1,
        "----B----", "----B----", V1, "----B----", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "----A----", V0,
        "---------", "----A----", V1, "---------", "----*----", V0,
        "---------", "----*----", V1, "---------", "----B----", V0,
        "----A----", "----B----", V1, "----A----", "---------", V0,
        "----*----", "---------", V1, "----*----", "---------", V0,
        "----B----", "---------", V1, "----B----", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "----A----", "---------", V1, "----A----", "---------", V1,
        "----*----", "----A----", V1, "----*----", "---------", V1,
        "----B----", "----*----", V1, "----B----", "----A----", V1,
        "---------", "----B----", V1, "---------", "----*----", V1,
        "---------", "---------", V1, "---------", "----B----", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "----A----", "---------", V1, "----A----", "---------", V0,
        "----*----", "---------", V1, "----*----", "---------", V0,
        "----B----", "----A----", V1, "----B----", "---------", V0,
        "---------", "----*----", V1, "---------", "----A----", V0,
        "---------", "----B----", V1, "---------", "----*----", V0,
        "---------", "---------", V1, "---------", "----B----", V0,

        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "----A----", "-----A---", V0, "----A----", "---A-----", V0,
        "----*----", "-----*---", V0, "----*----", "---*-----", V0,
        "----B----", "-----B---", V0, "----B----", "---B-----", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---A*B---", "---A*B---", V1, "---A*B---", "----A*B--", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---A*B---", "-----A*B-", V1, "---A*B---", "------A*B", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---A*B---", "--A*B----", V1, "---A*B---", "-A*B-----", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---A*B---", "--A*B----", V1, "---A*B---", "A*B------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---A*B---", V0, "---------", "---------", V0,
        "---A*B---", "---------", V0, "---A*B---", "---------", V0,
        "---------", "---------", V0, "---------", "---A*B---", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "------A--", V1,
        "-----A---", "-----A---", V1, "-----A---", "-----*---", V1,
        "----*----", "----*----", V1, "----*----", "----B----", V1,
        "---B-----", "---B-----", V1, "---B-----", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "--------A", V0,
        "---------", "-------A-", V1, "---------", "-------*-", V0,
        "---------", "------*--", V1, "---------", "------B--", V0,
        "-----A---", "-----B---", V1, "-----A---", "---------", V0,
        "----*----", "---------", V1, "----*----", "---------", V0,
        "---B-----", "---------", V1, "---B-----", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "-----A---", "---------", V1, "-----A---", "---------", V1,
        "----*----", "----A----", V1, "----*----", "---------", V1,
        "---B-----", "---*-----", V1, "---B-----", "---A-----", V1,
        "---------", "--B------", V1, "---------", "--*------", V1,
        "---------", "---------", V1, "---------", "-B-------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "-----A---", "---------", V0, "-----A---", "------A--", V0,
        "----*----", "---------", V0, "----*----", "-----*---", V0,
        "---B-----", "---------", V0, "---B-----", "----B----", V0,
        "---------", "--A------", V0, "---------", "---------", V0,
        "---------", "-*-------", V0, "---------", "---------", V0,
        "---------", "B--------", V0, "---------", "---------", V0,

        "---------", "---------", V0, "---------", "---------", V1,
        "---------", "---------", V0, "---------", "---------", V1,
        "---------", "---------", V0, "---------", "---------", V1,
        "-----A---", "----A----", V0, "---A-----", "---A-----", V1,
        "----*----", "---*-----", V0, "----*----", "----*----", V1,
        "---B-----", "--B------", V0, "-----B---", "-----B---", V1,
        "---------", "---------", V0, "---------", "---------", V1,
        "---------", "---------", V0, "---------", "---------", V1,
        "---------", "---------", V0, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "-A-------", V1,
        "---------", "--A------", V1, "---------", "--*------", V1,
        "---A-----", "---*-----", V1, "---A-----", "---B-----", V1,
        "----*----", "----B----", V1, "----*----", "---------", V1,
        "-----B---", "---------", V1, "-----B---", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "A--------", V0, "---------", "---------", V1,
        "---------", "-*-------", V0, "---------", "---------", V1,
        "---------", "--B------", V0, "---------", "---------", V1,
        "---A-----", "---------", V0, "---A-----", "---------", V1,
        "----*----", "---------", V0, "----*----", "----A----", V1,
        "-----B---", "---------", V0, "-----B---", "-----*---", V1,
        "---------", "---------", V0, "---------", "------B--", V1,
        "---------", "---------", V0, "---------", "---------", V1,
        "---------", "---------", V0, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---A-----", "---------", V1, "---A-----", "---------", V0,
        "----*----", "---------", V1, "----*----", "---------", V0,
        "-----B---", "-----A---", V1, "-----B---", "---------", V0,
        "---------", "------*--", V1, "---------", "------A--", V0,
        "---------", "-------B-", V1, "---------", "-------*-", V0,
        "---------", "---------", V1, "---------", "--------B", V0,

        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---A-----", "----A----", V0, "---A-----", "--A------", V0,
        "----*----", "-----*---", V0, "----*----", "---*-----", V0,
        "-----B---", "------B--", V0, "-----B---", "----B----", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---A-----", "-----A---", V1, "---A-----", "------A--", V1,
        "----*----", "----*----", V1, "----*----", "-----*---", V1,
        "-----B---", "---B-----", V1, "-----B---", "----B----", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---A-----", "-------A-", V1, "---A-----", "--------A", V0,
        "----*----", "------*--", V1, "----*----", "-------*-", V0,
        "-----B---", "-----B---", V1, "-----B---", "------B--", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---A-----", "-----A---", V1, "---A-----", "----A----", V1,
        "----*----", "----*----", V1, "----*----", "---*-----", V1,
        "-----B---", "---B-----", V1, "-----B---", "--B------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---A-----", "---A-----", V1, "---A-----", "--A------", V0,
        "----*----", "--*------", V1, "----*----", "-*-------", V0,
        "-----B---", "-B-------", V1, "-----B---", "B--------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,
        "---------", "---------", V1, "---------", "---------", V0,

        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "-----A---", V1,
        "---------", "-----A---", V1, "---------", "----*----", V1,
        "---A-----", "----*----", V1, "---A-----", "---B-----", V1,
        "----*----", "---B-----", V1, "----*----", "---------", V1,
        "-----B---", "---------", V1, "-----B---", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,
        "---------", "---------", V1, "---------", "---------", V1,

        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "-------A-", V0, "---------", "---------", V0,
        "---A-----", "------*--", V0, "---A-----", "---------", V0,
        "----*----", "-----B---", V0, "----*----", "---A-----", V0,
        "-----B---", "---------", V0, "-----B---", "--*------", V0,
        "---------", "---------", V0, "---------", "-B-------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
        "---------", "---------", V0, "---------", "---------", V0,
    };

    bool ok = true;
    int ncol = 3;
    size_t i = 0;
    size_t k = 0;
    for (; i < sizeof(vals)/sizeof(void*); k++) {
        char *grids[2][9];
        for (int j = 0; j < 9; j++) {
            grids[0][j] = vals[i+ncol*2*j];
        }
        for (int j = 0; j < 9; j++) {
            grids[1][j] = vals[i+1+ncol*2*j];
        }
        //label := "?" //vals[i+2].(string)
        bool expect = vals[i+2] != 0;

        struct tg_segment segs[2];
        for (int j = 0; j < 2; j++) {
            for (int y = 0; y < 9; y++) {
                for (int x = 0; x < 9; x++) {
                    char ch = grids[j][8-y][x];
                    if (ch == 'A') {
                        segs[j].a.x = x;
                        segs[j].a.y = y;
                    } else if (ch == 'B') {
                        segs[j].b.x = x;
                        segs[j].b.y = y;
                    }
                }
            }
        }
        struct tg_segment tests[][2] = {
            {segs[0], segs[1]},
            {flip(segs[0]), segs[1]},
            {segs[0], flip(segs[1])},
            {flip(segs[0]), flip(segs[1])},
        };
        for (int j = 0; j < 4; j++) {
            if (tg_segment_intersects_segment(tests[j][0], tests[j][1]) != 
                expect) 
            {
                ok = false;
                fprintf(stderr, "MISMATCH SEGMENTS: "
                    "[(%.0f, %.0f, %.0f, %.0f), (%.0f, %.0f, %.0f, %.0f)] "
                    "(TEST %d)\n", 
                    segs[0].a.x, segs[0].a.y, segs[0].b.x, segs[0].b.y,
                    segs[1].a.x, segs[1].a.y, segs[1].b.x, segs[1].b.y,
                    j);
                fprintf(stderr, "EXPECTED: %s, GOT: %s\n", 
                    expect?"true":"false", expect?"false":"true");
                fprintf(stderr, "GRID 1\n");
                for (int i = 0; i < 9; i++) {
                    fprintf(stderr, "%s\n", grids[0][i]);
                }
                fprintf(stderr, "GRID 2\n");
                for (int i = 0; i < 9; i++) {
                    fprintf(stderr, "%s\n", grids[1][i]);
                }
            }
        }
        // move to next block
        if (k%2 == 1) {
            i += ncol*2*9 - ncol;
        } else {
            i += ncol;
        }
    }
    if (!ok) {
        fprintf(stderr, "segment intersects test failed\n");
        exit(1);
    }
}

void test_segment_covers_rect() {
    assert(tg_segment_covers_rect(S(10,10,20,20), R(15,15,15,15)));
    assert(!tg_segment_covers_rect(S(10,10,20,20), R(15,16,15,16)));
    assert(tg_segment_covers_rect(S(10,15,20,15), R(15,15,16,15)));
}

int main(int argc, char **argv) {
    do_test(test_segment_raycast);
    do_test(test_segment_intersects_segment);
    do_test(test_segment_covers_rect);
    return 0;
}
