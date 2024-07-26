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
	printf("wifi - enter wifi cridentials\n"); printf("calibrate-flow - calibrate flow sensor\n");
	printf("calibrate-adc - enter wifi cridentials\n");
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

		}else if(strcmp(command, "calibrate-adc") == 0){

			char tmpline[64] = {0};

			printf("\nApply <voltage>mV(relative to gnd) to D32 and enter 'c' to continue>>");
			fflush(stdout);
			getLineInput(tmpline, 64);

		}else{

			printf("Console Error: Unknown Command \"%s\"\n", command);
			fflush(stdout);
		}

	}while(strcmp(command, "exit") != 0);
}

