/*
 * author: Adam Seals
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// #include "cc.h"
#include "network.h"
#include "funcs.h"
#include "command.h"
#include "config.h"

#include "esp_wifi.h"
#include "inttypes.h"
#include "stddef.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/adc_types_legacy.h"
#include "lwip/sockets.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "MAIN";

void app_main(void){

	esp_err_t ret;

	//======= INITIALIZE STORAGE =======
	//This is used by the wifi driver to store cridentials

	printf("Initializing flash storage...\n");
	fflush(stdout);

	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		printf("Erasing flash\n");
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}else{
		printf("Some other error with nvs: %i\n", ret);
	}

	nvs_handle_t nvsHandle;
	nvs_open("storage", NVS_READWRITE, &nvsHandle);

	ESP_ERROR_CHECK(ret);

	fflush(stdin);
	printf("Enter something in the next 5 seconds to enter command mode.\n");
	fflush(stdout);

	vTaskDelay(5000/portTICK_PERIOD_MS);

	char tmp[512]="";
	// scanf("%s", tmp);
	getLineInput(tmp, 512);

	struct wifiCridentials wifiCrids = {
		.ssid = "",
		.passwd = ""
	};

	if(strcmp(tmp, "") != 0){
		commandMode(&nvsHandle, &wifiCrids);
	}

	// ======= CONFIGURE SPI =======
	printf("Configuring SPI interface...\n");
	fflush(stdout);
	gpio_reset_pin(MISO);
	gpio_reset_pin(CLK);
	gpio_reset_pin(CS);

	gpio_set_direction(MISO, GPIO_MODE_INPUT);//set input on miso
	gpio_set_direction(CLK, GPIO_MODE_OUTPUT);//set clk to output
	gpio_set_direction(CS, GPIO_MODE_OUTPUT);

	spi_device_handle_t spi;

	spi_bus_config_t busCfg = {
		.miso_io_num = MISO,
		.mosi_io_num = MOSI,
		.sclk_io_num = CLK,
		.quadwp_io_num = -1,// NOTE: quadwp and quadhd are unused for us, docs say its used for "4 bit (qio/qout) transactions"
		.quadhd_io_num = -1,
		.max_transfer_sz = (4 * 8)
	};

	spi_device_interface_config_t devCfg = {
		.mode = 0,
		.clock_speed_hz = 4*1000*1000, // may be able to change to (up to) 4,300,000 (43*100*1000)
		.spics_io_num = CS,
		.queue_size = 3
	};

	ret = spi_bus_initialize(VSPI_HOST, &busCfg, DMA_CHAN); // TODO: figure out why VSPI_HOST is not in the enum called "spi_host_device_t"
	ESP_ERROR_CHECK(ret);
	ret = spi_bus_add_device(VSPI_HOST, &devCfg, &spi);
	ESP_ERROR_CHECK(ret);

	// ======= CONFIGURE ADC INPUT =======
	printf("Configuring pressure sensor...\n");
	fflush(stdout);
	adc_oneshot_unit_handle_t adcHandle;
	adc_oneshot_unit_init_cfg_t adcInitCfg = {

		.unit_id = ADC_UNIT_PRESSURE,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	adc_oneshot_chan_cfg_t adcCfg = {

		.atten = ADC_ATTENUATION_PRESSURE,
		.bitwidth = ADC_BITWIDTH_PRESSURE
	};

	ret = adc_oneshot_new_unit(&adcInitCfg, &adcHandle);
	ESP_ERROR_CHECK(ret);

	ret = adc_oneshot_config_channel(adcHandle, ADC_CHANNEL_PRESSURE, &adcCfg);
	ESP_ERROR_CHECK(ret);

	// 
	adc_cali_handle_t adcCaliHandle = NULL;
	adc_cali_line_fitting_config_t adcCaliConf = {
		.atten = ADC_ATTENUATION_PRESSURE,
		.unit_id = ADC_UNIT_PRESSURE,
		.bitwidth = ADC_BITWIDTH_PRESSURE,
		.default_vref = 0,
	};

	ret = adc_cali_create_scheme_line_fitting(&adcCaliConf, &adcCaliHandle);
	ESP_ERROR_CHECK(ret);

	// ======= CONFIGURE PULSE CONTER =======

	printf("Configuring flow sensor...\n");
	fflush(stdout);
	pcnt_unit_config_t pcntUnitConf = {

		.low_limit = -1,
		.high_limit = 32767, //basically no limit, I wont be using this
		.intr_priority = 0,
	};
	pcnt_unit_handle_t pcntUnit;

	ret = pcnt_new_unit(&pcntUnitConf, &pcntUnit);
	ESP_ERROR_CHECK(ret);

	pcnt_chan_config_t pcntChanConf = {

		.edge_gpio_num = FLOWMETER_PCNT_PIN,
		.flags.invert_edge_input = false,
	};
	pcnt_channel_handle_t pcntChan;

	ret = pcnt_new_channel(pcntUnit, &pcntChanConf, &pcntChan);
	ESP_ERROR_CHECK(ret);
	ret = pcnt_channel_set_edge_action(pcntChan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
	ESP_ERROR_CHECK(ret);

	pcnt_glitch_filter_config_t pcntGlitchCfg = {

		.max_glitch_ns = 0x2000,

		/*
			max frequency of sensor in Hz is:
			1/(max_glitch_ns * 10^-9 *2)
			must make sure that this can accomidate the maximum expected flow reading of the system

			with max_glitch_ns = 0x2000 = 8192
			f_max = ~61 KHz ... Or approximately 20,000 Rotations/Second
			im pretty sure this is far beyond the physical capabilities of the sensor
		*/
	};

	ret = pcnt_unit_set_glitch_filter(pcntUnit, &pcntGlitchCfg);
	ESP_ERROR_CHECK(ret);
	ret = pcnt_unit_enable(pcntUnit);
	ESP_ERROR_CHECK(ret);
	ret = pcnt_unit_start(pcntUnit);
	ESP_ERROR_CHECK(ret);

	//======= CONNECT TO/INITIALIZE WIFI & CONNECT TO TCP SERVER =======

	printf("Connecting to WIFI...\n");
	fflush(stdout);
	esp_err_t status = WIFI_FAILURE;

	//initialize default esp event loop
	ESP_ERROR_CHECK(esp_event_loop_create_default()); // this is required for connectWifi()
	// connect to wireless AP
	status = connectWifi(&wifiCrids);

	if(status == ESP_ERR_WIFI_SSID){

		printf("Wifi SSID field empty. Please reboot and enter them in command mode.\n");
		abort();
	}
	if(status == ESP_ERR_WIFI_PASSWORD){

		printf("Wifi password field empty. Please reboot and enter them in command mode.\n");
		abort();
	}
	if(WIFI_SUCCESS != status) {
		ESP_LOGI(TAG, "Failed to associate to AP, dying...\n");
		abort();
	}
	
	printf("Connecting to server...\n");
	fflush(stdout);

	// == RESOLVE IP OVER mDNS ==

	ret = mdns_init();

	mdns_result_t * result = NULL;

	do{
		ret = mdns_query_ptr(MDNS_SERVICE_TYPENAME, "_tcp", 3000, 1,  &result);
		if(ret){
			printf("mDNS query Failed, Trying again.\n");
		}
		if(!result){
			printf("No results found through mDNS! Trying again.\n");
		}
	}while(ret || !result);

	printf("Connecting to the following server:\n");
	mdns_print_result(result);

	int socketfd = 0;

	uint32_t address = result->addr->next->addr.u_addr.ip4.addr;

	printf("ip4 address: %"PRIu32" \n", address);
	status = connect_tcp_server(&socketfd, &address, htons(result->port));
	mdns_query_results_free(result);

	if(TCP_SUCCESS != status){
		ESP_LOGI(TAG, "Failed to connect to remote server, dying...\n");
		abort();
	}

	// Restore Device ID (assigned from server), from nvs
	uint32_t deviceID;
	uint32_t recvBuff[2];
	char sendBuff[64];

	ret = nvs_get_u32(nvsHandle, "devID", &deviceID);
	printf("ID from nvs: %"PRIu32"\n", deviceID);

	switch(ret){

		case ESP_OK: break;
		case ESP_ERR_NVS_NOT_FOUND: {

			deviceID = 0;
			break;
		}
		default: {
			printf("WTF, default case on nvs?\n");
			abort();
			break;
		}
	}

	sprintf(sendBuff, "<%"PRIu32"\n", deviceID);

	// send existing id to server
	write(socketfd, sendBuff, strlen(sendBuff));
	printf("ID to server: %s\n", sendBuff);

	// receive a new ID or an echo of the current one
	assert(read(socketfd, recvBuff, 4) == 4);

	printf("ID from server: %"PRIu32"\n", *recvBuff);

	ret = nvs_set_u32(nvsHandle, "devID", *recvBuff);
	ESP_ERROR_CHECK(ret);

	while(1){
	// for(int i = 0; i < 10; i++){

		fflush(stdout);

		union temperature {

			float ftemp;
			uint32_t itemp;
		}temp;

		// short pressureMv = multisamplePressureADC(adcHandle, ADC_CHANNEL_PRESSURE);

		// short pressureMv = multisamplePressureADC(adcHandle, adcCaliHandle, ADC_CHANNEL_PRESSURE);
		// printf("Pressure: %i mv\n", pressureMv);
		// temp.ftemp = multisampleTempSPI(spi);
		// printf("Temp: %f\n", temp.ftemp);
		// float pps = takeGPM(pcntChan, pcntUnit);
		short pressureMv = 420;
		printf("Pressure: %"PRIi16" mv\n", pressureMv);
		temp.ftemp = 20.0192f;
		printf("Temp: %f\n", temp.ftemp);
		float pps = 5.232;

		// unsigned sendbuff[3] = {pressureMv, temp.itemp, pps};
		char sendbuff[256] = {0};
		sprintf(sendbuff, "<%i,%f,%f\n", pressureMv, temp.ftemp, pps);

		printf("%u\n", pressureMv);
		printf("%f\n", temp.ftemp);
		printf("%f\n", pps);

		printf("==================================================================================\n");
		fflush(stdout);

		send(socketfd, &sendbuff, strlen(sendbuff), 0);

		vTaskDelay(2000/portTICK_PERIOD_MS);
		// vTaskDelay(1);
	}
	close(socketfd);
	abort();
}
