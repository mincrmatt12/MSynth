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
		NotInserted, // Nothing is currently inserted into the USB port.
		TxnErrorDuringEnumeration, // There was a transactional error during enumeration
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

	// Various common USB types/structure
	namespace raw {
		struct alignas(4) SetupData {
			uint8_t bmRequestType;
			uint8_t bRequest;
			uint16_t wValue;
			uint16_t wIndex;
			uint16_t wLength;
		};

		struct DescriptorHead {
			uint8_t bLength;
			uint8_t bDescriptorType;

			const inline static uint8_t TypeDEVICE = 1;
			const inline static uint8_t TypeCONFIGURATION = 2;
			const inline static uint8_t TypeINTERFACE = 4;
			const inline static uint8_t TypeENDPOINT = 5;
		};

		struct alignas(4) DeviceDescriptor {
			DescriptorHead head;
			uint16_t bcdUSB;
			uint8_t bDeviceClass;
			uint8_t bDeviceSubClass;
			uint8_t bDeviceProtocol;
			uint8_t bMaxPacketSize0;
			uint16_t idVendor;
			uint16_t idProduct;
			uint16_t bcdDevice;
			uint8_t iManufacturer;
			uint8_t iProduct;
			uint8_t iSerialNumber;
			uint8_t bNumConfigurations;
		};

		struct alignas(4) ConfigurationDescriptor {
			DescriptorHead head;
			uint16_t wTotalLength;
			uint8_t bNumInterfaces;
			uint8_t bConfigurationValue;
			uint8_t iConfiguration;
			uint8_t bmAttributes;
			uint8_t bMaxPower;
		};

		struct alignas(4) InterfaceDescriptor {
			DescriptorHead head;
			uint8_t bInterfaceNumber;
			uint8_t bAlternateSetting;
			uint8_t bNumEndpoints;
			uint8_t bInterfaceClass;
			uint8_t bInterfaceSubClass;
			uint8_t bInterfaceProtocol;
			uint8_t iInterface;
		};

		static_assert(sizeof(SetupData) == 8, "invalid packing rules on SetupData");
		static_assert(sizeof(DeviceDescriptor) == 18 + 2 /* align */, "invalid packing rules on DeviceDescriptor");
		static_assert(sizeof(ConfigurationDescriptor) == 9 + 3 /* align */, "invalid packing rules on ConfigurationDescriptor");
		static_assert(sizeof(InterfaceDescriptor) == 9 + 3 /* align */, "invalid packing rules on InterfaceDescriptor");
	}

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
			//
			// Enumeration on the MSynth is separated into three phases:
			// 	1. Get the maximum EP0 packet size by doing an 8-byte read with mps = 8, and set the address to 0x10
			// 	2. Read the entire device descriptor, or if necessary the configuration descriptor data
			// 	3. Pass off remaining initialization to the correct class driver.

			// Start by allocating a pipe
			pipe_t ep0_pipe = allocate_pipe();

			// PHASE 1 START
			// Get the maximum EP0 size

			// Set the pipe up for a SETUP (out-style packet to control)
			configure_pipe(ep0_pipe, 0 /* setup address */, 0 /* DCP */, 8 /* minimum maximum packet size */, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, false /* data toggle is always 0 */);
			// Create the SETUP payload
			raw::SetupData request;
			request.bmRequestType = 0b1'00'00000; //device to host, standard, device
			request.bRequest = 6; // GET_DESCRIPTOR
			request.wValue = (1 /* DEVICE */ << 8);
			request.wIndex = 0;
			request.wLength = 8; // only get the first 8 bytes
			// Send this request
			submit_xfer(ep0_pipe, sizeof(request), &request, true); // is_setup=True
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}

			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}
			
			// It was! Now we can read the device descriptor
			alignas(4) uint8_t partial_device_descriptor[8];
			configure_pipe(ep0_pipe, 0, 0, 8, pipe::EndpointDirectionIn, pipe::EndpointTypeControl, true); // data toggle is 1
			submit_xfer(ep0_pipe, 8, &partial_device_descriptor);
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}

			// Alright, we now have the max EP0-size in partial_device_descriptor[7].
			// This is one of the values that will get passed to the newly created device
			uint8_t ep0_mps = partial_device_descriptor[7];

			// Send a status phase.
			configure_pipe(ep0_pipe, 0, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true); // data toggle is still 1, yo
			// Send a 0-byte message. Note that sending `nullptr` to submit_xfer is invalid due to how states are handled, so we send the special
			// value 0xfeedfeed instead (it will never be deref'd)
			submit_xfer(ep0_pipe, 0, (void*)0xfeedfeed);
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}

			// CONTROL TRANSFER 1 is complete.

			// TODO: do we need a reset here?
			
			configure_pipe(ep0_pipe, 0 /* setup address */, 0 /* DCP */, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, false /* data toggle is always 0 */);
			// Set address to 0x10 (some random number) to make the USB happy
			request.bmRequestType = 0;
			request.bRequest = 5; // SET_ADDRESS
			request.wValue = 0x10; // New address
			request.wIndex = 0;
			request.wLength = 0;
			// Send this request
			submit_xfer(ep0_pipe, sizeof(request), &request, true); // is_setup=True
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}
			// No data phase, so send an IN with size == 0
			//
			// TODO: should this go to addr == 0x10?
			configure_pipe(ep0_pipe, 0 /* setup address */, 0 /* DCP */, ep0_mps, pipe::EndpointDirectionIn, pipe::EndpointTypeControl, true);
			submit_xfer(ep0_pipe, 0, (void*)0xdeefdeef);
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}

			// CONTROL TRANSFER 2 (SET_ADDRESS) is complete
			// PHASE 1 IS COMPLETE

			// PHASE 2 START
			// Read the entire device descriptor. It is _exactly_ 18 bytes (0x12) long
			request.bmRequestType = 0b1'00'00000; //device to host, standard, device
			request.bRequest = 6; // GET_DESCRIPTOR
			request.wValue = (1 /* DEVICE */ << 8);
			request.wIndex = 0;
			request.wLength = 18; // get the entire thing
			configure_pipe(ep0_pipe, 0x10 /* new address */, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true);
			// Send this request
			submit_xfer(ep0_pipe, sizeof(request), &request, true); // is_setup=True
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}

			// Read the entire thing in the data phase
			configure_pipe(ep0_pipe, 0x10 /* new address */, 0, ep0_mps, pipe::EndpointDirectionIn, pipe::EndpointTypeControl, false);
			raw::DeviceDescriptor dd;
			submit_xfer(ep0_pipe, 0x12, &dd);
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}
			
			// Send a STATUS OUT
			configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true); // data toggle is still 1, yo
			submit_xfer(ep0_pipe, 0, (void*)0xfeedfeed);
			// Wait for it to finish
			while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
			// Was this transfer ok?
			if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
				// We have failed
				return init_status::TxnErrorDuringEnumeration;
			}

			// CONTROL TRANSFER 3 (get_descriptor 2) COMPLETE

			if (dd.bDeviceClass == 0 && dd.bDeviceSubClass == 0 && dd.bDeviceProtocol == 0) {
				// We need to first find the size of the descriptors
				request.bmRequestType = 0b1'00'00000; //device to host, standard, device
				request.bRequest = 6; // GET_DESCRIPTOR
				request.wValue = (2 /* CONFIGURATION */ << 8);
				request.wIndex = 0;
				request.wLength = 9; // get the entire thing
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true);
				submit_xfer(ep0_pipe, sizeof(request), &request, true); // is_setup=True
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}

				// Read the entire thing in the data phase
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionIn, pipe::EndpointTypeControl, false);
				raw::ConfigurationDescriptor cd;
				submit_xfer(ep0_pipe, 0x9, &cd);
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}
				
				// Send a STATUS OUT
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true); // data toggle is still 1, yo
				submit_xfer(ep0_pipe, 0, (void*)0xfeedfeed);
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}

				// We now have the CD, so let's read the entire descriptor set
				alignas(4) uint8_t configuration_descriptor_set[cd.wTotalLength];
				request.bmRequestType = 0b1'00'00000; //device to host, standard, device
				request.bRequest = 6; // GET_DESCRIPTOR
				request.wValue = (2 /* CONFIGURATION */ << 8);
				request.wIndex = 0;
				request.wLength = cd.wTotalLength; // get the entire thing
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true);
				submit_xfer(ep0_pipe, sizeof(request), &request, true); // is_setup=True
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}

				// Read the entire thing in the data phase
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionIn, pipe::EndpointTypeControl, false);
				submit_xfer(ep0_pipe, cd.wTotalLength, &configuration_descriptor_set);
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}
				
				// Send a STATUS OUT
				configure_pipe(ep0_pipe, 0x10, 0, ep0_mps, pipe::EndpointDirectionOut, pipe::EndpointTypeControl, true); // data toggle is still 1, yo
				submit_xfer(ep0_pipe, 0, (void*)0xfeedfeed);
				while (check_xfer_state(ep0_pipe) == transaction_status::Busy) {;}
				if (check_xfer_state(ep0_pipe) != transaction_status::Ack) {
					return init_status::TxnErrorDuringEnumeration;
				}

				// We now have a set of INTERFACE and ENDPOINT DESCRIPTORS.
				// It is therefore safe to destroy ep0_pipe

				destroy_pipe(ep0_pipe);
				// Using some questionable logic, we iterate over all of them.

				// Devices will almost certainly re-request this data but eh.
				for (int i = 0; i < cd.wTotalLength; i += ((raw::DescriptorHead *)(configuration_descriptor_set + i))->bLength) {
					if (((raw::DescriptorHead *)configuration_descriptor_set + i)->bDescriptorType != raw::DescriptorHead::TypeINTERFACE) continue;
					const raw::InterfaceDescriptor& id = *(raw::InterfaceDescriptor *)(configuration_descriptor_set + i);

					if ((
					 (SupportedDevices::handles(id.bInterfaceClass, id.bInterfaceSubClass, id.bInterfaceProtocol) && 
					  (
						device.template assign<SupportedDevices>(), 
						dev<SupportedDevices>().init(ep0_mps, id.bInterfaceClass, id.bInterfaceSubClass, id.bInterfaceProtocol, static_cast<HostBase *>(this)),
						true
					  )
					 ) || ...
					)) {
						return init_status::Ok;
					}
				}

				// We have not got the correct thing
				return init_status::NotSupported;
			}

			// PHASE 2 COMPLETE

			else {
				
				// PHASE 3a START (no config)
				// We can now destroy our pipe
				destroy_pipe(ep0_pipe);

				// Begin deferring to the different types of SupportedDevices.

				// If you get a compile error here you probably didn't define a static handles in your device class
				if (!(
				 (SupportedDevices::handles(dd.bDeviceClass, dd.bDeviceSubClass, dd.bDeviceProtocol) && 
				  (
					device.template assign<SupportedDevices>(), 
					dev<SupportedDevices>().init(ep0_mps, dd.bDeviceClass, dd.bDeviceSubClass, dd.bDeviceProtocol, static_cast<HostBase *>(this)),
					true
				  )
				 ) || ...
				)) {
					return init_status::NotSupported;
				}

			}

			// Otherwise, we have succesfully initialized it

			return init_status::Ok;
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
		// Does this driver deal with this device/interface combo?
		static bool handles(uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol);
		
		void init(uint8_t ep0_mps, uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol, HostBase *hb);
	private:
		uint8_t ep_in, ep_out;
	};

	struct HID {
		static bool handles(uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol);

		void init(uint8_t ep0_mps, uint8_t bClass, uint8_t bSubClass, uint8_t bProtocol, HostBase *hb);
	};
}
