// Platform setup ------------------------------------------------------------------------------------

// We define a more generic symbol, in case more Teensy boards based on different lines are supported
// ESP32

// Lower limit (fastest) step rate in uS for this platform -------------------------------------------
#define MaxRate_LowerLimit 2

// New symbols for the Serial ports so they can be remapped if necessary -----------------------------
#define SerialA Serial
// SerialA is always enabled, SerialB and SerialC are optional
#define SerialB Serial2
#define HAL_SERIAL_B_ENABLED
#include <BluetoothSerial.h>
BluetoothSerial SerialC;
#define HAL_SERIAL_C_ENABLED
#define HAL_SERIAL_C_BLUETOOTH

// New symbol for the default I2C port ---------------------------------------------------------------
#include <Wire.h>
#define HAL_Wire Wire

//--------------------------------------------------------------------------------------------------
// Interrupts

#define ISR(f) void f (void)
hw_timer_t * itimer1 = NULL;
void TIMER1_COMPA_vect(void);

hw_timer_t * itimer3 = NULL;
void TIMER3_COMPA_vect(void);

hw_timer_t * itimer4 = NULL;
void TIMER4_COMPA_vect(void);

portMUX_TYPE siderealTimerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE motorTimerMux = portMUX_INITIALIZER_UNLOCKED;

void timerAlarmsEnable() { timerAlarmEnable(itimer1); timerAlarmEnable(itimer3); timerAlarmEnable(itimer4); }
void timerAlarmsDisable() { timerAlarmDisable(itimer1); timerAlarmDisable(itimer3); timerAlarmDisable(itimer4); }

// Non-volatile storage ------------------------------------------------------------------------------
#if defined(NV_AT24C32)
  #include "../drivers/NV_I2C_EEPROM_AT24C32.h"
#elif defined(NV_MB85RC256V)
  #include "../drivers/NV_I2C_FRAM_MB85RC256V.h"
#else
  #include "../drivers/NV_EEPROM_ESP.h"
#endif

// Use an RTC (Real Time Clock) if present -----------------------------------------------------------
#include "../drivers/RTCw.h"

//--------------------------------------------------------------------------------------------------
// Initialize timers

// frequency compensation (F_COMP/1000000.0) for adjusting microseconds to timer counts
#define F_COMP 16000000.0

void HAL_Init(void) {
}

extern long int siderealInterval;
extern void SiderealClockSetInterval (long int);

// Init Axis1 and Axis2 motor timers and set their priorities
void HAL_Init_Timers_Motor() {
  // set the system timer for millis() to the second highest priority
  itimer3 = timerBegin(2, 5, true);  // the timer frequency of an ESP32 is 80MHz.  80/5 = 16 MHz (which is a timer count in OnStep due to it's Mega2560 heritage.)
  timerAttachInterrupt(itimer3, &TIMER3_COMPA_vect, true);
  timerAlarmWrite(itimer3, 1000, true);
  timerAlarmEnable(itimer3);
  
  itimer4 = timerBegin(3, 5, true);
  timerAttachInterrupt(itimer4, &TIMER4_COMPA_vect, true);
  timerAlarmWrite(itimer4, 1000, true);
  timerAlarmEnable(itimer4);
}

//--------------------------------------------------------------------------------------------------
// Set timer1 to interval (in microseconds*16), for the 1/100 second sidereal timer

// Init sidereal clock timer
void HAL_Init_Timer_Sidereal() {
  itimer1 = timerBegin(1, 5, true);
  timerAttachInterrupt(itimer1, &TIMER1_COMPA_vect, true);
  timerAlarmWrite(itimer1, 1000, true);
  timerAlarmEnable(itimer1);

  SiderealClockSetInterval(siderealInterval);
}

void Timer1SetInterval(long iv, double rateRatio) {
  iv=round(((double)iv)/rateRatio);
  timerAlarmWrite(itimer1, iv, true);
}

//--------------------------------------------------------------------------------------------------
// Quickly reprogram the interval (in microseconds*(F_COMP/1000000.0)) for the motor timers, must work from within the motor ISR timers

IRAM_ATTR void QuickSetIntervalAxis1(uint32_t r) {
  timerAlarmWrite(itimer3, r, true);
}

IRAM_ATTR void QuickSetIntervalAxis2(uint32_t r) {
  timerAlarmWrite(itimer4, r, true);
}

// --------------------------------------------------------------------------------------------------
// Fast port writing help

#define CLR(x,y) (x&=(~(1<<y)))
#define SET(x,y) (x|=(1<<y))
#define TGL(x,y) (x^=(1<<y))

// We use standard #define's to do **fast** digitalWrite's to the step and dir pins for the Axis1/2 stepper drivers
#define StepPinAxis1_HIGH digitalWrite(Axis1StepPin, HIGH)
#define StepPinAxis1_LOW digitalWrite(Axis1StepPin, LOW)
#define DirPinAxis1_HIGH digitalWrite(Axis1DirPin, HIGH)
#define DirPinAxis1_LOW digitalWrite(Axis1DirPin, LOW)

#define StepPinAxis2_HIGH digitalWrite(Axis2StepPin, HIGH)
#define StepPinAxis2_LOW digitalWrite(Axis2StepPin, LOW)
#define DirPinAxis2_HIGH digitalWrite(Axis2DirPin, HIGH)
#define DirPinAxis2_LOW digitalWrite(Axis2DirPin, LOW)

