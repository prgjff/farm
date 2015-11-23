#include <wiringPi.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#ifdef _WIN32
#include <Windows.h>
#endif

#include "locking.h"

#define MAXTIMINGS 85
#define MAX_MEASURE 10

struct measure
{
	float temp;
	float humidity;
};

	float Tmin = -100;
	float Tmax = -100;
	int timeDelay = -1;
	static int DHTPIN = 7;
	static int RELAY1PIN = 8;
	static int RELAY2PIN = 9;

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
        wiringPiSetup();
        pinMode(RELAY1PIN, OUTPUT);
	pinMode(RELAY2PIN, OUTPUT);
        if (active){
        	digitalWrite(RELAY1PIN, LOW);
                digitalWrite(RELAY2PIN, LOW);
		printf("Обогрев включен\n");
        }else{
		digitalWrite(RELAY1PIN, HIGH);
		digitalWrite(RELAY2PIN, HIGH);
		printf("Обогрев выключен\n");
	}
}

static uint8_t sizecvt(const int read)
{
  if (read > 255 || read < 0)
  {
    printf("Invalid data from wiringPi library\n");
    exit(EXIT_FAILURE);
  }
  return (uint8_t)read;
}

static char read_dht22_dat(struct measure *m)
{
  char result = 0;
  uint8_t laststate = HIGH;
  uint8_t counter = 0;
  uint8_t j = 0, i;
  int dht22_dat[5] = {0,0,0,0,0};

  int lockfd = open_lockfile(LOCKFILE);

  if (wiringPiSetup () == -1)
    exit(EXIT_FAILURE);

  if (setuid(getuid()) < 0)
  {
    perror("Dropping privileges failed\n");
    exit(EXIT_FAILURE);
  }

  // pull pin down for 18 milliseconds
  pinMode(DHTPIN, OUTPUT);
  digitalWrite(DHTPIN, HIGH);
  delay(10);
  digitalWrite(DHTPIN, LOW);
  delay(18);
  // then pull it up for 40 microseconds
  digitalWrite(DHTPIN, HIGH);
  delayMicroseconds(40); 
  // prepare to read the pin
  pinMode(DHTPIN, INPUT);

  // detect change and read data
  for ( i=0; i< MAXTIMINGS; i++) {
    counter = 0;
    while (sizecvt(digitalRead(DHTPIN)) == laststate) {
      counter++;
      delayMicroseconds(1);
      if (counter == 255) {
        break;
      }
    }
    laststate = sizecvt(digitalRead(DHTPIN));

    if (counter == 255) break;

    // ignore first 3 transitions
    if ((i >= 4) && (i%2 == 0)) {
      // shove each bit into the storage bytes
      dht22_dat[j/8] <<= 1;
      if (counter > 16)
        dht22_dat[j/8] |= 1;
      j++;
    }
  }

  // check we read 40 bits (8bit x 5 ) + verify checksum in the last byte
  // print it out if data is good
  if ((j >= 40) && 
      (dht22_dat[4] == ((dht22_dat[0] + dht22_dat[1] + dht22_dat[2] + dht22_dat[3]) & 0xFF)) ) {
        float t, h;
        h = (float)dht22_dat[0] * 256 + (float)dht22_dat[1];
        h /= 10;
        t = (float)(dht22_dat[2] & 0x7F)* 256 + (float)dht22_dat[3];
        t /= 10.0;
        if ((dht22_dat[2] & 0x80) != 0)  t *= -1;

    m->temp = t;
    m->humidity = h;
    result = 1;
  }
  else
  {
    result = 0;
  }

  delay(1500);
  close_lockfile(lockfd);
  
  return result; 
}

struct measure getMeasure()
{
	int indexMeasure = 0;
	struct measure result;

	while ((read_dht22_dat(&result) == 0) && (indexMeasure < MAX_MEASURE))
	{
		delay(2000); // wait 2sec to refresh
		indexMeasure++;
	}

	//result.temp = 10;
	//result.humidity = 25.5;
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
		#ifdef __unix__
		sleep(timeDelay * 60);
		#else
		Sleep(timeDelay * 60 * 1000);
		#endif
	}
	return 0;
}
