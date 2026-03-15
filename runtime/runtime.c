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

// ─────────────────────────────────────────────
//  std.crypto — hashing & cryptography
// ─────────────────────────────────────────────

// ── SHA-256 ───────────────────────────────────
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
#define RR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define S0(x) (RR(x,2)^RR(x,13)^RR(x,22))
#define S1(x) (RR(x,6)^RR(x,11)^RR(x,25))
#define s0(x) (RR(x,7)^RR(x,18)^((x)>>3))
#define s1(x) (RR(x,17)^RR(x,19)^((x)>>10))

static void sha256_block(uint32_t h[8], const uint8_t *block) {
    uint32_t w[64], a,b,c,d,e,f,g,hh,t1,t2;
    for (int i=0;i<16;i++)
        w[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
             ((uint32_t)block[i*4+2]<<8)|(uint32_t)block[i*4+3];
    for (int i=16;i<64;i++) w[i]=s1(w[i-2])+w[i-7]+s0(w[i-15])+w[i-16];
    a=h[0];b=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];hh=h[7];
    for (int i=0;i<64;i++){
        t1=hh+S1(e)+CH(e,f,g)+K256[i]+w[i];
        t2=S0(a)+MAJ(a,b,c);
        hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;
    h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}

char *__taipan_sha256(const char *input) {
    size_t len = strlen(input);
    uint32_t h[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    size_t bit_len = len * 8;
    size_t pad_len = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = calloc(1, pad_len);
    memcpy(msg, input, len);
    msg[len] = 0x80;
    for (int i=0;i<8;i++) msg[pad_len-1-i]=(uint8_t)(bit_len>>(i*8));
    for (size_t i=0;i<pad_len;i+=64) sha256_block(h, msg+i);
    free(msg);
    char *out = malloc(65);
    snprintf(out,65,
        "%08x%08x%08x%08x%08x%08x%08x%08x",
        h[0],h[1],h[2],h[3],h[4],h[5],h[6],h[7]);
    return out;
}

// ── MD5 ───────────────────────────────────────
static const uint32_t MD5_S[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5, 9,14,20,5, 9,14,20,5, 9,14,20,5, 9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};
static const uint32_t MD5_K[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
    0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
    0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
    0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
    0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
    0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
    0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
    0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
    0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
#define MD5_RL(x,n) (((x)<<(n))|((x)>>(32-(n))))

char *__taipan_md5(const char *input) {
    size_t len = strlen(input);
    uint32_t a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
    size_t pad = ((len+8)/64+1)*64;
    uint8_t *msg = calloc(1,pad);
    memcpy(msg,input,len);
    msg[len]=0x80;
    uint64_t bits=(uint64_t)len*8;
    memcpy(msg+pad-8,&bits,8);
    for (size_t off=0;off<pad;off+=64){
        uint32_t *M=(uint32_t*)(msg+off);
        uint32_t A=a0,B=b0,C=c0,D=d0,F,g2;
        for(int i=0;i<64;i++){
            if(i<16){F=(B&C)|(~B&D);g2=i;}
            else if(i<32){F=(D&B)|(~D&C);g2=(5*i+1)%16;}
            else if(i<48){F=B^C^D;g2=(3*i+5)%16;}
            else{F=C^(B|(~D));g2=(7*i)%16;}
            F+=A+MD5_K[i]+M[g2];
            A=D;D=C;C=B;B+=MD5_RL(F,MD5_S[i]);
        }
        a0+=A;b0+=B;c0+=C;d0+=D;
    }
    free(msg);
    char *out=malloc(33);
    // MD5 is little-endian
    snprintf(out,33,"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        (uint8_t)a0,(uint8_t)(a0>>8),(uint8_t)(a0>>16),(uint8_t)(a0>>24),
        (uint8_t)b0,(uint8_t)(b0>>8),(uint8_t)(b0>>16),(uint8_t)(b0>>24),
        (uint8_t)c0,(uint8_t)(c0>>8),(uint8_t)(c0>>16),(uint8_t)(c0>>24),
        (uint8_t)d0,(uint8_t)(d0>>8),(uint8_t)(d0>>16),(uint8_t)(d0>>24));
    return out;
}

// ── Random bytes ──────────────────────────────
char *__taipan_crypto_random_bytes(int32_t n) {
    char *buf = malloc((size_t)n+1);
    FILE *f = fopen("/dev/urandom","rb");
    if (f) { fread(buf,(size_t)n,1,f); fclose(f); }
    else   { for(int i=0;i<n;i++) buf[i]=(char)(rand()&0xff); }
    buf[n]='\0';
    return buf;
}

// ── Random hex token ─────────────────────────
char *__taipan_crypto_token(int32_t bytes) {
    char *raw = __taipan_crypto_random_bytes(bytes);
    char *hex = malloc((size_t)bytes*2+1);
    for(int i=0;i<bytes;i++) sprintf(hex+i*2,"%02x",(unsigned char)raw[i]);
    hex[bytes*2]='\0';
    free(raw);
    return hex;
}

// ── Base64 encode ─────────────────────────────
static const char B64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
char *__taipan_base64_encode(const char *input) {
    size_t len=strlen(input);
    size_t out_len=((len+2)/3)*4+1;
    char *out=malloc(out_len);
    size_t i=0,j=0;
    while(i<len){
        uint32_t o=(uint8_t)input[i++]<<16;
        if(i<len) o|=(uint8_t)input[i++]<<8;
        if(i<len) o|=(uint8_t)input[i++];
        out[j++]=B64[(o>>18)&63];
        out[j++]=B64[(o>>12)&63];
        out[j++]=(i-1<len||len%3!=1)?B64[(o>>6)&63]:'=';
        out[j++]=(i<len+1||len%3==0)?B64[o&63]:'=';
    }
    out[j]='\0';
    return out;
}

// ── Simple hash (djb2) ────────────────────────
int32_t __taipan_hash(const char *s) {
    uint32_t h=5381;
    while(*s) h=((h<<5)+h)+(uint8_t)*s++;
    return (int32_t)h;
}

// ─────────────────────────────────────────────
//  std.collections — Vector, HashMap, Set
// ─────────────────────────────────────────────

// ── Vector (dynamic array of i8* / generic via casting) ──
typedef struct {
    void   **data;
    int32_t  len;
    int32_t  cap;
} TaipanVector;

void *__taipan_vec_new(void) {
    TaipanVector *v = malloc(sizeof(TaipanVector));
    v->cap  = 8;
    v->len  = 0;
    v->data = malloc(sizeof(void*) * (size_t)v->cap);
    return v;
}
int32_t __taipan_vec_push(void *vp, void *item) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v) return -1;
    if (v->len >= v->cap) {
        v->cap *= 2;
        v->data = realloc(v->data, sizeof(void*) * (size_t)v->cap);
    }
    v->data[v->len++] = item;
    return v->len;
}
void *__taipan_vec_get(void *vp, int32_t idx) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v || idx < 0 || idx >= v->len) return NULL;
    return v->data[idx];
}
int32_t __taipan_vec_set(void *vp, int32_t idx, void *item) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v || idx < 0 || idx >= v->len) return -1;
    v->data[idx] = item;
    return 0;
}
int32_t __taipan_vec_len(void *vp) {
    TaipanVector *v = (TaipanVector*)vp;
    return v ? v->len : 0;
}
int32_t __taipan_vec_pop(void *vp) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v || v->len == 0) return -1;
    v->len--;
    return v->len;
}
int32_t __taipan_vec_clear(void *vp) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v) return -1;
    v->len = 0;
    return 0;
}
void __taipan_vec_free(void *vp) {
    TaipanVector *v = (TaipanVector*)vp;
    if (!v) return;
    free(v->data);
    free(v);
}

// ── HashMap (string keys, string values) ──────
#define HM_BUCKETS 64
typedef struct HMEntry {
    char           *key;
    char           *val;
    struct HMEntry *next;
} HMEntry;
typedef struct {
    HMEntry *buckets[HM_BUCKETS];
    int32_t  size;
} TaipanHashMap;

static uint32_t hm_hash(const char *k) {
    uint32_t h = 5381;
    while (*k) h = ((h<<5)+h)+(uint8_t)*k++;
    return h % HM_BUCKETS;
}
void *__taipan_hm_new(void) {
    TaipanHashMap *m = calloc(1, sizeof(TaipanHashMap));
    return m;
}
int32_t __taipan_hm_set(void *mp,  const char *key, const char *val) {
    TaipanHashMap *m = (TaipanHashMap*)mp;
    if (!m||!key) return -1;
    uint32_t b = hm_hash(key);
    HMEntry *e = m->buckets[b];
    while (e) {
        if (!strcmp(e->key, key)) { free(e->val); e->val=strdup(val); return 0; }
        e = e->next;
    }
    HMEntry *ne = malloc(sizeof(HMEntry));
    ne->key  = strdup(key);
    ne->val  = strdup(val);
    ne->next = m->buckets[b];
    m->buckets[b] = ne;
    m->size++;
    return 0;
}
char *__taipan_hm_get(void *mp, const char *key) {
    TaipanHashMap *m = (TaipanHashMap*)mp;
    if (!m||!key) return strdup("");
    uint32_t b = hm_hash(key);
    HMEntry *e = m->buckets[b];
    while (e) {
        if (!strcmp(e->key, key)) return e->val;
        e = e->next;
    }
    return (char*)"";
}
int32_t __taipan_hm_has(void *mp, const char *key) {
    TaipanHashMap *m = (TaipanHashMap*)mp;
    if (!m||!key) return 0;
    uint32_t b = hm_hash(key);
    HMEntry *e = m->buckets[b];
    while (e) { if (!strcmp(e->key,key)) return 1; e=e->next; }
    return 0;
}
int32_t __taipan_hm_delete(void *mp, const char *key) {
    TaipanHashMap *m = (TaipanHashMap*)mp;
    if (!m||!key) return -1;
    uint32_t b = hm_hash(key);
    HMEntry **ep = &m->buckets[b];
    while (*ep) {
        if (!strcmp((*ep)->key, key)) {
            HMEntry *del = *ep;
            *ep = del->next;
            free(del->key); free(del->val); free(del);
            m->size--;
            return 0;
        }
        ep = &(*ep)->next;
    }
    return -1;
}
int32_t __taipan_hm_size(void *mp) {
    TaipanHashMap *m = (TaipanHashMap*)mp; return m ? m->size : 0; }
void __taipan_hm_free(void *mp) {
    TaipanHashMap *m = (TaipanHashMap*)mp;
    if (!m) return;
    for (int i=0;i<HM_BUCKETS;i++) {
        HMEntry *e=m->buckets[i];
        while(e){HMEntry*nx=e->next;free(e->key);free(e->val);free(e);e=nx;}
    }
    free(m);
}

// ── Set (unique string values) ────────────────
typedef struct SetEntry { char *val; struct SetEntry *next; } SetEntry;
typedef struct { SetEntry *buckets[HM_BUCKETS]; int32_t size; } TaipanSet;

void *__taipan_set_new(void) { return calloc(1,sizeof(TaipanSet)); }
int32_t __taipan_set_add(void *sp, const char *val) {
    TaipanSet *s = (TaipanSet*)sp;
    if (!s||!val) return -1;
    uint32_t b=hm_hash(val);
    SetEntry *e=s->buckets[b];
    while(e){if(!strcmp(e->val,val))return 0;e=e->next;}
    SetEntry *ne=malloc(sizeof(SetEntry));
    ne->val=strdup(val); ne->next=s->buckets[b]; s->buckets[b]=ne; s->size++;
    return 1;
}
int32_t __taipan_set_has(void *sp, const char *val) {
    TaipanSet *s = (TaipanSet*)sp;
    if (!s||!val) return 0;
    uint32_t b=hm_hash(val);
    SetEntry *e=s->buckets[b];
    while(e){if(!strcmp(e->val,val))return 1;e=e->next;}
    return 0;
}
int32_t __taipan_set_remove(void *sp, const char *val) {
    TaipanSet *s = (TaipanSet*)sp;
    if (!s||!val) return -1;
    uint32_t b=hm_hash(val);
    SetEntry **ep=&s->buckets[b];
    while(*ep){
        if(!strcmp((*ep)->val,val)){
            SetEntry *d=*ep;*ep=d->next;free(d->val);free(d);s->size--;return 0;
        }
        ep=&(*ep)->next;
    }
    return -1;
}
int32_t __taipan_set_size(void *sp) {
    TaipanSet *s = (TaipanSet*)sp; return s?s->size:0; }
void __taipan_set_free(void *sp) {
    TaipanSet *s = (TaipanSet*)sp;
    if(!s)return;
    for(int i=0;i<HM_BUCKETS;i++){
        SetEntry *e=s->buckets[i];
        while(e){SetEntry*nx=e->next;free(e->val);free(e);e=nx;}
    }
    free(s);
}

// ─────────────────────────────────────────────
//  std.tensor — N-dimensional tensor engine
//  CPU backend (CUDA hooks reserved)
// ─────────────────────────────────────────────
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    float   *data;      // flat data buffer
    int32_t *shape;     // dimension sizes
    int32_t *strides;   // strides per dim
    int32_t  ndim;      // number of dimensions
    int32_t  size;      // total elements
    int32_t  owned;     // 1 = we own data, 0 = view
} TaipanTensor;

// ── Allocate ──────────────────────────────────
TaipanTensor *__taipan_tensor_new(int32_t *shape, int32_t ndim) {
    TaipanTensor *t = malloc(sizeof(TaipanTensor));
    t->ndim    = ndim;
    t->shape   = malloc(sizeof(int32_t) * (size_t)ndim);
    t->strides = malloc(sizeof(int32_t) * (size_t)ndim);
    t->owned   = 1;
    int32_t size = 1;
    for (int i = ndim-1; i >= 0; i--) {
        t->shape[i]   = shape[i];
        t->strides[i] = size;
        size *= shape[i];
    }
    t->size = size;
    t->data = calloc((size_t)size, sizeof(float));
    return t;
}

// Convenience: 1D tensor
TaipanTensor *__taipan_tensor_1d(int32_t n) {
    int32_t s[1] = {n};
    return __taipan_tensor_new(s, 1);
}

// Convenience: 2D tensor (matrix)
TaipanTensor *__taipan_tensor_2d(int32_t rows, int32_t cols) {
    int32_t s[2] = {rows, cols};
    return __taipan_tensor_new(s, 2);
}

void __taipan_tensor_free(TaipanTensor *t) {
    if (!t) return;
    if (t->owned) free(t->data);
    free(t->shape);
    free(t->strides);
    free(t);
}

// ── Access ────────────────────────────────────
float __taipan_tensor_get(TaipanTensor *t, int32_t idx) {
    if (!t || idx < 0 || idx >= t->size) return 0.0f;
    return t->data[idx];
}
void __taipan_tensor_set(TaipanTensor *t, int32_t idx, float val) {
    if (!t || idx < 0 || idx >= t->size) return;
    t->data[idx] = val;
}
// 2D access
float __taipan_tensor_get2d(TaipanTensor *t, int32_t r, int32_t c) {
    if (!t || t->ndim < 2) return 0.0f;
    return t->data[r * t->strides[0] + c * t->strides[1]];
}
void __taipan_tensor_set2d(TaipanTensor *t, int32_t r, int32_t c, float val) {
    if (!t || t->ndim < 2) return;
    t->data[r * t->strides[0] + c * t->strides[1]] = val;
}
int32_t __taipan_tensor_size(TaipanTensor *t) { return t ? t->size : 0; }
int32_t __taipan_tensor_rows(TaipanTensor *t) { return (t && t->ndim >= 1) ? t->shape[0] : 0; }
int32_t __taipan_tensor_cols(TaipanTensor *t) { return (t && t->ndim >= 2) ? t->shape[1] : 0; }

// ── Fill ops ──────────────────────────────────
void __taipan_tensor_fill(TaipanTensor *t, float val) {
    if (!t) return;
    for (int32_t i = 0; i < t->size; i++) t->data[i] = val;
}
void __taipan_tensor_zeros(TaipanTensor *t) { __taipan_tensor_fill(t, 0.0f); }
void __taipan_tensor_ones(TaipanTensor *t)  { __taipan_tensor_fill(t, 1.0f); }

// Xavier uniform init
void __taipan_tensor_xavier(TaipanTensor *t) {
    if (!t) return;
    float limit = sqrtf(6.0f / (float)t->size);
    for (int32_t i = 0; i < t->size; i++)
        t->data[i] = ((float)rand()/(float)RAND_MAX) * 2.0f * limit - limit;
}
// Random normal (Box-Muller)
void __taipan_tensor_randn(TaipanTensor *t, float mean, float std) {
    if (!t) return;
    for (int32_t i = 0; i < t->size-1; i+=2) {
        float u1 = (float)rand()/(float)RAND_MAX + 1e-10f;
        float u2 = (float)rand()/(float)RAND_MAX;
        float z0 = sqrtf(-2.0f*logf(u1))*cosf(2.0f*3.14159265f*u2);
        float z1 = sqrtf(-2.0f*logf(u1))*sinf(2.0f*3.14159265f*u2);
        t->data[i]   = mean + std * z0;
        t->data[i+1] = mean + std * z1;
    }
}

// ── Element-wise ops ──────────────────────────
TaipanTensor *__taipan_tensor_add(TaipanTensor *a, TaipanTensor *b) {
    if (!a || !b || a->size != b->size) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++) out->data[i] = a->data[i] + b->data[i];
    return out;
}
TaipanTensor *__taipan_tensor_sub(TaipanTensor *a, TaipanTensor *b) {
    if (!a || !b || a->size != b->size) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++) out->data[i] = a->data[i] - b->data[i];
    return out;
}
TaipanTensor *__taipan_tensor_mul(TaipanTensor *a, TaipanTensor *b) {
    if (!a || !b || a->size != b->size) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++) out->data[i] = a->data[i] * b->data[i];
    return out;
}
TaipanTensor *__taipan_tensor_scale(TaipanTensor *a, float s) {
    if (!a) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++) out->data[i] = a->data[i] * s;
    return out;
}
TaipanTensor *__taipan_tensor_add_scalar(TaipanTensor *a, float s) {
    if (!a) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++) out->data[i] = a->data[i] + s;
    return out;
}

// ── Reduction ops ─────────────────────────────
float __taipan_tensor_sum(TaipanTensor *t) {
    if (!t) return 0.0f;
    float s = 0.0f;
    for (int32_t i = 0; i < t->size; i++) s += t->data[i];
    return s;
}
float __taipan_tensor_mean(TaipanTensor *t) {
    if (!t || t->size == 0) return 0.0f;
    return __taipan_tensor_sum(t) / (float)t->size;
}
float __taipan_tensor_max(TaipanTensor *t) {
    if (!t || t->size == 0) return 0.0f;
    float m = t->data[0];
    for (int32_t i = 1; i < t->size; i++) if (t->data[i] > m) m = t->data[i];
    return m;
}
float __taipan_tensor_min(TaipanTensor *t) {
    if (!t || t->size == 0) return 0.0f;
    float m = t->data[0];
    for (int32_t i = 1; i < t->size; i++) if (t->data[i] < m) m = t->data[i];
    return m;
}

// ── Matrix multiply ───────────────────────────
// (M x K) @ (K x N) = (M x N)
TaipanTensor *__taipan_tensor_matmul(TaipanTensor *a, TaipanTensor *b) {
    if (!a || !b || a->ndim != 2 || b->ndim != 2) return NULL;
    int32_t M = a->shape[0], K = a->shape[1], N = b->shape[1];
    if (K != b->shape[0]) return NULL;
    int32_t s[2] = {M, N};
    TaipanTensor *out = __taipan_tensor_new(s, 2);
    for (int32_t i = 0; i < M; i++)
        for (int32_t j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int32_t k = 0; k < K; k++)
                sum += a->data[i*K+k] * b->data[k*N+j];
            out->data[i*N+j] = sum;
        }
    return out;
}

// ── Transpose ─────────────────────────────────
TaipanTensor *__taipan_tensor_transpose(TaipanTensor *a) {
    if (!a || a->ndim != 2) return NULL;
    int32_t s[2] = {a->shape[1], a->shape[0]};
    TaipanTensor *out = __taipan_tensor_new(s, 2);
    for (int32_t i = 0; i < a->shape[0]; i++)
        for (int32_t j = 0; j < a->shape[1]; j++)
            out->data[j*a->shape[0]+i] = a->data[i*a->shape[1]+j];
    return out;
}

// ── Activation functions ──────────────────────
TaipanTensor *__taipan_tensor_relu(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        out->data[i] = t->data[i] > 0.0f ? t->data[i] : 0.0f;
    return out;
}
TaipanTensor *__taipan_tensor_sigmoid(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        out->data[i] = 1.0f / (1.0f + expf(-t->data[i]));
    return out;
}
TaipanTensor *__taipan_tensor_tanh_act(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        out->data[i] = tanhf(t->data[i]);
    return out;
}
TaipanTensor *__taipan_tensor_softmax(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    float mx = __taipan_tensor_max(t);
    float sum = 0.0f;
    for (int32_t i = 0; i < t->size; i++) { out->data[i]=expf(t->data[i]-mx); sum+=out->data[i]; }
    for (int32_t i = 0; i < t->size; i++) out->data[i] /= sum;
    return out;
}
TaipanTensor *__taipan_tensor_gelu(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++) {
        float x = t->data[i];
        out->data[i] = 0.5f*x*(1.0f+tanhf(0.7978845608f*(x+0.044715f*x*x*x)));
    }
    return out;
}

// ── Loss functions ────────────────────────────
float __taipan_tensor_mse(TaipanTensor *pred, TaipanTensor *target) {
    if (!pred || !target || pred->size != target->size) return 0.0f;
    float loss = 0.0f;
    for (int32_t i = 0; i < pred->size; i++) {
        float d = pred->data[i] - target->data[i];
        loss += d * d;
    }
    return loss / (float)pred->size;
}
float __taipan_tensor_cross_entropy(TaipanTensor *pred, TaipanTensor *target) {
    if (!pred || !target || pred->size != target->size) return 0.0f;
    float loss = 0.0f;
    for (int32_t i = 0; i < pred->size; i++)
        loss -= target->data[i] * logf(pred->data[i] + 1e-10f);
    return loss;
}

// ── Gradient ops (for backprop) ───────────────
TaipanTensor *__taipan_tensor_relu_grad(TaipanTensor *t, TaipanTensor *grad) {
    if (!t || !grad) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        out->data[i] = t->data[i] > 0.0f ? grad->data[i] : 0.0f;
    return out;
}
TaipanTensor *__taipan_tensor_sigmoid_grad(TaipanTensor *sigmoid_out, TaipanTensor *grad) {
    if (!sigmoid_out || !grad) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(sigmoid_out->size);
    for (int32_t i = 0; i < sigmoid_out->size; i++) {
        float s = sigmoid_out->data[i];
        out->data[i] = grad->data[i] * s * (1.0f - s);
    }
    return out;
}

// ── Normalization ─────────────────────────────
TaipanTensor *__taipan_tensor_layer_norm(TaipanTensor *t, float eps) {
    if (!t) return NULL;
    float mean = __taipan_tensor_mean(t);
    float var  = 0.0f;
    for (int32_t i = 0; i < t->size; i++) { float d=t->data[i]-mean; var+=d*d; }
    var /= (float)t->size;
    float std_inv = 1.0f / sqrtf(var + eps);
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        out->data[i] = (t->data[i] - mean) * std_inv;
    return out;
}

// ── Dot product ───────────────────────────────
float __taipan_tensor_dot(TaipanTensor *a, TaipanTensor *b) {
    if (!a || !b || a->size != b->size) return 0.0f;
    float s = 0.0f;
    for (int32_t i = 0; i < a->size; i++) s += a->data[i] * b->data[i];
    return s;
}

// ── Copy ──────────────────────────────────────
TaipanTensor *__taipan_tensor_copy(TaipanTensor *t) {
    if (!t) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(t->size);
    memcpy(out->data, t->data, (size_t)t->size * sizeof(float));
    memcpy(out->shape,   t->shape,   (size_t)t->ndim * sizeof(int32_t));
    memcpy(out->strides, t->strides, (size_t)t->ndim * sizeof(int32_t));
    out->ndim = t->ndim;
    return out;
}

// ── Print ─────────────────────────────────────
void __taipan_tensor_print(TaipanTensor *t) {
    if (!t) { printf("tensor(NULL)\n"); return; }
    printf("tensor(shape=[");
    for (int i=0;i<t->ndim;i++) { if(i) printf(","); printf("%d",t->shape[i]); }
    printf("], data=[");
    int show = t->size > 8 ? 8 : t->size;
    for (int i=0;i<show;i++) { if(i) printf(", "); printf("%.4f",t->data[i]); }
    if (t->size > 8) printf(", ...");
    printf("])\n");
}

// ── Save / Load (binary format) ───────────────
int32_t __taipan_tensor_save(TaipanTensor *t, const char *path) {
    if (!t) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(&t->ndim, sizeof(int32_t), 1, f);
    fwrite(t->shape, sizeof(int32_t), (size_t)t->ndim, f);
    fwrite(t->data,  sizeof(float),   (size_t)t->size, f);
    fclose(f);
    return 0;
}
TaipanTensor *__taipan_tensor_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    int32_t ndim;
    fread(&ndim, sizeof(int32_t), 1, f);
    int32_t *shape = malloc(sizeof(int32_t) * (size_t)ndim);
    fread(shape, sizeof(int32_t), (size_t)ndim, f);
    TaipanTensor *t = __taipan_tensor_new(shape, ndim);
    free(shape);
    fread(t->data, sizeof(float), (size_t)t->size, f);
    fclose(f);
    return t;
}

// ─────────────────────────────────────────────
//  std.nn — Neural Network layers
// ─────────────────────────────────────────────

// ── Linear Layer ─────────────────────────────
// y = x @ W^T + b
typedef struct {
    TaipanTensor *W;      // weights [out x in]
    TaipanTensor *b;      // bias    [out]
    TaipanTensor *dW;     // weight gradients
    TaipanTensor *db;     // bias gradients
    TaipanTensor *input;  // cached input for backprop
    int32_t       in_features;
    int32_t       out_features;
} TaipanLinear;

void *__taipan_nn_linear_new(int32_t in_features, int32_t out_features) {
    TaipanLinear *l = malloc(sizeof(TaipanLinear));
    l->in_features  = in_features;
    l->out_features = out_features;
    l->W     = __taipan_tensor_2d(out_features, in_features);
    l->b     = __taipan_tensor_1d(out_features);
    l->dW    = __taipan_tensor_2d(out_features, in_features);
    l->db    = __taipan_tensor_1d(out_features);
    l->input = NULL;
    // Xavier init
    float limit = sqrtf(6.0f / (float)(in_features + out_features));
    for (int32_t i = 0; i < l->W->size; i++)
        l->W->data[i] = ((float)rand()/(float)RAND_MAX)*2.0f*limit - limit;
    __taipan_tensor_zeros(l->b);
    return (void*)l;
}

// Forward: input [in] -> output [out]
void *__taipan_nn_linear_forward(void *lp, void *xp) {
    TaipanLinear *l = (TaipanLinear*)lp;
    TaipanTensor *x = (TaipanTensor*)xp;
    // cache input
    if (l->input) __taipan_tensor_free(l->input);
    l->input = __taipan_tensor_copy(x);
    // y = W @ x + b
    TaipanTensor *out = __taipan_tensor_1d(l->out_features);
    for (int32_t i = 0; i < l->out_features; i++) {
        float sum = l->b->data[i];
        for (int32_t j = 0; j < l->in_features; j++)
            sum += l->W->data[i * l->in_features + j] * x->data[j];
        out->data[i] = sum;
    }
    return (void*)out;
}

// Backward: grad_out [out] -> grad_in [in], updates dW, db
void *__taipan_nn_linear_backward(void *lp, void *gp) {
    TaipanLinear *l  = (TaipanLinear*)lp;
    TaipanTensor *go = (TaipanTensor*)gp;
    // db = grad_out
    for (int32_t i = 0; i < l->out_features; i++)
        l->db->data[i] += go->data[i];
    // dW = grad_out outer input
    for (int32_t i = 0; i < l->out_features; i++)
        for (int32_t j = 0; j < l->in_features; j++)
            l->dW->data[i*l->in_features+j] += go->data[i] * l->input->data[j];
    // grad_in = W^T @ grad_out
    TaipanTensor *gi = __taipan_tensor_1d(l->in_features);
    for (int32_t j = 0; j < l->in_features; j++) {
        float sum = 0.0f;
        for (int32_t i = 0; i < l->out_features; i++)
            sum += l->W->data[i*l->in_features+j] * go->data[i];
        gi->data[j] = sum;
    }
    return (void*)gi;
}

void __taipan_nn_linear_zero_grad(void *lp) {
    TaipanLinear *l = (TaipanLinear*)lp;
    __taipan_tensor_zeros(l->dW);
    __taipan_tensor_zeros(l->db);
}

void __taipan_nn_linear_free(void *lp) {
    TaipanLinear *l = (TaipanLinear*)lp;
    __taipan_tensor_free(l->W);
    __taipan_tensor_free(l->b);
    __taipan_tensor_free(l->dW);
    __taipan_tensor_free(l->db);
    if (l->input) __taipan_tensor_free(l->input);
    free(l);
}

// Get weights/bias for inspection
void *__taipan_nn_linear_weights(void *lp) { return ((TaipanLinear*)lp)->W; }
void *__taipan_nn_linear_bias(void *lp)    { return ((TaipanLinear*)lp)->b; }
void *__taipan_nn_linear_dw(void *lp)      { return ((TaipanLinear*)lp)->dW; }
void *__taipan_nn_linear_db(void *lp)      { return ((TaipanLinear*)lp)->db; }

// ── SGD Optimizer ─────────────────────────────
void __taipan_nn_sgd_step(void *lp, float lr) {
    TaipanLinear *l = (TaipanLinear*)lp;
    for (int32_t i = 0; i < l->W->size; i++)
        l->W->data[i] -= lr * l->dW->data[i];
    for (int32_t i = 0; i < l->b->size; i++)
        l->b->data[i] -= lr * l->db->data[i];
}

// ── Adam Optimizer ────────────────────────────
typedef struct {
    TaipanTensor *mW, *vW;  // moment estimates for W
    TaipanTensor *mb, *vb;  // moment estimates for b
    int32_t       step;
} TaipanAdam;

void *__taipan_nn_adam_new(void *lp) {
    TaipanLinear *l = (TaipanLinear*)lp;
    TaipanAdam *a = calloc(1, sizeof(TaipanAdam));
    a->mW   = __taipan_tensor_2d(l->out_features, l->in_features);
    a->vW   = __taipan_tensor_2d(l->out_features, l->in_features);
    a->mb   = __taipan_tensor_1d(l->out_features);
    a->vb   = __taipan_tensor_1d(l->out_features);
    a->step = 0;
    return (void*)a;
}

void __taipan_nn_adam_step(void *lp, void *ap, float lr) {
    TaipanLinear *l = (TaipanLinear*)lp;
    TaipanAdam   *a = (TaipanAdam*)ap;
    float beta1=0.9f, beta2=0.999f, eps=1e-8f;
    a->step++;
    float bc1 = 1.0f - powf(beta1, (float)a->step);
    float bc2 = 1.0f - powf(beta2, (float)a->step);
    // Update W
    for (int32_t i = 0; i < l->W->size; i++) {
        float g = l->dW->data[i];
        a->mW->data[i] = beta1*a->mW->data[i] + (1-beta1)*g;
        a->vW->data[i] = beta2*a->vW->data[i] + (1-beta2)*g*g;
        float mh = a->mW->data[i]/bc1;
        float vh = a->vW->data[i]/bc2;
        l->W->data[i] -= lr * mh / (sqrtf(vh) + eps);
    }
    // Update b
    for (int32_t i = 0; i < l->b->size; i++) {
        float g = l->db->data[i];
        a->mb->data[i] = beta1*a->mb->data[i] + (1-beta1)*g;
        a->vb->data[i] = beta2*a->vb->data[i] + (1-beta2)*g*g;
        float mh = a->mb->data[i]/bc1;
        float vh = a->vb->data[i]/bc2;
        l->b->data[i] -= lr * mh / (sqrtf(vh) + eps);
    }
}

void __taipan_nn_adam_free(void *ap) {
    TaipanAdam *a = (TaipanAdam*)ap;
    __taipan_tensor_free(a->mW);
    __taipan_tensor_free(a->vW);
    __taipan_tensor_free(a->mb);
    __taipan_tensor_free(a->vb);
    free(a);
}

// ── MSE backward ─────────────────────────────
void *__taipan_nn_mse_backward(void *pred, void *target) {
    TaipanTensor *p = (TaipanTensor*)pred;
    TaipanTensor *t = (TaipanTensor*)target;
    TaipanTensor *g = __taipan_tensor_1d(p->size);
    float scale = 2.0f / (float)p->size;
    for (int32_t i = 0; i < p->size; i++)
        g->data[i] = scale * (p->data[i] - t->data[i]);
    return (void*)g;
}

// ─────────────────────────────────────────────
//  std.async — Fiber-based coroutine scheduler
//  Uses POSIX ucontext for stack switching
// ─────────────────────────────────────────────
#include <ucontext.h>
#include <sys/time.h>

#define TAIPAN_MAX_FIBERS  256
#define TAIPAN_STACK_SIZE  (64 * 1024)  // 64KB per fiber

typedef enum {
    FIBER_READY,
    FIBER_RUNNING,
    FIBER_WAITING,
    FIBER_DONE
} FiberState;

typedef struct {
    ucontext_t  ctx;
    char       *stack;
    FiberState  state;
    int32_t     id;
    int64_t     wake_time;   // for async_sleep
    void      (*fn)(void);   // fiber function
} TaipanFiber;

static TaipanFiber  fibers[TAIPAN_MAX_FIBERS];
static int32_t      fiber_count   = 0;
static int32_t      current_fiber = -1;
static ucontext_t   scheduler_ctx;

static int64_t now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void fiber_entry(void) {
    TaipanFiber *f = &fibers[current_fiber];
    f->fn();
    f->state = FIBER_DONE;
    swapcontext(&f->ctx, &scheduler_ctx);
}

// ── Public API ────────────────────────────────

// Initialize async runtime
void __taipan_async_init(void) {
    memset(fibers, 0, sizeof(fibers));
    fiber_count   = 0;
    current_fiber = -1;
}

// Spawn a new fiber — takes a function pointer as i8*
int32_t __taipan_async_spawn(void (*fn)(void)) {
    if (fiber_count >= TAIPAN_MAX_FIBERS) return -1;
    int32_t id = fiber_count++;
    TaipanFiber *f = &fibers[id];
    f->id        = id;
    f->state     = FIBER_READY;
    f->fn        = fn;
    f->wake_time = 0;
    f->stack     = malloc(TAIPAN_STACK_SIZE);
    getcontext(&f->ctx);
    f->ctx.uc_stack.ss_sp   = f->stack;
    f->ctx.uc_stack.ss_size = TAIPAN_STACK_SIZE;
    f->ctx.uc_link          = &scheduler_ctx;
    makecontext(&f->ctx, fiber_entry, 0);
    return id;
}

// Yield to scheduler
void __taipan_async_yield(void) {
    if (current_fiber < 0) return;
    TaipanFiber *f = &fibers[current_fiber];
    f->state = FIBER_READY;
    swapcontext(&f->ctx, &scheduler_ctx);
}

// Sleep for ms milliseconds
void __taipan_async_sleep(int32_t ms) {
    if (current_fiber < 0) { usleep((useconds_t)ms * 1000); return; }
    TaipanFiber *f = &fibers[current_fiber];
    f->state     = FIBER_WAITING;
    f->wake_time = now_ms() + ms;
    swapcontext(&f->ctx, &scheduler_ctx);
}

// Wait for a fiber to finish
void __taipan_async_join(int32_t id) {
    if (id < 0 || id >= fiber_count) return;
    while (fibers[id].state != FIBER_DONE)
        __taipan_async_yield();
}

// Get current fiber id
int32_t __taipan_async_self(void) { return current_fiber; }

// Run the event loop until all fibers done
void __taipan_async_run(void) {
    int running = 1;
    while (running) {
        running = 0;
        int64_t now = now_ms();
        for (int32_t i = 0; i < fiber_count; i++) {
            TaipanFiber *f = &fibers[i];
            if (f->state == FIBER_DONE) continue;
            running = 1;
            // Wake sleeping fibers
            if (f->state == FIBER_WAITING) {
                if (now >= f->wake_time) f->state = FIBER_READY;
                else continue;
            }
            if (f->state == FIBER_READY) {
                f->state     = FIBER_RUNNING;
                current_fiber = i;
                swapcontext(&scheduler_ctx, &f->ctx);
                current_fiber = -1;
            }
        }
    }
    // cleanup stacks
    for (int32_t i = 0; i < fiber_count; i++)
        if (fibers[i].stack) { free(fibers[i].stack); fibers[i].stack=NULL; }
    fiber_count = 0;
}

// Channel (simple message passing between fibers)
#define CHAN_BUF 64
typedef struct {
    char    *msgs[CHAN_BUF];
    int32_t  head, tail, count;
} TaipanChan;

void *__taipan_chan_new(void) {
    TaipanChan *c = calloc(1, sizeof(TaipanChan));
    return (void*)c;
}
int32_t __taipan_chan_send(void *cp, const char *msg) {
    TaipanChan *c = (TaipanChan*)cp;
    if (c->count >= CHAN_BUF) return -1;
    c->msgs[c->tail] = strdup(msg);
    c->tail = (c->tail + 1) % CHAN_BUF;
    c->count++;
    return 0;
}
char *__taipan_chan_recv(void *cp) {
    TaipanChan *c = (TaipanChan*)cp;
    while (c->count == 0) __taipan_async_yield();
    char *msg = c->msgs[c->head];
    c->head = (c->head + 1) % CHAN_BUF;
    c->count--;
    return msg;
}
int32_t __taipan_chan_len(void *cp) {
    return ((TaipanChan*)cp)->count;
}
void __taipan_chan_free(void *cp) {
    TaipanChan *c = (TaipanChan*)cp;
    for (int i=0;i<CHAN_BUF;i++) if(c->msgs[i]) free(c->msgs[i]);
    free(c);
}

// ─────────────────────────────────────────────
//  std.json — JSON parse and stringify
// ─────────────────────────────────────────────

typedef enum {
    JSON_NULL, JSON_BOOL, JSON_INT, JSON_FLOAT,
    JSON_STRING, JSON_ARRAY, JSON_OBJECT
} JsonType;

typedef struct JsonNode {
    JsonType type;
    char    *key;       // for object fields
    union {
        int32_t  bool_val;
        int32_t  int_val;
        float    float_val;
        char    *str_val;
        struct { struct JsonNode **items; int32_t count; } array;
        struct { struct JsonNode **fields; int32_t count; } object;
    } val;
    struct JsonNode *next;
} JsonNode;

static JsonNode *json_node_new(JsonType t) {
    JsonNode *n = calloc(1, sizeof(JsonNode));
    n->type = t;
    return n;
}

void __taipan_json_free(void *np) {
    JsonNode *n = (JsonNode*)np;
    if (!n) return;
    if (n->key) free(n->key);
    if (n->type == JSON_STRING && n->val.str_val) free(n->val.str_val);
    if (n->type == JSON_ARRAY) {
        for (int i=0;i<n->val.array.count;i++) __taipan_json_free(n->val.array.items[i]);
        free(n->val.array.items);
    }
    if (n->type == JSON_OBJECT) {
        for (int i=0;i<n->val.object.count;i++) __taipan_json_free(n->val.object.fields[i]);
        free(n->val.object.fields);
    }
    free(n);
}

// ── Parser ────────────────────────────────────
static const char *jp; // parse cursor

static void json_skip_ws(void) { while(*jp==' '||*jp=='\t'||*jp=='\n'||*jp=='\r') jp++; }

static JsonNode *json_parse_value(void);

static JsonNode *json_parse_string(void) {
    jp++; // skip "
    const char *start = jp;
    while (*jp && *jp != '"') { if (*jp=='\\') jp++; jp++; }
    size_t len = (size_t)(jp - start);
    char *s = malloc(len+1); memcpy(s,start,len); s[len]='\0';
    if (*jp=='"') jp++;
    JsonNode *n = json_node_new(JSON_STRING);
    n->val.str_val = s;
    return n;
}

static JsonNode *json_parse_number(void) {
    const char *start = jp;
    int is_float = 0;
    if (*jp=='-') jp++;
    while (*jp>='0'&&*jp<='9') jp++;
    if (*jp=='.') { is_float=1; jp++; while(*jp>='0'&&*jp<='9') jp++; }
    if (*jp=='e'||*jp=='E') { is_float=1; jp++; if(*jp=='+'||*jp=='-') jp++; while(*jp>='0'&&*jp<='9') jp++; }
    char buf[64]; size_t l=(size_t)(jp-start); if(l>63)l=63;
    memcpy(buf,start,l); buf[l]='\0';
    JsonNode *n;
    if (is_float) { n=json_node_new(JSON_FLOAT); n->val.float_val=(float)atof(buf); }
    else          { n=json_node_new(JSON_INT);   n->val.int_val=(int32_t)atoi(buf); }
    return n;
}

static JsonNode *json_parse_array(void) {
    jp++; // skip [
    JsonNode *n = json_node_new(JSON_ARRAY);
    n->val.array.items = NULL;
    n->val.array.count = 0;
    json_skip_ws();
    if (*jp==']') { jp++; return n; }
    int cap=8;
    n->val.array.items = malloc(sizeof(JsonNode*)*(size_t)cap);
    while (*jp && *jp!=']') {
        JsonNode *item = json_parse_value();
        if (n->val.array.count >= cap) { cap*=2; n->val.array.items=realloc(n->val.array.items,sizeof(JsonNode*)*(size_t)cap); }
        n->val.array.items[n->val.array.count++] = item;
        json_skip_ws();
        if (*jp==',') jp++;
        json_skip_ws();
    }
    if (*jp==']') jp++;
    return n;
}

static JsonNode *json_parse_object(void) {
    jp++; // skip {
    JsonNode *n = json_node_new(JSON_OBJECT);
    n->val.object.fields = NULL;
    n->val.object.count  = 0;
    json_skip_ws();
    if (*jp=='}') { jp++; return n; }
    int cap=8;
    n->val.object.fields = malloc(sizeof(JsonNode*)*(size_t)cap);
    while (*jp && *jp!='}') {
        json_skip_ws();
        if (*jp!='"') break;
        JsonNode *key_node = json_parse_string();
        char *key = key_node->val.str_val; key_node->val.str_val=NULL;
        __taipan_json_free(key_node);
        json_skip_ws();
        if (*jp==':') jp++;
        json_skip_ws();
        JsonNode *val = json_parse_value();
        val->key = key;
        if (n->val.object.count >= cap) { cap*=2; n->val.object.fields=realloc(n->val.object.fields,sizeof(JsonNode*)*(size_t)cap); }
        n->val.object.fields[n->val.object.count++] = val;
        json_skip_ws();
        if (*jp==',') jp++;
        json_skip_ws();
    }
    if (*jp=='}') jp++;
    return n;
}

static JsonNode *json_parse_value(void) {
    json_skip_ws();
    if (*jp=='"') return json_parse_string();
    if (*jp=='{') return json_parse_object();
    if (*jp=='[') return json_parse_array();
    if (*jp=='-'||(*jp>='0'&&*jp<='9')) return json_parse_number();
    if (strncmp(jp,"true",4)==0)  { jp+=4; JsonNode *n=json_node_new(JSON_BOOL);  n->val.bool_val=1; return n; }
    if (strncmp(jp,"false",5)==0) { jp+=5; JsonNode *n=json_node_new(JSON_BOOL);  n->val.bool_val=0; return n; }
    if (strncmp(jp,"null",4)==0)  { jp+=4; return json_node_new(JSON_NULL); }
    return json_node_new(JSON_NULL);
}

void *__taipan_json_parse(const char *input) {
    jp = input;
    return (void*)json_parse_value();
}

// ── Stringify ─────────────────────────────────
static void json_write(JsonNode *n, char **buf, size_t *len, size_t *cap) {
    #define JW(s) do { size_t sl=strlen(s); if(*len+sl+1>*cap){*cap=(*cap+sl)*2;*buf=realloc(*buf,*cap);} memcpy(*buf+*len,s,sl); *len+=sl; (*buf)[*len]='\0'; } while(0)
    if (!n) { JW("null"); return; }
    char tmp[64];
    switch(n->type) {
        case JSON_NULL:   JW("null"); break;
        case JSON_BOOL:   JW(n->val.bool_val?"true":"false"); break;
        case JSON_INT:    snprintf(tmp,64,"%d",n->val.int_val); JW(tmp); break;
        case JSON_FLOAT:  snprintf(tmp,64,"%g",n->val.float_val); JW(tmp); break;
        case JSON_STRING: JW("\""); JW(n->val.str_val); JW("\""); break;
        case JSON_ARRAY:
            JW("[");
            for(int i=0;i<n->val.array.count;i++){
                if(i) JW(",");
                json_write(n->val.array.items[i],buf,len,cap);
            }
            JW("]"); break;
        case JSON_OBJECT:
            JW("{");
            for(int i=0;i<n->val.object.count;i++){
                if(i) JW(",");
                JW("\""); JW(n->val.object.fields[i]->key); JW("\":");
                json_write(n->val.object.fields[i],buf,len,cap);
            }
            JW("}"); break;
    }
}

char *__taipan_json_stringify(void *np) {
    size_t len=0, cap=256;
    char *buf = malloc(cap);
    buf[0]='\0';
    json_write((JsonNode*)np, &buf, &len, &cap);
    return buf;
}

// ── Accessors ─────────────────────────────────
char   *__taipan_json_get_str  (void *np, const char *key) {
    JsonNode *n=(JsonNode*)np;
    if (!n||n->type!=JSON_OBJECT) return (char*)"";
    for(int i=0;i<n->val.object.count;i++)
        if(!strcmp(n->val.object.fields[i]->key,key))
            return n->val.object.fields[i]->type==JSON_STRING ? n->val.object.fields[i]->val.str_val : (char*)"";
    return (char*)"";
}
int32_t __taipan_json_get_int  (void *np, const char *key) {
    JsonNode *n=(JsonNode*)np;
    if (!n||n->type!=JSON_OBJECT) return 0;
    for(int i=0;i<n->val.object.count;i++)
        if(!strcmp(n->val.object.fields[i]->key,key))
            return n->val.object.fields[i]->val.int_val;
    return 0;
}
float   __taipan_json_get_float(void *np, const char *key) {
    JsonNode *n=(JsonNode*)np;
    if (!n||n->type!=JSON_OBJECT) return 0.0f;
    for(int i=0;i<n->val.object.count;i++)
        if(!strcmp(n->val.object.fields[i]->key,key))
            return n->val.object.fields[i]->val.float_val;
    return 0.0f;
}
int32_t __taipan_json_array_len(void *np) {
    JsonNode *n=(JsonNode*)np;
    return (n&&n->type==JSON_ARRAY) ? n->val.array.count : 0;
}
void   *__taipan_json_array_get(void *np, int32_t idx) {
    JsonNode *n=(JsonNode*)np;
    if (!n||n->type!=JSON_ARRAY||idx<0||idx>=n->val.array.count) return NULL;
    return (void*)n->val.array.items[idx];
}
char   *__taipan_json_to_str   (void *np) {
    JsonNode *n=(JsonNode*)np;
    if (!n) return (char*)"null";
    if (n->type==JSON_STRING) return n->val.str_val;
    return __taipan_json_stringify(np);
}

// ── Builder ───────────────────────────────────
void *__taipan_json_object_new (void) { return (void*)json_node_new(JSON_OBJECT); }
void *__taipan_json_array_new2 (void) { return (void*)json_node_new(JSON_ARRAY); }
void  __taipan_json_set_str    (void *np, const char *key, const char *val) {
    JsonNode *n=(JsonNode*)np; if(!n||n->type!=JSON_OBJECT) return;
    JsonNode *v=json_node_new(JSON_STRING); v->key=strdup(key); v->val.str_val=strdup(val);
    n->val.object.fields=realloc(n->val.object.fields,sizeof(JsonNode*)*(size_t)(n->val.object.count+1));
    n->val.object.fields[n->val.object.count++]=v;
}
void  __taipan_json_set_int    (void *np, const char *key, int32_t val) {
    JsonNode *n=(JsonNode*)np; if(!n||n->type!=JSON_OBJECT) return;
    JsonNode *v=json_node_new(JSON_INT); v->key=strdup(key); v->val.int_val=val;
    n->val.object.fields=realloc(n->val.object.fields,sizeof(JsonNode*)*(size_t)(n->val.object.count+1));
    n->val.object.fields[n->val.object.count++]=v;
}
void  __taipan_json_array_push2(void *np, void *item) {
    JsonNode *n=(JsonNode*)np; if(!n||n->type!=JSON_ARRAY) return;
    n->val.array.items=realloc(n->val.array.items,sizeof(JsonNode*)*(size_t)(n->val.array.count+1));
    n->val.array.items[n->val.array.count++]=(JsonNode*)item;
}

// ─────────────────────────────────────────────
//  std.thread — POSIX threads
// ─────────────────────────────────────────────
#include <pthread.h>

typedef struct {
    pthread_t   tid;
    void      (*fn)(void);
    int32_t     done;
} TaipanThread;

#define MAX_THREADS 64
static TaipanThread threads[MAX_THREADS];
static int32_t      thread_count = 0;

static void *thread_entry(void *arg) {
    TaipanThread *t = (TaipanThread*)arg;
    t->fn();
    t->done = 1;
    return NULL;
}

int32_t __taipan_thread_spawn(void (*fn)(void)) {
    if (thread_count >= MAX_THREADS) return -1;
    int32_t id = thread_count++;
    TaipanThread *t = &threads[id];
    t->fn   = fn;
    t->done = 0;
    pthread_create(&t->tid, NULL, thread_entry, (void*)t);
    return id;
}
void __taipan_thread_join(int32_t id) {
    if (id<0||id>=thread_count) return;
    pthread_join(threads[id].tid, NULL);
}
void __taipan_thread_sleep(int32_t ms) {
    usleep((useconds_t)ms * 1000);
}
int32_t __taipan_thread_done(int32_t id) {
    if (id<0||id>=thread_count) return 1;
    return threads[id].done;
}

// ── Mutex ─────────────────────────────────────
void *__taipan_mutex_new(void) {
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    return (void*)m;
}
void __taipan_mutex_lock(void *mp) {
    pthread_mutex_lock((pthread_mutex_t*)mp);
}
void __taipan_mutex_unlock(void *mp) {
    pthread_mutex_unlock((pthread_mutex_t*)mp);
}
void __taipan_mutex_free(void *mp) {
    pthread_mutex_destroy((pthread_mutex_t*)mp);
    free(mp);
}

// ─────────────────────────────────────────────
//  std.data — Dataset utilities for ML
// ─────────────────────────────────────────────

typedef struct {
    float   *X;       // features [n_samples x n_features]
    float   *y;       // labels   [n_samples x n_labels]
    int32_t  n_samples;
    int32_t  n_features;
    int32_t  n_labels;
    int32_t *indices;  // for shuffling
} TaipanDataset;

// ── Create dataset ────────────────────────────
void *__taipan_data_new(int32_t n_samples, int32_t n_features, int32_t n_labels) {
    TaipanDataset *d = malloc(sizeof(TaipanDataset));
    d->n_samples  = n_samples;
    d->n_features = n_features;
    d->n_labels   = n_labels;
    d->X = calloc((size_t)(n_samples * n_features), sizeof(float));
    d->y = calloc((size_t)(n_samples * n_labels),   sizeof(float));
    d->indices = malloc(sizeof(int32_t) * (size_t)n_samples);
    for (int32_t i = 0; i < n_samples; i++) d->indices[i] = i;
    return (void*)d;
}

void __taipan_data_free(void *dp) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return;
    free(d->X); free(d->y); free(d->indices); free(d);
}

// ── Set/get samples ───────────────────────────
void __taipan_data_set_x(void *dp, int32_t row, int32_t col, float val) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples||col<0||col>=d->n_features) return;
    d->X[row * d->n_features + col] = val;
}
float __taipan_data_get_x(void *dp, int32_t row, int32_t col) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples||col<0||col>=d->n_features) return 0.0f;
    return d->X[row * d->n_features + col];
}
void __taipan_data_set_y(void *dp, int32_t row, int32_t col, float val) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples||col<0||col>=d->n_labels) return;
    d->y[row * d->n_labels + col] = val;
}
float __taipan_data_get_y(void *dp, int32_t row, int32_t col) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples||col<0||col>=d->n_labels) return 0.0f;
    return d->y[row * d->n_labels + col];
}
int32_t __taipan_data_n_samples (void *dp) { return ((TaipanDataset*)dp)->n_samples; }
int32_t __taipan_data_n_features(void *dp) { return ((TaipanDataset*)dp)->n_features; }
int32_t __taipan_data_n_labels  (void *dp) { return ((TaipanDataset*)dp)->n_labels; }

// ── Shuffle ───────────────────────────────────
void __taipan_data_shuffle(void *dp) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return;
    for (int32_t i = d->n_samples-1; i > 0; i--) {
        int32_t j = rand() % (i+1);
        int32_t tmp = d->indices[i];
        d->indices[i] = d->indices[j];
        d->indices[j] = tmp;
    }
}

// ── Get a batch as tensors ─────────────────────
// Returns X tensor for batch starting at offset
void *__taipan_data_batch_x(void *dp, int32_t offset, int32_t batch_size) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return NULL;
    if (offset + batch_size > d->n_samples) batch_size = d->n_samples - offset;
    int32_t s[2] = {batch_size, d->n_features};
    TaipanTensor *t = __taipan_tensor_new(s, 2);
    for (int32_t i = 0; i < batch_size; i++) {
        int32_t idx = d->indices[offset + i];
        for (int32_t j = 0; j < d->n_features; j++)
            t->data[i * d->n_features + j] = d->X[idx * d->n_features + j];
    }
    return (void*)t;
}
// Returns y tensor for batch
void *__taipan_data_batch_y(void *dp, int32_t offset, int32_t batch_size) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return NULL;
    if (offset + batch_size > d->n_samples) batch_size = d->n_samples - offset;
    int32_t s[2] = {batch_size, d->n_labels};
    TaipanTensor *t = __taipan_tensor_new(s, 2);
    for (int32_t i = 0; i < batch_size; i++) {
        int32_t idx = d->indices[offset + i];
        for (int32_t j = 0; j < d->n_labels; j++)
            t->data[i * d->n_labels + j] = d->y[idx * d->n_labels + j];
    }
    return (void*)t;
}

// ── Normalization ─────────────────────────────
// Normalize features to [0,1] per column
void __taipan_data_normalize(void *dp) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return;
    for (int32_t j = 0; j < d->n_features; j++) {
        float mn = d->X[j], mx = d->X[j];
        for (int32_t i = 1; i < d->n_samples; i++) {
            float v = d->X[i * d->n_features + j];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        float range = mx - mn;
        if (range < 1e-8f) range = 1.0f;
        for (int32_t i = 0; i < d->n_samples; i++)
            d->X[i * d->n_features + j] = (d->X[i * d->n_features + j] - mn) / range;
    }
}

// Standardize features to mean=0, std=1 per column
void __taipan_data_standardize(void *dp) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return;
    for (int32_t j = 0; j < d->n_features; j++) {
        float mean = 0.0f;
        for (int32_t i = 0; i < d->n_samples; i++)
            mean += d->X[i * d->n_features + j];
        mean /= (float)d->n_samples;
        float var = 0.0f;
        for (int32_t i = 0; i < d->n_samples; i++) {
            float diff = d->X[i * d->n_features + j] - mean;
            var += diff * diff;
        }
        var /= (float)d->n_samples;
        float std = sqrtf(var + 1e-8f);
        for (int32_t i = 0; i < d->n_samples; i++)
            d->X[i * d->n_features + j] = (d->X[i * d->n_features + j] - mean) / std;
    }
}

// ── Train/test split ──────────────────────────
// Returns a new dataset with first split_at samples
void *__taipan_data_split_train(void *dp, float ratio) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return NULL;
    int32_t n_train = (int32_t)((float)d->n_samples * ratio);
    TaipanDataset *train = (TaipanDataset*)__taipan_data_new(n_train, d->n_features, d->n_labels);
    for (int32_t i = 0; i < n_train; i++) {
        int32_t idx = d->indices[i];
        memcpy(train->X + i*d->n_features, d->X + idx*d->n_features, sizeof(float)*(size_t)d->n_features);
        memcpy(train->y + i*d->n_labels,   d->y + idx*d->n_labels,   sizeof(float)*(size_t)d->n_labels);
    }
    return (void*)train;
}
void *__taipan_data_split_test(void *dp, float ratio) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return NULL;
    int32_t n_train = (int32_t)((float)d->n_samples * ratio);
    int32_t n_test  = d->n_samples - n_train;
    TaipanDataset *test = (TaipanDataset*)__taipan_data_new(n_test, d->n_features, d->n_labels);
    for (int32_t i = 0; i < n_test; i++) {
        int32_t idx = d->indices[n_train + i];
        memcpy(test->X + i*d->n_features, d->X + idx*d->n_features, sizeof(float)*(size_t)d->n_features);
        memcpy(test->y + i*d->n_labels,   d->y + idx*d->n_labels,   sizeof(float)*(size_t)d->n_labels);
    }
    return (void*)test;
}

// ── Load CSV ──────────────────────────────────
// Simple CSV loader: last column = label
void *__taipan_data_load_csv(const char *path, int32_t n_features, int32_t n_labels) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    // Count lines
    int32_t n = 0;
    char line[4096];
    while (fgets(line, sizeof(line), f)) n++;
    rewind(f);
    TaipanDataset *d = (TaipanDataset*)__taipan_data_new(n, n_features, n_labels);
    int32_t row = 0;
    while (fgets(line, sizeof(line), f) && row < n) {
        char *tok = strtok(line, ",\n");
        for (int32_t col = 0; col < n_features + n_labels && tok; col++) {
            float v = (float)atof(tok);
            if (col < n_features) d->X[row*n_features+col] = v;
            else                  d->y[row*n_labels+(col-n_features)] = v;
            tok = strtok(NULL, ",\n");
        }
        row++;
    }
    fclose(f);
    return (void*)d;
}

// ── Save CSV ──────────────────────────────────
int32_t __taipan_data_save_csv(void *dp, const char *path) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int32_t i = 0; i < d->n_samples; i++) {
        for (int32_t j = 0; j < d->n_features; j++) {
            if (j) fprintf(f, ",");
            fprintf(f, "%g", d->X[i*d->n_features+j]);
        }
        for (int32_t j = 0; j < d->n_labels; j++)
            fprintf(f, ",%g", d->y[i*d->n_labels+j]);
        fprintf(f, "\n");
    }
    fclose(f);
    return 0;
}

// ── Get sample as tensor ──────────────────────
void *__taipan_data_get_x_tensor(void *dp, int32_t row) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples) return NULL;
    TaipanTensor *t = __taipan_tensor_1d(d->n_features);
    memcpy(t->data, d->X + row*d->n_features, sizeof(float)*(size_t)d->n_features);
    return (void*)t;
}
void *__taipan_data_get_y_tensor(void *dp, int32_t row) {
    TaipanDataset *d = (TaipanDataset*)dp;
    if (!d||row<0||row>=d->n_samples) return NULL;
    TaipanTensor *t = __taipan_tensor_1d(d->n_labels);
    memcpy(t->data, d->y + row*d->n_labels, sizeof(float)*(size_t)d->n_labels);
    return (void*)t;
}

// ─────────────────────────────────────────────
//  std.transformer — Production Transformer
//  Pure C, no dependencies, faster than Python
//  Architecture: GPT-style (decoder-only) +
//                BERT-style (encoder-only)
// ─────────────────────────────────────────────

// ── Config ────────────────────────────────────
typedef struct {
    int32_t vocab_size;
    int32_t seq_len;
    int32_t d_model;    // embedding dim
    int32_t n_heads;    // attention heads
    int32_t n_layers;   // transformer blocks
    int32_t d_ff;       // feedforward dim (usually 4*d_model)
    float   dropout;    // dropout rate (0 = disabled)
} TransformerConfig;

void *__taipan_transformer_config(int32_t vocab, int32_t seq, int32_t d_model,
                                   int32_t heads, int32_t layers, int32_t d_ff) {
    TransformerConfig *c = malloc(sizeof(TransformerConfig));
    c->vocab_size = vocab;
    c->seq_len    = seq;
    c->d_model    = d_model;
    c->n_heads    = heads;
    c->n_layers   = layers;
    c->d_ff       = d_ff;
    c->dropout    = 0.0f;
    return (void*)c;
}

// ── Embedding ─────────────────────────────────
typedef struct {
    TaipanTensor *token_embed;   // [vocab x d_model]
    TaipanTensor *pos_embed;     // [seq x d_model]
    int32_t       d_model;
    int32_t       seq_len;
} TaipanEmbedding;

void *__taipan_embed_new(int32_t vocab, int32_t seq, int32_t d_model) {
    TaipanEmbedding *e = malloc(sizeof(TaipanEmbedding));
    e->d_model  = d_model;
    e->seq_len  = seq;
    e->token_embed = __taipan_tensor_2d(vocab, d_model);
    e->pos_embed   = __taipan_tensor_2d(seq,   d_model);
    // Xavier init token embeddings
    float limit = sqrtf(1.0f / (float)d_model);
    for (int32_t i = 0; i < e->token_embed->size; i++)
        e->token_embed->data[i] = ((float)rand()/(float)RAND_MAX)*2.0f*limit - limit;
    // Sinusoidal positional encoding
    for (int32_t pos = 0; pos < seq; pos++) {
        for (int32_t i = 0; i < d_model; i++) {
            float angle = (float)pos / powf(10000.0f, (float)(i/2*2) / (float)d_model);
            e->pos_embed->data[pos*d_model+i] = (i%2==0) ? sinf(angle) : cosf(angle);
        }
    }
    return (void*)e;
}

// Forward: token_ids[seq] -> output[seq x d_model]
void *__taipan_embed_forward(void *ep, int32_t *token_ids, int32_t seq) {
    TaipanEmbedding *e = (TaipanEmbedding*)ep;
    int32_t s[2] = {seq, e->d_model};
    TaipanTensor *out = __taipan_tensor_new(s, 2);
    for (int32_t t = 0; t < seq; t++) {
        int32_t tid = token_ids[t];
        if (tid < 0) tid = 0;
        for (int32_t d = 0; d < e->d_model; d++)
            out->data[t*e->d_model+d] =
                e->token_embed->data[tid*e->d_model+d] +
                e->pos_embed->data[t*e->d_model+d];
    }
    return (void*)out;
}

// Convenience: embed from i32 tensor of token ids
void *__taipan_embed_tensor(void *ep, void *ids_tp) {
    TaipanEmbedding *e  = (TaipanEmbedding*)ep;
    TaipanTensor    *ids = (TaipanTensor*)ids_tp;
    int32_t seq = ids->size;
    int32_t s[2] = {seq, e->d_model};
    TaipanTensor *out = __taipan_tensor_new(s, 2);
    for (int32_t t = 0; t < seq; t++) {
        int32_t tid = (int32_t)ids->data[t];
        if (tid < 0) tid = 0;
        for (int32_t d = 0; d < e->d_model; d++)
            out->data[t*e->d_model+d] =
                e->token_embed->data[tid*e->d_model+d] +
                e->pos_embed->data[t*e->d_model+d];
    }
    return (void*)out;
}

void __taipan_embed_free(void *ep) {
    TaipanEmbedding *e = (TaipanEmbedding*)ep;
    __taipan_tensor_free(e->token_embed);
    __taipan_tensor_free(e->pos_embed);
    free(e);
}

// ── Scaled Dot-Product Attention ──────────────
// Q[seq x d_k], K[seq x d_k], V[seq x d_v] -> out[seq x d_v]
static TaipanTensor *scaled_dot_attention(TaipanTensor *Q, TaipanTensor *K,
                                           TaipanTensor *V, int32_t causal) {
    int32_t seq  = Q->shape[0];
    int32_t d_k  = Q->shape[1];
    int32_t d_v  = V->shape[1];
    float   scale = 1.0f / sqrtf((float)d_k);

    // scores = Q @ K^T * scale  [seq x seq]
    int32_t ss[2] = {seq, seq};
    TaipanTensor *scores = __taipan_tensor_new(ss, 2);
    for (int32_t i = 0; i < seq; i++)
        for (int32_t j = 0; j < seq; j++) {
            float s = 0.0f;
            for (int32_t k = 0; k < d_k; k++)
                s += Q->data[i*d_k+k] * K->data[j*d_k+k];
            scores->data[i*seq+j] = s * scale;
            // Causal mask: future positions = -inf
            if (causal && j > i) scores->data[i*seq+j] = -1e9f;
        }

    // softmax over last dim (per row)
    for (int32_t i = 0; i < seq; i++) {
        float mx = scores->data[i*seq];
        for (int32_t j = 1; j < seq; j++) if (scores->data[i*seq+j]>mx) mx=scores->data[i*seq+j];
        float sum = 0.0f;
        for (int32_t j = 0; j < seq; j++) { scores->data[i*seq+j]=expf(scores->data[i*seq+j]-mx); sum+=scores->data[i*seq+j]; }
        for (int32_t j = 0; j < seq; j++) scores->data[i*seq+j] /= sum;
    }

    // out = scores @ V  [seq x d_v]
    int32_t os[2] = {seq, d_v};
    TaipanTensor *out = __taipan_tensor_new(os, 2);
    for (int32_t i = 0; i < seq; i++)
        for (int32_t j = 0; j < d_v; j++) {
            float s = 0.0f;
            for (int32_t k = 0; k < seq; k++)
                s += scores->data[i*seq+k] * V->data[k*d_v+j];
            out->data[i*d_v+j] = s;
        }
    __taipan_tensor_free(scores);
    return out;
}

// ── Multi-Head Attention ──────────────────────
typedef struct {
    TaipanTensor *Wq, *Wk, *Wv, *Wo; // [d_model x d_model]
    TaipanTensor *bq, *bk, *bv, *bo; // [d_model]
    int32_t d_model, n_heads, d_k;
    int32_t causal;
} TaipanMHA;

void *__taipan_mha_new(int32_t d_model, int32_t n_heads, int32_t causal) {
    TaipanMHA *m = malloc(sizeof(TaipanMHA));
    m->d_model = d_model;
    m->n_heads = n_heads;
    m->d_k     = d_model / n_heads;
    m->causal  = causal;
    float limit = sqrtf(2.0f / (float)(d_model + d_model));
    #define INIT_W(W, r, c) \
        W = __taipan_tensor_2d(r,c); \
        for(int32_t _i=0;_i<W->size;_i++) W->data[_i]=((float)rand()/(float)RAND_MAX)*2.0f*limit-limit;
    #define INIT_B(B, n) B = __taipan_tensor_1d(n); __taipan_tensor_zeros(B);
    INIT_W(m->Wq, d_model, d_model)
    INIT_W(m->Wk, d_model, d_model)
    INIT_W(m->Wv, d_model, d_model)
    INIT_W(m->Wo, d_model, d_model)
    INIT_B(m->bq, d_model)
    INIT_B(m->bk, d_model)
    INIT_B(m->bv, d_model)
    INIT_B(m->bo, d_model)
    return (void*)m;
}

// Forward: x[seq x d_model] -> out[seq x d_model]
void *__taipan_mha_forward(void *mp, void *xp) {
    TaipanMHA    *m = (TaipanMHA*)mp;
    TaipanTensor *x = (TaipanTensor*)xp;
    int32_t seq     = x->shape[0];
    int32_t d       = m->d_model;
    int32_t dk      = m->d_k;
    int32_t h       = m->n_heads;

    // Project Q, K, V: [seq x d_model]
    int32_t s2[2] = {seq, d};
    TaipanTensor *Q = __taipan_tensor_new(s2, 2);
    TaipanTensor *K = __taipan_tensor_new(s2, 2);
    TaipanTensor *V = __taipan_tensor_new(s2, 2);

    for (int32_t t = 0; t < seq; t++)
        for (int32_t j = 0; j < d; j++) {
            float q=m->bq->data[j], k2=m->bk->data[j], v=m->bv->data[j];
            for (int32_t i2 = 0; i2 < d; i2++) {
                float xi = x->data[t*d+i2];
                q  += m->Wq->data[j*d+i2] * xi;
                k2 += m->Wk->data[j*d+i2] * xi;
                v  += m->Wv->data[j*d+i2] * xi;
            }
            Q->data[t*d+j] = q;
            K->data[t*d+j] = k2;
            V->data[t*d+j] = v;
        }

    // Multi-head attention: split heads, attend, concat
    int32_t out_s[2] = {seq, d};
    TaipanTensor *concat = __taipan_tensor_new(out_s, 2);

    for (int32_t hi = 0; hi < h; hi++) {
        // Extract head slice [seq x dk]
        int32_t hs[2] = {seq, dk};
        TaipanTensor *Qh = __taipan_tensor_new(hs, 2);
        TaipanTensor *Kh = __taipan_tensor_new(hs, 2);
        TaipanTensor *Vh = __taipan_tensor_new(hs, 2);
        for (int32_t t = 0; t < seq; t++)
            for (int32_t j = 0; j < dk; j++) {
                Qh->data[t*dk+j] = Q->data[t*d + hi*dk + j];
                Kh->data[t*dk+j] = K->data[t*d + hi*dk + j];
                Vh->data[t*dk+j] = V->data[t*d + hi*dk + j];
            }
        TaipanTensor *head_out = scaled_dot_attention(Qh, Kh, Vh, m->causal);
        // Write head output into concat
        for (int32_t t = 0; t < seq; t++)
            for (int32_t j = 0; j < dk; j++)
                concat->data[t*d + hi*dk + j] = head_out->data[t*dk+j];
        __taipan_tensor_free(Qh);
        __taipan_tensor_free(Kh);
        __taipan_tensor_free(Vh);
        __taipan_tensor_free(head_out);
    }
    __taipan_tensor_free(Q);
    __taipan_tensor_free(K);
    __taipan_tensor_free(V);

    // Output projection Wo: [seq x d_model]
    TaipanTensor *out = __taipan_tensor_new(out_s, 2);
    for (int32_t t = 0; t < seq; t++)
        for (int32_t j = 0; j < d; j++) {
            float s = m->bo->data[j];
            for (int32_t i2 = 0; i2 < d; i2++)
                s += m->Wo->data[j*d+i2] * concat->data[t*d+i2];
            out->data[t*d+j] = s;
        }
    __taipan_tensor_free(concat);
    return (void*)out;
}

void __taipan_mha_free(void *mp) {
    TaipanMHA *m = (TaipanMHA*)mp;
    __taipan_tensor_free(m->Wq); __taipan_tensor_free(m->Wk);
    __taipan_tensor_free(m->Wv); __taipan_tensor_free(m->Wo);
    __taipan_tensor_free(m->bq); __taipan_tensor_free(m->bk);
    __taipan_tensor_free(m->bv); __taipan_tensor_free(m->bo);
    free(m);
}

// ── FeedForward Block ─────────────────────────
typedef struct {
    TaipanTensor *W1, *b1;  // [d_ff x d_model]
    TaipanTensor *W2, *b2;  // [d_model x d_ff]
    int32_t d_model, d_ff;
} TaipanFFN;

void *__taipan_ffn_new(int32_t d_model, int32_t d_ff) {
    TaipanFFN *f = malloc(sizeof(TaipanFFN));
    f->d_model = d_model;
    f->d_ff    = d_ff;
    float l1 = sqrtf(2.0f/(float)(d_model+d_ff));
    float l2 = sqrtf(2.0f/(float)(d_ff+d_model));
    f->W1 = __taipan_tensor_2d(d_ff,    d_model);
    f->W2 = __taipan_tensor_2d(d_model, d_ff);
    f->b1 = __taipan_tensor_1d(d_ff);
    f->b2 = __taipan_tensor_1d(d_model);
    for(int32_t i=0;i<f->W1->size;i++) f->W1->data[i]=((float)rand()/(float)RAND_MAX)*2.0f*l1-l1;
    for(int32_t i=0;i<f->W2->size;i++) f->W2->data[i]=((float)rand()/(float)RAND_MAX)*2.0f*l2-l2;
    __taipan_tensor_zeros(f->b1);
    __taipan_tensor_zeros(f->b2);
    return (void*)f;
}

// Forward: x[seq x d_model] -> out[seq x d_model]
void *__taipan_ffn_forward(void *fp, void *xp) {
    TaipanFFN    *f = (TaipanFFN*)fp;
    TaipanTensor *x = (TaipanTensor*)xp;
    int32_t seq = x->shape[0];

    // hidden = GELU(x @ W1^T + b1)  [seq x d_ff]
    int32_t hs[2] = {seq, f->d_ff};
    TaipanTensor *hidden = __taipan_tensor_new(hs, 2);
    for (int32_t t = 0; t < seq; t++)
        for (int32_t j = 0; j < f->d_ff; j++) {
            float s = f->b1->data[j];
            for (int32_t i = 0; i < f->d_model; i++)
                s += f->W1->data[j*f->d_model+i] * x->data[t*f->d_model+i];
            // GELU activation
            hidden->data[t*f->d_ff+j] = 0.5f*s*(1.0f+tanhf(0.7978845608f*(s+0.044715f*s*s*s)));
        }

    // out = hidden @ W2^T + b2  [seq x d_model]
    int32_t os[2] = {seq, f->d_model};
    TaipanTensor *out = __taipan_tensor_new(os, 2);
    for (int32_t t = 0; t < seq; t++)
        for (int32_t j = 0; j < f->d_model; j++) {
            float s = f->b2->data[j];
            for (int32_t i = 0; i < f->d_ff; i++)
                s += f->W2->data[j*f->d_ff+i] * hidden->data[t*f->d_ff+i];
            out->data[t*f->d_model+j] = s;
        }
    __taipan_tensor_free(hidden);
    return (void*)out;
}

void __taipan_ffn_free(void *fp) {
    TaipanFFN *f = (TaipanFFN*)fp;
    __taipan_tensor_free(f->W1); __taipan_tensor_free(f->W2);
    __taipan_tensor_free(f->b1); __taipan_tensor_free(f->b2);
    free(f);
}

// ── Transformer Block ─────────────────────────
typedef struct {
    TaipanMHA *attn;
    TaipanFFN *ffn;
    int32_t    d_model;
} TaipanBlock;

void *__taipan_block_new(int32_t d_model, int32_t n_heads, int32_t d_ff, int32_t causal) {
    TaipanBlock *b = malloc(sizeof(TaipanBlock));
    b->d_model = d_model;
    b->attn    = (TaipanMHA*)__taipan_mha_new(d_model, n_heads, causal);
    b->ffn     = (TaipanFFN*)__taipan_ffn_new(d_model, d_ff);
    return (void*)b;
}

// Forward: x[seq x d_model] -> out[seq x d_model]
// Pre-norm architecture (like GPT-2)
void *__taipan_block_forward(void *bp, void *xp) {
    TaipanBlock  *b = (TaipanBlock*)bp;
    TaipanTensor *x = (TaipanTensor*)xp;
    int32_t seq = x->shape[0];
    int32_t d   = b->d_model;

    // LayerNorm 1 + attention + residual
    TaipanTensor *ln1 = __taipan_tensor_layer_norm(x, 1e-5f);
    // Reshape ln1 to [seq x d_model]
    int32_t s2[2] = {seq, d};
    ln1->ndim = 2; ln1->shape[0] = seq; ln1->shape[1] = d;
    ln1->strides[0] = d; ln1->strides[1] = 1;

    TaipanTensor *attn_out = (TaipanTensor*)__taipan_mha_forward(b->attn, ln1);
    // Residual connection
    TaipanTensor *res1 = __taipan_tensor_new(s2, 2);
    for (int32_t i = 0; i < x->size; i++) res1->data[i] = x->data[i] + attn_out->data[i];
    __taipan_tensor_free(ln1);
    __taipan_tensor_free(attn_out);

    // LayerNorm 2 + FFN + residual
    TaipanTensor *ln2 = __taipan_tensor_layer_norm(res1, 1e-5f);
    ln2->ndim = 2; ln2->shape[0] = seq; ln2->shape[1] = d;
    ln2->strides[0] = d; ln2->strides[1] = 1;

    TaipanTensor *ffn_out = (TaipanTensor*)__taipan_ffn_forward(b->ffn, ln2);
    TaipanTensor *out = __taipan_tensor_new(s2, 2);
    for (int32_t i = 0; i < res1->size; i++) out->data[i] = res1->data[i] + ffn_out->data[i];
    __taipan_tensor_free(ln2);
    __taipan_tensor_free(ffn_out);
    __taipan_tensor_free(res1);
    return (void*)out;
}

void __taipan_block_free(void *bp) {
    TaipanBlock *b = (TaipanBlock*)bp;
    __taipan_mha_free(b->attn);
    __taipan_ffn_free(b->ffn);
    free(b);
}

// ── Full Transformer (GPT-style decoder) ──────
#define MAX_BLOCKS 32
typedef struct {
    TaipanEmbedding *embed;
    TaipanBlock     *blocks[MAX_BLOCKS];
    TaipanTensor    *lm_head_W;  // [vocab x d_model] — output projection
    TaipanTensor    *lm_head_b;  // [vocab]
    int32_t          n_layers;
    int32_t          d_model;
    int32_t          vocab_size;
} TaipanTransformer;

void *__taipan_transformer_new(void *cfg_p) {
    TransformerConfig *cfg = (TransformerConfig*)cfg_p;
    TaipanTransformer *t   = malloc(sizeof(TaipanTransformer));
    t->n_layers   = cfg->n_layers;
    t->d_model    = cfg->d_model;
    t->vocab_size = cfg->vocab_size;
    t->embed      = (TaipanEmbedding*)__taipan_embed_new(cfg->vocab_size, cfg->seq_len, cfg->d_model);
    for (int32_t i = 0; i < cfg->n_layers; i++)
        t->blocks[i] = (TaipanBlock*)__taipan_block_new(cfg->d_model, cfg->n_heads, cfg->d_ff, 1);
    // LM head
    t->lm_head_W = __taipan_tensor_2d(cfg->vocab_size, cfg->d_model);
    t->lm_head_b = __taipan_tensor_1d(cfg->vocab_size);
    float lim = sqrtf(1.0f/(float)cfg->d_model);
    for(int32_t i=0;i<t->lm_head_W->size;i++)
        t->lm_head_W->data[i]=((float)rand()/(float)RAND_MAX)*2.0f*lim-lim;
    __taipan_tensor_zeros(t->lm_head_b);
    return (void*)t;
}

// Forward: token_ids tensor [seq] -> logits [seq x vocab]
void *__taipan_transformer_forward(void *tp, void *ids_p) {
    TaipanTransformer *t   = (TaipanTransformer*)tp;
    TaipanTensor      *ids = (TaipanTensor*)ids_p;
    int32_t seq = ids->size;

    // Embed
    TaipanTensor *x = (TaipanTensor*)__taipan_embed_tensor(t->embed, ids);
    int32_t s2[2] = {seq, t->d_model};
    x->ndim=2; x->shape[0]=seq; x->shape[1]=t->d_model;
    x->strides[0]=t->d_model; x->strides[1]=1;

    // Transformer blocks
    for (int32_t i = 0; i < t->n_layers; i++) {
        TaipanTensor *next = (TaipanTensor*)__taipan_block_forward(t->blocks[i], x);
        __taipan_tensor_free(x);
        x = next;
    }

    // LM head: [seq x d_model] -> [seq x vocab]
    int32_t ls[2] = {seq, t->vocab_size};
    TaipanTensor *logits = __taipan_tensor_new(ls, 2);
    for (int32_t ti = 0; ti < seq; ti++)
        for (int32_t v = 0; v < t->vocab_size; v++) {
            float s = t->lm_head_b->data[v];
            for (int32_t d = 0; d < t->d_model; d++)
                s += t->lm_head_W->data[v*t->d_model+d] * x->data[ti*t->d_model+d];
            logits->data[ti*t->vocab_size+v] = s;
        }
    __taipan_tensor_free(x);
    return (void*)logits;
}

// Get logits for last token -> softmax -> next token probs [vocab]
void *__taipan_transformer_next_probs(void *tp, void *ids_p) {
    TaipanTensor *logits = (TaipanTensor*)__taipan_transformer_forward(tp, ids_p);
    TaipanTransformer *t = (TaipanTransformer*)tp;
    TaipanTensor *ids    = (TaipanTensor*)ids_p;
    int32_t seq   = ids->size;
    int32_t vocab = t->vocab_size;
    // Extract last token logits
    TaipanTensor *last = __taipan_tensor_1d(vocab);
    memcpy(last->data, logits->data + (seq-1)*vocab, sizeof(float)*(size_t)vocab);
    __taipan_tensor_free(logits);
    TaipanTensor *probs = (TaipanTensor*)__taipan_tensor_softmax(last);
    __taipan_tensor_free(last);
    return (void*)probs;
}

// Greedy decode: given context, predict next n tokens
void *__taipan_transformer_generate(void *tp, void *ids_p, int32_t n_new) {
    TaipanTransformer *t   = (TaipanTransformer*)tp;
    TaipanTensor      *ids = (TaipanTensor*)ids_p;
    int32_t orig_seq = ids->size;
    int32_t max_seq  = orig_seq + n_new;
    // ctx holds full growing sequence
    TaipanTensor *ctx = __taipan_tensor_1d(max_seq);
    memcpy(ctx->data, ids->data, sizeof(float)*(size_t)orig_seq);
    int32_t cur_seq = orig_seq;
    TaipanTensor *out = __taipan_tensor_1d(n_new);
    for (int32_t step = 0; step < n_new; step++) {
        TaipanTensor *window = __taipan_tensor_1d(cur_seq);
        memcpy(window->data, ctx->data, sizeof(float)*(size_t)cur_seq);
        TaipanTensor *probs = (TaipanTensor*)__taipan_transformer_next_probs(t, window);
        int32_t best = 0; float best_p = probs->data[0];
        for (int32_t v = 1; v < t->vocab_size; v++)
            if (probs->data[v] > best_p) { best_p=probs->data[v]; best=v; }
        ctx->data[cur_seq] = (float)best;
        out->data[step]    = (float)best;
        cur_seq++;
        __taipan_tensor_free(window);
        __taipan_tensor_free(probs);
    }
    __taipan_tensor_free(ctx);
    return (void*)out;
}

// Cross entropy loss for language modeling
float __taipan_transformer_loss(void *logits_p, void *targets_p) {
    TaipanTensor *logits  = (TaipanTensor*)logits_p;
    TaipanTensor *targets = (TaipanTensor*)targets_p;
    int32_t seq   = targets->size;
    int32_t vocab = logits->size / seq;
    float loss = 0.0f;
    for (int32_t t = 0; t < seq; t++) {
        int32_t tgt = (int32_t)targets->data[t];
        // log softmax
        float *row = logits->data + t*vocab;
        float mx = row[0];
        for(int32_t v=1;v<vocab;v++) if(row[v]>mx) mx=row[v];
        float sum=0.0f;
        for(int32_t v=0;v<vocab;v++) sum+=expf(row[v]-mx);
        loss -= (row[tgt]-mx) - logf(sum);
    }
    return loss / (float)seq;
}

void __taipan_transformer_free(void *tp) {
    TaipanTransformer *t = (TaipanTransformer*)tp;
    __taipan_embed_free(t->embed);
    for(int32_t i=0;i<t->n_layers;i++) __taipan_block_free(t->blocks[i]);
    __taipan_tensor_free(t->lm_head_W);
    __taipan_tensor_free(t->lm_head_b);
    free(t);
}

int32_t __taipan_transformer_param_count(void *tp) {
    TaipanTransformer *t = (TaipanTransformer*)tp;
    int32_t d = t->d_model, v = t->vocab_size, seq = t->embed->seq_len;
    int32_t total = v*d + seq*d; // embeddings
    for(int32_t i=0;i<t->n_layers;i++) {
        total += 4*d*d + 4*d;   // MHA weights+biases
        total += t->blocks[i]->ffn->d_ff*d*2 + t->blocks[i]->ffn->d_ff + d; // FFN
    }
    total += v*d + v; // lm head
    return total;
}

// ─────────────────────────────────────────────
//  std.autograd — Automatic Differentiation
//  Reverse-mode autodiff (like PyTorch autograd)
//  Wengert tape implementation
// ─────────────────────────────────────────────

#define AG_MAX_NODES  65536
#define AG_MAX_INPUTS 4

typedef enum {
    AG_LEAF,    // input / parameter
    AG_ADD,     AG_SUB,    AG_MUL,    AG_DIV,
    AG_NEG,     AG_EXP,    AG_LOG,    AG_SQRT,
    AG_POW,     AG_SIN,    AG_COS,    AG_TANH,
    AG_RELU,    AG_SIGMOID,AG_MATMUL, AG_SUM,
    AG_MEAN,    AG_RESHAPE
} AgOp;

typedef struct AgNode {
    float    val;              // forward value
    float    grad;             // accumulated gradient
    AgOp     op;               // operation that created this
    int32_t  inputs[AG_MAX_INPUTS]; // indices of input nodes
    int32_t  n_inputs;
    float    saved[AG_MAX_INPUTS];  // saved scalars for backward
    int32_t  requires_grad;
    int32_t  visited;          // for topo sort
} AgNode;

typedef struct {
    AgNode  nodes[AG_MAX_NODES];
    int32_t count;
    int32_t topo[AG_MAX_NODES]; // topological order
    int32_t topo_count;
} AgTape;

static AgTape *g_tape = NULL;

// ── Tape management ───────────────────────────
void __taipan_ag_init(void) {
    if (!g_tape) g_tape = calloc(1, sizeof(AgTape));
    memset(g_tape, 0, sizeof(AgTape));
}

void __taipan_ag_reset(void) {
    if (g_tape) memset(g_tape, 0, sizeof(AgTape));
}

static int32_t ag_new_node(float val, AgOp op, int32_t req_grad) {
    if (!g_tape || g_tape->count >= AG_MAX_NODES) return -1;
    int32_t id = g_tape->count++;
    AgNode *n  = &g_tape->nodes[id];
    n->val           = val;
    n->grad          = 0.0f;
    n->op            = op;
    n->n_inputs      = 0;
    n->requires_grad = req_grad;
    n->visited       = 0;
    return id;
}

// ── Create leaf (parameter / input) ──────────
int32_t __taipan_ag_leaf(float val, int32_t requires_grad) {
    return ag_new_node(val, AG_LEAF, requires_grad);
}

float __taipan_ag_val(int32_t id) {
    if (!g_tape || id < 0 || id >= g_tape->count) return 0.0f;
    return g_tape->nodes[id].val;
}

float __taipan_ag_grad(int32_t id) {
    if (!g_tape || id < 0 || id >= g_tape->count) return 0.0f;
    return g_tape->nodes[id].grad;
}

void __taipan_ag_zero_grad(void) {
    if (!g_tape) return;
    for (int32_t i = 0; i < g_tape->count; i++)
        g_tape->nodes[i].grad = 0.0f;
}

// ── Forward ops ───────────────────────────────
#define AG_BINOP(name, OP, val_expr) \
int32_t __taipan_ag_##name(int32_t a, int32_t b) { \
    if (!g_tape) return -1; \
    float av = g_tape->nodes[a].val, bv = g_tape->nodes[b].val; \
    int32_t id = ag_new_node(val_expr, OP, \
        g_tape->nodes[a].requires_grad || g_tape->nodes[b].requires_grad); \
    g_tape->nodes[id].inputs[0] = a; \
    g_tape->nodes[id].inputs[1] = b; \
    g_tape->nodes[id].n_inputs  = 2; \
    return id; \
}

AG_BINOP(add, AG_ADD, av + bv)
AG_BINOP(sub, AG_SUB, av - bv)
AG_BINOP(mul, AG_MUL, av * bv)
AG_BINOP(div, AG_DIV, bv != 0.0f ? av/bv : 0.0f)

int32_t __taipan_ag_pow(int32_t a, float exp) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(powf(av, exp), AG_POW, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    g_tape->nodes[id].saved[0]  = exp;
    return id;
}

int32_t __taipan_ag_neg(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(-av, AG_NEG, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    return id;
}

int32_t __taipan_ag_exp(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    float ev = expf(av);
    int32_t id = ag_new_node(ev, AG_EXP, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    g_tape->nodes[id].saved[0]  = ev; // save exp(x) for backward
    return id;
}

int32_t __taipan_ag_log(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(logf(av + 1e-10f), AG_LOG, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    return id;
}

int32_t __taipan_ag_sqrt_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    float sv = sqrtf(av + 1e-10f);
    int32_t id = ag_new_node(sv, AG_SQRT, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    g_tape->nodes[id].saved[0]  = sv;
    return id;
}

int32_t __taipan_ag_sin_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(sinf(av), AG_SIN, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    return id;
}

int32_t __taipan_ag_cos_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(cosf(av), AG_COS, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    return id;
}

int32_t __taipan_ag_tanh_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    float tv = tanhf(av);
    int32_t id = ag_new_node(tv, AG_TANH, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    g_tape->nodes[id].saved[0]  = tv;
    return id;
}

int32_t __taipan_ag_relu_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    int32_t id = ag_new_node(av > 0.0f ? av : 0.0f, AG_RELU, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    return id;
}

int32_t __taipan_ag_sigmoid_op(int32_t a) {
    if (!g_tape) return -1;
    float av = g_tape->nodes[a].val;
    float sv = 1.0f / (1.0f + expf(-av));
    int32_t id = ag_new_node(sv, AG_SIGMOID, g_tape->nodes[a].requires_grad);
    g_tape->nodes[id].inputs[0] = a;
    g_tape->nodes[id].n_inputs  = 1;
    g_tape->nodes[id].saved[0]  = sv;
    return id;
}

// MSE loss: sum((pred - target)^2) / n
int32_t __taipan_ag_mse(int32_t pred, int32_t target) {
    if (!g_tape) return -1;
    float pv = g_tape->nodes[pred].val;
    float tv = g_tape->nodes[target].val;
    float d  = pv - tv;
    int32_t id = ag_new_node(d*d, AG_MUL,
        g_tape->nodes[pred].requires_grad);
    g_tape->nodes[id].inputs[0] = pred;
    g_tape->nodes[id].inputs[1] = target;
    g_tape->nodes[id].n_inputs  = 2;
    g_tape->nodes[id].saved[0]  = d; // save diff
    return id;
}

// ── Topological sort ──────────────────────────
static void ag_topo_visit(int32_t id) {
    if (!g_tape || id < 0) return;
    AgNode *n = &g_tape->nodes[id];
    if (n->visited) return;
    n->visited = 1;
    for (int32_t i = 0; i < n->n_inputs; i++)
        ag_topo_visit(n->inputs[i]);
    g_tape->topo[g_tape->topo_count++] = id;
}

// ── Backward pass ─────────────────────────────
void __taipan_ag_backward(int32_t loss_id) {
    if (!g_tape || loss_id < 0) return;
    // Reset topo
    g_tape->topo_count = 0;
    for (int32_t i = 0; i < g_tape->count; i++)
        g_tape->nodes[i].visited = 0;
    // Build topo order
    ag_topo_visit(loss_id);
    // Seed gradient
    g_tape->nodes[loss_id].grad = 1.0f;
    // Reverse pass
    for (int32_t ti = g_tape->topo_count - 1; ti >= 0; ti--) {
        int32_t id = g_tape->topo[ti];
        AgNode *n  = &g_tape->nodes[id];
        if (!n->requires_grad) continue;
        float g = n->grad;
        int32_t a = n->inputs[0];
        int32_t b = n->inputs[1];
        float av = (a >= 0) ? g_tape->nodes[a].val : 0.0f;
        float bv = (b >= 0) ? g_tape->nodes[b].val : 0.0f;
        switch (n->op) {
            case AG_LEAF: break;
            case AG_ADD:
                if (a >= 0) g_tape->nodes[a].grad += g;
                if (b >= 0) g_tape->nodes[b].grad += g;
                break;
            case AG_SUB:
                if (a >= 0) g_tape->nodes[a].grad += g;
                if (b >= 0) g_tape->nodes[b].grad -= g;
                break;
            case AG_MUL:
                if (a >= 0) g_tape->nodes[a].grad += g * bv;
                if (b >= 0) g_tape->nodes[b].grad += g * av;
                break;
            case AG_DIV:
                if (a >= 0) g_tape->nodes[a].grad += g / bv;
                if (b >= 0) g_tape->nodes[b].grad -= g * av / (bv*bv);
                break;
            case AG_NEG:
                if (a >= 0) g_tape->nodes[a].grad -= g;
                break;
            case AG_EXP:
                if (a >= 0) g_tape->nodes[a].grad += g * n->saved[0];
                break;
            case AG_LOG:
                if (a >= 0) g_tape->nodes[a].grad += g / (av + 1e-10f);
                break;
            case AG_SQRT:
                if (a >= 0) g_tape->nodes[a].grad += g / (2.0f * n->saved[0] + 1e-10f);
                break;
            case AG_POW:
                if (a >= 0) g_tape->nodes[a].grad += g * n->saved[0] * powf(av, n->saved[0]-1.0f);
                break;
            case AG_SIN:
                if (a >= 0) g_tape->nodes[a].grad += g * cosf(av);
                break;
            case AG_COS:
                if (a >= 0) g_tape->nodes[a].grad -= g * sinf(av);
                break;
            case AG_TANH:
                if (a >= 0) g_tape->nodes[a].grad += g * (1.0f - n->saved[0]*n->saved[0]);
                break;
            case AG_RELU:
                if (a >= 0) g_tape->nodes[a].grad += g * (av > 0.0f ? 1.0f : 0.0f);
                break;
            case AG_SIGMOID:
                if (a >= 0) g_tape->nodes[a].grad += g * n->saved[0] * (1.0f - n->saved[0]);
                break;
            case AG_SUM: case AG_MEAN: case AG_RESHAPE: case AG_MATMUL:
                if (a >= 0) g_tape->nodes[a].grad += g;
                break;
        }
    }
}

// ── SGD update ────────────────────────────────
void __taipan_ag_sgd_step(int32_t *params, int32_t n_params, float lr) {
    if (!g_tape) return;
    for (int32_t i = 0; i < n_params; i++) {
        int32_t id = params[i];
        if (id < 0 || id >= g_tape->count) continue;
        g_tape->nodes[id].val -= lr * g_tape->nodes[id].grad;
    }
}

// Update a single parameter
void __taipan_ag_update(int32_t id, float lr) {
    if (!g_tape || id < 0 || id >= g_tape->count) return;
    g_tape->nodes[id].val -= lr * g_tape->nodes[id].grad;
}

// Print node info
void __taipan_ag_print(int32_t id) {
    if (!g_tape || id < 0 || id >= g_tape->count) return;
    AgNode *n = &g_tape->nodes[id];
    printf("ag[%d]: val=%.6f grad=%.6f op=%d\n", id, n->val, n->grad, (int)n->op);
}

// ── Tensor-level autograd ─────────────────────
// Create a tracked tensor (each element is an ag node)
void *__taipan_ag_tensor(void *tp) {
    TaipanTensor *t = (TaipanTensor*)tp;
    if (!g_tape || !t) return NULL;
    // Store node ids as float (cast to int when using)
    TaipanTensor *ids = __taipan_tensor_1d(t->size);
    for (int32_t i = 0; i < t->size; i++)
        ids->data[i] = (float)__taipan_ag_leaf(t->data[i], 1);
    return (void*)ids;
}

// Element-wise add two id tensors
void *__taipan_ag_tadd(void *ap, void *bp) {
    TaipanTensor *a = (TaipanTensor*)ap;
    TaipanTensor *b = (TaipanTensor*)bp;
    if (!a || !b || a->size != b->size) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++)
        out->data[i] = (float)__taipan_ag_add((int32_t)a->data[i], (int32_t)b->data[i]);
    return (void*)out;
}

void *__taipan_ag_tmul(void *ap, void *bp) {
    TaipanTensor *a = (TaipanTensor*)ap;
    TaipanTensor *b = (TaipanTensor*)bp;
    if (!a || !b || a->size != b->size) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(a->size);
    for (int32_t i = 0; i < a->size; i++)
        out->data[i] = (float)__taipan_ag_mul((int32_t)a->data[i], (int32_t)b->data[i]);
    return (void*)out;
}

// Sum all ids into one loss node
int32_t __taipan_ag_tsum(void *ids_p) {
    TaipanTensor *ids = (TaipanTensor*)ids_p;
    if (!ids || ids->size == 0) return -1;
    int32_t acc = (int32_t)ids->data[0];
    for (int32_t i = 1; i < ids->size; i++)
        acc = __taipan_ag_add(acc, (int32_t)ids->data[i]);
    return acc;
}

// Extract values from id tensor into a regular tensor
void *__taipan_ag_tvals(void *ids_p) {
    TaipanTensor *ids = (TaipanTensor*)ids_p;
    if (!ids) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(ids->size);
    for (int32_t i = 0; i < ids->size; i++)
        out->data[i] = __taipan_ag_val((int32_t)ids->data[i]);
    return (void*)out;
}

// Extract grads from id tensor into a regular tensor
void *__taipan_ag_tgrads(void *ids_p) {
    TaipanTensor *ids = (TaipanTensor*)ids_p;
    if (!ids) return NULL;
    TaipanTensor *out = __taipan_tensor_1d(ids->size);
    for (int32_t i = 0; i < ids->size; i++)
        out->data[i] = __taipan_ag_grad((int32_t)ids->data[i]);
    return (void*)out;
}

// ─────────────────────────────────────────────
//  std.nn extras — Conv2D, Dropout, BatchNorm
//  std.optim — RMSProp, LR Scheduler
//  std.linalg — SVD, inverse, determinant
// ─────────────────────────────────────────────

// ── Dropout ───────────────────────────────────
void *__taipan_nn_dropout(void *xp, float rate, int32_t training) {
    TaipanTensor *x = (TaipanTensor*)xp;
    if (!x) return NULL;
    TaipanTensor *out = __taipan_tensor_copy(x);
    if (!training || rate <= 0.0f) return (void*)out;
    float scale = 1.0f / (1.0f - rate);
    for (int32_t i = 0; i < out->size; i++) {
        float r = (float)rand() / (float)RAND_MAX;
        out->data[i] = (r > rate) ? out->data[i] * scale : 0.0f;
    }
    return (void*)out;
}

// ── BatchNorm ─────────────────────────────────
typedef struct {
    TaipanTensor *gamma;   // scale  [features]
    TaipanTensor *beta;    // shift  [features]
    TaipanTensor *running_mean;
    TaipanTensor *running_var;
    float         momentum;
    float         eps;
    int32_t       features;
} TaipanBatchNorm;

void *__taipan_nn_batchnorm_new(int32_t features) {
    TaipanBatchNorm *bn = malloc(sizeof(TaipanBatchNorm));
    bn->features      = features;
    bn->momentum      = 0.1f;
    bn->eps           = 1e-5f;
    bn->gamma         = __taipan_tensor_1d(features);
    bn->beta          = __taipan_tensor_1d(features);
    bn->running_mean  = __taipan_tensor_1d(features);
    bn->running_var   = __taipan_tensor_1d(features);
    __taipan_tensor_ones(bn->gamma);
    __taipan_tensor_zeros(bn->beta);
    __taipan_tensor_zeros(bn->running_mean);
    __taipan_tensor_ones(bn->running_var);
    return (void*)bn;
}

// Forward: x[batch x features] -> normalized
void *__taipan_nn_batchnorm_forward(void *bnp, void *xp, int32_t training) {
    TaipanBatchNorm *bn = (TaipanBatchNorm*)bnp;
    TaipanTensor    *x  = (TaipanTensor*)xp;
    int32_t batch = (x->ndim == 2) ? x->shape[0] : 1;
    int32_t feat  = (x->ndim == 2) ? x->shape[1] : x->size;
    int32_t s2[2] = {batch, feat};
    TaipanTensor *out = __taipan_tensor_new(s2, 2);
    for (int32_t f = 0; f < feat; f++) {
        float mean = 0.0f, var = 0.0f;
        if (training) {
            for (int32_t i = 0; i < batch; i++) mean += x->data[i*feat+f];
            mean /= (float)batch;
            for (int32_t i = 0; i < batch; i++) { float d=x->data[i*feat+f]-mean; var+=d*d; }
            var /= (float)batch;
            // Update running stats
            bn->running_mean->data[f] = (1-bn->momentum)*bn->running_mean->data[f] + bn->momentum*mean;
            bn->running_var->data[f]  = (1-bn->momentum)*bn->running_var->data[f]  + bn->momentum*var;
        } else {
            mean = bn->running_mean->data[f];
            var  = bn->running_var->data[f];
        }
        float std_inv = 1.0f / sqrtf(var + bn->eps);
        for (int32_t i = 0; i < batch; i++)
            out->data[i*feat+f] = bn->gamma->data[f] * (x->data[i*feat+f]-mean)*std_inv + bn->beta->data[f];
    }
    return (void*)out;
}

void __taipan_nn_batchnorm_free(void *bnp) {
    TaipanBatchNorm *bn = (TaipanBatchNorm*)bnp;
    __taipan_tensor_free(bn->gamma);
    __taipan_tensor_free(bn->beta);
    __taipan_tensor_free(bn->running_mean);
    __taipan_tensor_free(bn->running_var);
    free(bn);
}

// ── Conv2D ────────────────────────────────────
typedef struct {
    TaipanTensor *W;   // [out_ch x in_ch x kH x kW]
    TaipanTensor *b;   // [out_ch]
    int32_t out_ch, in_ch, kH, kW, stride, padding;
} TaipanConv2D;

void *__taipan_nn_conv2d_new(int32_t in_ch, int32_t out_ch, int32_t kernel, int32_t stride, int32_t padding) {
    TaipanConv2D *c = malloc(sizeof(TaipanConv2D));
    c->out_ch = out_ch; c->in_ch = in_ch;
    c->kH = kernel; c->kW = kernel;
    c->stride = stride; c->padding = padding;
    int32_t w_size = out_ch * in_ch * kernel * kernel;
    int32_t ws[1] = {w_size};
    c->W = __taipan_tensor_new(ws, 1);
    c->b = __taipan_tensor_1d(out_ch);
    float limit = sqrtf(2.0f / (float)(in_ch * kernel * kernel));
    for (int32_t i = 0; i < w_size; i++)
        c->W->data[i] = ((float)rand()/(float)RAND_MAX)*2.0f*limit - limit;
    __taipan_tensor_zeros(c->b);
    return (void*)c;
}

// Forward: input[in_ch x H x W] -> output[out_ch x H_out x W_out]
void *__taipan_nn_conv2d_forward(void *cp, void *xp) {
    TaipanConv2D *c = (TaipanConv2D*)cp;
    TaipanTensor *x = (TaipanTensor*)xp;
    // x assumed flat: in_ch * H * W
    int32_t total  = x->size;
    int32_t hw     = total / c->in_ch;
    int32_t H      = (int32_t)sqrtf((float)hw);
    int32_t W      = H;
    int32_t H_out  = (H + 2*c->padding - c->kH) / c->stride + 1;
    int32_t W_out  = (W + 2*c->padding - c->kW) / c->stride + 1;
    int32_t out_sz = c->out_ch * H_out * W_out;
    int32_t os[1]  = {out_sz};
    TaipanTensor *out = __taipan_tensor_new(os, 1);
    for (int32_t oc = 0; oc < c->out_ch; oc++)
        for (int32_t oh = 0; oh < H_out; oh++)
            for (int32_t ow = 0; ow < W_out; ow++) {
                float sum = c->b->data[oc];
                for (int32_t ic = 0; ic < c->in_ch; ic++)
                    for (int32_t kh = 0; kh < c->kH; kh++)
                        for (int32_t kw = 0; kw < c->kW; kw++) {
                            int32_t ih = oh*c->stride - c->padding + kh;
                            int32_t iw = ow*c->stride - c->padding + kw;
                            if (ih>=0&&ih<H&&iw>=0&&iw<W)
                                sum += x->data[ic*H*W+ih*W+iw] *
                                       c->W->data[oc*c->in_ch*c->kH*c->kW + ic*c->kH*c->kW + kh*c->kW + kw];
                        }
                out->data[oc*H_out*W_out + oh*W_out + ow] = sum;
            }
    return (void*)out;
}

void __taipan_nn_conv2d_free(void *cp) {
    TaipanConv2D *c = (TaipanConv2D*)cp;
    __taipan_tensor_free(c->W);
    __taipan_tensor_free(c->b);
    free(c);
}

// ── Trainable Embedding ───────────────────────
typedef struct {
    TaipanTensor *W;   // [vocab x d_model]
    TaipanTensor *dW;  // gradients
    int32_t vocab, d_model;
} TaipanNNEmbed;

void *__taipan_nn_embed_new(int32_t vocab, int32_t d_model) {
    TaipanNNEmbed *e = malloc(sizeof(TaipanNNEmbed));
    e->vocab = vocab; e->d_model = d_model;
    e->W  = __taipan_tensor_2d(vocab, d_model);
    e->dW = __taipan_tensor_2d(vocab, d_model);
    float limit = sqrtf(1.0f/(float)d_model);
    for(int32_t i=0;i<e->W->size;i++)
        e->W->data[i]=((float)rand()/(float)RAND_MAX)*2.0f*limit-limit;
    __taipan_tensor_zeros(e->dW);
    return (void*)e;
}

void *__taipan_nn_embed_forward(void *ep, void *ids_p) {
    TaipanNNEmbed *e  = (TaipanNNEmbed*)ep;
    TaipanTensor  *ids = (TaipanTensor*)ids_p;
    int32_t seq = ids->size;
    int32_t s2[2] = {seq, e->d_model};
    TaipanTensor *out = __taipan_tensor_new(s2, 2);
    for(int32_t i=0;i<seq;i++){
        int32_t tid=(int32_t)ids->data[i];
        if(tid<0||tid>=e->vocab) tid=0;
        memcpy(out->data+i*e->d_model, e->W->data+tid*e->d_model, sizeof(float)*(size_t)e->d_model);
    }
    return (void*)out;
}

void __taipan_nn_embed_free(void *ep) {
    TaipanNNEmbed *e=(TaipanNNEmbed*)ep;
    __taipan_tensor_free(e->W);
    __taipan_tensor_free(e->dW);
    free(e);
}

// ── RMSProp optimizer ─────────────────────────
typedef struct {
    TaipanTensor *vW, *vb;
    float alpha, eps;
} TaipanRMSProp;

void *__taipan_nn_rmsprop_new(void *lp) {
    TaipanLinear *l = (TaipanLinear*)lp;
    TaipanRMSProp *r = calloc(1, sizeof(TaipanRMSProp));
    r->vW    = __taipan_tensor_2d(l->out_features, l->in_features);
    r->vb    = __taipan_tensor_1d(l->out_features);
    r->alpha = 0.99f;
    r->eps   = 1e-8f;
    return (void*)r;
}

void __taipan_nn_rmsprop_step(void *lp, void *rp, float lr) {
    TaipanLinear  *l = (TaipanLinear*)lp;
    TaipanRMSProp *r = (TaipanRMSProp*)rp;
    for(int32_t i=0;i<l->W->size;i++){
        float g=l->dW->data[i];
        r->vW->data[i]=r->alpha*r->vW->data[i]+(1-r->alpha)*g*g;
        l->W->data[i]-=lr*g/(sqrtf(r->vW->data[i])+r->eps);
    }
    for(int32_t i=0;i<l->b->size;i++){
        float g=l->db->data[i];
        r->vb->data[i]=r->alpha*r->vb->data[i]+(1-r->alpha)*g*g;
        l->b->data[i]-=lr*g/(sqrtf(r->vb->data[i])+r->eps);
    }
}

void __taipan_nn_rmsprop_free(void *rp) {
    TaipanRMSProp *r=(TaipanRMSProp*)rp;
    __taipan_tensor_free(r->vW);
    __taipan_tensor_free(r->vb);
    free(r);
}

// ── LR Scheduler ─────────────────────────────
float __taipan_optim_step_decay(float lr, int32_t epoch, int32_t step_size, float gamma) {
    return lr * powf(gamma, (float)(epoch / step_size));
}
float __taipan_optim_cosine_decay(float lr, int32_t epoch, int32_t max_epochs) {
    return lr * 0.5f * (1.0f + cosf(3.14159265f * (float)epoch / (float)max_epochs));
}
float __taipan_optim_warmup(float lr, int32_t epoch, int32_t warmup_epochs) {
    if (epoch >= warmup_epochs) return lr;
    return lr * (float)epoch / (float)warmup_epochs;
}

// ── std.linalg ────────────────────────────────
// Matrix inverse (Gauss-Jordan)
void *__taipan_linalg_inv(void *ap) {
    TaipanTensor *A = (TaipanTensor*)ap;
    if (!A || A->ndim != 2 || A->shape[0] != A->shape[1]) return NULL;
    int32_t n = A->shape[0];
    // Augmented matrix [A | I]
    float *aug = malloc(sizeof(float) * (size_t)(n * 2 * n));
    for(int32_t i=0;i<n;i++){
        for(int32_t j=0;j<n;j++) aug[i*2*n+j]=A->data[i*n+j];
        for(int32_t j=0;j<n;j++) aug[i*2*n+n+j]=(i==j)?1.0f:0.0f;
    }
    for(int32_t col=0;col<n;col++){
        // Pivot
        int32_t pivot=col;
        for(int32_t row=col+1;row<n;row++)
            if(fabsf(aug[row*2*n+col])>fabsf(aug[pivot*2*n+col])) pivot=row;
        if(pivot!=col)
            for(int32_t j=0;j<2*n;j++){float tmp=aug[col*2*n+j];aug[col*2*n+j]=aug[pivot*2*n+j];aug[pivot*2*n+j]=tmp;}
        float diag=aug[col*2*n+col];
        if(fabsf(diag)<1e-10f){free(aug);return NULL;}
        for(int32_t j=0;j<2*n;j++) aug[col*2*n+j]/=diag;
        for(int32_t row=0;row<n;row++){
            if(row==col) continue;
            float f=aug[row*2*n+col];
            for(int32_t j=0;j<2*n;j++) aug[row*2*n+j]-=f*aug[col*2*n+j];
        }
    }
    int32_t s2[2]={n,n};
    TaipanTensor *inv=__taipan_tensor_new(s2,2);
    for(int32_t i=0;i<n;i++)
        for(int32_t j=0;j<n;j++) inv->data[i*n+j]=aug[i*2*n+n+j];
    free(aug);
    return (void*)inv;
}

// Determinant (LU decomposition)
float __taipan_linalg_det(void *ap) {
    TaipanTensor *A=(TaipanTensor*)ap;
    if(!A||A->ndim!=2||A->shape[0]!=A->shape[1]) return 0.0f;
    int32_t n=A->shape[0];
    float *lu=malloc(sizeof(float)*(size_t)(n*n));
    memcpy(lu,A->data,sizeof(float)*(size_t)(n*n));
    float det=1.0f;
    for(int32_t i=0;i<n;i++){
        int32_t pivot=i;
        for(int32_t k=i+1;k<n;k++) if(fabsf(lu[k*n+i])>fabsf(lu[pivot*n+i])) pivot=k;
        if(pivot!=i){
            for(int32_t j=0;j<n;j++){float t=lu[i*n+j];lu[i*n+j]=lu[pivot*n+j];lu[pivot*n+j]=t;}
            det=-det;
        }
        if(fabsf(lu[i*n+i])<1e-10f){free(lu);return 0.0f;}
        det*=lu[i*n+i];
        for(int32_t k=i+1;k<n;k++){
            float f=lu[k*n+i]/lu[i*n+i];
            for(int32_t j=i;j<n;j++) lu[k*n+j]-=f*lu[i*n+j];
        }
    }
    free(lu);
    return det;
}

// Matrix norm (Frobenius)
float __taipan_linalg_norm(void *ap) {
    TaipanTensor *A=(TaipanTensor*)ap;
    if(!A) return 0.0f;
    float s=0.0f;
    for(int32_t i=0;i<A->size;i++) s+=A->data[i]*A->data[i];
    return sqrtf(s);
}

// Outer product: a[m] x b[n] -> C[m x n]
void *__taipan_linalg_outer(void *ap, void *bp) {
    TaipanTensor *a=(TaipanTensor*)ap, *b=(TaipanTensor*)bp;
    if(!a||!b) return NULL;
    int32_t s2[2]={a->size,b->size};
    TaipanTensor *C=__taipan_tensor_new(s2,2);
    for(int32_t i=0;i<a->size;i++)
        for(int32_t j=0;j<b->size;j++)
            C->data[i*b->size+j]=a->data[i]*b->data[j];
    return (void*)C;
}

// Trace of matrix
float __taipan_linalg_trace(void *ap) {
    TaipanTensor *A=(TaipanTensor*)ap;
    if(!A||A->ndim!=2) return 0.0f;
    int32_t n=A->shape[0]<A->shape[1]?A->shape[0]:A->shape[1];
    float s=0.0f;
    for(int32_t i=0;i<n;i++) s+=A->data[i*A->shape[1]+i];
    return s;
}

// Cosine similarity
float __taipan_linalg_cosine(void *ap, void *bp) {
    TaipanTensor *a=(TaipanTensor*)ap, *b=(TaipanTensor*)bp;
    if(!a||!b||a->size!=b->size) return 0.0f;
    float dot=0.0f, na=0.0f, nb=0.0f;
    for(int32_t i=0;i<a->size;i++){
        dot+=a->data[i]*b->data[i];
        na+=a->data[i]*a->data[i];
        nb+=b->data[i]*b->data[i];
    }
    return dot/(sqrtf(na)*sqrtf(nb)+1e-10f);
}

// Linear solve Ax = b via Gaussian elimination
void *__taipan_linalg_solve(void *Ap, void *bp) {
    TaipanTensor *A=(TaipanTensor*)Ap, *b=(TaipanTensor*)bp;
    if(!A||!b||A->ndim!=2||A->shape[0]!=A->shape[1]) return NULL;
    int32_t n=A->shape[0];
    float *aug=malloc(sizeof(float)*(size_t)(n*(n+1)));
    for(int32_t i=0;i<n;i++){
        for(int32_t j=0;j<n;j++) aug[i*(n+1)+j]=A->data[i*n+j];
        aug[i*(n+1)+n]=b->data[i];
    }
    for(int32_t col=0;col<n;col++){
        int32_t pivot=col;
        for(int32_t row=col+1;row<n;row++)
            if(fabsf(aug[row*(n+1)+col])>fabsf(aug[pivot*(n+1)+col])) pivot=row;
        if(pivot!=col)
            for(int32_t j=0;j<=n;j++){float t=aug[col*(n+1)+j];aug[col*(n+1)+j]=aug[pivot*(n+1)+j];aug[pivot*(n+1)+j]=t;}
        float d=aug[col*(n+1)+col];
        if(fabsf(d)<1e-10f){free(aug);return NULL;}
        for(int32_t j=0;j<=n;j++) aug[col*(n+1)+j]/=d;
        for(int32_t row=0;row<n;row++){
            if(row==col) continue;
            float f=aug[row*(n+1)+col];
            for(int32_t j=0;j<=n;j++) aug[row*(n+1)+j]-=f*aug[col*(n+1)+j];
        }
    }
    TaipanTensor *x=__taipan_tensor_1d(n);
    for(int32_t i=0;i<n;i++) x->data[i]=aug[i*(n+1)+n];
    free(aug);
    return (void*)x;
}

// ── load_images (PPM format) ──────────────────
// Loads a PPM image as a flat float tensor [H*W*3] normalized to [0,1]
void *__taipan_data_load_ppm(const char *path) {
    FILE *f=fopen(path,"rb");
    if(!f) return NULL;
    char magic[3]; int W,H,maxval;
    fscanf(f,"%2s %d %d %d ",magic,&W,&H,&maxval);
    if(magic[0]!='P'||magic[1]!='6'){fclose(f);return NULL;}
    int32_t size=H*W*3;
    int32_t s[1]={size};
    TaipanTensor *t=__taipan_tensor_new(s,1);
    uint8_t *buf=malloc((size_t)(W*H*3));
    fread(buf,1,(size_t)(W*H*3),f);
    for(int32_t i=0;i<size;i++) t->data[i]=(float)buf[i]/(float)maxval;
    free(buf); fclose(f);
    return (void*)t;
}

// Save tensor as PPM image
int32_t __taipan_data_save_ppm(void *tp, const char *path, int32_t W, int32_t H) {
    TaipanTensor *t=(TaipanTensor*)tp;
    if(!t) return -1;
    FILE *f=fopen(path,"wb");
    if(!f) return -1;
    fprintf(f,"P6\n%d %d\n255\n",W,H);
    for(int32_t i=0;i<W*H*3;i++){
        float v=t->data[i]; if(v<0)v=0; if(v>1)v=1;
        uint8_t b=(uint8_t)(v*255.0f);
        fwrite(&b,1,1,f);
    }
    fclose(f);
    return 0;
}
