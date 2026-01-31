
#ifndef ESP32_PWM_SERVO_H
#define ESP32_PWM_SERVO_H

#include "Arduino.h"
#include "driver/ledc.h"


#ifndef ESP32
 	#error "this library is only for ESP32"
#endif

//#define ESP32_PWM_SERVO_DEBUG   						// If you comment this line, the DPRINT & DPRINTLN lines are defined as blank.

#ifdef ESP32_PWM_SERVO_DEBUG    						// Macros are usually in all capital letters.
   #define DPRINT(...)    Serial.print(__VA_ARGS__)     // DPRINT is a macro, debug print
   #define DPRINTLN(...)  Serial.println(__VA_ARGS__)   // DPRINTLN is a macro, debug print with new line
#else
   #define DPRINT(...)     //now defines a blank line
   #define DPRINTLN(...)   //now defines a blank line
#endif

#define DEF_PWM_FREQ	 50		// PWM frequency for servo pulse signal


#define MIN_HW_ANGLE	  0		// lowest harware angle default value.
#define MAX_HW_ANGLE	180		// higher harware angle default value.

#define LOW_US_TICK	   500		// default duration of a low PWM pulse in µsec
#define HIGH_US_TICK  2500      // default duration of a high PWM pulse in µsec

#define NOT_SET			-1

#define CLOCKWISE		0
#define COUNTERCLOCWISE	1

#define SPEED_MS_DEG	    200 // default servo whith speed control, in ms per degrees
#define MAX_TIMEMARK_DELAY	200	// max duration between two servo position update in goToAtSpeed()

class ESP32_PWM_Servo
{
  public:
				ESP32_PWM_Servo();

	bool		useTimer(int8_t timer);			// Provide specific (allready runing) PWM timer to attach() function
	bool		useChannel(int8_t channel);		// Provide (force) a specific PWM channel to attach() function
	bool		setTickDuration(uint16_t lowUsTick, uint16_t highUsTick); // set low/high PWM pusle duration for min/max angle 
	bool		setHwAngles(int16_t minHwAngle, int16_t maxHwAngle); // set min/max hardware angle while low/highUsTick is write

	bool		attach(uint8_t pwmPin, uint32_t pwmFreq = 0);
	bool		setLimits(int16_t minAngle, int16_t maxAngle);	// set min/max angle limits

	uint32_t	writeMicroseconds(uint16_t us);				// write a PWM tick duration. Usefull to get calibration values for setTickDuration()
	bool		goToAngle(float angle);		 				// set servo angle (into "minAngle" - "maxAngle" range)
	bool		goToAtSpeed(float angle, uint16_t speed);	// set servo angle to reach at speed (in ms/deg)
//	void		stop();										// stop servo rotation (only available in controled speed mode)
//	float		getSpeed();									// get servo speed (only available in controled speed mode. positive = clockwise, negative = counterclockwise)
	int8_t		getPwmTimer();								// Return the timer used for the PWM signal (0-3; -1 if not set)
	uint32_t	getPwmFreq();								// Return the frequency of the PWM signal or -1 if an error occure
	uint32_t	getMaxDutyVal();							// Return maximum value of PWM duty cycle or -1 if an error occure
//	int8_t		getServoPin();								// Return the pin used to drive the servo (from ledc struct)

  private:
	static ledc_timer_config_t	*_timers_config[LEDC_TIMER_MAX];		// PWM timers config pointers array
	static uint8_t				_clients_for_timer[LEDC_TIMER_MAX];		// PWM timer's clients (channels) counters
	static int8_t				_default_timer;							// PWM timer use as default timer 
	static bool					_pwm_channel_used[LEDC_CHANNEL_MAX];	// PWM channel used by all classes
	ledc_channel_config_t 		*_ledc_channel_config = NULL;			// PWM channel config structure
	
	int16_t		_minHwAngle;		// minimum angle permit by hardware 
	int16_t		_maxHwAngle;		// maximum angle permit by hardware
	uint16_t	_lowUsTick;			// low PWM pulse duration in µsec
	uint16_t	_highUsTick;		// high PWM pulse duration in µsec

	int16_t		_minAngle;			// minimum angle limit
	int16_t		_maxAngle;			// maximum angle limit

	int8_t 		_pwmTimer;		// timer use for PWM clock
	int8_t 		_pwmChannel;	// channel use for pwm clock
	uint32_t	_pwmMaxDuty;	// maximum value of PWM duty cycle. (0 to (2^20_bit_duty_resolution); -1 for error)

	uint32_t	_speedMsDeg;	// current speed in ms per degre
	uint32_t	_timeMark;		// use to store last position update for goToAtSpeed()
	float		_curPos;		// last know position of the servo

	bool		angleIsValid(float angle);		    // check if angle is in MIN_ANGLE-MAX_ANGLE range
	uint16_t	angleToUs(float angle);				// convert an angle to PWM pulse duration, with min/max angle compensation
	uint32_t	usToDuty(uint16_t us);				// convert PWM pulse duration to PWM duty cycle value
	uint16_t	dutyToUs(uint32_t duty);			// convert PWM duty cycle value to PWM pulse duration

	static int	espSilentLog(const char* string, va_list args);	// (catch esp log output) and return number of char print
	static int	_logHasOccure;	// true (not null) if an esp log has occure in silent mode (catch by espSilentLog)
};

/* TO DO:
- Move attach() PWM auto manager into a separate library (brocker)
	-> Move PWM freq/channel to class contructor
- Create detach() function
- Create destructor (with PWM unregistration, static variable freeup ...)
*/
#endif // ESP32_PWM_SERVO_H