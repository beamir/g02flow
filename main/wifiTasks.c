/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

/* Geoff Notes:
The ssid and pwd are displayed in LOGI output. Reduce notifications
    in production version.
Set the network device name in MENUCONIG > Componant config > LWIP > Local netif hostname (=Beamir)
    Note it will be displayed all caps BEAMIR.
*/
#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_log.h"

#include "wifi_manager.h"
#include "http_app.h"
#include "mdns.h"


/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifiTasks";

//do client tasks one at a time, by whoever has the connectionSemaphore
xSemaphoreHandle connectionSemaphore = NULL;
StaticSemaphore_t xMutexBuffer1;
//extern xSemaphoreHandle connectionSemaphore; //put in another file if they need to check
//  Can not take mutex in wifiSetup() and give in cb_connection_ok() when connection
//first made, because cb_connection_ok() is not the owner (the taker). So use global.
bool ipAvailable;

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code! This is an example on how you can integrate your code with wifi-manager
 */
void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(30000) );
	}
}


/**
 * @brief this is an exemple of a callback that you can setup in your own app to get notified of wifi manager event.
 */
void cb_connection_ok(void *pvParameter){
	ip_event_got_ip_t* param = (ip_event_got_ip_t*)pvParameter;

	/* transform IP to human readable string */
	char str_ip[16];
	esp_ip4addr_ntoa(&param->ip_info.ip, str_ip, IP4ADDR_STRLEN_MAX);

	ESP_LOGI(TAG, "I have a connection and my IP is %s!", str_ip);
    //got ip, clients can operate. I can't unblock mutex because I am
    // not the task that took the semaphore.
    ipAvailable = true;
    
    // TODO: this works but doesn't change name in router to BEAMIR212
    //      Aborts happen if you try this sooner.
    // Assume user can have more than one esp32 on network. Try to keep
    //  names unique by appending a number from the mac address.
    const char *staHostname;
    uint8_t mac6b[6]; //six byte universal mac address
    esp_netif_t* staHandle = wifi_manager_get_esp_netif_sta();
    if (ESP_OK == esp_netif_get_hostname(staHandle, &staHostname)) {
        //ESP_LOGI(TAG,"Got %s\n",staHostname);
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac6b));
        //ESP_LOGI(TAG,"last 3 mac %X:%X:%X",mac6b[3],mac6b[4],mac6b[5]);
        //reuse str_ip
        sprintf(str_ip,"%s%02u",staHostname,mac6b[5]);
        ESP_LOGI(TAG,"Wish my name was %s",str_ip);
        esp_netif_set_hostname(staHandle, str_ip);
        //esp_netif_get_hostname(staHandle, &staHostname);
        //ESP_LOGI(TAG,"2nd %s\n",staHostname);
    } else {
        strcpy(str_ip,staHostname); //used for mDNS
    }
    
    // This code was taken from mDNS Service in Espressif docs.
    // Verified is working by:
// d:\projects\g02flow>dns-sd -G v4 Beamir212.local
// Timestamp     A/R Flags if Hostname                  Address                                      TTL
// 15:50:09.106  Add     2  7 Beamir212.local.          192.168.1.102                                120

    // LOG showed:
//W (1350250) httpd_uri: httpd_uri: Method '2' not allowed for URI '/helloworld'
//W (1350250) httpd_txrx: httpd_resp_send_err: 405 Method Not Allowed - Request method for this URI is not handled by server

    //initialize mDNS service
    if (ESP_OK == mdns_init()) {
        //set hostname
        mdns_hostname_set(str_ip);
        //set default instance
        mdns_instance_name_set("Change Flow Monitor Settings");
        ESP_LOGI(TAG,"Browse to http://%s.local/helloworld",str_ip);
    } else {
        ESP_LOGW(TAG,"MDNS Init failed.");
    }


}


// Log why station disconnected.
void cb_connection_failed(void *pvParameter){
	wifi_event_sta_disconnected_t* param = (wifi_event_sta_disconnected_t*)pvParameter;

    //When router power loss, reason is 200. During retries reason is 201. After
    //router reboot esp manager reconnects (with possibly new ip).
	ESP_LOGW(TAG, "WiFi %s disconnected because %u.",param->ssid,param->reason);
}

// callback for http GET requests. WEB SERVER.
static esp_err_t my_get_handler(httpd_req_t *req){

	/* our custom page sits at /helloworld in this example */
	if(strcmp(req->uri, "/helloworld") == 0){

		ESP_LOGI(TAG, "Serving page /helloworld");

		const char* response = "<html><body><h1>Hello World!</h1></body></html>";

		httpd_resp_set_status(req, "200 OK");
		httpd_resp_set_type(req, "text/html");
		httpd_resp_send(req, response, strlen(response));
	}
	else{
		/* send a 404 otherwise */
		httpd_resp_send_404(req);
	}

	return ESP_OK;
}


//void app_main() orignally in wifi_manager examples
void wifiSetup()
{
    ipAvailable = false;
    /* Create a mutex type semaphore. */
    //ESP_ERROR_CHECK(connectionSemaphore = xSemaphoreCreateMutex());
    //connectionSemaphore = xSemaphoreCreateMutex();
    
    /* Create a mutex semaphore without using any dynamic memory
    allocation.  The mutex's data structures will be saved into
    the xMutexBuffer variable. */
    connectionSemaphore = xSemaphoreCreateMutexStatic( &xMutexBuffer1 );
    /* The *pxMutexBuffer was not NULL, so it is expected that the handle will not be NULL. */
    if ( connectionSemaphore == NULL ) { ESP_LOGE(TAG,"Unimaginable"); }

    // Take the semaphore until STA connected
    if ( xSemaphoreTake(connectionSemaphore, 100 / portTICK_RATE_MS) == pdFALSE ) {
        ESP_LOGE(TAG,"Unimaginable"); //just created so no one else could have taken
    }
    
	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);

    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &cb_connection_failed);

    /* set custom handler for the http server
	 * Now navigate to /helloworld to see the custom page
	 * */
	http_app_set_handler_hook(HTTP_GET, &my_get_handler);

	/* your code should go here. Here we simply create a task on core 2 that monitors free heap memory */
	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);


    // Block return until connection succeeds.
    while ( !ipAvailable) {
        ESP_LOGI(TAG,"No connection yet...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (xSemaphoreGive(connectionSemaphore) == pdFALSE ) {
        ESP_LOGE(TAG,"Unimaginable");
    }
}


//--------------------------------- Client Functions ---------------------------
#include "esp_http_client.h"
#include "cJSON.h"
 
//extra stuff a client needs
typedef struct {
    char *key;
    char *val;
} Header;

typedef enum {
    Get,
    Post
} HttpMethod;

struct appFetchParams
{
    HttpMethod method;
    int headerCount;
    Header header[10];
    char *body;
    void (*OnGotData)(char *incomingBuffer, struct appFetchParams *appParams); //function  urlData -> clientData
    char * rxBuf;   //the data the client wants
    int16_t rxLast; //index of last rxBuf element (size - 1)
    esp_err_t err;  //error code returned by esp client process (s/b ESP_OK)
    int status;   // http response status code (to client request)
};

//general purpose buffers for any use, any client
char rxClientBuf[300];
char txClientBuf[1024];

// Started by clientFetch, we gather url data as it becomes available,
//  then call user function to process. After user function, handler
//  exits for final time and esp_http_client_perform() somehow knows
//  it is done.
// Although the clientEventHandler() is generic, it can not be used
//  by multiple clients simultaneously (unless it's reentrant?). Each
//  client is responsible for insuring exclusive use.
esp_err_t clientEventHandler(esp_http_client_event_t *evt)
{
    static char *buffer = NULL;
    static int indexBuffer = 0;

    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA Len=%d", evt->data_len);
    //printf("%.*s\n", evt->data_len, (char *)evt->data);
        if (buffer == NULL)
        {
            buffer = (char *)malloc(evt->data_len);
        }
        else
        {
            buffer = (char *)realloc(buffer, evt->data_len + indexBuffer);
        }
        memcpy(&buffer[indexBuffer], evt->data, evt->data_len);
        indexBuffer += evt->data_len;
        break;

    case HTTP_EVENT_ON_FINISH:
        //cap the input buffer
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        buffer = (char *)realloc(buffer, indexBuffer + 1);
        memcpy(&buffer[indexBuffer], "\0", 1);
    
        //get the handler for this particular data
        struct appFetchParams * appParams = (struct appFetchParams *)evt->user_data;
        if (appParams->OnGotData != NULL)
        {
            appParams->OnGotData(buffer, appParams);
        }

        free(buffer);
        buffer = NULL;
        indexBuffer = 0;
        break;

    // Full slate. Not all are necessary.
    // case HTTP_EVENT_ERROR:
    //     ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
    //     break;
    // case HTTP_EVENT_ON_CONNECTED:
    //     ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
    //     break;
    // case HTTP_EVENT_HEADER_SENT:
    //     ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
    //     break;
    // case HTTP_EVENT_ON_HEADER:
    //     ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
    //     printf("%.*s", evt->data_len, (char*)evt->data);
    //     break;
    // case HTTP_EVENT_ON_DATA:
    //     ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    //     if (!esp_http_client_is_chunked_response(evt->client)) {
    //         printf("%.*s", evt->data_len, (char*)evt->data);
    //     }
    //     break;
    // case HTTP_EVENT_ON_FINISH:
    //     ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
    //     break;
    // case HTTP_EVENT_DISCONNECTED:
    //     ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");

    default:
        //Assume you want to ignore any other event.
        break;
    }

    return ESP_OK;
}


//Kicks off fetching data, then leaves clientEventHandler() (typically) to do the work.
void clientFetch(esp_http_client_config_t * pClientConfig)
{
    //dig out our extra user configuration parameters
    struct appFetchParams * userParams = pClientConfig->user_data;

    esp_http_client_handle_t client = esp_http_client_init(pClientConfig);
    if (userParams->method == Post)
    {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
    }
    for (int i = 0; i < userParams->headerCount; i++)
    {
        esp_http_client_set_header(client, userParams->header[i].key, userParams->header[i].val);
    }
    if(userParams->body != NULL)
    {
        esp_http_client_set_post_field(client, userParams->body, strlen(userParams->body));
    }
    esp_err_t err = esp_http_client_perform(client); //THIS BLOCKS UNTIL DONE!!!
    userParams->err = err; //save for user
    if (err == ESP_OK)
    {
        userParams->status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET status = %d",
                 esp_http_client_get_status_code(client));
        //ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %d",
        //         esp_http_client_get_status_code(client),
        //         esp_http_client_get_content_length(client));

    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    //Although an esp32 is usually an IOT endpoint making short, infrequent
    //  connections that only need one transfer, it is possible to leave the
    //  client connection open for subsequent transfers.
    //This is from idf docs (first request same as above).

    // // second request
    // esp_http_client_set_url(client, "http://httpbin.org/anything")
    // esp_http_client_set_method(client, HTTP_METHOD_DELETE);
    // esp_http_client_set_header(client, "HeaderKey", "HeaderValue");
    // err = esp_http_client_perform(client);

    esp_http_client_cleanup(client);
}


void qodProcessURLdata(char *incomingBuffer, struct appFetchParams *appParams) {
    //want contents/quotes/quote
    cJSON *payload = cJSON_Parse(incomingBuffer);
    cJSON *contents = cJSON_GetObjectItem(payload, "contents");
    cJSON *quotes = cJSON_GetObjectItem(contents, "quotes");
    cJSON *quotesElement;
    cJSON_ArrayForEach(quotesElement, quotes)
    {
        cJSON *quote = cJSON_GetObjectItem(quotesElement, "quote");
        //ESP_LOGI(TAG,"%s",quote->valuestring);
        //Note only the "quote" quotesElement is saved. Note rxLast + 1 = rxBuf size.
        strncpy(appParams->rxBuf, quote->valuestring, appParams->rxLast);
        //printf("\nEssence =%s",quote->valuestring);
        (appParams->rxBuf)[appParams->rxLast] = 0; //in case quote longer than buf
        //printf("\nTakeaway=%s",appParams->rxBuf);
    }
    cJSON_Delete(payload);
}


//Load QOD (quote of the day) url info and fetch() results.
void qodget(void){

    ESP_LOGI(TAG, "qod awaiting turn...");
    //if (xSemaphoreTake(connectionSemaphore, 10000 / portTICK_RATE_MS))
    xSemaphoreTake(connectionSemaphore, portMAX_DELAY); //wait indefinitely

    struct appFetchParams qodFetchParams = {
        .method = Get,
        .headerCount = 0,
        .body = NULL,
        .OnGotData = qodProcessURLdata,
        .rxBuf = rxClientBuf,
        .rxLast = sizeof(rxClientBuf) - 1 //last element set to NULL for safety.
    };
      
    // one of these for every webpage you want
    esp_http_client_config_t qodConfig = {
        .url = "http://quotes.rest/qod",
        .event_handler = clientEventHandler,
        //.username = "user",
        //.password = "secret",
        //.port = 80,
        //.timeout_ms = 2000,
        //.buffer_size = 300,
        //.buffer_size_tx = 300,
        .user_data = (void *)&qodFetchParams
    };

    clientFetch(&qodConfig); //blocks until complete, result in rxBuf

    if (xSemaphoreGive(connectionSemaphore) == pdFALSE ) {
        ESP_LOGE(TAG,"Unimaginable");
    }
    ESP_LOGI(TAG, "qodget finished with err=%d, ret=[%d],\r\n--%s--", qodFetchParams.err, qodFetchParams.status, qodFetchParams.rxBuf);

    //Analyze results. Status = 200 is good. One failure can be:
    // {
    //     "error": {
    //         "code": 429,
    //         "message": "Too Many Requests: Rate limit of 10 requests per hour exceeded. Please wait for 18 minutes and 27 seconds."
    //     }
    // }
    if (qodFetchParams.status == 429) strcpy(qodFetchParams.rxBuf,"qod Requests exceeded per hour.");
}

void SMSsend(char *mobile) {
    /* Our provider is messagemedia.com
    1: goto API and get an API key and API secret
    2: goto (developer tools) authentication and get Example request with Basic Authentication

POST /v1/messages HTTP/1.1
Host: api.messagemedia.com
Accept: application/json
Content-Type: application/json
Authorization: Basic dGhpc2lzYWtleTp0aGlzaXNhc2VjcmV0Zm9ybW1iYXNpY2F1dGhyZXN0YXBp
{
  "messages": [
    {
      "content": "Hello World",
      "destination_number": "+61491570156",
      "format": "SMS"
    }
  ]
} 

    Note: Host=url, POST=follows url (url/v1/messages), Accept/Content/Auth=headers
    Note: long Auth string is encoded block described previously:

Authorization: Basic Base64(api_key:api_secret)

    3: goto Postman and start a new request using information from the Example.
        select POST
        https://apimessagemedia.com/v1/messages
        >Headers
            Accepts        application/json
            Content-Type   application/json
            (Can't seem to get rid of auto-generated headers but ok)
        >Auth
            Type is Basic Auth
            Username is api key (step 1)
            Password is api secret (step 1)
            NOW THE AUTHENTICATION HEADER IS FILLED IN
        >Body
            Select "raw"
            Paste in the body from example (step 2)
            Change content message and phone number
        Click Send
        Returns (hopefully) 202 accepted with a body of stuff
        But 202 doesn't mean your text number was correct!!!


        */

    ESP_LOGI(TAG, "SMS awaiting turn...");
    //if (xSemaphoreTake(connectionSemaphore, 10000 / portTICK_RATE_MS))
    xSemaphoreTake(connectionSemaphore, portMAX_DELAY); //wait indefinitely

    struct appFetchParams SMSFetchParams = {
        .method = Post,
        .headerCount = 3,
        .body = txClientBuf,
        .OnGotData = NULL,
        .rxBuf = rxClientBuf,
        .rxLast = sizeof(rxClientBuf) - 1 //last element set to NULL for safety.
    };
    //Don't do this inside initializer or compiler complains you didn't do them all.
    SMSFetchParams.header[0].key = "Content-Type",
    SMSFetchParams.header[0].val = "application/json",
    SMSFetchParams.header[1].key = "Accept",
    SMSFetchParams.header[1].val = "application/json",
    SMSFetchParams.header[2].key = "Authorization",
    SMSFetchParams.header[2].val = "Basic c09keU5SOU5aVlhtbHVQb3czQ3E6T1c0bVlnaWgzQ0ZSelRlQXg4NTNZa21xejZId0tI",

    //Create JSON body with message and phone number.
    sprintf(SMSFetchParams.body,
          "{"
            "\"messages\": ["
              "{"
                "\"content\": \"%s\","
                "\"destination_number\": \"%s\","
                "\"format\": \"SMS\""
              "}"
            "]"
          "}",
          rxClientBuf, mobile);
    
    // If you want to use JSON to create the body, see: \esp-idf\components\json\cJSON>readme
    //      or https://github.com/espressif/esp-idf/blob/master/components/json/README
    // cJSON *root,*mess;
    // root = cJSON_CreateObject();
    // cJSON_AddItemToObject(root, "messages", mess=cJSON_CreateObject());
    // cJSON_AddArrayToObject(mess, );

    //ESP_LOGI(TAG,"To messagemedia: %s",SMSFetchParams.body);

    // one of these for every webpage you want
    esp_http_client_config_t clientConfig = {
        .url = "https://api.messagemedia.com/v1/messages",
        .event_handler = clientEventHandler,
        //.username = "user",
        //.password = "secret",
        //.port = 80,
        //.timeout_ms = 2000,
        //.buffer_size = 300,
        //.buffer_size_tx = 300,
        .user_data = (void *)&SMSFetchParams
    };


//clientFetch(&clientConfig); //blocks until complete, result in rxBuf


    if (xSemaphoreGive(connectionSemaphore) == pdFALSE ) {
        ESP_LOGE(TAG,"Unimaginable");
    }
    ESP_LOGI(TAG, "SMS finished with err=%d, ret=[%d], tried=%s", SMSFetchParams.err, SMSFetchParams.status, SMSFetchParams.rxBuf);
    // [202]
    //Analyze results. Status in 200's usually good. One failure can be:
 
}