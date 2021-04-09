#ifndef _NVSTASKS_H_
#define _NVSTASKS_H_

#define FILEREAD true
#define FILEWRITE false

//Array-Of-Structures keys are 16 bits.
typedef uint16_t aosKey_t;

//List all array-of-structures files
enum aosFiles {
    DURATIONS,
    VOLTAGES,
    noOfFiles //must be last in list
};

//need possibly 10 stations x 3 waterings x 7 days x baseline/previous/current = 630 durations
typedef struct {
    int startTime; //seconds since cycle start
    int flowTime;  //seconds
    int totalFlow; //pulses
    int avgFlow;   // totalFlow/flowTime
    int startTimeDiff; // from baseline
    int flowTimeDiff; // from baseline
    int totalFlowDiff; // from baseline
    int avgFlowDiff; // from baseline
} oDuration;
typedef oDuration * pDuration;
#define showDuration(rcd) printf("startTime=%d, flowTime=%d etc.\n", rcd.startTime, rcd.flowTime)
#define readDuration(key, rcdName) aosfile(FILEREAD, DURATIONS, (key), (void *)&rcdName)
#define writeDuration(key, rcdName) aosfile(FILEWRITE, DURATIONS, (key), (void *)&rcdName)
//example:  Duration rcdFirst;
//          readDuration(3, rcdFirst);
//      or  if (readDuration(2,rcdFirst)) else printf("not found");


// power history every 5 minutes?
// solar voltage, bat voltage, seconds ago
// keep last 2 weeks? = 12/hr * 24 * 14 = 4032
typedef struct {
    uint16_t batterymV; //millivolts
    uint16_t solarmV; //millivolts
    time_t timestamp;
} oVoltage;
typedef oVoltage * pVoltage;
#define showVoltage(rcdName) printf("Battery %d mV, solar %d mV on %s", rcdName.batterymV, rcdName.solarmV, displayTime(rcdName.timestamp))
#define readVoltage(key, rcdName) aosfile(FILEREAD, VOLTAGES, (key), (void *)&rcdName)
#define writeVoltage(key, rcdName) aosfile(FILEWRITE, VOLTAGES, (key), (void *)&rcdName)


void filesInit(void); //call once
void filesStatus(void); //call if curious
//Not intended to be called directly. Use easier macros.
bool aosfile(bool readWrite, uint8_t fileNo, aosKey_t key, void * rcd);

#endif