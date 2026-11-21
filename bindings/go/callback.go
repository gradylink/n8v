package n8v

/*
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <n8v/n8v_c.h>
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

//export n8vGoClick
func n8vGoClick(userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func())()
}

//export n8vGoBool
func n8vGoBool(value C.bool, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(bool))(bool(value))
}

//export n8vGoInt
func n8vGoInt(value C.int, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(int))(int(value))
}

//export n8vGoFileDialog
func n8vGoFileDialog(paths **C.char, count C.size_t, userdata unsafe.Pointer) {
	h := cgo.Handle(uintptr(userdata))
	fn := h.Value().(func([]string))
	h.Delete()
	out := make([]string, 0, int(count))
	if count > 0 {
		for _, p := range unsafe.Slice(paths, int(count)) {
			out = append(out, C.GoString(p))
		}
	}
	fn(out)
}

//export n8vGoMessageDialog
func n8vGoMessageDialog(button C.int, userdata unsafe.Pointer) {
	if userdata == nil {
		return
	}
	h := cgo.Handle(uintptr(userdata))
	fn := h.Value().(func(int))
	h.Delete()
	fn(int(button))
}

//export n8vGoFloat
func n8vGoFloat(value C.float, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(float64))(float64(value))
}

//export n8vGoText
func n8vGoText(text *C.char, length C.size_t, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(string))(C.GoStringN(text, C.int(length)))
}

//export n8vGoPoint
func n8vGoPoint(x, y C.float, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(float64, float64))(float64(x), float64(y))
}

//export n8vGoZoom
func n8vGoZoom(factor, x, y C.float, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(float64, float64, float64))(float64(factor), float64(x), float64(y))
}

//export n8vGoKey
func n8vGoKey(key C.int, mods C.uint, userdata unsafe.Pointer) C.bool {
	return C.bool(cgo.Handle(uintptr(userdata)).Value().(func(Key, Mods) bool)(Key(key), Mods(mods)))
}

//export n8vGoPasteFiles
func n8vGoPasteFiles(paths **C.char, count C.size_t, userdata unsafe.Pointer) {
	out := make([]string, 0, int(count))
	if count > 0 {
		for _, p := range unsafe.Slice(paths, int(count)) {
			out = append(out, C.GoString(p))
		}
	}
	cgo.Handle(uintptr(userdata)).Value().(func([]string))(out)
}

//export n8vGoLinkStyle
func n8vGoLinkStyle(url *C.char, out *C.n8v_link_style, userdata unsafe.Pointer) C.bool {
	style, ok := cgo.Handle(uintptr(userdata)).Value().(func(string) (LinkStyle, bool))(C.GoString(url))
	if !ok {
		return false
	}
	out.foreground = C.n8v_color{C.float(style.Foreground.R), C.float(style.Foreground.G), C.float(style.Foreground.B), C.float(style.Foreground.A)}
	out.background = C.n8v_color{C.float(style.Background.R), C.float(style.Background.G), C.float(style.Background.B), C.float(style.Background.A)}
	out.bold = C.bool(style.Bold)
	out.clickable = C.bool(style.Clickable)
	return true
}

//export n8vGoImageResolve
func n8vGoImageResolve(src *C.char, userdata unsafe.Pointer) *C.char {
	return cstr(cgo.Handle(uintptr(userdata)).Value().(func(string) string)(C.GoString(src)))
}

//export n8vGoURLHandler
func n8vGoURLHandler(url *C.char, userdata unsafe.Pointer) C.bool {
	return C.bool(cgo.Handle(uintptr(userdata)).Value().(func(string) bool)(C.GoString(url)))
}

//export n8vGoVisibleChild
func n8vGoVisibleChild(id *C.char, top, height C.float, userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func(string, float64, float64))(C.GoString(id), float64(top), float64(height))
}
