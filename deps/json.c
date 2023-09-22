// https://github.com/tidwall/json.c
//
// Copyright 2023 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef JSON_STATIC
#include "json.h"
#else
enum json_type { 
    JSON_NULL,
    JSON_FALSE,
    JSON_NUMBER,
    JSON_STRING,
    JSON_TRUE,
    JSON_ARRAY,
    JSON_OBJECT,
};
struct json { void *priv[4]; };

struct json_valid {
    bool valid;
    size_t pos;
};

#define JSON_EXTERN static
#endif

#ifndef JSON_EXTERN
#define JSON_EXTERN
#endif

#ifndef JSON_MAXDEPTH
#define JSON_MAXDEPTH 1024
#endif

struct vutf8res { int n; uint32_t cp; };

// parse and validate a single utf8 codepoint.
// The first byte has already been checked from the vstring function.
static inline struct vutf8res vutf8(const uint8_t data[], int64_t len) {
    uint32_t cp;
    int n = 0;
    if (data[0]>>4 == 14) {
        if (len < 3) goto fail;
        if (((data[1]>>6)|(data[2]>>6<<2)) != 10) goto fail;
        cp = ((uint32_t)(data[0]&15)<<12)|((uint32_t)(data[1]&63)<<6)|
             ((uint32_t)(data[2]&63));
        n = 3;
    } else if (data[0]>>3 == 30) {
        if (len < 4) goto fail;
        if (((data[1]>>6)|(data[2]>>6<<2)|(data[3]>>6<<4)) != 42) goto fail;
        cp = ((uint32_t)(data[0]&7)<<18)|((uint32_t)(data[1]&63)<<12)|
             ((uint32_t)(data[2]&63)<<6)|((uint32_t)(data[3]&63));
        n = 4;
    } else if (data[0]>>5 == 6) {
        if (len < 2) goto fail;
        if (data[1]>>6 != 2) goto fail;
        cp = ((uint32_t)(data[0]&31)<<6)|((uint32_t)(data[1]&63));
        n = 2;
    } else {
        goto fail;
    }
    if (cp < 128) goto fail; // don't allow multibyte ascii characters
    if (cp >= 0x10FFFF) goto fail; // restricted to utf-16
    if (cp >= 0xD800 && cp <= 0xDFFF) goto fail; // needs surrogate pairs
    return (struct vutf8res) { .n = n, .cp = cp };
fail:
    return (struct vutf8res) { 0 };
}

static inline int64_t vesc(const uint8_t *json, int64_t jlen, int64_t i) {
    // The first byte has already been checked from the vstring function.
    i += 1;
    if (i == jlen) return -(i+1);
    switch (json[i]) {
    case '"': case '\\': case '/': case 'b': case 'f': case 'n': 
    case 'r': case 't': return i+1;
    case 'u':
        for (int j = 0; j < 4; j++) {
            i++;
            if (i == jlen) return -(i+1);
            if (!((json[i] >= '0' && json[i] <= '9') ||
                  (json[i] >= 'a' && json[i] <= 'f') ||
                  (json[i] >= 'A' && json[i] <= 'F'))) return -(i+1);
        }
        return i+1;
    }
    return -(i+1);
}

#ifndef ludo
#define ludo
#define ludo1(i,f) f; i++;
#define ludo2(i,f) ludo1(i,f); ludo1(i,f);
#define ludo4(i,f) ludo2(i,f); ludo2(i,f);
#define ludo8(i,f) ludo4(i,f); ludo4(i,f);
#define ludo16(i,f) ludo8(i,f); ludo8(i,f);
#define ludo32(i,f) ludo16(i,f); ludo16(i,f);
#define ludo64(i,f) ludo32(i,f); ludo32(i,f);
#define for1(i,n,f) while(i+1<=(n)) { ludo1(i,f); }
#define for2(i,n,f) while(i+2<=(n)) { ludo2(i,f); } for1(i,n,f);
#define for4(i,n,f) while(i+4<=(n)) { ludo4(i,f); } for1(i,n,f);
#define for8(i,n,f) while(i+8<=(n)) { ludo8(i,f); } for1(i,n,f);
#define for16(i,n,f) while(i+16<=(n)) { ludo16(i,f); } for1(i,n,f);
#define for32(i,n,f) while(i+32<=(n)) { ludo32(i,f); } for1(i,n,f);
#define for64(i,n,f) while(i+64<=(n)) { ludo64(i,f); } for1(i,n,f);
#endif

static const uint8_t strtoksu[256] = {
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
#ifndef JSON_NOVALIDATEUTF8
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,
    //0=ascii, 1=quote, 2=escape, 3=utf82, 4=utf83, 5=utf84, 6=error
#endif
};

static int64_t vstring(const uint8_t *json, int64_t jlen, int64_t i) {
    while (1) {
        for8(i, jlen, { if (strtoksu[json[i]]) goto tok; })
        break;
    tok:
        if (json[i] == '"') {
            return i+1;
#ifndef JSON_NOVALIDATEUTF8
        } else if (json[i] > 127) {
            struct vutf8res res = vutf8(json+i, jlen-i);
            if (res.n == 0) break;
            i += res.n;
#endif
        } else if (json[i] == '\\') {
            if ((i = vesc(json, jlen, i)) < 0) break;
        } else {
            break;
        }
    } 
    return -(i+1);
}

static int64_t vnumber(const uint8_t *data, int64_t dlen, int64_t i) {
    i--;
    // sign
    if (data[i] == '-') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
    }
    // int 
    if (data[i] == '0') {
        i++;
    } else {
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    // frac
    if (i == dlen) return i;
    if (data[i] == '.') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    // exp
    if (i == dlen) return i;
    if (data[i] == 'e' || data[i] == 'E') {
        i++;
        if (i == dlen) return -(i+1);
        if (data[i] == '+' || data[i] == '-') i++;
        if (i == dlen) return -(i+1);
        if (data[i] < '0' || data[i] > '9') return -(i+1);
        i++;
        for (; i < dlen; i++) {
            if (data[i] >= '0' && data[i] <= '9') continue;
            break;
        }
    }
    return i;
}

static int64_t vnull(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+3 <= dlen && data[i] == 'u' && data[i+1] == 'l' &&
        data[i+2] == 'l' ? i+3 : -(i+1);
}

static int64_t vtrue(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+3 <= dlen && data[i] == 'r' && data[i+1] == 'u' &&
        data[i+2] == 'e' ? i+3 : -(i+1);
}

static int64_t vfalse(const uint8_t *data, int64_t dlen, int64_t i) {
    return i+4 <= dlen && data[i] == 'a' && data[i+1] == 'l' &&
        data[i+2] == 's' && data[i+3] == 'e' ? i+4 : -(i+1);
}

static int64_t vcolon(const uint8_t *json, int64_t len, int64_t i) {
    if (i == len) return -(i+1);
    if (json[i] == ':') return i+1;
    do {
        switch (json[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ':': return i+1;
        default: return -(i+1);
        }
    } while (++i < len);
    return -(i+1);
}

static int64_t vcomma(const uint8_t *json, int64_t len, int64_t i, uint8_t end)
{
    if (i == len) return -(i+1);
    if (json[i] == ',') return i;
    do {
        switch (json[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ',': return i;
        default: return json[i] == end ? i : -(i+1);
        }
    } while (++i < len);
    return -(i+1);
}

static int64_t vany(const uint8_t *data, int64_t dlen, int64_t i, int depth);

static int64_t varray(const uint8_t *data, int64_t dlen, int64_t i, int depth) {
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case ']': return i+1;
        default:
            for (; i < dlen; i++) {
                if ((i = vany(data, dlen, i, depth+1)) < 0) return i;
                if ((i = vcomma(data, dlen, i, ']')) < 0) return i;
                if (data[i] == ']') return i+1;
            }
        }
    }
    return -(i+1);
}

static int64_t vkey(const uint8_t *json, int64_t len, int64_t i) {
    for16(i, len, { if (strtoksu[json[i]]) goto tok; })
    return -(i+1);
tok:
    if (json[i] == '"') return i+1;
    return vstring(json, len, i);
}

static int64_t vobject(const uint8_t *data, int64_t dlen, int64_t i, int depth)
{
    for (; i < dlen; i++) {
        switch (data[i]) {
        case '"':
        key:
            if ((i = vkey(data, dlen, i+1)) < 0) return i;
            if ((i = vcolon(data, dlen, i)) < 0) return i;
            if ((i = vany(data, dlen, i, depth+1)) < 0) return i;
            if ((i = vcomma(data, dlen, i, '}')) < 0) return i;
            if (data[i] == '}') return i+1;
            i++;
            for (; i < dlen; i++) {
                switch (data[i]) {
                case ' ': case '\t': case '\n': case '\r': continue;
                case '"': goto key;
                default: return -(i+1);
                }
            }
            return -(i+1);
        case ' ': case '\t': case '\n': case '\r': continue;
        case '}': return i+1;
        default:
            return -(i+1);
        }
    }
    return -(i+1);
}

static int64_t vany(const uint8_t *data, int64_t dlen, int64_t i, int depth) {
    if (depth > JSON_MAXDEPTH) return -(i+1);
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        case '{': return vobject(data, dlen, i+1, depth);
        case '[': return varray(data, dlen, i+1, depth);
        case '"': return vstring(data, dlen, i+1);
        case 't': return vtrue(data, dlen, i+1);
        case 'f': return vfalse(data, dlen, i+1);
        case 'n': return vnull(data, dlen, i+1);
        case '-': case '0': case '1': case '2': case '3': case '4': 
        case '5': case '6': case '7': case '8': case '9':
            return vnumber(data, dlen, i+1);
        }
        break;
    }
    return -(i+1);
}

static int64_t vpayload(const uint8_t *data, int64_t dlen, int64_t i) {
    for (; i < dlen; i++) {
        switch (data[i]) {
        case ' ': case '\t': case '\n': case '\r': continue;
        default:
            if ((i = vany(data, dlen, i, 1)) < 0) return i;
            for (; i < dlen; i++) {
                switch (data[i]) {
                case ' ': case '\t': case '\n': case '\r': continue;
                default: return -(i+1);
                }
            }
            return i;
        }
    }
    return -(i+1);
}

JSON_EXTERN
struct json_valid json_validn_ex(const char *json_str, size_t len, int opts) {
    (void)opts; // for future use
    int64_t ilen = len;
    if (ilen < 0) return (struct json_valid) { 0 };
    int64_t pos = vpayload((uint8_t*)json_str, len, 0);
    if (pos > 0) return (struct json_valid) { .valid = true };
    return (struct json_valid) { .pos = (-pos)-1 };
}

JSON_EXTERN struct json_valid json_valid_ex(const char *json_str, int opts) {
    return json_validn_ex(json_str, json_str?strlen(json_str):0, opts);
}

JSON_EXTERN bool json_validn(const char *json_str, size_t len) {
    return json_validn_ex(json_str, len, 0).valid;
}

JSON_EXTERN bool json_valid(const char *json_str) {
    return json_validn(json_str, json_str?strlen(json_str):0);
}

// don't changes these flags without changing the numtoks table too.
enum iflags { IESC = 1, IDOT = 2, ISCI = 4, ISIGN = 8 };

#define jmake(info, raw, end, len) ((struct json) { .priv = { \
    (void*)(uintptr_t)(info), (void*)(uintptr_t)(raw), \
    (void*)(uintptr_t)(end), (void*)(uintptr_t)(len) } })
#define jinfo(json) ((int)(uintptr_t)((json).priv[0]))
#define jraw(json) ((uint8_t*)(uintptr_t)((json).priv[1]))
#define jend(json) ((uint8_t*)(uintptr_t)((json).priv[2]))
#define jlen(json) ((size_t)(uintptr_t)((json).priv[3]))

static const uint8_t strtoksa[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
};

static inline size_t count_string(uint8_t *raw, uint8_t *end, int *infoout) {
    size_t len = end-raw;
    size_t i = 1;
    int info = 0;
    bool e = false;
    while (1) {
        for8(i, len, {
            if (strtoksa[raw[i]]) goto tok;
            e = false;
        });
        break;
    tok:
        if (raw[i] == '"') {
            i++;
            if (!e) {
                break;
            }
            e = false;
            continue;
        }
        if (raw[i] == '\\') {
            info |= IESC;
            e = !e;
        }
        i++;
    }
    *infoout = info;
    return i;
}

static struct json take_string(uint8_t *raw, uint8_t *end) {
    int info = 0;
    size_t i = count_string(raw, end, &info);
    return jmake(info, raw, end, i);
}

static const uint8_t numtoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,1,0,1,3,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,5,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    // don't changes these flags without changing enum iflags too.
};

static struct json take_number(uint8_t *raw, uint8_t *end) {
    int64_t len = end-raw;
    int info = raw[0] == '-' ? ISIGN : 0;
    int64_t i = 1;
    for16(i, len, {
        if (!numtoks[raw[i]]) goto done;
        info |= (numtoks[raw[i]]-1);
    });
done:
    return jmake(info, raw, end, i);
}

static const uint8_t nesttoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2,0,0,
};

static size_t count_nested(uint8_t *raw, uint8_t *end) {
    size_t len = end-raw;
    size_t i = 1;
    int depth = 1;
    int kind = 0;
    if (i >= len) return i;
    while (depth) {
        for16(i, len, { if (nesttoks[raw[i]]) goto tok0; });
        break;
    tok0:
        kind = nesttoks[raw[i]];
        i++;
        if (kind-1) {
            depth += kind-3;
        } else {
            while (1) {
                for16(i, len, { if (raw[i]=='"') goto tok1; });
                break;
            tok1:
                i++;
                if (raw[i-2] == '\\') {
                    size_t j = i-3;
                    size_t e = 1;
                    while (j > 0 && raw[j] == '\\') {
                        e = (e+1)&1;
                        j--;
                    }
                    if (e) continue;
                }
                break;
            }
        }
    }
    return i;
}

static struct json take_literal(uint8_t *raw, uint8_t *end, size_t litlen) {
    size_t rlen = end-raw;
    return jmake(0, raw, end, rlen < litlen ? rlen : litlen);
}

static struct json peek_any(uint8_t *raw, uint8_t *end) {
    while (raw < end) {
        switch (*raw){
        case '}': case ']': return (struct json){ 0 };
        case '{': case '[': return jmake(0, raw, end, 0);
        case '"': return take_string(raw, end);
        case 'n': return take_literal(raw, end, 4);
        case 't': return take_literal(raw, end, 4);
        case 'f': return take_literal(raw, end, 5);
        case '-': case '0': case '1': case '2': case '3': case '4': case '5': 
        case '6': case '7': case '8': case '9': return take_number(raw, end);
        }
        raw++;
    }
    return (struct json){ 0 };
}

JSON_EXTERN struct json json_first(struct json json) {
    uint8_t *raw = jraw(json);
    uint8_t *end = jend(json);
    if (end <= raw || (*raw != '{' &&  *raw != '[')) return (struct json){0};
    return peek_any(raw+1, end);
}

JSON_EXTERN struct json json_next(struct json json) {
    uint8_t *raw = jraw(json);
    uint8_t *end = jend(json);
    if (end <= raw) return (struct json){ 0 };
    raw += jlen(json) == 0 ? count_nested(raw, end): jlen(json);
    return peek_any(raw, end);
}

JSON_EXTERN struct json json_parsen(const char *json_str, size_t len) {
    if (len > 0 && (json_str[0] == '[' || json_str[0] == '{')) {
        return jmake(0, json_str, json_str+len, 0);
    }
    if (len == 0) return (struct json){ 0 };
    return peek_any((uint8_t*)json_str, (uint8_t*)json_str+len);
}

JSON_EXTERN struct json json_parse(const char *json_str) {
    return json_parsen(json_str, json_str?strlen(json_str):0);
}

JSON_EXTERN bool json_exists(struct json json) {
    return jraw(json) && jend(json);
}

JSON_EXTERN const char *json_raw(struct json json) {
    return (char*)jraw(json);
}

JSON_EXTERN size_t json_raw_length(struct json json) {
    if (jlen(json)) return jlen(json);
    if (jraw(json) < jend(json)) return count_nested(jraw(json), jend(json));
    return 0;
}

static const uint8_t typetoks[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,3,0,0,0,0,0,0,0,0,0,0,2,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,0,0,6,0,0,0,0,
};

JSON_EXTERN enum json_type json_type(struct json json) {
    return jraw(json) < jend(json) ? typetoks[*jraw(json)] : JSON_NULL;
}

JSON_EXTERN struct json json_ensure(struct json json) {
    return jmake(jinfo(json), jraw(json), jend(json), json_raw_length(json));
}

static int strcmpn(const char *a, size_t alen, const char *b, size_t blen) {
    size_t n = alen < blen ? alen : blen;
    int cmp = strncmp(a, b, n);
    if (cmp == 0) {
        cmp = alen < blen ? -1 : alen > blen ? 1 : 0;
    }
    return cmp;
}

static const uint8_t hextoks[256] = { 
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,0,0,0,0,0,0,
    0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static uint32_t decode_hex(const uint8_t *str) {
    return (((int)hextoks[str[0]])<<12) | (((int)hextoks[str[1]])<<8) |
           (((int)hextoks[str[2]])<<4) | (((int)hextoks[str[3]])<<0);
}

static bool is_surrogate(uint32_t cp) {
    return cp > 55296 && cp < 57344;
}

static uint32_t decode_codepoint(uint32_t cp1, uint32_t cp2) {
    return cp1 > 55296  && cp1 < 56320 && cp2 > 56320 && cp2 < 57344 ?
        ((cp1 - 55296) << 10) | ((cp2 - 56320) + 65536) :
        65533;
}

static inline int encode_codepoint(uint8_t dst[], uint32_t cp) {
    if (cp < 128) {
        dst[0] = cp;
        return 1;
    } else if (cp < 2048) {
        dst[0] = 192 | (cp >> 6);
        dst[1] = 128 | (cp & 63);
        return 2;
    } else if (cp > 1114111 || is_surrogate(cp)) {
        cp = 65533; // error codepoint
    }
    if (cp < 65536) {
        dst[0] = 224 | (cp >> 12);
        dst[1] = 128 | ((cp >> 6) & 63);
        dst[2] = 128 | (cp & 63);
        return 3;
    }
    dst[0] = 240 | (cp >> 18);
    dst[1] = 128 | ((cp >> 12) & 63);
    dst[2] = 128 | ((cp >> 6) & 63);
    dst[3] = 128 | (cp & 63);
    return 4;
}

// for_each_utf8 iterates over each UTF-8 bytes in jstr, unescaping along the
// way. 'f' is a loop expression that will make available the 'ch' char which 
// is just a single byte in a UTF-8 series.
#define for_each_utf8(jstr, len, f) { \
    size_t nn = (len); \
    int ch = 0; \
    (void)ch; \
    for (size_t ii = 0; ii < nn; ii++) { \
        if ((jstr)[ii] != '\\') { \
            ch = (jstr)[ii]; \
            if (1) f \
            continue; \
        }; \
        ii++; \
        if (ii == nn) break; \
        switch  ((jstr)[ii]) { \
        case '\\': ch = '\\'; break; \
        case '/' : ch = '/';  break; \
        case 'b' : ch = '\b'; break; \
        case 'f' : ch = '\f'; break; \
        case 'n' : ch = '\n'; break; \
        case 'r' : ch = '\r'; break; \
        case 't' : ch = '\t'; break; \
        case '"' : ch = '"';  break; \
        case 'u' : \
            if (ii+5 > nn) { nn = 0; continue; }; \
            uint32_t cp = decode_hex((jstr)+ii+1); \
            ii += 5; \
            if (is_surrogate(cp)) { \
                if (nn-ii >= 6 && (jstr)[ii] == '\\' && (jstr)[ii+1] == 'u') { \
                    cp = decode_codepoint(cp, decode_hex((jstr)+ii+2)); \
                    ii += 6; \
                } \
            } \
            uint8_t _bytes[4]; \
            int _n = encode_codepoint(_bytes, cp); \
            for (int _j = 0; _j < _n; _j++) { \
                ch = _bytes[_j]; \
                if (1) f \
            } \
            ii--; \
            continue; \
        default: \
            continue; \
        }; \
        if (1) f \
    } \
}

JSON_EXTERN 
int json_raw_comparen(struct json json, const char *str, size_t len) {
    char *raw = (char*)jraw(json);
    if (!raw) raw = "";
    size_t rlen = json_raw_length(json);
    return strcmpn(raw, rlen, str, len);
}

JSON_EXTERN int json_raw_compare(struct json json, const char *str) {
    return json_raw_comparen(json, str, strlen(str));
}

JSON_EXTERN size_t json_string_length(struct json json) {
    size_t len = json_raw_length(json);
    if (json_type(json) != JSON_STRING) {
        return len;
    }
    len = len < 2 ? 0 : len - 2;
    if ((jinfo(json)&IESC) != IESC) {
        return len;
    }
    uint8_t *raw = jraw(json)+1;
    size_t count = 0;
    for_each_utf8(raw, len, { count++; });
    return count;
}

JSON_EXTERN 
int json_string_comparen(struct json json, const char *str, size_t slen) {
    if (json_type(json) != JSON_STRING) {
        return json_raw_comparen(json, str, slen);
    }
    uint8_t *raw = jraw(json);
    size_t rlen = json_raw_length(json);
    raw++;
    rlen = rlen < 2 ? 0 : rlen - 2;
    if ((jinfo(json)&IESC) != IESC) {
        return strcmpn((char*)raw, rlen, str, slen);
    }
    int cmp = 0;
    uint8_t *sp = (uint8_t*)(str ? str : "");
    for_each_utf8(raw, rlen, {
        if (!*sp || ch > *sp) {
            cmp = 1;
            goto done;
        } else if (ch < *sp) {
            cmp = -1;
            goto done;
        }
        sp++;
    });
done:
    if (cmp == 0 && *sp) cmp = -1;
    return cmp;
}

JSON_EXTERN 
int json_string_compare(struct json json, const char *str) {
    return json_string_comparen(json, str, str?strlen(str):0);
}

JSON_EXTERN size_t json_string_copy(struct json json, char *str, size_t n) {
    size_t len = json_raw_length(json);
    uint8_t *raw = jraw(json);
    bool isjsonstr = json_type(json) == JSON_STRING;
    bool isesc = false;
    if (isjsonstr) {
        raw++;
        len = len < 2 ? 0 : len - 2;
        isesc = (jinfo(json)&IESC) == IESC;
    }
    if (!isesc) {
        if (n == 0) return len;
        n = n-1 < len ? n-1 : len;
        memcpy(str, raw, n);
        str[n] = '\0';
        return len;
    }
    size_t count = 0;
    for_each_utf8(raw, len, {
        if (count < n) str[count] = ch;
        count++;
    });
    if (n > count) str[count] = '\0';
    else if (n > 0) str[n-1] = '\0';
    return count;
}

JSON_EXTERN size_t json_array_count(struct json json) {
    size_t count = 0;
    if (json_type(json) == JSON_ARRAY) {
        json = json_first(json);
        while (json_exists(json)) {
            count++;
            json = json_next(json);
        }
    }
    return count;
}

JSON_EXTERN struct json json_array_get(struct json json, size_t index) {
    if (json_type(json) == JSON_ARRAY) {
        json = json_first(json);
        while (json_exists(json)) {
            if (index == 0) return json;
            json = json_next(json);
            index--;
        }
    }
    return (struct json) { 0 };
}

JSON_EXTERN
struct json json_object_getn(struct json json, const char *key, size_t len) {
    if (json_type(json) == JSON_OBJECT) {
        json = json_first(json);
        while (json_exists(json)) {
            if (json_string_comparen(json, key, len) == 0) {
                return json_next(json);
            }
            json = json_next(json_next(json));
        }
    }
    return (struct json) { 0 };
}

JSON_EXTERN struct json json_object_get(struct json json, const char *key) {
    return json_object_getn(json, key, key?strlen(key):0);
}

static double stod(const uint8_t *str, size_t len, char *buf) {
    memcpy(buf, str, len);
    buf[len] = '\0';
    char *ptr;
    double x = strtod(buf, &ptr);
    return (size_t)(ptr-buf) == len ? x : 0;
}

static double parse_double_big(const uint8_t *str, size_t len) {
    char buf[512];
    if (len >= sizeof(buf)) return 0;
    return stod(str, len, buf);
}

static double parse_double(const uint8_t *str, size_t len) {
    char buf[32];
    if (len >= sizeof(buf)) return parse_double_big(str, len);
    return stod(str, len, buf);
}

static int64_t parse_int64(const uint8_t *s, size_t len) {
    char buf[21];
    double y;
    if (len == 0) return 0;
    if (len < sizeof(buf) && sizeof(long long) == sizeof(int64_t)) {
        memcpy(buf, s, len);
        buf[len] = '\0';
        char *ptr = NULL;
        int64_t x = strtoll(buf, &ptr, 10);
        if ((size_t)(ptr-buf) == len) return x;
        y = strtod(buf, &ptr);
        if ((size_t)(ptr-buf) == len) goto clamp;
    }
    y = parse_double(s, len);
clamp:
    if (y < (double)INT64_MIN) return INT64_MIN;
    if (y > (double)INT64_MAX) return INT64_MAX;
    return y;
}

static uint64_t parse_uint64(const uint8_t *s, size_t len) {
    char buf[21];
    double y;
    if (len == 0) return 0;
    if (len < sizeof(buf) && sizeof(long long) == sizeof(uint64_t) &&
        s[0] != '-')
    {
        memcpy(buf, s, len);
        buf[len] = '\0';
        char *ptr = NULL;
        uint64_t x = strtoull(buf, &ptr, 10);
        if ((size_t)(ptr-buf) == len) return x;
        y = strtod(buf, &ptr);
        if ((size_t)(ptr-buf) == len) goto clamp;
    }
    y = parse_double(s, len);
clamp:
    if (y < 0) return 0;
    if (y > (double)UINT64_MAX) return UINT64_MAX;
    return y;
}

JSON_EXTERN double json_double(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 3) return 0.0;
        return parse_double(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_double(jraw(json), jlen(json));
    default:
        return 0.0;
    }
}

JSON_EXTERN int64_t json_int64(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 2) return 0;
        return parse_int64(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_int64(jraw(json), jlen(json));
    default:
        return 0;
    }
}

JSON_EXTERN int json_int(struct json json) {
    int64_t x = json_int64(json);
    if (x < (int64_t)INT_MIN) return INT_MIN;
    if (x > (int64_t)INT_MAX) return INT_MAX;
    return x;
}

JSON_EXTERN uint64_t json_uint64(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return 1;
    case JSON_STRING:
        if (jlen(json) < 2) return 0;
        return parse_uint64(jraw(json)+1, jlen(json)-2);
    case JSON_NUMBER:
        return parse_uint64(jraw(json), jlen(json));
    default:
        return 0;
    }
}

JSON_EXTERN bool json_bool(struct json json) {
    switch (json_type(json)) {
    case JSON_TRUE:
        return true;
    case JSON_NUMBER:
         return json_double(json) != 0.0; 
    case JSON_STRING: {
        char *trues[] = { "1", "t", "T", "true", "TRUE", "True" };
        for (size_t i = 0; i < sizeof(trues)/sizeof(char*); i++) {
            if (json_string_compare(json, trues[i]) == 0) return true;
        }
        return false;
    }
    default:
        return false;
    }
}

struct jesc_buf { 
    uint8_t *esc;
    size_t esclen;
    size_t count;
};

static void jesc_append(struct jesc_buf *buf, uint8_t ch) {
    if (buf->esclen > 1) { 
        *(buf->esc++) = ch;
        buf->esclen--; 
    }
    buf->count++;
}
static void jesc_append2(struct jesc_buf *buf, uint8_t c1, uint8_t c2) {
    jesc_append(buf, c1);
    jesc_append(buf, c2);
}

static const uint8_t hexchars[] = "0123456789abcdef";

static void 
jesc_append_ux(struct jesc_buf *buf, uint8_t c1, uint8_t c2, uint16_t x) {
    jesc_append2(buf, c1, c2);
    jesc_append2(buf, hexchars[x>>12&0xF], hexchars[x>>8&0xF]);
    jesc_append2(buf, hexchars[x>>4&0xF], hexchars[x>>0&0xF]);
}

JSON_EXTERN
size_t json_escapen(const char *str, size_t len, char *esc, size_t n) {
    uint8_t cpbuf[4];
    struct jesc_buf buf  = { .esc = (uint8_t*)esc, .esclen = n };
    jesc_append(&buf, '"');
    for (size_t i = 0; i < len; i++) {
        uint32_t c = (uint8_t)str[i];
        if (c < ' ') {
            switch (c) {
            case '\n': jesc_append2(&buf, '\\', 'n'); break;
            case '\b': jesc_append2(&buf, '\\', 'b'); break;
            case '\f': jesc_append2(&buf, '\\', 'f'); break;
            case '\r': jesc_append2(&buf, '\\', 'r'); break;
            case '\t': jesc_append2(&buf, '\\', 't'); break;
            default: jesc_append_ux(&buf, '\\', 'u', c);
            }
        } else if (c == '>' || c == '<' || c == '&') {
            // make web safe
            jesc_append_ux(&buf, '\\', 'u', c);
        } else if (c == '\\') {
            jesc_append2(&buf, '\\', '\\');
        } else if (c == '"') {
            jesc_append2(&buf, '\\', '"');
        } else if (c > 127) {
            struct vutf8res res = vutf8((uint8_t*)(str+i), len-i);
            if (res.n == 0) {
                res.n = 1;
                res.cp = 0xfffd;
            }
            int cpn = encode_codepoint(cpbuf, res.cp);
            for (int j = 0; j < cpn; j++) {
                jesc_append(&buf, cpbuf[j]);
            }
            i = i + res.n - 1;
        } else {
            jesc_append(&buf, str[i]);
        }
    }
    jesc_append(&buf, '"');
    if (buf.esclen > 0) {
        // add to null terminator
        *(buf.esc++) = '\0';
        buf.esclen--;
    }
    return buf.count;
}

JSON_EXTERN size_t json_escape(const char *str, char *esc, size_t n) {
    return json_escapen(str, str?strlen(str):0, esc, n);
}

JSON_EXTERN
struct json json_getn(const char *json_str, size_t len, const char *path) {
    if (!path) return (struct json) { 0 };
    struct json json = json_parsen(json_str, len);
    int i = 0;
    bool end = false;
    char *p = (char*)path;
    for (; !end && json_exists(json); i++) {
        // get the next component
        const char *key = p;
        while (*p && *p != '.') p++;
        size_t klen = p-key;
        if (*p == '.') p++;
        else if (!*p) end = true;
        enum json_type type = json_type(json);
        if (type == JSON_OBJECT) {
            json = json_object_getn(json, key, klen);
        } else if (type == JSON_ARRAY) {
            if (klen == 0) { i = 0; break; }
            char *end;
            size_t index = strtol(key, &end, 10);
            if (*end && *end != '.') { i = 0; break; }
            json = json_array_get(json, index);
        } else {
            i = 0; 
            break;
        }
    }
    return i == 0 ? (struct json) { 0 } : json;
}

JSON_EXTERN struct json json_get(const char *json_str, const char *path) {
    return json_getn(json_str, json_str?strlen(json_str):0, path);
}

JSON_EXTERN bool json_string_is_escaped(struct json json) {
    return (jinfo(json)&IESC) == IESC;
}
