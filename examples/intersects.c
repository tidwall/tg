#include <stdio.h>
#include <tg.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <geom-a> <geom-b>\n", argv[0]);
        return 1;
    }
    struct tg_geom *a = tg_parse_wkt(argv[1]);
    if (tg_geom_error(a)) {
        fprintf(stderr, "%s\n", tg_geom_error(a));
        return 1;
    }
    struct tg_geom *b = tg_parse_wkt(argv[2]);
    if (tg_geom_error(b)) {
        fprintf(stderr, "%s\n", tg_geom_error(b));
        return 1;
    }
    if (tg_geom_intersects(a, b)) {
        printf("yes\n");
    } else {
        printf("no\n");
    }
    tg_geom_free(a);
    tg_geom_free(b);
    return 0;
}
