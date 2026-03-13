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
// Math operations (wraps libm)
float   __taipan_sqrt   (float x);
float   __taipan_pow    (float base, float exp);
float   __taipan_abs_f  (float x);
float   __taipan_floor  (float x);
float   __taipan_ceil   (float x);
float   __taipan_sin    (float x);
float   __taipan_cos    (float x);
float   __taipan_tan    (float x);
float   __taipan_log    (float x);
float   __taipan_log2   (float x);
int32_t __taipan_abs_i  (int32_t x);
int32_t __taipan_min_i  (int32_t a, int32_t b);
int32_t __taipan_max_i  (int32_t a, int32_t b);
float   __taipan_min_f  (float a, float b);
float   __taipan_max_f  (float a, float b);

// std.math extras
float   __taipan_clamp_f (float x, float lo, float hi);
int32_t __taipan_clamp_i (int32_t x, int32_t lo, int32_t hi);
// std.io input
char   *__taipan_read_line  (void);
int32_t __taipan_read_int   (void);
float   __taipan_read_float (void);
// std.string extras
char   *__taipan_str_upper    (const char *s);
char   *__taipan_str_lower    (const char *s);
char   *__taipan_str_trim     (const char *s);
int32_t __taipan_str_contains (const char *s, const char *sub);
char   *__taipan_str_replace  (const char *s, const char *from, const char *to);
char   *__taipan_str_split    (const char *s, const char *delim);
int32_t __taipan_str_to_int   (const char *s);
float   __taipan_str_to_float (const char *s);
char   *__taipan_int_to_str   (int32_t n);
char   *__taipan_float_to_str (float f);
// std.rand
int32_t __taipan_rand_seed  (int32_t s);
int32_t __taipan_rand_int   (int32_t lo, int32_t hi);
float   __taipan_rand_float (void);
// std.time
int64_t __taipan_time_now       (void);
int64_t __taipan_time_timestamp (void);
int32_t __taipan_time_sleep     (int32_t ms);
// std.env
char   *__taipan_getenv (const char *name);
// std.mem
char   *__taipan_mem_copy (char *dst, const char *src, int32_t n);
char   *__taipan_mem_zero (char *dst, int32_t n);
// std.process
int32_t __taipan_exec (const char *cmd);

// std.fs
int64_t __taipan_fs_open      (const char *path, const char *mode);
int32_t __taipan_fs_close     (int64_t handle);
char   *__taipan_fs_read      (const char *path);
char   *__taipan_fs_read_line (int64_t handle);
int32_t __taipan_fs_write     (int64_t handle, const char *data);
int32_t __taipan_fs_writeln   (int64_t handle, const char *data);
int32_t __taipan_fs_write_file(const char *path, const char *data);
int32_t __taipan_fs_append    (const char *path, const char *data);
int32_t __taipan_fs_exists    (const char *path);
int32_t __taipan_fs_delete    (const char *path);
int32_t __taipan_fs_mkdir     (const char *path);
int64_t __taipan_fs_size      (const char *path);
