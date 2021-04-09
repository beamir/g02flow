// Geoff Clark 4/7/21
// Notes: the RTC clock source is a CONFIG setting.
//          Internal 150kHz RC (default) features lowest
//              deep sleep current and no external componants
//          External 32kHz crystal on 32K_XP and 32K_XN pins
//              better accuracy, 1 uA more Deep Sleep current
//          External oscillator at 32K_XN pin < 1 v p-p square
//              and 1 nF 32_XP -> Gnd, no GPIO
//          Internal 8.5 MHz better stability, +5 nA deep sleep.
//      Details for external in esp32 hardware design guidelines.
//      RTC keeps time during resets and sleep modes.
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_sntp.h"
//#include "esp_wifi.h"
//#include "nvs_flash.h"
//#include "protocol_examples_common.h"
#include "esp_log.h"

#define TAG "timeTasks"

//If not using cloud time then if power failure time is unknown.
//  At least bring time from 1970 to latest year for cosmetics.
void timeAhead(void){
    struct timeval temp;
    
    time(&temp.tv_sec); //gets time in seconds (from esp-idf)

    if (temp.tv_sec < 50000) {
        temp.tv_sec = 1609488000; // 1/1/2021 00:00:00 from www.unixtimestamp.com
        temp.tv_usec = 0;
        settimeofday(&temp,NULL); // do not set TZ
    }
}

//Use this as a callback for debugging or to show the current time.
//Internally, every time an NTP response arrives, time is updated with:
//  settimeofday((const struct timeval *tv, const struct timezone *tz);
//YOU DO NOT NEED A CALLBACK to keep time current.
//  USAGE: time_show(NULL); //just show current time
//         sntp_set_time_sync_notification_cb(time_show); //display time with each update
void time_show(struct timeval *tv)
{
    if (tv != NULL) printf("secs %ld\n", tv->tv_sec);

    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now); //gets time in seconds (from esp-idf)

    localtime_r(&now, &timeinfo); //converts "now" seconds into "timeinfo" struct
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Chula Vista is: %s", strftime_buf);
}

// An application with this initialization code will periodically synchronize the
//  time. The time synchronization period is determined by CONFIG_LWIP_SNTP_UPDATE_DELAY
//  (default value is one hour). To modify the variable, set CONFIG_LWIP_SNTP_UPDATE_DELAY
//  in project configuration ( Component config → LWIP → SNTP -> 3600000).
void timeCloudStart(char * serverName){
    //ESP_LOGI(TAG,"Changing time server to %s.", serverName);
    //ESP_LOGI(TAG,"Changing time server from %s.", sntp_getservername(0));  //THIS ABORTS THE RUNTIME!! ASSUME MISSING NAME.
    //sntp_set_sync_interval(uint32_t interval_ms); // do this in config
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH); // or SNTP_SYNC_MODE_IMMED
    sntp_setservername(0, serverName); //more than one time server allowed!!
    sntp_init(); //this appears safe to do more than once
//sntp_set_time_sync_notification_cb(time_show);  //comment out if debugged
    ESP_LOGI(TAG,"Cloud time server started every %d ms.", sntp_get_sync_interval());
}


// Set time zone
//      Time zone info google "setenv tz knowledge" and you can get
//          https://knowledgebase.progress.com/articles/Article/P129473
//          which has a chart of strings to use for each TZ.
void timeTZset(char * tzname) {
    // Set timezone to CV
    setenv("TZ", tzname, 1);
    tzset();
}

// UNTESTED
void timeCloudStop(void){
    sntp_setservername(0, "doNotUse");
    esp_restart();
}


//replacement for asctime() so you don't need the variables.
// asctime() returns 26 char[] NEWLINE AND NULL terminated.
char * displayTime(time_t mySeconds) {
    struct tm * tp;
    tp = localtime(&mySeconds);
    return asctime(tp);
}