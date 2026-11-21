#include "radar_wifi.h"
#include "common.h"
#include "cy_wcm.h"
#include "cybsp_wifi.h"
#include "json.h"
#include <string.h>

typedef struct { uint8_t command; char ssid[33], password[64]; } wifi_request_t;
static QueueHandle_t requests;
static volatile bool initialized, busy;
static volatile uint32_t address, network_generation;
static uint32_t last_error;
static volatile cy_rslt_t driver_error;
static volatile uint32_t detected_auth = CY_WCM_SECURITY_UNKNOWN, selected_auth = CY_WCM_SECURITY_UNKNOWN;
static volatile bool compatibility_retry;
static const char *wifi_state = "idle";

/* WCM otherwise replaces the radio's specific error with BSP_INIT_ERROR. */
cy_rslt_t __real_cybsp_wifi_init_primary_extended(whd_interface_t*, whd_init_config_t*,
    whd_resource_source_t*, whd_buffer_funcs_t*, whd_netif_funcs_t*);
cy_rslt_t __wrap_cybsp_wifi_init_primary_extended(whd_interface_t *interface, whd_init_config_t *config,
    whd_resource_source_t *resource, whd_buffer_funcs_t *buffer, whd_netif_funcs_t *netif) {
    driver_error = __real_cybsp_wifi_init_primary_extended(interface, config, resource, buffer, netif);
    return driver_error;
}

static void set_state(const char *name, uint32_t ip, uint32_t error) {
    taskENTER_CRITICAL();
    if (address != ip) ++network_generation;
    wifi_state = name; address = ip; last_error = error;
    taskEXIT_CRITICAL();
}

bool radar_wifi_connected(void) { return address != 0; }
uint32_t radar_wifi_network_generation(void) { return network_generation; }

static void report(void) {
    uint32_t ip, error; const char *name; bool pending;
    taskENTER_CRITICAL();
    ip = address; error = last_error; name = wifi_state; pending = busy;
    taskEXIT_CRITICAL();
    terminal_printf("{\"wifi\":{\"protocol\":1,\"state\":\"%s\",\"busy\":%s,"
        "\"ip\":\"%lu.%lu.%lu.%lu\",\"port\":8080,\"path\":\"/radar\","
        "\"error_code\":%lu,\"driver_error\":%lu,\"initialized\":%s,\"persistent\":false,"
        "\"detected_auth\":%lu,\"selected_auth\":%lu,\"compatibility_retry\":%s}}\n", name, pending ? "true" : "false",
        (unsigned long)(ip & 255), (unsigned long)((ip >> 8) & 255),
        (unsigned long)((ip >> 16) & 255), (unsigned long)(ip >> 24), (unsigned long)error,
        (unsigned long)driver_error, initialized ? "true" : "false",
        (unsigned long)detected_auth, (unsigned long)selected_auth, compatibility_retry ? "true" : "false");
}

static void wifi_event(cy_wcm_event_t event, cy_wcm_event_data_t *data) {
    /* WCM callback must not take terminal_mutex or make a blocking WCM call. */
    if (event == CY_WCM_EVENT_DISCONNECTED) set_state("disconnected", 0, 0);
    else if (event == CY_WCM_EVENT_IP_CHANGED && data && data->ip_addr.version == CY_WCM_IP_VER_V4)
        set_state("connected", data->ip_addr.ip.v4, 0);
}

static void wifi_task(void *unused) {
    (void)unused;
    wifi_request_t request;
    for (;;) {
        if (xQueueReceive(requests, &request, pdMS_TO_TICKS(200)) == pdTRUE) {
            cy_rslt_t result = CY_RSLT_SUCCESS;
            if (request.command == 30) {
                compatibility_retry = false;
                detected_auth = selected_auth = CY_WCM_SECURITY_UNKNOWN;
                set_state("connecting", 0, 0);
                if (!initialized) {
                    cy_wcm_config_t config = { .interface = CY_WCM_INTERFACE_TYPE_STA };
                    /* Radar BSP enables only the 150 MHz PLL. The SDIO HAL needs
                     * a 50 MHz-capable source (HF dividers are 1/2/4/8). Supply
                     * it from the unused FLL/path 0; CPU/radar clocks stay as set. */
                    Cy_SysClk_FllDisable();
                    result = Cy_SysClk_FllConfigure(8000000UL, 100000000UL, CY_SYSCLK_FLLPLL_OUTPUT_AUTO);
                    if (result == CY_RSLT_SUCCESS) result = Cy_SysClk_FllEnable(10000UL);
                    if (result == CY_RSLT_SUCCESS) result = cy_wcm_init(&config);
                    if (result == CY_RSLT_SUCCESS) {
                        initialized = true;
                        result = cy_wcm_register_event_callback(wifi_event);
                    }
                }
                if (result == CY_RSLT_SUCCESS) {
                    cy_wcm_connect_params_t params = {0};
                    cy_wcm_ip_address_t ip = {0};
                    memcpy(params.ap_credentials.SSID, request.ssid, sizeof(request.ssid));
                    memcpy(params.ap_credentials.password, request.password, sizeof(request.password));
                    /* WCM discovers AP authentication internally for UNKNOWN. */
                    params.ap_credentials.security = CY_WCM_SECURITY_UNKNOWN;
                    result = cy_wcm_connect_ap(&params, &ip);
                    detected_auth = selected_auth = params.ap_credentials.security;
                    /* WHD rejects FT-PSK before authentication. Many mesh APs
                     * advertise FT alongside ordinary WPA2-PSK; try that AKM
                     * once, keeping AES and the same credentials. Do not apply
                     * this fallback to enterprise, WPA3-only or unknown modes. */
                    if (result == WHD_UNKNOWN_SECURITY_TYPE &&
                        params.ap_credentials.security == CY_WCM_SECURITY_WPA2_FBT_PSK) {
                        compatibility_retry = true;
                        selected_auth = params.ap_credentials.security = CY_WCM_SECURITY_WPA2_AES_PSK;
                        result = cy_wcm_connect_ap(&params, &ip);
                    }
                    memset(&params, 0, sizeof(params));
                    if (result == CY_RSLT_SUCCESS && ip.version == CY_WCM_IP_VER_V4)
                        set_state("connected", ip.ip.v4, 0);
                    else if (result == CY_RSLT_SUCCESS) result = CY_RSLT_TYPE_ERROR;
                }
            } else {
                if (initialized) result = cy_wcm_disconnect_ap();
                set_state("disconnected", 0, result);
            }
            memset(&request, 0, sizeof(request));
            if (result != CY_RSLT_SUCCESS) set_state("error", 0, result);
            busy = false;
        }
    }
}

static bool copy_field(struct json object, const char *key, char *out, size_t capacity) {
    struct json value = json_object_get(object, key);
    if (json_type(value) != JSON_STRING) return false;
    size_t length = json_string_copy(value, out, capacity);
    return length < capacity && strlen(out) == length; /* reject embedded NUL */
}

bool radar_wifi_command(uint8_t id, const char *payload, uint16_t length) {
    /* Status is emitted from the existing command/process task only. An async
     * Wi-Fi task print could interleave a multi-part BLE identity/settings JSON. */
    if (id == 31) { if (length) return false; report(); return true; }
    if (!requests || busy || (id != 30 && id != 32)) return false;
    wifi_request_t request = { .command = id };
    if (id == 30) {
        if (!payload || !json_validn(payload, length)) return false;
        struct json object = json_parsen(payload, length);
        if (!copy_field(object, "ssid", request.ssid, sizeof(request.ssid)) || !request.ssid[0] ||
            !copy_field(object, "password", request.password, sizeof(request.password))) return false;
    } else if (length) return false;
    busy = true;
    bool accepted = xQueueSend(requests, &request, 0) == pdTRUE;
    memset(&request, 0, sizeof(request));
    if (!accepted) busy = false;
    return accepted;
}

void radar_wifi_init(void) {
    requests = xQueueCreate(1, sizeof(wifi_request_t));
    if (!requests || xTaskCreate(wifi_task, "wifi", 3072, NULL, 2, NULL) != pdPASS) {
        if (requests) vQueueDelete(requests);
        requests = NULL;
        set_state("error", 0, 1);
    }
    radar_stream_init();
}
