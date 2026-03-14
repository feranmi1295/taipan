#define _GNU_SOURCE
#include "runtime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

// ─────────────────────────────────────────────
//  Panic — abort with message
// ─────────────────────────────────────────────
void __taipan_panic(const char *msg) {
    fprintf(stderr, "\n[taipan panic]: %s\n", msg);
    abort();
}

// ─────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────
void __taipan_init(void) {
    // future: GC init, thread pool, etc.
}

// ─────────────────────────────────────────────
//  Array — layout: [TaipanArrayHeader][data...]
//
//  Public pointer always points to data start.
//  Header is at (data_ptr - sizeof(TaipanArrayHeader))
// ─────────────────────────────────────────────

static TaipanArrayHeader *arr_header(void *data_ptr) {
    return (TaipanArrayHeader *)((char *)data_ptr - sizeof(TaipanArrayHeader));
}

void *__taipan_array_new(int32_t length, int32_t elem_size) {
    if (length < 0 || elem_size <= 0)
        __taipan_panic("array_new: invalid length or elem_size");
    size_t total = sizeof(TaipanArrayHeader) + (size_t)length * (size_t)elem_size;
    TaipanArrayHeader *hdr = calloc(1, total);
    if (!hdr) __taipan_panic("array_new: out of memory");
    hdr->length    = length;
    hdr->elem_size = elem_size;
    return (char *)hdr + sizeof(TaipanArrayHeader);
}

int32_t __taipan_array_len(void *data_ptr) {
    if (!data_ptr) return 0;
    return arr_header(data_ptr)->length;
}

void __taipan_array_free(void *data_ptr) {
    if (!data_ptr) return;
    free(arr_header(data_ptr));
}

// Push returns a potentially reallocated pointer
void *__taipan_array_push(void *data_ptr, void *elem) {
    if (!data_ptr) __taipan_panic("array_push: null array");
    TaipanArrayHeader *hdr = arr_header(data_ptr);
    int32_t old_len  = hdr->length;
    int32_t elem_sz  = hdr->elem_size;
    int32_t new_len  = old_len + 1;
    size_t  new_size = sizeof(TaipanArrayHeader) + (size_t)new_len * (size_t)elem_sz;
    TaipanArrayHeader *new_hdr = realloc(hdr, new_size);
    if (!new_hdr) __taipan_panic("array_push: out of memory");
    new_hdr->length = new_len;
    char *new_data  = (char *)new_hdr + sizeof(TaipanArrayHeader);
    memcpy(new_data + (size_t)old_len * (size_t)elem_sz, elem, (size_t)elem_sz);
    return new_data;
}

void *__taipan_array_get(void *data_ptr, int32_t index) {
    if (!data_ptr) __taipan_panic("array_get: null array");
    TaipanArrayHeader *hdr = arr_header(data_ptr);
    if (index < 0 || index >= hdr->length)
        __taipan_panic("array_get: index out of bounds");
    return (char *)data_ptr + (size_t)index * (size_t)hdr->elem_size;
}

// ─────────────────────────────────────────────
//  String operations
// ─────────────────────────────────────────────
int32_t __taipan_str_len(const char *s) {
    if (!s) return 0;
    return (int32_t)strlen(s);
}

char *__taipan_str_concat(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *out = malloc(la + lb + 1);
    if (!out) __taipan_panic("str_concat: out of memory");
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

int32_t __taipan_str_eq(const char *a, const char *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

char *__taipan_str_slice(const char *s, int32_t start, int32_t end) {
    if (!s) return strdup("");
    int32_t len = (int32_t)strlen(s);
    if (start < 0) start = 0;
    if (end > len) end = len;
    if (start >= end) return strdup("");
    int32_t slice_len = end - start;
    char *out = malloc((size_t)slice_len + 1);
    if (!out) __taipan_panic("str_slice: out of memory");
    memcpy(out, s + start, (size_t)slice_len);
    out[slice_len] = '\0';
    return out;
}

char *__taipan_int_to_str(int32_t n) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", n);
    return strdup(buf);
}

char *__taipan_float_to_str(float f) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", f);
    return strdup(buf);
}

// ─────────────────────────────────────────────
//  I/O
// ─────────────────────────────────────────────
void __taipan_println(const char *s) {
    puts(s ? s : "(null)");
}

void __taipan_print(const char *s) {
    fputs(s ? s : "(null)", stdout);
}

void __taipan_println_i32(int32_t n) {
    printf("%d\n", n);
}

void __taipan_println_f32(float f) {
    printf("%g\n", f);
}

void __taipan_println_bool(int32_t b) {
    puts(b ? "true" : "false");
}

char *__taipan_readline(void) {
    char buf[4096];
    if (!fgets(buf, sizeof(buf), stdin)) return strdup("");
    // strip trailing newline
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
    return strdup(buf);
}

// ─────────────────────────────────────────────
//  Memory
// ─────────────────────────────────────────────
void *__taipan_alloc(int32_t bytes) {
    if (bytes <= 0) __taipan_panic("alloc: size must be > 0");
    void *ptr = malloc((size_t)bytes);
    if (!ptr) __taipan_panic("alloc: out of memory");
    return ptr;
}

void __taipan_free(void *ptr) {
    free(ptr);
}

void *__taipan_realloc(void *ptr, int32_t bytes) {
    if (bytes <= 0) __taipan_panic("realloc: size must be > 0");
    void *new_ptr = realloc(ptr, (size_t)bytes);
    if (!new_ptr) __taipan_panic("realloc: out of memory");
    return new_ptr;
}

void __taipan_memcpy(void *dst, const void *src, int32_t bytes) {
    if (bytes > 0) memcpy(dst, src, (size_t)bytes);
}

void __taipan_memset(void *dst, int32_t val, int32_t bytes) {
    if (bytes > 0) memset(dst, val, (size_t)bytes);
}

// ─────────────────────────────────────────────
//  Math
// ─────────────────────────────────────────────
float   __taipan_sqrt_f32 (float x)          { return sqrtf(x); }
float   __taipan_pow_f32  (float x, float y) { return powf(x, y); }
float   __taipan_abs_f32  (float x)          { return fabsf(x); }
int32_t __taipan_abs_i32  (int32_t x)        { return x < 0 ? -x : x; }
float   __taipan_floor_f32(float x)          { return floorf(x); }
float   __taipan_ceil_f32 (float x)          { return ceilf(x); }
int32_t __taipan_min_i32  (int32_t a, int32_t b) { return a < b ? a : b; }
int32_t __taipan_max_i32  (int32_t a, int32_t b) { return a > b ? a : b; }
float   __taipan_min_f32  (float a, float b) { return a < b ? a : b; }
float   __taipan_max_f32  (float a, float b) { return a > b ? a : b; }
// ─────────────────────────────────────────────
//  Math — wraps libm
// ─────────────────────────────────────────────
#include <math.h>
float   __taipan_sqrt  (float x)            { return sqrtf(x); }
float   __taipan_pow   (float b, float e)   { return powf(b, e); }
float   __taipan_abs_f (float x)            { return fabsf(x); }
float   __taipan_floor (float x)            { return floorf(x); }
float   __taipan_ceil  (float x)            { return ceilf(x); }
float   __taipan_sin   (float x)            { return sinf(x); }
float   __taipan_cos   (float x)            { return cosf(x); }
float   __taipan_tan   (float x)            { return tanf(x); }
float   __taipan_log   (float x)            { return logf(x); }
float   __taipan_log2  (float x)            { return log2f(x); }
int32_t __taipan_abs_i (int32_t x)          { return x < 0 ? -x : x; }
int32_t __taipan_min_i (int32_t a, int32_t b) { return a < b ? a : b; }
int32_t __taipan_max_i (int32_t a, int32_t b) { return a > b ? a : b; }
float   __taipan_min_f (float a, float b)   { return a < b ? a : b; }
float   __taipan_max_f (float a, float b)   { return a > b ? a : b; }

// ─────────────────────────────────────────────
//  std.math extras
// ─────────────────────────────────────────────
float __taipan_clamp_f(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
int32_t __taipan_clamp_i(int32_t x, int32_t lo, int32_t hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

// ─────────────────────────────────────────────
//  std.io — input
// ─────────────────────────────────────────────
char *__taipan_read_line(void) {
    char *buf = malloc(1024);
    if (!buf) return NULL;
    if (!fgets(buf, 1024, stdin)) { free(buf); return strdup(""); }
    size_t l = strlen(buf);
    if (l > 0 && buf[l-1] == '\n') buf[l-1] = '\0';
    return buf;
}
int32_t __taipan_read_int(void) {
    int32_t v = 0; scanf("%d", &v); return v;
}
float __taipan_read_float(void) {
    float v = 0; scanf("%f", &v); return v;
}

// ─────────────────────────────────────────────
//  std.string extras
// ─────────────────────────────────────────────
char *__taipan_str_upper(const char *s) {
    if (!s) return strdup("");
    char *o = strdup(s);
    for (char *p = o; *p; p++) *p = (char)toupper((unsigned char)*p);
    return o;
}
char *__taipan_str_lower(const char *s) {
    if (!s) return strdup("");
    char *o = strdup(s);
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
char *__taipan_str_trim(const char *s) {
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    const char *e = s + strlen(s);
    while (e > s && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n')) e--;
    size_t l = (size_t)(e - s);
    char *o = malloc(l + 1); memcpy(o, s, l); o[l] = '\0';
    return o;
}
int32_t __taipan_str_contains(const char *s, const char *sub) {
    if (!s || !sub) return 0;
    return strstr(s, sub) ? 1 : 0;
}
char *__taipan_str_replace(const char *s, const char *from, const char *to) {
    if (!s || !from || !to) return strdup(s ? s : "");
    size_t slen=strlen(s), flen=strlen(from), tlen=strlen(to);
    if (flen == 0) return strdup(s);
    size_t count = 0;
    const char *p = s;
    while ((p = strstr(p, from))) { count++; p += flen; }
    char *out = malloc(slen + count*(tlen > flen ? tlen-flen : 0) + count + 1);
    char *w = out; p = s;
    const char *found;
    while ((found = strstr(p, from))) {
        size_t pre = (size_t)(found - p);
        memcpy(w, p, pre); w += pre;
        memcpy(w, to, tlen); w += tlen;
        p = found + flen;
    }
    strcpy(w, p);
    return out;
}
char *__taipan_str_split(const char *s, const char *delim) {
    // Returns first token only for now (full split needs array support)
    if (!s || !delim) return strdup("");
    char *copy = strdup(s);
    char *tok = strtok(copy, delim);
    char *result = strdup(tok ? tok : "");
    free(copy);
    return result;
}
int32_t __taipan_str_to_int(const char *s)   { return s ? (int32_t)atoi(s) : 0; }
float   __taipan_str_to_float(const char *s) { return s ? (float)atof(s) : 0.0f; }

// ─────────────────────────────────────────────
//  std.rand
// ─────────────────────────────────────────────
#include <stdlib.h>
#include <time.h>
int32_t __taipan_rand_seed(int32_t s) { srand((unsigned)s); return 0; }
int32_t __taipan_rand_int(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    return lo + (int32_t)(rand() % (hi - lo + 1));
}
float __taipan_rand_float(void) { return (float)rand() / (float)RAND_MAX; }

// ─────────────────────────────────────────────
//  std.time
// ─────────────────────────────────────────────
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
int64_t __taipan_time_now(void) { return (int64_t)time(NULL); }
int64_t __taipan_time_timestamp(void) { return (int64_t)time(NULL); }
int32_t __taipan_time_sleep(int32_t ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
    return 0;
}

// ─────────────────────────────────────────────
//  std.env
// ─────────────────────────────────────────────
char *__taipan_getenv(const char *name) {
    char *v = getenv(name);
    return v ? v : (char*)"";
}

// ─────────────────────────────────────────────
//  std.mem
// ─────────────────────────────────────────────
char *__taipan_mem_copy(char *dst, const char *src, int32_t n) {
    return (char*)memcpy(dst, src, (size_t)n);
}
char *__taipan_mem_zero(char *dst, int32_t n) {
    return (char*)memset(dst, 0, (size_t)n);
}

// ─────────────────────────────────────────────
//  std.process
// ─────────────────────────────────────────────
int32_t __taipan_exec(const char *cmd) { return (int32_t)system(cmd); }

// ─────────────────────────────────────────────
//  std.fs — file system
// ─────────────────────────────────────────────
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

// open a file, returns FILE* cast to i64 handle (0 = error)
int64_t __taipan_fs_open(const char *path, const char *mode) {
    FILE *f = fopen(path, mode);
    return f ? (int64_t)(uintptr_t)f : 0;
}

// close a file handle
int32_t __taipan_fs_close(int64_t handle) {
    if (!handle) return -1;
    return fclose((FILE*)(uintptr_t)handle);
}

// read entire file into a malloc'd string (caller owns it)
char *__taipan_fs_read(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return strdup("");
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return strdup(""); }
    size_t n = fread(buf, 1, (size_t)sz, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

// read one line from handle
char *__taipan_fs_read_line(int64_t handle) {
    if (!handle) return strdup("");
    char buf[4096];
    if (!fgets(buf, sizeof(buf), (FILE*)(uintptr_t)handle)) return strdup("");
    size_t l = strlen(buf);
    if (l > 0 && buf[l-1] == '\n') buf[l-1] = '\0';
    return strdup(buf);
}

// write string to file handle
int32_t __taipan_fs_write(int64_t handle, const char *data) {
    if (!handle || !data) return -1;
    return fputs(data, (FILE*)(uintptr_t)handle);
}

// write string + newline to file handle
int32_t __taipan_fs_writeln(int64_t handle, const char *data) {
    if (!handle || !data) return -1;
    int r = fputs(data, (FILE*)(uintptr_t)handle);
    fputc('\n', (FILE*)(uintptr_t)handle);
    return r;
}

// write entire string to a file path (overwrites)
int32_t __taipan_fs_write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    int r = fputs(data, f);
    fclose(f);
    return r;
}

// append string to a file path
int32_t __taipan_fs_append(const char *path, const char *data) {
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    int r = fputs(data, f);
    fclose(f);
    return r;
}

// check if file exists
int32_t __taipan_fs_exists(const char *path) {
    return access(path, F_OK) == 0 ? 1 : 0;
}

// delete a file
int32_t __taipan_fs_delete(const char *path) {
    return remove(path) == 0 ? 1 : 0;
}

// make a directory
int32_t __taipan_fs_mkdir(const char *path) {
    return mkdir(path, 0755) == 0 ? 1 : 0;
}

// get file size in bytes
int64_t __taipan_fs_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

// ─────────────────────────────────────────────
//  std.net — TCP sockets
// ─────────────────────────────────────────────
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

// Create a TCP socket, returns fd or -1
int32_t __taipan_net_socket(void) {
    return (int32_t)socket(AF_INET, SOCK_STREAM, 0);
}

// Connect to host:port, returns 0 on success or -1
int32_t __taipan_net_connect(int32_t fd, const char *host, int32_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);
    struct hostent *he = gethostbyname(host);
    if (!he) return -1;
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    return (int32_t)connect(fd, (struct sockaddr*)&addr, sizeof(addr));
}

// Bind + listen on port, returns server fd or -1
int32_t __taipan_net_listen(int32_t port, int32_t backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, backlog) < 0) { close(fd); return -1; }
    return (int32_t)fd;
}

// Accept a connection, returns client fd or -1
int32_t __taipan_net_accept(int32_t server_fd) {
    struct sockaddr_in client;
    socklen_t len = sizeof(client);
    return (int32_t)accept(server_fd, (struct sockaddr*)&client, &len);
}

// Send data, returns bytes sent or -1
int32_t __taipan_net_send(int32_t fd, const char *data) {
    if (!data) return -1;
    return (int32_t)send(fd, data, strlen(data), 0);
}

// Receive data into malloc'd buffer, returns string or ""
char *__taipan_net_recv(int32_t fd, int32_t buf_size) {
    char *buf = malloc((size_t)buf_size + 1);
    if (!buf) return strdup("");
    int32_t n = (int32_t)recv(fd, buf, (size_t)buf_size, 0);
    if (n <= 0) { free(buf); return strdup(""); }
    buf[n] = '\0';
    return buf;
}

// Close a socket
int32_t __taipan_net_close(int32_t fd) {
    return (int32_t)close(fd);
}

// Get client IP address from accepted fd
char *__taipan_net_peer_addr(int32_t fd) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr*)&addr, &len) < 0) return strdup("unknown");
    return strdup(inet_ntoa(addr.sin_addr));
}

// Set socket non-blocking
int32_t __taipan_net_set_nonblocking(int32_t fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return (int32_t)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
