// Hints: change File>Preferences>Keyboard shortcuts so ctrl-S saves all.
//	Every new .c file has to be added manually to the list in CMakeLists.txt.
//		This is solve path problems, sometimes.


//#include <stdio.h>
//#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pinTasks.h"
#include "wifiTasks.h"

// Set the level before .h file
//#define LOG_LOCAL_LEVEL ESP_LOG_NONE
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define TAG "inMain"


void app_main(void)
{
	//esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG,"Starting...\n\n");
	
	LEDsetup();
	LEDblink(3);

	wifiSetup(); //this blocks until a connection is made
	//vTaskDelay(10000 / portTICK_RATE_MS); //how do I know when ready?

		qodget();

    ESP_LOGI(TAG,"Goodbye world!\n");

	//vTaskDelay(portMAX_DELAY); //wait indefinitely
}
