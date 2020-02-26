#pragma once
// usb.h - usb core driver / class interface layers
//
// This is supposed to be a nice to use driver, both in simplicity and in speed.
// Specifically, it has to support high speed low latency transfers without too much work, because
// that's its only primary function in the MSynth.
//
// Applications should be able to change at compile time which device classes are supported.
// This driver does not support hubs.
//
// The various supported devices are allowed to have state. If there is only one device class / no heap
// the application can use a static allocation form (essentially a tagged union), or if there is a heap,
// can use a polymorphic approach.
//
// The general flow, from the application side, is as follows:
//
// Application declares some USB driver object.
// The USB global interrupts are targeted at this object.
// Driver object is told to initialize USB port.
// App checks driver object and sees that device has been inserted (TODO: should this be an interrupt?)
// App triggers device initialization, and gets back a status with what device it was.
//
// App can then directly access the device through a useful interface, and USB backend stuff is handled separately.
//
// The USB peripheral is simpler in terms of API complexity, primarily because it doesn't support DMA. This limits the amount
// of asynchronous fluff that is required to keep the driver performant, and instead it mostly relies on carefully timed polling and
// interrupts.

#include <type_traits>
#include <stdint.h>
#include "util.h"

#include <stm32f4xx.h>
#include <stm32f4xx_ll_gpio.h>
#include <stm32f4xx_ll_bus.h>
#include <stm32f4xx_ll_system.h>
#include <stm32f4xx_ll_exti.h>

#define USB_OTG_HS_HOST       ((USB_OTG_HostTypeDef *)((uint32_t )USB_OTG_HS_PERIPH_BASE + USB_OTG_HOST_BASE))
#define USB_OTG_HS_HC(i)      ((USB_OTG_HostChannelTypeDef *)((uint32_t)USB_OTG_HS_PERIPH_BASE + USB_OTG_HOST_CHANNEL_BASE + (i)*USB_OTG_HOST_CHANNEL_SIZE))
#define USB_OTG_HS_PCGCCTL    *(__IO uint32_t *)((uint32_t)USB_OTG_HS_PERIPH_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_HS_HPRT0      *(__IO uint32_t *)((uint32_t)USB_OTG_HS_PERIPH_BASE + USB_OTG_HOST_PORT_BASE)
#define USB_OTG_HS_DFIFO(i)   *(__IO uint32_t *)((uint32_t)USB_OTG_HS_PERIPH_BASE + USB_OTG_FIFO_BASE + (i) * USB_OTG_FIFO_SIZE)

namespace usb {
	// There are some helper utilities here, but the majority of USB stuff happens through the Host struct
	
	enum struct init_status {
		Ok,
		NotSupported,
		NotInserted
	};

	template<typename T, typename ...Ts>
	inline constexpr bool is_contained_v = std::disjunction_v<std::is_same<T, Ts>...>;

	template<std::size_t I0, typename T, typename ...Ts>
	struct pack_index_helper;

	template<std::size_t I0, typename T, typename T0, typename ...T1>
	struct pack_index_helper<I0, T, T0, T1...> {
		const static inline std::size_t value = std::is_same_v<T, T0> ? I0 : pack_index_helper<I0+1, T, T1...>::value;
	};

	template<std::size_t I0, typename T, typename T0>
	struct pack_index_helper<I0, T, T0> {
		const static inline std::size_t value = std::is_same_v<T, T0> ? I0 : -1;
	};

	template<typename T, typename ...Ts>
	inline constexpr bool pack_index = pack_index_helper<0, T, Ts...>::value;


	// STATE HOLDERS
	//
	// all must implement template<typename T> holds()
	//                    template<typename T> get()
	//                    template<typename T> assign()
	//                    reset();

	// StaticStateHolder - a tagged union, in effect. Good for when there's no heap, or when
	// there's only one device type. It also forgoes any destructing logic, so can shrink program size
	template<typename ...SupportedDevices>
	struct StaticStateHolder {
		template<typename T>
		inline static constexpr bool is_supported_device_v = is_contained_v<T, SupportedDevices...>;

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, bool> holds() const {
			return index == pack_index<T, SupportedDevices...>;
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, const T&> get() const {
			return *reinterpret_cast<const T*>(&storage);
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, T&> get() {
			return *reinterpret_cast<T*>(&storage);
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>> assign() {
			new (&storage) T();
			index = pack_index<T, SupportedDevices...>;
		}

		void reset() {
			index = npos;
		}

	private:
		const inline static std::size_t npos = -1;
		std::size_t index = npos;
		typename std::aligned_union<0, SupportedDevices...>::type storage;
	};

	// DynamicStateHolder - allocates its objects on the heap and calls the correct destructor. Because the types still don't inherit from
	// a common base, they don't have a virtual destructor, so we still need some cast logic.
	template<typename ...SupportedDevices>
	struct DynamicStateHolder {
		template<typename T>
		inline static constexpr bool is_supported_device_v = is_contained_v<T, SupportedDevices...>;

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, bool> holds() const {
			return index == pack_index<T, SupportedDevices...>;
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, const T&> get() const {
			return *reinterpret_cast<const T*>(held);
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>, T&> get() {
			return *reinterpret_cast<T*>(held);
		}

		template<typename T>
		inline std::enable_if_t<is_supported_device_v<T>> assign() {
			reset();
			held = reinterpret_cast<void *>(new T());
			index = pack_index<T, SupportedDevices...>;
		}

		void reset() {
			if (index != npos) {
				// This extreme expression will call the correct destructor using short circuit evaluation.
				//
				// Although we could probably avoid this with virtual destructors, there's no need for them with the static case
				// so I use this instead.
				((index == pack_index<SupportedDevices, SupportedDevices...> && (delete reinterpret_cast<SupportedDevices *>(held), true)) || ...);
			}
			index = npos;
			held = nullptr;
		}

	private:
		const inline static std::size_t npos = -1;
		std::size_t index = npos;
		void * held = nullptr;
	};

	// Type of pipe IDs
	typedef int16_t pipe_t;

	// Various pipe constants
	namespace pipe {
		// The requested configurtion is invalid
		inline const pipe_t InvalidConfig = -1;
		// There are too many active pipes right now
		inline const pipe_t Busy = -2;

		enum EndpointType : uint8_t {
			EndpointTypeControl = 0,
			EndpointTypeIso = 1,
			EndpointTypeBulk = 2,
			EndpointTypeInterrupt = 3
		};

		enum EndpointDirection : uint8_t {
			EndpointDirectionOut = 0,
			EndpointDirectionIn = 1
		};
	}

	// HostBase is a low-level type that implements most of the logic in Host. It is effectively the template-invariant
	// components of Host, such as low-level OTG_HS management and IRQ handling.
	//
	// An application is free to use this type if it desires, although automated enumeration and other nice things are not
	// handled by it. 
	//
	// In particular, HostBase is the interface that device classes use. This separates the low-level pipe allocation from the user.
	struct HostBase {
		// Non-copyable type
		HostBase(const HostBase& other) = delete;
		// Non-moveable type (global/references only)
		HostBase(HostBase &&other) = delete;
		// Default constructor
		HostBase() {};

		// POWER MANAGEMENT
		// (usb-level)

		// Enable port power. This method requires that init() has been called.
		//
		// Once enabled, a peripheral may be detected at any time. It is wise (although not strictly required) to poll inserted() after
		// calling this function, so that devices which check if a host is smart don't get confused.
		void enable();
		void disable();

		// PCDET
		inline bool inserted() const {
			return USB_OTG_HS_HPRT0 & USB_OTG_HPRT_PCSTS;
		}

		// INITIALIZATION

		// Setup the host after a PCDET interrupt.
		init_status init_host_after_connect();

		// Initialize the host AHB-peripheral. This function will only work correctly if interrupts are set up correctly.
		// At the very least, ensure the USB global interrupt goes to the correct irq function. If overcurrent is not mapped correctly
		// the system may hang if the connected peripheral triggers an overcurrent situation.
		void init();

		// INTERRUPTS
		void overcurrent_irq();
		void usb_global_irq(); 

		// PIPE MANAGEMENT
		
		pipe_t create_pipe(

	private:
		// Various state flags:
		// These are kept in an app-controlled variable because the peripheral docs are unclear.
		// Some of these flags in the USB are cleared as part of the interrupt scheme.
		volatile uint16_t got_penchng : 1;
	};

	template<template<typename...> typename StateHolderImpl, typename ...SupportedDevices>
	struct Host : private HostBase {
		using StateHolder = StateHolderImpl<SupportedDevices...>;

		// Non-copyable type
		Host(const Host& other) = delete;
		// Non-moveable type (global/references only)
		Host(Host &&other) = delete;
		// Default constructor
		Host() {};

		// Host doesn't actually have very many responsibilities; only those core to keeping things running.
		// It also handles channel allocation and packet management, as well as power management.
		
		// POWER MANAGEMENT
		//
		// Detection of inserted devices is elsewhere

		using HostBase::enable;

		// Kill port power. This hard disconnects the device, and init_periph will need to be called again.
		//
		// Note that it is not necessary to handle overcurrents manually, as this class takes care of this functionality internally.
		void disable() {
			HostBase::disable();
			device.reset();
		}

		// PERIPHERAL DETECTION
		using HostBase::inserted;

		// INITIALIZATION
		
		// Initialize a connected peripheral. To check which peripheral was connected (if init_status == Ok)
		// use the template specializations of inserted.
		init_status init_periph() {
			// Initialize the USB low-level stuff for enumeration
			if (auto result = HostBase::init_host_after_connect(); result != init_status::Ok) return result;

			// Begin enumeration
			return init_status::NotSupported;
		}

		using HostBase::init;

		// DEVICE STATE / API
		template<typename T>
		inline std::enable_if_t<is_contained_v<T, SupportedDevices...>, bool> inserted() const {
			return device.template holds<T>();
		}

		template<typename T>
		inline constexpr std::enable_if_t<!is_contained_v<T, SupportedDevices...>, bool> inserted() const {
			return false;
		}

		template<typename T>
		inline std::enable_if_t<is_contained_v<T, SupportedDevices...>, const T&> dev() const {
			return device.template get<T>();
		}

		template<typename T>
		inline std::enable_if_t<is_contained_v<T, SupportedDevices...>, T&> dev() {
			return device.template get<T>();
		}

		// INTERRUPTS
		using HostBase::overcurrent_irq;
		using HostBase::usb_global_irq;
	private:

		// The connected device's state.
		StateHolder device;
	};

	struct MidiDevice {
	};

	struct HID {
	};
}
