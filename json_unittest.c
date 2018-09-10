#include <stdio.h>  /* printf(), fprintf() */
#include <stdlib.h> /* free */
#include "json.h"

static int main_ret = 0;
static int test_count = 0;
static int test_pass = 0;

#define EXPECT_EQ_BASE(equality, expect, actual, format) \
    do {\
        test_count++;\
        if (equality) {\
            test_pass++;\
        }\
        else {\
            fprintf(stderr, "%s:%d: expect: " format " actual: " format "\n", __FILE__, __LINE__, expect, actual);\
            main_ret = -1;\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ_BASE((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_EQ_STRING(expect, actual, length)\
    EXPECT_EQ_BASE(sizeof(expect) - 1 == length && memcmp(expect, actual, length) == 0, expect, actual, "%s")
#define EXPECT_TRUE(actual) EXPECT_EQ_BASE((actual) != 0, "true", "false", "%s")
#define EXPECT_FALSE(actual) EXPECT_EQ_BASE((actual) == 0, "false", "true", "%s")
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ_BASE((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")

static void test_parse_null(void) {
    json_value v;
    init_value(&v);
    set_boolean(&v, 0);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "null"));
    EXPECT_EQ_INT(TYPE_NULL, get_type(&v));
    free_value(&v);
}

static void test_parse_true(void) {
    json_value v;
    init_value(&v);
    set_boolean(&v, 0);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "true"));
    EXPECT_EQ_INT(TYPE_TRUE, get_type(&v));
    free_value(&v);
}

static void test_parse_false(void) {
    json_value v;
    init_value(&v);
    set_boolean(&v, 1);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "false"));
    EXPECT_EQ_INT(TYPE_FALSE, get_type(&v));
    free_value(&v);
}

#define TEST_ERROR(error, json)\
    do {\
        json_value v;\
        v.type = TYPE_FALSE;\
        EXPECT_EQ_INT(error, parse(&v, json));\
        EXPECT_EQ_INT(TYPE_NULL, get_type(&v));\
    } while(0)

static void test_parse_expect_value(void) {
    TEST_ERROR(PARSE_EXPECT_VALUE, "");
    TEST_ERROR(PARSE_EXPECT_VALUE, " ");
}

static void test_parse_invalid_value(void) {
    TEST_ERROR(PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(PARSE_INVALID_VALUE, "?");

    /* invaild number */
    TEST_ERROR(PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */ 
    TEST_ERROR(PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */  
    TEST_ERROR(PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(PARSE_INVALID_VALUE, "nan");

    /* invalid value in array */
    TEST_ERROR(PARSE_INVALID_VALUE, "[1,]");
    TEST_ERROR(PARSE_INVALID_VALUE, "[\"a\", nul]");
}

static void test_parse_root_not_singular(void) {
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "null x");

    /* invaild number */
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0123"); /* after zero should be '.' or nothing */
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0x123");
}

#define TEST_NUMBER(expect, json)\
    do {\
        json_value v;\
        init_value(&v);\
        EXPECT_EQ_INT(PARSE_OK, parse(&v, json));\
        EXPECT_EQ_INT(TYPE_NUMBER, get_type(&v));\
        EXPECT_EQ_DOUBLE(expect, get_number(&v));\
    } while(0)

static void test_parse_number(void) {
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1415, "3.1415");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000");

    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}

static void test_parse_number_too_big(void) {
    TEST_ERROR(PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(PARSE_NUMBER_TOO_BIG, "-1e309");
}

#define TEST_STRING(expect, json)\
    do {\
        json_value v;\
        init_value(&v);\
        EXPECT_EQ_INT(PARSE_OK, parse(&v, json));\
        EXPECT_EQ_STRING(expect, get_string(&v), get_string_length(&v));\
        free_value(&v); \
    } while(0)

static void test_parse_string(void) {
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STRING("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING("\x24", "\"\\u0024\"");
    TEST_STRING("\xC2\xA2", "\"\\u00A2\"");
    TEST_STRING("\xE2\x82\xAC", "\"\\u20AC\"");
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");
}

static void test_parse_invalid_unicode_hex(void) {
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
}

static void test_parse_invalid_unicode_surrogate(void) {
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

static void test_parse_missing_quotation_mark(void) {
    TEST_ERROR(PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(PARSE_MISS_QUOTATION_MARK, "\"abc");
}

static void test_parse_invalid_string_escape(void) {
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

static void test_parse_invalid_string_char(void) {
    TEST_ERROR(PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

static void test_parse_array(void) {
    json_value v;
    size_t i, j;

    init_value(&v);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "[ ]"));
    EXPECT_EQ_INT(TYPE_ARRAY, get_type(&v));
    EXPECT_EQ_SIZE_T(0, get_array_size(&v));
    free_value(&v);

    init_value(&v);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(TYPE_ARRAY, get_type(&v));
    EXPECT_EQ_SIZE_T(5, get_array_size(&v));
    EXPECT_EQ_INT(TYPE_NULL,   get_type(get_array_element(&v, 0)));
    EXPECT_EQ_INT(TYPE_FALSE,  get_type(get_array_element(&v, 1)));
    EXPECT_EQ_INT(TYPE_TRUE,   get_type(get_array_element(&v, 2)));
    EXPECT_EQ_INT(TYPE_NUMBER, get_type(get_array_element(&v, 3)));
    EXPECT_EQ_INT(TYPE_STRING, get_type(get_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, get_number(get_array_element(&v, 3)));
    EXPECT_EQ_STRING("abc", get_string(get_array_element(&v, 4)), get_string_length(get_array_element(&v, 4)));
    free_value(&v);

    init_value(&v);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(TYPE_ARRAY, get_type(&v));
    EXPECT_EQ_SIZE_T(4, get_array_size(&v));
    for (i = 0; i < 4; i++) {
        json_value* a = get_array_element(&v, i);
        EXPECT_EQ_INT(TYPE_ARRAY, get_type(a));
        EXPECT_EQ_SIZE_T(i, get_array_size(a));
        for (j = 0; j < i; j++) {
            json_value* e = get_array_element(a, j);
            EXPECT_EQ_INT(TYPE_NUMBER, get_type(e));
            EXPECT_EQ_DOUBLE((double)j, get_number(e));
        }
    }
    free_value(&v);
}

static void test_parse_miss_key(void) {
    TEST_ERROR(PARSE_MISS_KEY, "{:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{1:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{true:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{false:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{null:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{[]:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{{}:1,");
    TEST_ERROR(PARSE_MISS_KEY, "{\"a\":1,");
}

static void test_parse_miss_colon(void) {
    TEST_ERROR(PARSE_MISS_COLON, "{\"a\"}");
    TEST_ERROR(PARSE_MISS_COLON, "{\"a\",\"b\"}");
}

static void test_parse_miss_comma_or_curly_bracket(void) {
    TEST_ERROR(PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1");
    TEST_ERROR(PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1]");
    TEST_ERROR(PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":1 \"b\"");
    TEST_ERROR(PARSE_MISS_COMMA_OR_CURLY_BRACKET, "{\"a\":{}");
}

static void test_parse_object(void) {
    json_value v;
    size_t i;

    init_value(&v);
    EXPECT_EQ_INT(PARSE_OK, parse(&v, " { } "));
    EXPECT_EQ_INT(TYPE_OBJECT, get_type(&v));
    EXPECT_EQ_SIZE_T(0, get_object_size(&v));
    free_value(&v);

    init_value(&v);
    EXPECT_EQ_INT(PARSE_OK, parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(TYPE_OBJECT, get_type(&v));
    EXPECT_EQ_SIZE_T(7, get_object_size(&v));
    EXPECT_EQ_STRING("n", get_object_key(&v, 0), get_object_key_length(&v, 0));
    EXPECT_EQ_INT(TYPE_NULL, get_type(get_object_value(&v, 0)));
    EXPECT_EQ_STRING("f", get_object_key(&v, 1), get_object_key_length(&v, 1));
    EXPECT_EQ_INT(TYPE_FALSE, get_type(get_object_value(&v, 1)));
    EXPECT_EQ_STRING("t", get_object_key(&v, 2), get_object_key_length(&v, 2));
    EXPECT_EQ_INT(TYPE_TRUE, get_type(get_object_value(&v, 2)));
    EXPECT_EQ_STRING("i", get_object_key(&v, 3), get_object_key_length(&v, 3));
    EXPECT_EQ_INT(TYPE_NUMBER, get_type(get_object_value(&v, 3)));
    EXPECT_EQ_DOUBLE(123.0, get_number(get_object_value(&v, 3)));
    EXPECT_EQ_STRING("s", get_object_key(&v, 4), get_object_key_length(&v, 4));
    EXPECT_EQ_INT(TYPE_STRING, get_type(get_object_value(&v, 4)));
    EXPECT_EQ_STRING("abc", get_string(get_object_value(&v, 4)), get_string_length(get_object_value(&v, 4)));
    EXPECT_EQ_STRING("a", get_object_key(&v, 5), get_object_key_length(&v, 5));
    EXPECT_EQ_INT(TYPE_ARRAY, get_type(get_object_value(&v, 5)));
    EXPECT_EQ_SIZE_T(3, get_array_size(get_object_value(&v, 5)));
    for (i = 0; i < 3; i++) {
        json_value* e = get_array_element(get_object_value(&v, 5), i);
        EXPECT_EQ_INT(TYPE_NUMBER, get_type(e));
        EXPECT_EQ_DOUBLE(i + 1.0, get_number(e));
    }
    EXPECT_EQ_STRING("o", get_object_key(&v, 6), get_object_key_length(&v, 6));
    {
        json_value* o = get_object_value(&v, 6);
        EXPECT_EQ_INT(TYPE_OBJECT, get_type(o));
        for (i = 0; i < 3; ++i) {
            json_value *ov = get_object_value(o, i);
            EXPECT_TRUE(i + '1' == get_object_key(o, i)[0]);
            EXPECT_EQ_SIZE_T(1, get_object_key_length(o, i));
            EXPECT_EQ_INT(TYPE_NUMBER, get_type(ov));
            EXPECT_EQ_DOUBLE(i + 1.0, get_number(ov));
        }
    }
    free_value(&v);
}

#define TEST_ROUNDTRIP(json)\
    do {\
        json_value v;\
        char* json2;\
        size_t length;\
        init_value(&v);\
        EXPECT_EQ_INT(PARSE_OK, parse(&v, json));\
        json2 = stringify(&v, &length);\
        EXPECT_EQ_STRING(json, json2, length);\
        free_value(&v);\
        free(json2);\
    } while(0)

static void test_stringify_number(void) {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1e+20");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");

    TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
    TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

static void test_stringify_string(void) {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

static void test_stringify_array(void) {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

static void test_stringify_object(void) {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

static void test_stringify(void) {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");

    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
}

static void test_parse(void) {
    test_parse_null();
    test_parse_true();
    test_parse_false();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_object();

    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
    test_parse_missing_quotation_mark();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_hex();
    test_parse_invalid_unicode_surrogate();
    test_parse_miss_key();
    test_parse_miss_colon();
    test_parse_miss_comma_or_curly_bracket();
}

#define TEST_EQUAL(json1, json2, equality)\
    do {\
        json_value v1;\
        json_value v2;\
        init_value(&v1);\
        init_value(&v2);\
        EXPECT_EQ_INT(PARSE_OK, parse(&v1, json1));\
        EXPECT_EQ_INT(PARSE_OK, parse(&v2, json2));\
        EXPECT_EQ_INT(equality, is_equal(&v1, &v2));\
        free_value(&v1);\
        free_value(&v2);\
    } while(0)

static void test_equal(void) {
    TEST_EQUAL("true", "true", 1);
    TEST_EQUAL("true", "false", 0);
    TEST_EQUAL("false", "false", 1);
    TEST_EQUAL("null", "null", 1);
    TEST_EQUAL("null", "0", 0);
    TEST_EQUAL("123", "123", 1);
    TEST_EQUAL("123", "456", 0);
    TEST_EQUAL("\"abc\"", "\"abc\"", 1);
    TEST_EQUAL("\"abc\"", "\"abcd\"", 0);
    TEST_EQUAL("[]", "[]", 1);
    TEST_EQUAL("[]", "null", 0);
    TEST_EQUAL("[1,2,3]", "[1,2,3]", 1);
    TEST_EQUAL("[1,2,3]", "[1,2,3,4]", 0);
    TEST_EQUAL("[[]]", "[[]]", 1);
    TEST_EQUAL("{}", "{}", 1);
    TEST_EQUAL("{}", "null", 0);
    TEST_EQUAL("{}", "[]", 0);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2}", 1);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"b\":2,\"a\":1}", 1);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":3}", 0);
    TEST_EQUAL("{\"a\":1,\"b\":2}", "{\"a\":1,\"b\":2,\"c\":3}", 0);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":{}}}}", 1);
    TEST_EQUAL("{\"a\":{\"b\":{\"c\":{}}}}", "{\"a\":{\"b\":{\"c\":[]}}}", 0);
}

static void test_copy(void) {
    json_value v1;
    json_value v2;
    init_value(&v1);
    parse(&v1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    init_value(&v2);
    copy(&v2, &v1);
    EXPECT_TRUE(is_equal(&v2, &v1));
    free_value(&v1);
    free_value(&v2);
}

static void test_move(void) {
    json_value v1;
    json_value v2;
    json_value v3;
    init_value(&v1);
    parse(&v1, "{\"t\":true,\"f\":false,\"n\":null,\"d\":1.5,\"a\":[1,2,3]}");
    init_value(&v2);
    copy(&v2, &v1);
    init_value(&v3);
    move(&v3, &v2);
    EXPECT_EQ_INT(TYPE_NULL, get_type(&v2));
    EXPECT_TRUE(is_equal(&v3, &v1));
    free_value(&v1);
    free_value(&v2);
    free_value(&v3);
}

static void test_swap(void) {
    json_value v1;
    json_value v2;
    init_value(&v1);
    init_value(&v2);
    set_string(&v1, "Hello", 5);
    set_string(&v2, "World!", 6);
    swap(&v1, &v2);
    EXPECT_EQ_STRING("World!", get_string(&v1), get_string_length(&v1));
    EXPECT_EQ_STRING("Hello",  get_string(&v2), get_string_length(&v2));
    free_value(&v1);
    free_value(&v2);
}

static void test_access_null(void) {
    json_value v;
    init_value(&v);
    set_string(&v, "a", 1);
    set_null(&v);
    EXPECT_EQ_INT(TYPE_NULL, get_type(&v));
    free_value(&v);
}

static void test_access_boolean(void) {
    json_value v;
    init_value(&v);
    set_string(&v, "a", 1);
    set_boolean(&v, 1);
    EXPECT_TRUE(get_boolean(&v));
    set_boolean(&v, 0);
    EXPECT_FALSE(get_boolean(&v));
    free_value(&v);
}

static void test_access_number(void) {
    json_value v;
    init_value(&v);
    set_string(&v, "a", 1);
    set_number(&v, 1234.5);
    EXPECT_EQ_DOUBLE(1234.5, get_number(&v));
    free_value(&v);
}

static void test_access_string(void) {
    json_value v;
    init_value(&v);
    set_string(&v, "", 0);
    EXPECT_EQ_STRING("", get_string(&v), get_string_length(&v));
    set_string(&v, "Hello", 5);
    EXPECT_EQ_STRING("Hello", get_string(&v), get_string_length(&v));
    free_value(&v);
}

static void test_access_array(void) {
    json_value a;
    json_value e;
    size_t i;
    size_t j;

    init_value(&a);

    for (j = 0; j <= 5; j += 5) {
        set_array(&a, j);
        EXPECT_EQ_SIZE_T(0, get_array_size(&a));
        EXPECT_EQ_SIZE_T(j, get_array_capacity(&a));
        for (i = 0; i < 10; i++) {
            init_value(&e);
            set_number(&e, i);
            move(pushback_array_element(&a), &e);
            free_value(&e);
        }

        EXPECT_EQ_SIZE_T(10, get_array_size(&a));
        for (i = 0; i < 10; i++) {
            EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
        }
    }

    popback_array_element(&a);
    EXPECT_EQ_SIZE_T(9, get_array_size(&a));
    for (i = 0; i < 9; i++) {
        EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
    }

    erase_array_element(&a, 4, 0);
    EXPECT_EQ_SIZE_T(9, get_array_size(&a));
    for (i = 0; i < 9; i++) {
        EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
    }

    erase_array_element(&a, 8, 1);
    EXPECT_EQ_SIZE_T(8, get_array_size(&a));
    for (i = 0; i < 8; i++) {
        EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
    }

    erase_array_element(&a, 0, 2);
    EXPECT_EQ_SIZE_T(6, get_array_size(&a));
    for (i = 0; i < 6; i++) {
        EXPECT_EQ_DOUBLE((double)i + 2, get_number(get_array_element(&a, i)));
    }

    for (i = 0; i < 2; i++) {
        init_value(&e);
        set_number(&e, i);
        move(insert_array_element(&a, i), &e);
        free_value(&e);
    }

    EXPECT_EQ_SIZE_T(8, get_array_size(&a));
    for (i = 0; i < 8; i++) {
        EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
    }

    EXPECT_TRUE(get_array_capacity(&a) > 8);
    shrink_array(&a);
    EXPECT_EQ_SIZE_T(8, get_array_capacity(&a));
    EXPECT_EQ_SIZE_T(8, get_array_size(&a));
    for (i = 0; i < 8; i++) {
        EXPECT_EQ_DOUBLE((double)i, get_number(get_array_element(&a, i)));
    }

    set_string(&e, "Hello", 5);
    move(pushback_array_element(&a), &e);
    free_value(&e);

    i = get_array_capacity(&a);
    clear_array(&a);
    EXPECT_EQ_SIZE_T(0, get_array_size(&a));
    EXPECT_EQ_SIZE_T(i, get_array_capacity(&a));
    shrink_array(&a);
    EXPECT_EQ_SIZE_T(0, get_array_capacity(&a));

    free_value(&a);
}

static void test_access_object(void) {
    json_value o;
    json_value v;
    json_value *pv;
    size_t i;
    size_t j;
    size_t index;

    init_value(&o);

    for (j = 0; j <=5; j += 5) {
        set_object(&o, j);
        EXPECT_EQ_SIZE_T(0, get_object_size(&o));
        EXPECT_EQ_SIZE_T(j, get_object_capacity(&o));
        for (i = 0; i < 10; i++) {
            char key[2] = "a";
            key[0] += i;
            init_value(&v);
            set_number(&v, i);
            move(set_object_value(&o, key, 1), &v);
            free_value(&v);
        }
        EXPECT_EQ_SIZE_T(10, get_object_size(&o));
        for (i = 0; i < 10; i++) {
            char key[] = "a";
            key[0] += i;
            index = find_object_index(&o, key, 1);
            EXPECT_TRUE(index != KEY_NOT_EXIST);
            pv = get_object_value(&o, index);
            EXPECT_EQ_DOUBLE((double)i, get_number(pv));
        }
    }

    index = find_object_index(&o, "j", 1);
    EXPECT_TRUE(index != KEY_NOT_EXIST);
    remove_object_value(&o, index);
    index = find_object_index(&o, "j", 1);
    EXPECT_TRUE(index == KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(9, get_object_size(&o));

    index = find_object_index(&o, "a", 1);
    EXPECT_TRUE(index != KEY_NOT_EXIST);
    remove_object_value(&o, index);
    index = find_object_index(&o, "a", 1);
    EXPECT_TRUE(index == KEY_NOT_EXIST);
    EXPECT_EQ_SIZE_T(8, get_object_size(&o));

    EXPECT_TRUE(get_object_capacity(&o) > 8);
    shrink_object(&o);
    EXPECT_EQ_SIZE_T(8, get_object_capacity(&o));
    EXPECT_EQ_SIZE_T(8, get_object_size(&o));
    for (i = 0; i < 8; i++) {
        char key[] = "a";
        key[0] += i + 1;
        EXPECT_EQ_DOUBLE((double)i + 1, get_number(get_object_value(&o, find_object_index(&o, key, 1))));
    }

    set_string(&v, "Hello", 5);
    move(set_object_value(&o, "World", 5), &v);
    free_value(&v);

    pv = find_object_value(&o, "World", 5);
    EXPECT_TRUE(pv != NULL);
    EXPECT_EQ_STRING("Hello", get_string(pv), get_string_length(pv));

    i = get_object_capacity(&o);
    clear_object(&o);
    EXPECT_EQ_SIZE_T(0, get_object_size(&o));
    EXPECT_EQ_SIZE_T(i, get_object_capacity(&o));
    shrink_object(&o);
    EXPECT_EQ_SIZE_T(0, get_object_capacity(&o));

    free_value(&o);
}

static void test_access(void) {
    test_access_null();
    test_access_boolean();
    test_access_number();
    test_access_string();
    test_access_array();
    test_access_object();
}

int main() {
    test_parse();
    test_stringify();
    test_equal();
    test_copy();
    test_move();
    test_swap();
    test_access();
    printf("%d/%d (%3.2f%%) passed\n", test_pass, test_count, test_pass * 100.0 / test_count);

    return main_ret;
}