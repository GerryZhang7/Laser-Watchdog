#include "gpiolib_addr.h"
#include "gpiolib_reg.c"
#include "gpiolib_reg.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>				//for the printf() function
#include <fcntl.h>
#include <linux/watchdog.h> 	//needed for the watchdog specific constants
#include <unistd.h> 			//needed for sleep
#include <sys/ioctl.h> 			//needed for the ioctl function
#include <stdlib.h> 			//for atoi
#include <time.h> 				//for time_t and the time() function
#include <sys/time.h>           //for gettimeofday()

#define PRINT_MSG(file, time1, programName, sev, str) \
	do{ \
			fprintf(logFile, "%s : %s : %s : %s", time1, programName, sev, str); \
			fflush(logFile); \
	}while(0)
#define PRINT_MSG1(file, time1, programName, sev, str) \
	do{ \
			fprintf(statsFile, "%s : %s : %s : %s ", time1, programName, sev, str); \
			fflush(statsFile); \
	}while(0)
#define PRINT_MSG2(file, time1, programName, sev, str, point) \
	do{ \
			fprintf(statsFile, "%s : %s : %s : %s : %d \n\n", time1, programName, sev, str, *point); \
			fflush(statsFile); \
	}while(0)

// #define PRINT_MSG2(file, time1, programName, sev, str, str, str) 
// PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Laser 1 was broken ", laser1Count " times \n\n");
// 	getTime(time1);
// 	PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Laser 2 was broken ", laser2Count " times \n\n");
// 	getTime(time1);
// 	PRINT_MSG3(statsFile, time1, programName, "sev = Info ", numberIn, " objects entered the room \n\n");
// 	getTime(time1);
// 	PRINT_MSG3(statsFile, time1, programName, "sev = Info ", numberOut, " objects exitted the room \n\n");

//HARDWARE DEPENDENT CODE

GPIO_Handle initializeGPIO() {
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if (gpio == NULL) {
		perror("Could not initialize GPIO");
	}
	return gpio;
}
enum State { START, EQUALS, NOT_EQUALS, TIMEOUT, LOGFILE, STATSFILE };
void readConfig(FILE* configFile, int* timeout, char* logFileName, char* statsFileName) {

	int i = 0;
	int j = 0;
	int k = 0;
	int parameter = 0;
	char buffer[255];

	enum State s = START;

	*timeout = 0;

	while (fgets(buffer, 255, configFile) != NULL) {
		i = 0;

		if (buffer[i] != '#') {
			while (buffer[i] != 0) {
				switch (s) {

				case START:
					if (buffer[i] == '=') {
						s = EQUALS;
					}
					else if (buffer[i] != '=') {
						s = NOT_EQUALS;
					}
					else {
						s = START;
					}
					++i;
					break;

				case NOT_EQUALS:
					if (buffer[i] != '=') {
						s = NOT_EQUALS;
					}
					else if (buffer[i] == '=') {
						s = EQUALS;
					}
					++i;
					break;

				case EQUALS:
					if (parameter == 0) {
						s = TIMEOUT;
					}
					else if (parameter == 1) {
						s = LOGFILE;
					}
					else if (parameter == 2) {
						s = STATSFILE;
					}
					//++i;
					break;

				case TIMEOUT:
					while (buffer[i] != 0) {
						if (buffer[i] >= '0' && buffer[i] <= '9')
						{
							//Move the previous digits up one position and add the
							//new digit
							*timeout = (*timeout * 10) + (buffer[i] - '0');
						}
						++i;
					}
					++parameter;
					s = NOT_EQUALS;
					break;

				case LOGFILE:
					j = 0;
					while (buffer[i] != 0 && buffer[i] != '\n')
					{
						//If the characters after the equal sign are not spaces or
						//equal signs, then it will add that character to the string
						if (buffer[i] != ' ' && buffer[i] != '=')
						{
							logFileName[j] = buffer[i];
							++j;
						}/*
						else {
							char defaultLog[24] = "/home/pi/defaultlog.log";
							for(int l = 0; l <= 24; l++){
								logFileName[j] = defaultLog[l];
								j++;
							}
						}*/
						++i;
					}
					/*	else {
						++i;
						s = NOT_EQUALS;
					}*/

					//Add a null terminator at the end
					logFileName[j] = 0;
					++parameter;
					s = NOT_EQUALS;

					break;

				case STATSFILE:
					k = 0;
					while (buffer[i] != 0 && buffer[i] != '\n')
					{
						//If the characters after the equal sign are not spaces or
						//equal signs, then it will add that character to the string
						if (buffer[i] != ' ' && buffer[i] != '=')
						{
							statsFileName[k] = buffer[i];
							++k;
						}
						/*else {
							char defaultStats[26] = "/home/pi/defaultstats.log";
							for(int l = 0; l <= 26; l++){
								statsFileName[k] = defaultStats[l];
								k++;
							}
						}*/
						++i;
					}
					//Add a null terminator at the end
					statsFileName[k] = 0;
					++parameter;
					s = NOT_EQUALS;

					break;

				default:
					break;

				}
				//++i;
			}
		}
	}
}

//Checking laser diode statuses for each diode; can probably just switch order to reverse left and right
#define LASER1_PIN_NUM 4 //Left diode
#define LASER2_PIN_NUM 6 //Right diode
int laserDiodeStatus(GPIO_Handle gpio, int diodeNumber) {
	if (gpio == NULL) {
		return -1;
	}

	if (diodeNumber == 1) {
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if (level_reg & (1 << LASER1_PIN_NUM)) {
			return 1;
		}
		else {
			return 0;
		}
	}
	else if (diodeNumber == 2) {
		uint32_t level_reg = gpiolib_read_reg(gpio, GPLEV(0));

		if (level_reg & (1 << LASER2_PIN_NUM)) {
			return 1;
		}
		else {
			return 0;
		}
	}
}

#endif

//END OF HARDWARE DEPENDENT CODE

//FINAL OUTPUT MESSAGE WITH RESULTS
void outputMessage(int laser1Count, int laser2Count, int numberIn, int numberOut) {
	printf("Laser 1 was broken %d times \n", laser1Count);
	printf("Laser 2 was broken %d times \n", laser2Count);
	printf("%d objects entered the room \n", numberIn);
	printf("%d objexts exitted the room \n", numberOut);
}