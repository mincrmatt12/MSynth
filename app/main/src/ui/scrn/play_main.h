#pragma once

#include "../base.h"
#include "../../evt/dispatch.h"
#include "../../evt/events.h"

namespace ms::ui::scrn {
	struct MainPlayScreen : UI, evt::EventHandler<evt::TouchEvent> {
		// The main playback screen these parts:
		// 	A display for the currently set patch.
		// 	An iconographic display of the parts of the patch
		//  A button with which to open the menu
		//  A view of the active notes.

		// TODO: some kind of "clickable label; or perhaps a "backgroundless button"
	};
}
