#pragma once
// fs.h -- routines to interact with the flash filesystem.

#include <stdint.h>
#include <iterator>

namespace fs {
	// FileFlags
	inline const uint16_t TopLevelMarkedForDeletion = (1 << 4);
	inline const uint16_t TopLevelDeleted = (1 << 5);
	inline const uint16_t TopLevelSystemGlobal = (1 << 0);
	inline const uint16_t TopLevelPrimary = (1 << 1);
	inline const uint16_t TopLevelPrivate = (1 << 2);

	inline const uint16_t FileDeleted = (1 << 0);
	inline const uint16_t FileMarkedForDeletion = (1 << 1);

	// DATATYPES
	struct File {
		char magic[4];
		char name[16];
		uint32_t used_by;
		uint16_t length;
		uint16_t flags;
		File * list_next;
		File * revision_next;

		bool ok() const {
			return magic[0] == 'f' && magic[1] == 'L' && magic[2] == 'e' && magic[3] == 'T';
		}

		// File provides an operator-> to allow for easier use of the revision_next
		File * operator->() {
			if (revision_next == (File *)0xffff'ffff)
				return this;
			return (*revision_next).operator->();
		}

		const File * operator->() const {
			if (revision_next == (File *)0xffff'ffff)
				return this;
			return (*revision_next).operator->();
		}
	};

	// FILE ITERATOR API
	template<typename T>
	struct FileIterator {
		// ITERATOR TYPEDEFS
		using iterator_category = std::forward_iterator_tag;
		using value_type = T;
		using distance_type = void;
		using reference_type = T&;
		using pointer_type = T*;

		pointer_type ptr;

		FileIterator(T& file) {
			ptr = file.operator->();
		}

		// Defaults to end
		FileIterator() {
			ptr = (pointer_type)0xffff'ffff;
		}

		pointer_type operator->() {
			return ptr;
		}

		reference_type operator*() {
			return *ptr;
		}

		FileIterator<T>& operator++() {
			ptr = (*ptr)->list_next;
			return *this;
		}

		FileIterator<T> operator++(int) {
			// Copy
			auto copy = *this;
			++this;
			return copy;
		}

		operator FileIterator<const T>() {
			return FileIterator<const T>(*ptr);
		}

		bool operator==(const FileIterator<T>& other) {
			return other.ptr == ptr;
		}

		bool operator!=(const FileIterator<T>& other) {
			return other.ptr != ptr;
		}
	};

	struct TopLevel {
		char magic[4];
		char name[12];

		File * list_top;
		uint16_t flags;
		uint16_t reserved;

		bool ok() const {
			return magic[0] == 'f' && magic[1] == 'S' && magic[2] == 't' && magic[3] == 'L';
		}

		// ITERATOR TYPEDEFS
		using iterator = FileIterator<File>;
		using const_iterator = FileIterator<const File>;

		iterator begin() {return iterator(*list_top);}
		const_iterator begin() const {return const_iterator(*list_top);}
		const_iterator cbegin() const {return const_iterator(*list_top);}

		iterator end() {return iterator();}
		const_iterator end() const {return const_iterator();}
		const_iterator cend() const {return const_iterator();}
	};
	
	struct Header {
		char magic[4];
		uint8_t version;
		uint8_t sector_count;
		uint16_t sector_use_mask;
		TopLevel top_level[16];

		bool ok() const {
			return magic[0] == 'M' && magic[1] == 'S' && magic[2] == 'F' && magic[3] == 'S';
		}
	};

	// BASIC ACCESS API
	bool is_present(); // Tries to find the Filesystem
	
	const void * open(const char *path); // Tries to open file (returning pointer to contents or 0)
	const void * open(const File& file); // Handles revision next.

	// Checks if path exists:
	// 	If path ends with /:
	// 	 Check if that top-level folder exists (other folders are just / in name)
	// 	Otherwise:
	// 	 Check if that file exists
	bool     exists(const char *path); 

	// Find a top-level folder.
	const TopLevel* find(const char *path);

	// Get a reference to a file
	const File* get(const char *path);
}
