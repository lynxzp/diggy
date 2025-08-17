#include "lyrics.h"
#include "sys.h"
#include "common.h"

// ============================================================================
// Route handling system
// ============================================================================
#define MAX_RESPONSE_SIZE 8192

typedef struct {
  const char* path;
  const char* content;
  const char* content_type;
  int path_len;  // Will be calculated at runtime
  int content_type_len;  // Pre-calculated content type length
} Route;

// Example static content (add more as needed)
static const char health_content[] = "OK";
static const char about_content[] = "This is a simple HTTP server";
static const char info_content[] = "Server v1.0.0\nMinimal HTTP implementation";

// ============================================================================
// Utility function to calculate string length
// ============================================================================
static int string_len(const char* str){
  volatile int len = 0; // Use volatile to prevent optimization and use strlen from stdlib, which not available
  while(str[len]) len++;
  return len;
}

// ============================================================================
// ROUTE TABLE - Add new routes here!
// Path lengths will be calculated at initialization
// ============================================================================
static Route routes[] = {
  {"/",  content, "text/plain; charset=utf-8"},
  {"/health", health_content,  "text/plain"},
  {"/about",  about_content,  "text/plain"},
  {"/info", info_content,  "text/plain"},
};
#define NUM_ROUTES (sizeof(routes) / sizeof(routes[0]))

// ============================================================================
// Initialize route path lengths
// ============================================================================
static void init_routes(void){
  int i;
  for(i = 0; i < NUM_ROUTES; i++){
    routes[i].path_len = string_len(routes[i].path);
    routes[i].content_type_len = string_len(routes[i].content_type);
  }
}

// Build HTTP response for a route
static int build_response(const Route* route, char* buf, int buf_size){
  static const char h1[] = "HTTP/1.1 200 OK\r\nContent-Length: ";
  static const char h2[] = "\r\nConnection: close\r\nContent-Type: ";
  static const char h3[] = "\r\n\r\n";

  int content_len = 0;
  const volatile char* p = (const volatile char*)route->content;
  while(p[content_len]) content_len++;

  char len_str[12];
  int len_digits = itoa(content_len, len_str);

  int pos = 0;

  // Check buffer size
  int needed = (sizeof(h1) - 1) + len_digits + (sizeof(h2) - 1) +
               route->content_type_len + (sizeof(h3) - 1) + content_len;
  if(needed > buf_size) return -1;

  // Build response
  memcpy_manual(buf + pos, h1, sizeof(h1) - 1);
  pos += sizeof(h1) - 1;

  memcpy_manual(buf + pos, len_str, len_digits);
  pos += len_digits;

  memcpy_manual(buf + pos, h2, sizeof(h2) - 1);
  pos += sizeof(h2) - 1;

  memcpy_manual(buf + pos, route->content_type, route->content_type_len);
  pos += route->content_type_len;

  memcpy_manual(buf + pos, h3, sizeof(h3) - 1);
  pos += sizeof(h3) - 1;

  memcpy_manual(buf + pos, route->content, content_len);
  pos += content_len;

  return pos;
}

// Build 404 response
static int build_404_response(char* buf, int buf_size){
  static const char resp[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Length: 9\r\n"
    "Connection: close\r\n"
    "Content-Type: text/plain\r\n"
    "\r\nNot Found";

  int len = sizeof(resp) - 1;
  if(len > buf_size) return -1;

  memcpy_manual(buf, resp, len);
  return len;
}

// Extract path from HTTP request
static int extract_path(const char* req, int req_len, const char** path_start, int* path_len){
  int i = 0;

  // Skip method
  while(i < req_len && req[i] != ' ') i++;
  while(i < req_len && req[i] == ' ') i++;

  if(i >= req_len || req[i] != '/') return 0;

  *path_start = &req[i];
  int start = i;

  // Find end of path
  while(i < req_len && req[i] != ' ' && req[i] != '?') i++;

  *path_len = i - start;
  return 1;
}

// Compare two strings
static int compare_strings(const char* s1, const char* s2, int len){
  int i;
  for(i = 0; i < len; i++){
    if(s1[i] != s2[i]) return 0;
  }
  return 1;
}

// Find matching route
static const Route* find_route(const char* path, int path_len){
  int i;
  for(i = 0; i < NUM_ROUTES; i++){
    if(routes[i].path_len == path_len &&
       compare_strings(routes[i].path, path, path_len)){
      return &routes[i];
    }
  }
  return 0;  // Not found
}

// Handle HTTP request
static void handle_request(int client_fd){
  char req_buf[512];
  char resp_buf[MAX_RESPONSE_SIZE];

  // Read request
  int req_len = (int)sys(SYS_read, client_fd, (i64)req_buf, sizeof(req_buf), 0, 0, 0);
  if(req_len <= 0){
    sys(SYS_close, client_fd, 0, 0, 0, 0, 0);
    return;
  }

  // Extract path
  const char* path;
  int path_len;
  if(!extract_path(req_buf, req_len, &path, &path_len)){
    // Invalid request, send 404
    int resp_len = build_404_response(resp_buf, sizeof(resp_buf));
    if(resp_len > 0){
      sys(SYS_write, client_fd, (i64)resp_buf, resp_len, 0, 0, 0);
    }
    sys(SYS_close, client_fd, 0, 0, 0, 0, 0);
    return;
  }

  // Find route
  const Route* route = find_route(path, path_len);
  int resp_len;

  if(route){
    resp_len = build_response(route, resp_buf, sizeof(resp_buf));
  } else {
    resp_len = build_404_response(resp_buf, sizeof(resp_buf));
  }

  // Send response
  if(resp_len > 0){
    sys(SYS_write, client_fd, (i64)resp_buf, resp_len, 0, 0, 0);
  }

  sys(SYS_close, client_fd, 0, 0, 0, 0, 0);
}

// ============================================================================
// Line printing functionality
// ============================================================================
static int find_next_line(const char* str, int start, int total_len, int* line_start, int* line_len){
  if(start >= total_len) return 0;

  *line_start = start;
  int i = start;
  while(i < total_len && str[i] != '\n') i++;

  *line_len = i - start;
  return 1;
}

static void print_next_line(int* current_pos){
  // Calculate content length at runtime
  int content_len = 0;
  const volatile char* p = (const volatile char*)content;
  while(p[content_len]) content_len++;

  int line_start, line_len;

  if(find_next_line(content, *current_pos, content_len, &line_start, &line_len)){
    sys(SYS_write, 1, (i64)(content + line_start), line_len, 0, 0, 0);
    sys(SYS_write, 1, (i64)"\n", 1, 0, 0, 0);

    *current_pos = line_start + line_len;
    if(*current_pos < content_len && content[*current_pos] == '\n'){
      (*current_pos)++;
    }
  } else {
    *current_pos = 0;  // Start over
  }
}

// ============================================================================
// Main server
// ============================================================================
void _start(void){
  // Initialize route path lengths
  init_routes();

  // Print startup message
  static const char startup_msg[] = "starting diggy server on :8080\n";
  sys(SYS_write, 1, (i64)startup_msg, sizeof(startup_msg) - 1, 0, 0, 0);

  // Create and configure socket
  int sock = (int)sys(SYS_socket, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
  int one = 1;
  sys(SYS_setsockopt, sock, SOL_SOCKET, SO_REUSEADDR, (i64)&one, sizeof(one), 0);

  // Bind and listen
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = 0;  // INADDR_ANY

  sys(SYS_bind, sock, (i64)&addr, sizeof(addr), 0, 0, 0);
  sys(SYS_listen, sock, 128, 0, 0, 0, 0);

  // Line printing state
  int current_pos = 0;
  int elapsed_ms = 0;
  const int interval_ms = 2000;  // 2 seconds
  const int poll_timeout_ms = 100;  // Check every 100ms

  // Poll structure
  struct pollfd pfd;
  pfd.fd = sock;
  pfd.events = POLLIN;

  // Main server loop
  for(;;){
    // Poll with timeout
    struct timespec ts;
    ts.tv_sec = poll_timeout_ms / 1000;
    ts.tv_nsec = (poll_timeout_ms % 1000) * 1000000;

#if defined(__x86_64__)
    int ready = (int)sys(SYS_poll, (i64)&pfd, 1, poll_timeout_ms, 0, 0, 0);
#elif defined(__aarch64__)
    int ready = (int)sys(SYS_ppoll, (i64)&pfd, 1, (i64)&ts, 0, 0, 0);
#endif

    // Handle incoming connections
    if(ready > 0 && (pfd.revents & POLLIN)){
      int client = (int)sys(SYS_accept, sock, 0, 0, 0, 0, 0);
      if(client >= 0){
        handle_request(client);
      }
    }

    // Update timer and print lines
    elapsed_ms += poll_timeout_ms;
    if(elapsed_ms >= interval_ms){
      elapsed_ms = 0;
      print_next_line(&current_pos);
    }
  }
}