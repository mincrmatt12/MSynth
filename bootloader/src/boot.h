#pragma once
// boot.h -- manages booting of app_hdr and boot params

#include "applist.h"

void boot_app(const app_hdr *app) __attribute__((noreturn));
