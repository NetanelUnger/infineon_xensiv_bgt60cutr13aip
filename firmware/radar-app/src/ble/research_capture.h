#pragma once
#include <stdbool.h>
#include <stdint.h>
/* All calls run on process_task; no locks or radio waits in acquisition. */
bool research_command(uint8_t id, const char *payload, uint16_t length);
void research_frame(const uint16_t *samples, uint32_t sample_count,
                    uint32_t timestamp_ms, uint32_t dropped);
