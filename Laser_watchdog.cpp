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
