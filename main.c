// build: cc -Os -s -static -fdata-sections -ffunction-sections -Wl,--gc-sections -o app server.c
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define RECV_BUF 8192

static const char *lyrics =
"Brothers of the mine rejoice!\n"
"(Swing, swing, swing with me!)\n"
"Raise your pick and raise your voice!\n"
"(Sing, sing, sing with me!)\n"
"\n"
"Down and down into the deep\n"
"Who knows what we'll find beneath?\n"
"Diamonds, rubies, gold, and more\n"
"Hidden in the mountain store\n"
"\n"
"Born underground\n"
"Suckled from a teat of stone\n"
"Raised in the dark\n"
"The safety of our mountain home\n"
"\n"
"Skin made of iron\n"
"Steel in our bones\n"
"To dig and dig makes us free\n"
"Come on, brothers, sing with me!\n"
"\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, diggy diggy hole\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, digging a hole\n"
"\n"
"The sunlight will not reach this low\n"
"(Deep, deep in the mine)\n"
"Never seen the blue moon glow\n"
"(Dwarves won't fly so high)\n"
"\n"
"Fill a glass and down some mead\n"
"Stuff your bellies at the feast!\n"
"Stumble home and fall asleep\n"
"Dreaming in our mountain keep\n"
"\n"
"Born underground\n"
"Grown inside a rocky womb\n"
"The Earth is our cradle\n"
"The mountain shall become our tomb\n"
"\n"
"Face us on the battlefield\n"
"You will meet your doom\n"
"We do not fear what lies beneath\n"
"We can never dig too deep\n"
"\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, diggy diggy hole\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, digging a hole\n"
"\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, diggy diggy hole\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, digging a hole\n"
"\n"
"Born underground\n"
"Suckled from a teat of stone\n"
"Raised in the dark\n"
"The safety of our mountain home\n"
"\n"
"Skin made of iron\n"
"Steel in our bones\n"
"To dig and dig makes us free\n"
"Come on, brothers, sing with me!\n"
"\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, diggy diggy hole\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, digging a hole\n"
"\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, diggy diggy hole\n"
"I am a dwarf and I'm digging a hole\n"
"Diggy diggy hole, digging a hole\n";

static void print_next_line_every_3s(const char *text, size_t len, time_t *next_tick, size_t *pos) {
    time_t now = time(NULL);
    if (now < *next_tick) return;

    // find next newline
    size_t start = *pos;
    if (start >= len) { start = 0; } // loop
    size_t i = start;
    while (i < len && text[i] != '\n') i++;

    // print [start, i) and a newline
    if (i > start) (void)write(STDOUT_FILENO, text + start, i - start);
    (void)write(STDOUT_FILENO, "\n", 1);

    *pos = (i < len) ? (i + 1) : 0;
    *next_tick = now + 2;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN); // avoid crash on client aborts

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return 1;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(s, 128) < 0) return 1;

    time_t next_tick = time(NULL); // start printing immediately
    size_t pos = 0;
    size_t lyrics_len = strlen(lyrics);

    for (;;) {
        // multiplex: wait up to 200ms for a client, also tick every 3s
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s, &rfds);
        struct timeval tv = {0, 200000}; // 200ms
        int r = select(s + 1, &rfds, NULL, NULL, &tv);
        if (r < 0 && errno != EINTR) {
            // even on error, still keep ticking prints
        }
        // periodic terminal print (no threads)
        print_next_line_every_3s(lyrics, lyrics_len, &next_tick, &pos);

        if (r > 0 && FD_ISSET(s, &rfds)) {
            int c = accept(s, NULL, NULL);
            if (c < 0) continue;

            char buf[RECV_BUF];
            int n = read(c, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = 0;

                // ultra-naive parse: METHOD PATH from request line
                char method[8] = {0}, path[1024] = {0};
                sscanf(buf, "%7s %1023s", method, path);

                const char *status = "200 OK";
                const char *body   = lyrics;

                if (strcmp(path, "/") == 0) {
                    body = lyrics;
                } else if (strcmp(path, "/health") == 0) {
                    body = "ok\n";
                } else {
                    status = "404 Not Found";
                    body   = "not found\n";
                }

                char hdr[256];
                int blen = (int)strlen(body);
                int hlen = snprintf(hdr, sizeof(hdr),
                                    "HTTP/1.1 %s\r\n"
                                    "Content-Type: text/plain; charset=utf-8\r\n"
                                    "Content-Length: %d\r\n"
                                    "Connection: close\r\n"
                                    "\r\n", status, blen);
                if (hlen > 0) {
                    (void)write(c, hdr, (size_t)hlen);
                    (void)write(c, body, (size_t)blen);
                }
            }
            close(c);
        }
    }
}
