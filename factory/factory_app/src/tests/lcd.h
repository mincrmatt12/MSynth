#pragma once
// lcd and backlight test
//
// Shows a bunch of test patterns + adjusts the backlight

#include "base.h"
#include <msynth/periphcfg.h>

struct LcdTest {
	void start();
	TestState loop(); 

private:
	int screenno;
};
