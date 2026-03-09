#include "json.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void test_jFind() {
    char buf[64];
    const char *obj = "{\"foo\":\"bar\",\"baz\":42}";

    // string value
    assert(jStr(jFind(obj, "foo"), buf, sizeof(buf)) == 3);
    assert(strcmp(buf, "bar") == 0);

    // numeric value
    assert(jUint(jFind(obj, "baz")) == 42);

    // missing key
    assert(jFind(obj, "missing") == NULL);

    // nested: finds key in inner object
    const char *nested = "{\"a\":{\"b\":\"c\"}}";
    assert(jStr(jFind(nested, "b"), buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "c") == 0);

    // no false match on prefix
    const char *prefixed = "{\"foobar\":1,\"foo\":2}";
    assert(jUint(jFind(prefixed, "foo")) == 2);
}

void test_jStr() {
    char buf[64];

    assert(jStr("\"hello\"", buf, sizeof(buf)) == 5);
    assert(strcmp(buf, "hello") == 0);

    // strips outer quotes, returns length
    assert(jStr("\"0xdeadbeef\"", buf, sizeof(buf)) == 10);
    assert(strcmp(buf, "0xdeadbeef") == 0);

    // not a string
    assert(jStr("123", buf, sizeof(buf)) == -1);
    assert(jStr(NULL, buf, sizeof(buf)) == -1);

    // truncates to fit buflen (stores buflen-1 chars + NUL)
    assert(jStr("\"abcde\"", buf, 4) == 3);
    assert(strcmp(buf, "abc") == 0);
}

void test_jUint() {
    // decimal
    assert(jUint("42") == 42);
    assert(jUint("0") == 0);

    // hex string
    assert(jUint("\"0xff\"") == 255);
    assert(jUint("\"0x0\"") == 0);
    assert(jUint("\"0x1a2b\"") == 0x1a2b);

    // unquoted hex
    assert(jUint("0x10") == 16);

    // NULL
    assert(jUint(NULL) == 0);
}

void test_jArrayGet() {
    char buf[8];
    const char *arr = "[\"a\",\"b\",\"c\"]";

    assert(jStr(jArrayGet(arr, 0), buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "a") == 0);
    assert(jStr(jArrayGet(arr, 1), buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "b") == 0);
    assert(jStr(jArrayGet(arr, 2), buf, sizeof(buf)) == 1);
    assert(strcmp(buf, "c") == 0);

    // out of range
    assert(jArrayGet(arr, 3) == NULL);

    // not an array
    assert(jArrayGet("{}", 0) == NULL);
    assert(jArrayGet(NULL, 0) == NULL);
}

void test_jArrayNext() {
    const char *arr = "[\"a\",\"b\",\"c\"]";
    char buf[8];

    const char *e0 = jArrayGet(arr, 0);
    const char *e1 = jArrayNext(e0);
    const char *e2 = jArrayNext(e1);
    const char *e3 = jArrayNext(e2);

    assert(e0 != NULL);
    assert(e1 != NULL);
    assert(e2 != NULL);
    assert(e3 == NULL);

    jStr(e0, buf, sizeof(buf)); assert(strcmp(buf, "a") == 0);
    jStr(e1, buf, sizeof(buf)); assert(strcmp(buf, "b") == 0);
    jStr(e2, buf, sizeof(buf)); assert(strcmp(buf, "c") == 0);

    // single-element array
    const char *single = "[42]";
    assert(jArrayGet(single, 0) != NULL);
    assert(jArrayNext(jArrayGet(single, 0)) == NULL);

    // object elements
    const char *objarr = "[{\"id\":1},{\"id\":2}]";
    const char *o0 = jArrayGet(objarr, 0);
    const char *o1 = jArrayNext(o0);
    const char *o2 = jArrayNext(o1);
    assert(o0 != NULL);
    assert(o1 != NULL);
    assert(o2 == NULL);
    assert(jUint(jFind(o0, "id")) == 1);
    assert(jUint(jFind(o1, "id")) == 2);
}

void test_jStrDup() {
    char *s = jStrDup("\"hello\"");
    assert(strcmp(s, "hello") == 0);
    free(s);

    // NULL or non-string → "0x"
    s = jStrDup(NULL);
    assert(strcmp(s, "0x") == 0);
    free(s);

    s = jStrDup("123");
    assert(strcmp(s, "0x") == 0);
    free(s);
}

void test_jValDup() {
    // string
    char *v = jValDup("\"hello\" rest");
    assert(strcmp(v, "\"hello\"") == 0);
    free(v);

    // number
    v = jValDup("42,next");
    assert(strcmp(v, "42") == 0);
    free(v);

    // object
    v = jValDup("{\"a\":1},rest");
    assert(strcmp(v, "{\"a\":1}") == 0);
    free(v);

    // array
    v = jValDup("[1,2,3]end");
    assert(strcmp(v, "[1,2,3]") == 0);
    free(v);

    // NULL
    v = jValDup(NULL);
    assert(v == NULL);
}

void test_resultById() {
    const char *batch =
        "[{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":\"0xbb\"},"
         "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":\"0xaa\"},"
         "{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":\"0xcc\"}]";

    char *r1 = resultById(batch, 1);
    assert(r1 != NULL);
    assert(strcmp(r1, "0xaa") == 0);
    free(r1);

    char *r2 = resultById(batch, 2);
    assert(r2 != NULL);
    assert(strcmp(r2, "0xbb") == 0);
    free(r2);

    char *r3 = resultById(batch, 3);
    assert(r3 != NULL);
    assert(strcmp(r3, "0xcc") == 0);
    free(r3);

    // missing id
    assert(resultById(batch, 99) == NULL);

    // element with error instead of result
    const char *withErr =
        "[{\"jsonrpc\":\"2.0\",\"id\":1,\"error\":{\"code\":-32000}}]";
    assert(resultById(withErr, 1) == NULL);
}

int main() {
    test_jFind();
    test_jStr();
    test_jUint();
    test_jArrayGet();
    test_jArrayNext();
    test_jStrDup();
    test_jValDup();
    test_resultById();
    return 0;
}
