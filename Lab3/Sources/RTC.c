/*! @file
 *
 *  @brief Routines for controlling the Real Time Clock (RTC) on the TWR-K70F120M.
 *
 *  This contains the functions for operating the real time clock (RTC).
 *
 *  @author Corey Stidston & Menka Mehta
 *  @date 2017-04-26
 */
/*!
 **  @addtogroup RTC_module RTC module documentation
 **  @{
 */
#include "RTC.h"
#include <types.h>
#include "MK70F12.h"
#include "LEDs.h"
#include <stdint.h>

static void *RTCArguments; //pointer to userArguments funtion
static void (*RTCCallback)(void *); //pointer to userCallback function

/*! @brief Initializes the RTC before first use.
 *
 *  Sets up the control register for the RTC and locks it.
 *  Enables the RTC and sets an interrupt every second.
 *  @param userFunction is a pointer to a user callback function.
 *  @param userArguments is a pointer to the user arguments to use with the user callback function.
 *  @return bool - TRUE if the RTC was successfully initialized.
 */
bool RTC_Init(void (*userFunction)(void*), void* userArguments)
{
	RTCArguments = userArguments; //Globally accessible (userArguments)
	RTCCallback = userFunction; //Globally accessible (userFunction)

	SIM_SCGC6 |= SIM_SCGC6_RTC_MASK;  	// Enable clock gate RTC

	// First Try and assert a software reset on the RTC
	RTC_CR = RTC_CR_SWR_MASK;

	// Check if there was a prior reset
//	if (RTC_CR == RTC_CR_SWR_MASK)
//	{
		RTC_CR &= ~RTC_CR_SWR_MASK;

		RTC_TSR = 0; // Time invalid flag

		//Enable 18pF load - pg 1394/2275 k70 manual
		RTC_CR |= RTC_CR_SC2P_MASK;
		RTC_CR |= RTC_CR_SC16P_MASK;

		//lab3 note hint 7 -RTC | pg 1394/2275 k70 manual
		RTC_CR |= RTC_CR_OSCE_MASK;	//Enables the 32.768kHz Oscillator

		// You need to wait at least 500ms for the RTC to startup
		for (int i = 0; i < 0x600000; i++);

		// lock the control reg
		RTC_LR &= ~RTC_LR_CRL_MASK;				// Locks the control register
//	}


	//RTC-IER Interrupt Enable Register | pg 1399/2275 K70 Manual
	//TSIE -Time Seconds Interrupt Enable
	RTC_IER |= RTC_IER_TSIE_MASK;			// Enables seconds enable interrupt (on by default)
	//TAIE - Time Alarm Interrupt Enable
	RTC_IER &= ~RTC_IER_TAIE_MASK;			// Disables Time Alarm Interrupt
	//TOIE - Time Overflow Interrupt Enable
	RTC_IER &= ~RTC_IER_TOIE_MASK;			// Disables Overflow Interrupt
	//TIIE - Time Invalid Interrupt Enable
	RTC_IER &= ~RTC_IER_TIIE_MASK;			// Disables Time Invalid Interrupt



	RTC_SR |= RTC_SR_TCE_MASK;				// Initialises the timer control

	// Initialise NVICs for RTC | pg 97/2275 k70 manual
	//IRQ 67(seconds interrupt) mod 32
	NVICICPR2 =  (1 << 3);	 //clears pending 'seconds' interrupts on RTC using IRQ value
	NVICISER2 =  (1 << 3); //sets/enables 'seconds' interrupts on RTC

	return true;
}

/*! @brief Sets the value of the real time clock.
 *
 *  @param hours The desired value of the real time clock hours (0-23).
 *  @param minutes The desired value of the real time clock minutes (0-59).
 *  @param seconds The desired value of the real time clock seconds (0-59).
 *  @note Assumes that the RTC module has been initialized and all input parameters are in range.
 */
void RTC_Set(const uint8_t hours, const uint8_t minutes, const uint8_t seconds)
{
	uint32_t timeInSeconds = (hours*3600)+(minutes*60)+(seconds); //calculates the value of the real time clock.
	//TCE - time counter enable | pg 1395/2275 K70 manual
	RTC_SR &= ~RTC_SR_TCE_MASK; //disables the time counter
	RTC_TSR = timeInSeconds; //loads in the time seconds registers

	//Not sure of this is needed
	// RTC_SR &= ~RTC_SR_TOF_MASK;				//!Time overflow flag is set when the time counter is enabled
	// RTC_SR &= ~RTC_SR_TIF_MASK;				//!The time invalid flag is set on software reset.

	//TCE - time counter enable | pg 1395/2275 K70 manual
	RTC_SR |= RTC_SR_TCE_MASK; //Enables the time counter
}

/*! @brief Gets the value of the real time clock.
 *
 *  @param hours The address of a variable to store the real time clock hours.
 *  @param minutes The address of a variable to store the real time clock minutes.
 *  @param seconds The address of a variable to store the real time clock seconds.
 *  @note Assumes that the RTC module has been initialized.
 */
void RTC_Get(uint8_t* const hours, uint8_t* const minutes, uint8_t* const seconds)
{
	uint32_t currentSeconds = RTC_TSR;
	// Should be reading this twice, checking if the reads match see p 1400 roughly

	//Link : http://stackoverflow.com/questions/24850738/how-to-convert-total-seconds-value-to-string-in-hours-minutes-seconds-format
	*hours = (currentSeconds/3600); //updates the current time value of hours
	*minutes = (currentSeconds % 3600) / 60; //updates the current time value of minutes
	*seconds = (currentSeconds % 3600) % 60; //updates the current time value of seconds
}

/*! @brief Interrupt service routine for the RTC.
 *
 *  The RTC has incremented one second.
 *  The user callback function will be called.
 *  @note Assumes the RTC has been initialized.
 */
void __attribute__ ((interrupt)) RTC_ISR(void)
{
	if (RTCCallback)
	{
		(*RTCCallback)(RTCArguments);
	}
}



















/*!
 ** @}
 */
