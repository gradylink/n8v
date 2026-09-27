// Package n8v binds the n8v C API.
//
// Build n8v with CMake first. The package links -ln8v from ../../build, or from wherever
// CGO_LDFLAGS points when used from the module cache.
//
//	n8v.Run(800, 600, "hello", func() {
//		n8v.Flex(func() {
//			n8v.Text("Hello", n8v.Bold())
//			n8v.Button("Click me", func() { clicks++ })
//		}, n8v.Vertical(), n8v.Gap(12), n8v.Padding(20, 20, 20, 20))
//	})
package n8v

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#cgo LDFLAGS: -L${SRCDIR}/../../build -Wl,-rpath,${SRCDIR}/../../build -ln8v
#include <stdlib.h>
#include <n8v/n8v_c.h>

extern void n8vGoClick(void *userdata);

static void *n8v_go_handle(uintptr_t h) { return (void *)h; }
static void n8v_go_button(n8v_button_options o, const char *label) { _n8v_set_button_opts(o); _n8v_button_commit(label); }
static void n8v_go_text(n8v_text_options o, const char *label) { _n8v_set_text_opts(o); _n8v_text_commit(label); }
static void n8v_go_checkbox(n8v_checkbox_options o, const char *label) { _n8v_set_checkbox_opts(o); _n8v_checkbox_commit(label); }
static void n8v_go_toggle(n8v_toggle_options o, const char *label) { _n8v_set_toggle_opts(o); _n8v_toggle_commit(label); }
static void n8v_go_radio(n8v_radio_options o, const char *label) { _n8v_set_radio_opts(o); _n8v_radio_commit(label); }
*/
import "C"

import (
	"runtime"
	"runtime/cgo"
	"unsafe"
)

func init() { runtime.LockOSThread() }

type Style int

const (
	Plain Style = iota
	Material
	Cupertino
	Fluent
)

type Align int

const (
	Start Align = iota
	Center
	End
)

type Sizing struct{ s C.n8v_sizing }

func Fit(min, max float64) Sizing  { return Sizing{C.n8v_sizing_fit(C.float(min), C.float(max))} }
func Grow(min, max float64) Sizing { return Sizing{C.n8v_sizing_grow(C.float(min), C.float(max))} }
func Fixed(px float64) Sizing      { return Sizing{C.n8v_sizing_fixed(C.float(px))} }
func Percent(f float64) Sizing     { return Sizing{C.n8v_sizing_percent(C.float(f))} }

// Option configures a widget. Widgets ignore options that do not apply to them.
type Option func(*config)

type config struct {
	vertical, listItem     bool
	gap                    int
	padding                C.n8v_padding
	hAlign, vAlign         Align
	width, height          Sizing
	clipH, clipV           bool
	bold, italic, strike   bool
	url, icon, placeholder string
	color                  C.n8v_color
	buttonStyle            C.n8v_button_style
	filled, trailing       bool
	password               bool
	onSubmit               func()
	rounding               C.n8v_rounding
}

func Vertical() Option             { return func(c *config) { c.vertical = true } }
func ListItem() Option             { return func(c *config) { c.listItem = true } }
func Gap(px int) Option            { return func(c *config) { c.gap = px } }
func Width(s Sizing) Option        { return func(c *config) { c.width = s } }
func Height(s Sizing) Option       { return func(c *config) { c.height = s } }
func AlignItems(h, v Align) Option { return func(c *config) { c.hAlign, c.vAlign = h, v } }
func Clip(horizontal, vertical bool) Option {
	return func(c *config) { c.clipH, c.clipV = horizontal, vertical }
}
func Bold() Option                   { return func(c *config) { c.bold = true } }
func Italic() Option                 { return func(c *config) { c.italic = true } }
func Strikethrough() Option          { return func(c *config) { c.strike = true } }
func Link(url string) Option         { return func(c *config) { c.url = url } }
func Icon(name string) Option        { return func(c *config) { c.icon = name } }
func FilledIcon() Option             { return func(c *config) { c.filled = true } }
func TrailingIcon() Option           { return func(c *config) { c.trailing = true } }
func Secondary() Option              { return func(c *config) { c.buttonStyle = C.N8V_BUTTON_STYLE_SECONDARY } }
func Ghost() Option                  { return func(c *config) { c.buttonStyle = C.N8V_BUTTON_STYLE_GHOST } }
func Placeholder(text string) Option { return func(c *config) { c.placeholder = text } }
func Password(on bool) Option        { return func(c *config) { c.password = on } }
func OnSubmit(fn func()) Option      { return func(c *config) { c.onSubmit = fn } }
func NoRounding() Option             { return func(c *config) { c.rounding = C.n8v_rounding_none() } }

func Padding(left, right, top, bottom int) Option {
	return func(c *config) {
		c.padding = C.n8v_padding{C.uint16_t(left), C.uint16_t(right), C.uint16_t(top), C.uint16_t(bottom)}
	}
}

// Color sets text colour or icon tint. Channels range from 0 to 255.
func Color(r, g, b, a float64) Option {
	return func(c *config) { c.color = C.n8v_color{C.float(r), C.float(g), C.float(b), C.float(a)} }
}

func Rounded(topLeft, topRight, bottomLeft, bottomRight float64) Option {
	return func(c *config) {
		c.rounding = C.n8v_rounding_fixed(C.float(topLeft), C.float(topRight), C.float(bottomLeft), C.float(bottomRight))
	}
}

func apply(opts []Option) config {
	c := config{rounding: C.n8v_rounding_style_default()}
	for _, o := range opts {
		o(&c)
	}
	return c
}

func (c config) direction() C.n8v_direction {
	if c.vertical {
		return C.N8V_DIRECTION_VERTICAL
	}
	return C.N8V_DIRECTION_HORIZONTAL
}

// C keeps strings, pixel data and callback handles until the next frame's events are pumped,
// so each frame owns them and frees the previous frame's set when it begins.
var (
	frameAllocs  []unsafe.Pointer
	frameHandles []cgo.Handle
)

func cstr(s string) *C.char {
	if s == "" {
		return nil
	}
	p := C.CString(s)
	frameAllocs = append(frameAllocs, unsafe.Pointer(p))
	return p
}

func callback(fn func()) (C.n8v_click_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_click_fn(C.n8vGoClick), C.n8v_go_handle(C.uintptr_t(h))
}

func Init(width, height int, title string) bool {
	t := C.CString(title)
	defer C.free(unsafe.Pointer(t))
	return bool(C.n8v_initialize(C.int(width), C.int(height), t))
}

func Pump() bool { return bool(C.n8v_pump_events()) }
func Shutdown()  { C.n8v_shutdown() }

func SetStyle(s Style)   { C.n8v_set_style_family(C.n8v_style_family(s)) }
func ActiveStyle() Style { return Style(C.n8v_active_style_family()) }

// Frame builds one frame of UI.
func Frame(body func()) {
	for _, p := range frameAllocs {
		C.free(p)
	}
	for _, h := range frameHandles {
		h.Delete()
	}
	frameAllocs, frameHandles = frameAllocs[:0], frameHandles[:0]
	C.n8v_begin_frame()
	body()
	C.n8v_end_frame()
}

// Run opens a window and builds a frame until it closes.
func Run(width, height int, title string, body func()) bool {
	if !Init(width, height, title) {
		return false
	}
	for Pump() {
		Frame(body)
	}
	Shutdown()
	return true
}

func Flex(body func(), opts ...Option) {
	c := apply(opts)
	C.n8v_open_flex(C.n8v_flex_options{
		direction: c.direction(), gap: C.uint16_t(c.gap), padding: c.padding,
		h_align: C.n8v_align(c.hAlign), v_align: C.n8v_align(c.vAlign),
		width: c.width.s, height: c.height.s,
		clip_horizontal: C.bool(c.clipH), clip_vertical: C.bool(c.clipV),
	})
	body()
	C.n8v_close_flex()
}

func Panel(body func(), opts ...Option) {
	c := apply(opts)
	role := C.n8v_panel_role(C.N8V_PANEL_ROLE_CARD)
	if c.listItem {
		role = C.N8V_PANEL_ROLE_LIST_ITEM
	}
	C.n8v_open_panel(C.n8v_panel_options{
		role: role, direction: c.direction(), gap: C.uint16_t(c.gap),
		h_align: C.n8v_align(c.hAlign), v_align: C.n8v_align(c.vAlign),
		width: c.width.s, height: c.height.s,
		clip_horizontal: C.bool(c.clipH), clip_vertical: C.bool(c.clipV),
	})
	body()
	C.n8v_close_panel()
}

// Sidebar holds Page calls. selected tracks the open page.
func Sidebar(title string, selected *Int, body func(), opts ...Option) {
	c := apply(opts)
	C.n8v_open_sidebar(C.n8v_sidebar_options{title: cstr(title), selected: selected.p, width: c.width.s})
	body()
	C.n8v_close_sidebar()
}

// Page adds a sidebar entry and runs body when it is the selected page.
func Page(name string, body func(), opts ...Option) {
	c := apply(opts)
	if C.n8v_open_page(C.n8v_page_options{name: cstr(name), icon: cstr(c.icon)}) {
		body()
		C.n8v_close_page()
	}
}

func Text(label string, opts ...Option) {
	c := apply(opts)
	C.n8v_go_text(C.n8v_text_options{
		bold: C.bool(c.bold), italic: C.bool(c.italic), strikethrough: C.bool(c.strike),
		url: cstr(c.url), color: c.color,
	}, cstr(label))
}

func (c config) iconVariant() C.n8v_icon_variant {
	if c.filled {
		return C.N8V_ICON_VARIANT_FILLED
	}
	return C.N8V_ICON_VARIANT_OUTLINE
}

func Button(label string, onClick func(), opts ...Option) {
	c := apply(opts)
	fn, ud := callback(onClick)
	pos := C.n8v_icon_position(C.N8V_ICON_POSITION_LEADING)
	if c.trailing {
		pos = C.N8V_ICON_POSITION_TRAILING
	}
	C.n8v_go_button(C.n8v_button_options{
		style: c.buttonStyle, on_click: fn, on_click_userdata: ud,
		icon: cstr(c.icon), icon_variant: c.iconVariant(), icon_position: pos,
	}, cstr(label))
}

func Checkbox(label string, checked *Bool) {
	C.n8v_go_checkbox(C.n8v_checkbox_options{checked: checked.p}, cstr(label))
}

func Toggle(label string, checked *Bool) {
	C.n8v_go_toggle(C.n8v_toggle_options{checked: checked.p}, cstr(label))
}

// Radio selects value in selected when clicked.
func Radio(label string, selected *Int, value int) {
	C.n8v_go_radio(C.n8v_radio_options{selected: selected.p, value: C.int(value)}, cstr(label))
}

func Entry(value *String, opts ...Option) {
	c := apply(opts)
	fn, ud := callback(c.onSubmit)
	C.n8v_entry(C.n8v_entry_options{
		value: value.p, placeholder: cstr(c.placeholder), password: C.bool(c.password),
		on_submit: fn, on_submit_userdata: ud,
	})
}

// Dropdown shows items and stores the chosen index in selected. -1 shows the placeholder.
func Dropdown(items []string, selected *Int, opts ...Option) {
	c := apply(opts)
	var list **C.char
	if len(items) > 0 {
		list = (**C.char)(C.malloc(C.size_t(len(items)) * C.size_t(unsafe.Sizeof(uintptr(0)))))
		frameAllocs = append(frameAllocs, unsafe.Pointer(list))
		for i, item := range items {
			unsafe.Slice(list, len(items))[i] = cstr(item)
		}
	}
	C.n8v_dropdown(C.n8v_dropdown_options{
		items: list, item_count: C.size_t(len(items)), selected: selected.p, placeholder: cstr(c.placeholder),
	})
}

func Slider(value *Float, min, max float64) {
	C.n8v_slider(C.n8v_slider_options{value: value.p, min: C.float(min), max: C.float(max)})
}

// Image draws a PNG, JPEG or SVG file.
func Image(path string, opts ...Option) {
	c := apply(opts)
	C.n8v_image(C.n8v_image_options{
		source_kind: C.N8V_IMAGE_SOURCE_PATH, path: cstr(path),
		width: c.width.s, height: c.height.s, rounding: c.rounding,
	})
}

// ImageBytes draws encoded image data.
func ImageBytes(data []byte, opts ...Option) {
	c := apply(opts)
	var p unsafe.Pointer
	if len(data) > 0 {
		p = C.CBytes(data)
		frameAllocs = append(frameAllocs, p)
	}
	C.n8v_image(C.n8v_image_options{
		source_kind: C.N8V_IMAGE_SOURCE_ENCODED, encoded_data: (*C.uint8_t)(p), encoded_size: C.size_t(len(data)),
		width: c.width.s, height: c.height.s, rounding: c.rounding,
	})
}

// IconImage draws a bundled icon such as "settings", "star" or "heart".
func IconImage(name string, opts ...Option) {
	c := apply(opts)
	C.n8v_icon(C.n8v_icon_options{
		name: cstr(name), variant: c.iconVariant(), width: c.width.s, height: c.height.s, tint: c.color,
	})
}
