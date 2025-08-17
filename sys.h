#pragma once

typedef long i64;
typedef unsigned short u16;
typedef unsigned int u32;

// ============================================================================
// System call definitions (unchanged)
// ============================================================================
#if defined(__x86_64__)
static inline i64 sys(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 r10 __asm__("r10") = d;
  register i64 r8  __asm__("r8")  = e;
  register i64 r9  __asm__("r9")  = f;
  i64 rax;
  __asm__ volatile("syscall"
    : "=a"(rax)
    : "a"(n), "D"(a), "S"(b), "d"(c), "r"(r10), "r"(r8), "r"(r9)
    : "rcx","r11","memory");
  return rax;
}
#  define SYS_read 0
#  define SYS_write 1
#  define SYS_close 3
#  define SYS_poll 7
#  define SYS_socket 41
#  define SYS_bind 49
#  define SYS_listen 50
#  define SYS_accept 43
#  define SYS_setsockopt 54
#  define SYS_exit 60
#elif defined(__aarch64__)
static inline i64 sys(i64 n,i64 a,i64 b,i64 c,i64 d,i64 e,i64 f){
  register i64 x8 __asm__("x8") = n;
  register i64 x0 __asm__("x0") = a;
  register i64 x1 __asm__("x1") = b;
  register i64 x2 __asm__("x2") = c;
  register i64 x3 __asm__("x3") = d;
  register i64 x4 __asm__("x4") = e;
  register i64 x5 __asm__("x5") = f;
  __asm__ volatile("svc 0" : "+r"(x0)
                   : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                   : "memory");
  return x0;
}
#  define SYS_close 57
#  define SYS_read 63
#  define SYS_write 64
#  define SYS_ppoll 73
#  define SYS_socket 198
#  define SYS_bind 200
#  define SYS_listen 201
#  define SYS_accept 202
#  define SYS_setsockopt 208
#  define SYS_exit 93
#else
#  error "Unsupported arch"
#endif

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define POLLIN 0x001

// ============================================================================
// Network structures
// ============================================================================
struct pollfd {
  int fd;
  short events;
  short revents;
};

struct timespec {
  i64 tv_sec;
  i64 tv_nsec;
};

struct in_addr{ u32 s_addr; };
struct sockaddr_in{
  u16 sin_family; u16 sin_port; struct in_addr sin_addr; unsigned char sin_zero[8];
};
