#include "json.h"

#include <assert.h> /* assert() */
#include <stdlib.h> /* NULL, strtod(), realloc(), malloc, free() */
#include <errno.h>  /* errno, ERANGE */
#include <math.h>   /* HUGE_VAL */
#include <string.h> /* memcpy(), memcmp() */
#include <stdio.h>  /* sprintf() */

#ifndef PARSE_STACK_INIT_SIZE
#define PARSE_STACK_INIT_SIZE 256
#endif

#ifndef PARSE_STRINGIFY_STACK_INIT_SIZE
#define PARSE_STRINGIFY_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char *)context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len)     memcpy(context_push(c, len), s, len)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}context;

static void* context_push(context* c, size_t size) {
    assert(size > 0);
    void* ret;
    if (c->top + size >= c->size) {
        if (c->size == 0) {
            c->size = PARSE_STACK_INIT_SIZE;
        }
        while (c->top + size >= c->size) {
            c->size += c->size >> 1;  /* c->size * 1.5 */
        }
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* context_pop(context* c, size_t size) {
    assert(c->top >= size);
    c->top -= size;
    return c->stack + c->top;
}

static void parse_whitespace(context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
        p++;
    }
    c->json = p;
}

static int parse_literal(context* c, json_value* v, const char* literal, json_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1] != '\0'; i++) {
        if (c->json[i] != literal[i + 1]) {
            return PARSE_INVALID_VALUE;
        }
    }
    c->json += i;
    v->type = type;
    return PARSE_OK;
}

static int parse_number(context* c, json_value* v) {
    const char* p = c->json;

    if (*p == '-') {
        p++;
    }
    if (*p == '0') {
        p++;
    }
    else {
        if (!ISDIGIT1TO9(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }

    if (*p == '.') {
        p++;

        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }

    if (*p == 'e' || *p == 'E') {
        p++;

        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }

    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL)) {
        return PARSE_NUMBER_TOO_BIG;
    }
    v->type = TYPE_NUMBER;
    c->json = p;
    return PARSE_OK;
}

void free_value(json_value* v) {
    assert(v != NULL);
    size_t i;
    switch (v->type) {
        case TYPE_STRING:
            free(v->u.s.s);
            break;
        case TYPE_ARRAY:
            for (i = 0; i < v->u.a.size; i++) {
                free_value(&v->u.a.e[i]);
            }
            free(v->u.a.e);
            break;
        case TYPE_OBJECT:
            for (i = 0; i < v->u.o.size; i++) {
                free(v->u.o.m[i].k);
                free_value(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default:
            break;
    }
    v->type = TYPE_NULL;
}

void set_string(json_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    free_value(v);
    v->u.s.s = (char *)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = TYPE_STRING;
}

static const char* parse_hex4(const char* p, unsigned int* u) {
    size_t i;
    *u = 0;
    for (i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;

        if (ch >= '0' && ch <= '9') {
            *u |= ch - '0';
        }
        else if (ch >= 'A' && ch <= 'F') {
            *u |= ch - 'A' + 10;
        }
        else if (ch >= 'a' && ch <= 'f') {
            *u |= ch - 'a' + 10;
        }
        else {
            return NULL;
        }
    }
    return p;
}

static void encode_utf8(context* c, unsigned int u) {
    if (u <= 0x7F) {
        PUTC(c, u & 0xFF);
    }
    else if (u <= 0x07FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | (u        & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >> 6)  & 0x3F));
        PUTC(c, 0x80 | (u         & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >> 6)  & 0x3F));
        PUTC(c, 0x80 | (u         & 0x3F));
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int parse_string_raw(context* c, char** str, size_t* len) {
    size_t head = c->top;
    const char* p;
    unsigned int u = 0;
    unsigned int u2 = 0;
    EXPECT(c, '\"');
    p = c->json;

    for (;;) {
        char ch = *p++;
        switch(ch) {
            case '\"':
                *len = c->top - head;
                *str = (char*)context_pop(c, *len);
                c->json = p;
                return PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/');  break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                        if (!(p = parse_hex4(p, &u))) {
                            STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                        }
                        if (u >= 0xD800 && u <= 0xDBFF) {
                            if (*p++ != '\\') {
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            } 
                            if (*p++ != 'u') {
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            if (!(p = parse_hex4(p, &u2))) {
                                STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                            }
                            if (u2 < 0xDC00 || u2 > 0xDFFF) {
                                STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            }
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)(ch) < 0x20) {
                    STRING_ERROR(PARSE_INVALID_STRING_CHAR);
                }
                PUTC(c, ch);
        }
    }
}

static int parse_string(context* c, json_value* v) {
    int ret;
    char* s;
    size_t len;
    if ((ret = parse_string_raw(c, &s, &len)) == PARSE_OK) {
        set_string(v, s, len);
    }
    return ret;
}

static int parse_value(context* c, json_value* v);

static int parse_array(context* c, json_value* v) {
    size_t size = 0;
    int ret = 0;
    size_t i = 0;
    EXPECT(c, '[');
    parse_whitespace(c);
    if (*c->json == ']') {
        c->json++;
        v->type = TYPE_ARRAY;
        v->u.a.size = 0;
        v->u.a.e = NULL;
        return PARSE_OK;
    }

    for (;;) {
        json_value e;
        init_value(&e);
        if ((ret = parse_value(c, &e)) != PARSE_OK) {
            break;
        }

        memcpy(context_push(c, sizeof(json_value)), &e, sizeof(json_value));
        size++;

        parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            parse_whitespace(c);
        } 
        else if (*c->json == ']') {
            c->json++;
            set_array(v, size);
            v->u.a.size = size;
            size = size * (sizeof(json_value));
            memcpy(v->u.a.e, context_pop(c, size), size);
            return PARSE_OK;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }

    for (i = 0; i < size; i++) {
        free_value((json_value*)context_pop(c, sizeof(json_value)));
    }
    v->type = TYPE_NULL;
    return ret;
}

static int parse_object(context* c, json_value* v) {
    size_t size = 0;
    size_t i;
    json_object o;
    int ret;
    EXPECT(c, '{');
    parse_whitespace(c);
    if (*c->json == '}') {
        c->json++;
        v->type = TYPE_OBJECT;
        v->u.o.m = NULL;
        v->u.o.size = 0;
        return PARSE_OK;
    }

    o.k = NULL;
    for (;;) {
        char *str;
        init_value(&o.v);

        /* parse key */
        if (*c->json != '\"') {
            ret =  PARSE_MISS_KEY;
            break;
        }
      
        if ((ret = parse_string_raw(c, &str, &o.klen)) != PARSE_OK) {
            break;
        }
        o.k = (char*)malloc(o.klen + 1);
        memcpy(o.k, str, o.klen);
        o.k[o.klen] = '\0';

        parse_whitespace(c);
        if (*c->json != ':') {
            ret =  PARSE_MISS_COLON;
            break;
        }
        c->json++;
        parse_whitespace(c);

        /* parse value */
        if ((ret = parse_value(c, &o.v) != PARSE_OK)) {
            break;
        }

        memcpy(context_push(c, sizeof(json_object)), &o, sizeof(json_object));
        size++;
        o.k = NULL;
        parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            parse_whitespace(c);
        } 
        else if (*c->json == '}') {
            size_t s = size * sizeof(json_object);
            c->json++;
            set_object(v, size);
            v->u.o.size = size;
            size = size * sizeof(json_object);
            memcpy(v->u.o.m, context_pop(c, size), size);
            return PARSE_OK;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
      
    }
    free(o.k);

    for (i = 0; i < size; i++) {
        json_object* o = (json_object*)context_pop(c, sizeof(json_object));
        free(o->k);
        free_value(&o->v);
    }
    return ret;
}

static int parse_value(context* c, json_value* v) {
    switch (*c->json) {
        case 'n':  return parse_literal(c, v, "null", TYPE_NULL);
        case 't':  return parse_literal(c, v, "true", TYPE_TRUE);
        case 'f':  return parse_literal(c, v, "false", TYPE_FALSE);
        case '\"': return parse_string(c, v); 
        case '[':  return parse_array(c, v);
        case '{':  return parse_object(c, v);
        case '\0': return PARSE_EXPECT_VALUE;
        default:   return parse_number(c, v);
    }
}

int parse(json_value* v, const char* json) {
    context c;
    int ret;
    assert(v);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    init_value(v);
    parse_whitespace(&c);
    if ((ret = parse_value(&c, v)) == PARSE_OK) {
        parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = TYPE_NULL;
            ret = PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

json_type get_type(const json_value* v) {
    assert(v != NULL);
    return v->type;
}

size_t get_string_length(const json_value* v) {
    assert(v != NULL && v->type == TYPE_STRING);
    return v->u.s.len;
}

const char* get_string(const json_value* v) {
    assert(v != NULL && v->type == TYPE_STRING);
    return v->u.s.s;
}

void set_boolean(json_value* v, int b) {
    free_value(v);
    v->type = b ? TYPE_TRUE : TYPE_FALSE;
}

int get_boolean(const json_value* v) {
    assert(v != NULL && (v->type == TYPE_TRUE || v->type == TYPE_FALSE));
    return v->type == TYPE_TRUE;
}

double get_number(const json_value* v) {
    assert(v != NULL && v->type == TYPE_NUMBER);
    return v->u.n;
}

void set_number(json_value* v, double n) {
    free_value(v);
    v->u.n = n;
    v->type = TYPE_NUMBER;
}

size_t get_array_size(json_value* v) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    return v->u.a.size;
}

json_value* get_array_element(json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

void set_array(json_value* v, size_t capacity) {
    assert(v != NULL);
    free_value(v);
    v->type = TYPE_ARRAY;
    v->u.a.size = 0;
    v->u.a.capacity = capacity;
    v->u.a.e = capacity > 0 ? (json_value*)malloc(sizeof(json_value) * capacity) : NULL;
}

size_t get_array_capacity(json_value* v) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    return v->u.a.capacity;
}

void reserve_array(json_value* v, size_t capacity) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    if (v->u.a.capacity < capacity) {
        v->u.a.capacity = capacity;
        v->u.a.e = (json_value*)realloc(v->u.a.e, sizeof(json_value) * capacity);
    }
}

void shrink_array(json_value* v) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    if (v->u.a.capacity > v->u.a.size) {
        v->u.a.capacity = v->u.a.size;
        v->u.a.e = (json_value*)realloc(v->u.a.e, sizeof(json_value) * v->u.a.capacity);
    }
}

json_value* pushback_array_element(json_value* v) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    if (v->u.a.size == v->u.a.capacity) {
        reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    }
    init_value(&v->u.a.e[v->u.a.size]);
    return &v->u.a.e[v->u.a.size++];
}

void popback_array_element(json_value* v) {
    assert(v != NULL && v->type == TYPE_ARRAY && v->u.a.size > 0);
    free_value(&v->u.a.e[--v->u.a.size]);
}

json_value* insert_array_element(json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    assert(index < v->u.a.size);
    size_t i;
    if (v->u.a.size == v->u.a.capacity) {
        reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    }
    v->u.a.size += 1;
    for (i = v->u.a.size; i > index; i--) {
        v->u.a.e[i] = v->u.a.e[i - 1];
    }
    return &v->u.a.e[index];
}

void erase_array_element(json_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == TYPE_ARRAY);
    assert(index + count <= v->u.a.size);
    size_t i;
    for (i = 0; i < count; i++) {
        free_value(&v->u.a.e[index + i]);
    }
    for (i = index + count; i < v->u.a.size; i++) {
        v->u.a.e[i - count] = v->u.a.e[i];
    }
    v->u.a.size -= count;
}

 void clear_array(json_value* v) {
     assert(v != NULL && v->type == TYPE_ARRAY);
     erase_array_element(v, 0, v->u.a.size);
 }

size_t get_object_size(const json_value* v) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    return v->u.o.size;
}

const char* get_object_key(const json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t get_object_key_length(const json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

json_value* get_object_value(const json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}

size_t find_object_index(const json_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    size_t i;
    for (i = 0; i < v->u.o.size; i++) {
        if (klen == v->u.o.m[i].klen && memcmp(v->u.o.m[i].k, key, klen) == 0) {
            return i;
        }
    }
    return KEY_NOT_EXIST;
}

json_value* find_object_value(const json_value* v, const char* key, size_t klen) {
    size_t index = find_object_index(v, key, klen);
    return (index != KEY_NOT_EXIST) ? &v->u.o.m[index].v : NULL;
}

void set_object(json_value* v, size_t capacity) {
    assert(v != NULL);
    free_value(v);
    v->type = TYPE_OBJECT;
    v->u.o.size = 0;
    v->u.o.capacity = capacity;
    v->u.o.m = capacity > 0 ? (json_object*)malloc(sizeof(json_object) * capacity) : NULL;
}

size_t get_object_capacity(json_value* v) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    return v->u.o.capacity;
}

void reserve_object(json_value* v, size_t capacity) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    if (v->u.o.capacity < capacity) {
        v->u.o.capacity = capacity;
        v->u.o.m = (json_object*)realloc(v->u.o.m, sizeof(json_object) * capacity);
    }
}

void shrink_object(json_value* v) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    if (v->u.o.capacity > v->u.o.size) {
        v->u.o.capacity = v->u.o.size;
        v->u.o.m = (json_object*)realloc(v->u.o.m, sizeof(json_object) * v->u.o.capacity);
    }
}

json_value* set_object_value(json_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == TYPE_OBJECT && key != NULL);
    json_value* ret;
    json_object* o;
    if ((ret = find_object_value(v, key, klen)) != NULL) {
        return ret;
    }
    if (v->u.o.size == v->u.o.capacity) {
        reserve_object(v, v->u.o.capacity == 0 ? 1 : v->u.o.capacity * 2);
    }
    o = &v->u.o.m[v->u.o.size];
    v->u.o.size += 1;
    o->klen = klen;
    o->k = (char*)malloc(klen + 1);
    memcpy(o->k, key, klen);
    o->k[klen] = '\0';
    init_value(&o->v);
    return &o->v;
}

void clear_object(json_value* v) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    size_t i;
    for (i = 0; i < v->u.o.size; i++) {
        free(v->u.o.m[i].k);
        free_value(&v->u.o.m[i].v);
    }
    v->u.o.size = 0;
}

void remove_object_value(json_value* v, size_t index) {
    assert(v != NULL && v->type == TYPE_OBJECT);
    assert(index < v->u.o.size);
    size_t i;
    free(v->u.o.m[index].k);
    free_value(&v->u.o.m[index].v);
    for (i = index; i < v->u.o.size - 1; i++) {
        v->u.o.m[i] = v->u.o.m[i + 1];
    }
    v->u.o.size -= 1;
}

void copy(json_value* dst, const json_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    size_t i = 0;
    switch (src->type) {
        case TYPE_STRING:
            set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case TYPE_ARRAY:
            dst->type = TYPE_ARRAY;
            dst->u.a.size = src->u.a.size;
            dst->u.a.e = (json_value*)malloc(dst->u.a.size * sizeof(json_value));
            for (i = 0; i < src->u.a.size; i++) {
                init_value(&dst->u.a.e[i]);
                copy(&dst->u.a.e[i], &src->u.a.e[i]);
            }
            break;
        case TYPE_OBJECT:
            dst->type = TYPE_OBJECT;
            dst->u.o.size = src->u.o.size;
            dst->u.o.m = (json_object*)malloc(dst->u.o.size * sizeof(json_object));
            for (i = 0; i < src->u.a.size; i++) {
                json_object* o1 = &dst->u.o.m[i];
                json_object* o2 = &src->u.o.m[i];
                o1->klen = o2->klen;
                o1->k = (char*)malloc(o1->klen + 1);
                memcpy(o1->k, o2->k, o1->klen);
                o1->k[o1->klen] = '\0';
                init_value(&o1->v);
                copy(&o1->v, &o2->v);
            }
            break;
        default:
            free_value(dst);
            memcpy(dst, src, sizeof(json_value));
            break;
    }
}

void move(json_value* dst, json_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    free_value(dst);
    memcpy(dst, src, sizeof(json_value));
    init_value(src);
}

void swap(json_value* lhs, json_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        json_value temp;
        memcpy(&temp, lhs, sizeof(json_value));
        memcpy(lhs,   rhs, sizeof(json_value));
        memcpy(rhs, &temp, sizeof(json_value));
    }
}

int is_equal(const json_value* lhs, const json_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    size_t i;
    if (lhs->type != rhs->type) {
        return 0;
    }
    switch (lhs->type) {
        case TYPE_NUMBER:
            return lhs->u.n == rhs->u.n;
        case TYPE_STRING:
            return lhs->u.s.len == rhs->u.s.len &&
                memcmp(lhs->u.s.s, rhs->u.s.s, rhs->u.s.len) == 0;
        case TYPE_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size) {
                return 0;
            }
            for (i = 0; i < lhs->u.a.size; i++) {
                if (!is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i])) {
                    return 0;
                }
            }
            return 1;
        case TYPE_OBJECT:
            if (lhs->u.o.size != rhs->u.o.size) {
                return 0;
            }
            for (i = 0; i < lhs->u.o.size; i++) {
                json_value* v = find_object_value(rhs, lhs->u.o.m[i].k, lhs->u.o.m[i].klen);
                if (v == NULL) {
                    return 0;
                }
                if (!is_equal(&lhs->u.o.m[i].v, v)) {
                    return 0;
                }
            }
            return 1;
        default:
            return 1;
    }
}

static void stringify_string(context* c, char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    char *head;
    char *p;
    size_t size;
    size_t i;
    assert(s != NULL);
    size = len * 6 + 2;
    p = head = (char*)context_push(c, size);
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                } 
                else {
                    *p++ = s[i];
                }
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

static void stringify_value(context* c, json_value* v) {
    size_t i;
    switch (v->type) {
        case TYPE_NULL:   PUTS(c, "null",  4); break;
        case TYPE_TRUE:   PUTS(c, "true",  4); break;
        case TYPE_FALSE:  PUTS(c, "false", 5); break;
        case TYPE_STRING: 
            stringify_string(c, v->u.s.s, v->u.s.len);
            break;
        case TYPE_NUMBER: 
            { 
                int length = sprintf(context_push(c, 32), "%.17g", v->u.n);
                c->top -= 32 - length;
            }
            break;
        case TYPE_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0) {
                    PUTC(c, ',');
                }
                stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case TYPE_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0) {
                    PUTC(c, ',');
                }
                stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

char* stringify(json_value* v, size_t *length) {
    context c;
    int ret;
    assert(v != NULL);
    c.stack = (char*)malloc(PARSE_STRINGIFY_STACK_INIT_SIZE);
    c.size = PARSE_STRINGIFY_STACK_INIT_SIZE;
    c.top = 0;
    stringify_value(&c, v);
    if (length) {
        *length = c.top;
    }
    PUTC(&c, '\0');
    return c.stack;
}
