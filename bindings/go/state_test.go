package n8v

import "testing"

func TestState(t *testing.T) {
	b, i, f, s := NewBool(true), NewInt(-1), NewFloat(0.5), NewString("héllo")
	if !b.Get() || i.Get() != -1 || f.Get() != 0.5 || s.Get() != "héllo" {
		t.Fatal("initial values")
	}
	b.Set(false)
	i.Set(3)
	f.Set(1)
	s.Set("")
	if b.Get() || i.Get() != 3 || f.Get() != 1 || s.Get() != "" {
		t.Fatal("updated values")
	}
}
