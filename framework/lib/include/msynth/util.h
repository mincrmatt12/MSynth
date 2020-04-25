#pragma once
// util.h - various common primitives throughout the API
#include <cstddef>
#include <stdint.h>
#include <type_traits>
#include <utility>

#define MSYNTH_APPCODE_MISSINGRSRC 0xc001
#define MSYNTH_APPCODE_MISSINGHW 0xc002
#define MSYNTH_APPCODE_ERROR 0xc000

namespace util {
	template<int N, int D>
	struct LowPassFilter {
		uint16_t operator()(int x, bool reset = false)
		{
			if (reset) {
				s = x;
				return x;
			}

			// Use integer arithmetic for weighted average
			s = (N * s + (D - N) * x + D / 2) / D;
			return s;
		}

	private:
		int s = 0;
	};

	void delay(uint32_t ms);

	template<typename Return, typename ...Args>
	struct FuncHolder {
		typedef Return (*PtrType)(void * argument, Args...);

		// Data API

		PtrType target=nullptr;
		void * argument=nullptr;

		// Functional API

		// Create from member function
		template<typename T>
		inline static FuncHolder<Return, Args...> create(T& instance, Return (T::* mptr)(Args...)) {
			return {reinterpret_cast<PtrType>(mptr), (void*)(&instance)};
		}

		// Create from lambda/function object.
		// Note that this does not store the object itself, so it should be kept around until called.
		template<typename T>
		inline static std::enable_if_t<std::is_invocable_r_v<Return, T, Args...> && std::is_object_v<std::decay_t<T>>, FuncHolder<Return, Args...>> create(T& instance) {
			return create(instance, T::operator());
		}
		
		// Create from a normal function pointer. Note that you must take a void * as your first parameter.
		// If you want to customize this pointer, change the public argument member.
		inline static FuncHolder<Return, Args...> create(PtrType target) {
			return {target};
		}

		// Execute callback
		inline void operator()(Args ...args) const {
			if (target == nullptr) return;
			target(argument, std::forward<Args>(args)...);
		}
	};
}

#ifdef __cplusplus
#define ISR(Name) extern "C" void __attribute__((used)) Name ## _IRQHandler()
#else
#define ISR(Name) void __attribute__((used)) Name ## _IRQHandler() __attribute__((used))
#endif
