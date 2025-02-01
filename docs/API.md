<!--- AUTOMATICALLY GENERATED: DO NOT EDIT --->

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
## Structs

- [tg_point](#structtg__point)
- [tg_segment](#structtg__segment)
- [tg_rect](#structtg__rect)
## Objects

- [tg_ring](#structtg__ring)
- [tg_line](#structtg__line)
- [tg_poly](#structtg__poly)
- [tg_geom](#structtg__geom)
## Enums

- [tg_geom_type](#tg_8h_1a041ea24bc56bb85748f965415a3bd864)
- [tg_index](#tg_8h_1a0dab409fed835315465fcb47a9a926c7)


<a name='group___geometry_constructors'></a>
## Geometry constructors

Functions for creating and freeing geometries. 



- [tg_geom_new_point()](#group___geometry_constructors_1ga5ed750d9db318f6677f85b53e5b2ab2c)
- [tg_geom_new_linestring()](#group___geometry_constructors_1ga2f22a237852eee8821cdf2db95b3c094)
- [tg_geom_new_polygon()](#group___geometry_constructors_1gac6d59362c7ec1971eca36b0ee4089409)
- [tg_geom_new_multipoint()](#group___geometry_constructors_1ga52fc0309986b70f9bb0622c65f801f92)
- [tg_geom_new_multilinestring()](#group___geometry_constructors_1gaccfd80de48f1e5c7de38cbf515d3bf9f)
- [tg_geom_new_multipolygon()](#group___geometry_constructors_1gad57b01099788e21a5103cd8293f4e364)
- [tg_geom_new_geometrycollection()](#group___geometry_constructors_1ga39569ef55606b8d54ed7cf14a4382bdd)
- [tg_geom_new_error()](#group___geometry_constructors_1ga53823d8d5f2f77ba2ac14b4f8a77700c)
- [tg_geom_clone()](#group___geometry_constructors_1ga76f55846aa97188b1840b9ad07dae000)
- [tg_geom_copy()](#group___geometry_constructors_1ga389a890d60908d3e2b448a0e2b3d558e)
- [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae)


<a name='group___geometry_accessors'></a>
## Geometry accessors

Functions for accessing various information about geometries, such as getting the geometry type or extracting underlying components or coordinates. 



- [tg_geom_typeof()](#group___geometry_accessors_1ga7a2e146938fab50268b7798cb3bc91cc)
- [tg_geom_type_string()](#group___geometry_accessors_1ga0435b45df2158200f3b527d1adb11a62)
- [tg_geom_rect()](#group___geometry_accessors_1ga68d67f900b847ae08e6515a620f4f657)
- [tg_geom_is_feature()](#group___geometry_accessors_1ga9bb37a84e3d0f67433309ff1ac3c6be3)
- [tg_geom_is_featurecollection()](#group___geometry_accessors_1gadd8c3b4c29006264c9716fc7b5ebb212)
- [tg_geom_point()](#group___geometry_accessors_1gad8c95d22a1f5900ddc22564575ba687b)
- [tg_geom_line()](#group___geometry_accessors_1ga36d293b2d81c81785fe72c84ff73f861)
- [tg_geom_poly()](#group___geometry_accessors_1ga37415d625c521abbe95ef799b2fd3809)
- [tg_geom_num_points()](#group___geometry_accessors_1gaeb827e40d40c8160973a5cfb880580df)
- [tg_geom_point_at()](#group___geometry_accessors_1ga4f43a8071ffe50ad5dafef4683a48414)
- [tg_geom_num_lines()](#group___geometry_accessors_1gab4081f22ed68c0364a32b8ece9bfd49f)
- [tg_geom_line_at()](#group___geometry_accessors_1gadfc1eecd7d86b2922e96502b6bdebc16)
- [tg_geom_num_polys()](#group___geometry_accessors_1ga6eb2bf473b2672dd5ff00731e358e33e)
- [tg_geom_poly_at()](#group___geometry_accessors_1gae87b48f5a0e34bd5f1c477aea4027508)
- [tg_geom_num_geometries()](#group___geometry_accessors_1ga9012c39360345e8e662ac36137165d14)
- [tg_geom_geometry_at()](#group___geometry_accessors_1ga55f13f898c2236e065e30f3313d48f38)
- [tg_geom_extra_json()](#group___geometry_accessors_1ga91a2927f57c59c0dbe9a9be89e4c93dd)
- [tg_geom_is_empty()](#group___geometry_accessors_1ga7fc477dff52783a69f2a876ca05b8183)
- [tg_geom_dims()](#group___geometry_accessors_1ga32f2c81b2638a85e5209fc093703b847)
- [tg_geom_has_z()](#group___geometry_accessors_1ga50731d93f23947df28bd12ef36f4fce2)
- [tg_geom_has_m()](#group___geometry_accessors_1ga0eb6e450280ba90eeed711c49f7b3cd0)
- [tg_geom_z()](#group___geometry_accessors_1ga706dd7e2dd524ea74f1ce258e133e112)
- [tg_geom_m()](#group___geometry_accessors_1ga4a9e6147ff09959e232106de59ee3feb)
- [tg_geom_extra_coords()](#group___geometry_accessors_1gac5115d2642b03552856865dad33dd494)
- [tg_geom_num_extra_coords()](#group___geometry_accessors_1ga5f85cf4c143703ee227e3c35accbde8b)
- [tg_geom_memsize()](#group___geometry_accessors_1ga4931914f5170cce2949cfdd79e34ef63)
- [tg_geom_search()](#group___geometry_accessors_1gad18cdb2a4ab1fa711dce821a0868ecd2)
- [tg_geom_fullrect()](#group___geometry_accessors_1gac1a077f09e247c022f09e48392b80051)


<a name='group___geometry_predicates'></a>
## Geometry predicates

Functions for testing the spatial relations of two geometries. 



- [tg_geom_equals()](#group___geometry_predicates_1ga87876bf188ea21a55900b497bad436f0)
- [tg_geom_intersects()](#group___geometry_predicates_1gacd094b340fcc39ae01f7e0493bc5f096)
- [tg_geom_disjoint()](#group___geometry_predicates_1ga852863a931d4acb87d26fb7688587804)
- [tg_geom_contains()](#group___geometry_predicates_1gac0edb8e95808d07ac21b1d17cfdd2544)
- [tg_geom_within()](#group___geometry_predicates_1gab4018711f7e427767e4fb9fd58e3afab)
- [tg_geom_covers()](#group___geometry_predicates_1gab50d4126fbefb891fdc387bcfab0b7bb)
- [tg_geom_coveredby()](#group___geometry_predicates_1gabffa9e68db9c9708ad334a4ecd055d0b)
- [tg_geom_touches()](#group___geometry_predicates_1gabc5c6a541f4e553b70432c33128296a4)
- [tg_geom_intersects_rect()](#group___geometry_predicates_1ga9e21b39dd7fdf8338b221ef18dc7dbe6)
- [tg_geom_intersects_xy()](#group___geometry_predicates_1ga5e059ff81a10aab64ec6b5f9af6d5bcf)


<a name='group___geometry_parsing'></a>
## Geometry parsing

Functions for parsing geometries from external data representations. It's recommended to use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors. 



- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)
- [tg_parse_geojsonn()](#group___geometry_parsing_1ga45f669345bccec7184f8a127ada71067)
- [tg_parse_geojson_ix()](#group___geometry_parsing_1gab78f1bd7fede29ebb2ecb9895d1c6bdf)
- [tg_parse_geojsonn_ix()](#group___geometry_parsing_1ga3b11ffa6b82d29245e673d096ac964db)
- [tg_parse_wkt()](#group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c)
- [tg_parse_wktn()](#group___geometry_parsing_1gaae7cb710cdb42db02b398760acd37f84)
- [tg_parse_wkt_ix()](#group___geometry_parsing_1ga0444fdcf8fb4cb754b4a2c1e07f899e5)
- [tg_parse_wktn_ix()](#group___geometry_parsing_1gadf71b642030b63dc399bd2f213a76ed8)
- [tg_parse_wkb()](#group___geometry_parsing_1ga6929035c4b0e606ef84537272d1ece30)
- [tg_parse_wkb_ix()](#group___geometry_parsing_1gae7c77d7de79d8f8023bff15f6ad2b7ae)
- [tg_parse_hex()](#group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67)
- [tg_parse_hexn()](#group___geometry_parsing_1ga2beb47ee201ddbd1a9a7a43c938c4396)
- [tg_parse_hex_ix()](#group___geometry_parsing_1ga8718e134723418426e5df2b13acdf0c1)
- [tg_parse_hexn_ix()](#group___geometry_parsing_1ga3d1f9eb85ff97812fed7cf7bf9e14bd4)
- [tg_parse_geobin()](#group___geometry_parsing_1ga4792bf4f319356fff3fa539cbd44b196)
- [tg_parse_geobin_ix()](#group___geometry_parsing_1gac0450996bcd71cdde81af451dd0e9571)
- [tg_parse()](#group___geometry_parsing_1gaa9e5850bb2cc4eb442c227cd5ac738a1)
- [tg_parse_ix()](#group___geometry_parsing_1gae0bfc62deb68979a46ed62facfee1280)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geobin_fullrect()](#group___geometry_parsing_1gac77e3d8d51a7e66381627cb25853d80f)
- [tg_geobin_rect()](#group___geometry_parsing_1gabf4ef20303d65ccb265ce5359abdfa79)
- [tg_geobin_point()](#group___geometry_parsing_1gab318b6a815f21bb81440ad3edbb61afe)


<a name='group___geometry_writing'></a>
## Geometry writing

Functions for writing geometries as external data representations. 



- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [tg_geom_geobin()](#group___geometry_writing_1gab0f92319db4b8c61c62e0bd735d907b7)


<a name='group___geometry_constructors_ex'></a>
## Geometry with alternative dimensions

Functions for working with geometries that have more than two dimensions or are empty. The extra dimensional coordinates contained within these geometries are only carried along and serve no other purpose than to be available for when it's desired to export to an output representation such as GeoJSON, WKT, or WKB. 



- [tg_geom_new_point_z()](#group___geometry_constructors_ex_1ga1f4b592d4005fea6b9b49b35b0c15e74)
- [tg_geom_new_point_m()](#group___geometry_constructors_ex_1ga68013f90e62307ef0245e8e3398eeda6)
- [tg_geom_new_point_zm()](#group___geometry_constructors_ex_1ga74f8f3f07973ec99af27950735d8bb30)
- [tg_geom_new_point_empty()](#group___geometry_constructors_ex_1ga64d9a692f5f8d7d5ba37fd0b5abdb9a3)
- [tg_geom_new_linestring_z()](#group___geometry_constructors_ex_1ga5b16e072effa305db633265277852a97)
- [tg_geom_new_linestring_m()](#group___geometry_constructors_ex_1ga1141d62a61f6aef6342e02bf930123c0)
- [tg_geom_new_linestring_zm()](#group___geometry_constructors_ex_1gaf7f088fd78eb1340fd91a46f4225d234)
- [tg_geom_new_linestring_empty()](#group___geometry_constructors_ex_1gadbd7a00e18648a3430bf89118448db58)
- [tg_geom_new_polygon_z()](#group___geometry_constructors_ex_1gac81514f9acbafce335d8982233772664)
- [tg_geom_new_polygon_m()](#group___geometry_constructors_ex_1gaf8a608082f6310107d029f25be660991)
- [tg_geom_new_polygon_zm()](#group___geometry_constructors_ex_1ga5b2cbc15b3780925a1cfa05da25e0285)
- [tg_geom_new_polygon_empty()](#group___geometry_constructors_ex_1ga72b1cb66d57ec21a4aef558cd64b647e)
- [tg_geom_new_multipoint_z()](#group___geometry_constructors_ex_1gad7609eb5456dfb084ee7d2ab0637db82)
- [tg_geom_new_multipoint_m()](#group___geometry_constructors_ex_1gaae2123bcb6b30a5efeae422ac4a4aec6)
- [tg_geom_new_multipoint_zm()](#group___geometry_constructors_ex_1ga772ab89b63583177d537615f1bd2adb5)
- [tg_geom_new_multipoint_empty()](#group___geometry_constructors_ex_1ga967114d5e9332630c7f99ac6a116991d)
- [tg_geom_new_multilinestring_z()](#group___geometry_constructors_ex_1ga9a855e4d8aeb386b3fcdf2f35da4bb53)
- [tg_geom_new_multilinestring_m()](#group___geometry_constructors_ex_1gad53b72d80be834a14a9a47d0953f08c3)
- [tg_geom_new_multilinestring_zm()](#group___geometry_constructors_ex_1ga5ff87656da4c72c947cef312acb50ef1)
- [tg_geom_new_multilinestring_empty()](#group___geometry_constructors_ex_1ga2798822f3fdb20132a6cc6ad2247fae4)
- [tg_geom_new_multipolygon_z()](#group___geometry_constructors_ex_1ga46c67962e6d151e46cc961a76e90941c)
- [tg_geom_new_multipolygon_m()](#group___geometry_constructors_ex_1ga824ac7a05f0cc5e4a74d9335f23954a0)
- [tg_geom_new_multipolygon_zm()](#group___geometry_constructors_ex_1ga998ef169fc03e82c75fe32757b057334)
- [tg_geom_new_multipolygon_empty()](#group___geometry_constructors_ex_1ga48684027f9beb519c52575bf6754a6f8)
- [tg_geom_new_geometrycollection_empty()](#group___geometry_constructors_ex_1ga15fc542f3f6b4bb4dc7e91d554e0dfff)


<a name='group___point_funcs'></a>
## Point functions

Functions for working directly with the [tg_point](#structtg__point) type. 



- [tg_point_rect()](#group___point_funcs_1ga07ff1afb45368e2f511da5cb8e689943)
- [tg_point_intersects_rect()](#group___point_funcs_1ga59e99021c916a5fe7a350aee695ef6d3)


<a name='group___segment_funcs'></a>
## Segment functions

Functions for working directly with the [tg_segment](#structtg__segment) type. 



- [tg_segment_rect()](#group___segment_funcs_1ga8218b7694fe5079da6ce0a241991cf78)
- [tg_segment_intersects_segment()](#group___segment_funcs_1ga12b1ec6c72d4120391daff364d5c7b9b)


<a name='group___rect_funcs'></a>
## Rectangle functions

Functions for working directly with the [tg_rect](#structtg__rect) type. 



- [tg_rect_expand()](#group___rect_funcs_1gadc227cfc279d612269a142841d736c79)
- [tg_rect_expand_point()](#group___rect_funcs_1gaad0df238f6f430d9cc6fd6b0c6807931)
- [tg_rect_center()](#group___rect_funcs_1ga44be4970fbbbdfa9e2ceb338a938d262)
- [tg_rect_intersects_rect()](#group___rect_funcs_1ga910f2b37e7c7b17f9c01bbddee871b11)
- [tg_rect_intersects_point()](#group___rect_funcs_1gac4d6e61cc5260ed913c736fd71defe05)


<a name='group___ring_funcs'></a>
## Ring functions

Functions for working directly with the [tg_ring](#structtg__ring) type.

There are no direct spatial predicates for [tg_ring](#structtg__ring). If you want to perform operations like "intersects" or "covers" then you must upcast the ring to a [tg_geom](#structtg__geom), like such:

```c
tg_geom_intersects((struct tg_geom*)ring, geom);
```
 



- [tg_ring_new()](#group___ring_funcs_1ga7defe1b6d43be8285a73f7977b342661)
- [tg_ring_new_ix()](#group___ring_funcs_1ga9a154a6a8dc4beaf7758ef39b21c1ce8)
- [tg_ring_free()](#group___ring_funcs_1ga51e9d824d50f67f89981f0edc7fe0cc5)
- [tg_ring_clone()](#group___ring_funcs_1gafda24e3f1274ae1f1421934722cfd67b)
- [tg_ring_copy()](#group___ring_funcs_1gaaa7b50bc974357abe9d67ec8d6ca0d91)
- [tg_ring_memsize()](#group___ring_funcs_1ga0f146af1880d1ed7d02bfbdd0298feac)
- [tg_ring_rect()](#group___ring_funcs_1ga64086c935039b1f0fcd8633be763427d)
- [tg_ring_num_points()](#group___ring_funcs_1ga2437033fd4a6d67a54ce2cf1f771b66c)
- [tg_ring_point_at()](#group___ring_funcs_1gad2450a013edd99a013dbaee437fb0c3d)
- [tg_ring_points()](#group___ring_funcs_1gaf97a197de54c13d01aa9d983115a336d)
- [tg_ring_num_segments()](#group___ring_funcs_1ga56dcd7ca3d210696979c3c6c62d71690)
- [tg_ring_segment_at()](#group___ring_funcs_1ga52b3ba9138aee4245fca05bedd00801d)
- [tg_ring_convex()](#group___ring_funcs_1ga2559c1e9f0a3b48d97b36010ac424703)
- [tg_ring_clockwise()](#group___ring_funcs_1gae677767bd4bc40480f0a703e78f2e713)
- [tg_ring_index_spread()](#group___ring_funcs_1gacecc3997e0cabb5532946acffee834f0)
- [tg_ring_index_num_levels()](#group___ring_funcs_1ga1bbaf39cc96d727f253c0f66c0ede4e4)
- [tg_ring_index_level_num_rects()](#group___ring_funcs_1ga292a911cdb05f7dbc19bcbd031226deb)
- [tg_ring_index_level_rect()](#group___ring_funcs_1ga7eab00bc99a6f4148d2e6ced95ff24c3)
- [tg_ring_nearest_segment()](#group___ring_funcs_1ga716e10054b4bda84efb259d57aff5015)
- [tg_ring_line_search()](#group___ring_funcs_1gabee40f4a66c2ebb4516a9f980ff5d998)
- [tg_ring_ring_search()](#group___ring_funcs_1gacd2c483213d8110c9373e8e47bd9f49e)
- [tg_ring_area()](#group___ring_funcs_1gabe14408b3ad596ed14b4ce698c2e7826)
- [tg_ring_perimeter()](#group___ring_funcs_1ga0d063ecfef895e21f6d7520db05d302e)


<a name='group___line_funcs'></a>
## Line functions

Functions for working directly with the [tg_line](#structtg__line) type.

There are no direct spatial predicates for [tg_line](#structtg__line). If you want to perform operations like "intersects" or "covers" then you must upcast the line to a [tg_geom](#structtg__geom), like such:

```c
tg_geom_intersects((struct tg_geom*)line, geom);
```
 



- [tg_line_new()](#group___line_funcs_1ga7f834a7213c8d87d8b7c7117bdf8bf63)
- [tg_line_new_ix()](#group___line_funcs_1ga4057ac4ba72aa24985d947fa0ba42c19)
- [tg_line_free()](#group___line_funcs_1ga13b2a0ed2014525d613c25c9232affd7)
- [tg_line_clone()](#group___line_funcs_1gac7cbf109cca2dffba781a47932e1411d)
- [tg_line_copy()](#group___line_funcs_1ga3a5d0aac59658eb2cfd8cdaea8ebf22f)
- [tg_line_memsize()](#group___line_funcs_1ga6c2476d9dd302a3dea68aa44e7241ab1)
- [tg_line_rect()](#group___line_funcs_1ga2e59f4517937e89ae69ac6c1f12bf797)
- [tg_line_num_points()](#group___line_funcs_1gab4483ad3c5418dd9674b1f03498d18e6)
- [tg_line_points()](#group___line_funcs_1ga45a892019af11582b2761301526519d9)
- [tg_line_point_at()](#group___line_funcs_1ga44955f7b0ee3bd13a33b24aeca01651a)
- [tg_line_num_segments()](#group___line_funcs_1gadb56b68cc4ce05cfa832e82e45995687)
- [tg_line_segment_at()](#group___line_funcs_1gad0c0982aff76d943f0ce50deb0f9183d)
- [tg_line_clockwise()](#group___line_funcs_1gaee0fcc56c7518fa81ebb3457d795ca49)
- [tg_line_index_spread()](#group___line_funcs_1ga8b851e64231b8b30f30a936cf35cde08)
- [tg_line_index_num_levels()](#group___line_funcs_1ga0cbcfec6e6ab81f31a47cd542e32b778)
- [tg_line_index_level_num_rects()](#group___line_funcs_1gab9d4afd57889a11179df01c1df26488b)
- [tg_line_index_level_rect()](#group___line_funcs_1gacc18e4ddb48ccc717f61f298fd6ae970)
- [tg_line_nearest_segment()](#group___line_funcs_1gaa85835e9619ba7d8557fda3335a0353a)
- [tg_line_line_search()](#group___line_funcs_1gafef8ca6e5381e93d4c4952343597c0e1)
- [tg_line_length()](#group___line_funcs_1ga994796f0088c82bb0672d84f1a1ce9d1)


<a name='group___poly_funcs'></a>
## Polygon functions

Functions for working directly with the [tg_poly](#structtg__poly) type.

There are no direct spatial predicates for [tg_poly](#structtg__poly). If you want to perform operations like "intersects" or "covers" then you must upcast the poly to a [tg_geom](#structtg__geom), like such:

```c
tg_geom_intersects((struct tg_geom*)poly, geom);
```
 



- [tg_poly_new()](#group___poly_funcs_1ga56c2615488ca202baa944c85c20a40f1)
- [tg_poly_free()](#group___poly_funcs_1ga7a591cc2298e4e24ae0e99b055a61fed)
- [tg_poly_clone()](#group___poly_funcs_1ga2f01b5838bcb3669e3aacace12062fbf)
- [tg_poly_copy()](#group___poly_funcs_1gae68cadbe9ae2fa26c92f010958c6ed11)
- [tg_poly_memsize()](#group___poly_funcs_1gabccc5c197d02868b9d24df7b7cabeea4)
- [tg_poly_exterior()](#group___poly_funcs_1ga802bb035aa8b2efd234973b98eefb128)
- [tg_poly_num_holes()](#group___poly_funcs_1ga23934cf6af1048e2e4b06befef3f8c1f)
- [tg_poly_hole_at()](#group___poly_funcs_1gad15090afe596c268858c65b5c5e47a73)
- [tg_poly_rect()](#group___poly_funcs_1gaac2de96544c41331886fd2b48e11ba6d)
- [tg_poly_clockwise()](#group___poly_funcs_1gac52d19818b926e8d9e70abb0e3889da8)


<a name='group___global_funcs'></a>
## Global environment

Functions for optionally setting the behavior of the TG environment. These, if desired, should be called only once at program start up and prior to calling any other tg_*() functions. 



- [tg_env_set_allocator()](#group___global_funcs_1gab1e1478a3870e90d6b5932f5e67a032b)
- [tg_env_set_index()](#group___global_funcs_1ga57a922edb770400033043354c1f4e80e)
- [tg_env_set_index_spread()](#group___global_funcs_1gaf9e9214a8db08c306fdb529192e9dd5f)
- [tg_env_set_print_fixed_floats()](#group___global_funcs_1gad9af7b45fd9c942f857b1121168f1600)

<a name='structtg__point'></a>
## tg_point
```c
struct tg_point {
    double x;
    double y;
};
```
The base point type used for all geometries. 

**See also**

- [Point functions](#group___point_funcs)



<a name='structtg__segment'></a>
## tg_segment
```c
struct tg_segment {
    struct tg_point a;
    struct tg_point b;
};
```
The base segment type used in [tg_line](#structtg__line) and [tg_ring](#structtg__ring) for joining two vertices. 

**See also**

- [Segment functions](#group___segment_funcs)



<a name='structtg__rect'></a>
## tg_rect
```c
struct tg_rect {
    struct tg_point min;
    struct tg_point max;
};
```
A rectangle defined by a minimum and maximum coordinates. Returned by the [tg_geom_rect()](#group___geometry_accessors_1ga68d67f900b847ae08e6515a620f4f657), [tg_ring_rect()](#group___ring_funcs_1ga64086c935039b1f0fcd8633be763427d), and other *_rect() functions for getting a geometry's minumum bounding rectangle. Also used internally for geometry indexing. 

**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='structtg__ring'></a>
## tg_ring
```c
struct tg_ring;
```
A ring is series of [tg_segment](#structtg__segment) which creates a shape that does not self-intersect and is fully closed, where the start and end points are the exact same value.

**Creating**

To create a new ring use the [tg_ring_new()](#group___ring_funcs_1ga7defe1b6d43be8285a73f7977b342661) function.

```c
struct tg_ring *ring = tg_ring_new(points, npoints);
```


**Upcasting**

A [tg_ring](#structtg__ring) can always be safely upcasted to a [tg_poly](#structtg__poly) or [tg_geom](#structtg__geom); allowing it to use any tg_poly_&ast;() or tg_geom_&ast;() function.

```c
struct tg_poly *poly = (struct tg_poly*)ring; // Cast to a tg_poly
struct tg_geom *geom = (struct tg_geom*)ring; // Cast to a tg_geom
```
 

**See also**

- [Ring functions](#group___ring_funcs)
- [Polygon functions](#group___poly_funcs)



<a name='structtg__line'></a>
## tg_line
```c
struct tg_line;
```
A line is a series of [tg_segment](#structtg__segment) that make up a linestring.

**Creating**

To create a new line use the [tg_line_new()](#group___line_funcs_1ga7f834a7213c8d87d8b7c7117bdf8bf63) function.

```c
struct tg_line *line = tg_line_new(points, npoints);
```


**Upcasting**

A [tg_line](#structtg__line) can always be safely upcasted to a [tg_geom](#structtg__geom); allowing it to use any tg_geom_&ast;() function.

```c
struct tg_geom *geom = (struct tg_geom*)line; // Cast to a tg_geom
```




**See also**

- [Line functions](#group___line_funcs)



<a name='structtg__poly'></a>
## tg_poly
```c
struct tg_poly;
```
A polygon consists of one exterior ring and zero or more holes.

**Creating**

To create a new polygon use the [tg_poly_new()](#group___poly_funcs_1ga56c2615488ca202baa944c85c20a40f1) function.

```c
struct tg_poly *poly = tg_poly_new(exterior, holes, nholes);
```


**Upcasting**

A [tg_poly](#structtg__poly) can always be safely upcasted to a [tg_geom](#structtg__geom); allowing it to use any tg_geom_&ast;() function.

```c
struct tg_geom *geom = (struct tg_geom*)poly; // Cast to a tg_geom
```




**See also**

- [Polygon functions](#group___poly_funcs)



<a name='structtg__geom'></a>
## tg_geom
```c
struct tg_geom;
```
A geometry is the common generic type that can represent a Point, LineString, Polygon, MultiPoint, MultiLineString, MultiPolygon, or GeometryCollection.

For geometries that are derived from GeoJSON, they may have addtional attributes such as being a Feature or a FeatureCollection; or include extra json fields.

**Creating**

To create a new geometry use one of the [Geometry constructors](#group___geometry_constructors) or [Geometry parsing](#group___geometry_parsing) functions.

```c
struct tg_geom *geom = tg_geom_new_point(point);
struct tg_geom *geom = tg_geom_new_polygon(poly);
struct tg_geom *geom = tg_parse_geojson(geojson);
```


**Upcasting**

Other types, specifically [tg_line](#structtg__line), [tg_ring](#structtg__ring), and [tg_poly](#structtg__poly), can be safely upcasted to a [tg_geom](#structtg__geom); allowing them to use any tg_geom_&ast;() function.

```c
struct tg_geom *geom1 = (struct tg_geom*)line; // Cast to a LineString
struct tg_geom *geom2 = (struct tg_geom*)ring; // Cast to a Polygon
struct tg_geom *geom3 = (struct tg_geom*)poly; // Cast to a Polygon
```




**See also**

- [Geometry constructors](#group___geometry_constructors)
- [Geometry accessors](#group___geometry_accessors)
- [Geometry predicates](#group___geometry_predicates)
- [Geometry parsing](#group___geometry_parsing)
- [Geometry writing](#group___geometry_writing)



<a name='tg_8h_1a041ea24bc56bb85748f965415a3bd864'></a>
## tg_geom_type
```c
enum tg_geom_type {
    TG_POINT              = 1, // Point.
    TG_LINESTRING         = 2, // LineString.
    TG_POLYGON            = 3, // Polygon.
    TG_MULTIPOINT         = 4, // MultiPoint, collection of points.
    TG_MULTILINESTRING    = 5, // MultiLineString, collection of linestrings.
    TG_MULTIPOLYGON       = 6, // MultiPolygon, collection of polygons.
    TG_GEOMETRYCOLLECTION = 7, // GeometryCollection, collection of geometries.
};
```
Geometry types.

All [tg_geom](#structtg__geom) are one of the following underlying types.



**See also**

- [tg_geom_typeof()](#group___geometry_accessors_1ga7a2e146938fab50268b7798cb3bc91cc)
- [tg_geom_type_string()](#group___geometry_accessors_1ga0435b45df2158200f3b527d1adb11a62)
- [Geometry accessors](#group___geometry_accessors)



<a name='tg_8h_1a0dab409fed835315465fcb47a9a926c7'></a>
## tg_index
```c
enum tg_index {
    TG_DEFAULT,  // default is TG_NATURAL or tg_env_set_default_index().
    TG_NONE,     // no indexing available, or disabled.
    TG_NATURAL,  // indexing with natural ring order, for rings/lines
    TG_YSTRIPES, // indexing using segment striping, rings only
};
```
Geometry indexing options.

Used for polygons, rings, and lines to make the point-in-polygon and geometry intersection operations fast.

An index can also be used for efficiently traversing, searching, and performing nearest-neighbor (kNN) queries on the segment using tg_ring_index_*() and tg_ring_nearest() functions. 


<a name='group___geometry_constructors_1ga5ed750d9db318f6677f85b53e5b2ab2c'></a>
## tg_geom_new_point()
```c
struct tg_geom *tg_geom_new_point(struct tg_point point);
```
Creates a Point geometry. 

**Parameters**

- **point**: Input point



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1ga2f22a237852eee8821cdf2db95b3c094'></a>
## tg_geom_new_linestring()
```c
struct tg_geom *tg_geom_new_linestring(const struct tg_line *line);
```
Creates a LineString geometry. 

**Parameters**

- **line**: Input line, caller retains ownership.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1gac6d59362c7ec1971eca36b0ee4089409'></a>
## tg_geom_new_polygon()
```c
struct tg_geom *tg_geom_new_polygon(const struct tg_poly *poly);
```
Creates a Polygon geometry. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1ga52fc0309986b70f9bb0622c65f801f92'></a>
## tg_geom_new_multipoint()
```c
struct tg_geom *tg_geom_new_multipoint(const struct tg_point *points, int npoints);
```
Creates a MultiPoint geometry. 

**Parameters**

- **points**: An array of points, caller retains ownership.
- **npoints**: The number of points in array



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1gaccfd80de48f1e5c7de38cbf515d3bf9f'></a>
## tg_geom_new_multilinestring()
```c
struct tg_geom *tg_geom_new_multilinestring(const struct tg_line *const lines[], int nlines);
```
Creates a MultiLineString geometry. 

**Parameters**

- **lines**: An array of lines, caller retains ownership.
- **nlines**: The number of lines in array



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1gad57b01099788e21a5103cd8293f4e364'></a>
## tg_geom_new_multipolygon()
```c
struct tg_geom *tg_geom_new_multipolygon(const struct tg_poly *const polys[], int npolys);
```
Creates a MultiPolygon geometry. 

**Parameters**

- **polys**: An array of polygons, caller retains ownership.
- **npolys**: The number of polygons in array



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1ga39569ef55606b8d54ed7cf14a4382bdd'></a>
## tg_geom_new_geometrycollection()
```c
struct tg_geom *tg_geom_new_geometrycollection(const struct tg_geom *const geoms[], int ngeoms);
```
Creates a GeometryCollection geometry. 

**Parameters**

- **geoms**: An array of geometries, caller retains ownership.
- **ngeoms**: The number of geometries in array



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1ga53823d8d5f2f77ba2ac14b4f8a77700c'></a>
## tg_geom_new_error()
```c
struct tg_geom *tg_geom_new_error(const char *errmsg);
```
Utility for returning an error message wrapped in a geometry. This operation does not return a real geometry, only an error message, which may be useful for generating custom errors from operations outside of the TG library. 


<a name='group___geometry_constructors_1ga76f55846aa97188b1840b9ad07dae000'></a>
## tg_geom_clone()
```c
struct tg_geom *tg_geom_clone(const struct tg_geom *geom);
```
Clones a geometry 

**Parameters**

- **geom**: Input geometry, caller retains ownership.



**Return**

- A duplicate of the provided geometry.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).
- This method of cloning uses implicit sharing through an atomic reference counter.


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1ga389a890d60908d3e2b448a0e2b3d558e'></a>
## tg_geom_copy()
```c
struct tg_geom *tg_geom_copy(const struct tg_geom *geom);
```
Copies a geometry 

**Parameters**

- **geom**: Input geometry, caller retains ownership.



**Return**

- A duplicate of the provided geometry.
- NULL if out of memory


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).
- This method performs a deep copy of the entire geometry to new memory.


**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae'></a>
## tg_geom_free()
```c
void tg_geom_free(struct tg_geom *geom);
```
Releases the memory associated with a geometry. 

**Parameters**

- **geom**: Input geometry



**See also**

- [Geometry constructors](#group___geometry_constructors)



<a name='group___geometry_accessors_1ga7a2e146938fab50268b7798cb3bc91cc'></a>
## tg_geom_typeof()
```c
enum tg_geom_type tg_geom_typeof(const struct tg_geom *geom);
```
Returns the geometry type. e.g. TG_POINT, TG_POLYGON, TG_LINESTRING 

**Parameters**

- **geom**: Input geometry



**Return**

- The geometry type


**See also**

- [tg_geom_type](.#tg_geom_type)
- [tg_geom_type_string()](#group___geometry_accessors_1ga0435b45df2158200f3b527d1adb11a62)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga0435b45df2158200f3b527d1adb11a62'></a>
## tg_geom_type_string()
```c
const char *tg_geom_type_string(enum tg_geom_type type);
```
Get the string representation of a geometry type. e.g. "Point", "Polygon", "LineString". 

**Parameters**

- **type**: Input geometry type



**Return**

- A string representing the type


**Note**

- The returned string does not need to be freed.


**See also**

- [tg_geom_typeof()](#group___geometry_accessors_1ga7a2e146938fab50268b7798cb3bc91cc)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga68d67f900b847ae08e6515a620f4f657'></a>
## tg_geom_rect()
```c
struct tg_rect tg_geom_rect(const struct tg_geom *geom);
```
Returns the minimum bounding rectangle of a geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- Minumum bounding rectangle


**See also**

- [tg_rect](#structtg__rect)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga9bb37a84e3d0f67433309ff1ac3c6be3'></a>
## tg_geom_is_feature()
```c
bool tg_geom_is_feature(const struct tg_geom *geom);
```
Returns true if the geometry is a GeoJSON Feature. 

**Parameters**

- **geom**: Input geometry



**Return**

- True or false


**See also**

- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gadd8c3b4c29006264c9716fc7b5ebb212'></a>
## tg_geom_is_featurecollection()
```c
bool tg_geom_is_featurecollection(const struct tg_geom *geom);
```
Returns true if the geometry is a GeoJSON FeatureCollection. 

**Parameters**

- **geom**: Input geometry



**Return**

- True or false


**See also**

- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gad8c95d22a1f5900ddc22564575ba687b'></a>
## tg_geom_point()
```c
struct tg_point tg_geom_point(const struct tg_geom *geom);
```
Returns the underlying point for the provided geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_POINT geometry, returns the point.
- For everything else returns the center of the geometry's bounding rectangle.


**See also**

- [tg_point](#structtg__point)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga36d293b2d81c81785fe72c84ff73f861'></a>
## tg_geom_line()
```c
const struct tg_line *tg_geom_line(const struct tg_geom *geom);
```
Returns the underlying line for the provided geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_LINESTRING geometry, returns the line.
- For everything else returns NULL.


**See also**

- [tg_line](#structtg__line)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga37415d625c521abbe95ef799b2fd3809'></a>
## tg_geom_poly()
```c
const struct tg_poly *tg_geom_poly(const struct tg_geom *geom);
```
Returns the underlying polygon for the provided geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_POLYGON geometry, returns the polygon.
- For everything else returns NULL.


**See also**

- [tg_poly](#structtg__poly)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gaeb827e40d40c8160973a5cfb880580df'></a>
## tg_geom_num_points()
```c
int tg_geom_num_points(const struct tg_geom *geom);
```
Returns the number of points in a MultiPoint geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_MULTIPOINT geometry, returns the number of points.
- For everything else returns zero.


**See also**

- [tg_geom_point_at()](#group___geometry_accessors_1ga4f43a8071ffe50ad5dafef4683a48414)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga4f43a8071ffe50ad5dafef4683a48414'></a>
## tg_geom_point_at()
```c
struct tg_point tg_geom_point_at(const struct tg_geom *geom, int index);
```
Returns the point at index for a MultiPoint geometry. 

**Parameters**

- **geom**: Input geometry
- **index**: Index of point



**Return**

- The point at index. Returns an empty point if the geometry type is not TG_MULTIPOINT or when the provided index is out of range.


**See also**

- [tg_geom_num_points()](#group___geometry_accessors_1gaeb827e40d40c8160973a5cfb880580df)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gab4081f22ed68c0364a32b8ece9bfd49f'></a>
## tg_geom_num_lines()
```c
int tg_geom_num_lines(const struct tg_geom *geom);
```
Returns the number of lines in a MultiLineString geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_MULTILINESTRING geometry, returns the number of lines.
- For everything else returns zero.


**See also**

- [tg_geom_line_at()](#group___geometry_accessors_1gadfc1eecd7d86b2922e96502b6bdebc16)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gadfc1eecd7d86b2922e96502b6bdebc16'></a>
## tg_geom_line_at()
```c
const struct tg_line *tg_geom_line_at(const struct tg_geom *geom, int index);
```
Returns the line at index for a MultiLineString geometry. 

**Parameters**

- **geom**: Input geometry
- **index**: Index of line



**Return**

- The line at index. Returns NULL if the geometry type is not TG_MULTILINE or when the provided index is out of range.


**See also**

- [tg_geom_num_lines()](#group___geometry_accessors_1gab4081f22ed68c0364a32b8ece9bfd49f)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga6eb2bf473b2672dd5ff00731e358e33e'></a>
## tg_geom_num_polys()
```c
int tg_geom_num_polys(const struct tg_geom *geom);
```
Returns the number of polygons in a MultiPolygon geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_MULTIPOLYGON geometry, returns the number of polygons.
- For everything else returns zero.


**See also**

- [tg_geom_poly_at()](#group___geometry_accessors_1gae87b48f5a0e34bd5f1c477aea4027508)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gae87b48f5a0e34bd5f1c477aea4027508'></a>
## tg_geom_poly_at()
```c
const struct tg_poly *tg_geom_poly_at(const struct tg_geom *geom, int index);
```
Returns the polygon at index for a MultiPolygon geometry. 

**Parameters**

- **geom**: Input geometry
- **index**: Index of polygon



**Return**

- The polygon at index. Returns NULL if the geometry type is not TG_MULTIPOLYGON or when the provided index is out of range.


**See also**

- [tg_geom_num_polys()](#group___geometry_accessors_1ga6eb2bf473b2672dd5ff00731e358e33e)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga9012c39360345e8e662ac36137165d14'></a>
## tg_geom_num_geometries()
```c
int tg_geom_num_geometries(const struct tg_geom *geom);
```
Returns the number of geometries in a GeometryCollection geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_MULTIGEOMETRY geometry, returns the number of geometries.
- For everything else returns zero.


**Note**

- A geometry that is a GeoJSON FeatureCollection can use this function to get number features in its collection.


**See also**

- [tg_geom_geometry_at()](#group___geometry_accessors_1ga55f13f898c2236e065e30f3313d48f38)
- [tg_geom_is_featurecollection()](#group___geometry_accessors_1gadd8c3b4c29006264c9716fc7b5ebb212)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga55f13f898c2236e065e30f3313d48f38'></a>
## tg_geom_geometry_at()
```c
const struct tg_geom *tg_geom_geometry_at(const struct tg_geom *geom, int index);
```
Returns the geometry at index for a GeometryCollection geometry. 

**Parameters**

- **geom**: Input geometry
- **index**: Index of geometry



**Return**

- For a TG_MULTIGEOMETRY geometry, returns the number of geometries.
- For everything else returns zero.


**Note**

- A geometry that is a GeoJSON FeatureCollection can use this function to get number features in its collection.


**See also**

- [tg_geom_geometry_at()](#group___geometry_accessors_1ga55f13f898c2236e065e30f3313d48f38)
- [tg_geom_is_featurecollection()](#group___geometry_accessors_1gadd8c3b4c29006264c9716fc7b5ebb212)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1ga91a2927f57c59c0dbe9a9be89e4c93dd'></a>
## tg_geom_extra_json()
```c
const char *tg_geom_extra_json(const struct tg_geom *geom);
```
Returns a string that represents any extra JSON from a parsed GeoJSON geometry. Such as the "id" or "properties" fields. 

**Parameters**

- **geom**: Input geometry



**Return**

- Returns a valid JSON object as a string, or NULL if the geometry did not come from GeoJSON or there is no extra JSON.


**Note**

- The returned string does not need to be freed.


**See also**

- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)



<a name='group___geometry_accessors_1ga7fc477dff52783a69f2a876ca05b8183'></a>
## tg_geom_is_empty()
```c
bool tg_geom_is_empty(const struct tg_geom *geom);
```
Tests whether a geometry is empty. An empty geometry is one that has no interior boundary. 

**Parameters**

- **geom**: Input geometry



**Return**

- True or false



<a name='group___geometry_accessors_1ga32f2c81b2638a85e5209fc093703b847'></a>
## tg_geom_dims()
```c
int tg_geom_dims(const struct tg_geom *geom);
```
Get the number of dimensions for a geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- 2 for standard geometries
- 3 when geometry has Z or M coordinates
- 4 when geometry has Z and M coordinates
- 0 when input is NULL



<a name='group___geometry_accessors_1ga50731d93f23947df28bd12ef36f4fce2'></a>
## tg_geom_has_z()
```c
bool tg_geom_has_z(const struct tg_geom *geom);
```
Tests whether a geometry has Z coordinates. 

**Parameters**

- **geom**: Input geometry



**Return**

- True or false



<a name='group___geometry_accessors_1ga0eb6e450280ba90eeed711c49f7b3cd0'></a>
## tg_geom_has_m()
```c
bool tg_geom_has_m(const struct tg_geom *geom);
```
Tests whether a geometry has M coordinates. 

**Parameters**

- **geom**: Input geometry



**Return**

- True or false



<a name='group___geometry_accessors_1ga706dd7e2dd524ea74f1ce258e133e112'></a>
## tg_geom_z()
```c
double tg_geom_z(const struct tg_geom *geom);
```
Get the Z coordinate of a Point geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_POINT geometry, returns the Z coodinate.
- For everything else returns zero.



<a name='group___geometry_accessors_1ga4a9e6147ff09959e232106de59ee3feb'></a>
## tg_geom_m()
```c
double tg_geom_m(const struct tg_geom *geom);
```
Get the M coordinate of a Point geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- For a TG_POINT geometry, returns the M coodinate.
- For everything else returns zero.



<a name='group___geometry_accessors_1gac5115d2642b03552856865dad33dd494'></a>
## tg_geom_extra_coords()
```c
const double *tg_geom_extra_coords(const struct tg_geom *geom);
```
Get the extra coordinates for a geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- Array of coordinates
- NULL if there are no extra coordinates


**Note**

- These are the raw coodinates provided by a constructor like [tg_geom_new_polygon_z()](#group___geometry_constructors_ex_1gac81514f9acbafce335d8982233772664) or from a parsed source like WKT "POLYGON Z ...".


**See also**

- [tg_geom_num_extra_coords()](#group___geometry_accessors_1ga5f85cf4c143703ee227e3c35accbde8b)



<a name='group___geometry_accessors_1ga5f85cf4c143703ee227e3c35accbde8b'></a>
## tg_geom_num_extra_coords()
```c
int tg_geom_num_extra_coords(const struct tg_geom *geom);
```
Get the number of extra coordinates for a geometry 

**Parameters**

- **geom**: Input geometry



**Return**

- The number of extra coordinates, or zero if none.


**See also**

- [tg_geom_extra_coords()](#group___geometry_accessors_1gac5115d2642b03552856865dad33dd494)



<a name='group___geometry_accessors_1ga4931914f5170cce2949cfdd79e34ef63'></a>
## tg_geom_memsize()
```c
size_t tg_geom_memsize(const struct tg_geom *geom);
```
Returns the allocation size of the geometry. 

**Parameters**

- **geom**: Input geometry



**Return**

- Size of geometry in bytes



<a name='group___geometry_accessors_1gad18cdb2a4ab1fa711dce821a0868ecd2'></a>
## tg_geom_search()
```c
void tg_geom_search(const struct tg_geom *geom, struct tg_rect rect, bool(*iter)(const struct tg_geom *geom, int index, void *udata), void *udata);
```
Iterates over all child geometries in geom that intersect rect 

**Note**

- Only iterates over collection types: TG_MULTIPOINT, TG_MULTILINESTRING, TG_MULTIPOLYGON, and TG_GEOMETRYCOLLECTION.
- A GeoJSON FeatureCollection works as well.


**See also**

- [tg_geom_typeof()](#group___geometry_accessors_1ga7a2e146938fab50268b7798cb3bc91cc)
- [Geometry accessors](#group___geometry_accessors)



<a name='group___geometry_accessors_1gac1a077f09e247c022f09e48392b80051'></a>
## tg_geom_fullrect()
```c
int tg_geom_fullrect(const struct tg_geom *geom, double min[4], double max[4]);
```
Returns the minimum bounding rectangle of a geometry on all dimensions. 

**Parameters**

- **geom**: Input geometry
- **min**: min values, must have room for 4 dimensions
- **max**: max values, must have room for 4 dimensions



**Return**

- number of dimensions, or zero if invalid geom.


**See also**

- [tg_geom_rect()](#group___geometry_accessors_1ga68d67f900b847ae08e6515a620f4f657)



<a name='group___geometry_predicates_1ga87876bf188ea21a55900b497bad436f0'></a>
## tg_geom_equals()
```c
bool tg_geom_equals(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether two geometries are topologically equal. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gacd094b340fcc39ae01f7e0493bc5f096'></a>
## tg_geom_intersects()
```c
bool tg_geom_intersects(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether two geometries intersect. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1ga852863a931d4acb87d26fb7688587804'></a>
## tg_geom_disjoint()
```c
bool tg_geom_disjoint(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether 'a' and 'b' have no point in common, and are fully disconnected geometries. 

**Note**

- Works the same as `!tg_geom_intersects(a, b)`


**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gac0edb8e95808d07ac21b1d17cfdd2544'></a>
## tg_geom_contains()
```c
bool tg_geom_contains(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether 'a' contains 'b', and 'b' is not touching the boundary of 'a'. 

**Note**

- Works the same as `tg_geom_within(b, a)`


**Warning**

- This predicate returns **false** when geometry 'b' is *on* or *touching* the boundary of geometry 'a'. Such as when a point is on the edge of a polygon. 
 For full coverage, consider using [tg_geom_covers](#group___geometry_predicates_1gab50d4126fbefb891fdc387bcfab0b7bb).


**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gab4018711f7e427767e4fb9fd58e3afab'></a>
## tg_geom_within()
```c
bool tg_geom_within(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether 'a' is contained inside of 'b' and not touching the boundary of 'b'. 

**Note**

- Works the same as `tg_geom_contains(b, a)`


**Warning**

- This predicate returns **false** when geometry 'a' is *on* or *touching* the boundary of geometry 'b'. Such as when a point is on the edge of a polygon. 
 For full coverage, consider using [tg_geom_coveredby](#group___geometry_predicates_1gabffa9e68db9c9708ad334a4ecd055d0b).


**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gab50d4126fbefb891fdc387bcfab0b7bb'></a>
## tg_geom_covers()
```c
bool tg_geom_covers(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether a geometry 'a' fully contains geometry 'b'. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gabffa9e68db9c9708ad334a4ecd055d0b'></a>
## tg_geom_coveredby()
```c
bool tg_geom_coveredby(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether 'a' is fully contained inside of 'b'. 

**Note**

- Works the same as `tg_geom_covers(b, a)`


**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1gabc5c6a541f4e553b70432c33128296a4'></a>
## tg_geom_touches()
```c
bool tg_geom_touches(const struct tg_geom *a, const struct tg_geom *b);
```
Tests whether a geometry 'a' touches 'b'. They have at least one point in common, but their interiors do not intersect. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1ga9e21b39dd7fdf8338b221ef18dc7dbe6'></a>
## tg_geom_intersects_rect()
```c
bool tg_geom_intersects_rect(const struct tg_geom *a, struct tg_rect b);
```
Tests whether a geometry intersects a rect. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_predicates_1ga5e059ff81a10aab64ec6b5f9af6d5bcf'></a>
## tg_geom_intersects_xy()
```c
bool tg_geom_intersects_xy(const struct tg_geom *a, double x, double y);
```
Tests whether a geometry intersects a point using xy coordinates. 

**See also**

- [Geometry predicates](#group___geometry_predicates)



<a name='group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4'></a>
## tg_parse_geojson()
```c
struct tg_geom *tg_parse_geojson(const char *geojson);
```
Parse geojson.

Supports [GeoJSON](https://datatracker.ietf.org/doc/html/rfc7946) standard, including Features, FeaturesCollection, ZM coordinates, properties, and arbritary JSON members. 

**Parameters**

- **geojson**: A geojson string. Must be UTF8 and null-terminated.



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_geojsonn()](#group___geometry_parsing_1ga45f669345bccec7184f8a127ada71067)
- [tg_parse_geojson_ix()](#group___geometry_parsing_1gab78f1bd7fede29ebb2ecb9895d1c6bdf)
- [tg_parse_geojsonn_ix()](#group___geometry_parsing_1ga3b11ffa6b82d29245e673d096ac964db)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga45f669345bccec7184f8a127ada71067'></a>
## tg_parse_geojsonn()
```c
struct tg_geom *tg_parse_geojsonn(const char *geojson, size_t len);
```
Parse geojson with an included data length. 

**Parameters**

- **geojson**: Geojson data. Must be UTF8.
- **len**: Length of data



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gab78f1bd7fede29ebb2ecb9895d1c6bdf'></a>
## tg_parse_geojson_ix()
```c
struct tg_geom *tg_parse_geojson_ix(const char *geojson, enum tg_index ix);
```
Parse geojson using provided indexing option. 

**Parameters**

- **geojson**: A geojson string. Must be UTF8 and null-terminated.
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_index](.#tg_index)
- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)
- [tg_parse_geojsonn_ix()](#group___geometry_parsing_1ga3b11ffa6b82d29245e673d096ac964db)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga3b11ffa6b82d29245e673d096ac964db'></a>
## tg_parse_geojsonn_ix()
```c
struct tg_geom *tg_parse_geojsonn_ix(const char *geojson, size_t len, enum tg_index ix);
```
Parse geojson using provided indexing option. 

**Parameters**

- **geojson**: Geojson data. Must be UTF8.
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_index](.#tg_index)
- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)
- [tg_parse_geojson_ix()](#group___geometry_parsing_1gab78f1bd7fede29ebb2ecb9895d1c6bdf)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c'></a>
## tg_parse_wkt()
```c
struct tg_geom *tg_parse_wkt(const char *wkt);
```
Parse Well-known text (WKT). 

**Parameters**

- **wkt**: A WKT string. Must be null-terminated



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wktn()](#group___geometry_parsing_1gaae7cb710cdb42db02b398760acd37f84)
- [tg_parse_wkt_ix()](#group___geometry_parsing_1ga0444fdcf8fb4cb754b4a2c1e07f899e5)
- [tg_parse_wktn_ix()](#group___geometry_parsing_1gadf71b642030b63dc399bd2f213a76ed8)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gaae7cb710cdb42db02b398760acd37f84'></a>
## tg_parse_wktn()
```c
struct tg_geom *tg_parse_wktn(const char *wkt, size_t len);
```
Parse Well-known text (WKT) with an included data length. 

**Parameters**

- **wkt**: WKT data
- **len**: Length of data



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wkt()](#group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga0444fdcf8fb4cb754b4a2c1e07f899e5'></a>
## tg_parse_wkt_ix()
```c
struct tg_geom *tg_parse_wkt_ix(const char *wkt, enum tg_index ix);
```
Parse Well-known text (WKT) using provided indexing option. 

**Parameters**

- **wkt**: A WKT string. Must be null-terminated
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wkt()](#group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c)
- [tg_parse_wktn_ix()](#group___geometry_parsing_1gadf71b642030b63dc399bd2f213a76ed8)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gadf71b642030b63dc399bd2f213a76ed8'></a>
## tg_parse_wktn_ix()
```c
struct tg_geom *tg_parse_wktn_ix(const char *wkt, size_t len, enum tg_index ix);
```
Parse Well-known text (WKT) using provided indexing option. 

**Parameters**

- **wkt**: WKT data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wkt()](#group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c)
- [tg_parse_wkt_ix()](#group___geometry_parsing_1ga0444fdcf8fb4cb754b4a2c1e07f899e5)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga6929035c4b0e606ef84537272d1ece30'></a>
## tg_parse_wkb()
```c
struct tg_geom *tg_parse_wkb(const uint8_t *wkb, size_t len);
```
Parse Well-known binary (WKB). 

**Parameters**

- **wkb**: WKB data
- **len**: Length of data



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wkb_ix()](#group___geometry_parsing_1gae7c77d7de79d8f8023bff15f6ad2b7ae)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gae7c77d7de79d8f8023bff15f6ad2b7ae'></a>
## tg_parse_wkb_ix()
```c
struct tg_geom *tg_parse_wkb_ix(const uint8_t *wkb, size_t len, enum tg_index ix);
```
Parse Well-known binary (WKB) using provided indexing option. 

**Parameters**

- **wkb**: WKB data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_wkb()](#group___geometry_parsing_1ga6929035c4b0e606ef84537272d1ece30)



<a name='group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67'></a>
## tg_parse_hex()
```c
struct tg_geom *tg_parse_hex(const char *hex);
```
Parse hex encoded Well-known binary (WKB) or GeoBIN. 

**Parameters**

- **hex**: A hex string. Must be null-terminated



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_hexn()](#group___geometry_parsing_1ga2beb47ee201ddbd1a9a7a43c938c4396)
- [tg_parse_hex_ix()](#group___geometry_parsing_1ga8718e134723418426e5df2b13acdf0c1)
- [tg_parse_hexn_ix()](#group___geometry_parsing_1ga3d1f9eb85ff97812fed7cf7bf9e14bd4)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga2beb47ee201ddbd1a9a7a43c938c4396'></a>
## tg_parse_hexn()
```c
struct tg_geom *tg_parse_hexn(const char *hex, size_t len);
```
Parse hex encoded Well-known binary (WKB) or GeoBIN with an included data length. 

**Parameters**

- **hex**: Hex data
- **len**: Length of data



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_hex()](#group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga8718e134723418426e5df2b13acdf0c1'></a>
## tg_parse_hex_ix()
```c
struct tg_geom *tg_parse_hex_ix(const char *hex, enum tg_index ix);
```
Parse hex encoded Well-known binary (WKB) or GeoBIN using provided indexing option. 

**Parameters**

- **hex**: Hex string. Must be null-terminated
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_hex()](#group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67)
- [tg_parse_hexn_ix()](#group___geometry_parsing_1ga3d1f9eb85ff97812fed7cf7bf9e14bd4)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga3d1f9eb85ff97812fed7cf7bf9e14bd4'></a>
## tg_parse_hexn_ix()
```c
struct tg_geom *tg_parse_hexn_ix(const char *hex, size_t len, enum tg_index ix);
```
Parse hex encoded Well-known binary (WKB) or GeoBIN using provided indexing option. 

**Parameters**

- **hex**: Hex data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_hex()](#group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67)
- [tg_parse_hex_ix()](#group___geometry_parsing_1ga8718e134723418426e5df2b13acdf0c1)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1ga4792bf4f319356fff3fa539cbd44b196'></a>
## tg_parse_geobin()
```c
struct tg_geom *tg_parse_geobin(const uint8_t *geobin, size_t len);
```
Parse GeoBIN binary using provided indexing option. 

**Parameters**

- **geobin**: GeoBIN data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_geobin_ix()](#group___geometry_parsing_1gac0450996bcd71cdde81af451dd0e9571)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [tg_geom_geobin()](#group___geometry_writing_1gab0f92319db4b8c61c62e0bd735d907b7)
- [https://github.com/tidwall/tg/blob/main/docs/GeoBIN.md](https://github.com/tidwall/tg/blob/main/docs/GeoBIN.md)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gac0450996bcd71cdde81af451dd0e9571'></a>
## tg_parse_geobin_ix()
```c
struct tg_geom *tg_parse_geobin_ix(const uint8_t *geobin, size_t len, enum tg_index ix);
```
Parse GeoBIN binary using provided indexing option. 

**Parameters**

- **geobin**: GeoBIN data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_geobin()](#group___geometry_parsing_1ga4792bf4f319356fff3fa539cbd44b196)



<a name='group___geometry_parsing_1gaa9e5850bb2cc4eb442c227cd5ac738a1'></a>
## tg_parse()
```c
struct tg_geom *tg_parse(const void *data, size_t len);
```
Parse data into a geometry by auto detecting the input type. The input data can be WKB, WKT, Hex, or GeoJSON. 

**Parameters**

- **data**: Data
- **len**: Length of data



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse_ix()](#group___geometry_parsing_1gae0bfc62deb68979a46ed62facfee1280)
- [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gae0bfc62deb68979a46ed62facfee1280'></a>
## tg_parse_ix()
```c
struct tg_geom *tg_parse_ix(const void *data, size_t len, enum tg_index ix);
```
Parse data using provided indexing option. 

**Parameters**

- **data**: Data
- **len**: Length of data
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A geometry or an error. Use [tg_geom_error()](#group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e) after parsing to check for errors.


**See also**

- [tg_parse()](#group___geometry_parsing_1gaa9e5850bb2cc4eb442c227cd5ac738a1)



<a name='group___geometry_parsing_1gae77b27ad34c2a215cc281647ab6dbc7e'></a>
## tg_geom_error()
```c
const char *tg_geom_error(const struct tg_geom *geom);
```
Return a parsing error.

Parsing functions, such as [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4), may fail due to invalid input data.

It's important to **always** check for errors after parsing.

**Example**

```c
struct tg_geom *geom = tg_parse_geojson(input);
if (tg_geom_error(geom)) {
    // The parsing failed due to invalid input or out of memory.

    // Get the error message.
    const char *err = tg_geom_error(geom);

    // Do something with the error, such as log it.
    printf("[err] %s\n", err);

    // Make sure to free the error geom and it's resources.
    tg_geom_free(geom);

    // !!
    // DO NOT use the return value of tg_geom_error() after calling 
    // tg_geom_free(). If you need to hang onto the error for a longer
    // period of time then you must copy it before freeing.
    // !!

    return;
} else {
    // ... Parsing succeeded 
}
```




**Return**

- A string describing the error
- NULL if there was no error


**See also**

- [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae)
- [tg_parse_geojson()](#group___geometry_parsing_1ga42ba8cac8e7f7d0b31c7d5eab63da8b4)
- [tg_parse_wkt()](#group___geometry_parsing_1ga5abcaa01d5b5e14ea017ff9dd5bd426c)
- [tg_parse_wkb()](#group___geometry_parsing_1ga6929035c4b0e606ef84537272d1ece30)
- [tg_parse_hex()](#group___geometry_parsing_1ga0fc4fd8cb076a78c44df07c517281f67)
- [Geometry parsing](#group___geometry_parsing)



<a name='group___geometry_parsing_1gac77e3d8d51a7e66381627cb25853d80f'></a>
## tg_geobin_fullrect()
```c
int tg_geobin_fullrect(const uint8_t *geobin, size_t len, double min[4], double max[4]);
```
Returns the minimum bounding rectangle of GeoBIN data. 

**Parameters**

- **geobin**: GeoBIN data
- **len**: Length of data
- **min**: min values, must have room for 4 dimensions
- **max**: max values, must have room for 4 dimensions



**Return**

- number of dimensions, or zero if rect cannot be determined.


**See also**

- [tg_geom_fullrect()](#group___geometry_accessors_1gac1a077f09e247c022f09e48392b80051)
- [tg_geom_rect()](#group___geometry_accessors_1ga68d67f900b847ae08e6515a620f4f657)



<a name='group___geometry_parsing_1gabf4ef20303d65ccb265ce5359abdfa79'></a>
## tg_geobin_rect()
```c
struct tg_rect tg_geobin_rect(const uint8_t *geobin, size_t len);
```
Returns the minimum bounding rectangle of GeoBIN data. 

**Parameters**

- **geobin**: GeoBIN data
- **len**: Length of data



**Return**

- the rectangle



<a name='group___geometry_parsing_1gab318b6a815f21bb81440ad3edbb61afe'></a>
## tg_geobin_point()
```c
struct tg_point tg_geobin_point(const uint8_t *geobin, size_t len);
```
Returns the center point of GeoBIN data. 

**Parameters**

- **geobin**: GeoBIN data
- **len**: Length of data



**Return**

- the center point



<a name='group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f'></a>
## tg_geom_geojson()
```c
size_t tg_geom_geojson(const struct tg_geom *geom, char *dst, size_t n);
```
Writes a GeoJSON representation of a geometry.

The content is stored as a C string in the buffer pointed to by dst. A terminating null character is automatically appended after the content written.



**Parameters**

- **geom**: Input geometry
- **dst**: Buffer where the resulting content is stored.
- **n**: Maximum number of bytes to be used in the buffer.



**Return**

- The number of characters, not including the null-terminator, needed to store the content into the C string buffer. If the returned length is greater than n-1, then only a parital copy occurred, for example:
```c
char str[64];
size_t len = tg_geom_geojson(geom, str, sizeof(str));
if (len > sizeof(str)-1) {
    // ... write did not complete ...
}
```




**See also**

- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [Geometry writing](#group___geometry_writing)



<a name='group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6'></a>
## tg_geom_wkt()
```c
size_t tg_geom_wkt(const struct tg_geom *geom, char *dst, size_t n);
```
Writes a Well-known text (WKT) representation of a geometry.

The content is stored as a C string in the buffer pointed to by dst. A terminating null character is automatically appended after the content written.



**Parameters**

- **geom**: Input geometry
- **dst**: Buffer where the resulting content is stored.
- **n**: Maximum number of bytes to be used in the buffer.



**Return**

- The number of characters, not including the null-terminator, needed to store the content into the C string buffer. If the returned length is greater than n-1, then only a parital copy occurred, for example:
```c
char str[64];
size_t len = tg_geom_wkt(geom, str, sizeof(str));
if (len > sizeof(str)-1) {
    // ... write did not complete ...
}
```




**See also**

- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [Geometry writing](#group___geometry_writing)



<a name='group___geometry_writing_1gac0938697f9270d1924c6746af68dd693'></a>
## tg_geom_wkb()
```c
size_t tg_geom_wkb(const struct tg_geom *geom, uint8_t *dst, size_t n);
```
Writes a Well-known binary (WKB) representation of a geometry.

The content is stored in the buffer pointed by dst.



**Parameters**

- **geom**: Input geometry
- **dst**: Buffer where the resulting content is stored.
- **n**: Maximum number of bytes to be used in the buffer.



**Return**

- The number of characters needed to store the content into the buffer. If the returned length is greater than n, then only a parital copy occurred, for example:
```c
uint8_t buf[64];
size_t len = tg_geom_wkb(geom, buf, sizeof(buf));
if (len > sizeof(buf)) {
    // ... write did not complete ...
}
```




**See also**

- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [Geometry writing](#group___geometry_writing)



<a name='group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b'></a>
## tg_geom_hex()
```c
size_t tg_geom_hex(const struct tg_geom *geom, char *dst, size_t n);
```
Writes a hex encoded Well-known binary (WKB) representation of a geometry.

The content is stored as a C string in the buffer pointed to by dst. A terminating null character is automatically appended after the content written.



**Parameters**

- **geom**: Input geometry
- **dst**: Buffer where the resulting content is stored.
- **n**: Maximum number of bytes to be used in the buffer.



**Return**

- The number of characters, not including the null-terminator, needed to store the content into the C string buffer. If the returned length is greater than n-1, then only a parital copy occurred, for example:
```c
char str[64];
size_t len = tg_geom_hex(geom, str, sizeof(str));
if (len > sizeof(str)-1) {
    // ... write did not complete ...
}
```




**See also**

- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [Geometry writing](#group___geometry_writing)



<a name='group___geometry_writing_1gab0f92319db4b8c61c62e0bd735d907b7'></a>
## tg_geom_geobin()
```c
size_t tg_geom_geobin(const struct tg_geom *geom, uint8_t *dst, size_t n);
```
Writes a GeoBIN representation of a geometry.

The content is stored in the buffer pointed by dst.



**Parameters**

- **geom**: Input geometry
- **dst**: Buffer where the resulting content is stored.
- **n**: Maximum number of bytes to be used in the buffer.



**Return**

- The number of characters needed to store the content into the buffer. If the returned length is greater than n, then only a parital copy occurred, for example:
```c
uint8_t buf[64];
size_t len = tg_geom_geobin(geom, buf, sizeof(buf));
if (len > sizeof(buf)) {
    // ... write did not complete ...
}
```




**See also**

- [tg_geom_geojson()](#group___geometry_writing_1gae4669b8ee598a46e7f4c5a11e645fe8f)
- [tg_geom_wkt()](#group___geometry_writing_1ga047599bbe51886a6c3fbe35a6372d5d6)
- [tg_geom_wkb()](#group___geometry_writing_1gac0938697f9270d1924c6746af68dd693)
- [tg_geom_hex()](#group___geometry_writing_1ga269f61715302d48f144ffaf46e8c397b)
- [Geometry writing](#group___geometry_writing)



<a name='group___geometry_constructors_ex_1ga1f4b592d4005fea6b9b49b35b0c15e74'></a>
## tg_geom_new_point_z()
```c
struct tg_geom *tg_geom_new_point_z(struct tg_point point, double z);
```
Creates a Point geometry that includes a Z coordinate. 

**Parameters**

- **point**: Input point
- **z**: The Z coordinate



**Return**

- A newly allocated geometry, or NULL if system is out of memory. The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga68013f90e62307ef0245e8e3398eeda6'></a>
## tg_geom_new_point_m()
```c
struct tg_geom *tg_geom_new_point_m(struct tg_point point, double m);
```
Creates a Point geometry that includes an M coordinate. 

**Parameters**

- **point**: Input point
- **m**: The M coordinate



**Return**

- A newly allocated geometry, or NULL if system is out of memory. The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga74f8f3f07973ec99af27950735d8bb30'></a>
## tg_geom_new_point_zm()
```c
struct tg_geom *tg_geom_new_point_zm(struct tg_point point, double z, double m);
```
Creates a Point geometry that includes a Z and M coordinates. 

**Parameters**

- **point**: Input point
- **z**: The Z coordinate
- **m**: The M coordinate



**Return**

- A newly allocated geometry, or NULL if system is out of memory. The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga64d9a692f5f8d7d5ba37fd0b5abdb9a3'></a>
## tg_geom_new_point_empty()
```c
struct tg_geom *tg_geom_new_point_empty();
```
Creates an empty Point geometry. 

**Return**

- A newly allocated geometry, or NULL if system is out of memory. The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga5b16e072effa305db633265277852a97'></a>
## tg_geom_new_linestring_z()
```c
struct tg_geom *tg_geom_new_linestring_z(const struct tg_line *line, const double *extra_coords, int ncoords);
```
Creates a LineString geometry that includes Z coordinates. 

**Parameters**

- **line**: Input line, caller retains ownership.
- **coords**: Array of doubles representing each Z coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga1141d62a61f6aef6342e02bf930123c0'></a>
## tg_geom_new_linestring_m()
```c
struct tg_geom *tg_geom_new_linestring_m(const struct tg_line *line, const double *extra_coords, int ncoords);
```
Creates a LineString geometry that includes M coordinates. 

**Parameters**

- **line**: Input line, caller retains ownership.
- **coords**: Array of doubles representing each M coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gaf7f088fd78eb1340fd91a46f4225d234'></a>
## tg_geom_new_linestring_zm()
```c
struct tg_geom *tg_geom_new_linestring_zm(const struct tg_line *line, const double *extra_coords, int ncoords);
```
Creates a LineString geometry that includes ZM coordinates. 

**Parameters**

- **line**: Input line, caller retains ownership.
- **coords**: Array of doubles representing each Z and M coordinate, interleaved. Caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gadbd7a00e18648a3430bf89118448db58'></a>
## tg_geom_new_linestring_empty()
```c
struct tg_geom *tg_geom_new_linestring_empty();
```
Creates an empty LineString geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gac81514f9acbafce335d8982233772664'></a>
## tg_geom_new_polygon_z()
```c
struct tg_geom *tg_geom_new_polygon_z(const struct tg_poly *poly, const double *extra_coords, int ncoords);
```
Creates a Polygon geometry that includes Z coordinates. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.
- **coords**: Array of doubles representing each Z coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gaf8a608082f6310107d029f25be660991'></a>
## tg_geom_new_polygon_m()
```c
struct tg_geom *tg_geom_new_polygon_m(const struct tg_poly *poly, const double *extra_coords, int ncoords);
```
Creates a Polygon geometry that includes M coordinates. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.
- **coords**: Array of doubles representing each M coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga5b2cbc15b3780925a1cfa05da25e0285'></a>
## tg_geom_new_polygon_zm()
```c
struct tg_geom *tg_geom_new_polygon_zm(const struct tg_poly *poly, const double *extra_coords, int ncoords);
```
Creates a Polygon geometry that includes ZM coordinates. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.
- **coords**: Array of doubles representing each Z and M coordinate, interleaved. Caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga72b1cb66d57ec21a4aef558cd64b647e'></a>
## tg_geom_new_polygon_empty()
```c
struct tg_geom *tg_geom_new_polygon_empty();
```
Creates an empty Polygon geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gad7609eb5456dfb084ee7d2ab0637db82'></a>
## tg_geom_new_multipoint_z()
```c
struct tg_geom *tg_geom_new_multipoint_z(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
```
Creates a MultiPoint geometry that includes Z coordinates. 

**Parameters**

- **points**: An array of points, caller retains ownership.
- **npoints**: The number of points in array
- **coords**: Array of doubles representing each Z coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gaae2123bcb6b30a5efeae422ac4a4aec6'></a>
## tg_geom_new_multipoint_m()
```c
struct tg_geom *tg_geom_new_multipoint_m(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
```
Creates a MultiPoint geometry that includes M coordinates. 

**Parameters**

- **points**: An array of points, caller retains ownership.
- **npoints**: The number of points in array
- **coords**: Array of doubles representing each M coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga772ab89b63583177d537615f1bd2adb5'></a>
## tg_geom_new_multipoint_zm()
```c
struct tg_geom *tg_geom_new_multipoint_zm(const struct tg_point *points, int npoints, const double *extra_coords, int ncoords);
```
Creates a MultiPoint geometry that includes ZM coordinates. 

**Parameters**

- **points**: An array of points, caller retains ownership.
- **npoints**: The number of points in array
- **coords**: Array of doubles representing each Z and M coordinate, interleaved. Caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga967114d5e9332630c7f99ac6a116991d'></a>
## tg_geom_new_multipoint_empty()
```c
struct tg_geom *tg_geom_new_multipoint_empty();
```
Creates an empty MultiPoint geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga9a855e4d8aeb386b3fcdf2f35da4bb53'></a>
## tg_geom_new_multilinestring_z()
```c
struct tg_geom *tg_geom_new_multilinestring_z(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
```
Creates a MultiLineString geometry that includes Z coordinates. 

**Parameters**

- **lines**: An array of lines, caller retains ownership.
- **nlines**: The number of lines in array
- **coords**: Array of doubles representing each Z coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1gad53b72d80be834a14a9a47d0953f08c3'></a>
## tg_geom_new_multilinestring_m()
```c
struct tg_geom *tg_geom_new_multilinestring_m(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
```
Creates a MultiLineString geometry that includes M coordinates. 

**Parameters**

- **lines**: An array of lines, caller retains ownership.
- **nlines**: The number of lines in array
- **coords**: Array of doubles representing each M coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga5ff87656da4c72c947cef312acb50ef1'></a>
## tg_geom_new_multilinestring_zm()
```c
struct tg_geom *tg_geom_new_multilinestring_zm(const struct tg_line *const lines[], int nlines, const double *extra_coords, int ncoords);
```
Creates a MultiLineString geometry that includes ZM coordinates. 

**Parameters**

- **lines**: An array of lines, caller retains ownership.
- **nlines**: The number of lines in array
- **coords**: Array of doubles representing each Z and M coordinate, interleaved. Caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga2798822f3fdb20132a6cc6ad2247fae4'></a>
## tg_geom_new_multilinestring_empty()
```c
struct tg_geom *tg_geom_new_multilinestring_empty();
```
Creates an empty MultiLineString geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga46c67962e6d151e46cc961a76e90941c'></a>
## tg_geom_new_multipolygon_z()
```c
struct tg_geom *tg_geom_new_multipolygon_z(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
```
Creates a MultiPolygon geometry that includes Z coordinates. 

**Parameters**

- **polys**: An array of polygons, caller retains ownership.
- **npolys**: The number of polygons in array
- **coords**: Array of doubles representing each Z coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga824ac7a05f0cc5e4a74d9335f23954a0'></a>
## tg_geom_new_multipolygon_m()
```c
struct tg_geom *tg_geom_new_multipolygon_m(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
```
Creates a MultiPolygon geometry that includes M coordinates. 

**Parameters**

- **polys**: An array of polygons, caller retains ownership.
- **npolys**: The number of polygons in array
- **coords**: Array of doubles representing each M coordinate, caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga998ef169fc03e82c75fe32757b057334'></a>
## tg_geom_new_multipolygon_zm()
```c
struct tg_geom *tg_geom_new_multipolygon_zm(const struct tg_poly *const polys[], int npolys, const double *extra_coords, int ncoords);
```
Creates a MultiPolygon geometry that includes ZM coordinates. 

**Parameters**

- **polys**: An array of polygons, caller retains ownership.
- **npolys**: The number of polygons in array
- **coords**: Array of doubles representing each Z and M coordinate, interleaved. Caller retains ownership.
- **ncoords**: Number of doubles in array.



**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga48684027f9beb519c52575bf6754a6f8'></a>
## tg_geom_new_multipolygon_empty()
```c
struct tg_geom *tg_geom_new_multipolygon_empty();
```
Creates an empty MultiPolygon geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___geometry_constructors_ex_1ga15fc542f3f6b4bb4dc7e91d554e0dfff'></a>
## tg_geom_new_geometrycollection_empty()
```c
struct tg_geom *tg_geom_new_geometrycollection_empty();
```
Creates an empty GeometryCollection geometry. 

**Return**

- A newly allocated geometry.
- NULL if system is out of memory.


**Note**

- The caller is responsible for freeing with [tg_geom_free()](#group___geometry_constructors_1gaf6f400f624b9f3e9052ac26ab17d72ae).


**See also**

- [Geometry with alternative dimensions](#group___geometry_constructors_ex)



<a name='group___point_funcs_1ga07ff1afb45368e2f511da5cb8e689943'></a>
## tg_point_rect()
```c
struct tg_rect tg_point_rect(struct tg_point point);
```
Returns the minimum bounding rectangle of a point. 

**See also**

- [Point functions](#group___point_funcs)



<a name='group___point_funcs_1ga59e99021c916a5fe7a350aee695ef6d3'></a>
## tg_point_intersects_rect()
```c
bool tg_point_intersects_rect(struct tg_point a, struct tg_rect b);
```
Tests whether a point fully intersects a rectangle. 

**See also**

- [Point functions](#group___point_funcs)



<a name='group___segment_funcs_1ga8218b7694fe5079da6ce0a241991cf78'></a>
## tg_segment_rect()
```c
struct tg_rect tg_segment_rect(struct tg_segment s);
```
Returns the minimum bounding rectangle of a segment. 

**See also**

- [Segment functions](#group___segment_funcs)



<a name='group___segment_funcs_1ga12b1ec6c72d4120391daff364d5c7b9b'></a>
## tg_segment_intersects_segment()
```c
bool tg_segment_intersects_segment(struct tg_segment a, struct tg_segment b);
```
Tests whether a segment intersects another segment. 

**See also**

- [Segment functions](#group___segment_funcs)



<a name='group___rect_funcs_1gadc227cfc279d612269a142841d736c79'></a>
## tg_rect_expand()
```c
struct tg_rect tg_rect_expand(struct tg_rect rect, struct tg_rect other);
```
Expands a rectangle to include an additional rectangle. 

**Parameters**

- **rect**: Input rectangle
- **other**: Input rectangle



**Return**

- Expanded rectangle


**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='group___rect_funcs_1gaad0df238f6f430d9cc6fd6b0c6807931'></a>
## tg_rect_expand_point()
```c
struct tg_rect tg_rect_expand_point(struct tg_rect rect, struct tg_point point);
```
Expands a rectangle to include an additional point. 

**Parameters**

- **rect**: Input rectangle
- **point**: Input Point



**Return**

- Expanded rectangle


**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='group___rect_funcs_1ga44be4970fbbbdfa9e2ceb338a938d262'></a>
## tg_rect_center()
```c
struct tg_point tg_rect_center(struct tg_rect rect);
```
Returns the center point of a rectangle 

**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='group___rect_funcs_1ga910f2b37e7c7b17f9c01bbddee871b11'></a>
## tg_rect_intersects_rect()
```c
bool tg_rect_intersects_rect(struct tg_rect a, struct tg_rect b);
```
Tests whether a rectangle intersects another rectangle. 

**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='group___rect_funcs_1gac4d6e61cc5260ed913c736fd71defe05'></a>
## tg_rect_intersects_point()
```c
bool tg_rect_intersects_point(struct tg_rect a, struct tg_point b);
```
Tests whether a rectangle and a point intersect. 

**See also**

- [Rectangle functions](#group___rect_funcs)



<a name='group___ring_funcs_1ga7defe1b6d43be8285a73f7977b342661'></a>
## tg_ring_new()
```c
struct tg_ring *tg_ring_new(const struct tg_point *points, int npoints);
```
Creates a ring from a series of points. 

**Parameters**

- **points**: Array of points
- **npoints**: Number of points in array



**Return**

- A newly allocated ring
- NULL if out of memory


**Note**

- A [tg_ring](#structtg__ring) can be safely upcasted to a [tg_geom](#structtg__geom). `(struct tg_geom*)ring`
- A [tg_ring](#structtg__ring) can be safely upcasted to a [tg_poly](#structtg__poly). `(struct tg_poly*)ring`
- All rings with 32 or more points are automatically indexed.


**See also**

- [tg_ring_new_ix()](#group___ring_funcs_1ga9a154a6a8dc4beaf7758ef39b21c1ce8)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga9a154a6a8dc4beaf7758ef39b21c1ce8'></a>
## tg_ring_new_ix()
```c
struct tg_ring *tg_ring_new_ix(const struct tg_point *points, int npoints, enum tg_index ix);
```
Creates a ring from a series of points using provided index option. 

**Parameters**

- **points**: Array of points
- **npoints**: Number of points in array
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A newly allocated ring
- NULL if out of memory


**Note**

- A [tg_ring](#structtg__ring) can be safely upcasted to a [tg_geom](#structtg__geom). `(struct tg_geom*)ring`
- A [tg_ring](#structtg__ring) can be safely upcasted to a [tg_poly](#structtg__poly). `(struct tg_poly*)ring`


**See also**

- [tg_ring_new()](#group___ring_funcs_1ga7defe1b6d43be8285a73f7977b342661)
- [tg_index](.#tg_index)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga51e9d824d50f67f89981f0edc7fe0cc5'></a>
## tg_ring_free()
```c
void tg_ring_free(struct tg_ring *ring);
```
Releases the memory associated with a ring. 

**Parameters**

- **ring**: Input ring



**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gafda24e3f1274ae1f1421934722cfd67b'></a>
## tg_ring_clone()
```c
struct tg_ring *tg_ring_clone(const struct tg_ring *ring);
```
Clones a ring 

**Parameters**

- **ring**: Input ring, caller retains ownership.



**Return**

- A duplicate of the provided ring.


**Note**

- The caller is responsible for freeing with [tg_ring_free()](#group___ring_funcs_1ga51e9d824d50f67f89981f0edc7fe0cc5).
- This method of cloning uses implicit sharing through an atomic reference counter.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gaaa7b50bc974357abe9d67ec8d6ca0d91'></a>
## tg_ring_copy()
```c
struct tg_ring *tg_ring_copy(const struct tg_ring *ring);
```
Copies a ring 

**Parameters**

- **ring**: Input ring, caller retains ownership.



**Return**

- A duplicate of the provided ring.
- NULL if out of memory


**Note**

- The caller is responsible for freeing with [tg_ring_free()](#group___ring_funcs_1ga51e9d824d50f67f89981f0edc7fe0cc5).
- This method performs a deep copy of the entire geometry to new memory.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga0f146af1880d1ed7d02bfbdd0298feac'></a>
## tg_ring_memsize()
```c
size_t tg_ring_memsize(const struct tg_ring *ring);
```
Returns the allocation size of the ring. 

**Parameters**

- **ring**: Input ring



**Return**

- Size of ring in bytes


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga64086c935039b1f0fcd8633be763427d'></a>
## tg_ring_rect()
```c
struct tg_rect tg_ring_rect(const struct tg_ring *ring);
```
Returns the minimum bounding rectangle of a rect. 

**Parameters**

- **ring**: Input ring



**Return**

- Minimum bounding retangle


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga2437033fd4a6d67a54ce2cf1f771b66c'></a>
## tg_ring_num_points()
```c
int tg_ring_num_points(const struct tg_ring *ring);
```
Returns the number of points. 

**Parameters**

- **ring**: Input ring



**Return**

- Number of points


**See also**

- [tg_ring_point_at()](#group___ring_funcs_1gad2450a013edd99a013dbaee437fb0c3d)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gad2450a013edd99a013dbaee437fb0c3d'></a>
## tg_ring_point_at()
```c
struct tg_point tg_ring_point_at(const struct tg_ring *ring, int index);
```
Returns the point at index. 

**Parameters**

- **ring**: Input ring
- **index**: Index of point



**Return**

- The point at index


**Note**

- This function performs bounds checking. Use [tg_ring_points()](#group___ring_funcs_1gaf97a197de54c13d01aa9d983115a336d) for direct access to the points.


**See also**

- [tg_ring_num_points()](#group___ring_funcs_1ga2437033fd4a6d67a54ce2cf1f771b66c)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gaf97a197de54c13d01aa9d983115a336d'></a>
## tg_ring_points()
```c
const struct tg_point *tg_ring_points(const struct tg_ring *ring);
```
Returns the underlying point array of a ring. 

**Parameters**

- **ring**: Input ring



**Return**

- Array or points


**See also**

- [tg_ring_num_points()](#group___ring_funcs_1ga2437033fd4a6d67a54ce2cf1f771b66c)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga56dcd7ca3d210696979c3c6c62d71690'></a>
## tg_ring_num_segments()
```c
int tg_ring_num_segments(const struct tg_ring *ring);
```
Returns the number of segments. 

**Parameters**

- **ring**: Input ring



**Return**

- Number of segments


**See also**

- [tg_ring_segment_at()](#group___ring_funcs_1ga52b3ba9138aee4245fca05bedd00801d)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga52b3ba9138aee4245fca05bedd00801d'></a>
## tg_ring_segment_at()
```c
struct tg_segment tg_ring_segment_at(const struct tg_ring *ring, int index);
```
Returns the segment at index. 

**Parameters**

- **ring**: Input ring
- **index**: Index of segment



**Return**

- The segment at index


**See also**

- [tg_ring_num_segments()](#group___ring_funcs_1ga56dcd7ca3d210696979c3c6c62d71690)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga2559c1e9f0a3b48d97b36010ac424703'></a>
## tg_ring_convex()
```c
bool tg_ring_convex(const struct tg_ring *ring);
```
Returns true if ring is convex. 

**Parameters**

- **ring**: Input ring



**Return**

- True if ring is convex.
- False if ring is concave.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gae677767bd4bc40480f0a703e78f2e713'></a>
## tg_ring_clockwise()
```c
bool tg_ring_clockwise(const struct tg_ring *ring);
```
Returns true if winding order is clockwise. 

**Parameters**

- **ring**: Input ring



**Return**

- True if clockwise
- False if counter-clockwise


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gacecc3997e0cabb5532946acffee834f0'></a>
## tg_ring_index_spread()
```c
int tg_ring_index_spread(const struct tg_ring *ring);
```
Returns the indexing spread for a ring.

The "spread" is the number of segments or rectangles that are grouped together to produce a unioned rectangle that is stored at a higher level.

For a tree based structure, this would be the number of items per node.



**Parameters**

- **ring**: Input ring



**Return**

- The spread, default is 16
- Zero if ring has no indexing


**See also**

- [tg_ring_index_num_levels()](#group___ring_funcs_1ga1bbaf39cc96d727f253c0f66c0ede4e4)
- [tg_ring_index_level_num_rects()](#group___ring_funcs_1ga292a911cdb05f7dbc19bcbd031226deb)
- [tg_ring_index_level_rect()](#group___ring_funcs_1ga7eab00bc99a6f4148d2e6ced95ff24c3)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga1bbaf39cc96d727f253c0f66c0ede4e4'></a>
## tg_ring_index_num_levels()
```c
int tg_ring_index_num_levels(const struct tg_ring *ring);
```
Returns the number of levels. 

**Parameters**

- **ring**: Input ring



**Return**

- The number of levels
- Zero if ring has no indexing


**See also**

- [tg_ring_index_spread()](#group___ring_funcs_1gacecc3997e0cabb5532946acffee834f0)
- [tg_ring_index_level_num_rects()](#group___ring_funcs_1ga292a911cdb05f7dbc19bcbd031226deb)
- [tg_ring_index_level_rect()](#group___ring_funcs_1ga7eab00bc99a6f4148d2e6ced95ff24c3)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga292a911cdb05f7dbc19bcbd031226deb'></a>
## tg_ring_index_level_num_rects()
```c
int tg_ring_index_level_num_rects(const struct tg_ring *ring, int levelidx);
```
Returns the number of rectangles at level. 

**Parameters**

- **ring**: Input ring
- **levelidx**: The index of level



**Return**

- The number of index levels
- Zero if ring has no indexing or levelidx is out of bounds.


**See also**

- [tg_ring_index_spread()](#group___ring_funcs_1gacecc3997e0cabb5532946acffee834f0)
- [tg_ring_index_num_levels()](#group___ring_funcs_1ga1bbaf39cc96d727f253c0f66c0ede4e4)
- [tg_ring_index_level_rect()](#group___ring_funcs_1ga7eab00bc99a6f4148d2e6ced95ff24c3)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga7eab00bc99a6f4148d2e6ced95ff24c3'></a>
## tg_ring_index_level_rect()
```c
struct tg_rect tg_ring_index_level_rect(const struct tg_ring *ring, int levelidx, int rectidx);
```
Returns a specific level rectangle. 

**Parameters**

- **ring**: Input ring
- **levelidx**: The index of level
- **rectidx**: The index of rectangle



**Return**

- The rectangle
- Empty rectangle if ring has no indexing, or levelidx or rectidx is out of bounds.


**See also**

- [tg_ring_index_spread()](#group___ring_funcs_1gacecc3997e0cabb5532946acffee834f0)
- [tg_ring_index_num_levels()](#group___ring_funcs_1ga1bbaf39cc96d727f253c0f66c0ede4e4)
- [tg_ring_index_level_num_rects()](#group___ring_funcs_1ga292a911cdb05f7dbc19bcbd031226deb)
- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1ga716e10054b4bda84efb259d57aff5015'></a>
## tg_ring_nearest_segment()
```c
bool tg_ring_nearest_segment(const struct tg_ring *ring, double(*rect_dist)(struct tg_rect rect, int *more, void *udata), double(*seg_dist)(struct tg_segment seg, int *more, void *udata), bool(*iter)(struct tg_segment seg, double dist, int index, void *udata), void *udata);
```
Iterates over segments from nearest to farthest.

This is a kNN operation. The caller must provide their own "rect_dist" and "seg_dist" callbacks to do the actual distance calculations.



**Parameters**

- **ring**: Input ring
- **rect_dist**: Callback that returns the distance to a [tg_rect](#structtg__rect).
- **seg_dist**: Callback that returns the distance to a [tg_segment](#structtg__segment).
- **iter**: Callback that returns each segment in the ring in order of nearest to farthest. Caller must return true to continue to the next segment, or return false to stop iterating.
- **udata**: User-defined data



**Return**

- True if operation succeeded, false if out of memory.


**Note**

- Though not typical, this operation may need to allocate memory. It's recommended to check the return value for success.
- The `*more` argument is an optional ref-value that is used for performing partial step-based or probability-based calculations. A detailed description of its use is outside the scope of this document. Ignoring it altogether is the preferred behavior.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gabee40f4a66c2ebb4516a9f980ff5d998'></a>
## tg_ring_line_search()
```c
void tg_ring_line_search(const struct tg_ring *a, const struct tg_line *b, bool(*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, int bidx, void *udata), void *udata);
```
Iterates over all segments in ring A that intersect with segments in line B. 

**Note**

- This efficently uses the indexes of each geometry, if available.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gacd2c483213d8110c9373e8e47bd9f49e'></a>
## tg_ring_ring_search()
```c
void tg_ring_ring_search(const struct tg_ring *a, const struct tg_ring *b, bool(*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, int bidx, void *udata), void *udata);
```
Iterates over all segments in ring A that intersect with segments in ring B. 

**Note**

- This efficently uses the indexes of each geometry, if available.


**See also**

- [Ring functions](#group___ring_funcs)



<a name='group___ring_funcs_1gabe14408b3ad596ed14b4ce698c2e7826'></a>
## tg_ring_area()
```c
double tg_ring_area(const struct tg_ring *ring);
```
Calculate the area of a ring. 


<a name='group___ring_funcs_1ga0d063ecfef895e21f6d7520db05d302e'></a>
## tg_ring_perimeter()
```c
double tg_ring_perimeter(const struct tg_ring *ring);
```
Calculate the perimeter length of a ring. 


<a name='group___line_funcs_1ga7f834a7213c8d87d8b7c7117bdf8bf63'></a>
## tg_line_new()
```c
struct tg_line *tg_line_new(const struct tg_point *points, int npoints);
```
Creates a line from a series of points. 

**Parameters**

- **points**: Array of points
- **npoints**: Number of points in array



**Return**

- A newly allocated line
- NULL if out of memory


**Note**

- A [tg_line](#structtg__line) can be safely upcasted to a [tg_geom](#structtg__geom). `(struct tg_geom*)line`
- All lines with 32 or more points are automatically indexed.


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga4057ac4ba72aa24985d947fa0ba42c19'></a>
## tg_line_new_ix()
```c
struct tg_line *tg_line_new_ix(const struct tg_point *points, int npoints, enum tg_index ix);
```
Creates a line from a series of points using provided index option. 

**Parameters**

- **points**: Array of points
- **npoints**: Number of points in array
- **ix**: Indexing option, e.g. TG_NONE, TG_NATURAL, TG_YSTRIPES



**Return**

- A newly allocated line
- NULL if out of memory


**Note**

- A [tg_line](#structtg__line) can be safely upcasted to a [tg_geom](#structtg__geom). `(struct tg_geom*)poly`


**See also**

- [tg_index](.#tg_index)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga13b2a0ed2014525d613c25c9232affd7'></a>
## tg_line_free()
```c
void tg_line_free(struct tg_line *line);
```
Releases the memory associated with a line. 

**Parameters**

- **line**: Input line



**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gac7cbf109cca2dffba781a47932e1411d'></a>
## tg_line_clone()
```c
struct tg_line *tg_line_clone(const struct tg_line *line);
```
Clones a line 

**Parameters**

- **line**: Input line, caller retains ownership.



**Return**

- A duplicate of the provided line.


**Note**

- The caller is responsible for freeing with [tg_line_free()](#group___line_funcs_1ga13b2a0ed2014525d613c25c9232affd7).
- This method of cloning uses implicit sharing through an atomic reference counter.


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga3a5d0aac59658eb2cfd8cdaea8ebf22f'></a>
## tg_line_copy()
```c
struct tg_line *tg_line_copy(const struct tg_line *line);
```
Copies a line 

**Parameters**

- **line**: Input line, caller retains ownership.



**Return**

- A duplicate of the provided line.
- NULL if out of memory


**Note**

- The caller is responsible for freeing with [tg_line_free()](#group___line_funcs_1ga13b2a0ed2014525d613c25c9232affd7).
- This method performs a deep copy of the entire geometry to new memory.


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga6c2476d9dd302a3dea68aa44e7241ab1'></a>
## tg_line_memsize()
```c
size_t tg_line_memsize(const struct tg_line *line);
```
Returns the allocation size of the line. 

**Parameters**

- **line**: Input line



**Return**

- Size of line in bytes


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga2e59f4517937e89ae69ac6c1f12bf797'></a>
## tg_line_rect()
```c
struct tg_rect tg_line_rect(const struct tg_line *line);
```
Returns the minimum bounding rectangle of a line. 

**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gab4483ad3c5418dd9674b1f03498d18e6'></a>
## tg_line_num_points()
```c
int tg_line_num_points(const struct tg_line *line);
```
Returns the number of points. 

**Parameters**

- **line**: Input line



**Return**

- Number of points


**See also**

- [tg_line_point_at()](#group___line_funcs_1ga44955f7b0ee3bd13a33b24aeca01651a)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga45a892019af11582b2761301526519d9'></a>
## tg_line_points()
```c
const struct tg_point *tg_line_points(const struct tg_line *line);
```
Returns the underlying point array of a line. 

**Parameters**

- **line**: Input line



**Return**

- Array or points


**See also**

- [tg_line_num_points()](#group___line_funcs_1gab4483ad3c5418dd9674b1f03498d18e6)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga44955f7b0ee3bd13a33b24aeca01651a'></a>
## tg_line_point_at()
```c
struct tg_point tg_line_point_at(const struct tg_line *line, int index);
```
Returns the point at index. 

**Parameters**

- **line**: Input line
- **index**: Index of point



**Return**

- The point at index


**Note**

- This function performs bounds checking. Use [tg_line_points()](#group___line_funcs_1ga45a892019af11582b2761301526519d9) for direct access to the points.


**See also**

- [tg_line_num_points()](#group___line_funcs_1gab4483ad3c5418dd9674b1f03498d18e6)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gadb56b68cc4ce05cfa832e82e45995687'></a>
## tg_line_num_segments()
```c
int tg_line_num_segments(const struct tg_line *line);
```
Returns the number of segments. 

**Parameters**

- **line**: Input line



**Return**

- Number of segments


**See also**

- [tg_line_segment_at()](#group___line_funcs_1gad0c0982aff76d943f0ce50deb0f9183d)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gad0c0982aff76d943f0ce50deb0f9183d'></a>
## tg_line_segment_at()
```c
struct tg_segment tg_line_segment_at(const struct tg_line *line, int index);
```
Returns the segment at index. 

**Parameters**

- **line**: Input line
- **index**: Index of segment



**Return**

- The segment at index


**See also**

- [tg_line_num_segments()](#group___line_funcs_1gadb56b68cc4ce05cfa832e82e45995687)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gaee0fcc56c7518fa81ebb3457d795ca49'></a>
## tg_line_clockwise()
```c
bool tg_line_clockwise(const struct tg_line *line);
```
Returns true if winding order is clockwise. 

**Parameters**

- **line**: Input line



**Return**

- True if clockwise
- False if counter-clockwise


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga8b851e64231b8b30f30a936cf35cde08'></a>
## tg_line_index_spread()
```c
int tg_line_index_spread(const struct tg_line *line);
```
Returns the indexing spread for a line.

The "spread" is the number of segments or rectangles that are grouped together to produce a unioned rectangle that is stored at a higher level.

For a tree based structure, this would be the number of items per node.



**Parameters**

- **line**: Input line



**Return**

- The spread, default is 16
- Zero if line has no indexing


**See also**

- [tg_line_index_num_levels()](#group___line_funcs_1ga0cbcfec6e6ab81f31a47cd542e32b778)
- [tg_line_index_level_num_rects()](#group___line_funcs_1gab9d4afd57889a11179df01c1df26488b)
- [tg_line_index_level_rect()](#group___line_funcs_1gacc18e4ddb48ccc717f61f298fd6ae970)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga0cbcfec6e6ab81f31a47cd542e32b778'></a>
## tg_line_index_num_levels()
```c
int tg_line_index_num_levels(const struct tg_line *line);
```
Returns the number of levels. 

**Parameters**

- **line**: Input line



**Return**

- The number of levels
- Zero if line has no indexing


**See also**

- [tg_line_index_spread()](#group___line_funcs_1ga8b851e64231b8b30f30a936cf35cde08)
- [tg_line_index_level_num_rects()](#group___line_funcs_1gab9d4afd57889a11179df01c1df26488b)
- [tg_line_index_level_rect()](#group___line_funcs_1gacc18e4ddb48ccc717f61f298fd6ae970)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gab9d4afd57889a11179df01c1df26488b'></a>
## tg_line_index_level_num_rects()
```c
int tg_line_index_level_num_rects(const struct tg_line *line, int levelidx);
```
Returns the number of rectangles at level. 

**Parameters**

- **line**: Input line
- **levelidx**: The index of level



**Return**

- The number of index levels
- Zero if line has no indexing or levelidx is out of bounds.


**See also**

- [tg_line_index_spread()](#group___line_funcs_1ga8b851e64231b8b30f30a936cf35cde08)
- [tg_line_index_num_levels()](#group___line_funcs_1ga0cbcfec6e6ab81f31a47cd542e32b778)
- [tg_line_index_level_rect()](#group___line_funcs_1gacc18e4ddb48ccc717f61f298fd6ae970)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gacc18e4ddb48ccc717f61f298fd6ae970'></a>
## tg_line_index_level_rect()
```c
struct tg_rect tg_line_index_level_rect(const struct tg_line *line, int levelidx, int rectidx);
```
Returns a specific level rectangle. 

**Parameters**

- **line**: Input line
- **levelidx**: The index of level
- **rectidx**: The index of rectangle



**Return**

- The rectangle
- Empty rectangle if line has no indexing, or levelidx or rectidx is out of bounds.


**See also**

- [tg_line_index_spread()](#group___line_funcs_1ga8b851e64231b8b30f30a936cf35cde08)
- [tg_line_index_num_levels()](#group___line_funcs_1ga0cbcfec6e6ab81f31a47cd542e32b778)
- [tg_line_index_level_num_rects()](#group___line_funcs_1gab9d4afd57889a11179df01c1df26488b)
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gaa85835e9619ba7d8557fda3335a0353a'></a>
## tg_line_nearest_segment()
```c
bool tg_line_nearest_segment(const struct tg_line *line, double(*rect_dist)(struct tg_rect rect, int *more, void *udata), double(*seg_dist)(struct tg_segment seg, int *more, void *udata), bool(*iter)(struct tg_segment seg, double dist, int index, void *udata), void *udata);
```
Iterates over segments from nearest to farthest. 

**See also**

- [tg_ring_nearest_segment()](#group___ring_funcs_1ga716e10054b4bda84efb259d57aff5015), which shares the same interface, for a detailed description.
- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1gafef8ca6e5381e93d4c4952343597c0e1'></a>
## tg_line_line_search()
```c
void tg_line_line_search(const struct tg_line *a, const struct tg_line *b, bool(*iter)(struct tg_segment aseg, int aidx, struct tg_segment bseg, int bidx, void *udata), void *udata);
```
Iterates over all segments in line A that intersect with segments in line B. 

**Note**

- This efficently uses the indexes of each geometry, if available.


**See also**

- [Line functions](#group___line_funcs)



<a name='group___line_funcs_1ga994796f0088c82bb0672d84f1a1ce9d1'></a>
## tg_line_length()
```c
double tg_line_length(const struct tg_line *line);
```
Calculate the length of a line. 


<a name='group___poly_funcs_1ga56c2615488ca202baa944c85c20a40f1'></a>
## tg_poly_new()
```c
struct tg_poly *tg_poly_new(const struct tg_ring *exterior, const struct tg_ring *const holes[], int nholes);
```
Creates a polygon. 

**Parameters**

- **exterior**: Exterior ring
- **holes**: Array of interior rings that are holes
- **nholes**: Number of holes in array



**Return**

- A newly allocated polygon
- NULL if out of memory
- NULL if exterior or any holes are NULL


**Note**

- A [tg_poly](#structtg__poly) can be safely upcasted to a [tg_geom](#structtg__geom). `(struct tg_geom*)poly`


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1ga7a591cc2298e4e24ae0e99b055a61fed'></a>
## tg_poly_free()
```c
void tg_poly_free(struct tg_poly *poly);
```
Releases the memory associated with a polygon. 

**Parameters**

- **poly**: Input polygon



**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1ga2f01b5838bcb3669e3aacace12062fbf'></a>
## tg_poly_clone()
```c
struct tg_poly *tg_poly_clone(const struct tg_poly *poly);
```
Clones a polygon. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.



**Return**

- A duplicate of the provided polygon.


**Note**

- The caller is responsible for freeing with [tg_poly_free()](#group___poly_funcs_1ga7a591cc2298e4e24ae0e99b055a61fed).
- This method of cloning uses implicit sharing through an atomic reference counter.


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1gae68cadbe9ae2fa26c92f010958c6ed11'></a>
## tg_poly_copy()
```c
struct tg_poly *tg_poly_copy(const struct tg_poly *poly);
```
Copies a polygon. 

**Parameters**

- **poly**: Input polygon, caller retains ownership.



**Return**

- A duplicate of the provided polygon.
- NULL if out of memory


**Note**

- The caller is responsible for freeing with [tg_poly_free()](#group___poly_funcs_1ga7a591cc2298e4e24ae0e99b055a61fed).
- This method performs a deep copy of the entire geometry to new memory.


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1gabccc5c197d02868b9d24df7b7cabeea4'></a>
## tg_poly_memsize()
```c
size_t tg_poly_memsize(const struct tg_poly *poly);
```
Returns the allocation size of the polygon. 

**Parameters**

- **poly**: Input polygon



**Return**

- Size of polygon in bytes


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1ga802bb035aa8b2efd234973b98eefb128'></a>
## tg_poly_exterior()
```c
const struct tg_ring *tg_poly_exterior(const struct tg_poly *poly);
```
Returns the exterior ring. 

**Parameters**

- **poly**: Input polygon



**Return**

- Exterior ring


**Note**

- The polygon maintains ownership of the exterior ring.


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1ga23934cf6af1048e2e4b06befef3f8c1f'></a>
## tg_poly_num_holes()
```c
int tg_poly_num_holes(const struct tg_poly *poly);
```
Returns the number of interior holes. 

**Parameters**

- **poly**: Input polygon



**Return**

- Number of holes


**See also**

- [tg_poly_hole_at()](#group___poly_funcs_1gad15090afe596c268858c65b5c5e47a73)
- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1gad15090afe596c268858c65b5c5e47a73'></a>
## tg_poly_hole_at()
```c
const struct tg_ring *tg_poly_hole_at(const struct tg_poly *poly, int index);
```
Returns an interior hole. 

**Parameters**

- **poly**: Input polygon
- **index**: Index of hole



**Return**

- Ring hole


**See also**

- [tg_poly_num_holes()](#group___poly_funcs_1ga23934cf6af1048e2e4b06befef3f8c1f)
- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1gaac2de96544c41331886fd2b48e11ba6d'></a>
## tg_poly_rect()
```c
struct tg_rect tg_poly_rect(const struct tg_poly *poly);
```
Returns the minimum bounding rectangle of a polygon. 

**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___poly_funcs_1gac52d19818b926e8d9e70abb0e3889da8'></a>
## tg_poly_clockwise()
```c
bool tg_poly_clockwise(const struct tg_poly *poly);
```
Returns true if winding order is clockwise. 

**Parameters**

- **poly**: Input polygon



**Return**

- True if clockwise
- False if counter-clockwise


**See also**

- [Polygon functions](#group___poly_funcs)



<a name='group___global_funcs_1gab1e1478a3870e90d6b5932f5e67a032b'></a>
## tg_env_set_allocator()
```c
void tg_env_set_allocator(void *(*malloc)(size_t), void *(*realloc)(void *, size_t), void(*free)(void *));
```
Allow for configuring a custom allocator.

This overrides the built-in malloc, realloc, and free functions for all TG operations. 

**Warning**

- This function, if needed, should be called **only once** at program start up and prior to calling any other tg_*() function.


**See also**

- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1ga57a922edb770400033043354c1f4e80e'></a>
## tg_env_set_index()
```c
void tg_env_set_index(enum tg_index ix);
```
Set the geometry indexing default.

This is a global override to the indexing for all yet-to-be created geometries. 

**Warning**

- This function, if needed, should be called **only once** at program start up and prior to calling any other tg_*() function.


**See also**

- [tg_index](.#tg_index)
- [tg_env_set_index()](#group___global_funcs_1ga57a922edb770400033043354c1f4e80e)
- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1gaf9e9214a8db08c306fdb529192e9dd5f'></a>
## tg_env_set_index_spread()
```c
void tg_env_set_index_spread(int spread);
```
Set the default index spread.

The "spread" is how many rectangles are grouped together on an indexed level before propagating up to a higher level.

Default is 16.

This is a global override to the indexing spread for all yet-to-be created geometries. 

**Warning**

- This function, if needed, should be called **only once** at program start up and prior to calling any other tg_*() function.


**See also**

- [tg_index](.#tg_index)
- [tg_env_set_index()](#group___global_funcs_1ga57a922edb770400033043354c1f4e80e)
- [About TG indexing](POLYGON_INDEXING.md)
- [Global environment](#group___global_funcs)



<a name='group___global_funcs_1gad9af7b45fd9c942f857b1121168f1600'></a>
## tg_env_set_print_fixed_floats()
```c
void tg_env_set_print_fixed_floats(bool print);
```
Set the floating point printing to be fixed size. By default floating points are printed using their smallest textual representation. Such as 800000 is converted to "8e5". This is ideal when both accuracy and size are desired. But there may be times when only fixed epresentations are wanted, in that case set this param to true. 


***

Generated with the help of [doxygen](https://www.doxygen.nl/index.html)
