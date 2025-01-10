# GeoBIN

A simple binary format for GeoJSON.

## Features

- Geometry data uses standard WKB format
- Includes the geometry bounds (MBR)
- All JSON members are preserved
- Only little endian WKB allowed

#### Goals

To provide a simple lossless format for storing GeoJSON as binary, and to have
the MBR easily accessible for spatial indexing.

It's a non-goal to have an emphasis on minimizing sizes, other that what is
already provided by converting JSON into WKB. In fact, GeoBIN will be a tiny
bit larger than WKB for non-point types. Roughly 34 bytes larger in most cases.

## The Format

The GeoBIN format consists of the Head byte, MBR Section, Extra JSON Section, 
and the geometries WKB. In the case of a FeatureCollection, each Feature is 
also represented as GeoBIN.

### Head byte

- 0x01: Point WKB (includes this byte)
- 0x02: Non-point Geometry
- 0x03: GeoJSON Feature
- 0x04: GeoJSON FeatureCollection

### MBR Section

All binary except 0x01 has an MBR that immediately follows the head byte.

The MBR format starts with a single byte integer that is the number of
dimensions, followed by a series of coordinates that define the rectangle.

#### 2D MBR

- 1-byte integer: number of dimensions (2)
- 8-byte float[4]: `[xmin, ymin, xmax, ymax]`

#### 3D MBR

- 1-byte integer: number of dimensions (3)
- 8-byte float[6]: `[xmin, ymin, zmin, xmax, ymax, zmax]`

#### 4D MBR

- 1-byte integer: number of dimensions (4)
- 8-byte float[8]: `[xmin, ymin, zmin, mmin, xmax, ymax, zmax, mmax]`

GeoBIN only supports 2, 3, or 4 dimensions.

### Extra JSON Section

GeoJSON is allowed to have any number of extra json members, other than the 
standard "type", "coordinates", "geometries", and "features". These extra json
members must be maintained in order to preserve the original GeoJSON object.

Immediately after the MBR Section is the Extra JSON Section, which is a 
null-terminated string.

For example given this GeoJSON.

```json
{
    "type": "Feature", 
    "id": 1934,
    "geometry": { "type": "Point", "coordinates": [-112, 33] },
    "properties": {
        "terrain": "desert"
    }
}
```

The "id" and "properties" are the 'extra json', and should be extracted into 
its own mini json document.

```json
{"id":1934,"properties":{"terrain":"desert"}}
```

An empty string is just the '\0' null character indicating that there is no
extra json.

It's required that when provided the extra json is valid json.

### 0x01: Point WKB

When the head byte is 0x01, the entire binary is a WKB Point.
The MBR must be cacluated from the WKB, which shouldn't be too difficult since
the WKB has its own head byte specifying dimensionality.

### 0x02: Non-point Geometry

When the head byte is 0x02, the binary will have an MBR, Extra JSON, and WKB.
Immediately after the Extra JSON Section is the raw geometry WKB. 

### 0x03: Feature

Just like a 0x02, but indicates that the geometry is wrapped in a Feature
object.

The following two are effictively the same thing, except that the first uses
0x02 and the second uses 0x03.

```json
{ "type": "LineString", "coordinates": [[10, 10], [20, 20]] }
```

```json
{
    "type": "Feature", 
    "geometry": { "type": "LineString", "coordinates": [[10, 10], [20, 20]] }
}
```

### 0x04: FeatureCollection

The 0x04 head byte means that the original GeoJSON is a FeatureCollection.
It's just like 0x02 except that after the Extra JSON Section, in lieu of raw
WKB, is a list of Features.

This list starts with the count, followed by the features. Each feature is a 
complete GeoBin object.

- 4-byte integer: number of features
- N-features: series of features. Each a valid GeoBIN object.

---

GeoBIN support is provided by [TG](https://github.com/tidwall/tg).
