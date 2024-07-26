#pragma once

#include <stdio.h>

#include "driver/pulse_cnt.h"
#include "esp_adc/adc_oneshot.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

double multisampleTempSPI(spi_device_handle_t devHandle);

int multisamplePressureADC(adc_oneshot_unit_handle_t adcHandle, adc_cali_handle_t adcCaliHandle, adc_channel_t adcChan);

double takeGPM(pcnt_channel_handle_t pcntChan, pcnt_unit_handle_t pcntUnit);

