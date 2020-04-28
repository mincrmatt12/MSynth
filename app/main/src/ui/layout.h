#pragma once
// The MSynth UI layout engine:
//
// For the most part, the MSynth uses a very static layout system. The only real ways positions of screen elements can change
// is with horizontal or vertical scrolling; which can be applied to any part of the screen as a rectangle.
//
// Overlays are implemented as separate views. Conditional sections are implemented here as well.
//
// The general model is that a given UI component, let's use a button for simplicity, will contain all of its state _except_ for
// positioning information.
//
// e.g.:
//
// { in class
// 	 Button myButton("says hi", &do_say_hi);
// }
//
// All components then get their layout (and events) from a global constexpr instance of something that eventually becomes a LayoutManager.
// The thing that makes this actually save RAM is that these instances are both highly templated and _stored in flash_.
//
// An example of this is shown:
//
// constexpr l::LayoutManager lm_playScreen({&Screen::myButton, l::Pos(0, 0, 4, 4)});
//
// As a consequence, the LayoutManager class is itself an EventHandler<KeyEvent, TouchEvent> which allows it to forward correctly.
//
// The system is more powerful than this, though. LayoutManagers can also contain explicit constraints. This can be used, for example, to create paginated views.
//
// The advantage of making the entire system a massive template monster is that with the correct optimizations the compiler should generate some fairly concise code
// for checking overlaps/events. It also _shouldn't_ emit much of anything for the actual definition of the LayoutManager itself, only functions. In c++20 the [[non_unique_address]] attribute
// should be applied to LayoutManagers to maximize this.

#include <tuple> 
#include <type_traits>
#include "../evt/events.h"
#include "bounding.h"

namespace ms::ui::layout {
	// TEMPLATE HELPERS
	template<int N, typename... Ts>
	using element_at = std::tuple_element_t<N, std::tuple<Ts...>>;

	template<typename T>
	struct pointed_class_type;

	template<typename C, typename P>
	struct pointed_class_type<P C::*> {
		typedef C clazz;
		typedef P member;
	};

	// UI OBJECT TRAITS
	
	namespace traits {
		// Include this (or something that inherits from it) to indicate usage of the mouse
		struct MouseTrait {};
		// Include this (or something that inherits from it) to indicate usage of the keys
		struct KeyTrait {};

		// This trait tells the layout system that there's a bounding box (called bbox) which has x and y parameters which lets events passed on get put into the local
		// coordinate system. This is essential for nested layoutmanagers. It's also auto-added if you use an Inside with the default bbox.
		struct HasAdjustableBBox : MouseTrait {};

		// This is the more customizable form of the Inside trait, which can be specified multiple times
		// and allows you to customize the location of the member to check (it's effectively an easy form for the function 
		// handler)
		template<auto Pointer, bool IsBBOX=false>
		struct BasicInside : std::conditional_t<IsBBOX && /* TODO check xy */ true, HasAdjustableBBox, MouseTrait> {
			typedef decltype(Pointer) MemberPointer;
			constexpr static MemberPointer pointer = Pointer;
		};

		// Including this specifices that you only want mouse events when the cursor is "inside" 
		// a given object. This object should be called "bbox" inside
		// the given LayoutParams -- if you want multiple, use the "basic" form
		template<typename T>
		using Inside = BasicInside<&T::bbox, true>;

		// This trait limits keyboard events to whenever the object is focused. Focusing is stored directly
		// on the parent UI (under the name currently_focused), and the specific order is specified by the layout parameters. Specifically, a member called `focus_index`
		// should exist, and should be unique for all objects present.
		struct FocusEnable : KeyTrait {};

		// This trait can be used as a "catch-all" case for when you want custom filter logic
		template<auto Pointer, bool IsMouse> 
		struct Predicate : std::conditional_t<!IsMouse, KeyTrait, MouseTrait> {
			constexpr static decltype(Pointer) pointer = Pointer;
		};

		// This trait is a slightly faster version of the above for flags; this only matches _if_ the specified flag is set
		// You also have to provide the type of event it applies to.
		template<size_t FlagNo, typename Kind>
		struct IfFlag : Kind {
			constexpr static inline size_t flag_no = FlagNo;
		};

		// Min pressure for interaction
		template<int16_t Val>
		struct MinPressure {
			constexpr static inline int16_t value = Val;
		};

		// This trait wraps another trait and causes it to have high priority over other traits, so that if it's type matches
		// and it is true, regardless of other traits, the event will be sent. Useful for "press-and-drag" type situations.
		// If there are multiple of these, they are logically _OR_'d together, as opposed to ANDed like usual.
		template<typename Trait>
		struct ForceIf {
			typedef Trait ContainedTrait;
		};

		template<typename Check, template <typename, typename...> typename Template>
		struct is_trait_instance : std::false_type {};

		template<template <typename, typename...> typename Template, typename... Inner>
		struct is_trait_instance<Template<Inner...>, Template> : std::true_type {};

		template<typename Check, template <auto, auto...> typename Template>
		struct is_trait_instance_r : std::false_type {};

		template<template <auto, auto...> typename Template, auto... Inner>
		struct is_trait_instance_r<Template<Inner...>, Template> : std::true_type {};
	};

	// The base LayoutTraits object is sort of "glue": it sets up the various static functions necessary based on its 
	// template arguments. These arguments are all part of the `traits` namespace.
	template<typename ...Traits>
	struct LayoutTraits {
		constexpr static inline bool uses_mouse = std::disjunction_v<std::is_base_of_v<traits::MouseTrait, Traits>...>;
		constexpr static inline bool uses_key = std::disjunction_v<std::is_base_of_v<traits::KeyTrait, Traits>...>;
		constexpr static inline bool has_adjustable_bbox = std::disjunction_v<std::is_base_of_v<traits::HasAdjustableBBox, Traits>...>;

		template<typename Event, typename UI, typename Child>
		inline static bool use(const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			static_assert(std::is_same_v<Event, evt::TouchEvent> || std::is_same_v<Event, evt::KeyEvent>, "other event types not supported");

			// First check necessary ForceIfs

			if (((std::is_base_of_v<event_to_trait_base_t<Event>, Traits> && traits::is_trait_instance<Traits, traits::ForceIf>::value && check(Traits::ContainedTrait(), evt, parent, child, params)) || ...)) return true;

			// Now check all the other clauses
			return !((std::is_base_of_v<event_to_trait_base_t<Event>, Traits> && !check(Traits(), evt, parent, child, params)) || ...);
		}
	
	private:
		template<typename T>
		using event_to_trait_base_t = std::conditional_t<std::is_same_v<T, evt::TouchEvent>, traits::MouseTrait, traits::KeyTrait>;

		template<typename Event, typename UI, typename Child>
		inline static bool check(const traits::FocusEnable&, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return parent.currently_focused == params.focus_index;
		}

		template<typename T, typename UI, typename Child>
		inline static std::enable_if_t<traits::is_trait_instance_r<T, traits::BasicInside>::value, bool> 
			check(const T&, const evt::TouchEvent& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {

			if constexpr (std::is_same_v<pointed_class_type<typename T::MemberPointer>::clazz, Child>) {
				return inside(evt.x, evt.y, child.*T::pointer);
			}
			else {
				return inside(evt.x, evt.y, params.*T::pointer);
			}
		}

		template<int16_t Min, typename UI, typename Child>
		inline static bool check(const traits::MinPressure<Min>&, const evt::TouchEvent& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return evt.pressure >= Min;
		}

		template<size_t FlagNo, typename Event, typename UI, typename Child>
		inline static bool check(const traits::IfFlag<FlagNo, event_to_trait_base_t<Event>>&, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return child.flags & (1 << FlagNo);
		}

		template<typename T, typename Event, typename UI, typename Child>
		inline static std::enable_if_t<traits::is_trait_instance_r<T, traits::Predicate>::value, bool>
			check(const T&, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {

			return (child.*T::pointer)(evt, parent, child, params);
		}

		template<typename T, typename Event, typename UI, typename Child>
		inline static constexpr bool check(const T&, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			// Default case, always true
			return true;
		}
	};

	// MAIN LAYOUTMANAGER DEFINITION
	//
	// "adapter" types are elsewhere.

	template<typename Managing, typename ...Children>
	class LayoutManager {
		// FRIENDS
		template<typename ...Args, size_t ...Is>
		friend constexpr auto make_layout_impl(Args... args, std::index_sequence<Is...> idx);

		// HELPER TYPEDEFS
		typedef std::tuple<Children Managing::*...> ChildrenContainer;
		typedef std::tuple<typename Children::LayoutParams...> ParamsContainer; 
		typedef std::index_sequence_for<Children...> ChildrenIter;

		constexpr static inline int ChildCount = std::tuple_size_v<ChildrenContainer>;

		ChildrenContainer children;
		ParamsContainer layout_params;

		template<typename ...Args, size_t ...Is>
		constexpr LayoutManager(std::tuple<Args...> args, std::index_sequence<Is...> idx) :
			children(std::get<Is * 2>(args)...),
			layout_params(std::get<Is * 2 + 1>(args)...)
		{}

		template<typename Child, size_t Index, typename Event>
		inline Event mangle(const Event& in) {
			if constexpr (std::is_same_v<Event, evt::TouchEvent> && Child::LayoutTraits::has_adjustable_bbox) {
				auto x = in;
				x.x -= std::get<Index>(layout_params).bbox.x;
				x.y -= std::get<Index>(layout_params).bbox.y;
				return x;
			}
			else return in;
		}

		template<typename Event, size_t... Is>
		void dispatch(Managing& mg, const Event& evt, std::index_sequence<Is...>) {
			// Separated because of Is b.s.

			// This function has a fairly simple responsibility; 	
			(((std::is_same_v<Event, evt::TouchEvent> ? Children::LayoutTraits::uses_mouse : Children::LayoutTraits::uses_key) && 
			  Children::LayoutTraits::use(evt, mg, mg.*std::get<Is>(children), mg.*std::get<Is>(layout_params)) && 
			  mg.*std::get<Is>(children).handle(mangle<Children, Is>(evt), mg, std::get<Is>(layout_params))) || ...);
		}

	public:
		void handle(Managing& mg, const evt::KeyEvent& evt) {
			dispatch(mg, evt, ChildrenIter{});
		}
	
		void handle(Managing& mg, const evt::TouchEvent& evt) {
			dispatch(mg, evt, ChildrenIter{});
		}
	};

	template<typename ...Args, size_t ...Is>
	constexpr auto make_layout_impl(Args... args, std::index_sequence<Is...> idx) {
		return LayoutManager<
			typename pointed_class_type<element_at<0, Args...>>::clazz,
			typename pointed_class_type<element_at<Is*2, Args...>>::member...
		>(std::forward_as_tuple(args...), idx);
	}

	template<typename ...Args>
	constexpr auto make_layout(Args... args) {
		return make_layout_impl<Args...>(args..., std::make_index_sequence<sizeof...(Args)/2>{});
	}
}

namespace ms::ui {
	// convenience alias 
	namespace l = layout;
}

