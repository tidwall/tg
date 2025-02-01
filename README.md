<p align="center">
<img src="docs/assets/logo.png" width="240" alt="TG">
</p>
<p align="center">
<a href="docs/API.md"><img src="https://img.shields.io/badge/api-reference-blue.svg?style=flat-square" alt="API Reference"></a>
</p>

TG is a geometry library for C that is small, fast, and easy to use.
It's designed for programs that need real-time geospatial, such as geofencing, monitoring, and streaming analysis.

## Features

- Implements OGC [Simple Features](https://en.wikipedia.org/wiki/Simple_Features) including Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, GeometryCollection. 
- Optimized [polygon indexing](docs/POLYGON_INDEXING.md) that introduces two new structures.
- Reads and writes [GeoJSON](https://en.wikipedia.org/wiki/GeoJSON), [WKT](https://en.wikipedia.org/wiki/Well-known_text_representation_of_geometry), [WKB](https://en.wikipedia.org/wiki/Well-known_text_representation_of_geometry), and [GeoBIN](docs/GEOBIN.md). 
- Provides a purely functional [API](docs/API.md) that is reentrant and thread-safe.
- Spatial predicates including "intersects", "covers", "touches", "equals", etc.
- Compiles to Webassembly using Emscripten
- [Test suite](tests/README.md) with 100% coverage using sanitizers and [Valgrind](https://valgrind.org).
- Self-contained library that is encapsulated in the single [tg.c](tg.c) source file.
- Pretty darn good performance. 🚀 <sup>[[benchmarks]](docs/BENCHMARKS.md)</sup>

## Goals

The main goal of TG is to provide the fastest, most memory efficent geometry library for the purpose of monitoring spatial relationships, specifically operations like point-in-polygon and geometry intersect.

It's a non-goal for TG to be a full GIS library. Consider [GEOS](https://libgeos.org) if you need GIS algorithms like generating a convex hull or voronoi diagram.

## Performance

TG uses [entirely new](docs/POLYGON_INDEXING.md) indexing structures that speed up [geometry predicates](docs/API.md#geometry-predicates). It can index more than 10GB per second of point data on modern hardware, while using less than 7% of additional memory, and can perform over 10 million point-in-polygon operations per second, even when using large polygons with over 10K points.

The following benchmark provides an example of the parsing, indexing, and point-in-polygon speed of TG vs GEOS when using a large polygon. In this case Brazil, which has 39K points.

<pre>
<b>Brazil              ops/sec    ns/op  points  hits       built      bytes</b>
tg/none              96,944    10315   39914  3257    46.73 µs    638,720
tg/natural       10,143,419       99   39914  3257    53.17 µs    681,360
tg/ystripes      15,174,761       66   39914  3257   884.06 µs  1,059,548
geos/none            29,708    33661   39914  3257   135.18 µs    958,104
geos/prepared     7,885,512      127   39914  3257  2059.94 µs  3,055,496
</pre>

- "built": Column showing how much time it took to construct the polygon and index.
- "bytes": Column showing the final in-memory size of the polygon and index.
- "none": No indexing was used.
- "natural": Using TG [Natural](docs/POLYGON_INDEXING.md#natural) indexing
- "ystripes": Using TG [YStripes](docs/POLYGON_INDEXING.md#ystripes) indexing
- "prepared": Using a [GEOS](https://libgeos.org) PreparedGeometry

See all [benchmarks](docs/BENCHMARKS.md) for more information.

## Using

Just drop the "tg.c" and "tg.h" files into your project. 
Uses standard C11 so most modern C compilers should work.

```sh
$ cc -c tg.c
```

## Programmer notes

*Check out the complete [API](docs/API.md) for detailed information.*

#### Pure functions

TG library functions are thread-safe, reentrant, and (mostly) without side
effects.
The exception being with the use of malloc by some functions like
[geometry constructors](docs/API.md#geometry-constructors).
In those cases, it's the programmer's responsibiilty to check the return value
before continuing.

```c
struct tg_geom *geom = tg_geom_new_point(-112, 33);
if (!geom) {
    // System is out of memory.
}
```

#### Fast cloning

The cloning of geometries, as with [tg_geom_clone()](docs/API.md#tg_geom_clone), are
O(1) operations that use implicit sharing through an atomic reference counter.
Geometry constructors like [tg_geom_new_polygon()](docs/API.md#tg_geom_new_polygon) will
use this method under the hood to maintain references of its inputs.

While this may only be an implementation detail, it's important for the
programmer to understand how TG uses memory and object references.

For example:

```c
struct tg_geom *geom = tg_geom_new_polygon(exterior, holes, nholes);
```

Above, a new geometry "geom" was created and includes a cloned reference to the 
[tg_ring](docs/API.md#tg_ring) "exterior" and all of the holes.

Providing `TG_NOATOMICS` to the compiler will disable the use of atomics and 
instead use non-atomic reference counters.

```
cc -DTG_NOATOMICS tg.c ...
```

Alternatively, the [tg_geom_copy()](docs/API.md#tg_geom_copy) method is available to perform a deep copy of the
geometry.

#### Avoid memory leaks

To avoid memory leaks, call [tg_geom_free()](docs/API.md#tg_geom_free) on geometries
created from [geometry constructors](docs/API.md#geometry-constructors), 
[geometry parsers](docs/API.md#geometry-parsing), [tg_geom_clone()](docs/API.md#tg_geom_clone), and [tg_geom_copy()](docs/API.md#tg_geom_copy)

In other words, for every `tg_geom_new_*()`, `tg_geom_parse_*()`, 
`tg_geom_clone()`, and `tg_geom_copy()` there should be (eventually and exactly)
one `tg_geom_free()`.

#### Upcasting

The TG object types [tg_line](docs/API.md#tg_line), [tg_ring](docs/API.md#tg_ring), and 
[tg_poly](docs/API.md#tg_poly) can be safely upcasted to a [tg_geom](docs/API.md#tg_geom) with no
cost at runtime.

```c
struct tg_geom *geom1 = (struct tg_geom*)line; // Cast tg_line to tg_geom
struct tg_geom *geom2 = (struct tg_geom*)ring; // Cast tg_ring to tg_geom
struct tg_geom *geom3 = (struct tg_geom*)poly; // Cast tg_poly to tg_geom
```

This allows for exposing all [tg_geom](docs/API.md#tg_geom) functions to the other
object types.

In addition, the [tg_ring](docs/API.md#tg_ring) type can also cast to a
[tg_poly](docs/API.md#tg_poly).

```c
struct tg_poly *poly = (struct tg_poly*)ring; // Cast tg_ring to tg_poly
```

*Do not downcast. It's not generally safe to cast from a [tg_geom](docs/API.md#tg_geom) to other
types.*


## Example

Create a program that tests if two geometries intersect using [WKT](https://en.wikipedia.org/wiki/Well-known_text_representation_of_geometry) as inputs.

```c
#include <stdio.h>
#include <tg.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <geom-a> <geom-b>\n", argv[0]);
        return 1;
    }

    // Parse the input geometries and check for errors.
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

    // Execute the "intersects" predicate to test if both geometries intersect.
    if (tg_geom_intersects(a, b)) {
        printf("yes\n");
    } else {
        printf("no\n");
    }

    // Free geometries when done.
    tg_geom_free(a);
    tg_geom_free(b);
    return 0;
}
```

Build and run the example:

```sh
$ cc -I. examples/intersects.c tg.c
$ ./a.out 'POINT(15 15)' 'POLYGON((10 10,20 10,20 20,10 20,10 10))'
yes
```

## License

TG source code is available under the MIT [License](/LICENSE).
