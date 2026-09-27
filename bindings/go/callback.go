package n8v

import "C"

import (
	"runtime/cgo"
	"unsafe"
)

//export n8vGoClick
func n8vGoClick(userdata unsafe.Pointer) {
	cgo.Handle(uintptr(userdata)).Value().(func())()
}
