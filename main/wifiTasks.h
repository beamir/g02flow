#ifndef _WIFITASKS_H_
#define _WIFITASKS_H_

// WIFI PROVISIONING, WEB SERVER, NVM FUNCTIONS

//call first, starts wifi_manager, then other wifi tasks
void wifiSetup(void);



// CLIENT FUNCTIONS

//gets quote of day
void qodget(void);

//sends a text
void SMSsend(char *mobile);

#endif