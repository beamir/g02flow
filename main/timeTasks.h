#ifndef _TIMETASKS_H_
#define _TIMETASKS_H_

// Manual set RTCC
void timeAhead(void);

// Manual set Time Zone (from chart on internet)
void timeTZset(char * tzname);

// time_show(NULL); //shows time
void time_show(struct timeval *tv);

// Cloud sets esp32 RTCC
void timeCloudStart(char * serverName);

void timeCloudStop(void);

char * displayTime(time_t mySeconds);

#endif