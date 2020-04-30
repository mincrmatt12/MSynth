#pragma once
// The MSynth UI layout engine:
//
// For the most part, the MSynth uses a very static layout system. The only real ways positions of screen elements can change
// is with horizontal or vertical scrolling; which can be applied to any part of the screen as a rectangle.
//
// Overlays are implemented as separate views. Conditional sections are implemented here as well.
//
// The general model is that a given UI component, let's use a button for simplicity, will contain all of its state _except_ for
// positioning information (or really anything the object wants, but the idea is only stuff that won't ever change at runtime; it's not impossible
// for say a label to put its styling information here as well if it so desires.).
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
// constexpr auto lm_playScreen = l::make_layout(&PlayScreen::myButton, Box{0, 0, 20, 50},
// 												 &PlayScreen::myButton2, Box{20, 0, 20, 50}); // where the underlying layoutparams type has an implicit conversion from Box
//
// The advantage of making the entire system a massive template monster is that with the correct optimizations the compiler should generate some fairly concise code
// for checking overlaps/events. It also _shouldn't_ emit much of anything for the actual definition of the LayoutManager itself, only functions. In c++20 the [[non_unique_address]] attribute
// should be applied to LayoutManagers to maximize this.
//
// The LayoutManager class itself doesn't implement EventHandler (because that would require it to have state, and would prevent it from handling multiple instances at once), instead
// the UI class should implement the necessary handlers itself and forward them to LayoutManager.handle(*this, event); For simple UIs, this is all the handler needs to be, although sadly
// it's impossible to do templated overrides so you have to write it twice.
//
// For redrawing the screen, the LayoutManager is capable of reading the dirty flag (standardized as the first flag in the flags bitmask; a helper class which always reads 0 and never
// lets you write to it can be used for invisible elements) and calling the appropriate draw methods. It also takes a bounding box within which to draw the UI. The positioning information is
// used to offset all capable elements (those that have and match the requirements of traits::HasAdjustableBBox) and the size information is used to setup the bounding globals in the draw engine
// so that elements outside the bounding box are clipped correctly.
//
// This is done to implement scrolling; a UI class can elect to handle the necessary dirty checking and bounding box updates, however various helper "components" are implemented that manage 
// the scrolling in various intuitive ways. They take advantage of another one of the features of the LayoutManager: the handle functions return a boolean corresponding to whether or not any element 
// actually handled an event. This will probably see more use in nested LayoutManagers, as they are perfectly capable of working inside of components themselves (although the relevant sizing concerns
// may limit their usefulness; although relevant scaling is always a potential solution. If the class is lacking some critical feature, you can also short circuit its handling and add your own
// clauses just with an if or or.
//
// We expect all elements to:
// 	- have a publicly visible bitfield called "flags" where bit 0 is the dirty flag
// 	- for all handled event types:
// 		- have a function of the form bool Element::handle(const Event& evt, const Element::LayoutParams& lp)
// 		  where the return code indicates if the event should be sent to other elements.
// 		- have the appropriate traits set in the LayoutTraits typedef
//  - have a publicly visible typedef for LayoutParams
//      - for it to work with the *_list generators, it also should be "re-initable" with a constructor of the form:
//        LayoutParams(const Box& fill, LayoutParams&& old)
//        which uses the settings (if any) on the old parameter and uses them to construct an appropriate instance
//        to fill the region `fill`

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
		struct HasAdjustableBBox {
		};

		// Helper type; automatically inherits from MouseTrait too
		struct HasAdjustableBBoxMixin : HasAdjustableBBox, MouseTrait {};

		// This is the more customizable form of the Inside trait, which can be specified multiple times
		// and allows you to customize the location of the member to check (it's effectively an easy form for the function 
		// handler)
		template<auto Pointer, bool IsBBOX=false>
		struct BasicInside : std::conditional_t<IsBBOX && /* TODO check xy */ true, HasAdjustableBBoxMixin, MouseTrait> {
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
		//
		// Additionally, the presence of this trait means that you must handle the 'fake' FocusEvent which is sent whenever the focus changes. Focus is automatically
		// updated whenever a mouse interaction occurs with the element. This behavior can be extended by the parent UI class, which is able to manually update its focus_index (
		// although you _do_ need to call the relevant method in LayoutManager to get the change to propogate; there are helper methods included with the mixins in bases.h to 
		// make this less annoying)
		template<typename Base=KeyTrait>
		struct FocusEnable : Base {};

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

		// Subscribe to a specific set of touch events
		template<decltype(evt::TouchEvent::state)... Types>
		struct MouseTypes {
			constexpr static inline uint32_t bitmask = ((uint32_t)Types | ...);
		};

		// This trait wraps another trait and causes it to have high priority over other traits, so that if it's type matches
		// and it is true, regardless of other traits, the event will be sent. Useful for "press-and-drag" type situations.
		// If there are multiple of these, they are logically _OR_'d together, as opposed to ANDed like usual.
		template<typename Trait>
		struct ForceIf : std::conditional_t<std::is_base_of_v<KeyTrait, Trait>, KeyTrait, MouseTrait>{
		};

		// This trait is used by the auto-layout helpers (make_vertical_list, etc.) to specify that vertical/horizontal offsets can be deduced
		// from a LayoutParams using the given FPtr. If this isn't present, HasAdjustableBBox will be used as a fallback to specify that the `bbox`
		// can be used for positioning (although the offset will be grabbed from w, h, x and y).
		template<auto FPtr>
		struct HasCalculatableSizeOffsets {
			// This should be a const compatible function pointer.
			constexpr static inline auto callback = FPtr;
		};

		// This trait informs the LayoutManager that this element is a container of other elements. This means that it should implement at least stubs
		// for all of the events, and it also means that if no element matches for a focus query, it will get deflected here.
		//
		// It also should have a public member named "focus" which will focus the selected index (or 0 for none).
		struct IsContainer;

		// Logically ORs the parameters. Can be used for e.g. multiple click regions.
		template<typename ...Bases>
		struct Disjunction : std::conditional_t<std::disjunction_v<std::is_base_of<KeyTrait, Bases>...>, KeyTrait, MouseTrait> {};

		// Logically ANDs the parameters (useful for complex enable statements)
		template<typename ...Bases>
		struct Conjunction : std::conditional_t<std::disjunction_v<std::is_base_of<KeyTrait, Bases>...>, KeyTrait, MouseTrait> {};

		// Helper type traits to check templated trait types.
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
		constexpr static inline bool uses_mouse = std::disjunction_v<std::is_base_of<traits::MouseTrait, Traits>...>;
		constexpr static inline bool uses_key = std::disjunction_v<std::is_base_of<traits::KeyTrait, Traits>...>;
		constexpr static inline bool has_adjustable_bbox = std::disjunction_v<std::is_base_of<traits::HasAdjustableBBox, Traits>...>;
		constexpr static inline bool is_container = std::disjunction_v<std::is_base_of<traits::IsContainer, Traits>...>;
		constexpr static inline bool is_focusable = std::disjunction_v<traits::is_trait_instance<Traits, traits::FocusEnable>...>;

		template<typename Event, typename UI, typename Child>
		inline static bool use(const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			static_assert(std::is_same_v<Event, evt::TouchEvent> || std::is_same_v<Event, evt::KeyEvent>, "other event types not supported");

			// First check necessary ForceIfs

			if (((std::is_base_of_v<event_to_trait_base_t<Event>, Traits> && 
				  traits::is_trait_instance<Traits, traits::ForceIf>::value && 
				  check(Traits(), evt, parent, child, params)) || ...)) return true;

			// Now check all the other clauses
			return !((std::is_base_of_v<event_to_trait_base_t<Event>, Traits> && !traits::is_trait_instance<Traits, traits::ForceIf>::value && !check(Traits(), evt, parent, child, params)) || ...);
		}

		template<typename LayoutParams>
		static constexpr bool get_positioning_info(const LayoutParams& lp, int16_t& origin_x, int16_t& origin_y, int16_t &width, int16_t& height) {
			if constexpr (std::disjunction_v<traits::is_trait_instance_r<Traits, traits::HasCalculatableSizeOffsets>::value...>) {
				// Use the relevant callbacks
				((traits::is_trait_instance_r<Traits, traits::HasCalculatableSizeOffsets>::value && ((lp.*Traits::callback)(origin_x, origin_y, width, height), true)) || ...);
				return true;
			}
			else {
				// Otherwise, try falling back to BBOX
				if constexpr (has_adjustable_bbox) {
					origin_x = lp.bbox.x;
					origin_y = lp.bbox.y;
					width = lp.bbox.w;
					height = lp.bbox.h;
					return true;
				}
				else {
					// Fail
					return false;
				}
			}
		}
	
	private:
		template<typename T>
		using event_to_trait_base_t = std::conditional_t<std::is_same_v<T, evt::TouchEvent>, traits::MouseTrait, traits::KeyTrait>;

		template<typename T, typename Event, typename UI, typename Child>
		inline static bool check(const traits::FocusEnable<T>&, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return parent.currently_focused == params.focus_index;
		}

		template<typename T, typename UI, typename Child>
		inline static std::enable_if_t<traits::is_trait_instance_r<T, traits::BasicInside>::value, bool> 
			check(const T&, const evt::TouchEvent& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {

			if constexpr (std::is_same_v<typename pointed_class_type<typename T::MemberPointer>::clazz, Child>) {
				return inside(evt.x, evt.y, child.*T::pointer);
			}
			else {
				return inside(evt.x, evt.y, params.*T::pointer);
			}
		}

		template<typename UI, typename Child, auto ...V>
		inline static bool check(const traits::MouseTypes<V...>& x, const evt::TouchEvent& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return evt.state & std::decay_t<decltype(x)>::bitmask;
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

		template<typename Event, typename UI, typename Child, typename... Inner>
		inline static bool check(const traits::Disjunction<Inner...>, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return (check(Inner{}, evt, parent, child, params) || ...);
		}

		template<typename Event, typename UI, typename Child, typename... Inner>
		inline static bool check(const traits::Conjunction<Inner...>, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return (check(Inner{}, evt, parent, child, params) && ...);
		}

		template<typename Event, typename UI, typename Child, typename Inner>
		inline static bool check(const traits::ForceIf<Inner>, const Event& evt, const UI& parent, const Child& child, const typename Child::LayoutParams& params) {
			return check(Inner{}, evt, parent, child, params);
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
		inline Event mangle(const Event& in) const {
			if constexpr (std::is_same_v<Event, evt::TouchEvent> && Child::LayoutTraits::has_adjustable_bbox) {
				auto x = in;
				x.x -= std::get<Index>(layout_params).bbox.x;
				x.y -= std::get<Index>(layout_params).bbox.y;
				return x;
			}
			else return in;
		}

		template<typename Event, size_t... Is>
		bool dispatch(Managing& mg, const Event& evt, std::index_sequence<Is...> iter) const {
			// Dispatch events.
			//
			// For all elements...                  ... if the event is the right type ...
			constexpr static bool is_key = std::is_same_v<Event, evt::TouchEvent>;
			return (((is_key ? Children::LayoutTraits::uses_mouse : Children::LayoutTraits::uses_key) && 
					  //                        ... if the traits say we should use the event ...
			          Children::LayoutTraits::use(evt, mg, mg.*std::get<Is>(children), std::get<Is>(layout_params)) && 
					  // then do the following
			          (
					   //                                ... run the event handler ...
					   (mg.*std::get<Is>(children)).handle(mangle<Children, Is>(evt), std::get<Is>(layout_params)) &&
					   // ... if mouse event && is focusable ...         ... then set the focus to the element ...               ... and discard the result ...
					   (!is_key && Children::LayoutTraits::is_focusable && (focus_impl(mg, focus_index_for<Children>(std::get<Is>(layout_params)), iter), true))
					  )
					) || ...
		    );
		}

		template<typename Child>
		std::enable_if_t<Child::LayoutTraits::has_adjustable_bbox> redraw_impl(Child& child, const Box& bound, const typename Child::LayoutParams& unadjust) const {
			typename Child::LayoutParams adjusted = unadjust;
			adjusted.bbox.x += bound.x;
			adjusted.bbox.y += bound.y;
			child.draw(adjusted);
		}

		template<typename Child>
		inline std::enable_if_t<!Child::LayoutTraits::has_adjustable_bbox> redraw_impl(Child& child, const Box& bound, const typename Child::LayoutParams& unadjust) const {
			child.draw(unadjust);
		}

		template<size_t... Is>
		void redraw(Managing &mg, const Box& bound, std::index_sequence<Is...>) const {
			// TODO: update the "fake" draw-stack (globals for bounds checking)

			// Over all children...
			((((mg.*std::get<Is>(children)).flags & 1 /* dirty flag is always the first one */) && (
				/* reset the dirty flag */ (mg.*std::get<Is>(children)).flags ^= 1,
				/* adjust bbox and call draw */ redraw_impl(mg.*std::get<Is>(children), bound, std::get<Is>(layout_params)),
				/* keep iterating */ false
			)) || ...);
		}

		template<size_t... Is>
		void redraw(Managing &mg, std::index_sequence<Is...>) const {
			// TODO: update the "fake" draw-stack (globals for bounds checking)

			// Over all children...
			((((mg.*std::get<Is>(children)).flags & 1 /* dirty flag is always the first one */) && (
				/* reset the dirty flag */ (mg.*std::get<Is>(children)).flags ^= 1,
				/* adjust bbox and call draw */(mg.*std::get<Is>(children)).draw(std::get<Is>(layout_params)),
				/* keep iterating */ false
			)) || ...);
		}

		// Helper method to get the correct focus
		template<typename Child>
		inline constexpr uint16_t focus_index_for(const typename Child::LayoutParams& lp) const {
			if constexpr (Child::LayoutTraits::is_focusable) {
				return lp.focus_index;
			}
			else return 0;
		}

		// Helper method to avoid sf errors.
		template<typename Child>
		void focus_impl_tail(Managing& mg, Child& c, int16_t target) {
			if constexpr (Child::LayoutTraits::is_container) {
				c.focus(mg, target);
			}
		}

		template<bool UpdateNew=true, size_t... Is>
		void focus_impl(Managing& mg, int16_t new_focus, std::index_sequence<Is...>) const {
			if constexpr ((Children::LayoutTraits::is_focusable || ...)) {
				// Inform the previously focused element of the upcoming change
				((Children::LayoutTraits::is_focusable && mg.currently_focused == focus_index_for<Children>(std::get<Is>(layout_params)) && (
					(mg.*std::get<Is>(children)).handle(evt::FocusEvent{false}), true
				)) || ...) || ((focus_impl_tail(mg, mg.*std::get<Is>(children), 0), false) || ...);

				// set the new focus
				mg.currently_focused = new_focus;

				if constexpr (UpdateNew) {
					if (new_focus == 0) return;
					// Inform the newly focused element of the change
					((Children::LayoutTraits::is_focusable && new_focus == focus_index_for<Children>(std::get<Is>(layout_params)) && (
						(mg.*std::get<Is>).handle(evt::FocusEvent{true}), true
					)) || ...) || ((focus_impl_tail(mg, mg.*std::get<Is>(children), new_focus), false) || ...);
				}
			}
		}

	public:
		// Properly dispatch an event to the contained children based on their traits.
		bool handle(Managing& mg, const evt::KeyEvent& evt) const {
			return dispatch(mg, evt, ChildrenIter{});
		}
	
		// Properly dispatch an event to the contained children based on their traits.
		bool handle(Managing& mg, const evt::TouchEvent& evt) const {
			// ... dispatch the events normally ...      ... but if nothing swallows the pressed event, unfocus everything ...                ... but still return false    
			return dispatch(mg, evt, ChildrenIter{}) || (evt.state == evt::TouchEvent::StatePressed && (focus_impl<false>(mg, 0, ChildrenIter{}), false));
		}

		// Redraw the entire layout using the given bounds. Their position is added to all of the local params (with adjustable bboxes, anyways)
		// and the releveant draw:: globals are adjusted so as to constrain the draw calls to within the given box.
		//
		// Note that unlike the convention for event handling (where elements have their own coordinate scheme) the coordinates presented here
		// are global. The reasoning is that it makes it easier for elements to draw like this (and that the draw:: functions don't need to add
		// coordinates themselves, which makes them slightly more optimized for when the coordinates are always zero.
		void redraw(Managing& mg, const Box &bound) const {
			redraw(mg, bound, ChildrenIter{});
		}

		// Redraw the entire layout without adjusting bounding boxes. GCC has a hard time removing the case when this is constant, so this
		// is provided for optimization purposes. We assume here the draw:: globals are set properly (i.e. they won't obstruct us)
		void redraw(Managing& mg) const {
			redraw(mg, ChildrenIter{});
		}

		// Mark an element as focused/unfocused. This sets the relevant index in the class, although in general, it isn't necessary to call this.
		// Whenever a focus event occurs from within the class, this is automagically called. This is only really used when cycling the focus ID manually.
		//
		// I would use a fancy wrapper class 
		void focus(Managing& mg, uint16_t index) const {
			focus_impl(mg, index, ChildrenIter{});
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

