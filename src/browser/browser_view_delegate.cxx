#include "browser_view_delegate.hxx"
#include "include/capi/views/cef_browser_view_capi.h"
#include "include/capi/views/cef_view_capi.h"

cef_size_t CEF_CALLBACK get_preferred_size(cef_view_delegate_t*, cef_view_t* view);
cef_size_t CEF_CALLBACK get_minimum_size(cef_view_delegate_t*, cef_view_t* view);
cef_size_t CEF_CALLBACK get_maximum_size(cef_view_delegate_t*, cef_view_t* view);
int CEF_CALLBACK get_height_for_width(cef_view_delegate_t*, cef_view_t* view, int);
void CEF_CALLBACK on_parent_view_changed(cef_view_delegate_t*, cef_view_t* view, int, cef_view_t*);
void CEF_CALLBACK on_child_view_changed(cef_view_delegate_t*, cef_view_t* view, int, cef_view_t*);
void CEF_CALLBACK on_window_changed(cef_view_delegate_t*, cef_view_t* view, int);
void CEF_CALLBACK on_layout_changed(cef_view_delegate_t*, cef_view_t* view, const cef_rect_t*);
void CEF_CALLBACK on_focus(cef_view_delegate_t*, cef_view_t* view);
void CEF_CALLBACK on_blur(cef_view_delegate_t*, cef_view_t* view);
void CEF_CALLBACK on_browser_created(cef_browser_view_delegate_t*, cef_browser_view_t*, cef_browser_t*);
void CEF_CALLBACK on_browser_destroyed(cef_browser_view_delegate_t*, cef_browser_view_t*, cef_browser_t*);
cef_browser_view_delegate_t* CEF_CALLBACK get_delegate_for_popup_browser_view(cef_browser_view_delegate_t*, cef_browser_view_t*, const cef_browser_settings_t*, cef_client_t*, int);
int CEF_CALLBACK on_popup_browser_view_created(cef_browser_view_delegate_t*, cef_browser_view_t*, cef_browser_view_t*, int);
cef_chrome_toolbar_type_t CEF_CALLBACK get_chrome_toolbar_type(cef_browser_view_delegate_t*);

namespace BrowserViewDelegate {
	Browser::BrowserViewDelegate* resolve(cef_browser_view_delegate_t* delegate) {
		return reinterpret_cast<Browser::BrowserViewDelegate*>(reinterpret_cast<uintptr_t>(delegate) - offsetof(Browser::BrowserViewDelegate, cef_delegate));
	}

	Browser::BrowserViewDelegate* resolve_view(cef_view_delegate_t* view_delegate) {
		auto offset = offsetof(Browser::BrowserViewDelegate, cef_delegate) + offsetof(cef_browser_view_delegate_t, base);
		return reinterpret_cast<Browser::BrowserViewDelegate*>(reinterpret_cast<uintptr_t>(view_delegate) - offset);
	}

	Browser::BrowserViewDelegate* resolve_base(cef_base_ref_counted_t* base) {
		auto offset = offsetof(Browser::BrowserViewDelegate, cef_delegate) + offsetof(cef_browser_view_delegate_t, base) + offsetof(cef_view_delegate_t, base);
		return reinterpret_cast<Browser::BrowserViewDelegate*>(reinterpret_cast<uintptr_t>(base) - offset);
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

Browser::BrowserViewDelegate::BrowserViewDelegate(Browser::Details details): details(details) {
	this->cef_delegate.base.base.size = sizeof(cef_browser_view_delegate_t);
	this->cef_delegate.base.base.add_ref = ::BrowserViewDelegate::add_ref;
	this->cef_delegate.base.base.release = ::BrowserViewDelegate::release;
	this->cef_delegate.base.base.has_one_ref = ::BrowserViewDelegate::has_one_ref;
	this->cef_delegate.base.base.has_at_least_one_ref = ::BrowserViewDelegate::has_any_refs;
	this->cef_delegate.base.get_preferred_size = ::get_preferred_size;
	this->cef_delegate.base.get_minimum_size = ::get_minimum_size;
	this->cef_delegate.base.get_maximum_size = ::get_maximum_size;
	this->cef_delegate.base.get_height_for_width = ::get_height_for_width;
	this->cef_delegate.base.on_parent_view_changed = ::on_parent_view_changed;
	this->cef_delegate.base.on_child_view_changed = ::on_child_view_changed;
	this->cef_delegate.base.on_window_changed = ::on_window_changed;
	this->cef_delegate.base.on_layout_changed = ::on_layout_changed;
	this->cef_delegate.base.on_focus = ::on_focus;
	this->cef_delegate.base.on_blur = ::on_blur;
	this->cef_delegate.on_browser_created = ::on_browser_created;
	this->cef_delegate.on_browser_destroyed = ::on_browser_destroyed;
	this->cef_delegate.get_delegate_for_popup_browser_view = ::get_delegate_for_popup_browser_view;
	this->cef_delegate.on_popup_browser_view_created = ::on_popup_browser_view_created;
	this->cef_delegate.get_chrome_toolbar_type = ::get_chrome_toolbar_type;
	this->refcount = 1;
}

void Browser::BrowserViewDelegate::add_ref() {
	this->refcount += 1;
}

int Browser::BrowserViewDelegate::release() {
	// Since this->refcount is atomic, the operation of decrementing it and checking if the result
	// is 0 must only access the value once. Accessing it twice would cause a race condition.
	if (--this->refcount == 0) {
		this->destroy();
		return 1;
	}
	return 0;
}

void Browser::BrowserViewDelegate::destroy() {
	// Any self-cleanup should be done here
}

cef_browser_view_delegate_t* Browser::BrowserViewDelegate::delegate() {
	this->add_ref();
	return &this->cef_delegate;
}

cef_size_t CEF_CALLBACK get_preferred_size(cef_view_delegate_t* delegate, cef_view_t* view) {
	view->base.release(&view->base);
	Browser::BrowserViewDelegate* self = BrowserViewDelegate::resolve_view(delegate);
	return cef_size_t { .width = self->details.preferred_width, .height = self->details.preferred_height };
}

cef_size_t CEF_CALLBACK get_minimum_size(cef_view_delegate_t* delegate, cef_view_t* view) {
	view->base.release(&view->base);
	Browser::BrowserViewDelegate* self = BrowserViewDelegate::resolve_view(delegate);
	return cef_size_t { .width = self->details.min_width, .height = self->details.min_height };
}

cef_size_t CEF_CALLBACK get_maximum_size(cef_view_delegate_t* delegate, cef_view_t* view) {
	view->base.release(&view->base);
	Browser::BrowserViewDelegate* self = BrowserViewDelegate::resolve_view(delegate);
	return cef_size_t { .width = self->details.max_width, .height = self->details.max_height };
}

int CEF_CALLBACK get_height_for_width(cef_view_delegate_t* delegate, cef_view_t* view, int) {
	view->base.release(&view->base);
	return BrowserViewDelegate::resolve_view(delegate)->details.preferred_height;
}

void CEF_CALLBACK on_parent_view_changed(cef_view_delegate_t*, cef_view_t* view, int, cef_view_t* parent_view) {
	view->base.release(&view->base);
	parent_view->base.release(&parent_view->base);
}

void CEF_CALLBACK on_child_view_changed(cef_view_delegate_t*, cef_view_t* view, int, cef_view_t* child_view) {
	view->base.release(&view->base);
	child_view->base.release(&child_view->base);
}

void CEF_CALLBACK on_window_changed(cef_view_delegate_t*, cef_view_t* view, int) {
	view->base.release(&view->base);
}

void CEF_CALLBACK on_layout_changed(cef_view_delegate_t*, cef_view_t* view, const cef_rect_t*) {
	view->base.release(&view->base);
}

void CEF_CALLBACK on_focus(cef_view_delegate_t*, cef_view_t* view) {
	view->base.release(&view->base);
}

void CEF_CALLBACK on_blur(cef_view_delegate_t*, cef_view_t* view) {
	view->base.release(&view->base);
}

void CEF_CALLBACK on_browser_created(cef_browser_view_delegate_t*, cef_browser_view_t* browser_view, cef_browser_t* browser) {
	browser_view->base.base.release(&browser_view->base.base);
	browser->base.release(&browser->base);
}

void CEF_CALLBACK on_browser_destroyed(cef_browser_view_delegate_t*, cef_browser_view_t* browser_view, cef_browser_t* browser) {
	browser_view->base.base.release(&browser_view->base.base);
	browser->base.release(&browser->base);
}

cef_browser_view_delegate_t* CEF_CALLBACK get_delegate_for_popup_browser_view(
	cef_browser_view_delegate_t* self,
	cef_browser_view_t* browser_view,
	const cef_browser_settings_t* browser_settings,
	cef_client_t* client,
	int is_devtools
) {
	browser_view->base.base.release(&browser_view->base.base);
	client->base.release(&client->base);
	return self;
}

int CEF_CALLBACK on_popup_browser_view_created(cef_browser_view_delegate_t* self, cef_browser_view_t* browser_view, cef_browser_view_t* popup_browser_view, int is_devtools) {
	browser_view->base.base.release(&browser_view->base.base);
	popup_browser_view->base.base.release(&popup_browser_view->base.base);
	return false;
}

cef_chrome_toolbar_type_t CEF_CALLBACK get_chrome_toolbar_type(cef_browser_view_delegate_t*) {
	return CEF_CTT_NONE;
}
