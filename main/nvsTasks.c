#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "nvsTasks.h"
#define TAG "nvsTasks"

//  NOTE to erase flash use >idf.py erase_flash flash monitor
//  NOTE encrypted flash is supported
//  NOTE key/value pairs can be pre-flashed from a cvs file with a utility
//  NOTE string values < 4000, blob < 508000 bytes.


/*
 Inital build says:
# ESP-IDF Partition Table
# Name, Type, SubType, Offset, Size, Flags
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,1M,

 Initial boot info says (identical):
I (58) boot: Partition Table:
I (61) boot: ## Label            Usage          Type ST Offset   Length
I (69) boot:  0 nvs              WiFi data        01 02 00009000 00006000   =    24,576 bytes
I (76) boot:  1 phy_init         RF data          01 01 0000f000 00001000   =     4,096
I (84) boot:  2 factory          factory app      00 00 00010000 00100000   = 1,048,576
I (91) boot: End of partition table

But grabbing a sample from D:\esp\esp-idf\components\partition_table and making my own:
main/partionsG02flow.csv
# Name,   Type, SubType, Offset,  Size, Flags
# Note: if you change the phy_init or app partition offset, make sure to change the offset in Kconfig.projbuild
nvs,      data, nvs,     ,        0x6000,
phy_init, data, phy,     ,        0x1000,
factory,  app,  factory, ,        1M,
g02nvs,   data, nvs,     ,        0x2000,

Then menuconfig -> Partition Table -> custom partition table CSV -> partitionsG02flow.csv

 Now build says
*******************************************************************************
# ESP-IDF Partition Table
# Name, Type, SubType, Offset, Size, Flags
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,1M,
g02nvs,data,nvs,0x110000,8K,
*******************************************************************************

and boot info says
I (60) boot: Partition Table:
I (64) boot: ## Label            Usage          Type ST Offset   Length
I (71) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (79) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (86) boot:  2 factory          factory app      00 00 00010000 00100000
I (94) boot:  3 g02nvs           WiFi data        01 02 00110000 00002000
I (101) boot: End of partition table

*/

//Example from Learn ESP32:
//stores an integer and increments it once per program run which
//  we assume assume happens after every chip reset.
void espResetCounter()
{
    //this delay is to give "idf.py monitor" time to start and display the
    //  results, especially if you erase_flash and want to see "Value not set..."
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    //must be initialized
    ESP_ERROR_CHECK(nvs_flash_init()); //default "nvs" partition
    // or if using my partition
    //nvs_flash_init_partition("g02nvs");

    //create your own chunk (namespace), with your own "name" (16 chars),
    //  keep handle. Keys <= 16 chars also. Value can be many types.
    nvs_handle handle;
    ESP_ERROR_CHECK(nvs_open("store", NVS_READWRITE, &handle));
    //also can open a namespace in another partition
    //nvs_open_from_partition("g02nvs","store", NVS_READWRITE, &handle);

    //read first, may already have data from before
    int32_t val = 0;
    // get from chunk "store" (my handle), key "val", value (int32)val.
    esp_err_t result = nvs_get_i32(handle, "val", &val);
    switch (result)
    {
    case ESP_ERR_NVS_NOT_FOUND:
    //case ESP_ERR_NOT_FOUND:
        ESP_LOGE(TAG, "Value not set yet");
        break;
    case ESP_OK:
        ESP_LOGI(TAG, "Value is %d", val);
        break;
    default:
        // like invalid handle, invalid name, invalid length, not enough
        //      space, read only, value too long, remove failed
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(result));
    break;
    }

    // set a new value
    val++;
    ESP_ERROR_CHECK(nvs_set_i32(handle, "val", val));

    // erase also allowed
    //ESP_ERROR_CHECK(nvs_erase_key(handle, "val"));

    //save and close NVS. Assume commit is used to insure multi-write integrity.
    ESP_ERROR_CHECK(nvs_commit(handle)); //write any pending changes
    nvs_close(handle); //frees memory once handle no longer needed.

}

//Extra data
// SSID (done by wifi-manager)
// Password

// flow meter name "irr758" default "Beamir212"
// link to cloud Y/N required for notification services
//      text number and email?
//      (text number and email entered in cloud in future)
// get time from cloud Y/N
//      use timezone
//      use timeservers??
//  or  enter HH:MM
//  nvs memory used%

// cycle  1-14 (7) days
// cycle start timeDate
// prev cycle start timeDate (null = none)
// baseline cycle start timeDate (null = none)
// pulses per liter (requires calibration)
// display liters/gallons
//
//Notifications if
// start time +/- 1-15 (5) minutes of baseline
// flow time +/- 1-60 (30) seconds of baseline
// total flow +/- 1-10 (5)% of baseline
// avg flow +/- 1-10 (5)% of baseline
// low bat voltage < 3.6? every hour?
// bat voltage full > 4.1v? once?
// bat voltage OK 3.6-4.1? every day?
// nvs < 2%?
// esp32 reboot
// connection outage (> 10 min?)
// NTS no response
// text log entries? Warning/Error only?
// power state (red/yellow/green & rising-solar/falling-noSolar)


// Initialize files and store details here.
nvs_handle fileHandles[noOfFiles];
size_t fileRcdSizes[noOfFiles];
aosKey_t maxKeys[noOfFiles];
void filesInit(void){
    
    ESP_ERROR_CHECK(nvs_flash_init_partition("g02nvs"));
    
    // Put the file name in "enum aosFiles" and open it here.
    ESP_ERROR_CHECK(nvs_open_from_partition("g02nvs", "oDuration", NVS_READWRITE, &fileHandles[DURATIONS]));
    // Create a structure and put the size here.
    fileRcdSizes[DURATIONS] = sizeof(oDuration);
    // Limit the number of records here.
    maxKeys[DURATIONS] = 630; //maximum record number (key = 0 to ?)
    // Dont need this because file always in use.
    //nvs_close(fileHandles[DURATIONS]); //frees memory

    // Create the read/write macros in .h using this template:
    //#define readDuration(key, rcdName) aosfile(FILEREAD, DURATIONS, (key), (void *)&rcdName)
    //#define writeDuration(key, rcdName) aosfile(FILEWRITE, DURATIONS, (key), (void *)&rcdName)

    // Put the file name in "enum aosFiles" and open it here.
    ESP_ERROR_CHECK(nvs_open_from_partition("g02nvs", "oVoltage", NVS_READWRITE, &fileHandles[VOLTAGES]));
    // Create a structure and put the size here.
    fileRcdSizes[VOLTAGES] = sizeof(oVoltage);
    // Limit the number of records here.
    maxKeys[VOLTAGES] = 4032; //maximum record number (key = 0 to ?)
    // Dont need this because file always in use.
    //nvs_close(fileHandles[VOLTAGES]); //frees memory
    // Create the read/write macros in .h using this template:
    //#define readDuration(key, rcdName) aosfile(FILEREAD, DURATIONS, (key), (void *)&rcdName)
    //#define writeDuration(key, rcdName) aosfile(FILEWRITE, DURATIONS, (key), (void *)&rcdName)
}

//wish it returned g02nvs %full
void filesStatus(void){
    // Example of nvs_get_stats() to get the number of used entries and free entries:
    nvs_stats_t nvs_stats;
    //nvs_get_stats(NULL, &nvs_stats); // "NULL" means use default name "nvs"
    nvs_get_stats("g02nvs", &nvs_stats); // or my namespace
    ESP_LOGI(TAG,"Count: UsedEntries = (%d), FreeEntries = (%d), TotalEntries = (%d), namespaces in partition = %d\n",
        nvs_stats.used_entries, nvs_stats.free_entries, nvs_stats.total_entries, nvs_stats.namespace_count);
}

// Read or write an "array of structures" file with numeric key and equal size records.
//  Returns 1=success, 0=not found or bad key, all other errors abort.
// This is a generic function. You are expected to make easier r/w macros for each
//  of your files.
bool aosfile(bool readWrite, uint8_t fileNo, aosKey_t key, void * rcd) {
    if (key > maxKeys[fileNo]) return false;
    char sKey[11]; // 16 bit key never needs more than 5 chars + NULL but compiler likes 11
    sprintf(sKey, "%d", key); //turn numeric into string

    esp_err_t result;
    if (readWrite) {
        result = nvs_get_blob(fileHandles[fileNo], sKey, rcd, &fileRcdSizes[fileNo]);
        //NOTE if destination is NULL and size is 0 then the actual size
        //  of that record is returned.
        //dontCare = 0;
        //result = nvs_get_blob(fileDuration, sKey, NULL, &dontCare);
        //printf("Key %s length is %d\n",sKey,dontCare);
    } else {
        result = nvs_set_blob(fileHandles[fileNo], sKey, rcd, fileRcdSizes[fileNo]);
        if (result == ESP_OK) ESP_ERROR_CHECK(nvs_commit(fileHandles[fileNo]));
    }
    if (result == ESP_OK) return true;
    if (result == ESP_ERR_NVS_NOT_FOUND) return false;

    // like invalid handle, invalid name, invalid length, not enough
    //      space, read only, value too long, remove failed
    ESP_LOGE(TAG, "Error (%s) %s file %u key %u!\n", esp_err_to_name(result), readWrite?"Reading":"Writing", fileNo, key);
    //fflush(stdout);
    //esp_restart();
    return false;
}
