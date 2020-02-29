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
		Ok, // Succesfully initialized and the device can be controlled through .dev()
		NotSupported, // The inserted device is either of a non-supported class, or is high-speed only.
		NotInserted // Nothing is currently inserted into the USB port.
	};

	enum struct transaction_status {
		Busy, // There is currently a transaction ongoing.
		InProgress, // The transfer has been started.
		Ack, // The transfer completed succesfully with an ACK state
		Nak, // The transfer completed with a NAK state (the data may not have been sent / received)
		Stall, // The transfer completed with a STALL state
		Nyet, // The transfer (somehow) completed with a NYET state (???)
		XferError, // The transfer failed either due to a bus error (SOF missing, bad CRC), babble or frame overrun or because of an internal problem with the driver
		DataToggleError, // An invalid state of the data toggle was detected (the driver should have handled this)
		Inactive, // There is no transfer ongoing on this pipe (only returned from get_xfer_State)
		NotAllocated, // This pipe is not allocated.
		ShuttingDown, // The pipe is being deallocated.
	};

	// Is the type T in the variadic template Ts
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

	// With indices starting at I0, at what position does T first occur in the variadic template pack Ts
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

	struct HostBase;

	// Various pipe constants
	namespace pipe {
		// There are too many active pipes right now
		inline const pipe_t Busy = -2; // -2 because -1 is intended as an "unassigned" default state.

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

		struct Callback {
			friend HostBase;
			typedef void (*PtrType)(void * argument, pipe_t source, transaction_status event);

			// Data API

			PtrType target=nullptr;
			void * argument=nullptr;

			// Functional API

			template<typename T>
			inline static Callback create(T& instance, void (T::* mptr)(pipe_t, transaction_status)) {
				return {reinterpret_cast<PtrType>(mptr), (void*)(&instance)};
			}
			
			inline static Callback create(PtrType target) {
				return {target};
			}

		private:
			// Callback
			inline void operator()(pipe_t idx, transaction_status sts) const {
				if (target == nullptr) return;
				target(argument, idx, sts);
			}
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

		// Create a new pipe (referred to as channels in the RM). A pipe is a unidirectional link to an endpoint which can be reconfigured at will.
		// The unitary operation in a pipe is a transaction. This differs slightly from the canonical definition, and the RM calls them transfers.
		// Each transfer is compromised of a series of same-typed packets.
		pipe_t allocate_pipe();
		// Set the device address, endpoint number, max packet size (larger transfers are put into separate packet transactions), endpoint direction (IN/OUT) and type (bulk,control,etc.), and 
		// data toggle. Data toggle is 1 for DATA1 and 0 for DATA0.
		void configure_pipe(pipe_t idx, uint8_t address, uint8_t endpoint_num, uint16_t max_pkt_size, pipe::EndpointDirection direction, pipe::EndpointType type, bool data_toggle);
		// Set the device address, endpoint number, max packet size (larger transfers are put into separate packet transactions), endpoint direction (IN/OUT) and type (bulk,control,etc.), but
		// keep the data toggle the same.
		inline void configure_pipe(pipe_t idx, uint8_t address, uint8_t endpoint_num, uint16_t max_pkt_size, pipe::EndpointDirection direction, pipe::EndpointType type) {
			configure_pipe(idx, address, endpoint_num, max_pkt_size, direction, type, data_toggles & (1 << idx));
		}
		// De-allocate pipe. The currently in progress transfer is aborted.
		void destroy_pipe(pipe_t idx);
		// Get the current data toggle for a pipe. This should be kept the same for a given endpoint (num + In/Out)
		bool get_pipe_data_toggle(pipe_t idx);

		// TRANSACTIONS
		
		// Begin a transfer, respecting the maximum packet size, over the indicated pipe. Upon completion, the status can be read using the check_xfer_state function.
		//
		// For IN endpoints, the length is the maximum size for receive. The total amount of received data can be read with the check_received_amount function.
		// If is_setup is true, the packet will be sent with a SETUP token. The length should be <= 8 bytes in this case.
		//
		// No constraints are placed on the location of buffer (i.e. no DMA is used, so it can be placed in CCMRAM)
		transaction_status submit_xfer(pipe_t idx, uint16_t length, void * buffer, bool is_setup=false);

		// Begin a transfer, respecting the maximum packet size, over the indicated pipe. Upon completion, the callback provided will be called.
		//
		// The state can also be read with the check_xfer_state function.
		// No constraints are placed on the location of buffer.
		transaction_status submit_xfer(pipe_t idx, uint16_t length, void * buffer, const pipe::Callback& cb, bool is_setup=false);

		// Check the state of a transfer on a pipe.
		transaction_status check_xfer_state(pipe_t idx);

		// Check the amount of received data (from the FIFO) for this packet
		uint16_t check_received_amount(pipe_t idx);

	private:
		// Channel callbacks
		pipe::Callback pipe_callbacks[8]; // We only handle 8 channels

		// Channel buffers
		// Set to 0 to indicate nothing present, and to a value of transaction_status + 0x1F00 to indicate a status.
		void * pipe_xfer_buffers[8] = {0};

		// Channel tx amounts. These are updated as data is read from/placed into the FIFOs
		uint16_t pipe_xfer_rx_amounts[8] = {0};

		// Data toggles
		uint8_t data_toggles = 0;
		
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

		// Check which device is inserted
		template<typename T>
		inline std::enable_if_t<is_contained_v<T, SupportedDevices...>, bool> inserted() const {
			return device.template holds<T>();
		}

		// Check which device is inserted
		//
		// Note that this overload will always return false because T is not a SupportedDevice
		template<typename T>
		inline constexpr std::enable_if_t<!is_contained_v<T, SupportedDevices...>, bool> inserted() const {
			return false;
		}

		// Get a reference to the device
		template<typename T>
		inline std::enable_if_t<is_contained_v<T, SupportedDevices...>, const T&> dev() const {
			return device.template get<T>();
		}

		// Get a reference to the device
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
