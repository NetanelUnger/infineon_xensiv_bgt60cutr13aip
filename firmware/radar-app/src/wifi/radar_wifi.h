#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Demo provisioning deliberately uses the existing unpaired BLE transport.
 * Commands: 30 {ssid,password}, 31 status, 32 disconnect. RAM-only credentials. */
void radar_wifi_init(void);
bool radar_wifi_command(uint8_t id, const char *payload, uint16_t length);
bool radar_wifi_connected(void);
uint32_t radar_wifi_network_generation(void);

void radar_stream_init(void);
/* Acquisition calls never wait for sockets, allocate memory, or run DSP. */
void radar_stream_frame(const uint16_t *samples, uint32_t words, uint32_t ms);
void radar_stream_fifo_error(void);
void radar_stream_configuration_changed(void);
