#define _GNU_SOURCE
#include "../runtime/runtime.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define PASS(name) printf("  [PASS] %s\n", name)
#define FAIL(name) printf("  [FAIL] %s\n", name)
#define CHECK(cond, name) do { if(cond) PASS(name); else FAIL(name); } while(0)

int main(void) {
    __taipan_init();

    printf("══════════════════════════════════════════\n");
    printf("  Taipan Runtime — Tests\n");
    printf("══════════════════════════════════════════\n");

    // ── Array ─────────────────────────────────
    printf("\n── Arrays ─────────────────────────────\n");

    int32_t *arr = __taipan_array_new(5, sizeof(int32_t));
    CHECK(__taipan_array_len(arr) == 5, "array_new length=5");

    // fill and read
    for (int i = 0; i < 5; i++) arr[i] = i * 10;
    CHECK(arr[0]==0 && arr[2]==20 && arr[4]==40, "array element access");

    // push
    int32_t val = 99;
    arr = __taipan_array_push(arr, &val);
    CHECK(__taipan_array_len(arr) == 6, "array_push length=6");
    CHECK(arr[5] == 99, "array_push value correct");

    // bounds via get
    int32_t *elem = __taipan_array_get(arr, 3);
    CHECK(*elem == 30, "array_get index 3");

    // null array len
    CHECK(__taipan_array_len(NULL) == 0, "array_len null=0");

    __taipan_array_free(arr);
    PASS("array_free");

    // ── Strings ───────────────────────────────
    printf("\n── Strings ────────────────────────────\n");

    CHECK(__taipan_str_len("hello") == 5, "str_len 5");
    CHECK(__taipan_str_len("") == 0,      "str_len 0");
    CHECK(__taipan_str_len(NULL) == 0,    "str_len null=0");

    char *cat = __taipan_str_concat("Hello, ", "Taipan!");
    CHECK(strcmp(cat, "Hello, Taipan!") == 0, "str_concat");
    free(cat);

    CHECK(__taipan_str_eq("abc", "abc") == 1, "str_eq true");
    CHECK(__taipan_str_eq("abc", "xyz") == 0, "str_eq false");
    CHECK(__taipan_str_eq(NULL, NULL)   == 1, "str_eq null==null");

    char *sl = __taipan_str_slice("Hello, Taipan!", 7, 13);
    CHECK(strcmp(sl, "Taipan") == 0, "str_slice");
    free(sl);

    char *ns = __taipan_int_to_str(42);
    CHECK(strcmp(ns, "42") == 0, "int_to_str");
    free(ns);

    char *fs = __taipan_float_to_str(3.14f);
    CHECK(fs[0] == '3', "float_to_str starts with 3");
    free(fs);

    // ── Memory ────────────────────────────────
    printf("\n── Memory ─────────────────────────────\n");

    void *buf = __taipan_alloc(64);
    CHECK(buf != NULL, "alloc 64 bytes");
    __taipan_memset(buf, 0, 64);
    CHECK(((char*)buf)[0] == 0, "memset zero");
    ((char*)buf)[0] = 'T';
    void *buf2 = __taipan_alloc(64);
    __taipan_memcpy(buf2, buf, 64);
    CHECK(((char*)buf2)[0] == 'T', "memcpy");
    buf = __taipan_realloc(buf, 128);
    CHECK(buf != NULL, "realloc 128");
    __taipan_free(buf);
    __taipan_free(buf2);
    PASS("alloc/free/realloc/memcpy/memset");

    // ── Math ──────────────────────────────────
    printf("\n── Math ───────────────────────────────\n");

    CHECK(__taipan_abs_i32(-5) == 5,   "abs_i32(-5)=5");
    CHECK(__taipan_abs_i32(5)  == 5,   "abs_i32(5)=5");
    CHECK(__taipan_min_i32(3,7) == 3,  "min_i32(3,7)=3");
    CHECK(__taipan_max_i32(3,7) == 7,  "max_i32(3,7)=7");

    float sq = __taipan_sqrt_f32(9.0f);
    CHECK(sq > 2.99f && sq < 3.01f,    "sqrt_f32(9)≈3");

    float pw = __taipan_pow_f32(2.0f, 10.0f);
    CHECK(pw > 1023.0f && pw < 1025.0f,"pow_f32(2,10)≈1024");

    CHECK(__taipan_floor_f32(3.7f) == 3.0f, "floor_f32(3.7)=3");
    CHECK(__taipan_ceil_f32(3.2f)  == 4.0f, "ceil_f32(3.2)=4");

    // ── I/O (just smoke test, no stdin) ───────
    printf("\n── I/O ────────────────────────────────\n");
    printf("  [smoke] println: ");
    __taipan_println("Hello from runtime!");
    printf("  [smoke] println_i32: ");
    __taipan_println_i32(12345);
    printf("  [smoke] println_f32: ");
    __taipan_println_f32(3.14f);
    printf("  [smoke] println_bool true/false: ");
    __taipan_println_bool(1);

    printf("\n══════════════════════════════════════════\n");
    printf("  Runtime tests complete\n");
    printf("══════════════════════════════════════════\n");
    return 0;
}