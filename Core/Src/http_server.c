#include "http_server.h"
#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"
#include "lwip/netif.h"
#include "cJSON.h"
#include "main.h"
#include "cmsis_os.h"
#include "can_handler.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define HTTP_PORT 80

extern struct netif gnetif;

/* Memory hooks for cJSON to use FreeRTOS heap */
/* Memory hooks for cJSON to use FreeRTOS heap */
static void *custom_malloc(size_t size) { return pvPortMalloc(size); }
static void custom_free(void *ptr) { vPortFree(ptr); }

static void http_send_json_response(struct netconn *conn, cJSON *json_obj) {
    char *json_str = cJSON_PrintUnformatted(json_obj);
    if (json_str) {
        char headers[256];
        snprintf(headers, sizeof(headers),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Connection: close\r\n"
                 "Content-Length: %zu\r\n\r\n", strlen(json_str));
        
        netconn_write(conn, headers, strlen(headers), NETCONN_COPY);
        netconn_write(conn, json_str, strlen(json_str), NETCONN_COPY);
        cJSON_free(json_str);
    }
}

static void http_send_error_response(struct netconn *conn, int code, const char *msg) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "HTTP/1.1 %d Error\r\n"
             "Content-Type: application/json\r\n"
             "Connection: close\r\n\r\n"
             "{\"status\":\"error\",\"error\":\"%s\"}", code, msg);
    netconn_write(conn, buf, strlen(buf), NETCONN_COPY);
}

static void http_handle_request(struct netconn *conn, char *req, u16_t req_len) {
    /* Split headers and body */
    char *body = strstr(req, "\r\n\r\n");
    if (body) {
        *body = '\0';
        body += 4;
    } else {
        body = "";
    }
    
    /* We need to be careful with strtok as it modifies the string. 
     * Since req is already a copy, it's fine. */
    char *method = strtok(req, " ");
    char *path = strtok(NULL, " ");
    
    if (!method || !path) {
        http_send_error_response(conn, 400, "Bad Request");
        return;
    }
    
    printf("HTTP: %s %s (body: %zu bytes)\r\n", method, path, strlen(body));
    
    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/api/ping") == 0) {
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON_AddStringToObject(resp, "message", "pong");
            http_send_json_response(conn, resp);
            cJSON_Delete(resp);
        } else if (strcmp(path, "/api/status") == 0) {
            cJSON *resp = cJSON_CreateObject();
            bool eth_up = netif_is_link_up(&gnetif);
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON_AddBoolToObject(resp, "eth_up", eth_up);
            cJSON_AddNumberToObject(resp, "led_state", HAL_GPIO_ReadPin(LD1_PORT, LD1_PIN));
            cJSON_AddBoolToObject(resp, "can_ok", true);
            cJSON_AddBoolToObject(resp, "teensy_alive", g_can_feedback.updated);
            
            cJSON *can_data = cJSON_CreateObject();
            cJSON_AddNumberToObject(can_data, "hall", g_can_feedback.hall_feedback);
            cJSON_AddNumberToObject(can_data, "i_comp", g_can_feedback.current_comp);
            cJSON_AddNumberToObject(can_data, "i_exh", g_can_feedback.current_exh);
            cJSON_AddNumberToObject(can_data, "i_damp1", g_can_feedback.current_damp1);
            cJSON_AddNumberToObject(can_data, "i_damp2", g_can_feedback.current_damp2);
            cJSON_AddNumberToObject(can_data, "i_damp3", g_can_feedback.current_damp3);
            cJSON_AddNumberToObject(can_data, "i_damp4", g_can_feedback.current_damp4);
            cJSON_AddNumberToObject(can_data, "v_fb", g_can_feedback.voltage_fb);
            cJSON_AddNumberToObject(can_data, "i_fb", g_can_feedback.current_fb);
            cJSON_AddItemToObject(resp, "can_feedback", can_data);

            g_can_feedback.updated = false; // reset flag
            
            http_send_json_response(conn, resp);
            cJSON_Delete(resp);
        } else if (strcmp(path, "/api/version") == 0) {
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "status", "ok");
            cJSON_AddStringToObject(resp, "version", "1.0.0");
            http_send_json_response(conn, resp);
            cJSON_Delete(resp);
        } else {
            http_send_error_response(conn, 404, "Not Found");
        }
    } else if (strcmp(method, "POST") == 0) {
        if (strlen(body) == 0) {
            http_send_error_response(conn, 400, "Missing Body");
        } else {
            cJSON *json = cJSON_Parse(body);
            if (!json) {
                http_send_error_response(conn, 400, "Invalid JSON");
            } else {
                if (strcmp(path, "/api/echo") == 0) {
                    cJSON *data = cJSON_GetObjectItem(json, "data");
                    cJSON *resp = cJSON_CreateObject();
                    cJSON_AddStringToObject(resp, "status", "ok");
                    if (data && cJSON_IsString(data)) {
                        cJSON_AddStringToObject(resp, "echo", data->valuestring);
                    } else {
                        cJSON_AddStringToObject(resp, "error", "Missing data field");
                    }
                    http_send_json_response(conn, resp);
                    cJSON_Delete(resp);
                } else if (strcmp(path, "/api/led") == 0) {
                    cJSON *action = cJSON_GetObjectItem(json, "action");
                    if (action && cJSON_IsString(action)) {
                        if (strcmp(action->valuestring, "on") == 0) {
                            HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_SET);
                        } else if (strcmp(action->valuestring, "off") == 0) {
                            HAL_GPIO_WritePin(LD1_PORT, LD1_PIN, GPIO_PIN_RESET);
                        } else if (strcmp(action->valuestring, "toggle") == 0) {
                            HAL_GPIO_TogglePin(LD1_PORT, LD1_PIN);
                        }
                    }
                    cJSON *resp = cJSON_CreateObject();
                    cJSON_AddStringToObject(resp, "status", "ok");
                    cJSON_AddNumberToObject(resp, "led_state", HAL_GPIO_ReadPin(LD1_PORT, LD1_PIN));
                    http_send_json_response(conn, resp);
                    cJSON_Delete(resp);
                } else if (strcmp(path, "/api/can") == 0) {
                    /* Expected JSON: {"p1": 0, "p2": 0, "temp": 0, "lf_dc": 0, "rf_dc": 0, "lr_dc": 0, "rr_dc": 0} */
                    uint16_t p1 = 0, p2 = 0, temp = 0, lf_dc = 0, rf_dc = 0, lr_dc = 0, rr_dc = 0;
                    cJSON *item;
                    if ((item = cJSON_GetObjectItem(json, "p1"))) p1 = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "p2"))) p2 = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "temp"))) temp = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "lf_dc"))) lf_dc = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "rf_dc"))) rf_dc = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "lr_dc"))) lr_dc = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "rr_dc"))) rr_dc = item->valueint;
                    
                    bool ok = CAN_Send_Tester_CMD(p1, p2, temp, lf_dc, rf_dc, lr_dc, rr_dc);
                    
                    cJSON *resp = cJSON_CreateObject();
                    cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
                    if (!ok) cJSON_AddStringToObject(resp, "error", "CAN TX failed");
                    http_send_json_response(conn, resp);
                    cJSON_Delete(resp);
                } else if (strcmp(path, "/api/cfg") == 0) {
                    /* Expected JSON: {"lf_freq": 0, "rf_freq": 0, "lr_freq": 0, "rr_freq": 0, "ps_v": 0, "ps_i": 0, "flags1": 0, "flags2": 0} */
                    uint16_t lf_f = 0, rf_f = 0, lr_f = 0, rr_f = 0, ps_v = 0, ps_i = 0;
                    uint8_t f1 = 0, f2 = 0;
                    cJSON *item;
                    if ((item = cJSON_GetObjectItem(json, "lf_freq"))) lf_f = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "rf_freq"))) rf_f = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "lr_freq"))) lr_f = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "rr_freq"))) rr_f = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "ps_v"))) ps_v = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "ps_i"))) ps_i = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "flags1"))) f1 = item->valueint;
                    if ((item = cJSON_GetObjectItem(json, "flags2"))) f2 = item->valueint;
                    
                    bool ok = CAN_Send_Tester_CFG(lf_f, rf_f, lr_f, rr_f, ps_v, ps_i, f1, f2);
                    
                    cJSON *resp = cJSON_CreateObject();
                    cJSON_AddStringToObject(resp, "status", ok ? "ok" : "error");
                    if (!ok) cJSON_AddStringToObject(resp, "error", "CAN TX failed");
                    http_send_json_response(conn, resp);
                    cJSON_Delete(resp);
                } else {
                    http_send_error_response(conn, 404, "Not Found");
                }
                cJSON_Delete(json);
            }
        }
    } else {
        http_send_error_response(conn, 405, "Method Not Allowed");
    }
}

static void http_server_serve(struct netconn *conn) {
    struct netbuf *inbuf;
    err_t err;
    char *full_req = NULL;
    u16_t full_len = 0;

    netconn_set_recvtimeout(conn, 2000);

    /* Loop to receive all fragments of the request */
    while ((err = netconn_recv(conn, &inbuf)) == ERR_OK) {
        char *data;
        u16_t len;
        netbuf_data(inbuf, (void**)&data, &len);

        char *new_req = pvPortMalloc(full_len + len + 1);
        if (new_req) {
            if (full_req) {
                memcpy(new_req, full_req, full_len);
                vPortFree(full_req);
            }
            memcpy(new_req + full_len, data, len);
            full_len += len;
            new_req[full_len] = '\0';
            full_req = new_req;
        }
        netbuf_delete(inbuf);

        /* Simple check: do we have headers and if it's POST, do we have the body? */
        char *body_start = strstr(full_req, "\r\n\r\n");
        if (body_start) {
            char *cl_header = strstr(full_req, "Content-Length:");
            if (cl_header) {
                int content_len = 0;
                sscanf(cl_header, "Content-Length: %d", &content_len);
                int current_body_len = strlen(body_start + 4);
                if (current_body_len >= content_len) break;
            } else {
                /* GET request or POST without Content-Length (unlikely for our GUI) */
                break;
            }
        }
        
        /* If we are here, we need more data or haven't found headers yet */
    }

    if (full_req) {
        http_handle_request(conn, full_req, full_len);
        vPortFree(full_req);
    }
    
    osDelay(10); /* Flush TCP */
    netconn_close(conn);
}

static void http_server_thread(void *arg) {
    struct netconn *conn, *newconn;
    err_t err;

    /* Initialize cJSON hooks */
    cJSON_Hooks hooks = { .malloc_fn = custom_malloc, .free_fn = custom_free };
    cJSON_InitHooks(&hooks);

    conn = netconn_new(NETCONN_TCP);
    if (conn != NULL) {
        err = netconn_bind(conn, NULL, HTTP_PORT);

        if (err == ERR_OK) {
            netconn_listen(conn);
            printf("HTTP: Server listening on port %d...\r\n", HTTP_PORT);

            while (1) {
                err = netconn_accept(conn, &newconn);
                if (err == ERR_OK) {
                    printf("HTTP: Connection accepted!\r\n");
                    http_server_serve(newconn);
                    netconn_delete(newconn);
                } else {
                    printf("HTTP: Accept error: %d\r\n", err);
                    osDelay(10);
                }
            }
        } else {
            printf("HTTP: Bind error: %d\r\n", err);
            netconn_delete(conn);
        }
    } else {
        printf("HTTP: Failed to create netconn\r\n");
    }
    for (;;) {
        osDelay(1000);
    }
}

osThreadId_t httpServerTaskHandle;
const osThreadAttr_t httpServerTask_attributes = {
  .name = "httpServerTask",
  .stack_size = 4096,
  .priority = (osPriority_t) osPriorityNormal,
};

void http_server_init(void) {
    httpServerTaskHandle = osThreadNew(http_server_thread, NULL, &httpServerTask_attributes);
}
