#pragma once
// applist - finds and parses app entries in flash
#include <stdint.h>

typedef struct {
	char magic[4];
	char name [32];

	uint32_t vectab;
	uint32_t entry;
	uint32_t appid;
	uint16_t flags;
	uint32_t dbgdat;
	uint32_t topofstack;
	uint32_t contents_size;
} app_hdr;

#define APPHEAD_FLAG_ISPRIMARY   (1 << 0)
#define APPHEAD_FLAG_ISTEST      (1 << 1)
#define APPHEAD_FLAG_USEEEPROM   (1 << 4)
#define APPHEAD_FLAG_EEPROM_SLOT (3 << 2)
#define APPHEAD_FLAG_USEFS       (1 << 9)
#define APPHEAD_FLAG_FS_SLOT     (31 << 5)
#define APPHEAD_FLAG_ISLOWSCREEN (1 << 10)
#define APPHEAD_FLAG_ISINSTALLED (1 << 11)
#define APPHEAD_FLAG_HASAPPCMD   (1 << 12)

extern uint8_t appheader_count;
extern const app_hdr *app_headers_found[32];

void detect_app_and_bucket_headers();
