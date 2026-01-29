
#ifndef ESP32_PWM_MOTOR_H
#define ESP32_PWM_MOTOR_H

#include "Arduino.h"
#include "driver/ledc.h"


#ifndef ESP32
 	#error "this library is only for ESP32"
#endif

// #define ESP32_PWM_MOTOR_DEBUG   						// If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.

#ifdef ESP32_PWM_MOTOR_DEBUG    						// Macros are usually in all capital letters.
   #define DPRINT(...)    Serial.print(__VA_ARGS__)     // DPRINT is a macro, debug print
   #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   // DPRINTLN is a macro, debug print with new line
#else
   #define DPRINT(...)     //now defines a blank line
   #define DPRINTLN(...)   //now defines a blank line
#endif

#define DEF_PWM_FREQ	400
#define NOT_SET			-1

#define CLOCKWISE		0
#define COUNTERCLOCWISE	1

#define MIN_SPEED        0.00
#define MAX_SPEED      100.00

#define ACTIVE_LOW	0
#define ACTIVE_HIGH	1

class ESP32_PWM_Motor
{
  public:
				ESP32_PWM_Motor();

	bool		useTimer(int8_t timer);			// Provide specific (allready runing) PWM timer to begin() function
	bool		useChannel(int8_t channel);		// Provide (force) a specific PWM channel to begin() function

	bool		attach(uint8_t pwmPin, int8_t dirPin = NOT_SET, uint32_t pwmFreq = 0);

	bool		setBreakPin(uint8_t breakPin, uint8_t mode);								// set motor driver break pin and its active mode
	bool		setSleepPin(uint8_t sleepPin, uint8_t mode);								// set motor driver sleep pin and its active mode
	bool		setMargin(uint8_t minMargin = MIN_SPEED, uint8_t maxMargin = MAX_SPEED);	// set motor min/max margin to remove dead zone and/or limit speed

	bool		runAtSpeed(float speed);		 				// set motor speed in % (0-100%, positive = clockwise, negative = counterclockwise)
	bool		accelToSpeed(float speed, uint32_t accel);		// set motor speed in % (0-100%, positive = clockwise, negative = counterclockwise) with acceleration
	void		stop();											// stop motor rotatation
	bool		doBreak();										// enable motor driver break mode
	bool		doNotBreak();									// disable motor driver break mode
	bool		sleep();										// set motor driver into sleep mode
	bool		doNotSleep();									// set motor driver into active mode
	bool		wakeup();										// wakeup motor driver from sleep mode

	float		getSpeed();						// get motor current speed (positive = clockwise, negative = counterclockwise)
	bool		isMoving();						// return true if motor speed consign is != 0
	bool		isBreaking();					// return true if motor driver break, false if not
	bool		isSleeping();					// return true if motor driver is sleeping, false if not

	int8_t		getPwmTimer();					// Return the timer used for the PWM signal (0-3; -1 if not set)
	uint32_t	getPwmFreq();					// Return the frequency of the PWM signal or -1 if an error occure
	uint32_t	getMaxDutyVal();				// Return maximum value of PWM duty cycle or -1 if an error occure


  private:
	static ledc_timer_config_t	*_timers_config[LEDC_TIMER_MAX];		// PWM timers config pointers array
	static uint8_t				_clients_for_timer[LEDC_TIMER_MAX];		// PWM timer's clients (channels) counters
	static int8_t				_default_timer;							// PWM timer use as default timer 
	static bool					_pwm_channel_used[LEDC_CHANNEL_MAX];	// PWM channels used by all classes
	ledc_channel_config_t 		*_ledc_channel_config = NULL;			// PWM channel config structure
	
	
	int8_t 		_dirPin;			// pin connected to motor driver direction pin
	int8_t 		_breakPin;			// pin connected to motor driver break pin
	int8_t 		_breakPinMode;		// mode of motor driver break pin (ACTIVE_LOW/ACTIVE_HIGH)
	int8_t 		_sleepPin;			// pin connected to motor driver sleep pin
	int8_t 		_sleepPinMode;		// mode of motor driver sleep pin (ACTIVE_LOW/ACTIVE_HIGH)

	uint8_t		_minMargin;			// minimum (startup) speed of the motor (0-100%)
	uint8_t		_maxMargin;			// maximum (limit) speed of the motor (0-100%)
	bool		_marginAreSet;		// flag use to check if custom margin are set or no 

	int8_t 		_pwmTimer;		// timer use for PWM clock
	int8_t 		_pwmChannel;	// channel use for pwm clock
	uint32_t	_pwmMaxDuty;	// maximum value of PWM duty cycle. (0 to (2^20_bit_duty_resolution); -1 for error)
	uint32_t	_accel_factor;	// acceleration factor in ms per speed %

	bool		accelIsValid(uint32_t accel);		// check if acceleration factor is valid
	bool		speedIsValid(float speed);		    // check if speed is in MIN_SPEED-MAX_SPEED range

	uint32_t	speedToDuty(float speed);			// convert speed in PWM duty cycle value
	float		dutyToSpeed(uint32_t duty);			// raw convert duty cycle to speed, with no direction
	float		speedInMargin(float speed);			// map speed into min/max margin range (c.f. setMotorMargin() )
	float		revertMargedSpeed(float speed);		// revert a speed mapped into margin to it's base value
	bool		dirPinFromSpeed(float speed);		// set dirPin direction from speed value

	static int	espSilentLog(const char* string, va_list args);	// (catch esp log output) and return number of char print
	static int	_logHasOccure;	// true (not null) if an esp log has occure in silent mode (catch by espSilentLog)
};

/* TO DO:
- Move attach() PWM auto manager into a separate library (brocker)
	-> Move PWM freq/channel to classcontructor
- Create detach() function (durring operation, remove ledc_fade_func_install for last client)
- Create destructor (with PWM unregistration, static variable freeup ...)
*/
#endif // ESP32_PWM_MOTOR_H