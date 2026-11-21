package n8v

/*
#include <stddef.h>
#include <stdbool.h>
#include <n8v/n8v_c.h>
*/
import "C"

import "unsafe"

//export n8vGoPaintButton
func n8vGoPaintButton(style C.int, hovered C.bool, pressed C.bool, out *C.n8v_button_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Button(ButtonStyle(style), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintText
func n8vGoPaintText(options *C.n8v_text_options, out *C.n8v_text_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Text(textPaintOptionsFromC(options)).toC()
}

//export n8vGoPaintCheckbox
func n8vGoPaintCheckbox(checked C.bool, hovered C.bool, pressed C.bool, out *C.n8v_checkbox_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Checkbox(bool(checked), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintRadio
func n8vGoPaintRadio(selected C.bool, hovered C.bool, pressed C.bool, out *C.n8v_radio_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Radio(bool(selected), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintToggle
func n8vGoPaintToggle(on C.bool, hovered C.bool, pressed C.bool, out *C.n8v_toggle_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Toggle(bool(on), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintEntry
func n8vGoPaintEntry(focused C.bool, hasValue C.bool, out *C.n8v_entry_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Entry(bool(focused), bool(hasValue)).toC()
}

//export n8vGoPaintDropdown
func n8vGoPaintDropdown(open C.bool, hasSelection C.bool, hovered C.bool, pressed C.bool, out *C.n8v_dropdown_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Dropdown(bool(open), bool(hasSelection), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintSlider
func n8vGoPaintSlider(hovered C.bool, pressed C.bool, out *C.n8v_slider_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Slider(bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintImage
func n8vGoPaintImage(out *C.n8v_image_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Image().toC()
}

//export n8vGoPaintIcon
func n8vGoPaintIcon(out *C.n8v_icon_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Icon().toC()
}

//export n8vGoPaintSidebar
func n8vGoPaintSidebar(out *C.n8v_sidebar_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Sidebar().toC()
}

//export n8vGoPaintPanel
func n8vGoPaintPanel(role C.int, hovered C.bool, pressed C.bool, out *C.n8v_panel_paint, ud unsafe.Pointer) {
	*out = paintOf(ud).Panel(PanelRole(role), bool(hovered), bool(pressed)).toC()
}

//export n8vGoPaintWindowBackground
func n8vGoPaintWindowBackground(out *C.n8v_color, ud unsafe.Pointer) {
	*out = paintOf(ud).WindowBackground().toC()
}
