#include "ESP32_PWM_Servo.h"

#include "Arduino.h"

// #include "esp32-hal.h"
// #include "soc/soc_caps.h"
#include "driver/ledc.h"

ledc_timer_config_t *ESP32_PWM_Servo::_timers_config[] = {NULL};	// init timers config pointers
uint8_t	ESP32_PWM_Servo::_clients_for_timer[] = {0};				// init timers clients channels counter
int8_t	ESP32_PWM_Servo::_default_timer = NOT_SET;
bool	ESP32_PWM_Servo::_pwm_channel_used[] = {0};				    // init channels usage log array

int		ESP32_PWM_Servo::_logHasOccure = NOT_SET;					// init espErr char counter attribute

/////////////////////////////////////////////////////////////////////////////////////
/*	ESP32_PWM_Servo constructor - build an instance of the class
/		Initalize servo default (safe) atributes values                            */
/////////////////////////////////////////////////////////////////////////////////////
ESP32_PWM_Servo::ESP32_PWM_Servo()
{	
	_pwmTimer	= NOT_SET;
	_pwmChannel	= NOT_SET;
	_pwmMaxDuty	= NOT_SET;

		// set min/max hardware angle to default value
	_minHwAngle = MIN_HW_ANGLE;
	_maxHwAngle = MAX_HW_ANGLE;

		// set min/max user angles limits to default value
	_minAngle = _minHwAngle;
	_maxAngle = _maxHwAngle;

		// set default PWM min/max pulse duration
	_lowUsTick  = LOW_US_TICK;
	_highUsTick = HIGH_US_TICK;

		// set speed control default variable
	_speedMsDeg = SPEED_MS_DEG;
	_timeMark = millis();
	_curPos = _minHwAngle - 1;	// not set
}

/////////////////////////////////////////////////////////////////////////////////////
/*	useTimer(timer) - set a specific timer for PWM signal before attach()
/		uint8_t timer: ledc high speed timer to use (0-3)
/	NOTE: Can be used once and before attach() (will be ignore otherwise)           
/         Timer of another motor can be use. Use thisMotor.getPwmTimer() to get it */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::useTimer(int8_t timer)
{
	if ((timer < LEDC_TIMER_MAX) && (_pwmTimer == NOT_SET)) {
		_pwmTimer = timer;
		return EXIT_SUCCESS;
	}
	
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	useChannel(channel) - set a specific channel for PWM signal before attach()
/		uint8_t channel: ledc high speed channel to use (0-7)
/	NOTE: Can be used once and before attach() (will be ignore otherwise)           
/         Usefull to force PWM channel to use if a conflict is meet                */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::useChannel(int8_t channel)
{
	if ((channel < LEDC_CHANNEL_MAX) && (_pwmChannel == NOT_SET)) {
		_pwmChannel = channel;
		
		return EXIT_SUCCESS;
	}
	
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	setTickDuration(lowUsTick, highUsTick) - set low/high PWM pusle duration
/		uint16_t lowUsTick: minimum speed at which the motor start
/		uint16_t highUsTick: maximum motor speed limit (in booth direction)          */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::setTickDuration(uint16_t lowUsTick, uint16_t highUsTick)
{																												DPRINTLN("  Setting up low/high PWM pulse duration");
	if ((lowUsTick >= 0) && (lowUsTick < highUsTick)) {															DPRINT("    -> low/high PWM pulse duration set to "); DPRINT(lowUsTick); DPRINT(" - "); DPRINTLN(highUsTick);
		_lowUsTick = lowUsTick;
		_highUsTick = highUsTick;

		return EXIT_SUCCESS;
	}
																												DPRINTLN("    -> Bad PWM pulse duration provided or fuction call prior attach().");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	setHwAngles(minHwAngle, maxHwAngle) - set set min/max hardware angle limits
/		uint16_t minHwAngle: minimum angle when lowUsTick is write
/		uint16_t maxHwAngle: maximum angle when lowUsTick is write                 */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::setHwAngles(int16_t minHwAngle, int16_t maxHwAngle)
{																												DPRINTLN("  Setting up servo physical limits");
	if ((minHwAngle < maxHwAngle)) {																			DPRINT("    -> min/max hardware angles range set from "); DPRINT(minHwAngle); DPRINT(" to "); DPRINTLN(maxHwAngle);
		_minHwAngle = minHwAngle;
		_maxHwAngle = maxHwAngle;

		return EXIT_SUCCESS;
	}
//	+++ setLimits with new hw angle if need	
//	+++ update _curPos = _minHwAngle - 1; if _curPos = "previous_minHwAngle" - 1; (keep not set)																									DPRINTLN("    -> Bad angles provided for HW limits or fuction call prior attach().");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	attach(pwmPin, pwmFreq) - setup servo pin and PWM signal frequency
/		uint8_t pwmPin: pin connected to the servo data pin
/		uint32_t pwmFreq: frequency of PWM output (= DEF_PWM_FREQ if not set)      */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::attach(uint8_t pwmPin, uint32_t pwmFreq)									
{																												DPRINTLN("Setting up PWM signal generator ...");
		// setup PWM timer if not previously configured before attach()
	if (_pwmTimer == NOT_SET) {																					DPRINTLN("  - PWM timer selection :");
			// use default timer if no pwmFreq is provided and if default timer is already defined ...
		if ((pwmFreq == 0) && (_default_timer != NOT_SET)) {													DPRINT("    -> No specific PWM frequency provided. Using previously configured timer "); DPRINT(_default_timer); DPRINTLN(" as default timer.");
			_pwmTimer = _default_timer;
		}
			// ... or select a free PWM timer if available.
		else {																									DPRINTLN("    -> Testing available timer");
			for (uint8_t timer = 0; timer <= LEDC_TIMER_MAX; timer++)
			{		// use first free timer ...
				if (_clients_for_timer[timer] == 0) {															DPRINT("    -> Timer "); DPRINT(timer); DPRINTLN(" free. Using it for PWM signal.");
					_pwmTimer = timer;
					
					break;
				}
					// ... or exit with error if no one are available
				if (timer == (LEDC_TIMER_MAX - 1)) {															DPRINTLN("    -> No suitable free timer available. Abording configuration.");
					return EXIT_FAILURE;
				}
			}
			
				// define default pwm_timer if not already set
			if (_default_timer == NOT_SET) {
					// set PWM frequency to default value if not provided
				if (!pwmFreq) {																					DPRINTLN("    -> No frequency provided to configure timer. Using default value.");
					pwmFreq = DEF_PWM_FREQ;
				}
					// set this timer as default timer
				_default_timer = _pwmTimer;																		DPRINT("    -> Using this timer as default timer with "); DPRINT(pwmFreq); DPRINTLN(" Hz base frequency.");
			}
		}
		
			// create PWM timer config structure if not yet set
		if (_timers_config[_pwmTimer] == NULL) {																DPRINT("  - Setting up timer "); DPRINT(_pwmTimer); DPRINTLN(" configuration.");
			_timers_config[_pwmTimer] = new ledc_timer_config_t;				// create timer config structure
			
				// seeding timer config parameters 
			_timers_config[_pwmTimer]->speed_mode	= LEDC_LOW_SPEED_MODE;		// set timer mode
			_timers_config[_pwmTimer]->freq_hz		= pwmFreq;					// set frequency of PWM signal
			_timers_config[_pwmTimer]->timer_num	= (ledc_timer_t)_pwmTimer;	// set timer index
			_timers_config[_pwmTimer]->clk_cfg		= LEDC_AUTO_CLK;			// set LEDC source clock

				// find max PWM resolution for pwmFreq
			uint8_t resBit = LEDC_TIMER_BIT_MAX;								// max PWM resolution in bit

				// redirect esp log to silent log function
			esp_log_set_vprintf(&ESP32_PWM_Servo::espSilentLog);
																												DPRINTLN("    -> Computing PWM maximum bit resolution for frequency.");
 			while ((resBit > 0) && (_logHasOccure != 0)) {
					// reset silent log flag tracker
				_logHasOccure = false;

					// test timer with current resBit
				_timers_config[_pwmTimer]->duty_resolution = (ledc_timer_bit_t)resBit;	// write resolution in timer config
				ledc_timer_config(_timers_config[_pwmTimer]);							// try to start timer with current config. Return write to char_in_esp_error_log.
  
				resBit--;
			}
																												DPRINT("    -> Resolution of "); DPRINT(_timers_config[_pwmTimer]->duty_resolution); DPRINTLN(" bits found");
				// reset silent log flag and restore esp log output tu UART0
			_logHasOccure = NOT_SET;
			esp_log_set_vprintf(&vprintf);

			if (resBit <= 0) {																					DPRINTLN("    -> Unexpected error durring PWM timer config. Abording configuration.");
					// free up memory and exit
				delete _timers_config[_pwmTimer];
				
				return EXIT_FAILURE;
			}
		}
	}
	else {																										DPRINT("  - PWM timer specified by user. Using timer ");DPRINT(_pwmTimer); DPRINTLN(" for PWM signal");
		// pwm timer set prior attach(). Statement for debug output.
	}
	
	
		// create PWM channel config structure
	_ledc_channel_config = new ledc_channel_config_t;
																												DPRINTLN("  - PWM channel selection :");
		// auto select PWM channel if no pwmChannel is provided prior setup()
	if (_pwmChannel == NOT_SET) {																				DPRINTLN("    -> No PWM channel specified. Auto-selecting a free one.");
		for (uint8_t channel = 0; channel < LEDC_CHANNEL_MAX; channel++)
		{		// use first free channel ...
			if (_pwm_channel_used[channel] == false) {															DPRINT("      -> Channel "); DPRINT(channel); DPRINTLN(" free. Using it for PWM signal.");
				_pwm_channel_used[channel] = true;
				_pwmChannel = channel;
				
				break;
			}
				// ... or exit with error if no one are available
			if (channel == (LEDC_CHANNEL_MAX - 1)) {															DPRINTLN("      -> No suitable free channel available. Abording configuration.");
					// free up memory and exit
				delete _ledc_channel_config;
				
				if (_clients_for_timer[_pwmTimer] == 0) {
					delete _timers_config[_pwmTimer];
				}
				
				return EXIT_FAILURE;
			}
		}
	}
	
	else {																										DPRINT("    -> PWM channel specified by user. Using channel ");DPRINT(_pwmChannel); DPRINTLN(" for this PWM signal");
		// pwm channel set prior attach(). Statement for debug output.
	}
	
		// seeding PWM channel config parameters 
	_ledc_channel_config->channel				= (ledc_channel_t)_pwmChannel;
	_ledc_channel_config->duty					= 0;
	_ledc_channel_config->hpoint				= 0;
	_ledc_channel_config->gpio_num				= pwmPin;
	_ledc_channel_config->speed_mode			= LEDC_LOW_SPEED_MODE;
	_ledc_channel_config->timer_sel				= (ledc_timer_t)_pwmTimer;
	_ledc_channel_config->intr_type				= LEDC_INTR_DISABLE;
	_ledc_channel_config->flags.output_invert	= 0;

		// start PWM signal on pwmPin.
	ledc_channel_config(_ledc_channel_config);																	DPRINT("  - Starting PWM signal on pin "); DPRINTLN(pwmPin);
																												DPRINTLN("PWM generator configuration success."); DPRINTLN();
		// register this instance for PWM timer used.
	_clients_for_timer[_pwmTimer]++;
	
		// set ledc fade service on for ledc_set_duty_and_update/ledc_set_fade_time_and_start functions
	ledc_fade_func_install(0);
		
		// set _pwmMaxDuty from duty bit resolution
	_pwmMaxDuty = getMaxDutyVal();																				DPRINT("_pwmMaxDuty set to "); DPRINTLN(_pwmMaxDuty);


	return EXIT_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	setLimits(minAngle, maxAngle) - set min/max angle limits
/		uint8_t minAngle: minimum angle limit
/		uint8_t maxAngle: maximum angle limit                                      */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::setLimits(int16_t minAngle, int16_t maxAngle)
{																												DPRINTLN("  Setting up minAngle/maxAngle limits");
	if ((minAngle >= _minHwAngle) && (maxAngle <= _maxHwAngle) && (minAngle < maxAngle)) {					DPRINTLN("    -> Min/max angle limit set to default value."); DPRINT(minAngle); DPRINT(" - "); DPRINT(maxAngle); DPRINTLN("°");
		_minAngle = minAngle;
		_maxAngle = maxAngle;																		

		return EXIT_SUCCESS;
	}
																												DPRINTLN("    -> Bad min/max angle limits provided.");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	writeMicroseconds(us) - compute and write PWM duty cycle from tick duration
/		float us: PWM tick duration                                                */
/////////////////////////////////////////////////////////////////////////////////////
uint32_t ESP32_PWM_Servo::writeMicroseconds(uint16_t us)
{																												DPRINTLN("Compute and write PWM frequency from tick duration.");
		// compute PWM duty cycle from us
	return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, (ledc_channel_t)_pwmChannel, usToDuty(us), 0);
}

/////////////////////////////////////////////////////////////////////////////////////
/*	goToAngle(angle) - set servo angle
/		float angle: angle in degres                                               */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::goToAngle(float angle)
{																												DPRINTLN("Setting servo angle.");
		// check if provide angle is valid
	if (angleIsValid(angle) == EXIT_SUCCESS) {
			// compute PWM tick duration from angle
		writeMicroseconds(angleToUs(angle));
		_curPos = angle;

		return EXIT_SUCCESS;
	}
																												DPRINTLN("  Speed setting aborded.");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	goToAtSpeed(angle, speed) - set servo angle to go at speed
/		float angle: angle to reach in degres
/		 uint16_t speed : speed in millis per degre                                */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::goToAtSpeed(float angle, uint16_t speed)
{																												DPRINTLN("Setting servo angle at speed.");
		// check if provide angle is valid
	if ((angleIsValid(angle) == EXIT_SUCCESS) && (speed != 0)) {
			// update current speed whith last request
		_speedMsDeg = speed;

			// reset timeMark for a to long delay 
		if (millis() - _timeMark > MAX_TIMEMARK_DELAY) {
			_timeMark = millis();
 		}

			// compute new position to reach 
		if (_timeMark < millis() ) {																		DPRINTLN("  Computing new sevo position");
			if (_curPos <= angle) {
				_curPos += ((float)(millis() - _timeMark) / (float)_speedMsDeg);
			}
    
			else {
				_curPos -= ((float)(millis() - _timeMark) / (float)_speedMsDeg);
			}
		}
			// update servo position and time mark for next call
		goToAngle(_curPos); 																		DPRINT("  Servo position update to "); DPRINTLN(_curPos);
		_timeMark = millis();			

		return EXIT_SUCCESS;
	}
																												DPRINTLN("  Speed setting aborded. Out of range angle detected");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	getPwmTimer() - Return the PWM timer (0-3) use by PWM signal or -1 if not set  */
/////////////////////////////////////////////////////////////////////////////////////
int8_t ESP32_PWM_Servo::getPwmTimer()
{		// check if PWM is configured and return timer ...
	if (_ledc_channel_config != NULL) {

		return _ledc_channel_config->timer_sel;
	}
		// ... or return NOT_SET if not configured
	return NOT_SET;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	getPwmFreq() - Return the frequency of the PWM signal                          */
/////////////////////////////////////////////////////////////////////////////////////
uint32_t ESP32_PWM_Servo::getPwmFreq()
{		// check if PWM is configured and return frequency ...
	if (_ledc_channel_config != NULL) {
		return ledc_get_freq(LEDC_LOW_SPEED_MODE, (ledc_timer_t)_pwmTimer);
	}
		// ... or error code if not configured
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	getMaxDutyVal() - Return the maximum value of PWM duty cycle                   */
/////////////////////////////////////////////////////////////////////////////////////
uint32_t ESP32_PWM_Servo::getMaxDutyVal()
{		// check if PWM is configured and return frequency ...
	if (_ledc_channel_config != NULL) {
		uint8_t duty_resolution = _timers_config[_pwmTimer]->duty_resolution;
		
		return (pow(2,duty_resolution) - 1);
	}
		// ... or return error code if not configured
	return -1;
}


/////////////////////////////////////////////////////////////////////////////////////
/*	angleIsValid(angle) - check if an angle is in MIN_ANGLE-MAX_ANGLE range
		float angle :  angle in °                                                  */
/////////////////////////////////////////////////////////////////////////////////////
bool ESP32_PWM_Servo::angleIsValid(float angle)
{		// check if angle value is in MIN_ANGLE-MAX_ANGLE range
	if (angle == constrain(angle, _minAngle, _maxAngle)) {														
		return EXIT_SUCCESS;
	}
																												DPRINTLN("    Invalid angle provided (not in min/max angle range.");
	return EXIT_FAILURE;
}

/////////////////////////////////////////////////////////////////////////////////////
/*	angleToUs(speed) - convert an angle to PWM pulse duration
		float angle : angle to convert with min/max angle compensation             */
/////////////////////////////////////////////////////////////////////////////////////
uint16_t ESP32_PWM_Servo::angleToUs(float angle)
{		// map angle value into min/max PWM signal µs range
		DPRINT("    Convert angle "); DPRINT(angle); DPRINT(" to "); DPRINT((angle - _minHwAngle) * (_highUsTick - _lowUsTick) / (_maxHwAngle - _minHwAngle) + _lowUsTick); DPRINTLN(" us");
	return round((angle - _minHwAngle) * (_highUsTick - _lowUsTick) / (_maxHwAngle - _minHwAngle) + _lowUsTick);
}

/////////////////////////////////////////////////////////////////////////////////////
/*	usToDuty(us) - convert PWM pulse duration tu duty cycle value
		uint16_t us : PWM pulse duration to convert                                */
/////////////////////////////////////////////////////////////////////////////////////
uint32_t ESP32_PWM_Servo::usToDuty(uint16_t us)
{		// map PWM signal µs lenghts to duty cycle
		DPRINT("    Convert us "); DPRINT(us); DPRINT(" to duty "); DPRINT((uint64_t)us * _pwmMaxDuty * getPwmFreq() / 1000000); DPRINTLN("");
	return round((uint64_t)us * _pwmMaxDuty * getPwmFreq() / 1000000);
}

/////////////////////////////////////////////////////////////////////////////////////
/*	dutyToUs(duty) - convert PWM duty cycle value to PWM pulse duration
		uint32_t duty : PWM duty cycle to convert                                */
/////////////////////////////////////////////////////////////////////////////////////
uint16_t ESP32_PWM_Servo::dutyToUs(uint32_t duty)
{		// map duty cycle to PWM signal µs lenghts
    return ((uint64_t)duty * 1000000 / getPwmFreq() / _pwmMaxDuty);
}

/////////////////////////////////////////////////////////////////////////////////////
/*	espSilentLog(...) - read number of character write durring esp_log msg
										                                           */
/////////////////////////////////////////////////////////////////////////////////////
int ESP32_PWM_Servo::espSilentLog(const char* string, va_list args)
{		// store number of character into "string"
	_logHasOccure = vsnprintf(NULL, 0, string, args);

	return vprintf("coucou", args);
}


/* destructor:
	-> delete (free) *ledc_channel_config memory AND set _pwm_channel_used[channel] = false
	-> _clients_for_timer[_pwmTimer]--;
	-> delete (free) *_timers_config[_pwmTimer] SI _clients_for_timer[_pwmTimer] == 0
	-> _default_timer = NOT_SET SI _default_timer != NOT_SET ET _clients_for_timer[_default_timer] == 0
*/