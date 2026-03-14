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

// std.net
int32_t  __taipan_net_socket          (void);
int32_t  __taipan_net_connect         (int32_t fd, const char *host, int32_t port);
int32_t  __taipan_net_listen          (int32_t port, int32_t backlog);
int32_t  __taipan_net_accept          (int32_t server_fd);
int32_t  __taipan_net_send            (int32_t fd, const char *data);
char    *__taipan_net_recv            (int32_t fd, int32_t buf_size);
int32_t  __taipan_net_close           (int32_t fd);
char    *__taipan_net_peer_addr       (int32_t fd);
int32_t  __taipan_net_set_nonblocking (int32_t fd);

// std.crypto
char   *__taipan_sha256              (const char *input);
char   *__taipan_md5                 (const char *input);
char   *__taipan_crypto_random_bytes (int32_t n);
char   *__taipan_crypto_token        (int32_t bytes);
char   *__taipan_base64_encode       (const char *input);
int32_t __taipan_hash                (const char *s);

// std.collections (opaque i8* handles in Taipan)
void   *__taipan_vec_new    (void);
int32_t __taipan_vec_push   (void *v, void *item);
void   *__taipan_vec_get    (void *v, int32_t idx);
int32_t __taipan_vec_set    (void *v, int32_t idx, void *item);
int32_t __taipan_vec_len    (void *v);
int32_t __taipan_vec_pop    (void *v);
int32_t __taipan_vec_clear  (void *v);
void    __taipan_vec_free   (void *v);
void   *__taipan_hm_new     (void);
int32_t __taipan_hm_set     (void *m, const char *k, const char *v2);
char   *__taipan_hm_get     (void *m, const char *k);
int32_t __taipan_hm_has     (void *m, const char *k);
int32_t __taipan_hm_delete  (void *m, const char *k);
int32_t __taipan_hm_size    (void *m);
void    __taipan_hm_free    (void *m);
void   *__taipan_set_new    (void);
int32_t __taipan_set_add    (void *s, const char *val);
int32_t __taipan_set_has    (void *s, const char *val);
int32_t __taipan_set_remove (void *s, const char *val);
int32_t __taipan_set_size   (void *s);
void    __taipan_set_free   (void *s);

// std.async
void    __taipan_async_init  (void);
int32_t __taipan_async_spawn (void (*fn)(void));
void    __taipan_async_yield (void);
void    __taipan_async_sleep (int32_t ms);
void    __taipan_async_join  (int32_t id);
int32_t __taipan_async_self  (void);
void    __taipan_async_run   (void);
void   *__taipan_chan_new     (void);
int32_t __taipan_chan_send    (void *c, const char *msg);
char   *__taipan_chan_recv    (void *c);
int32_t __taipan_chan_len     (void *c);
void    __taipan_chan_free    (void *c);

// std.async
void    __taipan_async_init  (void);
int32_t __taipan_async_spawn (void (*fn)(void));
void    __taipan_async_yield (void);
void    __taipan_async_sleep (int32_t ms);
void    __taipan_async_join  (int32_t id);
int32_t __taipan_async_self  (void);
void    __taipan_async_run   (void);
void   *__taipan_chan_new     (void);
int32_t __taipan_chan_send    (void *c, const char *msg);
char   *__taipan_chan_recv    (void *c);
int32_t __taipan_chan_len     (void *c);
void    __taipan_chan_free    (void *c);
