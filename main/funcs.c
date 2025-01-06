#include <stdio.h>

#include "funcs.h"

#include "driver/gptimer.h"
#include "esp_err.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

double tempSPI(spi_device_handle_t devHandle){

	uint16_t data;
	spi_transaction_t transaction = {

		.tx_buffer = NULL,
		.rx_buffer = &data,
		.length = 32,
		.rxlength = 32,
	};

	ESP_ERROR_CHECK(spi_device_polling_transmit(devHandle, &transaction));

	int32_t res = (int32_t) SPI_SWAP_DATA_RX(data, 32);

	int32_t thermocouple = res >> 18;

	if(res & ((1 << 2) | (1 << 1) | (1 << 0)))
		printf("Sensor is not connected\n");
	else{


		if(thermocouple & (1<<13)){
			thermocouple ^= ((1<<13) & (1<<31));
		}

		// int32_t internal = (res << 16) >> 20;
	}

	fflush(stdout);
	return thermocouple * 0.25;
}

int multisamplePressureADC(adc_oneshot_unit_handle_t adcHandle, adc_cali_handle_t adcCaliHandle, adc_channel_t adcChan){

	int average = 0;

	for(int i = 0; i < 50; i++){

		int output = 0;
		esp_err_t ret = adc_oneshot_read(adcHandle, adcChan, &output);

		ESP_ERROR_CHECK(ret);
		average += output;
	}

	average /= 50;
	int voltage = 0;
	esp_err_t ret = adc_cali_raw_to_voltage(adcCaliHandle, average, &voltage);
	ESP_ERROR_CHECK(ret);
	return voltage;
	// return average*2450/4095;
}

double takeGPM(pcnt_channel_handle_t pcntChan, pcnt_unit_handle_t pcntUnit){

	static bool isFirstRun = true;

	static gptimer_config_t timerCfg = {

		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1 * 1000 * 1000,
	};

	static gptimer_handle_t timer = NULL;

	esp_err_t err;

	if(isFirstRun){

		err = gptimer_new_timer(&timerCfg, &timer);
		ESP_ERROR_CHECK(err);
		err = gptimer_enable(timer);
		ESP_ERROR_CHECK(err);
		err = gptimer_start(timer);
		ESP_ERROR_CHECK(err);

		isFirstRun = false;
		return -1;
	}

	#define CLOCK_COUNTS_PER_SECOND 1000000.0

	unsigned long long timerCount = 0;

	err = gptimer_get_raw_count(timer, &timerCount);
	ESP_ERROR_CHECK(err);

	if(timerCount < (int)CLOCK_COUNTS_PER_SECOND){

		vTaskDelay((((int) CLOCK_COUNTS_PER_SECOND - timerCount + 10) / 1000 )/ portTICK_PERIOD_MS);//wait for at least 1 seconds worth of data
	}

	err = gptimer_get_raw_count(timer, &timerCount);
	ESP_ERROR_CHECK(err);

	err = gptimer_set_raw_count(timer, 0);
	ESP_ERROR_CHECK(err);

	double seconds = timerCount / CLOCK_COUNTS_PER_SECOND;

	int pulseCount = 0;

	err = pcnt_unit_get_count(pcntUnit, &pulseCount);
	ESP_ERROR_CHECK(err);

	err = pcnt_unit_clear_count(pcntUnit);
	ESP_ERROR_CHECK(err);

	double pulsesPerSec = pulseCount/seconds;
	printf("Pulses Per Second: %lf\n", pulsesPerSec);
	printf("pulseCount: %i, timerCount: %llu, seconds: %lf, GPM: %lf\n", pulseCount, timerCount, seconds, pulsesPerSec*0.2642/10.0);
	fflush(stdout);
	return pulsesPerSec;
}

// float basic_lerp(const float M, const float X, const float B){
// 	return M*X + B;
// }

float basic_lerp(const float X, const struct lerpSpec spec){
	return spec.M*X + spec.B;
}
