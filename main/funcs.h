#pragma once

#include <stdio.h>

#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

struct lerpSpec{
	float M;
	float B;
};

double tempSPI(spi_device_handle_t devHandle);

int multisamplePressureADC(adc_oneshot_unit_handle_t adcHandle, adc_cali_handle_t adcCaliHandle, adc_channel_t adcChan);

double takeGPM(pcnt_channel_handle_t pcntChan, pcnt_unit_handle_t pcntUnit);

// float basic_lerp(const float M, const float X, const float B);
float basic_lerp(const float X, const struct lerpSpec spec);
