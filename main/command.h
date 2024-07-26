#pragma once

#include <stddef.h>

#include "nvs.h"

struct wifiCridentials;

void getLineInput(char buf[], size_t len);

void printHelp();

void commandMode(nvs_handle_t * nvsHandle, struct wifiCridentials * wifiCrids);

