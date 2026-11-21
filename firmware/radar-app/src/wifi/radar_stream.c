#include "radar_wifi.h"
#include "common.h"
#include "radar_config.h"
#include "json.h"
#include "lwip/sockets.h"
#include "lwip/errno.h"
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define RAW_WORDS (SEQ0_NUMBER_OF_CHIRPS * SEQ0_SAMPLES_PER_CHIRP * SEQ0_RX_ANTENNAS)
#define RAW_HEADER 40u
#define SLOT_COUNT 6u
#define WS_PORT 8080
typedef struct { uint8_t bytes[RAW_HEADER + RAW_WORDS * 2u]; } raw_slot_t;
static raw_slot_t slots[SLOT_COUNT];
static QueueHandle_t free_slots, ready_slots;
static volatile bool active, configuration_changed;
static uint32_t session_id, sent;
static volatile uint32_t sequence, queue_drops, fifo_errors, offered;

static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static uint32_t now_ms(void) { return xTaskGetTickCount() * portTICK_PERIOD_MS; }

void radar_stream_fifo_error(void) { if (active) ++fifo_errors; }
void radar_stream_configuration_changed(void) { configuration_changed = true; active = false; }

void radar_stream_frame(const uint16_t *samples, uint32_t words, uint32_t ms) {
    if (!active || !free_slots) return;
    if (words != RAW_WORDS) { radar_stream_configuration_changed(); return; }
    uint32_t number = sequence++;
    ++offered;
    raw_slot_t *slot;
    if (xQueueReceive(free_slots, &slot, 0) != pdTRUE) { ++queue_drops; return; }
    uint8_t *p = slot->bytes;
    memcpy(p, "RDR1", 4); put16(p + 4, 1); put16(p + 6, RAW_HEADER);
    put32(p + 8, session_id); put32(p + 12, number); put32(p + 16, ms);
    put32(p + 20, fifo_errors); put32(p + 24, queue_drops);
    put16(p + 28, SEQ0_NUMBER_OF_CHIRPS); put16(p + 30, SEQ0_RX_ANTENNAS);
    put16(p + 32, SEQ0_SAMPLES_PER_CHIRP); put16(p + 34, SEQ0_AVERAGING_FACTOR);
    put32(p + 36, RAW_WORDS * 2u);
    /* PSOC 6 is little endian. No conversion, truncation or DSP on ADC samples. */
    memcpy(p + RAW_HEADER, samples, RAW_WORDS * 2u);
    if (xQueueSend(ready_slots, &slot, 0) != pdTRUE) {
        ++queue_drops; (void)xQueueSend(free_slots, &slot, 0);
    }
}

static void discard_queue(void) {
    raw_slot_t *slot;
    while (xQueueReceive(ready_slots, &slot, 0) == pdTRUE)
        (void)xQueueSend(free_slots, &slot, 0);
}

/* Only the socket task owns and closes descriptors. All I/O is bounded. */
static bool send_all(int fd, const void *data, size_t length) {
    const uint8_t *p = data;
    uint32_t started = now_ms();
    while (length) {
        if (!radar_wifi_connected() || now_ms() - started > 2000) return false;
        int n = lwip_send(fd, p, length, 0);
        if (n <= 0) return false;
        p += n; length -= (size_t)n;
    }
    return true;
}

static bool ws_send(int fd, uint8_t opcode, const void *data, size_t size) {
    uint8_t header[4] = { (uint8_t)(0x80u | opcode), 0, 0, 0 };
    size_t h = 2;
    if (size > 65535) return false;
    if (size < 126) header[1] = size;
    else { header[1] = 126; header[2] = size >> 8; header[3] = size; h = 4; }
    return send_all(fd, header, h) && (!size || send_all(fd, data, size));
}

static bool text_message(int fd, const char *text) { return ws_send(fd, 1, text, strlen(text)); }

static bool equal_ci(const char *a, const char *b) {
    while (*a && *b) if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
    return !*a && !*b;
}

static bool token_ci(char *value, const char *token) {
    for (char *p = value; *p;) {
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        char *end = p; while (*end && *end != ',') ++end;
        char *trim = end; while (trim > p && (trim[-1] == ' ' || trim[-1] == '\t')) --trim;
        char saved = *trim; *trim = 0; bool match = equal_ci(p, token); *trim = saved;
        if (match) return true;
        p = *end ? end + 1 : end;
    }
    return false;
}

static bool handshake(int fd) {
    char request[2048]; size_t length = 0; uint32_t started = now_ms();
    /* Byte reads avoid consuming an immediately following WebSocket command. */
    while (length < sizeof(request) - 1 && now_ms() - started < 4000) {
        int n = lwip_recv(fd, request + length, 1, 0);
        if (n <= 0) return false;
        ++length;
        if (length >= 4 && !memcmp(request + length - 4, "\r\n\r\n", 4)) break;
    }
    request[length] = 0;
    if (length < 4 || memcmp(request + length - 4, "\r\n\r\n", 4)) return false;
    char *line = strstr(request, "\r\n"); if (!line) return false;
    *line = 0;
    if (strcmp(request, "GET /radar HTTP/1.1")) return false;
    bool upgrade = false, connection = false, version = false;
    char key[25] = {0}; line += 2;
    while (*line) {
        char *end = strstr(line, "\r\n"); if (!end) return false; *end = 0;
        char *colon = strchr(line, ':');
        if (colon) {
            *colon++ = 0; while (*colon == ' ' || *colon == '\t') ++colon;
            char *tail = colon + strlen(colon); while (tail > colon && (tail[-1] == ' ' || tail[-1] == '\t')) *--tail = 0;
            if (equal_ci(line, "Upgrade")) upgrade = equal_ci(colon, "websocket");
            if (equal_ci(line, "Connection")) connection = token_ci(colon, "upgrade");
            if (equal_ci(line, "Sec-WebSocket-Version")) version = !strcmp(colon, "13");
            if (equal_ci(line, "Sec-WebSocket-Key") && strlen(colon) == 24) memcpy(key, colon, 25);
        }
        line = end + 2;
    }
    uint8_t decoded[18]; size_t decoded_length;
    if (!upgrade || !connection || !version || !key[0] ||
        mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_length, (uint8_t*)key, 24) || decoded_length != 16) return false;
    char input[61]; uint8_t digest[20], accept[32]; size_t output_length;
    snprintf(input, sizeof(input), "%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", key);
    if (mbedtls_sha1((uint8_t*)input, strlen(input), digest) ||
        mbedtls_base64_encode(accept, sizeof(accept), &output_length, digest, sizeof(digest))) return false;
    accept[output_length] = 0;
    char response[200];
    int n = snprintf(response, sizeof(response), "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", accept);
    return n > 0 && (size_t)n < sizeof(response) && send_all(fd, response, n);
}

static bool begin_capture(int fd) {
    if (configuration_changed) return text_message(fd, "{\"type\":\"error\",\"reason\":\"configuration_changed_reset_required\"}");
    if (active) return text_message(fd, "{\"type\":\"error\",\"reason\":\"already_recording\"}");
    discard_queue();
    ++session_id; if (!session_id) ++session_id;
    sequence = queue_drops = fifo_errors = offered = sent = 0;
    /* Save the exact compiled register list, not just a name or guessed timing. */
    size_t count = sizeof(register_list) / sizeof(register_list[0]);
    size_t capacity = 1200 + count * 28;
    char *meta = pvPortMalloc(capacity);
    if (!meta) return false;
    size_t used = (size_t)snprintf(meta, capacity,
        "{\"type\":\"started\",\"format\":\"radar-raw-v1\",\"protocol\":1,\"session\":%lu,"
        "\"firmware\":\"" FW_VERSION "\",\"sensor\":\"BGT60CUTR13AIP\",\"config\":\"%s\","
        "\"samples\":%u,\"rx\":%u,\"chirps\":%u,\"averages\":%u,"
        "\"encoding\":\"u12.4\",\"byte_order\":\"little\",\"order\":\"chirp_sample_rx\","
        "\"header_bytes\":40,\"timestamp\":\"software_fifo_read_ms_u32\","
        "\"range_step_m\":null,\"chirp_interval_s\":null,\"system_clock_hz\":%lu,"
        "\"fifo_threshold\":%u,\"registers\":[",
        (unsigned long)session_id, USE_CASE_NAME, SEQ0_SAMPLES_PER_CHIRP, SEQ0_RX_ANTENNAS,
        SEQ0_NUMBER_OF_CHIRPS, SEQ0_AVERAGING_FACTOR, (unsigned long)SYSTEM_CLOCK_HZ, FIFO_THRESHOLD);
    for (size_t i = 0; i < count && used < capacity; ++i)
        used += (size_t)snprintf(meta + used, capacity - used, "%s[%lu,%lu]", i ? "," : "",
            (unsigned long)register_list[i][0], (unsigned long)register_list[i][1]);
    bool ok = false;
    if (used + 3 < capacity) {
        memcpy(meta + used, "]}", 3); ok = text_message(fd, meta);
    }
    vPortFree(meta);
    /* Producer is the higher-priority radar task. Enable only after metadata is sent. */
    if (ok && !configuration_changed) active = true;
    else if (ok) return text_message(fd, "{\"type\":\"error\",\"reason\":\"configuration_changed_reset_required\"}");
    return ok;
}

static bool drain_one(int fd) {
    raw_slot_t *slot;
    if (xQueueReceive(ready_slots, &slot, 0) != pdTRUE) return true;
    bool ok = ws_send(fd, 2, slot->bytes, sizeof(slot->bytes));
    (void)xQueueSend(free_slots, &slot, 0);
    if (ok) ++sent;
    return ok;
}

static bool end_capture(int fd, const char *reason) {
    active = false;
    while (uxQueueMessagesWaiting(ready_slots)) if (!drain_one(fd)) return false;
    char summary[280];
    snprintf(summary, sizeof(summary), "{\"type\":\"stopped\",\"session\":%lu,\"reason\":\"%s\","
        "\"offered\":%lu,\"sent\":%lu,\"queue_dropped\":%lu,\"fifo_errors\":%lu}",
        (unsigned long)session_id, reason, (unsigned long)offered, (unsigned long)sent,
        (unsigned long)queue_drops, (unsigned long)fifo_errors);
    return text_message(fd, summary);
}

/* Browser control messages are small, final, masked frames. The receive buffer
 * handles arbitrary TCP segmentation and multiple coalesced WS frames. */
static bool consume(int fd, uint8_t *buffer, size_t *used) {
    while (*used >= 2) {
        uint8_t opcode = buffer[0] & 15;
        if ((buffer[0] & 0xf0) != 0x80 || !(buffer[1] & 0x80)) return false;
        size_t length = buffer[1] & 127, header = 2;
        if (length == 127) return false;
        if (length == 126) {
            if (*used < 4) return true;
            length = ((size_t)buffer[2] << 8) | buffer[3]; header = 4;
            if (length < 126) return false;
        }
        if (length > 256 || (opcode >= 8 && length > 125)) return false;
        if (*used < header + 4 + length) return true;
        uint8_t payload[257];
        for (size_t i = 0; i < length; ++i) payload[i] = buffer[header + 4 + i] ^ buffer[header + (i % 4)];
        payload[length] = 0;
        bool ok = true;
        if (opcode == 8) { (void)ws_send(fd, 8, payload, length); return false; }
        else if (opcode == 9) ok = ws_send(fd, 10, payload, length);
        else if (opcode == 10) { /* pong */ }
        else if (opcode == 1 && json_validn((char*)payload, length)) {
            struct json cmd = json_object_get(json_parsen((char*)payload, length), "command");
            if (json_type(cmd) != JSON_STRING) return false;
            if (!json_string_compare(cmd, "start")) ok = begin_capture(fd);
            else if (!json_string_compare(cmd, "stop")) ok = end_capture(fd, "requested");
            else ok = text_message(fd, "{\"type\":\"error\",\"reason\":\"unknown_command\"}");
        } else return false;
        if (!ok) return false;
        size_t consumed = header + 4 + length;
        memmove(buffer, buffer + consumed, *used - consumed); *used -= consumed;
    }
    return true;
}

static int readable(int fd, unsigned milliseconds) {
    fd_set set; FD_ZERO(&set); FD_SET(fd, &set);
    struct timeval timeout = { milliseconds / 1000, (milliseconds % 1000) * 1000 };
    return lwip_select(fd + 1, &set, NULL, NULL, &timeout);
}

static void client(int fd, uint32_t generation) {
    struct timeval timeout = {1, 0}; int one = 1;
    lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    if (!handshake(fd) || !text_message(fd, "{\"type\":\"hello\",\"protocol\":1,\"firmware\":\"" FW_VERSION "\"}")) return;
    uint8_t buffer[512]; size_t used = 0; bool was_active = false;
    uint32_t last_control = now_ms(), last_ping = now_ms();
    while (radar_wifi_connected() && generation == radar_wifi_network_generation()) {
        int ready = readable(fd, 5); if (ready < 0) break;
        if (ready) {
            int n = lwip_recv(fd, buffer + used, sizeof(buffer) - used, 0);
            if (n <= 0) break;
            used += n; last_control = now_ms();
            if (!consume(fd, buffer, &used) || used == sizeof(buffer)) break;
        }
        if (was_active && !active && configuration_changed) {
            if (!end_capture(fd, "configuration_changed")) break;
        }
        was_active = active;
        if (active && !drain_one(fd)) break;
        if (now_ms() - last_ping > 5000) {
            if (!ws_send(fd, 9, NULL, 0)) break;
            last_ping = now_ms();
        }
        if (now_ms() - last_control > 20000) break;
    }
}

static void socket_task(void *unused) {
    (void)unused;
    for (;;) {
        if (!radar_wifi_connected()) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        int listener = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener < 0) { vTaskDelay(pdMS_TO_TICKS(1000)); continue; }
        int one = 1; lwip_setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        struct sockaddr_in address = { .sin_family = AF_INET, .sin_port = htons(WS_PORT), .sin_addr = { .s_addr = INADDR_ANY } };
        if (lwip_bind(listener, (struct sockaddr*)&address, sizeof(address)) || lwip_listen(listener, 1)) {
            lwip_close(listener); vTaskDelay(pdMS_TO_TICKS(1000)); continue;
        }
        uint32_t generation = radar_wifi_network_generation();
        while (radar_wifi_connected() && generation == radar_wifi_network_generation()) {
            int ready = readable(listener, 200); if (ready < 0) break; if (!ready) continue;
            int fd = lwip_accept(listener, NULL, NULL); if (fd < 0) continue;
            client(fd, generation);
            active = false; discard_queue(); lwip_close(fd);
        }
        lwip_close(listener);
    }
}

void radar_stream_init(void) {
    free_slots = xQueueCreate(SLOT_COUNT, sizeof(raw_slot_t*));
    ready_slots = xQueueCreate(SLOT_COUNT, sizeof(raw_slot_t*));
    if (!free_slots || !ready_slots) return;
    for (unsigned i = 0; i < SLOT_COUNT; ++i) { raw_slot_t *p = &slots[i]; (void)xQueueSend(free_slots, &p, 0); }
    (void)xTaskCreate(socket_task, "radar-ws", 3072, NULL, 2, NULL);
}
