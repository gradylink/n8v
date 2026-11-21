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
extern void n8vGoBool(bool value, void *userdata);
extern void n8vGoInt(int value, void *userdata);
extern void n8vGoFileDialog(char **paths, size_t count, void *userdata);
extern void n8vGoMessageDialog(int button, void *userdata);
extern void n8vGoFloat(float value, void *userdata);
extern void n8vGoText(char *text, size_t length, void *userdata);
extern void n8vGoPoint(float x, float y, void *userdata);
extern void n8vGoZoom(float factor, float x, float y, void *userdata);
extern bool n8vGoKey(int key, unsigned mods, void *userdata);
extern void n8vGoPasteFiles(char **paths, size_t count, void *userdata);
extern bool n8vGoLinkStyle(char *url, n8v_link_style *out, void *userdata);
extern char *n8vGoImageResolve(char *src, void *userdata);

static void n8v_go_cb_text(const char *text, size_t length, void *userdata) { n8vGoText((char *)text, length, userdata); }
static bool n8v_go_cb_key(n8v_key key, unsigned mods, void *userdata) { return n8vGoKey((int)key, mods, userdata); }
static void n8v_go_cb_paste_files(const char *const *paths, size_t count, void *userdata) { n8vGoPasteFiles((char **)paths, count, userdata); }
static bool n8v_go_cb_link_style(const char *url, n8v_link_style *out, void *userdata) { return n8vGoLinkStyle((char *)url, out, userdata); }
static const char *n8v_go_cb_image_resolve(const char *src, void *userdata) { return n8vGoImageResolve((char *)src, userdata); }

static void n8v_go_file_dialog_result(const char *const *paths, size_t count, void *userdata) { n8vGoFileDialog((char **)paths, count, userdata); }
static void n8v_go_show_file_dialog(n8v_file_dialog_options o, uintptr_t h) {
  o.on_result = n8v_go_file_dialog_result;
  o.on_result_userdata = (void *)h;
  n8v_show_file_dialog(o);
}
static void n8v_go_message_dialog_result(int button, void *userdata) { n8vGoMessageDialog(button, userdata); }
static void n8v_go_show_message_dialog(n8v_message_dialog_options o, uintptr_t h) {
  o.on_result = n8v_go_message_dialog_result;
  o.on_result_userdata = (void *)h;
  n8v_show_message_dialog(o);
}
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
	id, image              string
	notify                 bool
	badge                  string
	badgeColor             C.n8v_badge_color
	stickToBottom          bool
	onHover                func(bool)
	onChange               func(int)
	multiline              bool
	submitCtrlEnter        bool
	minLines, maxLines     int
	matchEntryStyle        bool
	minWidth, maxWidth     float64
	compact                bool
	onClick                func()
	attachElement          C.n8v_attach_point
	attachParent           C.n8v_attach_point
	offsetX, offsetY       float64

	small, darken, large, clickUnstyled bool
	accent                              C.n8v_color
	hasPadding                          bool
	buttonImage                         string
	buttonImageHeight                   int
	onToggle                            func(bool)
	onSlide                             func(float64)
	onInput                             func(string)
	onKey                               func(Key, Mods) bool
	onPasteFiles                        func([]string)
	onClickAt                           func(x, y float64)
	onZoom                              func(factor, x, y float64)
	onDrag                              func(dx, dy float64)
	autoplay, loop, muted, controls     bool
	collapsed                           *Bool
	pageImageSize                       float64
	linkStyle                           func(url string) (LinkStyle, bool)
	imageResolver                       func(src string) string
	imageState                          string
	opaque, keepOnScreen                bool
	screenMargin                        float64
	onDismiss                           func()
	popoverSide                         PopoverSide
	alignEnd                            bool
	popoverGap                          float64
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
func Bold() Option            { return func(c *config) { c.bold = true } }
func Italic() Option          { return func(c *config) { c.italic = true } }
func Strikethrough() Option   { return func(c *config) { c.strike = true } }
func Link(url string) Option  { return func(c *config) { c.url = url } }
func Icon(name string) Option { return func(c *config) { c.icon = name } }

// BadgeColor is a named color resolved against the toolkit's palette where it has one.
type BadgeColor int

const (
	BadgeDefault BadgeColor = iota
	BadgeRed
	BadgeOrange
	BadgeYellow
	BadgeGreen
	BadgeBlue
	BadgePurple
	BadgePink
)

// Notify draws attention to a Page: a marker on the left of its row and an emphasised name.
func Notify() Option { return func(c *config) { c.notify = true } }

// Badge shows short text (e.g. a count) in a colored pill on the right of a Page's row. Empty text shows nothing.
func Badge(text string, color BadgeColor) Option {
	return func(c *config) { c.badge, c.badgeColor = text, C.n8v_badge_color(color) }
}
func FilledIcon() Option             { return func(c *config) { c.filled = true } }
func TrailingIcon() Option           { return func(c *config) { c.trailing = true } }
func Primary() Option                { return func(c *config) { c.buttonStyle = C.N8V_BUTTON_STYLE_PRIMARY } }
func Secondary() Option              { return func(c *config) { c.buttonStyle = C.N8V_BUTTON_STYLE_SECONDARY } }
func Ghost() Option                  { return func(c *config) { c.buttonStyle = C.N8V_BUTTON_STYLE_GHOST } }
func Placeholder(text string) Option { return func(c *config) { c.placeholder = text } }
func Password(on bool) Option        { return func(c *config) { c.password = on } }
func OnSubmit(fn func()) Option      { return func(c *config) { c.onSubmit = fn } }

// ID names a widget so scroll helpers, IsHovered and OnHover can refer to it.
func ID(id string) Option { return func(c *config) { c.id = id } }

// StickToBottom keeps a scrolling Flex pinned to its end as content grows.
func StickToBottom() Option { return func(c *config) { c.stickToBottom = true } }

// OnHover fires on both hover enter and leave. Requires ID.
func OnHover(fn func(bool)) Option { return func(c *config) { c.onHover = fn } }

// Multiline makes an Entry a soft-wrapping text area (Password is ignored). Milsko has no multi-line widget, so there it stays single-line.
func Multiline() Option { return func(c *config) { c.multiline = true } }

// SubmitOnCtrlEnter makes a multiline Entry submit on Ctrl+Enter (Cmd+Enter on macOS) and insert a newline on Enter. Without it, Enter
// submits and Shift+Enter inserts a newline. Single-line entries always submit on Enter.
func SubmitOnCtrlEnter() Option { return func(c *config) { c.submitCtrlEnter = true } }

// MatchEntryStyle draws a multiline Entry like a single-line one (same fill and shape in the toolkit's theme) instead of the toolkit's plain
// text area. Only GTK4 draws the two differently; elsewhere this does nothing.
func MatchEntryStyle() Option { return func(c *config) { c.matchEntryStyle = true } }

// Lines sizes a multiline Entry: it shows min lines, and with max > min it grows with its text up to max lines, then scrolls. With
// max <= min the height is fixed at min lines and the text scrolls. min <= 0 means 3.
func Lines(min, max int) Option { return func(c *config) { c.minLines, c.maxLines = min, max } }

// OnChange is called with the new selection when a Sidebar's selected page changes.
func OnChange(fn func(int)) Option { return func(c *config) { c.onChange = fn } }

// MinWidth and MaxWidth bound a Sidebar's width in pixels (0 means unbounded).
func MinWidth(px float64) Option { return func(c *config) { c.minWidth = px } }
func MaxWidth(px float64) Option { return func(c *config) { c.maxWidth = px } }

// Compact makes a Sidebar use its condensed layout.
func Compact() Option { return func(c *config) { c.compact = true } }

// OnClick paints Text as a link and calls fn on click instead of opening the Link URL.
func OnClick(fn func()) Option { return func(c *config) { c.onClick = fn } }

// PageImage sets the image shown for a Page (mutually exclusive with Icon).
func PageImage(path string) Option { return func(c *config) { c.image = path } }

// AttachPoint is where a Floating element or its parent is anchored.
type AttachPoint int

const (
	LeftTop AttachPoint = iota
	LeftCenter
	LeftBottom
	CenterTop
	CenterCenter
	CenterBottom
	RightTop
	RightCenter
	RightBottom
)

// Attach sets which point of the floating element sits on which point of its parent.
func Attach(element, parent AttachPoint) Option {
	return func(c *config) {
		c.attachElement, c.attachParent = C.n8v_attach_point(element), C.n8v_attach_point(parent)
	}
}

// Offset shifts a Floating element in pixels after attaching.
func Offset(x, y float64) Option { return func(c *config) { c.offsetX, c.offsetY = x, y } }

func NoRounding() Option { return func(c *config) { c.rounding = C.n8v_rounding_none() } }

func Padding(left, right, top, bottom int) Option {
	return func(c *config) {
		c.padding = C.n8v_padding{C.uint16_t(left), C.uint16_t(right), C.uint16_t(top), C.uint16_t(bottom)}
		c.hasPadding = true
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

// Small and Darken make Text secondary: smaller and fainter, by whatever the style considers so. Large is jumbo text for things like a
// message that is only emoji.
func Small() Option  { return func(c *config) { c.small = true } }
func Darken() Option { return func(c *config) { c.darken = true } }
func Large() Option  { return func(c *config) { c.large = true } }

// ClickUnstyled, with OnClick, keeps Text clickable but not painted as a link.
func ClickUnstyled() Option { return func(c *config) { c.clickUnstyled = true } }

// Accent draws a colored bar down a Panel's left edge. Alpha 0 means none. Channels range from 0 to 255.
func Accent(r, g, b, a float64) Option {
	return func(c *config) { c.accent = C.n8v_color{C.float(r), C.float(g), C.float(b), C.float(a)} }
}

// ThemeColor is a color that follows the active theme where the toolkit has a palette. It sets the same color Color does.
func ThemeColor(name BadgeColor) Option { return func(c *config) { c.color = themeColor(name) } }

// AccentTheme is Accent with a theme color.
func AccentTheme(name BadgeColor) Option { return func(c *config) { c.accent = themeColor(name) } }

func themeColor(name BadgeColor) C.n8v_color { return C.n8v_color_theme(C.n8v_named_color(name)) }

// ButtonImage shows an image in a Button instead of an icon, height pixels tall (0 for the usual size).
func ButtonImage(path string, height int) Option {
	return func(c *config) { c.buttonImage, c.buttonImageHeight = path, height }
}

// OnToggle is called with the new value when a Checkbox, Toggle or ToggleButton flips, or when a Sidebar is collapsed or expanded.
func OnToggle(fn func(bool)) Option { return func(c *config) { c.onToggle = fn } }

// OnSlide is called with the new value when a Slider moves.
func OnSlide(fn func(float64)) Option { return func(c *config) { c.onSlide = fn } }

// OnInput is called with the new text when an Entry's value changes.
func OnInput(fn func(string)) Option { return func(c *config) { c.onInput = fn } }

// Key is a key an Entry can offer to OnKey.
type Key int

const (
	KeyUp Key = iota
	KeyDown
	KeyEscape
)

// Mods is a combination of ModShift, ModCtrl and ModAlt.
type Mods uint

const (
	ModShift Mods = 1
	ModCtrl  Mods = 2
	ModAlt   Mods = 4
)

// OnKey lets an Entry's owner handle some keys first: return true to stop the entry acting on the key. Multiline entries only, so far,
// and not in every backend.
func OnKey(fn func(key Key, mods Mods) bool) Option { return func(c *config) { c.onKey = fn } }

// OnPasteFiles is called with the paths when files are pasted into an Entry. Multiline entries only, so far, and not in every backend.
func OnPasteFiles(fn func(paths []string)) Option { return func(c *config) { c.onPasteFiles = fn } }

// OnClickAt makes an Image clickable and tells where in it (each axis from 0 to 1). It wins over OnClick where the toolkit can say where.
func OnClickAt(fn func(x, y float64)) Option { return func(c *config) { c.onClickAt = fn } }

// OnZoom handles pinch and Ctrl+wheel zooming over an Image: factor is the change since the last call (above 1 zooms in) around the
// point (x, y) of the image, each axis from 0 to 1.
func OnZoom(fn func(factor, x, y float64)) Option { return func(c *config) { c.onZoom = fn } }

// OnDrag handles dragging over an Image with the change in position since the last call in pixels, for panning.
func OnDrag(fn func(dx, dy float64)) Option { return func(c *config) { c.onDrag = fn } }

// Autoplay, Loop, Muted and Controls configure Video and Audio.
func Autoplay() Option { return func(c *config) { c.autoplay = true } }
func Loop() Option     { return func(c *config) { c.loop = true } }
func Muted() Option    { return func(c *config) { c.muted = true } }

// Controls shows play/pause and a seek bar on a Video.
func Controls() Option { return func(c *config) { c.controls = true } }

// Collapsible lets the user fold a Sidebar to a narrow strip of icons. The initial state is read from collapsed and every toggle is
// written back to it, so the caller can persist it. Use OnToggle to be told when it changes.
func Collapsible(collapsed *Bool) Option { return func(c *config) { c.collapsed = collapsed } }

// PageImageSize sets the size of a Page's image in pixels; 0 for the usual size.
func PageImageSize(px float64) Option { return func(c *config) { c.pageImageSize = px } }

// LinkStyle is how a Document draws one link.
type LinkStyle struct {
	Foreground, Background Rgba
	Bold                   bool
	// Clickable keeps the link clickable (a styled link is otherwise plain text).
	Clickable bool
}

// Rgba is a color with channels from 0 to 255 (alpha too).
type Rgba struct{ R, G, B, A float64 }

// DocumentLinkStyle lets a Document restyle chosen links: return false to leave a link alone.
func DocumentLinkStyle(fn func(url string) (LinkStyle, bool)) Option {
	return func(c *config) { c.linkStyle = fn }
}

// ImageResolver maps the src of each image in a Document to something n8v can load (a file path, say). Return "" for none.
func ImageResolver(fn func(src string) string) Option {
	return func(c *config) { c.imageResolver = fn }
}

// ImageState names where a Document keeps what it has loaded for its images.
func ImageState(name string) Option { return func(c *config) { c.imageState = name } }

// Opaque makes a Floating element swallow the pointer instead of letting it through to what is under it.
func Opaque() Option { return func(c *config) { c.opaque = true } }

// OnDismiss is called when the user clicks outside a Floating element or Popover.
func OnDismiss(fn func()) Option { return func(c *config) { c.onDismiss = fn } }

// KeepOnScreen nudges a Floating element back inside the window, margin pixels from its edges. It also sets a Popover's margin.
func KeepOnScreen(margin float64) Option {
	return func(c *config) { c.keepOnScreen, c.screenMargin = true, margin }
}

// PopoverSide is which side of its parent a Popover opens on.
type PopoverSide int

const (
	Below PopoverSide = iota
	Above
	ToRight
	ToLeft
)

// Side picks which side of its parent a Popover opens on (Below by default).
func Side(s PopoverSide) Option { return func(c *config) { c.popoverSide = s } }

// AlignEnd lines a Popover's far edge, not its near one, up with its parent's.
func AlignEnd() Option { return func(c *config) { c.alignEnd = true } }

// PopoverGap is the space in pixels between a Popover and its parent.
func PopoverGap(px float64) Option { return func(c *config) { c.popoverGap = px } }

func apply(opts []Option) config {
	c := config{
		rounding:      C.n8v_rounding_style_default(),
		attachElement: C.N8V_ATTACH_POINT_RIGHT_TOP, attachParent: C.N8V_ATTACH_POINT_RIGHT_TOP,
	}
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

func boolCallback(fn func(bool)) (C.n8v_bool_change_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_bool_change_fn(C.n8vGoBool), C.n8v_go_handle(C.uintptr_t(h))
}

func intCallback(fn func(int)) (C.n8v_int_change_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_int_change_fn(C.n8vGoInt), C.n8v_go_handle(C.uintptr_t(h))
}

func callback(fn func()) (C.n8v_click_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_click_fn(C.n8vGoClick), C.n8v_go_handle(C.uintptr_t(h))
}

func floatCallback(fn func(float64)) (C.n8v_float_change_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_float_change_fn(C.n8vGoFloat), C.n8v_go_handle(C.uintptr_t(h))
}

func textCallback(fn func(string)) (C.n8v_text_change_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_text_change_fn(C.n8v_go_cb_text), C.n8v_go_handle(C.uintptr_t(h))
}

func pointCallback(fn func(x, y float64)) (C.n8v_point_fn, unsafe.Pointer) {
	if fn == nil {
		return nil, nil
	}
	h := cgo.NewHandle(fn)
	frameHandles = append(frameHandles, h)
	return C.n8v_point_fn(C.n8vGoPoint), C.n8v_go_handle(C.uintptr_t(h))
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
	hover, hoverData := boolCallback(c.onHover)
	C.n8v_open_flex(C.n8v_flex_options{
		direction: c.direction(), gap: C.uint16_t(c.gap), padding: c.padding,
		h_align: C.n8v_align(c.hAlign), v_align: C.n8v_align(c.vAlign),
		width: c.width.s, height: c.height.s,
		clip_horizontal: C.bool(c.clipH), clip_vertical: C.bool(c.clipV),
		id: cstr(c.id), stick_to_bottom: C.bool(c.stickToBottom),
		on_hover: hover, on_hover_userdata: hoverData,
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
		id: cstr(c.id), accent: c.accent, has_padding: C.bool(c.hasPadding), padding: c.padding,
	})
	body()
	C.n8v_close_panel()
}

// Sidebar holds Page calls. selected tracks the open page.
func Sidebar(title string, selected *Int, body func(), opts ...Option) {
	c := apply(opts)
	fn, ud := intCallback(c.onChange)
	collapse, collapseData := boolCallback(c.onToggle)
	o := C.n8v_sidebar_options{
		title: cstr(title), selected: selected.p, on_change: fn, on_change_userdata: ud,
		width: c.width.s, min_width: C.float(c.minWidth), max_width: C.float(c.maxWidth), compact: C.bool(c.compact),
		on_collapse: collapse, on_collapse_userdata: collapseData,
	}
	if c.collapsed != nil {
		o.collapsed = c.collapsed.p
	}
	C.n8v_open_sidebar(o)
	body()
	C.n8v_close_sidebar()
}

// SidebarSection adds a titled heading between a Sidebar's pages. Only valid inside a Sidebar.
func SidebarSection(title string) {
	C.n8v_sidebar_section(C.n8v_sidebar_section_options{title: cstr(title)})
}

// SidebarDivider adds a rule between a Sidebar's pages. Only valid inside a Sidebar.
func SidebarDivider() { C.n8v_sidebar_divider() }

// Page adds a sidebar entry and runs body when it is the selected page.
func Page(name string, body func(), opts ...Option) {
	c := apply(opts)
	if C.n8v_open_page(C.n8v_page_options{name: cstr(name), icon: cstr(c.icon), image: cstr(c.image), image_rounding: c.rounding, notify: C.bool(c.notify), badge: cstr(c.badge), badge_color: c.badgeColor, image_size: C.float(c.pageImageSize)}) {
		body()
		C.n8v_close_page()
	}
}

func Text(label string, opts ...Option) {
	c := apply(opts)
	fn, ud := callback(c.onClick)
	C.n8v_go_text(C.n8v_text_options{
		bold: C.bool(c.bold), italic: C.bool(c.italic), strikethrough: C.bool(c.strike),
		small: C.bool(c.small), darken: C.bool(c.darken), large: C.bool(c.large),
		url: cstr(c.url), color: c.color, on_click: fn, on_click_userdata: ud, click_unstyled: C.bool(c.clickUnstyled),
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
		icon: cstr(c.icon), icon_variant: c.iconVariant(), icon_position: pos, id: cstr(c.id),
		image: cstr(c.buttonImage), image_height: C.int(c.buttonImageHeight),
	}, cstr(label))
}

// ToggleButton is a button that stays pressed while checked is true; each click flips it. It takes the same icon and style
// options as Button (Icon, Ghost, Primary, ...).
func ToggleButton(label string, checked *Bool, opts ...Option) {
	c := apply(opts)
	pos := C.n8v_icon_position(C.N8V_ICON_POSITION_LEADING)
	if c.trailing {
		pos = C.N8V_ICON_POSITION_TRAILING
	}
	change, changeData := boolCallback(c.onToggle)
	C.n8v_go_button(C.n8v_button_options{
		style: c.buttonStyle, checked: checked.p, on_change: change, on_change_userdata: changeData,
		icon: cstr(c.icon), icon_variant: c.iconVariant(), icon_position: pos, id: cstr(c.id),
		image: cstr(c.buttonImage), image_height: C.int(c.buttonImageHeight),
	}, cstr(label))
}

func Checkbox(label string, checked *Bool, opts ...Option) {
	c := apply(opts)
	fn, ud := boolCallback(c.onToggle)
	C.n8v_go_checkbox(C.n8v_checkbox_options{checked: checked.p, on_change: fn, on_change_userdata: ud, id: cstr(c.id)}, cstr(label))
}

func Toggle(label string, checked *Bool, opts ...Option) {
	c := apply(opts)
	fn, ud := boolCallback(c.onToggle)
	C.n8v_go_toggle(C.n8v_toggle_options{checked: checked.p, on_change: fn, on_change_userdata: ud, id: cstr(c.id)}, cstr(label))
}

// Radio selects value in selected when clicked.
func Radio(label string, selected *Int, value int, opts ...Option) {
	c := apply(opts)
	fn, ud := intCallback(c.onChange)
	C.n8v_go_radio(C.n8v_radio_options{selected: selected.p, value: C.int(value), on_change: fn, on_change_userdata: ud, id: cstr(c.id)}, cstr(label))
}

func Entry(value *String, opts ...Option) {
	c := apply(opts)
	fn, ud := callback(c.onSubmit)
	mode := C.n8v_submit_mode(C.N8V_SUBMIT_ON_ENTER)
	if c.submitCtrlEnter {
		mode = C.N8V_SUBMIT_ON_CTRL_ENTER
	}
	o := C.n8v_entry_options{
		value: value.p, placeholder: cstr(c.placeholder), password: C.bool(c.password),
		on_submit: fn, on_submit_userdata: ud, id: cstr(c.id),
		multiline: C.bool(c.multiline), submit_mode: mode, match_entry_style: C.bool(c.matchEntryStyle),
		min_lines: C.uint16_t(max(c.minLines, 0)), max_lines: C.uint16_t(max(c.maxLines, 0)),
	}
	o.on_change, o.on_change_userdata = textCallback(c.onInput)
	if c.onKey != nil {
		h := cgo.NewHandle(c.onKey)
		frameHandles = append(frameHandles, h)
		o.on_key, o.on_key_userdata = C.n8v_key_fn(C.n8v_go_cb_key), C.n8v_go_handle(C.uintptr_t(h))
	}
	if c.onPasteFiles != nil {
		h := cgo.NewHandle(c.onPasteFiles)
		frameHandles = append(frameHandles, h)
		o.on_paste_files, o.on_paste_files_userdata = C.n8v_paste_files_fn(C.n8v_go_cb_paste_files), C.n8v_go_handle(C.uintptr_t(h))
	}
	C.n8v_entry(o)
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
	fn, ud := intCallback(c.onChange)
	C.n8v_dropdown(C.n8v_dropdown_options{
		items: list, item_count: C.size_t(len(items)), selected: selected.p, placeholder: cstr(c.placeholder),
		on_change: fn, on_change_userdata: ud, id: cstr(c.id),
	})
}

func Slider(value *Float, min, max float64, opts ...Option) {
	c := apply(opts)
	fn, ud := floatCallback(c.onSlide)
	C.n8v_slider(C.n8v_slider_options{value: value.p, min: C.float(min), max: C.float(max), on_change: fn, on_change_userdata: ud, id: cstr(c.id)})
}

// imageOptions fills in what every image source shares: size, rounding and the pointer callbacks.
func (c config) imageOptions(o C.n8v_image_options) C.n8v_image_options {
	o.width, o.height, o.rounding = c.width.s, c.height.s, c.rounding
	o.on_click, o.on_click_userdata = callback(c.onClick)
	o.on_click_at, o.on_click_at_userdata = pointCallback(c.onClickAt)
	o.on_drag, o.on_drag_userdata = pointCallback(c.onDrag)
	if c.onZoom != nil {
		h := cgo.NewHandle(c.onZoom)
		frameHandles = append(frameHandles, h)
		o.on_zoom, o.on_zoom_userdata = C.n8v_zoom_fn(C.n8vGoZoom), C.n8v_go_handle(C.uintptr_t(h))
	}
	return o
}

// Image draws a PNG, JPEG or SVG file. OnClick, OnClickAt, OnZoom and OnDrag make it interactive.
func Image(path string, opts ...Option) {
	c := apply(opts)
	C.n8v_image(c.imageOptions(C.n8v_image_options{source_kind: C.N8V_IMAGE_SOURCE_PATH, path: cstr(path)}))
}

// ImageBytes draws encoded image data.
func ImageBytes(data []byte, opts ...Option) {
	c := apply(opts)
	var p unsafe.Pointer
	if len(data) > 0 {
		p = C.CBytes(data)
		frameAllocs = append(frameAllocs, p)
	}
	C.n8v_image(c.imageOptions(C.n8v_image_options{
		source_kind: C.N8V_IMAGE_SOURCE_ENCODED, encoded_data: (*C.uint8_t)(p), encoded_size: C.size_t(len(data)),
	}))
}

// ImageRGBA draws width*height pixels of 8-bit RGBA, row by row from the top left.
func ImageRGBA(pixels []byte, width, height int, opts ...Option) {
	c := apply(opts)
	if len(pixels) < width*height*4 {
		return
	}
	p := C.CBytes(pixels[:width*height*4])
	frameAllocs = append(frameAllocs, p)
	C.n8v_image(c.imageOptions(C.n8v_image_options{
		source_kind: C.N8V_IMAGE_SOURCE_RGBA, pixels: (*C.uint8_t)(p), pixel_width: C.int(width), pixel_height: C.int(height),
	}))
}

// Video plays a file or URL (anything with a scheme, like https://).
func Video(source string, opts ...Option) {
	c := apply(opts)
	C.n8v_video(C.n8v_video_options{
		source: cstr(source), width: c.width.s, height: c.height.s, rounding: c.rounding,
		autoplay: C.bool(c.autoplay), loop: C.bool(c.loop), muted: C.bool(c.muted), controls: C.bool(c.controls), id: cstr(c.id),
	})
}

// Audio shows a player bar (play/pause, a seek bar and the time) for a file or URL. Fit width means 320.
func Audio(source string, opts ...Option) {
	c := apply(opts)
	C.n8v_audio(C.n8v_audio_options{
		source: cstr(source), width: c.width.s,
		autoplay: C.bool(c.autoplay), loop: C.bool(c.loop), muted: C.bool(c.muted), id: cstr(c.id),
	})
}

// Divider draws a rule. Color overrides the style's own divider color.
func Divider(opts ...Option) {
	c := apply(opts)
	C.n8v_divider(C.n8v_divider_options{color: c.color, id: cstr(c.id)})
}

// IconImage draws a bundled icon such as "settings", "star" or "heart".
func IconImage(name string, opts ...Option) {
	c := apply(opts)
	C.n8v_icon(C.n8v_icon_options{
		name: cstr(name), variant: c.iconVariant(), width: c.width.s, height: c.height.s, tint: c.color,
	})
}

// Document renders markdown. Defaults to Grow in both axes.
func Document(markdown string, opts ...Option) {
	c := apply(opts)
	w, h := c.width, c.height
	if w.s == (C.n8v_sizing{}) {
		w = Grow(0, 0)
	}
	if h.s == (C.n8v_sizing{}) {
		h = Grow(0, 0)
	}
	o := C.n8v_document_options{markdown: cstr(markdown), width: w.s, height: h.s, id: cstr(c.id), image_state: cstr(c.imageState)}
	if c.linkStyle != nil {
		hd := cgo.NewHandle(c.linkStyle)
		frameHandles = append(frameHandles, hd)
		o.link_style, o.link_style_userdata = C.n8v_link_style_fn(C.n8v_go_cb_link_style), C.n8v_go_handle(C.uintptr_t(hd))
	}
	if c.imageResolver != nil {
		hd := cgo.NewHandle(c.imageResolver)
		frameHandles = append(frameHandles, hd)
		o.image_resolver, o.image_resolver_userdata = C.n8v_image_resolve_fn(C.n8v_go_cb_image_resolve), C.n8v_go_handle(C.uintptr_t(hd))
	}
	C.n8v_document(o)
}

// Floating layers body over the enclosing element without affecting its layout. Use Attach and Offset to place it.
func Floating(body func(), opts ...Option) {
	c := apply(opts)
	dismiss, dismissData := callback(c.onDismiss)
	C.n8v_open_floating(C.n8v_floating_options{
		width: c.width.s, height: c.height.s,
		element_attach_point: c.attachElement, parent_attach_point: c.attachParent,
		offset_x: C.float(c.offsetX), offset_y: C.float(c.offsetY), opaque: C.bool(c.opaque), id: cstr(c.id),
		on_dismiss: dismiss, on_dismiss_userdata: dismissData,
		keep_on_screen: C.bool(c.keepOnScreen), screen_margin: C.float(c.screenMargin),
	})
	body()
	C.n8v_close_floating()
}

// Popover opens body beside the enclosing element: Side, AlignEnd and PopoverGap place it, and OnDismiss fires on a click outside.
func Popover(body func(), opts ...Option) {
	c := apply(opts)
	dismiss, dismissData := callback(c.onDismiss)
	C.n8v_open_popover(C.n8v_popover_options{
		side: C.n8v_popover_side(c.popoverSide), align_end: C.bool(c.alignEnd),
		width: c.width.s, height: c.height.s, gap: C.float(c.popoverGap), screen_margin: C.float(c.screenMargin),
		on_dismiss: dismiss, on_dismiss_userdata: dismissData, id: cstr(c.id),
	})
	body()
	C.n8v_close_popover()
}

// IsHovered reports whether the pointer was over the element with this ID as of the last completed frame.
func IsHovered(id string) bool {
	s := C.CString(id)
	defer C.free(unsafe.Pointer(s))
	return bool(C.n8v_is_hovered(s))
}

func withCStr(s string, fn func(*C.char)) {
	p := C.CString(s)
	defer C.free(unsafe.Pointer(p))
	fn(p)
}

func ScrollToBottom(containerID string) {
	withCStr(containerID, func(p *C.char) { C.n8v_scroll_to_bottom(p) })
}
func ScrollToTop(containerID string) {
	withCStr(containerID, func(p *C.char) { C.n8v_scroll_to_top(p) })
}
func ScrollToLeft(containerID string) {
	withCStr(containerID, func(p *C.char) { C.n8v_scroll_to_left(p) })
}
func ScrollToRight(containerID string) {
	withCStr(containerID, func(p *C.char) { C.n8v_scroll_to_right(p) })
}

// ScrollToOffset scrolls to an absolute pixel offset from the content's top-left.
func ScrollToOffset(containerID string, x, y float64) {
	withCStr(containerID, func(p *C.char) { C.n8v_scroll_to_offset(p, C.float(x), C.float(y)) })
}

// ScrollToElement scrolls so the element tagged elementID sits at the container's top-left, offset by the padding.
func ScrollToElement(containerID, elementID string, paddingLeft, paddingTop float64) {
	withCStr(containerID, func(c *C.char) {
		withCStr(elementID, func(e *C.char) {
			C.n8v_scroll_to_element(c, e, C.float(paddingLeft), C.float(paddingTop))
		})
	})
}

// FileFilter is a named group of patterns in a file dialog's type dropdown. A pattern is a glob ("*.png"), a dotted
// extension (".png") or a bare extension ("png"); matching is case-insensitive.
type FileFilter struct {
	Name     string
	Patterns []string
}

// FileDialogOptions configures OpenFile, SaveFile and OpenFolder. The zero value is a plain dialog.
type FileDialogOptions struct {
	Title string
	// InitialDir is the directory the dialog starts in.
	InitialDir string
	// DefaultName pre-fills the file name (SaveFile only).
	DefaultName string
	// Filters are shown in a type dropdown; the first is selected. Empty means all files.
	Filters []FileFilter
	// Multiple allows selecting several files or folders (not SaveFile).
	Multiple bool
}

func showFileDialog(kind C.n8v_file_dialog_kind, opts FileDialogOptions, onResult func(paths []string)) {
	// n8v copies every string, so all of this is freed as soon as the call returns.
	var allocs []unsafe.Pointer
	str := func(s string) *C.char {
		if s == "" {
			return nil
		}
		p := C.CString(s)
		allocs = append(allocs, unsafe.Pointer(p))
		return p
	}
	defer func() {
		for _, p := range allocs {
			C.free(p)
		}
	}()

	o := C.n8v_file_dialog_options{
		kind: kind, title: str(opts.Title), initial_dir: str(opts.InitialDir), default_name: str(opts.DefaultName),
		multiple: C.bool(opts.Multiple),
	}
	if len(opts.Filters) > 0 {
		filters := (*C.n8v_file_filter)(C.calloc(C.size_t(len(opts.Filters)), C.size_t(unsafe.Sizeof(C.n8v_file_filter{}))))
		allocs = append(allocs, unsafe.Pointer(filters))
		for i, f := range opts.Filters {
			var patterns **C.char
			if len(f.Patterns) > 0 {
				patterns = (**C.char)(C.malloc(C.size_t(len(f.Patterns)) * C.size_t(unsafe.Sizeof(uintptr(0)))))
				allocs = append(allocs, unsafe.Pointer(patterns))
				for j, p := range f.Patterns {
					unsafe.Slice(patterns, len(f.Patterns))[j] = str(p)
				}
			}
			unsafe.Slice(filters, len(opts.Filters))[i] = C.n8v_file_filter{
				name: str(f.Name), patterns: patterns, pattern_count: C.size_t(len(f.Patterns)),
			}
		}
		o.filters, o.filter_count = filters, C.size_t(len(opts.Filters))
	}

	// The handle outlives the frame: it is released by n8vGoFileDialog when the result arrives.
	C.n8v_go_show_file_dialog(o, C.uintptr_t(cgo.NewHandle(onResult)))
}

// OpenFile shows the system open dialog without blocking. onResult runs from a later Pump with the chosen paths, or none
// if the user cancelled. The platform's own dialog is used where there is one (the XDG portal on Linux), else the toolkit's.
func OpenFile(opts FileDialogOptions, onResult func(paths []string)) {
	showFileDialog(C.N8V_FILE_DIALOG_OPEN_FILE, opts, onResult)
}

// SaveFile shows the system save dialog; the result is the single path to write to.
func SaveFile(opts FileDialogOptions, onResult func(paths []string)) {
	showFileDialog(C.N8V_FILE_DIALOG_SAVE_FILE, opts, onResult)
}

// OpenFolder shows the system folder chooser.
func OpenFolder(opts FileDialogOptions, onResult func(paths []string)) {
	showFileDialog(C.N8V_FILE_DIALOG_OPEN_FOLDER, opts, onResult)
}

// MessageKind picks the icon or styling of a message dialog.
type MessageKind int

const (
	MessageInfo MessageKind = iota
	MessageWarning
	MessageError
)

// MessageButton is one button of a message dialog.
type MessageButton struct {
	Label string
	// Destructive styles the button as a dangerous action (e.g. red) where the toolkit supports it.
	Destructive bool
}

// MessageDialogOptions configures ShowMessage.
type MessageDialogOptions struct {
	Title   string
	Message string
	Kind    MessageKind
	// Buttons are shown in order and the first is the default. Empty shows a single "OK".
	Buttons []MessageButton
}

// ShowMessage shows a message dialog without blocking. onResult runs from a later Pump with the index of the pressed
// button, or -1 if the dialog was dismissed (or none could be shown). onResult may be nil.
func ShowMessage(opts MessageDialogOptions, onResult func(button int)) {
	// n8v copies every string, so all of this is freed as soon as the call returns.
	var allocs []unsafe.Pointer
	str := func(s string) *C.char {
		p := C.CString(s)
		allocs = append(allocs, unsafe.Pointer(p))
		return p
	}
	defer func() {
		for _, p := range allocs {
			C.free(p)
		}
	}()

	o := C.n8v_message_dialog_options{title: str(opts.Title), message: str(opts.Message), kind: C.n8v_message_dialog_kind(opts.Kind)}
	if len(opts.Buttons) > 0 {
		buttons := (*C.n8v_message_button)(C.calloc(C.size_t(len(opts.Buttons)), C.size_t(unsafe.Sizeof(C.n8v_message_button{}))))
		allocs = append(allocs, unsafe.Pointer(buttons))
		for i, b := range opts.Buttons {
			unsafe.Slice(buttons, len(opts.Buttons))[i] = C.n8v_message_button{label: str(b.Label), destructive: C.bool(b.Destructive)}
		}
		o.buttons, o.button_count = buttons, C.size_t(len(opts.Buttons))
	}

	var handle C.uintptr_t
	if onResult != nil {
		// Released by n8vGoMessageDialog when the result arrives.
		handle = C.uintptr_t(cgo.NewHandle(onResult))
	}
	C.n8v_go_show_message_dialog(o, handle)
}
