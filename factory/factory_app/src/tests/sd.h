#pragma once
// lcd and backlight test
//
// Shows a bunch of test patterns + adjusts the backlight

#include "base.h"
#include <msynth/sd.h>

struct SdTest {
	void start();
	TestState loop(); 

private:
	// SD Test flow 
	// 	Wait for user to confirm -- showing inserted state
	// 	Init card -- show errors
	// 	Present menu
	// 	 1 - erase card and do read/write test
	// 	 2 - just read card and do hexdump
	// 	 3 - exit and reset (de-init sd)
	
	enum {
		WaitingForStartEjected,
		WaitingForStartInserted,
		RunningSdInit,
		WaitingForActionSelection,
		ErasingCard,
		WritingCard,
		VerifyingCardReadData,
		ReadingDataFromCardForDump,
		ShowingDumpOnScreen,
		ResettingCardAndExiting,
		ShowingInitError,
		ShowingAccessError,
		UndefinedState
	} state = WaitingForStartInserted, last_state = UndefinedState;

	const void * uiFnt;
	const void * bigFnt;

	sd::init_status sd_init_error;
	sd::access_status sd_access_error;
};
