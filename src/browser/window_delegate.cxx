#include "window_delegate.hxx"
#include "include/capi/views/cef_window_capi.h"

void CEF_CALLBACK on_window_created(cef_window_delegate_t*, cef_window_t*);
void CEF_CALLBACK on_window_closing(cef_window_delegate_t*, cef_window_t*);
void CEF_CALLBACK on_window_destroyed(cef_window_delegate_t*, cef_window_t*);
void CEF_CALLBACK on_window_activation_changed(cef_window_delegate_t*, cef_window_t*, int);
void CEF_CALLBACK on_window_bounds_changed(cef_window_delegate_t*, cef_window_t*, const cef_rect_t*);
cef_window_t* CEF_CALLBACK get_parent_window(cef_window_delegate_t*, cef_window_t*, int*, int*);
cef_rect_t CEF_CALLBACK get_initial_bounds(cef_window_delegate_t*, cef_window_t*);
cef_show_state_t CEF_CALLBACK get_initial_show_state(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK is_frameless(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK can_resize(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK can_maximize(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK can_minimize(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK can_close(cef_window_delegate_t*, cef_window_t*);
int CEF_CALLBACK on_accelerator(cef_window_delegate_t*, cef_window_t*, int);
int CEF_CALLBACK on_key_event(cef_window_delegate_t*, cef_window_t*, const cef_key_event_t*);

namespace WindowDelegate {
	Browser::WindowDelegate* resolve(cef_window_delegate_t* delegate) {
		return reinterpret_cast<Browser::WindowDelegate*>(reinterpret_cast<uintptr_t>(delegate) - offsetof(Browser::WindowDelegate, cef_delegate));
	}

	Browser::WindowDelegate* resolve_view(cef_view_delegate_t* view_delegate) {
		auto offset = offsetof(Browser::WindowDelegate, cef_delegate) + offsetof(cef_window_delegate_t, base) + offsetof(cef_panel_delegate_t, base);
		return reinterpret_cast<Browser::WindowDelegate*>(reinterpret_cast<uintptr_t>(view_delegate) - offset);
	}

	Browser::WindowDelegate* resolve_base(cef_base_ref_counted_t* base) {
		auto offset = offsetof(Browser::WindowDelegate, cef_delegate) + offsetof(cef_window_delegate_t, base) + offsetof(cef_panel_delegate_t, base) + offsetof(cef_view_delegate_t, base);
		return reinterpret_cast<Browser::WindowDelegate*>(reinterpret_cast<uintptr_t>(base) - offset);
	}

	void CEF_CALLBACK add_ref(cef_base_ref_counted_t* delegate) {
		resolve_base(delegate)->add_ref();
	}

	int CEF_CALLBACK release(cef_base_ref_counted_t* delegate) {
		return resolve_base(delegate)->release();
	}

	int CEF_CALLBACK has_one_ref(cef_base_ref_counted_t* delegate) {
		return (resolve_base(delegate)->refcount == 1) ? 1 : 0;
	}

	int CEF_CALLBACK has_any_refs(cef_base_ref_counted_t* app) {
		return (resolve_base(app)->refcount >= 1) ? 1 : 0;
	}
}

Browser::WindowDelegate::WindowDelegate(cef_browser_view_t* browser_view, Details details): browser_view(browser_view), details(details) {
	// A long chain of bases. cef_delegate is cef_window_delegate_t, the "base chain" is as follows:
	// cef_window_delegate_t > cef_panel_delegate_t > cef_view_delegate_t > cef_base_ref_counted_t
	this->cef_delegate.base.base.base.size = sizeof(cef_window_delegate_t);
	this->cef_delegate.base.base.base.add_ref = ::WindowDelegate::add_ref;
	this->cef_delegate.base.base.base.release = ::WindowDelegate::release;
	this->cef_delegate.base.base.base.has_one_ref = ::WindowDelegate::has_one_ref;
	this->cef_delegate.base.base.base.has_at_least_one_ref = ::WindowDelegate::has_any_refs;

	// The `cef_view_delegate_t` functions won't be called here,
	// implement the ones in browser_view_delegate.cxx instead
	this->cef_delegate.base.base.get_preferred_size = nullptr;
	this->cef_delegate.base.base.get_minimum_size = nullptr;
	this->cef_delegate.base.base.get_maximum_size = nullptr;
	this->cef_delegate.base.base.get_height_for_width = nullptr;
	this->cef_delegate.base.base.on_child_view_changed = nullptr;
	this->cef_delegate.base.base.on_window_changed = nullptr;
	this->cef_delegate.base.base.on_layout_changed = nullptr;
	this->cef_delegate.base.base.on_focus = nullptr;
	this->cef_delegate.base.base.on_blur = nullptr;

	this->cef_delegate.on_window_created = ::on_window_created;
	this->cef_delegate.on_window_closing = ::on_window_closing;
	this->cef_delegate.on_window_destroyed = ::on_window_destroyed;
	this->cef_delegate.on_window_activation_changed = ::on_window_activation_changed;
	this->cef_delegate.on_window_bounds_changed = ::on_window_bounds_changed;
	this->cef_delegate.get_parent_window = ::get_parent_window;
	this->cef_delegate.get_initial_bounds = ::get_initial_bounds;
	this->cef_delegate.get_initial_show_state = ::get_initial_show_state;
	this->cef_delegate.is_frameless = ::is_frameless;
	this->cef_delegate.can_resize = ::can_resize;
	this->cef_delegate.can_maximize = ::can_maximize;
	this->cef_delegate.can_minimize = ::can_minimize;
	this->cef_delegate.can_close = ::can_close;
	this->cef_delegate.on_accelerator = ::on_accelerator;
	this->cef_delegate.on_key_event = ::on_key_event;
	this->refcount = 1;
}

void Browser::WindowDelegate::add_ref() {
	this->refcount += 1;
}

int Browser::WindowDelegate::release() {
	// Since this->refcount is atomic, the operation of decrementing it and checking if the result
	// is 0 must only access the value once. Accessing it twice would cause a race condition.
	if (--this->refcount == 0) {
		this->destroy();
		return 1;
	}
	return 0;
}

void Browser::WindowDelegate::destroy() {
	this->browser_view->base.base.release(&this->browser_view->base.base);
}

cef_window_delegate_t* Browser::WindowDelegate::window_delegate() {
	this->add_ref();
	return &this->cef_delegate;
}

cef_view_delegate_t* Browser::WindowDelegate::view_delegate() {
	this->add_ref();
	return &this->cef_delegate.base.base;
}

void CEF_CALLBACK on_window_created(cef_window_delegate_t* self, cef_window_t* window) {
	window->base.add_child_view(&window->base, &WindowDelegate::resolve(self)->browser_view->base);
	window->show(window);
}

void CEF_CALLBACK on_window_closing(cef_window_delegate_t*, cef_window_t*) {

}

void CEF_CALLBACK on_window_destroyed(cef_window_delegate_t*, cef_window_t*) {

}

void CEF_CALLBACK on_window_activation_changed(cef_window_delegate_t*, cef_window_t*, int) {

}

void CEF_CALLBACK on_window_bounds_changed(cef_window_delegate_t*, cef_window_t*, const cef_rect_t*) {

}

cef_window_t* CEF_CALLBACK get_parent_window(cef_window_delegate_t*, cef_window_t*, int*, int*) {
	return nullptr;
}

cef_rect_t CEF_CALLBACK get_initial_bounds(cef_window_delegate_t* self_, cef_window_t* view) {
	auto self = WindowDelegate::resolve(self_);
	return cef_rect_t {
		.x = self->details.startx,
		.y = self->details.starty,
		.width = self->details.preferred_width,
		.height = self->details.preferred_height,
	};
}

cef_show_state_t CEF_CALLBACK get_initial_show_state(cef_window_delegate_t*, cef_window_t*) {
	return CEF_SHOW_STATE_NORMAL;
}

int CEF_CALLBACK is_frameless(cef_window_delegate_t* self, cef_window_t*) {
	return !WindowDelegate::resolve(self)->details.frame;
}

int CEF_CALLBACK can_resize(cef_window_delegate_t* self, cef_window_t*) {
	return !WindowDelegate::resolve(self)->details.resizeable;
}

int CEF_CALLBACK can_maximize(cef_window_delegate_t*, cef_window_t*) {
	return false;
}

int CEF_CALLBACK can_minimize(cef_window_delegate_t*, cef_window_t*) {
	return false;
}

int CEF_CALLBACK can_close(cef_window_delegate_t*, cef_window_t*) {
	return true;
}

int CEF_CALLBACK on_accelerator(cef_window_delegate_t*, cef_window_t*, int) {
	return false;
}

int CEF_CALLBACK on_key_event(cef_window_delegate_t*, cef_window_t*, const cef_key_event_t*) {
	return false;
}

