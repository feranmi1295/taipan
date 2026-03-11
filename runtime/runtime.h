#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdint.h>
#include <stdio.h>

// ─────────────────────────────────────────────
//  Taipan Array — heap-allocated with length header
//
//  Memory layout:
//    [ i32 length ][ i32 elem_size ][ ... data ... ]
//
//  Pointer passed around in Taipan programs points
//  to the START OF DATA, not the header.
//  Header sits at ptr - 8.
// ─────────────────────────────────────────────
typedef struct {
    int32_t length;
    int32_t elem_size;
} TaipanArrayHeader;

// Array operations
void    *__taipan_array_new  (int32_t length, int32_t elem_size);
int32_t  __taipan_array_len  (void *data_ptr);
void     __taipan_array_free (void *data_ptr);
void    *__taipan_array_push (void *data_ptr, void *elem);
void    *__taipan_array_get  (void *data_ptr, int32_t index);

// String operations
int32_t  __taipan_str_len    (const char *s);
char    *__taipan_str_concat (const char *a, const char *b);
int32_t  __taipan_str_eq     (const char *a, const char *b);
char    *__taipan_str_slice  (const char *s, int32_t start, int32_t end);
char    *__taipan_str_concat (const char *a, const char *b);
int32_t  __taipan_str_eq     (const char *a, const char *b);
char    *__taipan_str_slice  (const char *s, int32_t start, int32_t end);
char    *__taipan_int_to_str (int32_t n);
char    *__taipan_float_to_str(float f);

// I/O
void     __taipan_println    (const char *s);
void     __taipan_print      (const char *s);
void     __taipan_println_i32(int32_t n);
void     __taipan_println_f32(float f);
void     __taipan_println_bool(int32_t b);
char    *__taipan_readline   (void);

// Memory
void    *__taipan_alloc      (int32_t bytes);
void     __taipan_free       (void *ptr);
void    *__taipan_realloc    (void *ptr, int32_t bytes);
void     __taipan_memcpy     (void *dst, const void *src, int32_t bytes);
void     __taipan_memset     (void *dst, int32_t val, int32_t bytes);

// Math
float    __taipan_sqrt_f32   (float x);
float    __taipan_pow_f32    (float x, float y);
float    __taipan_abs_f32    (float x);
int32_t  __taipan_abs_i32    (int32_t x);
float    __taipan_floor_f32  (float x);
float    __taipan_ceil_f32   (float x);
int32_t  __taipan_min_i32    (int32_t a, int32_t b);
int32_t  __taipan_max_i32    (int32_t a, int32_t b);
float    __taipan_min_f32    (float a, float b);
float    __taipan_max_f32    (float a, float b);

// Runtime
void     __taipan_init       (void);
void     __taipan_panic      (const char *msg);

#endif /* RUNTIME_H */