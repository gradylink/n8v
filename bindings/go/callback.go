package n8v

/*
#include <stdbool.h>
#include <stddef.h>
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
