package n8v

/*
#include <stdlib.h>
#include <n8v/n8v_c.h>

extern bool n8vGoURLHandler(char *url, void *userdata);
extern void n8vGoVisibleChild(char *id, float top, float height, void *userdata);

static bool n8v_go_cb_url(const char *url, void *userdata) { return n8vGoURLHandler((char *)url, userdata); }
static void n8v_go_cb_visible(const char *id, float top, float height, void *userdata) { n8vGoVisibleChild((char *)id, top, height, userdata); }
static void *n8v_go_window_handle(uintptr_t h) { return (void *)h; }
*/
import "C"

import "runtime/cgo"

// Focus gives keyboard focus to the widget with this ID.
func Focus(id string) {
	withCStr(id, func(p *C.char) { C.n8v_focus(p) })
}

// ScrollState is where a scrolling container is scrolled to and how big its view is, all in pixels.
type ScrollState struct{ OffsetX, OffsetY, ViewWidth, ViewHeight float64 }

// ScrollStateOf reports a scrolling container's state; ok is false if there is no such container or the backend cannot say.
func ScrollStateOf(containerID string) (state ScrollState, ok bool) {
	withCStr(containerID, func(p *C.char) {
		var x, y, w, h C.float
		if ok = bool(C.n8v_scroll_state(p, &x, &y, &w, &h)); ok {
			state = ScrollState{float64(x), float64(y), float64(w), float64(h)}
		}
	})
	return
}

// VisibleChild is a child of a scrolling container that is in view.
type VisibleChild struct {
	ID          string
	Top, Height float64
}

// VisibleChildren lists the ID'd children of a scrolling container that are in view or within margin pixels of it. ok is false if
// there is no such container or the backend cannot say.
func VisibleChildren(containerID string, margin float64) (children []VisibleChild, ok bool) {
	h := cgo.NewHandle(func(id string, top, height float64) {
		children = append(children, VisibleChild{id, top, height})
	})
	defer h.Delete()
	withCStr(containerID, func(p *C.char) {
		ok = bool(C.n8v_visible_children(p, C.float(margin), C.n8v_visible_child_fn(C.n8v_go_cb_visible), C.n8v_go_window_handle(C.uintptr_t(h))))
	})
	return
}

// OpenURL opens a link in the default application (the browser for a web address).
func OpenURL(url string) {
	withCStr(url, func(p *C.char) { C.n8v_open_url(p) })
}

var urlHandler cgo.Handle

// SetURLHandler sees every link before it is opened: return true to say it was handled, false to let n8v open it. nil removes it.
func SetURLHandler(fn func(url string) bool) {
	if urlHandler != 0 {
		C.n8v_set_url_handler(nil, nil)
		urlHandler.Delete()
		urlHandler = 0
	}
	if fn == nil {
		return
	}
	urlHandler = cgo.NewHandle(fn)
	C.n8v_set_url_handler(C.n8v_url_handler_fn(C.n8v_go_cb_url), C.n8v_go_window_handle(C.uintptr_t(urlHandler)))
}

// Wake makes Pump return promptly from another goroutine, e.g. when work finishes.
func Wake() { C.n8v_wake() }

// RaiseWindow brings the window to the front. Call it from the UI goroutine.
func RaiseWindow() { C.n8v_raise_window() }

// IsDarkTheme reports whether the UI currently has a dark appearance.
func IsDarkTheme() bool { return bool(C.n8v_is_dark_theme()) }

// WindowSize is the window's size in logical pixels; ok is false if the backend cannot say.
func WindowSize() (width, height float64, ok bool) {
	var w, h C.float
	if !C.n8v_window_size(&w, &h) {
		return 0, 0, false
	}
	return float64(w), float64(h), true
}
