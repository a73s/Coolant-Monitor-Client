#include <stdio.h>

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "lwip/sockets.h"
#include "esp_log.h"

#include "network.h"

static const char *TAG = "WIFI";
static EventGroupHandle_t wifi_event_group;
static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG, "Connecting to AP...");
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < MAX_FAILURES) {
			ESP_LOGI(TAG, "Reconnecting to AP...");
			esp_wifi_connect();
			s_retry_num++;
		} else {
			xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
		}
	}
}

//event handler for ip events
static void ip_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
	}

}

esp_err_t connectWifi(struct wifiCridentials * wifiCrids) {

	int status = WIFI_FAILURE;

	//initialize the esp network interface
	ESP_ERROR_CHECK(esp_netif_init());

	//create wifi station in the wifi driver
	esp_netif_create_default_wifi_sta();

	//setup wifi station with the default wifi configuration
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	wifi_event_group = xEventGroupCreate();

	esp_event_handler_instance_t wifi_handler_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_handler_event_instance));

	esp_event_handler_instance_t got_ip_event_instance;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL, &got_ip_event_instance));

	// set the wifi controller to be a station
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	/** START THE WIFI DRIVER **/
	if(strcmp(wifiCrids->ssid, "") != 0 && strcmp(wifiCrids->passwd, "") != 0){

		wifi_config_t new_wifi_config = {
			.sta = {
				.threshold.authmode = WIFI_AUTH_WPA2_PSK,
				.pmf_cfg = {
					.capable = true,
					.required = false
				},
			},
		};

		//they are the same length so this should be fine
		strncpy((char*)new_wifi_config.sta.ssid, wifiCrids->ssid, 32);
		strncpy((char*)new_wifi_config.sta.password, wifiCrids->passwd, 64);

		// set the wifi config
		// this will be stored in the default NVS (non volotile storage) partition of the flash
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &new_wifi_config));
		
	}

	wifi_config_t  recovered_wifi_config;
	ESP_ERROR_CHECK(esp_wifi_get_config(WIFI_IF_STA, &recovered_wifi_config));

	if(strcmp((char*)recovered_wifi_config.sta.ssid, "") == 0){

		return ESP_ERR_WIFI_SSID;
	}
	if(strcmp((char*)recovered_wifi_config.sta.password, "") == 0){

		return ESP_ERR_WIFI_PASSWORD;
	}

	// start the wifi driver
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "STA initialization complete");

	/** NOW WE WAIT **/
	EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
		WIFI_SUCCESS | WIFI_FAILURE,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_SUCCESS) {
		ESP_LOGI(TAG, "Connected to ap");
		status = WIFI_SUCCESS;
	} else if (bits & WIFI_FAILURE) {
		ESP_LOGI(TAG, "Failed to connect to ap");
		status = WIFI_FAILURE;
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
		status = WIFI_FAILURE;
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
	vEventGroupDelete(wifi_event_group);
	// ESP_ERROR_CHECK(esp_event_loop_delete_default());

	return status;
}

// connect to the server and return the result
esp_err_t connect_tcp_server(int *ret_sockHandle, uint32_t * network_order_ip, short int network_order_port)
{
	struct sockaddr_in serverInfo = {0};
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = *network_order_ip;
	serverInfo.sin_port = network_order_port;

	*ret_sockHandle = socket(AF_INET, SOCK_STREAM, 0);

	if (*ret_sockHandle < 0){
		ESP_LOGE(TAG, "Failed to create a socket..?");
		return TCP_FAILURE;
	}

	bool connected = false;

	connected = connect(*ret_sockHandle, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) == 0;

	if (!connected){
		ESP_LOGE(TAG, "Failed to connect to %s!", inet_ntoa(serverInfo.sin_addr.s_addr));

		close(*ret_sockHandle);
		return TCP_FAILURE;
	}

	ESP_LOGI(TAG, "Connected to TCP server.");
	return TCP_SUCCESS;
}

void mdns_print_result(mdns_result_t * result){

	static const char * ip_protocol_str[] = {"V4", "V6", "MAX"};

	mdns_result_t * tmpResult = result;
	mdns_ip_addr_t * tmpAddr = NULL;
	int i = 1, t;
	printf("%d: Type: %s\n / %i", i++, ip_protocol_str[tmpResult->ip_protocol], tmpResult->ip_protocol);
	if(tmpResult->instance_name != NULL){
		printf("  PTR : %s\n", tmpResult->instance_name);
	}
	if(tmpResult->hostname != NULL){
		printf("  SRV : %s.local:%u\n", tmpResult->hostname, tmpResult->port);
	}
	if(tmpResult->txt_count != 0){
		printf("  TXT : [%u] ", tmpResult->txt_count);
		for(t=0; t<tmpResult->txt_count; t++){
			printf("%s=%s; ", tmpResult->txt[t].key, tmpResult->txt[t].value);
		}
		printf("\n");
	}
	tmpAddr = tmpResult->addr;
	while(tmpAddr != NULL){
		if(tmpAddr->addr.type == IPADDR_TYPE_V6){
			printf("  AAAA: " IPV6STR "\n", IPV62STR(tmpAddr->addr.u_addr.ip6));
		} else {
			printf("  A   : " IPSTR "\n", IP2STR(&(tmpAddr->addr.u_addr.ip4)));
		}
		tmpAddr = tmpAddr->next;
	}
}

