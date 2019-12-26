#include "applist.h"

uint8_t appheader_count = 0;
uint8_t bucketheader_count = 0;

const app_hdr *app_headers_found[32];
const bucket_hdr *bucket_headers_found[12];

void load_applist_at(uint32_t ptr) {
	bucket_hdr *bucket = (bucket_hdr *)ptr;
	if (bucket->magic[0] != 'M' || bucket->magic[1] != 'S' || bucket->magic[2] != 'B' || bucket->magic[3] != 'C') return;
	bucket_headers_found[bucketheader_count++] = bucket;
	for (int i = 0; i < bucket->header_count; ++i) {
		app_hdr *app = (app_hdr *)(ptr + sizeof(bucket_hdr) + i*sizeof(app_hdr));
		if (app->magic[0] == 'M' && app->magic[1] == 'A' && app->magic[2] == 'P' && app->magic[3] == 'P') app_headers_found[appheader_count++] = app;
	}
}

void detect_app_and_bucket_headers() {
	load_applist_at(0x08004000);
	load_applist_at(0x08008000);
	load_applist_at(0x0800C000);
	load_applist_at(0x08010000);
	load_applist_at(0x08020000);
	load_applist_at(0x08040000);
	load_applist_at(0x08060000);
	load_applist_at(0x08080000);
	load_applist_at(0x080A0000);
	load_applist_at(0x080E0000);
}
