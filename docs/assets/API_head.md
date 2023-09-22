# TG C API

C API for the TG geometry library.

This document provides a detailed description of the functions and types in the
[tg.h](../tg.h) and [tg.c](../tg.c) source files for the TG library.

For a more general overview please see the project 
[README](https://github.com/tidwall/tg). 

## Table of contents

- [Programing notes](#programing-notes)
- [Structs](#structs)
- [Objects](#objects)
- [Enums](#enums)
- [Geometry constructors](#geometry-constructors)
- [Geometry accessors](#geometry-accessors)
- [Geometry predicates](#geometry-predicates)
- [Geometry parsing](#geometry-predicates)
- [Geometry writing](#geometry-writing)
- [Geometry with alternative dimensions](#geometry-with-alternative-dimensions)
- [Point functions](#point-functions)
- [Segment functions](#segment-functions)
- [Rectangle functions](#rectangle-functions)
- [Ring functions](#ring-functions)
- [Line functions](#line-functions)
- [Polygon functions](#polygon-functions)
- [Global environment](#global-environment)

## Programing notes

#### Pure functions

TG library functions are thread-safe, reentrant, and (mostly) without side
effects.
The exception being with the use of malloc by some functions like
[geometry constructors](#geometry-constructors).
In those cases, it's the programmer's responsibiilty to check the return value
before continuing.

```c
struct tg_geom *geom = tg_geom_new_point(-112, 33);
if (!geom) {
    // System is out of memory.
}
```

#### Fast cloning

The cloning of geometries, as with [tg_geom_clone()](#tg_geom_clone), are
O(1) operations that use implicit sharing through an atomic reference counter.
Geometry constructors like [tg_geom_new_polygon()](#tg_geom_new_polygon) will
use this method under the hood to maintain references of its inputs.

While this may only be an implementation detail, it's important for the
programmer to understand how TG uses memory and object references.

For example:

```c
struct tg_geom *geom = tg_geom_new_polygon(exterior, holes, nholes);
```

Above, a new geometry "geom" was created and includes a cloned reference to the 
[tg_ring](#tg_ring) "exterior" and all of the holes.

Providing `TG_NOATOMICS` to the compiler will disable the use of atomics and 
instead use non-atomic reference counters.

```
cc -DTG_NOATOMICS tg.c ...
```

Alternatively, the [tg_geom_copy()](#tg_geom_copy) method is available to perform a deep copy of the
geometry.

#### Avoid memory leaks

To avoid memory leaks, call [tg_geom_free()](#tg_geom_free) on geometries
created from [geometry constructors](#geometry-constructors), 
[geometry parsers](#geometry-parsing), [tg_geom_clone()](#tg_geom_clone), and [tg_geom_copy()](#tg_geom_copy).

In other words, for every `tg_geom_new_*()`, `tg_geom_parse_*()`, 
`tg_geom_clone()`, and `tg_geom_copy()` there should be (eventually and exactly)
one `tg_geom_free()`.

#### Upcasting

The TG object types [tg_line](#tg_line), [tg_ring](#tg_ring), and 
[tg_poly](#tg_poly) can be safely upcasted to a [tg_geom](#tg_geom) with no
cost at runtime.

```c
struct tg_geom *geom1 = (struct tg_geom*)line; // Cast tg_line to tg_geom
struct tg_geom *geom2 = (struct tg_geom*)ring; // Cast tg_ring to tg_geom
struct tg_geom *geom3 = (struct tg_geom*)poly; // Cast tg_poly to tg_geom
```

This allows for exposing all [tg_geom](#tg_geom) functions to the other
object types.

In addition, the [tg_ring](#tg_ring) type can also cast to a
[tg_poly](#tg_poly).

```c
struct tg_poly *poly = (struct tg_poly*)ring; // Cast tg_ring to tg_poly
```

*Do not downcast. It's not generally safe to cast from a tg_geom to other
types.*
