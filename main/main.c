// Hints: change File>Preferences>Keyboard shortcuts so ctrl-S saves all.
//	Every new .c file has to be added manually to the list in CMakeLists.txt.
//		This is solve path problems, sometimes.
// <ctl><alt>downArrow makes extra cursors straight down, useful if changing similar
//		lines. <esc> to exit.
// ESP-WROVERs have extra memory PSRAM
//		menuconfig -> component config -> esp32 -> Support for external, spi-conn.. -> SPI RAM config
//			select Try to allocate memories of WiFi and LWIP in SPIRAM firstly...
//			select Allow .bss segment placed in external memory (NEW)
//		...and now you should see +4M memory extra!
//		Some restrictions. One is no external memory in stack. So task memory always only internal.
// The ESP-WROVER-KIT v4.1 has built in JTAG (one serial) etc. nice
// There is a tool for viewing the partion table in the build folder, see Flash tutorial,
//		but usually adjust it in the menuconfig, first option.
// Extra files to be flashed in the esp32 kept in main/files and added to main/component.mk
//		and main/CMakeLists.txt with specific language.
// Files can then be referenced in your code like
/*
//                            yourName[]               orig.name
  extern const unsigned char indexHtml[] asm("_binary_index_html_start");
  printf("html = %s\n", indexHtml);

  extern const unsigned char sample[] asm("_binary_sample_txt_start");
  printf("sample = %s\n", sample);

// Text files like 2 above are null terminated. Binary will need size.
  extern const unsigned char imgStart[] asm("_binary_pinout_jpg_start");
  extern const unsigned char imgEnd[] asm("_binary_pinout_jpg_end");
  const unsigned int imgSize = imgEnd - imgStart;
  printf("img size is %d\n", imgSize);
*/


//#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h" // if you want memory stats

#include "pinTasks.h"
#include "wifiTasks.h"
#include "timeTasks.h"
#include "nvsTasks.h"

// Set the level before .h file
//#define LOG_LOCAL_LEVEL ESP_LOG_NONE
//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#define TAG "inMain"


void app_main(void)
{
	//esp_log_level_set(TAG, ESP_LOG_DEBUG);
    ESP_LOGI(TAG,"Starting...\n\n");

	//check memory
	ESP_LOGI(TAG, "xPortGetFreeHeapSize %dk = DRAM", xPortGetFreeHeapSize());
	int DRam = heap_caps_get_free_size(MALLOC_CAP_8BIT);
	int IRam = heap_caps_get_free_size(MALLOC_CAP_32BIT) - heap_caps_get_free_size(MALLOC_CAP_8BIT);
  	ESP_LOGI(TAG, "DRAM \t\t %d", DRam);
  	ESP_LOGI(TAG, "IRam \t\t %d", IRam);
	//int free = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT); //largest useable piece ~165k
	//Can put this at start and end of tasks to make sure no memory leaks.
	ESP_LOGI("memory","stack %d, total ram %d, internal memory %d, external memory %d\n",
    	uxTaskGetStackHighWaterMark(NULL), heap_caps_get_free_size(MALLOC_CAP_8BIT),
    	heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));


	//idf.py size: (initialized (.data) + not (.bss) = static DRAM) + static IRAM + flash code + flash rodata = total image
	//idf.py size-files for details
	//            ------------  Free  ----------  --------------  Used ---------------  --4MB Flash --
	//History       DRAM   IRAM  HeapNow HeapEnd  static DRAM static IRAM ~total image    code  rodata
	//v0.3 4/8/21 272996  43348   241456  205436  35748 19.8% 87685 66.9%       790472  581731  148800

	LEDsetup();
	LEDblink(3);

	//test nvs
	filesInit();
	// oDuration flowDone;
	// flowDone.flowTime = 10;
	// flowDone.startTime = 20;
	// showDuration(flowDone); //print it
	// writeDuration(4, flowDone);
	// flowDone.flowTime = 30;
	// showDuration(flowDone); //print it
	// readDuration(4, flowDone);
	// showDuration(flowDone); //print it

	oVoltage myStuff;
	myStuff.batterymV = 3400;
	myStuff.solarmV = 4200;
	myStuff.timestamp = 3600;
	showVoltage(myStuff);
	writeVoltage(2,myStuff);
	myStuff.batterymV = 200;
	showVoltage(myStuff);
	readVoltage(2,myStuff);
	myStuff.timestamp = 103600;
	showVoltage(myStuff);


	wifiSetup(); //this blocks until a connection is made

		//qodget(); //this blocks until complete

	//sends the message currently in the rxBuf
	//SMSsend("+16199933344"); //this blocks until complete

	//set the Real Time Clock (RTC) which survives reset.
	//if (database(timeFromCloud) == yes) {
		timeTZset("PST8PDT"); // time zone from database??
		timeCloudStart("pool.ntp.org"); // time server from database??
		vTaskDelay(10000 / portTICK_RATE_MS);
		time_show(NULL);
	//} else {
	//	//Using RTC time.
	//	timeAhead(); // May look better. UNTESTED
	//}

    ESP_LOGI(TAG,"Goodbye world!\n");

	//vTaskDelay(10000 / portTICK_RATE_MS); //how do I know when ready?
	vTaskDelay(portMAX_DELAY); //wait indefinitely
	// Don't exit this task or you may eventually get:
	// Exception in thread Thread-2:
	// Traceback (most recent call last):
	//   File "E:\python\lib\threading.py", line 954, in _bootstrap_inner
	//     self.run()
	// ...
}
