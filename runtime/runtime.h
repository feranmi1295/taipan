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

// std.json
void   *__taipan_json_parse      (const char *input);
char   *__taipan_json_stringify  (void *n);
void    __taipan_json_free       (void *n);
char   *__taipan_json_get_str    (void *n, const char *key);
int32_t __taipan_json_get_int    (void *n, const char *key);
float   __taipan_json_get_float  (void *n, const char *key);
int32_t __taipan_json_array_len  (void *n);
void   *__taipan_json_array_get  (void *n, int32_t idx);
char   *__taipan_json_to_str     (void *n);
void   *__taipan_json_object_new (void);
void   *__taipan_json_array_new2 (void);
void    __taipan_json_set_str    (void *n, const char *key, const char *val);
void    __taipan_json_set_int    (void *n, const char *key, int32_t val);
void    __taipan_json_array_push2(void *n, void *item);

// std.thread
int32_t __taipan_thread_spawn (void (*fn)(void));
void    __taipan_thread_join  (int32_t id);
void    __taipan_thread_sleep (int32_t ms);
int32_t __taipan_thread_done  (int32_t id);
void   *__taipan_mutex_new    (void);
void    __taipan_mutex_lock   (void *m);
void    __taipan_mutex_unlock (void *m);
void    __taipan_mutex_free   (void *m);

// std.data
void   *__taipan_data_new          (int32_t n_samples, int32_t n_features, int32_t n_labels);
void    __taipan_data_free         (void *d);
void    __taipan_data_set_x        (void *d, int32_t row, int32_t col, float val);
float   __taipan_data_get_x        (void *d, int32_t row, int32_t col);
void    __taipan_data_set_y        (void *d, int32_t row, int32_t col, float val);
float   __taipan_data_get_y        (void *d, int32_t row, int32_t col);
int32_t __taipan_data_n_samples    (void *d);
int32_t __taipan_data_n_features   (void *d);
int32_t __taipan_data_n_labels     (void *d);
void    __taipan_data_shuffle      (void *d);
void   *__taipan_data_batch_x      (void *d, int32_t offset, int32_t batch_size);
void   *__taipan_data_batch_y      (void *d, int32_t offset, int32_t batch_size);
void    __taipan_data_normalize    (void *d);
void    __taipan_data_standardize  (void *d);
void   *__taipan_data_split_train  (void *d, float ratio);
void   *__taipan_data_split_test   (void *d, float ratio);
void   *__taipan_data_load_csv     (const char *path, int32_t n_features, int32_t n_labels);
int32_t __taipan_data_save_csv     (void *d, const char *path);
void   *__taipan_data_get_x_tensor (void *d, int32_t row);
void   *__taipan_data_get_y_tensor (void *d, int32_t row);

// std.transformer
void   *__taipan_transformer_config     (int32_t vocab, int32_t seq, int32_t d_model, int32_t heads, int32_t layers, int32_t d_ff);
void   *__taipan_embed_new              (int32_t vocab, int32_t seq, int32_t d_model);
void   *__taipan_embed_tensor           (void *e, void *ids);
void    __taipan_embed_free             (void *e);
void   *__taipan_mha_new                (int32_t d_model, int32_t n_heads, int32_t causal);
void   *__taipan_mha_forward            (void *m, void *x);
void    __taipan_mha_free               (void *m);
void   *__taipan_ffn_new                (int32_t d_model, int32_t d_ff);
void   *__taipan_ffn_forward            (void *f, void *x);
void    __taipan_ffn_free               (void *f);
void   *__taipan_block_new              (int32_t d_model, int32_t n_heads, int32_t d_ff, int32_t causal);
void   *__taipan_block_forward          (void *b, void *x);
void    __taipan_block_free             (void *b);
void   *__taipan_transformer_new        (void *cfg);
void   *__taipan_transformer_forward    (void *t, void *ids);
void   *__taipan_transformer_next_probs (void *t, void *ids);
void   *__taipan_transformer_generate   (void *t, void *ids, int32_t n_new);
float   __taipan_transformer_loss       (void *logits, void *targets);
void    __taipan_transformer_free       (void *t);
int32_t __taipan_transformer_param_count(void *t);

// std.autograd
void    __taipan_ag_init      (void);
void    __taipan_ag_reset     (void);
int32_t __taipan_ag_leaf      (float val, int32_t requires_grad);
float   __taipan_ag_val       (int32_t id);
float   __taipan_ag_grad      (int32_t id);
void    __taipan_ag_zero_grad (void);
int32_t __taipan_ag_add       (int32_t a, int32_t b);
int32_t __taipan_ag_sub       (int32_t a, int32_t b);
int32_t __taipan_ag_mul       (int32_t a, int32_t b);
int32_t __taipan_ag_div       (int32_t a, int32_t b);
int32_t __taipan_ag_pow       (int32_t a, float exp);
int32_t __taipan_ag_neg       (int32_t a);
int32_t __taipan_ag_exp       (int32_t a);
int32_t __taipan_ag_log       (int32_t a);
int32_t __taipan_ag_sqrt_op   (int32_t a);
int32_t __taipan_ag_sin_op    (int32_t a);
int32_t __taipan_ag_cos_op    (int32_t a);
int32_t __taipan_ag_tanh_op   (int32_t a);
int32_t __taipan_ag_relu_op   (int32_t a);
int32_t __taipan_ag_sigmoid_op(int32_t a);
int32_t __taipan_ag_mse       (int32_t pred, int32_t target);
void    __taipan_ag_backward  (int32_t loss_id);
void    __taipan_ag_update    (int32_t id, float lr);
void    __taipan_ag_print     (int32_t id);
void   *__taipan_ag_tensor    (void *t);
void   *__taipan_ag_tadd      (void *a, void *b);
void   *__taipan_ag_tmul      (void *a, void *b);
int32_t __taipan_ag_tsum      (void *ids);
void   *__taipan_ag_tvals     (void *ids);
void   *__taipan_ag_tgrads    (void *ids);

// std.nn extras
void   *__taipan_nn_dropout          (void *x, float rate, int32_t training);
void   *__taipan_nn_batchnorm_new    (int32_t features);
void   *__taipan_nn_batchnorm_forward(void *bn, void *x, int32_t training);
void    __taipan_nn_batchnorm_free   (void *bn);
void   *__taipan_nn_conv2d_new       (int32_t in_ch, int32_t out_ch, int32_t kernel, int32_t stride, int32_t padding);
void   *__taipan_nn_conv2d_forward   (void *c, void *x);
void    __taipan_nn_conv2d_free      (void *c);
void   *__taipan_nn_embed_new        (int32_t vocab, int32_t d_model);
void   *__taipan_nn_embed_forward    (void *e, void *ids);
void    __taipan_nn_embed_free       (void *e);
// std.optim extras
void   *__taipan_nn_rmsprop_new      (void *layer);
void    __taipan_nn_rmsprop_step     (void *layer, void *rms, float lr);
void    __taipan_nn_rmsprop_free     (void *rms);
float   __taipan_optim_step_decay    (float lr, int32_t epoch, int32_t step_size, float gamma);
float   __taipan_optim_cosine_decay  (float lr, int32_t epoch, int32_t max_epochs);
float   __taipan_optim_warmup        (float lr, int32_t epoch, int32_t warmup_epochs);
// std.linalg
void   *__taipan_linalg_inv          (void *A);
float   __taipan_linalg_det          (void *A);
float   __taipan_linalg_norm         (void *A);
void   *__taipan_linalg_outer        (void *a, void *b);
float   __taipan_linalg_trace        (void *A);
float   __taipan_linalg_cosine       (void *a, void *b);
void   *__taipan_linalg_solve        (void *A, void *b);
// image io
void   *__taipan_data_load_ppm       (const char *path);
int32_t __taipan_data_save_ppm       (void *t, const char *path, int32_t W, int32_t H);

// std.brain
void   *__taipan_brain_new          (void);
void    __taipan_brain_free         (void *b);
void    __taipan_brain_learn        (void *b, const char *text);
char   *__taipan_brain_think        (void *b, const char *query);
void    __taipan_brain_forget       (void *b, float decay_rate);
int32_t __taipan_brain_size         (void *b);
int32_t __taipan_brain_interactions (void *b);
int32_t __taipan_brain_edges        (void *b);
char   *__taipan_brain_most_important(void *b);
char   *__taipan_brain_search       (void *b, const char *keyword);
int32_t __taipan_brain_save         (void *b, const char *path);
void   *__taipan_brain_load         (const char *path);
float   __taipan_brain_similarity   (void *b, const char *a, const char *b_text);

// std.neuro — Spiking Neural Networks
void   *__taipan_neuro_new           (float dt);
void    __taipan_neuro_free          (void *net);
int32_t __taipan_neuro_add_neuron    (void *net, float threshold, float leak, int32_t layer);
int32_t __taipan_neuro_connect       (void *net, int32_t pre, int32_t post, float weight, float delay);
void    __taipan_neuro_inject        (void *net, int32_t id, float current);
void    __taipan_neuro_step          (void *net);
void    __taipan_neuro_run           (void *net, int32_t steps);
float   __taipan_neuro_potential     (void *net, int32_t id);
int32_t __taipan_neuro_fired         (void *net, int32_t id);
int32_t __taipan_neuro_spike_count   (void *net, int32_t id);
float   __taipan_neuro_time          (void *net);
int32_t __taipan_neuro_total_spikes  (void *net);
int32_t __taipan_neuro_n_neurons     (void *net);
float   __taipan_neuro_synapse_weight(void *net, int32_t sid);
void    __taipan_neuro_reset         (void *net);
void    __taipan_neuro_rate_encode   (void *net, int32_t id, float rate, int32_t window);
int32_t __taipan_neuro_add_layer     (void *net, int32_t n, float threshold, float leak, int32_t layer_id);
void    __taipan_neuro_connect_layers(void *net, int32_t fs, int32_t fn, int32_t ts, int32_t tn, float w, float noise);
void    __taipan_neuro_print_raster  (void *net, int32_t width);
