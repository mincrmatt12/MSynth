#include "stm32f4xx.h"
#include "sys.h"
#include "applist.h"
#include "boot.h"
#include "ui.h"

int main() {
	// Start the bootloader
	sys_init();
	// lcd_init();
	ui_init();
	// eeprom_init();
	
	puts("MSignBoot v1");

	uint32_t held_code = ui_get_button();

	// Find all the apps
	detect_app_and_bucket_headers();

	if (appheader_count == 0) {
		puts("No apps are installed");
	}
	else if (!held_code) {
		// No special code, boot first priamry app
		
		for (int i = 0; i < appheader_count; ++i) {
			printf("got app %s; id %08lx\n", app_headers_found[i]->name, app_headers_found[i]->appid);
			if (app_headers_found[i]->flags & APPHEAD_FLAG_ISPRIMARY) {
				puts("booting primary");
				boot_app(app_headers_found[i]);
			}
		}

		puts("no primary app");
		// We haven't booted an app, try the TEST app
		//
		for (int i = 0; i < appheader_count; ++i) {
			if (app_headers_found[i]->flags & APPHEAD_FLAG_ISTEST) {
				puts("booting test");
				boot_app(app_headers_found[i]);
			}
		}
	}
	else if (held_code & UI_BUTTON_F4) {
		puts("override boot mode; test");
		// Try and boot the test app.
		for (int i = 0; i < appheader_count; ++i) {
			if (app_headers_found[i]->flags & APPHEAD_FLAG_ISTEST) {
				puts("booting test");
				boot_app(app_headers_found[i]);
			}
		}
	}

	// if the bootloader tries to exit, panic
	puts("exitpanic");
	while (1) {
		// panic
	}
}
