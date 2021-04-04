//Small tasks involving GPIO
// gac 4/3/21

//#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Set the level before .h file
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#define TAG "pinTasks"


//---------------------------- Control the LED(s) -----------------------------
#define LEDdebug GPIO_NUM_2
QueueHandle_t blinkAmtQ;

//a task is passed one void pointer, returns void, but NEVER exits
//this task waits for queue, gets count, and blinks
// IF YOU USE ANY ESP_LOGI() THEN INCREASE TASK SIZE TO 4096! Else 1024.
void LEDblinkTask(void * params){
    int rxCount;
    
    // UBaseType_t uxHighWaterMark;
    // /* Inspect our own high water mark on entering the task. */
    // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    // ESP_LOGI(TAG,"High mark %d",uxHighWaterMark);
    
    while(1){
        xQueueReceive(blinkAmtQ, &rxCount , portMAX_DELAY); //wait indefinitely for an rxCount from queue
        //ESP_LOGI(TAG,"Do %d blinks.\n",rxCount);

        while(rxCount--){
            gpio_set_level(LEDdebug, 1);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            gpio_set_level(LEDdebug, 0);
            vTaskDelay(500 / portTICK_PERIOD_MS);
        }

    /* Calling the function will have used some stack space, we would 
        therefore now expect uxTaskGetStackHighWaterMark() to return a 
        value lower than when it was called on entering the task. */
        // uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
        // ESP_LOGI(TAG,"High aftr %d",uxHighWaterMark);
    }
}

//call LEDsetup() first, then anyone can call LEDblink().
void LEDsetup(void){
    gpio_pad_select_gpio(LEDdebug);
	gpio_set_direction(LEDdebug, GPIO_MODE_OUTPUT);
    blinkAmtQ = xQueueCreate(3, sizeof(int)); //max 3 integers in queue
    xTaskCreate(&LEDblinkTask, "blink LED", 1024, NULL, 2, NULL);
}

//blink the debug LED "count" times. Do LEDsetup() first. Non-blocking, triggers task.
void LEDblink(int count){
    //ESP_LOGI(TAG,"Request %d blinks.\n",count);
    xQueueSend(blinkAmtQ, &count, 100 / portTICK_PERIOD_MS); //push count, if unsuccessful after 100 ms, ignore
}

