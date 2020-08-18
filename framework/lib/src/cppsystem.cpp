#include <cstdio>
#include <cstdlib> 
#include <new> 

void* operator new(size_t size) 
{ 
    return malloc(size); 
} 

void operator delete(void *p) noexcept 
{ 
    free(p); 
} 

void* operator new[](size_t size) 
{ 
    return operator new(size); // Same as regular new
} 

void operator delete[](void *p) noexcept 
{ 
    operator delete(p); // Same as regular delete
} 

void* operator new(size_t size, std::nothrow_t) noexcept 
{ 
    return operator new(size); // Same as regular new 
} 

void operator delete(void *p,  std::nothrow_t) noexcept 
{ 
    operator delete(p); // Same as regular delete
} 

void* operator new[](size_t size, std::nothrow_t) noexcept 
{ 
    return operator new(size); // Same as regular new
} 

void operator delete[](void *p,  std::nothrow_t) noexcept 
{ 
    operator delete(p); // Same as regular delete
}

void operator delete(void *p, size_t) {
	free(p);
}

namespace std {
	[[noreturn]] void __throw_length_error(const char *msg) {
		// TODO:
		
		puts("length error:");
		puts(msg);
		while(1) {;}
	}

	[[noreturn]] void __throw_bad_alloc() {
		puts("bad alloc");
		while (1) {;}
	}
}

extern "C" void __cxa_pure_virtual() {
	while(1) {;}
}
