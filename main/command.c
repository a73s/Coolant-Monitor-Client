#include <stdio.h>
#include <string.h>

#include "funcs.h"
#include "network.h"
#include "config.h"

#include "nvs.h"

void getLineInput(char buf[], size_t len){

	memset(buf, 0, len);
	fflush(stdin); //clears any junk in stdin
	char *bufp;
	bufp = buf;

	while(true){

		vTaskDelay(100/portTICK_PERIOD_MS);
		*bufp = getchar();
		if(*bufp != '\0' && (unsigned char)*bufp != 0xff && *bufp != '\r'){ //ignores null input, 0xFF, CR in CRLF
			
			if(*bufp != '\b') printf("%c",*bufp);
			//'enter' (EOL) handler
			if(*bufp == '\n'){
				*bufp = '\0';
				break;
			} //backspace handler
			else if (*bufp == '\b'){
				if(bufp-buf >= 1){
					bufp--;
					printf("\b \b");
				}
			}
			else{
				//pointer to next character
				bufp++;
			}
			fflush(stdout);
		}
		
		//only accept len-1 characters, (len) character being null terminator.
		if(bufp-buf > (len)-2){
			bufp = buf + (len -1);
			*bufp = '\0';
			break;
		}
	}
}

void printHelp(){

	printf("help - print this command\n");
	printf("wifi - enter wifi cridentials\n");
	printf("calibrate-flow - Set the multiplier for the flow rate sensor. output = Hz * multiplier\n");
	printf("calibrate-pressure - Calibrate the pressure sensor linearly. You will be asked to enter M and B, the output of the sensor will be M*x+B, x being the original reading.\n");
	printf("calibrate-temperature - Calibrate the temperature sensor linearly. You will be asked to enter M and B, the output of the sensor will be M*x+B, x being the original reading.\n");
	printf("exit - exit command mode\n");
}

void commandMode(nvs_handle_t * nvsHandle, struct wifiCridentials * wifiCrids){

	printf("####### COMMAND MODE #######\n");
	printHelp();

	const uint8_t maxLen = 64;
	char command[maxLen];
	memset(command, 0, maxLen);

	do{
		printf("\n>> ");
		fflush(stdout);
		getLineInput(command, maxLen);

		if(strcmp(command, "help") == 0){
			printHelp();
		}
		else if(strcmp(command, "wifi") == 0){

			//these sizes are set to be consistent with the max sizes in wifi_config_t
			#define ssidlen 32
			#define passlen 64
			unsigned char ssid[ssidlen] = {0};
			unsigned char pass[passlen] = {0};

			printf("\nEnter wifi SSID>> ");
			fflush(stdout);
			getLineInput((char*) ssid, ssidlen);
			printf("\nEnter wifi password>> ");
			fflush(stdout);
			getLineInput((char*) pass, passlen);

			strncpy(wifiCrids->ssid,(char*) ssid, ssidlen);
			strncpy(wifiCrids->passwd,(char*) pass, passlen);

		}else if(strcmp(command, "calibrate-pressure") == 0){

			char tmpline[64] = {0};

			float M = 0;
			do{
				printf("\nEnter M (decimal): ");
				fflush(stdout);
				getLineInput(tmpline, 64);
				M = atof(tmpline);

				if(M == 0){
					printf("\nEntry failed, Please try again.");
					fflush(stdout);
				}
			}while(M == 0);

			float B = 0;
			do{
				printf("\nEnter B (decimal): ");
				fflush(stdout);
				getLineInput(tmpline, 64);
				B = atof(tmpline);

				if(B == 0){
					printf("\nEntry failed, Please try again.");
					fflush(stdout);
				}
			}while(B == 0);

			printf("\nWould You like to apply M = %f and B = %f? (Y/N): ", M, B);
			fflush(stdout);
			getLineInput(tmpline, 64);

			if(strcmp(tmpline, "Y") || strcmp(tmpline, "y")){

				esp_err_t ret = nvs_set_blob(*nvsHandle, "pressure_B", &B, sizeof(float));
				esp_err_t ret2 = nvs_set_blob(*nvsHandle, "pressure_M", &M, sizeof(float));
				printf("\nValues applied. returns: %i, %i", ret, ret2);
				fflush(stdout);
			}else{
				printf("\nValues not applied.");
				fflush(stdout);
			}

		}else if(strcmp(command, "calibrate-temperature") == 0){

			char tmpline[64] = {0};

			float M = 0;
			do{
				printf("\nEnter M (decimal): ");
				fflush(stdout);
				getLineInput(tmpline, 64);
				M = atof(tmpline);

				if(M == 0){
					printf("\nEntry failed, Please try again.");
					fflush(stdout);
				}
			}while(M == 0);

			float B = 0;
			do{
				printf("\nEnter B (decimal): ");
				fflush(stdout);
				getLineInput(tmpline, 64);
				B = atof(tmpline);

				if(B == 0){
					printf("\nEntry failed, Please try again.");
					fflush(stdout);
				}
			}while(B == 0);

			printf("\nWould You like to apply M = %f and B = %f? (Y/N): ", M, B);
			fflush(stdout);
			getLineInput(tmpline, 64);

			if(strcmp(tmpline, "Y") || strcmp(tmpline, "y")){

				esp_err_t ret = nvs_set_blob(*nvsHandle, "temperature_B", &B, sizeof(float));
				esp_err_t ret2 = nvs_set_blob(*nvsHandle, "temperature_M", &M, sizeof(float));
				printf("\nValues applied. returns: %i, %i", ret, ret2);
				fflush(stdout);
			}else{
				printf("\nValues not applied.");
				fflush(stdout);
			}

		}else if(strcmp(command, "calibrate-flow") == 0){

			char tmpline[64] = {0};

			float mult = 0;
			do{
				printf("\nEnter Multiplier (decimal): ");
				fflush(stdout);
				getLineInput(tmpline, 64);
				mult = atof(tmpline);

				if(mult == 0){
					printf("\nEntry failed, Please try again.");
					fflush(stdout);
				}
			}while(mult == 0);

			printf("\nWould You like to apply Multiplier = %f? (Y/N): ", mult);
			fflush(stdout);
			getLineInput(tmpline, 64);

			if(strcmp(tmpline, "Y") || strcmp(tmpline, "y")){

				esp_err_t ret = nvs_set_blob(*nvsHandle, "flow_multiplier", &mult, sizeof(float));
				printf("\nValues applied. returns: %i", ret);
				fflush(stdout);
			}else{
				printf("\nValues not applied.");
				fflush(stdout);
			}

		}else{

			printf("Console Error: Unknown Command \"%s\"\n", command);
			fflush(stdout);
		}

	}while(strcmp(command, "exit") != 0);
}

