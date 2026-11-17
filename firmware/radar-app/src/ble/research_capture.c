/* Experimental capture: first averaged SEQ0 chirp, all three RX, per frame.
 * Preserves static echoes. No people filter, DC removal, FFT or temporal
 * averaging is applied here. Post-processing and provenance live in the client.
 * ADC format/order: Infineon BGT60CUTR13AIP FAQ 1286305, SEQ0 unsigned U12.4,
 * sample-major RX interleaving. Physical range calibration is intentionally null.
 */
#include "research_capture.h"
#include "common.h"
#include "json.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define CAPACITY 256u
#define CHIRP_WORDS (64u * 3u)
#define FRAME_WORDS (CHIRP_WORDS * 17u)
typedef struct { uint32_t ms, dropped; uint16_t adc[CHIRP_WORDS]; } row_t;
static row_t rows[CAPACITY];
static uint32_t capture_id, count, duration_ms, started_ms, total_dropped;
static bool active;
static const char *reason = "idle";

static uint32_t now_ms(void) { return xTaskGetTickCount() * portTICK_PERIOD_MS; }
static void expire(void) {
    if (active && (uint32_t)(now_ms() - started_ms) >= duration_ms) {
        active = false; reason = count ? "duration" : "no_frames";
    }
}
static void status(void) {
    expire();
    terminal_printf("{\"research\":{\"protocol\":1,\"id\":%lu,\"active\":%s,"
        "\"count\":%lu,\"duration_ms\":%lu,\"reason\":\"%s\",\"capacity\":256,"
        "\"dropped\":%lu,\"samples\":64,\"rx\":3,\"chirp_index\":0,"
        "\"chirps_per_frame\":17,\"averages\":16,\"encoding\":\"u12.4\","
        "\"order\":\"sample_rx\",\"range_step_m\":null,"
        "\"config\":\"presence_detection_tracking_seq0\"}}\n",
        (unsigned long)capture_id, active ? "true" : "false", (unsigned long)count,
        (unsigned long)duration_ms, reason, (unsigned long)total_dropped);
}
static bool number(struct json obj, const char *name, uint32_t *out) {
    struct json v = json_object_get(obj, name);
    double n = json_double(v);
    if (json_type(v) != JSON_NUMBER || !isfinite(n) || n < 0 || n > 4294967295.0 || floor(n) != n) return false;
    *out = (uint32_t)n; return true;
}
bool research_command(uint8_t id, const char *payload, uint16_t length) {
    expire();
    struct json obj = json_parsen(payload ? payload : "", length);
    if (id == 20) {
        uint32_t ms;
        if (active || !number(obj, "duration_ms", &ms) || (ms != 2000 && ms != 5000 && ms != 10000)) return false;
        ++capture_id; if (!capture_id) ++capture_id;
        count = total_dropped = 0; duration_ms = ms; started_ms = now_ms();
        reason = "capturing"; active = true; status(); return true;
    }
    if (id == 21) { if (length) return false; status(); return true; }
    if (id == 23) {
        if (length) return false;
        if (active) { active = false; reason = "cancelled"; }
        status(); return true;
    }
    if (id == 22) {
        uint32_t requested_id, offset;
        if (active || !number(obj, "id", &requested_id) || requested_id != capture_id ||
            !number(obj, "offset", &offset) || offset >= count) return false;
        /* Two records fit below terminal_printf's 4096-byte limit. Clients pull
         * one page at a time and check id/offset so reconnects cannot mix runs. */
        static char buffer[3200];
        unsigned used = (unsigned)snprintf(buffer, sizeof(buffer),
            "{\"research_page\":{\"id\":%lu,\"offset\":%lu,\"rows\":[",
            (unsigned long)capture_id, (unsigned long)offset);
        uint32_t end = offset + 2; if (end > count) end = count;
        for (uint32_t i = offset; i < end; ++i) {
            used += (unsigned)snprintf(buffer + used, sizeof(buffer) - used,
                "%s{\"ms\":%lu,\"dropped\":%lu,\"adc\":[", i == offset ? "" : ",",
                (unsigned long)rows[i].ms, (unsigned long)rows[i].dropped);
            for (unsigned j = 0; j < CHIRP_WORDS; ++j)
                used += (unsigned)snprintf(buffer + used, sizeof(buffer) - used,
                    "%s%u", j ? "," : "", rows[i].adc[j]);
            used += (unsigned)snprintf(buffer + used, sizeof(buffer) - used, "]}");
        }
        (void)snprintf(buffer + used, sizeof(buffer) - used, "]}}\n");
        terminal_printf("%s", buffer); return true;
    }
    return false;
}
void research_frame(const uint16_t *samples, uint32_t sample_count,
                    uint32_t timestamp_ms, uint32_t dropped) {
    expire(); if (!active) return;
    /* Ignore frames already queued before the start command. */
    uint32_t relative = timestamp_ms - started_ms;
    if (relative >= duration_ms) return;
    if (sample_count != FRAME_WORDS) { active = false; reason = "shape_changed"; return; }
    if (count == CAPACITY) { active = false; reason = "capacity"; return; }
    rows[count].ms = relative; rows[count].dropped = dropped;
    memcpy(rows[count].adc, samples, sizeof(rows[count].adc));
    total_dropped += dropped; ++count;
}
