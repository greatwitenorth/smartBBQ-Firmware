// This #include statement was automatically added by the Spark IDE.
#include "PID.h"
#include "SmartProbe.h"
#include "SmartBbq.h"
#include "Adafruit_LEDBackpack.h"
#include "SimpleButton.h"
#include "ThermistorProbe.h"
#include "SimpleLed.h"
#include "math.h"

#define PROBE1 A6
#define PROBE2 A5
#define PROBE3 A4
#define PROBE4 A3

#define PROBEINDICATOR1 D7
#define PROBEINDICATOR2 D6
#define PROBEINDICATOR3 D5
#define PROBEINDICATOR4 D4

#define BUTTON_LEFT A2
#define BUTTON_CENTER D3
#define BUTTON_RIGHT D2

#define FAN1 A1
#define FAN2 A0

#define LEDCYCLE 300U
#define ALARMSETDELAY 800U

#define DEBUG FALSE

const double version = 1.0;

unsigned long lastPublished = 0UL;

char alarmData[40];
char oldAlarmData[40];

char tempData[40];
char oldTempData[40];

// API variables
double probeTemp1 = 0.0;
double probeTemp2 = 0.0;
double probeTemp3 = 0.0;
double probeTemp4 = 0.0;

double probeAlarm1 = 0.0;
double probeAlarm2 = 0.0;
double probeAlarm3 = 0.0;
double probeAlarm4 = 0.0;

bool prevProbeAlarmE1 = FALSE;
bool prevProbeAlarmE2 = FALSE;
bool prevProbeAlarmE3 = FALSE;
bool prevProbeAlarmE4 = FALSE;

bool probeAlarmE1 = FALSE;
bool probeAlarmE2 = FALSE;
bool probeAlarmE3 = FALSE;
bool probeAlarmE4 = FALSE;

// display timer
unsigned long ledLastMillis = 0;

// alarm set timer
unsigned long alarmFirstMillis = 0;

// Define display
Adafruit_7segment display = Adafruit_7segment();

// Define buttons
SimpleButton buttonLeft(BUTTON_LEFT);
SimpleButton buttonCenter(BUTTON_CENTER);
SimpleButton buttonRight(BUTTON_RIGHT);

// Define SmartBbq
SmartBbq bbq(PROBE1,PROBE2,PROBE3,PROBE4,PROBEINDICATOR1,PROBEINDICATOR2,PROBEINDICATOR3,PROBEINDICATOR4,FAN1,FAN2);

//Define PID Variables we'll be connecting to
double Setpoint, Input, Output;

//Specify the links and initial tuning parameters
PID myPID(&Input, &Output, &Setpoint,2,5,1, DIRECT);

void setup() {
    // init serial
    Serial.begin(9600);

    // init display
    display.begin(0x70);

    // Fan test
    pinMode(FAN1, OUTPUT);
    pinMode(FAN2, OUTPUT);

    digitalWrite(FAN1, LOW);

    //initialize PID variables we're linked to
    Input = bbq.getProbeTemp(3);
    Setpoint = bbq.getProbeAlarm(3);

    //turn the PID on
    myPID.SetMode(AUTOMATIC);

    // Our verison number as well as the identifier to the app that this core is meant for BBQ'ing!
    Spark.variable("smartBBQ", (void*)&version, DOUBLE);

    // API - setup
    Spark.variable("probeTemp1", &probeTemp1, DOUBLE);
    Spark.variable("probeTemp2", &probeTemp2, DOUBLE);
    Spark.variable("probeTemp3", &probeTemp3, DOUBLE);
    Spark.variable("probeTemp4", &probeTemp4, DOUBLE);

    Spark.variable("probeAlarm1", &probeAlarm1, DOUBLE);
    Spark.variable("probeAlarm2", &probeAlarm2, DOUBLE);
    Spark.variable("probeAlarm3", &probeAlarm3, DOUBLE);
    Spark.variable("probeAlarm4", &probeAlarm4, DOUBLE);


    // Debug delay
    if(DEBUG) {
        delay(3000);
    }
}

void loop() {
    // update display
    updateDisplay();

    // check if a button was pressed
    checkButtons();

    // update fan
    updatePID();

    // API - update variables
    probeTemp1 = bbq.getProbeTemp(0);
    probeTemp2 = bbq.getProbeTemp(1);
    probeTemp3 = bbq.getProbeTemp(2);
    probeTemp4 = bbq.getProbeTemp(3);

    probeAlarm1 = bbq.getProbeAlarm(0);
    probeAlarm2 = bbq.getProbeAlarm(1);
    probeAlarm3 = bbq.getProbeAlarm(2);
    probeAlarm4 = bbq.getProbeAlarm(3);

    probeAlarmE1 = bbq.getProbeAlarmE(0);
    probeAlarmE2 = bbq.getProbeAlarmE(1);
    probeAlarmE3 = bbq.getProbeAlarmE(2);
    probeAlarmE4 = bbq.getProbeAlarmE(3);

    // Only publish a max of every 1.5 seconds
    unsigned long now = millis();
    if(now - lastPublished > 1500UL) {
      lastPublished = now;

      // Eventually once the 63 char limit is raised, we can send an entire json
      // object of our probes. For now we'll split things up.
      sprintf(tempData,"[%.0f,%.0f,%.0f,%.0f]",0,0,0,probeTemp4);

      if(strcmp(tempData, oldTempData) != 0){
        Spark.publish("temps", tempData);
        lastPublished = now;
      }

      strncpy(oldTempData, tempData, 40);

      // Publish our alarm if an alarm has changed state
      if( probeAlarmE1 != prevProbeAlarmE1 ||
          probeAlarmE2 != prevProbeAlarmE2 ||
          probeAlarmE3 != prevProbeAlarmE3 ||
          probeAlarmE4 != prevProbeAlarmE4 ){
        sprintf(alarmData,"[%d,%d,%d,%d]",probeAlarmE1,probeAlarmE2,probeAlarmE3,probeAlarmE4);
        Spark.publish("alarms", alarmData);
        lastPublished = now;
      }

      // Record this state for next time
      prevProbeAlarmE1 = probeAlarmE1;
      prevProbeAlarmE2 = probeAlarmE2;
      prevProbeAlarmE3 = probeAlarmE3;
      prevProbeAlarmE4 = probeAlarmE4;
    }

    // delay for short period just makes things work nicely
    delay(1);
}

void checkButtons() {
    if(buttonLeft.isPressed()) {
        bbq.down();
        alarmFirstMillis = millis();
        printAlarm();
    } else if(buttonCenter.isPressed()) {
        bbq.up();
        alarmFirstMillis = millis();
        printAlarm();
    } else if(buttonRight.isPressed()) {
        bbq.select();
        printTemp();
    }
}

void updateDisplay() {
    if(showAlarm()) {
        printAlarm();
    } else {
        if(cycleCheck(&ledLastMillis, LEDCYCLE))
        {
            printTemp();
        }
    }
}

void updatePID() {
    Setpoint = bbq.getProbeAlarm(3); // target temp
    Input = bbq.getProbeTemp(3); // current temp
    myPID.Compute();
    analogWrite(FAN2,Output);
}

void printTemp() {
    double temp = bbq.getActiveTemp();

    if(temp > 50.0) {
        display.printFloat(temp);
    } else {
        display.printError();
    }

    display.writeDisplay();
}

void printAlarm() {
    double alarm = bbq.getActiveAlarm();

    display.printFloat(alarm);
    display.writeDisplay();
}

boolean cycleCheck(unsigned long *lastMillis, unsigned int cycle) {
    unsigned long currentMillis = millis();

    if(currentMillis - *lastMillis >= cycle) {
        *lastMillis = currentMillis;
        return true;
    } else {
        return false;
    }
}

boolean showAlarm() {
    if(millis() - alarmFirstMillis >= ALARMSETDELAY) {
        return false;
    } else {
        return true;
    }
}
