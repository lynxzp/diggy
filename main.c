#include "lyrics.h"
#include "sys.h"
#include "notstdlib.h"

// ============================================================================
// Add syscalls for file operations
// ============================================================================
#if defined(__x86_64__)
#  define SYS_open 2
#  define O_RDONLY 0
#elif defined(__aarch64__)
#  define SYS_openat 56
#  define AT_FDCWD -100
#  define O_RDONLY 0
#endif

// ============================================================================
// Configuration structure
// ============================================================================
typedef struct {
  int port;
  u32 host;  // IP address in network byte order
  int interval_ms;
  int poll_timeout_ms;
  int mine;
} Config;

// Default configuration
static Config config = {
  .port = 8080,
  .host = 0,  // INADDR_ANY
  .interval_ms = 2000,
  .poll_timeout_ms = 100,
  .mine = 1
};

// ============================================================================
// String to integer conversion
// ============================================================================
static int str_to_int(const char* str, int len){
  int result = 0;
  int i;
  for(i = 0; i < len; i++){
    if(str[i] < '0' || str[i] > '9') break;
    result = result * 10 + (str[i] - '0');
  }
  return result;
}

// ============================================================================
// Parse IP address (e.g., "0.0.0.0" or "127.0.0.1")
// ============================================================================
static u32 parse_ip(const char* str, int len){
  u32 result = 0;
  int octet = 0;
  int octet_count = 0;
  int i;

  for(i = 0; i <= len; i++){
    if(i == len || str[i] == '.'){
      if(octet_count >= 4) return 0;  // Too many octets
      result |= (octet & 0xFF) << (8 * octet_count);
      octet_count++;
      octet = 0;
    } else if(str[i] >= '0' && str[i] <= '9'){
      octet = octet * 10 + (str[i] - '0');
      if(octet > 255) return 0;  // Invalid octet
    } else {
      return 0;  // Invalid character
    }
  }

  return (octet_count == 4) ? result : 0;
}

// ============================================================================
// String comparison
// ============================================================================
static int str_equals(const char* s1, const char* s2, int len){
  int i;
  for(i = 0; i < len; i++){
    if(s1[i] != s2[i]) return 0;
  }
  return 1;
}


static void parse_config_line(const char* line, int len);

// ============================================================================
// Load configuration from file
// ============================================================================
static void load_config(const char* filename){
  char buffer[1024];
  int fd, bytes_read;

  // Open file
#if defined(__x86_64__)
  fd = (int)sys(SYS_open, (i64)filename, O_RDONLY, 0, 0, 0, 0);
#elif defined(__aarch64__)
  fd = (int)sys(SYS_openat, AT_FDCWD, (i64)filename, O_RDONLY, 0, 0, 0);
#endif

  if(fd < 0){
    // File not found or cannot open - use defaults
    static const char msg[] = "Config file not found, using defaults\n";
    sys(SYS_write, 1, (i64)msg, sizeof(msg) - 1, 0, 0, 0);
    return;
  }

  // Read file content
  bytes_read = (int)sys(SYS_read, fd, (i64)buffer, sizeof(buffer) - 1, 0, 0, 0);
  sys(SYS_close, fd, 0, 0, 0, 0, 0);

  if(bytes_read <= 0) return;

  // Parse line by line
  int line_start = 0;
  int i;
  for(i = 0; i <= bytes_read; i++){
    if(i == bytes_read || buffer[i] == '\n'){
      if(i > line_start){
        parse_config_line(buffer + line_start, i - line_start);
      }
      line_start = i + 1;
    }
  }

  // Print loaded configuration
  static const char msg1[] = "Config file loaded:\n";
  static const char msg2[] = "  port: ";
  static const char msg3[] = "\n  interval_ms: ";
  static const char msg4[] = "\n  poll_timeout_ms: ";
  static const char msg5[] = "\n  mine: ";
  static const char msg6[] = "\n";
  char num_buf[12];
  int num_len;

  sys(SYS_write, 1, (i64)msg1, sizeof(msg1) - 1, 0, 0, 0);

  sys(SYS_write, 1, (i64)msg2, sizeof(msg2) - 1, 0, 0, 0);
  num_len = itoa(config.port, num_buf);
  sys(SYS_write, 1, (i64)num_buf, num_len, 0, 0, 0);

  sys(SYS_write, 1, (i64)msg3, sizeof(msg3) - 1, 0, 0, 0);
  num_len = itoa(config.interval_ms, num_buf);
  sys(SYS_write, 1, (i64)num_buf, num_len, 0, 0, 0);

  sys(SYS_write, 1, (i64)msg4, sizeof(msg4) - 1, 0, 0, 0);
  num_len = itoa(config.poll_timeout_ms, num_buf);
  sys(SYS_write, 1, (i64)num_buf, num_len, 0, 0, 0);

  sys(SYS_write, 1, (i64)msg5, sizeof(msg5) - 1, 0, 0, 0);
  num_len = itoa(config.mine, num_buf);
  sys(SYS_write, 1, (i64)num_buf, num_len, 0, 0, 0);

  sys(SYS_write, 1, (i64)msg6, sizeof(msg6) - 1, 0, 0, 0);
}

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
// ROUTE TABLE - Add new routes here!
// Path lengths will be calculated at initialization
// ============================================================================
static const char content_type[] = "text/plain; charset=utf-8";
static Route routes[] = {
  {"/",  content, content_type},
  {"/health", health_content,  content_type},
  {"/about",  about_content,  content_type},
  {"/info", info_content,  content_type},
};
#define NUM_ROUTES (sizeof(routes) / sizeof(routes[0]))

// ============================================================================
// Initialize route path lengths
// ============================================================================
static void init_routes(void){
  int i;
  for(i = 0; i < NUM_ROUTES; i++){
    routes[i].path_len = str_len(routes[i].path);
    routes[i].content_type_len = str_len(routes[i].content_type);
  }
}
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
  // Only print if mining is enabled
  if(!config.mine) return;

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
// Environment overrides: read /proc/self/environ and apply DIGGY_* vars
// ============================================================================
static void parse_env_var(const char* kv, int len) {
  static int header_printed = 0;
  int eq_pos = -1;
  int i;
  for(i = 0; i < len; i++){
    if(kv[i] == '='){ eq_pos = i; break; }
  }
  if(eq_pos <= 0) return;
  if(eq_pos < 6) return; // need at least "DIGGY_"
  if(!compare_strings(kv, "DIGGY_", 6)) return;

  const char* key = kv + 6;
  int key_len = eq_pos - 6;
  const char* value = kv + eq_pos + 1;
  int value_len = len - eq_pos - 1;

  int applied = 0;
  if(key_len == 4 && str_equals(key, "PORT", 4)){
    config.port = str_to_int(value, value_len);
    applied = 1;
  } else if(key_len == 4 && str_equals(key, "HOST", 4)){
    u32 h = parse_ip(value, value_len);
    if(h) {
      config.host = h;
      applied = 1;
    }
  } else if(key_len == 11 && str_equals(key, "INTERVAL_MS", 11)){
    config.interval_ms = str_to_int(value, value_len);
    applied = 1;
  } else if(key_len == 15 && str_equals(key, "POLL_TIMEOUT_MS", 15)){
    config.poll_timeout_ms = str_to_int(value, value_len);
    applied = 1;
  } else if(key_len == 4 && str_equals(key, "MINE", 4)){
    config.mine = str_to_int(value, value_len);
    applied = 1;
  }

  // Print loaded env var in "KEY=VALUE" form once applied
  if(applied){
    if(!header_printed){
      static const char hdr[] = "Env overrides loaded:\n";
      sys(SYS_write, 1, (i64)hdr, sizeof(hdr) - 1, 0, 0, 0);
      header_printed = 1;
    }
    sys(SYS_write, 1, (i64)"  ", 2, 0, 0, 0);
    sys(SYS_write, 1, (i64)kv, len, 0, 0, 0);
    sys(SYS_write, 1, (i64)"\n", 1, 0, 0, 0);
  }
}

static void load_env_overrides(void){
  char buffer[4096];
  int fd, bytes_read;
#if defined(__x86_64__)
  fd = (int)sys(SYS_open, (i64)"/proc/self/environ", O_RDONLY, 0, 0, 0, 0);
#elif defined(__aarch64__)
  fd = (int)sys(SYS_openat, AT_FDCWD, (i64)"/proc/self/environ", O_RDONLY, 0, 0, 0);
#endif
  if(fd < 0) return;

  bytes_read = (int)sys(SYS_read, fd, (i64)buffer, sizeof(buffer), 0, 0, 0);
  sys(SYS_close, fd, 0, 0, 0, 0, 0);
  if(bytes_read <= 0) return;

  int pos = 0;
  while(pos < bytes_read){
      int start = pos;
      while(pos < bytes_read && buffer[pos] != '\0') pos++;
      if(pos > start){
          parse_env_var(buffer + start, pos - start);
        }
      pos++; // skip NUL
    }
}


// ============================================================================
// Parse configuration line (key=value format)
// ============================================================================
static int apply_config_kv(const char* key, int key_len, const char* value, int value_len){
  if(key_len == 4 && str_equals(key, "port", 4)){
    config.port = str_to_int(value, value_len);
    return 1;
  } else if(key_len == 4 && str_equals(key, "host", 4)){
    u32 h = parse_ip(value, value_len);
    if(h){ config.host = h; return 1; }
  } else if(key_len == 11 && str_equals(key, "interval_ms", 11)){
    config.interval_ms = str_to_int(value, value_len);
    return 1;
  } else if(key_len == 15 && str_equals(key, "poll_timeout_ms", 15)){
    config.poll_timeout_ms = str_to_int(value, value_len);
    return 1;
  } else if(key_len == 4 && str_equals(key, "mine", 4)){
    config.mine = str_to_int(value, value_len);
    return 1;
  }
  return 0;
}

// ============================================================================
// Parse configuration line (key=value format)
// ============================================================================
static void parse_config_line(const char* line, int len){
  int eq_pos = -1;
  int i;
  for(i = 0; i < len; i++){
    if(line[i] == '='){ eq_pos = i; break; }
  }
  if(eq_pos <= 0 || eq_pos >= len - 1) return;

  const char* key = line;
  int key_len = eq_pos;
  const char* value = line + eq_pos + 1;
  int value_len = len - eq_pos - 1;

  (void)apply_config_kv(key, key_len, value, value_len);
}

// ============================================================================
// CLI args: parse argv from initial stack
// Supports: -port=, -host=, -interval_ms=, -poll_timeout_ms=, -mine=
// ============================================================================

static void load_cli_overrides(void){
  char buf[2048];
  int fd, n;

#if defined(__x86_64__)
  fd = (int)sys(SYS_open, (i64)"/proc/self/cmdline", O_RDONLY, 0, 0, 0, 0);
#elif defined(__aarch64__)
  fd = (int)sys(SYS_openat, AT_FDCWD, (i64)"/proc/self/cmdline", O_RDONLY, 0, 0, 0);
#endif
  if(fd < 0) return;

  n = (int)sys(SYS_read, fd, (i64)buf, sizeof(buf), 0, 0, 0);
  sys(SYS_close, fd, 0, 0, 0, 0, 0);
  if(n <= 0) return;

  int pos = 0;
  int printed = 0;

  // skip argv[0]
  while(pos < n && buf[pos] != '\0') pos++;
  if(pos < n) pos++;

  while(pos < n){
    int start = pos;
    while(pos < n && buf[pos] != '\0') pos++;
    int len = pos - start;
    if(len > 0){
      const char* a = buf + start;

      // strip leading '-'s
      int off = 0; while(off < len && a[off] == '-') off++;
      const char* s = a + off;
      int slen = len - off;

      // find '='
      int eq = -1;
      for(int j = 0; j < slen; j++){
        if(s[j] == '='){ eq = j; break; }
      }
      if(eq > 0 && eq < slen - 1){
        if(apply_config_kv(s, eq, s + eq + 1, slen - eq - 1)){
          if(!printed){
            static const char hdr[] = "CLI overrides loaded:\n";
            sys(SYS_write, 1, (i64)hdr, sizeof(hdr) - 1, 0, 0, 0);
            printed = 1;
          }
          sys(SYS_write, 1, (i64)"  ", 2, 0, 0, 0);
          sys(SYS_write, 1, (i64)s, slen, 0, 0, 0);
          sys(SYS_write, 1, (i64)"\n", 1, 0, 0, 0);
        }
      }
    }
    pos++; // past NUL
  }
}

// ============================================================================
// Main server
// ============================================================================
void _start(void){
  // Load configuration first
  load_config("diggy.conf");
  load_env_overrides();
  load_cli_overrides();

  // Initialize route path lengths
  init_routes();

  // Print startup message with actual port
  static const char startup_msg1[] = "starting diggy server on :";
  char port_str[12];
  int port_len = itoa(config.port, port_str);
  static const char startup_msg2[] = "\n";

  sys(SYS_write, 1, (i64)startup_msg1, sizeof(startup_msg1) - 1, 0, 0, 0);
  sys(SYS_write, 1, (i64)port_str, port_len, 0, 0, 0);
  sys(SYS_write, 1, (i64)startup_msg2, sizeof(startup_msg2) - 1, 0, 0, 0);

  // Create and configure socket
  int sock = (int)sys(SYS_socket, AF_INET, SOCK_STREAM, 0, 0, 0, 0);
  int one = 1;
  sys(SYS_setsockopt, sock, SOL_SOCKET, SO_REUSEADDR, (i64)&one, sizeof(one), 0);

  // Bind and listen using config values
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config.port);
  addr.sin_addr.s_addr = config.host;  // Use configured host

  sys(SYS_bind, sock, (i64)&addr, sizeof(addr), 0, 0, 0);
  sys(SYS_listen, sock, 128, 0, 0, 0, 0);

  // Line printing state
  int current_pos = 0;
  int elapsed_ms = 0;

  // Poll structure
  struct pollfd pfd;
  pfd.fd = sock;
  pfd.events = POLLIN;

  // Main server loop
  for(;;){
    // Poll with configured timeout
    struct timespec ts;
    ts.tv_sec = config.poll_timeout_ms / 1000;
    ts.tv_nsec = (config.poll_timeout_ms % 1000) * 1000000;

#if defined(__x86_64__)
    int ready = (int)sys(SYS_poll, (i64)&pfd, 1, config.poll_timeout_ms, 0, 0, 0);
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
    elapsed_ms += config.poll_timeout_ms;
    if(elapsed_ms >= config.interval_ms){
      elapsed_ms = 0;
      print_next_line(&current_pos);
    }
  }
}