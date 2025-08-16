// main.c — tiny HTTP "OK" on :8080, x86_64 + aarch64
typedef long i64; typedef unsigned short u16; typedef unsigned int u32;

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
#  define SYS_socket 41
#  define SYS_setsockopt 54
#  define SYS_bind 49
#  define SYS_listen 50
#  define SYS_accept 43
#  define SYS_write 1
#  define SYS_close 3
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
#  define SYS_socket 198
#  define SYS_setsockopt 208
#  define SYS_bind 200
#  define SYS_listen 201
#  define SYS_accept 202
#  define SYS_write 64
#  define SYS_close 57
#  define SYS_exit 93
#else
#  error "Unsupported arch"
#endif

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 1
#define SO_REUSEADDR 2

struct in_addr{ u32 s_addr; };
struct sockaddr_in{
  u16 sin_family; u16 sin_port; struct in_addr sin_addr; unsigned char sin_zero[8];
};
static u16 htons(u16 x){ return (u16)((x<<8)|(x>>8)); }

void _start(void){
  int s=(int)sys(SYS_socket,AF_INET,SOCK_STREAM,0,0,0,0);
  int one=1; sys(SYS_setsockopt,s,SOL_SOCKET,SO_REUSEADDR,(i64)&one,sizeof(one),0);

  struct sockaddr_in a = {0};
  a.sin_family=AF_INET; a.sin_port=htons(8080); a.sin_addr.s_addr=0;
  sys(SYS_bind,s,(i64)&a,sizeof(a),0,0,0);
  sys(SYS_listen,s,128,0,0,0,0);

  static const char resp[]="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\nOK";
  for(;;){
    int c=(int)sys(SYS_accept,s,0,0,0,0,0); if(c<0) break;
    sys(SYS_write,c,(i64)resp,(i64)sizeof(resp)-1,0,0,0);
    sys(SYS_close,c,0,0,0,0,0);
  }
  sys(SYS_close,s,0,0,0,0,0);
  sys(SYS_exit,0,0,0,0,0,0);
}
