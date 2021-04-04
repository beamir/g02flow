#ifndef _PINTASKS_H_
#define _PINTASKS_H_

//call first
void LEDsetup(void);

//call after, whenever you wish, only 3 blink requests concurrently, else ignored
void LEDblink(int count);

#endif