#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#endif

struct measure
{
	float temp;
	float humidity;
};

	float Tmin = -100;
	float Tmax = -100;
	int timeDelay = -1;

void printHelp(void)
{
	printf("  -h, --help\tПоказать эту справку и выйти\n");
	printf("  -L, --Tmin\tТемпература включения обогрева\n");
	printf("  -D, --delay\tВремя (в минутах) между замерами температуры.\n");
	printf("  -H, --Tmax\tТемпература выключения обогрева\n");
}

void prepareArgs(int argc, char *argv[])
{
	const char* short_options = "L:H:D:h";
	const struct option long_options[] = {
		{"Tmin", required_argument, 0, 'L'},
		{"Tmax", required_argument, 0, 'H'},
		{"delay", required_argument,0, 'D'},
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
		
	};

	int c;
	while ((c = getopt_long(argc, argv, short_options, long_options, NULL))!=-1){
		switch(c) {
			case 'L':
				Tmin = atof(optarg); 
				break;

			case 'H':
				Tmax = atof(optarg);
				break;

			case 'D':
				timeDelay = atoi(optarg);
				break;

			case 'h':
				printHelp();
				exit(0);
		}
	}
	if ((Tmin <= -50) || (Tmax >= 50) || (Tmin >= Tmax)){
		printf("Ошибка в параметрах. -50 < Tmin(Tmax) < +50; Tmin < Tmax\n");
		exit(-1);
	}
	if (timeDelay < 0){
		printf("Неуказано время между замерами\n");
		exit(-1);
	}
	printf("Options: Tmin = %.1fC Tmax = %.1fC Delay = %dmin\n", Tmin, Tmax, timeDelay);
}

void heater(char active)
{
	if (active)
		printf("Обогрев включен\n");
	else
		printf("Обогрев выключен\n");
}

int mi = 0;

struct measure getMeasure()
{
	struct measure result;
	if (mi > 100)	result.temp = mi - 100;
	else if (mi > 50) result.temp = 100 - mi;
	else result.temp = mi;
	result.humidity = 25.5;
	mi++;
	return result;
}

void getCurrentTime(char *buffer)
{
	time_t rawtime;
	struct tm * timeinfo;
	time (&rawtime);
		timeinfo = localtime (&rawtime);

	strftime(buffer, 80, "%d.%m.%Y %H:%M:%S", timeinfo);	
}

void writeMeasureToLog(const char* strTimeInfo, struct measure m, char heaterActive)
{
	FILE *log = fopen("farm.log", "ab");
	if (log == NULL){
		printf("log file error\n");
		return;
	}
	fprintf(log, "%s;%.1f;%.1f;%d\r\n", strTimeInfo, m.temp, m.humidity, heaterActive);
	fclose(log);
	return;
}

int main(int argc, char *argv[])
{
	
	char strTimeInfo [80];
	prepareArgs(argc, argv);

	char heaterActive = 0;
	while(1)
	{
		struct measure m = getMeasure();

		if (!heaterActive){
			if (m.temp <= Tmin)	heaterActive = 1;
		}	else if (m.temp >= Tmax) heaterActive = 0;

		getCurrentTime(strTimeInfo);
		printf("%s\tTemp: %.1fC; Humidity: %.1f%%\t", strTimeInfo, m.temp, m.humidity);
		writeMeasureToLog(strTimeInfo, m, heaterActive);
		heater(heaterActive);

		fflush(stdout);
		Sleep(timeDelay * 60 * 1000);
	}
	return 0;
}