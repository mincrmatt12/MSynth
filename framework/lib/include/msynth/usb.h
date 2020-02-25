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

#define USB_OTG_FS_HOST       ((USB_OTG_HostTypeDef *)((uint32_t )USB_OTG_FS_PERIPH_BASE + USB_OTG_HOST_BASE))
#define USB_OTG_FS_HC(i)      ((USB_OTG_HostChannelTypeDef *)((uint32_t)USB_OTG_FS_PERIPH_BASE + USB_OTG_HOST_CHANNEL_BASE + (i)*USB_OTG_HOST_CHANNEL_SIZE))
#define USB_OTG_FS_PCGCCTL    *(__IO uint32_t *)((uint32_t)USB_OTG_FS_PERIPH_BASE + USB_OTG_PCGCCTL_BASE)
#define USB_OTG_FS_HPRT0      *(__IO uint32_t *)((uint32_t)USB_OTG_FS_PERIPH_BASE + USB_OTG_HOST_PORT_BASE)
#define USB_OTG_FS_DFIFO(i)   *(__IO uint32_t *)((uint32_t)USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE + (i) * USB_OTG_FIFO_SIZE)

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

	template<template<typename...> typename StateHolderImpl, typename ...SupportedDevices>
	struct Host {
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
		
		// Enable port power. This method requires that init() has been called.
		//
		// Once enabled, a peripheral may be detected at any time. It is wise (although not strictly required) to poll inserted() after
		// calling this function, so that devices which check if a host is smart don't get confused.
		void enable() {
			// Enable the HPRT interrupts
			USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_PRTIM;
			// Set FS
			USB_OTG_FS_HOST->HCFG = 1; // Set 48mhz FS (will adjust)

			// Turn on port power
			LL_GPIO_SetOutputPin(GPIOE, LL_GPIO_PIN_2); // dwrrrrrrrrrn
			USB_OTG_FS_HPRT0 |= USB_OTG_HPRT_PPWR;
		}
		// Kill port power. This hard disconnects the device, and init_periph will need to be called again.
		//
		// Note that it is not necessary to handle overcurrents manually, as this class takes care of this functionality internally.
		void disable() {
			// Check the state of the USB
			if (inserted()) {
				// Device was inserted, so let's disable the port
				USB_OTG_FS_HPRT0 &= ~(USB_OTG_HPRT_PENA);

				util::delay(5);

				device.reset();
			}

			// Kill port power
			USB_OTG_FS_HPRT0 &= ~(USB_OTG_HPRT_PPWR);
			LL_GPIO_ResetOutputPin(GPIOE, LL_GPIO_PIN_2);

			// TODO: Clean up after the old thing
		}

		// PERIPHERAL DETECTION
		bool inserted() {
			return USB_OTG_FS_HPRT0 & USB_OTG_HPRT_PCSTS;
		}

		// INITIALIZATION
		
		// Initialize a connected peripheral. To check which peripheral was connected (if init_status == Ok)
		// use the template specializations of inserted.
		init_status init_periph() {
			if (!inserted()) return init_status::NotInserted;
			// Start by resetting the port
			USB_OTG_FS_HPRT0 |= USB_OTG_HPRT_PRST;
			// Wait at least twice the value in the datasheet, you never know what bullcrap is in them these days
			util::delay(20);
			USB_OTG_FS_HPRT0 &= ~(USB_OTG_HPRT_PRST);
			// Wait for a PENCHNG interrupt
			while (!got_penchng) {;}
			got_penchng = 0;

			// Check enumerated speed
			if ((USB_OTG_FS_HPRT0 & USB_OTG_HPRT_PSPD) == USB_OTG_HPRT_PSPD_0) {
				// Full speed
			}
			else {
				// Low speed
				// TODO: set the speed; i don't think i own anything though that'll trigger this
			}
		}

		// Initialize the host AHB-peripheral. This function will only work correctly if interrupts are set up correctly.
		// At the very least, ensure the USB global interrupt goes to the correct irq function. If overcurrent is not mapped correctly
		// the system may hang if the connected peripheral triggers an overcurrent situation.
		void init() {
			LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_OTGFS);
			LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
			LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOE);

			util::delay(2);
			// Start resetting USB
			USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;

			// Setup the power switch GPIOs
			{
				LL_GPIO_InitTypeDef init;
				init.Mode = LL_GPIO_MODE_OUTPUT;
				init.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
				init.Pull = LL_GPIO_PULL_NO;
				init.Pin = LL_GPIO_PIN_2;
				init.Speed = LL_GPIO_SPEED_FREQ_LOW;

				LL_GPIO_Init(GPIOE, &init); // USB_EN

				init.Pin = LL_GPIO_PIN_3;
				init.Mode = LL_GPIO_MODE_INPUT;
				init.Pull = LL_GPIO_PULL_UP;

				LL_GPIO_Init(GPIOE, &init); // USB_FAULT

				init.Pin = LL_GPIO_PIN_14 | LL_GPIO_PIN_15;
				init.Mode = LL_GPIO_MODE_ALTERNATE;
				init.Alternate = LL_GPIO_AF_10; // this isn't in the datasheet but it's in the examples
				init.Pull = LL_GPIO_PULL_NO;
				init.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;

				LL_GPIO_Init(GPIOB, &init);
			}

			while (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_CSRST) {;}
			
			// Initialize USB
			USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FHMOD;
			USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
			USB_OTG_FS->GUSBCFG |= (0x6 << USB_OTG_GUSBCFG_TRDT_Pos);
			USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_HNPCAP;

			// Clear all interrupts
			USB_OTG_FS->GINTSTS = 0xFFFFFFFF; // technically not compliant but screw it

			// Power UP PHY
			USB_OTG_FS_PCGCCTL = 0;
			USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_PWRDWN;

			// Disable sensing
			USB_OTG_FS->GCCFG &= ~(USB_OTG_GCCFG_VBUSASEN | USB_OTG_GCCFG_VBUSBSEN);
			USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
			// Disable SOF
			USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_SOFOUTEN;
		
			// Flush all FIFOs
			USB_OTG_FS->GRSTCTL = (15 << USB_OTG_GRSTCTL_TXFNUM_Pos) | USB_OTG_GRSTCTL_TXFFLSH;
			while (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) {;}
			USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_RXFFLSH;
			while (USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) {;}

			// Enable mode mismatch/OTG interrupts
			USB_OTG_FS->GINTMSK = 0;
			USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_OTGINT | USB_OTG_GINTMSK_MMISM;

			// Enable interrupts
			USB_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

			// We are now inited. To continue host initilaization,
			// the user must call enable();
		}

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
		void overcurrent_irq();
		void usb_global_irq() {
			// Do nothing RN
		}
	private:
		// Various state flags:
		// These are kept in an app-controlled variable because the peripheral docs are unclear.
		// Some of these flags in the USB are cleared as part of the interrupt scheme.
		volatile uint16_t got_penchng : 1;

		StateHolder device;
	};

	struct MidiDevice {
	};

	struct HID {
	};
}
