/*
 * WaterAlarm.c
 *
 * Created: 21.07.2016 13:28:40
 * Author : Sergey Shelepin
 *          sergey.shelepin@gmail.com
 */ 
#define LEVELS_NUM 5
#define TRUE 1
#define FALSE 0

#define WDT8S    1
#define WDT0125s 0

#define MODE_BUZ   1
#define MODE_SLEEP 0

#define watchdog_reset() __asm__ __volatile__ ("wdr")

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

uint16_t voltage; 

const uint8_t buz_beep_dur[LEVELS_NUM]  = {10,  8,  6,  5,  4};
const uint8_t buz_cycle_dur[LEVELS_NUM] = {21,  16,  13,  9,  7};
const uint8_t buz_ovr_len[LEVELS_NUM]   = {21, 32, 39, 36, 35};
const uint8_t sleep_dur[LEVELS_NUM]     = {75, 50, 33, 22, 15};
const uint8_t frequency[LEVELS_NUM]     = {18, 14, 10, 8, 5};

uint8_t cur_level;
uint8_t mode;

volatile uint8_t buz_counter;
volatile uint8_t wdt_counter;


uint8_t GetLevel() {
	
	uint8_t wlsen = (PINB >> 1); // excluding PB0
	
	wlsen &= 0b00001111; // clear high bits
	
	if(!(wlsen & _BV(3))) {
		return 4;
	}
	if(!(wlsen & _BV(2))) {
		return 3;
	}
	if(!(wlsen & _BV(1))) {
		return 2;
	}
	if(!(wlsen  & _BV(0))) {
		return 1;
	}
	return 0;
}

void WDTSetup(uint8_t is8s) {
	//WDT setup
	cli();
	watchdog_reset();
	MCUSR = 0;
	if (is8s) {
		WDTCR = _BV(WDE) | _BV(WDCE);
		WDTCR = _BV(WDTIE) | _BV(WDP3) | _BV(WDP0); // interrupt mode, prescaler = 1024K for 8 sec time-out
	} else{
		WDTCR = _BV(WDE) | _BV(WDCE);
		WDTCR = _BV(WDTIE) | _BV(WDP1) | _BV(WDP0); // interrupt mode, prescaler = 16K for 0.125 sec time-out
	}
	sei();
}

ISR(WDT_vect) {
	++wdt_counter;
	++buz_counter;
}

void timer_on(uint8_t ocr0a_value) {
	TCNT0  = 0;//initialize counter value to 0
	OCR0A = ocr0a_value; //set compare match register value
	TCCR0B = _BV(CS01)| _BV(CS00); //set prescaler = 64
}

void inline timer_ofF() {
	TCCR0B = 0; //set entire register to 0
}

int main(void) {

	//PINs setup
	//set PB0 as output
	DDRB |= _BV(PB0);
	//set PB1-PB4 as input
	DDRB &= ~(_BV(PB1) | _BV(PB2) | _BV(PB3) |_BV(PB4)) ;
	
	//Timer setup
	TCCR0A =  _BV(WGM01); // turn on CTC mode
// 	TCCR0B = _BV(CS01)| _BV(CS00); //set prescaler = 64
// 	TCNT0  = 0;//initialize counter value to 0
// 	OCR0A = 6; //set compare match register value
	
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	
	mode = MODE_BUZ;
	WDTSetup(WDT0125s);

	cur_level = GetLevel();

    while (1) {
		
		if (mode == MODE_BUZ) {
// 			cur_level = GetLevel();


// buz as per level
			timer_on(frequency[cur_level]);
			wdt_counter = 0;
			buz_counter = 0;
		 
			while (buz_counter < buz_ovr_len[cur_level]) {
				if (wdt_counter >= buz_beep_dur[cur_level]) {
					TCCR0A &= ~_BV(COM0A0);				
					if (wdt_counter == buz_cycle_dur[cur_level] ) {
						wdt_counter = 0;
					}
				} else	{
					TCCR0A |= _BV(COM0A0);
				}
			} 
			TCCR0A &= ~_BV(COM0A0);		
			timer_ofF();
// end of buz as per level
			
			mode = MODE_SLEEP;
			WDTSetup(WDT8S);

//			++ cur_level; // for tests
			
		} else { // mode == MODE_SLEEP
			
			wdt_counter = 0;
			do {
				sleep_enable();
				sleep_cpu();
				sleep_disable();
				cur_level = GetLevel();
			} while(wdt_counter < sleep_dur[cur_level]);
			
			mode = MODE_BUZ;
			wdt_counter = 0;
			WDTSetup(WDT0125s);
		}
		
	}
}
