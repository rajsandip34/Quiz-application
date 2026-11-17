#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int socket_t;
#define CLOSE_SOCKET close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PORT 8080
#define REQUEST_CAP 16384

typedef struct {
    int id;
    const char *prompt;
    const char *choices[4];
    int answer;
    const char *explanation;
} Question;

static const Question questions[] = {
    {1, "Which planet is known as the Red Planet?", {"Venus", "Mars", "Jupiter", "Mercury"}, 1, "Mars looks reddish because iron minerals in its soil have oxidized, or rusted."},
    {2, "What does the C function `printf` do?", {"Reads a file", "Prints formatted text", "Allocates memory", "Stops a program"}, 1, "printf writes formatted output to the standard output stream, usually your terminal."},
    {3, "Which ocean is the largest?", {"Atlantic", "Indian", "Arctic", "Pacific"}, 3, "The Pacific Ocean is the largest and deepest of Earth's ocean basins."},
    {4, "How many sides does a hexagon have?", {"Five", "Six", "Seven", "Eight"}, 1, "The prefix hex- means six, so a hexagon has six sides."},
    {5, "Which one is a programming language?", {"HTTP", "HTML", "Python", "USB"}, 2, "Python is a general-purpose programming language. HTML is a markup language; HTTP and USB are standards."}
};
static const int question_count = (int)(sizeof questions / sizeof questions[0]);

static int send_all(socket_t client, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        int n = send(client, data + sent, (int)(length - sent), 0);
        if (n <= 0) return 0;
        sent += (size_t)n;
    }
    return 1;
}

static void respond(socket_t client, const char *status, const char *type, const char *body, size_t length) {
    char header[512];
    int n = snprintf(header, sizeof header,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\nAccess-Control-Allow-Origin: *\r\n\r\n",
        status, type, length);
    if (n > 0) send_all(client, header, (size_t)n);
    send_all(client, body, length);
}

static int read_request(socket_t client, char *buffer, size_t cap, size_t *body_start, size_t *body_length) {
    size_t used = 0;
    size_t header_end = 0;
    size_t expected_body = 0;
    while (used + 1 < cap) {
        int n = recv(client, buffer + used, (int)(cap - used - 1), 0);
        if (n <= 0) return 0;
        used += (size_t)n;
        buffer[used] = '\0';
        if (!header_end) {
            char *marker = strstr(buffer, "\r\n\r\n");
            if (marker) {
                header_end = (size_t)(marker - buffer) + 4;
                char *cl = strstr(buffer, "Content-Length:");
                if (cl && cl < marker) expected_body = (size_t)strtoul(cl + 15, NULL, 10);
                if (expected_body > cap - header_end - 1) return 0;
            }
        }
        if (header_end && used >= header_end + expected_body) {
            *body_start = header_end;
            *body_length = expected_body;
            return 1;
        }
    }
    return 0;
}

static void handle_questions(socket_t client) {
    char body[8192];
    size_t used = (size_t)snprintf(body, sizeof body, "{\"questions\":[");
    for (int i = 0; i < question_count; i++) {
        const Question *q = &questions[i];
        int n = snprintf(body + used, sizeof body - used,
            "%s{\"id\":%d,\"prompt\":\"%s\",\"choices\":[\"%s\",\"%s\",\"%s\",\"%s\"]}",
            i ? "," : "", q->id, q->prompt, q->choices[0], q->choices[1], q->choices[2], q->choices[3]);
        if (n < 0 || (size_t)n >= sizeof body - used) return;
        used += (size_t)n;
    }
    used += (size_t)snprintf(body + used, sizeof body - used, "]}");
    respond(client, "200 OK", "application/json; charset=utf-8", body, used);
}

static void handle_submit(socket_t client, const char *body, size_t body_length) {
    char input[REQUEST_CAP];
    size_t copy = body_length < sizeof input - 1 ? body_length : sizeof input - 1;
    memcpy(input, body, copy);
    input[copy] = '\0';
    char *answers = strstr(input, "\"answers\"");
    if (!answers || !(answers = strchr(answers, '['))) {
        const char *error = "{\"error\":\"Expected an answers array.\"}";
        respond(client, "400 Bad Request", "application/json; charset=utf-8", error, strlen(error));
        return;
    }
    answers++;
    int selected[32];
    int count = 0;
    while (*answers && *answers != ']' && count < 32) {
        if ((*answers >= '0' && *answers <= '9') || *answers == '-') {
            char *end;
            long value = strtol(answers, &end, 10);
            if (end == answers || value < 0 || value > 3) break;
            selected[count++] = (int)value;
            answers = end;
        } else answers++;
    }
    if (count != question_count) {
        const char *error = "{\"error\":\"Please answer every question.\"}";
        respond(client, "400 Bad Request", "application/json; charset=utf-8", error, strlen(error));
        return;
    }
    int score = 0;
    for (int i = 0; i < question_count; i++) if (selected[i] == questions[i].answer) score++;

    char result[8192];
    size_t used = (size_t)snprintf(result, sizeof result, "{\"score\":%d,\"total\":%d,\"results\":[", score, question_count);
    for (int i = 0; i < question_count; i++) {
        const Question *q = &questions[i];
        int n = snprintf(result + used, sizeof result - used,
            "%s{\"id\":%d,\"selected\":%d,\"correct\":%d,\"explanation\":\"%s\"}",
            i ? "," : "", q->id, selected[i], q->answer, q->explanation);
        if (n < 0 || (size_t)n >= sizeof result - used) return;
        used += (size_t)n;
    }
    used += (size_t)snprintf(result + used, sizeof result - used, "]}");
    respond(client, "200 OK", "application/json; charset=utf-8", result, used);
}

static const char *content_type(const char *path) {
    if (strstr(path, ".css")) return "text/css; charset=utf-8";
    if (strstr(path, ".js")) return "text/javascript; charset=utf-8";
    if (strstr(path, ".svg")) return "image/svg+xml";
    return "text/html; charset=utf-8";
}

static void handle_file(socket_t client, const char *path) {
    const char *name = strcmp(path, "/") == 0 ? "index.html" : path + 1;
    if (strstr(name, "..") || strchr(name, '/') || strchr(name, '\\')) {
        const char *msg = "Not found";
        respond(client, "404 Not Found", "text/plain; charset=utf-8", msg, strlen(msg));
        return;
    }
    FILE *file = fopen(name, "rb");
    if (!file) {
        const char *msg = "Not found";
        respond(client, "404 Not Found", "text/plain; charset=utf-8", msg, strlen(msg));
        return;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size < 0 || size > 1024 * 1024) { fclose(file); return; }
    char *data = (char *)malloc((size_t)size);
    if (!data) { fclose(file); return; }
    size_t got = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (got == (size_t)size) respond(client, "200 OK", content_type(name), data, got);
    free(data);
}

static void handle_client(socket_t client) {
    char request[REQUEST_CAP];
    size_t body_start = 0, body_length = 0;
    if (!read_request(client, request, sizeof request, &body_start, &body_length)) return;
    char method[8] = {0}, path[256] = {0};
    if (sscanf(request, "%7s %255s", method, path) != 2) return;
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/questions") == 0) handle_questions(client);
    else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/submit") == 0) handle_submit(client, request + body_start, body_length);
    else if (strcmp(method, "GET") == 0) handle_file(client, path);
    else {
        const char *msg = "Method not allowed";
        respond(client, "405 Method Not Allowed", "text/plain; charset=utf-8", msg, strlen(msg));
    }
}

int main(void) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { fprintf(stderr, "Winsock initialization failed\n"); return 1; }
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) { perror("socket"); return 1; }
    int reuse = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof reuse);
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);
    if (bind(server, (struct sockaddr *)&address, sizeof address) < 0 || listen(server, 16) < 0) {
        perror("bind/listen"); CLOSE_SOCKET(server); return 1;
    }
    printf("Quiz app running at http://localhost:%d\nPress Ctrl+C to stop.\n", PORT);
    for (;;) {
        socket_t client = accept(server, NULL, NULL);
        if (client < 0) continue;
        handle_client(client);
        CLOSE_SOCKET(client);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
