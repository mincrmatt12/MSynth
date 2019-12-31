#include "fs.h"
#include <string.h>
#include <stdio.h>

struct PathInfo {
	char toplevel_name[13] = {0};
	bool is_file = false;
	char filename[17] = {0};
	bool is_valid = false;

	PathInfo(const char* from) {
		int length = strlen(from);
		if (length > 12 + 16 + 1) return;

		int i;
		for (i = 0; i < length; ++i) {
			if (from[i] != '/') continue;
			else {
				this->is_valid = true;
				strncpy(this->toplevel_name, from, i);
				if (i == length-1) return;
				break;
			}
		}

		if (i == length) {
			strncpy(this->toplevel_name, from, length);
			this->is_valid = true;
			return;
		}

		strncpy(this->filename, from + i + 1, length-i-1);
		this->is_file = true;
	}
};

const fs::Header& fs_header() {
	return ((fs::Header *)0x080E'0000)->ok() ?
		*((fs::Header *)0x080E'0000) : *((fs::Header *)0x080E'0000);
}

bool fs::is_present() {
	// The filesystem is ALWAYS at the end of flash, and is only ever a maximum of two sectors large. Search for it
	// there
	
	return ((fs::Header *)0x080E'0000)->ok() || ((fs::Header *)0x080A'0000)->ok();
}

const fs::TopLevel * fs::find(const char * path) {
	// First, make sure the fs is present
	if (!fs::is_present()) return nullptr;
	// split the path up
	PathInfo pi(path);
	if (!pi.is_valid || pi.is_file) return nullptr;

	uint32_t mask = 1;
	const auto& header = fs_header();
	for (const auto &toplevel : header.top_level) {
		if (toplevel.flags & TopLevelDeleted) continue;
		if (!(mask & header.sector_use_mask) || !toplevel.ok()) {
			mask <<= 1;
		}
		else if (strcmp(toplevel.name, pi.toplevel_name) == 0) {
			// Make sure it's not deleted
			return &toplevel;
		}
		else mask <<= 1;
	}
	return nullptr;
}

const fs::File * fs::get(const char * path) {
	// split the path up
	PathInfo pi(path);
	if (!pi.is_valid || !pi.is_file) return nullptr;

	const auto * toplevel = fs::find(pi.toplevel_name);
	if (toplevel == nullptr) return nullptr;

	for (const auto& file : *toplevel) {
		if (file.flags & FileDeleted) continue;
		if (strcmp(file->name, pi.filename) == 0) {
			return &file;
		}
	}
	return nullptr;
}

bool fs::exists(const char *path) {
	// First, make sure the fs is present
	if (!fs::is_present()) return false;

	// Next, try and identify the parts of the path.
	PathInfo pi(path);
	if (!pi.is_valid) return false;

	if (pi.is_file) {
		return fs::get(path) != nullptr;
	}
	else {
		return fs::find(path) != nullptr;
	}
}

const void * fs::open(const char *path) {
	const auto * f = get(path);
	if (f == nullptr) return nullptr;
	return open(*f);
}

const void * fs::open(const fs::File& f) {
	if (!f->ok()) return nullptr;

	// file data is after the header
	return (const void *)(&f + 1); // Take advantage of pointer arithmetic (save a sizeof)
}
