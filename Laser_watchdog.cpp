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


//The #defines found below are macros that are used to structure the different type of log messages that this program prints

//The PRINT_MSG macro is used to print messages to a log file where we will keep track of the watchdog and initializations of GPIO pins.
#define PRINT_MSG(file, time1, programName, sev, str) \
	do{ \
			fprintf(logFile, "%s : %s : %s : %s", time1, programName, sev, str); \
			fflush(logFile); \
	}while(0)
//PRINT_MSG1 is used to write the inaugural message to a Stats file in order to know when it is first written to.
#define PRINT_MSG1(file, time1, programName, sev, str) \
	do{ \
			fprintf(statsFile, "%s : %s : %s : %s ", time1, programName, sev, str); \
			fflush(statsFile); \
	}while(0)
//PRINT_MSG2 writes to the Stats file and updates when each laser was broken and then outputs the total amount of times each laser was broken. The amount of times an object "enters" or "exits" is also printed at the end.
#define PRINT_MSG2(file, time1, programName, sev, str, point) \
	do{ \
			fprintf(statsFile, "%s : %s : %s : %s : %d \n\n", time1, programName, sev, str, *point); \
			fflush(statsFile); \
	}while(0)

//HARDWARE DEPENDENT CODE

//initializeGPIO is used to initialize all of the GPIO pins on the Raspberry Pi.
GPIO_Handle initializeGPIO() {
	GPIO_Handle gpio;
	gpio = gpiolib_init_gpio();
	if (gpio == NULL) {
		perror("Could not initialize GPIO");
	}
	return gpio;
}

//Declaring an enum that will be used to read a config file.
enum State { START, EQUALS, NOT_EQUALS, TIMEOUT, LOGFILE, STATSFILE };
//The readConfig function uses a State Machine to read the necessary information (watchdog timer, and directory of the log and stats file that will be later written to) from a config file.
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
						}
						++i;
					}
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
			}
		}
	}
}

//Checks the status of each laser by detecting if the corresponding photodiode receives light from the laser. If the photodiode does not receive light
// it does not produce a voltage and the input pin from the Raspberry Pi does not detect an input voltage.

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

int main(const int argc, const char* const argv[]) {
	enum State { START, LEFT, LEFT_BOTH, LEFT_RIGHT, OUT, RIGHT, RIGHT_BOTH, RIGHT_LEFT, IN, NONE };
	enum State s = START;

	int laser1Count = 0;
	int* a = &laser1Count;

	int laser2Count = 0;
	int* b = &laser2Count;

	int numberIn = 0;
	int* c = &numberIn;

	int numberOut = 0;
	int* d = &numberOut;

	//Create a string that contains the program name
	const char* argName = argv[0];

	//These variables will be used to count how long the name of the program is
	int i = 0;
	int namelength = 0;

	while (argName[i] != 0)
	{
		namelength++;
		i++;
	}

	char programName[namelength];

	i = 0;

	//Copy the name of the program without the ./ at the start
	//of argv[0]
	while (argName[i + 2] != 0)
	{
		programName[i] = argName[i + 2];
		i++;
	}

	//Create a file pointer named configFile
	FILE* configFile;
	//Set configFile to point to the Lab4Sample.cfg file. It is
	//set to read the file.
	configFile = fopen("/home/pi/Lab4Sample.cfg", "r");

	//Output a warning message if the file cannot be openned
	if (!configFile)
	{
		perror("The config file could not be opened");
		return -1;
	}

	//Declare the variables that will be passed to the readConfig function
	int timeout;
	char logFileName[50];
	char statsFileName[50];

	//Call the readConfig function to read from the config file
	readConfig(configFile, &timeout, logFileName, statsFileName);

	//Close the configFile now that we have finished reading from it
	fclose(configFile);

	//Creates a pointer called logFile that points to the logFile.
	FILE* logFile;

	logFile = fopen(logFileName, "a");

	//Creates a pointer called statsFile that points to the statsFile.
	FILE* statsFile;

	statsFile = fopen(statsFileName, "a");

	//Check that the file opens properly.
	if (!configFile)
	{
		perror("The log file could not be opened");
		return -1;
	}

	//Start of writing messages to log file.

	char time1[30];
	getTime(time1);

	getTime(time1);
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The program is running\n\n");

	GPIO_Handle gpio = initializeGPIO();

	getTime(time1);

	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The GPIO pins have been initialized\n\n");

	uint32_t sel_reg = gpiolib_read_reg(gpio, GPFSEL(1));
	sel_reg &= ~(1 << 18);
	gpiolib_write_reg(gpio, GPFSEL(1), sel_reg);

	getTime(time1);

	PRINT_MSG(logFile, time1, programName, "sev = Info ", "Pin 16 has been set as output\n\n");

	if (argc != 2) {
		PRINT_MSG(logFile, time1, programName, "sev = Error ", "Invalid number of arguments\n\n");
		errorMessage(-1);
		return -1;
	}

	//End of writing to log file.

	//Watchdog initialization
	int watchdog;
	if ((watchdog = open("/dev/watchdog", O_RDWR | O_NOCTTY)) < 0) {
		printf("Error: Couldn't open watchdog device! %d\n", watchdog);
		return -1;
	}
	getTime(time1);
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The Watchdog file has been opened\n\n");
	ioctl(watchdog, WDIOC_SETTIMEOUT, &timeout);
	getTime(time1);
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The Watchdog time limit has been set\n\n");
	ioctl(watchdog, WDIOC_GETTIMEOUT, &timeout);
	printf("The watchdog timeout is %d seconds.\n\n", timeout);
	//

	time_t startTime = time(NULL);
	int timeLimit = atoi(argv[1]);

	getTime(time1);
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "User has entered a valid time\n\n");


	//INSERT STATE MACHINE HERE


	outputMessage(laser1Count, laser2Count, numberIn, numberOut);

	getTime(time1);
	PRINT_MSG1(statsFile, time1, programName, "sev = Info ", "Stats have been updated\n\n");

	getTime(time1);
	PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Number of times laser 1 was broken: ", a);
	getTime(time1);
	PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Number of times laser 2 was broken: ", b);
	getTime(time1);
	PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Number of objects entered the room: ", c);
	getTime(time1);
	PRINT_MSG2(statsFile, time1, programName, "sev = Info ", "Number of objects exitted the room: ", d);


	write(watchdog, "V", 1);
	getTime(time1);
	//Log that the Watchdog was disabled
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The Watchdog was disabled\n\n");

	//Close the watchdog file so that it is not accidentally tampered with
	close(watchdog);
	getTime(time1);
	//Log that the Watchdog was closed
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The Watchdog was closed\n\n");

	//Free the gpio pins
	gpiolib_free_gpio(gpio);
	getTime(time1);
	//Log that the GPIO pins were freed
	PRINT_MSG(logFile, time1, programName, "sev = Info ", "The GPIO pins have been freed\n\n");

	return 0;
}


#endif

