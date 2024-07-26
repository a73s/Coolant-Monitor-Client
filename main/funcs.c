#include <stdio.h>

#include "funcs.h"

#include "driver/gptimer.h"
#include "esp_err.h"
#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

double multisampleTempSPI(spi_device_handle_t devHandle){
	double cumulative = 0;

	for(int i = 0; i < 10; i++){
		uint16_t data;
		spi_transaction_t transaction = {

			.tx_buffer = NULL,
			.rx_buffer = &data,
			.length = 16,
			.rxlength = 16,
		};

		ESP_ERROR_CHECK(spi_device_polling_transmit(devHandle, &transaction));

		int16_t res = (int16_t) SPI_SWAP_DATA_RX(data, 16);

		if(res & (1 << 2))
			printf("Sensor is not connected\n");
		else{
			res >>=3; // this will push out the bottom 3 bits, these are "Thermocouple inputs", "Device ID", and "State"
			cumulative += res * 0.25;
		}

		fflush(stdout);

		vTaskDelay(500/portTICK_PERIOD_MS);
	}
	return cumulative/10;
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
	printf("pulseCount: %i, timerCount: %llu, seconds: %lf\n", pulseCount, timerCount, seconds);
	fflush(stdout);
	return pulsesPerSec;
}

