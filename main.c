#include "lyrics.h"
// main.c — tiny HTTP router
// "/" returns content form lyrics.h
// "/health" => 200, others => 404

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
#  define SYS_read 0
#  define SYS_write 1
#  define SYS_close 3
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

struct in_addr{ u32 s_addr; };
struct sockaddr_in{
  u16 sin_family; u16 sin_port; struct in_addr sin_addr; unsigned char sin_zero[8];
};
static u16 htons(u16 x){ return (u16)((x<<8)|(x>>8)); }

// Simple int to string conversion
static int itoa(int val, char* buf){
  int i = 0, j, tmp;
  char rev[12];
  if(val == 0){ buf[0] = '0'; return 1; }
  while(val > 0){
    rev[i++] = '0' + (val % 10);
    val /= 10;
  }
  for(j = 0; j < i; j++) buf[j] = rev[i-j-1];
  return i;
}

static int path_case(const char *buf, int n){
  // returns: 0="/", 1="/health", 2=other/unknown
  int i=0;
  // skip method token
  while(i<n && buf[i]!=' ') i++;
  while(i<n && buf[i]==' ') i++;
  if(i>=n || buf[i]!='/') return 2;
  int start=i;
  while(i<n && buf[i]!=' ') i++;
  int len=i-start;
  if(len==1) return 0; // "/"
  if(len==7){
    const char h[]="/health";
    for(int k=0;k<7;k++) if(start+k>=n || buf[start+k]!=h[k]) return 2;
    return 1;
  }
  return 2;
}

// Prevent compiler from optimizing to memcpy
static void* memcpy_manual(void* dst, const void* src, int n){
  char* d = (char*)dst;
  const char* s = (const char*)src;
  while(n--) *d++ = *s++;
  return dst;
}

void _start(void){
  int s=(int)sys(SYS_socket,AF_INET,SOCK_STREAM,0,0,0,0);
  int one=1; sys(SYS_setsockopt,s,SOL_SOCKET,SO_REUSEADDR,(i64)&one,sizeof(one),0);

  struct sockaddr_in a = {0};
  a.sin_family=AF_INET; a.sin_port=htons(8080); a.sin_addr.s_addr=0;
  sys(SYS_bind,s,(i64)&a,sizeof(a),0,0,0);
  sys(SYS_listen,s,128,0,0,0,0);

  static const char resp_health[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length: 2\r\n"
    "Connection: close\r\n"
    "Content-Type: text/plain\r\n"
    "\r\nOK";

  static const char resp_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 9\r\n"
    "Connection: close\r\n"
    "Content-Type: text/plain\r\n"
    "\r\nNot Found";

  // Static allocation for complete response
  static const char h1[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
  static const char h2[] = "\r\nConnection: close\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";

  // Use static buffer large enough for response
  static char resp_main[1024];
  char len_str[12];
  int content_len = sizeof(content) - 1;
  int len_digits = itoa(content_len, len_str);

  int pos = 0;
  // Copy headers
  memcpy_manual(resp_main + pos, h1, sizeof(h1) - 1);
  pos += sizeof(h1) - 1;
  memcpy_manual(resp_main + pos, len_str, len_digits);
  pos += len_digits;
  memcpy_manual(resp_main + pos, h2, sizeof(h2) - 1);
  pos += sizeof(h2) - 1;
  memcpy_manual(resp_main + pos, content, content_len);
  pos += content_len;

  for(;;){
    int c=(int)sys(SYS_accept,s,0,0,0,0,0); if(c<0) break;

    char buf[512];
    int n=(int)sys(SYS_read,c,(i64)buf,(i64)sizeof(buf),0,0,0);
    int which = (n>0) ? path_case(buf,n) : 2;

    if(which==0){
      // Main page with content
      sys(SYS_write,c,(i64)resp_main,(i64)pos,0,0,0);
    }else if(which==1){
      // Health check
      sys(SYS_write,c,(i64)resp_health,(i64)(sizeof(resp_health)-1),0,0,0);
    }else{
      // 404
      sys(SYS_write,c,(i64)resp_404,(i64)(sizeof(resp_404)-1),0,0,0);
    }

    sys(SYS_close,c,0,0,0,0,0);
  }
  sys(SYS_close,s,0,0,0,0,0);
  sys(SYS_exit,0,0,0,0,0,0);
}
