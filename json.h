#ifndef JSON_H__
#define JSON_H__

#include <stddef.h> /* size_t */

typedef enum {
    TYPE_NULL,
    TYPE_FALSE,
    TYPE_TRUE,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_ARRAY,
    TYPE_OBJECT
} json_type;

#define KEY_NOT_EXIST ((size_t)-1)

typedef struct json_value json_value;
typedef struct json_object json_object;

struct json_value {
    union {
        struct { json_object* m; size_t size, capacity; }o; /* object */
        struct { json_value* e; size_t size, capacity; }a;  /* array */
        struct { char* s; size_t len; }s; /* string: null-terminated string, string length */
        double n;                         /* number */
    }u;
    json_type type;
};

struct json_object {
    char *k; size_t klen; /* key string, key string length */
    json_value v;         /* value */
};

enum {
    PARSE_OK = 0,
    PARSE_EXPECT_VALUE,
    PARSE_INVALID_VALUE,
    PARSE_ROOT_NOT_SINGULAR,
    PARSE_NUMBER_TOO_BIG,
    PARSE_MISS_QUOTATION_MARK,
    PARSE_INVALID_STRING_ESCAPE,
    PARSE_INVALID_STRING_CHAR,
    PARSE_INVALID_UNICODE_HEX,
    PARSE_INVALID_UNICODE_SURROGATE,
    PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    PARSE_MISS_KEY,
    PARSE_MISS_COLON,
    PARSE_MISS_COMMA_OR_CURLY_BRACKET,
    STRINGIFY_OK
};

#define init_value(v) do { (v)->type = TYPE_NULL; } while(0)

int parse(json_value* v, const char* json);

json_type get_type(const json_value* v);

void free_value(json_value* v);

#define set_null(v) free_value(v)

double get_number(const json_value* v);
void set_number(json_value* v, double n);

const char* get_string(const json_value* v);
void set_string(json_value* v, const char* s, size_t len);
size_t get_string_length(const json_value* v);

int get_boolean(const json_value* v);
void set_boolean(json_value* v, int b);

size_t get_array_size(json_value* v);
size_t get_array_capacity(json_value* v);
json_value* get_array_element(json_value* v, size_t index);
void set_array(json_value* v, size_t capacity);
void reserve_array(json_value* v, size_t capacity);
json_value* insert_array_element(json_value* v, size_t index);
json_value* pushback_array_element(json_value* v);
void popback_array_element(json_value* v);
void erase_array_element(json_value* v, size_t index, size_t count);
void clear_array(json_value* v);
void shrink_array(json_value* v);

size_t get_object_size(const json_value* v);
size_t get_object_capacity(json_value* v);
const char* get_object_key(const json_value* v, size_t index);
size_t get_object_key_length(const json_value* v, size_t index);
json_value* get_object_value(const json_value* v, size_t index);
size_t find_object_index(const json_value* v, const char* key, size_t klen);
json_value* find_object_value(const json_value* v, const char* key, size_t klen);
void set_object(json_value* v, size_t capacity);
json_value* set_object_value(json_value* v, const char* key, size_t klen);
void clear_object(json_value* v);
void shrink_object(json_value* v);
void reserve_object(json_value* v, size_t capacity);
void remove_object_value(json_value* v, size_t index);

char* stringify(json_value* v, size_t *length);

void copy(json_value* dst, const json_value* src);
void move(json_value* dst, json_value* src); 
void swap(json_value* lhs, json_value* rhs);

#endif /* JSON_H__ */