/* BLE transport v1: JSON notifications use u16 LE {message,offset,total} + UTF-8.
 * RX is the existing D9/id/length/payload/CRC16 serial protocol, fragmented into
 * ordered write-with-response operations. Only info/settings commands allowed.
 * No pairing/bond persistence: intended for local evaluation with one client. */
#include "radar_ble.h"
#include "common.h"
#include "cyhal.h"
#include "cybsp_bt_config.h"
#include "cybt_platform_config.h"
#include "cybt_platform_interface.h"
#include "wiced_bt_stack.h"
#include "wiced_bt_dev.h"
#include "wiced_bt_gatt.h"
#include "cycfg_bt_settings.h"
#include "cycfg_gap.h"
#include "cycfg_gatt_db.h"
#include <string.h>
#include <stdlib.h>

#define MAX_JSON 12288
#define MAX_COMMAND 2048
typedef struct { uint16_t length, generation; char text[]; } message_t;
static QueueHandle_t replies, frames;
/* All GATT calls execute in the Bluetooth stack task. A polling handshake keeps
 * stack callbacks free of task-blocking calls and bounds outstanding buffers. */
static volatile unsigned tx_state; /* 0 idle, 1 pending, 2 sent, 3 rejected */
static struct { uint8_t *data; uint16_t length, conn, generation; } tx_packet;
static volatile uint16_t connection, mtu = 23, generation;
static volatile bool subscribed;
static uint8_t rx[MAX_COMMAND + 6];
static size_t rx_used;
static TickType_t rx_time;
extern uint8_t radar_cccd[2];
static void require(bool ok) { if (!ok) { __disable_irq(); for (;;) {} } }

static uint16_t u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static uint16_t crc16(const uint8_t *p, size_t n) {
    uint16_t crc = 0xffff;
    while (n--) { crc ^= (uint16_t)*p++ << 8;
        for (int i=0;i<8;i++) crc = (crc & 0x8000) ? (crc << 1)^0x1021 : crc << 1;
    } return crc;
}

static void enqueue_line(const char *text, size_t length) {
    if (!subscribed || !length || text[0] != '{') return;
    bool frame = strstr(text, "\"tracks\"") != NULL;
    /* Diagnostic chatter does not consume reply capacity. */
    if (!frame && !strstr(text,"\"version\"") && !strstr(text,"\"settings\"") &&
        !strstr(text,"\"research\"") && !strstr(text,"\"research_page\"") && !strstr(text,"\"wifi\"") &&
        !strstr(text,"\"cmd\"") && !strstr(text,"\"warning\"") && !strstr(text,"\"error\"")) return;
    static TickType_t last_frame;
    TickType_t now = xTaskGetTickCount();
    if (frame && (now-last_frame) < pdMS_TO_TICKS(200)) return;
    if (frame) last_frame = now;
    message_t *m = pvPortMalloc(sizeof(*m) + length);
    if (!m) return;
    m->length = length; m->generation = generation;
    memcpy(m->text,text,length);
    QueueHandle_t q = frame ? frames : replies;
    if (frame) { message_t *old; if (xQueueReceive(q,&old,0)==pdTRUE) vPortFree(old); }
    if (xQueueSend(q,&m,0)!=pdTRUE) vPortFree(m);
}

void radar_ble_terminal_bytes(const char *data, size_t length) {
    static char line[MAX_JSON]; static size_t used; static bool overflow;
    if (!replies) return;
    for (size_t i=0;i<length;i++) {
        if (data[i]=='\n') {
            if (!overflow) { line[used]=0; enqueue_line(line,used); }
            used=0; overflow=false;
        } else if (used < sizeof(line)-1) line[used++]=data[i];
        else overflow=true;
    }
}

static wiced_bt_gatt_status_t receive_bytes(const uint8_t *p, size_t n) {
    TickType_t now=xTaskGetTickCount();
    if (now-rx_time > pdMS_TO_TICKS(5000)) rx_used=0;
    rx_time=now;
    if (!n || rx_used+n > sizeof(rx)) { rx_used=0; return WICED_BT_GATT_INVALID_ATTR_LEN; }
    memcpy(rx+rx_used,p,n); rx_used+=n;
    if (rx[0]!=0xd9) { rx_used=0; return WICED_BT_GATT_ILLEGAL_PARAMETER; }
    if (rx_used<4) return WICED_BT_GATT_SUCCESS;
    size_t length=u16(rx+2), total=length+6;
    if (length>MAX_COMMAND || rx_used>total) { rx_used=0; return WICED_BT_GATT_INVALID_ATTR_LEN; }
    if (rx_used<total) return WICED_BT_GATT_SUCCESS;
    rx_used=0;
    if (crc16(rx,total-2)!=u16(rx+total-2)) return WICED_BT_GATT_ILLEGAL_PARAMETER;
    if (!terminal_submit_ble(rx[1],rx+4,length)) return WICED_BT_GATT_ILLEGAL_PARAMETER;
    return WICED_BT_GATT_SUCCESS;
}

static gatt_db_lookup_table_t *attribute(uint16_t handle) {
    for (unsigned i=0;i<app_gatt_db_ext_attr_tbl_size;i++)
        if (app_gatt_db_ext_attr_tbl[i].handle==handle) return &app_gatt_db_ext_attr_tbl[i];
    return NULL;
}
static void free_buffer(void *p) { vPortFree(p); }
static void sent_buffer(void *p) { vPortFree(p); tx_state=2; }
static void send_in_stack(void *unused) {
    (void)unused;
    if(connection && subscribed && tx_packet.generation==generation &&
       wiced_bt_gatt_server_send_notification(tx_packet.conn,RADAR_TX_HANDLE,
           tx_packet.length,tx_packet.data,(void*)sent_buffer)==WICED_BT_GATT_SUCCESS) return;
    vPortFree(tx_packet.data); tx_state=3;
}

static wiced_bt_gatt_status_t request(wiced_bt_gatt_attribute_request_t *r, uint16_t *err) {
    switch (r->opcode) {
    case GATT_REQ_MTU:
        mtu=r->data.remote_mtu < CY_BT_MTU_SIZE ? r->data.remote_mtu : CY_BT_MTU_SIZE;
        if (mtu<23) mtu=23;
        return wiced_bt_gatt_server_send_mtu_rsp(r->conn_id,r->data.remote_mtu,CY_BT_MTU_SIZE);
    case GATT_REQ_WRITE: {
        wiced_bt_gatt_write_req_t *w=&r->data.write_req; *err=w->handle;
        if (w->offset) return WICED_BT_GATT_INVALID_OFFSET;
        wiced_bt_gatt_status_t status=WICED_BT_GATT_SUCCESS;
        if (w->handle==RADAR_RX_HANDLE) {
            if (!subscribed) return WICED_BT_GATT_WRITE_NOT_PERMIT;
            status=receive_bytes(w->p_val,w->val_len);
        } else if (w->handle==RADAR_CCCD_HANDLE || w->handle==9) {
            if (w->val_len!=2 || w->p_val[1] || w->p_val[0]>(w->handle==9?2:1)) return WICED_BT_GATT_INVALID_ATTR_LEN;
            memcpy(attribute(w->handle)->p_data,w->p_val,2);
            if(w->handle==RADAR_CCCD_HANDLE) { subscribed=w->p_val[0]==1; rx_used=0; }
        } else return WICED_BT_GATT_WRITE_NOT_PERMIT;
        if (status!=WICED_BT_GATT_SUCCESS) return status;
        return wiced_bt_gatt_server_send_write_rsp(r->conn_id,r->opcode,w->handle);
    }
    case GATT_REQ_READ:
    case GATT_REQ_READ_BLOB: {
        *err=r->data.read_req.handle;
        gatt_db_lookup_table_t *a=attribute(*err);
        if (!a) return WICED_BT_GATT_INVALID_HANDLE;
        unsigned off=r->data.read_req.offset;
        if (off>a->cur_len) return WICED_BT_GATT_INVALID_OFFSET;
        unsigned n=a->cur_len-off; if(n>r->len_requested) n=r->len_requested;
        return wiced_bt_gatt_server_send_read_handle_rsp(r->conn_id,r->opcode,n,a->p_data+off,NULL);
    }
    case GATT_REQ_READ_BY_TYPE: {
        wiced_bt_gatt_read_by_type_t *read=&r->data.read_by_type;
        uint8_t *buf=pvPortMalloc(r->len_requested), pair=0; unsigned used=0;
        if(!buf) return WICED_BT_GATT_INSUF_RESOURCE;
        uint16_t h=read->s_handle;
        while((h=wiced_bt_gatt_find_handle_by_type(h,read->e_handle,&read->uuid))!=0) {
            *err=h; gatt_db_lookup_table_t *a=attribute(h);
            if(!a) break;
            int n=wiced_bt_gatt_put_read_by_type_rsp_in_stream(buf+used,r->len_requested-used,&pair,h,a->cur_len,a->p_data);
            if(!n) break;
            used+=n; h++;
        }
        if(!used) {vPortFree(buf); return WICED_BT_GATT_ATTRIBUTE_NOT_FOUND;}
        wiced_bt_gatt_status_t status=wiced_bt_gatt_server_send_read_by_type_rsp(r->conn_id,r->opcode,pair,used,buf,(void*)free_buffer);
        if(status!=WICED_BT_GATT_SUCCESS) vPortFree(buf);
        return status;
    }
    case GATT_HANDLE_VALUE_NOTIF: return WICED_BT_GATT_SUCCESS;
    default: return WICED_BT_GATT_REQ_NOT_SUPPORTED;
    }
}

static wiced_bt_gatt_status_t gatt_event(wiced_bt_gatt_evt_t event,wiced_bt_gatt_event_data_t *e) {
    switch(event) {
    case GATT_CONNECTION_STATUS_EVT:
        subscribed=false; memset(radar_cccd,0,2); mtu=23; rx_used=0; generation++;
        connection=e->connection_status.connected ? e->connection_status.conn_id : 0;
        if(!connection) wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,0,NULL);
        break;
    case GATT_ATTRIBUTE_REQUEST_EVT: {
        uint16_t err=0;
        wiced_bt_gatt_status_t s=request(&e->attribute_request,&err);
        if(s!=WICED_BT_GATT_SUCCESS) wiced_bt_gatt_server_send_error_rsp(e->attribute_request.conn_id,e->attribute_request.opcode,err,s);
        return s;
    }
    case GATT_GET_RESPONSE_BUFFER_EVT:
        e->buffer_request.buffer.p_app_rsp_buffer=pvPortMalloc(e->buffer_request.len_requested);
        e->buffer_request.buffer.p_app_ctxt=(void*)free_buffer;
        return e->buffer_request.buffer.p_app_rsp_buffer ? WICED_BT_GATT_SUCCESS : WICED_BT_GATT_INSUF_RESOURCE;
    case GATT_APP_BUFFER_TRANSMITTED_EVT:
        if(e->buffer_xmitted.p_app_ctxt) ((void(*)(void*))e->buffer_xmitted.p_app_ctxt)(e->buffer_xmitted.p_app_data);
        break;
    default: break;
    }
    return WICED_BT_GATT_SUCCESS;
}

static wiced_result_t management(wiced_bt_management_evt_t event,wiced_bt_management_evt_data_t *e) {
    if(event==BTM_ENABLED_EVT && e->enabled.status==WICED_BT_SUCCESS) {
        /* Stable random-static address from silicon unique ID, never a copied
         * example public address. Most significant two bits must be 1. */
        uint64_t uid=Cy_SysLib_GetUniqueId(); uint8_t address[6];
        for(unsigned i=0;i<6;i++) address[5-i]=(uid>>(8*i)) & 255;
        address[0]|=0xc0;
        require(wiced_bt_set_local_bdaddr(address,BLE_ADDR_RANDOM)==WICED_BT_SUCCESS);
        wiced_bt_set_pairable_mode(FALSE,FALSE);
        require(wiced_bt_gatt_register(gatt_event)==WICED_BT_GATT_SUCCESS);
        require(wiced_bt_gatt_db_init(gatt_database,gatt_database_len,NULL)==WICED_BT_GATT_SUCCESS);
        uint8_t flags=6, uuid[]={RADAR_SERVICE_UUID};
        wiced_bt_ble_advert_elem_t adv[]={
            {.advert_type=BTM_BLE_ADVERT_TYPE_FLAG,.len=1,.p_data=&flags},
            {.advert_type=BTM_BLE_ADVERT_TYPE_128SRV_COMPLETE,.len=16,.p_data=uuid}
        };
        wiced_bt_ble_advert_elem_t scan={.advert_type=BTM_BLE_ADVERT_TYPE_NAME_COMPLETE,.len=9,.p_data=app_gap_device_name};
        wiced_bt_ble_set_raw_advertisement_data(2,adv);
        wiced_bt_ble_set_raw_scan_response_data(1,&scan);
        wiced_bt_start_advertisements(BTM_BLE_ADVERT_UNDIRECTED_HIGH,0,NULL);
    }
    return WICED_BT_SUCCESS;
}

static void transmit(void *unused) {
    (void)unused; uint16_t message_id=0;
    for(;;) {
        message_t *m=NULL;
        if(xQueueReceive(replies,&m,0)!=pdTRUE && xQueueReceive(frames,&m,pdMS_TO_TICKS(20))!=pdTRUE) continue;
        uint16_t id=++message_id, off=0;
        TickType_t deadline=xTaskGetTickCount();
        while(off<m->length && connection && subscribed && m->generation==generation) {
            if(xTaskGetTickCount()-deadline>pdMS_TO_TICKS(15000)) break;
            if(tx_state==1) {vTaskDelay(pdMS_TO_TICKS(2)); continue;}
            uint16_t n=m->length-off, limit=mtu-9;
            if(n>limit) n=limit;
            uint8_t *buf=pvPortMalloc(n+6);
            if(!buf) break;
            put16(buf,id); put16(buf+2,off); put16(buf+4,m->length); memcpy(buf+6,m->text+off,n);
            tx_packet.data=buf; tx_packet.length=n+6; tx_packet.conn=connection; tx_packet.generation=generation;
            tx_state=1;
            if(wiced_bt_serialize_function_from_isr(send_in_stack,NULL)!=WICED_BT_SUCCESS) {
                vPortFree(buf); tx_state=3;
            }
            while(tx_state==1) vTaskDelay(pdMS_TO_TICKS(2));
            if(tx_state==2) {off+=n; vTaskDelay(pdMS_TO_TICKS(25));}
            else vTaskDelay(pdMS_TO_TICKS(20));
        }
        vPortFree(m);
    }
}

void radar_ble_init(void) {
    replies=xQueueCreate(6,sizeof(message_t*)); frames=xQueueCreate(1,sizeof(message_t*));
    require(replies && frames);
    require(xTaskCreate(transmit,"ble-json",2048,NULL,2,NULL)==pdPASS);
    cybt_platform_config_init(&cybsp_bt_platform_cfg);
    require(wiced_bt_stack_init(management,&wiced_bt_cfg_settings)==WICED_BT_SUCCESS);
}
