package n8v

/*
#include <stdbool.h>
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
