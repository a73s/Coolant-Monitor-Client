#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "mdns.h"
// #include "lwip/mdns.h"

struct wifiCridentials {
	char ssid[32];
	char passwd[64];
};

#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define TCP_SUCCESS 1 << 0
#define TCP_FAILURE 1 << 1
#define MAX_FAILURES 10

esp_err_t connectWifi(struct wifiCridentials * wifiCrids);

//these are defined in funcs.c
//static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
//static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

esp_err_t connect_tcp_server(int *ret_sockHandle, uint32_t * network_order_ip, short int network_order_port);

esp_err_t readFromTcpServer(int sockHandle, char * readBuff, int sizeOfBuff, int * ret_readLength);

esp_err_t sendToTcpServer(int sockHandle, char * sendStr, int sendSizeB);

void mdns_print_result(mdns_result_t * result);
