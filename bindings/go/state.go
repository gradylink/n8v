package n8v

/*
#include <stdlib.h>
#include <string.h>
#include <n8v/n8v_c.h>
*/
import "C"

import "unsafe"

// n8v writes widget state through pointers it keeps between frames, so state lives in C memory
// for the life of the program.

type Bool struct{ p *C.bool }

func NewBool(v bool) *Bool {
	b := &Bool{(*C.bool)(C.malloc(C.size_t(unsafe.Sizeof(C.bool(false)))))}
	b.Set(v)
	return b
}

func (b *Bool) Get() bool  { return bool(*b.p) }
func (b *Bool) Set(v bool) { *b.p = C.bool(v) }

type Int struct{ p *C.int }

func NewInt(v int) *Int {
	i := &Int{(*C.int)(C.malloc(C.size_t(unsafe.Sizeof(C.int(0)))))}
	i.Set(v)
	return i
}

func (i *Int) Get() int  { return int(*i.p) }
func (i *Int) Set(v int) { *i.p = C.int(v) }

type Float struct{ p *C.float }

func NewFloat(v float64) *Float {
	f := &Float{(*C.float)(C.malloc(C.size_t(unsafe.Sizeof(C.float(0)))))}
	f.Set(v)
	return f
}

func (f *Float) Get() float64  { return float64(*f.p) }
func (f *Float) Set(v float64) { *f.p = C.float(v) }

type String struct{ p *C.n8v_string_buf }

func NewString(v string) *String {
	s := &String{(*C.n8v_string_buf)(C.calloc(1, C.size_t(unsafe.Sizeof(C.n8v_string_buf{}))))}
	C.n8v_string_buf_init(s.p)
	s.Set(v)
	return s
}

func (s *String) Get() string { return C.GoStringN(s.p.data, C.int(s.p.length)) }

func (s *String) Set(v string) {
	C.n8v_string_buf_free(s.p)
	s.p.data = C.CString(v)
	s.p.length = C.size_t(len(v))
	s.p.capacity = C.size_t(len(v) + 1)
}
