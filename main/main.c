#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN GPIO_NUM_2

// see https://www.learnesp32.com/3_blinkey for details
void blinky(void *params)
{
	gpio_pad_select_gpio(PIN);
	gpio_set_direction(PIN, GPIO_MODE_OUTPUT);
	int isOn = 0;
	while (true)
	{
		isOn = !isOn;
		gpio_set_level(PIN, isOn);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
void app_main(void)
{
  printf("Hello world!\n");
  
  xTaskCreate(&blinky, "blink led", 2048, NULL, 2, NULL);
}
