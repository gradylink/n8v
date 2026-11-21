package n8v

/*
#include <stdlib.h>
#include <string.h>
#include <n8v/n8v_c.h>

extern void n8vGoPaintButton(int style, bool hovered, bool pressed, n8v_button_paint *out, void *ud);
extern void n8vGoPaintText(n8v_text_options * options, n8v_text_paint *out, void *ud);
extern void n8vGoPaintCheckbox(bool checked, bool hovered, bool pressed, n8v_checkbox_paint *out, void *ud);
extern void n8vGoPaintRadio(bool selected, bool hovered, bool pressed, n8v_radio_paint *out, void *ud);
extern void n8vGoPaintToggle(bool on, bool hovered, bool pressed, n8v_toggle_paint *out, void *ud);
extern void n8vGoPaintEntry(bool focused, bool hasValue, n8v_entry_paint *out, void *ud);
extern void n8vGoPaintDropdown(bool open, bool hasSelection, bool hovered, bool pressed, n8v_dropdown_paint *out, void *ud);
extern void n8vGoPaintSlider(bool hovered, bool pressed, n8v_slider_paint *out, void *ud);
extern void n8vGoPaintImage(n8v_image_paint *out, void *ud);
extern void n8vGoPaintIcon(n8v_icon_paint *out, void *ud);
extern void n8vGoPaintSidebar(n8v_sidebar_paint *out, void *ud);
extern void n8vGoPaintPanel(int role, bool hovered, bool pressed, n8v_panel_paint *out, void *ud);
extern void n8vGoPaintWindowBackground(n8v_color *out, void *ud);

n8v_button_paint n8v_go_paint_button(n8v_button_style style, bool hovered, bool pressed, void *ud) { n8v_button_paint out; memset(&out, 0, sizeof out); n8vGoPaintButton((int)style, hovered, pressed, &out, ud); return out; }
n8v_text_paint n8v_go_paint_text(n8v_text_options options, void *ud) { n8v_text_paint out; memset(&out, 0, sizeof out); n8vGoPaintText(&options, &out, ud); return out; }
n8v_checkbox_paint n8v_go_paint_checkbox(bool checked, bool hovered, bool pressed, void *ud) { n8v_checkbox_paint out; memset(&out, 0, sizeof out); n8vGoPaintCheckbox(checked, hovered, pressed, &out, ud); return out; }
n8v_radio_paint n8v_go_paint_radio(bool selected, bool hovered, bool pressed, void *ud) { n8v_radio_paint out; memset(&out, 0, sizeof out); n8vGoPaintRadio(selected, hovered, pressed, &out, ud); return out; }
n8v_toggle_paint n8v_go_paint_toggle(bool on, bool hovered, bool pressed, void *ud) { n8v_toggle_paint out; memset(&out, 0, sizeof out); n8vGoPaintToggle(on, hovered, pressed, &out, ud); return out; }
n8v_entry_paint n8v_go_paint_entry(bool focused, bool hasValue, void *ud) { n8v_entry_paint out; memset(&out, 0, sizeof out); n8vGoPaintEntry(focused, hasValue, &out, ud); return out; }
n8v_dropdown_paint n8v_go_paint_dropdown(bool open, bool hasSelection, bool hovered, bool pressed, void *ud) { n8v_dropdown_paint out; memset(&out, 0, sizeof out); n8vGoPaintDropdown(open, hasSelection, hovered, pressed, &out, ud); return out; }
n8v_slider_paint n8v_go_paint_slider(bool hovered, bool pressed, void *ud) { n8v_slider_paint out; memset(&out, 0, sizeof out); n8vGoPaintSlider(hovered, pressed, &out, ud); return out; }
n8v_image_paint n8v_go_paint_image(void *ud) { n8v_image_paint out; memset(&out, 0, sizeof out); n8vGoPaintImage(&out, ud); return out; }
n8v_icon_paint n8v_go_paint_icon(void *ud) { n8v_icon_paint out; memset(&out, 0, sizeof out); n8vGoPaintIcon(&out, ud); return out; }
n8v_sidebar_paint n8v_go_paint_sidebar(void *ud) { n8v_sidebar_paint out; memset(&out, 0, sizeof out); n8vGoPaintSidebar(&out, ud); return out; }
n8v_panel_paint n8v_go_paint_panel(n8v_panel_role role, bool hovered, bool pressed, void *ud) { n8v_panel_paint out; memset(&out, 0, sizeof out); n8vGoPaintPanel((int)role, hovered, pressed, &out, ud); return out; }
n8v_color n8v_go_paint_window_background(void *ud) { n8v_color out; memset(&out, 0, sizeof out); n8vGoPaintWindowBackground(&out, ud); return out; }
static void *n8v_go_paint_handle(uintptr_t h) { return (void *)h; }
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

// Insets is a padding in pixels.
type Insets struct{ Left, Right, Top, Bottom int }

// Corners is a corner radius in pixels.
type Corners struct{ TopLeft, TopRight, BottomLeft, BottomRight float64 }

// Font is one of the bundled fonts.
type Font int

const (
	DejaVuSans Font = iota
	Roboto
	Inter
	Selawik
)

// ButtonStyle is which kind of Button a paint callback is asked about.
type ButtonStyle int

const (
	ButtonSecondary ButtonStyle = iota
	ButtonPrimary
	ButtonGhost
)

// PanelRole is which kind of Panel a paint callback is asked about.
type PanelRole int

const (
	PanelCard PanelRole = iota
	PanelListItem
)

func (c Rgba) toC() C.n8v_color {
	return C.n8v_color{C.float(c.R), C.float(c.G), C.float(c.B), C.float(c.A)}
}
func rgbaFromC(c C.n8v_color) Rgba {
	return Rgba{float64(c.r), float64(c.g), float64(c.b), float64(c.a)}
}
func (p Insets) toC() C.n8v_padding {
	return C.n8v_padding{C.uint16_t(p.Left), C.uint16_t(p.Right), C.uint16_t(p.Top), C.uint16_t(p.Bottom)}
}
func insetsFromC(p C.n8v_padding) Insets {
	return Insets{int(p.left), int(p.right), int(p.top), int(p.bottom)}
}
func (r Corners) toC() C.n8v_corner_radius {
	return C.n8v_corner_radius{C.float(r.TopLeft), C.float(r.TopRight), C.float(r.BottomLeft), C.float(r.BottomRight)}
}
func cornersFromC(r C.n8v_corner_radius) Corners {
	return Corners{float64(r.top_left), float64(r.top_right), float64(r.bottom_left), float64(r.bottom_right)}
}

// TextPaintOptions is what CustomPaint.Text is asked about: the options of the text being drawn.
type TextPaintOptions struct {
	Bold, Italic, Strikethrough, Small, Darken, Large, ClickUnstyled bool
	URL                                                              string
	Color                                                            Rgba
}

func textPaintOptionsFromC(o *C.n8v_text_options) TextPaintOptions {
	return TextPaintOptions{
		Bold: bool(o.bold), Italic: bool(o.italic), Strikethrough: bool(o.strikethrough), Small: bool(o.small), Darken: bool(o.darken),
		Large: bool(o.large), ClickUnstyled: bool(o.click_unstyled), URL: C.GoString(o.url), Color: rgbaFromC(o.color),
	}
}

func (o TextPaintOptions) toC() (C.n8v_text_options, func()) {
	c := C.n8v_text_options{
		bold: C.bool(o.Bold), italic: C.bool(o.Italic), strikethrough: C.bool(o.Strikethrough), small: C.bool(o.Small),
		darken: C.bool(o.Darken), large: C.bool(o.Large), click_unstyled: C.bool(o.ClickUnstyled), color: o.Color.toC(),
	}
	if o.URL == "" {
		return c, func() {}
	}
	c.url = C.CString(o.URL)
	return c, func() { C.free(unsafe.Pointer(c.url)) }
}

// ButtonPaint is how a button is drawn.
type ButtonPaint struct {
	Background        Rgba
	TextColor         Rgba
	CornerRadius      Corners
	Padding           Insets
	Font              Font
	FontSize          int
	TransitionSeconds float64
}

func (p ButtonPaint) toC() C.n8v_button_paint {
	var o C.n8v_button_paint
	o.background = p.Background.toC()
	o.text_color = p.TextColor.toC()
	o.corner_radius = p.CornerRadius.toC()
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	return o
}

func buttonPaintFromC(o C.n8v_button_paint) ButtonPaint {
	return ButtonPaint{
		Background:        rgbaFromC(o.background),
		TextColor:         rgbaFromC(o.text_color),
		CornerRadius:      cornersFromC(o.corner_radius),
		Padding:           insetsFromC(o.padding),
		Font:              Font(o.font),
		FontSize:          int(o.font_size),
		TransitionSeconds: float64(o.transition_seconds),
	}
}

// TextPaint is how a text is drawn.
type TextPaint struct {
	Color    Rgba
	Font     Font
	FontSize int
}

func (p TextPaint) toC() C.n8v_text_paint {
	var o C.n8v_text_paint
	o.color = p.Color.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	return o
}

func textPaintFromC(o C.n8v_text_paint) TextPaint {
	return TextPaint{
		Color:    rgbaFromC(o.color),
		Font:     Font(o.font),
		FontSize: int(o.font_size),
	}
}

// CheckboxPaint is how a checkbox is drawn.
type CheckboxPaint struct {
	Background        Rgba
	BorderColor       Rgba
	BorderWidth       float64
	CheckColor        Rgba
	CornerRadius      Corners
	IndicatorSize     float64
	Padding           Insets
	Font              Font
	FontSize          int
	TransitionSeconds float64
	CheckMark         int
}

func (p CheckboxPaint) toC() C.n8v_checkbox_paint {
	var o C.n8v_checkbox_paint
	o.background = p.Background.toC()
	o.border_color = p.BorderColor.toC()
	o.border_width = C.float(p.BorderWidth)
	o.check_color = p.CheckColor.toC()
	o.corner_radius = p.CornerRadius.toC()
	o.indicator_size = C.float(p.IndicatorSize)
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	o.check_mark = C.int(p.CheckMark)
	return o
}

func checkboxPaintFromC(o C.n8v_checkbox_paint) CheckboxPaint {
	return CheckboxPaint{
		Background:        rgbaFromC(o.background),
		BorderColor:       rgbaFromC(o.border_color),
		BorderWidth:       float64(o.border_width),
		CheckColor:        rgbaFromC(o.check_color),
		CornerRadius:      cornersFromC(o.corner_radius),
		IndicatorSize:     float64(o.indicator_size),
		Padding:           insetsFromC(o.padding),
		Font:              Font(o.font),
		FontSize:          int(o.font_size),
		TransitionSeconds: float64(o.transition_seconds),
		CheckMark:         int(o.check_mark),
	}
}

// RadioPaint is how a radio is drawn.
type RadioPaint struct {
	Background        Rgba
	BorderColor       Rgba
	BorderWidth       float64
	DotColor          Rgba
	IndicatorSize     float64
	Padding           Insets
	Font              Font
	FontSize          int
	TransitionSeconds float64
}

func (p RadioPaint) toC() C.n8v_radio_paint {
	var o C.n8v_radio_paint
	o.background = p.Background.toC()
	o.border_color = p.BorderColor.toC()
	o.border_width = C.float(p.BorderWidth)
	o.dot_color = p.DotColor.toC()
	o.indicator_size = C.float(p.IndicatorSize)
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	return o
}

func radioPaintFromC(o C.n8v_radio_paint) RadioPaint {
	return RadioPaint{
		Background:        rgbaFromC(o.background),
		BorderColor:       rgbaFromC(o.border_color),
		BorderWidth:       float64(o.border_width),
		DotColor:          rgbaFromC(o.dot_color),
		IndicatorSize:     float64(o.indicator_size),
		Padding:           insetsFromC(o.padding),
		Font:              Font(o.font),
		FontSize:          int(o.font_size),
		TransitionSeconds: float64(o.transition_seconds),
	}
}

// TogglePaint is how a toggle is drawn.
type TogglePaint struct {
	TrackOnColor      Rgba
	TrackOffColor     Rgba
	TrackBorderColor  Rgba
	TrackBorderWidth  float64
	KnobOnColor       Rgba
	KnobOffColor      Rgba
	KnobGlyphColor    Rgba
	ShowGlyphWhenOn   bool
	TrackWidth        float64
	TrackHeight       float64
	KnobSizeOff       float64
	KnobSizeOn        float64
	Padding           Insets
	Font              Font
	FontSize          int
	TransitionSeconds float64
}

func (p TogglePaint) toC() C.n8v_toggle_paint {
	var o C.n8v_toggle_paint
	o.track_on_color = p.TrackOnColor.toC()
	o.track_off_color = p.TrackOffColor.toC()
	o.track_border_color = p.TrackBorderColor.toC()
	o.track_border_width = C.float(p.TrackBorderWidth)
	o.knob_on_color = p.KnobOnColor.toC()
	o.knob_off_color = p.KnobOffColor.toC()
	o.knob_glyph_color = p.KnobGlyphColor.toC()
	o.show_glyph_when_on = C.bool(p.ShowGlyphWhenOn)
	o.track_width = C.float(p.TrackWidth)
	o.track_height = C.float(p.TrackHeight)
	o.knob_size_off = C.float(p.KnobSizeOff)
	o.knob_size_on = C.float(p.KnobSizeOn)
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	return o
}

func togglePaintFromC(o C.n8v_toggle_paint) TogglePaint {
	return TogglePaint{
		TrackOnColor:      rgbaFromC(o.track_on_color),
		TrackOffColor:     rgbaFromC(o.track_off_color),
		TrackBorderColor:  rgbaFromC(o.track_border_color),
		TrackBorderWidth:  float64(o.track_border_width),
		KnobOnColor:       rgbaFromC(o.knob_on_color),
		KnobOffColor:      rgbaFromC(o.knob_off_color),
		KnobGlyphColor:    rgbaFromC(o.knob_glyph_color),
		ShowGlyphWhenOn:   bool(o.show_glyph_when_on),
		TrackWidth:        float64(o.track_width),
		TrackHeight:       float64(o.track_height),
		KnobSizeOff:       float64(o.knob_size_off),
		KnobSizeOn:        float64(o.knob_size_on),
		Padding:           insetsFromC(o.padding),
		Font:              Font(o.font),
		FontSize:          int(o.font_size),
		TransitionSeconds: float64(o.transition_seconds),
	}
}

// EntryPaint is how a entry is drawn.
type EntryPaint struct {
	Background        Rgba
	TextColor         Rgba
	PlaceholderColor  Rgba
	BorderColor       Rgba
	BorderWidth       float64
	Outlined          bool
	LabelColor        Rgba
	LabelFontSize     int
	TransitionSeconds float64
	CornerRadius      Corners
	Padding           Insets
	Font              Font
	FontSize          int
}

func (p EntryPaint) toC() C.n8v_entry_paint {
	var o C.n8v_entry_paint
	o.background = p.Background.toC()
	o.text_color = p.TextColor.toC()
	o.placeholder_color = p.PlaceholderColor.toC()
	o.border_color = p.BorderColor.toC()
	o.border_width = C.float(p.BorderWidth)
	o.outlined = C.bool(p.Outlined)
	o.label_color = p.LabelColor.toC()
	o.label_font_size = C.uint16_t(p.LabelFontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	o.corner_radius = p.CornerRadius.toC()
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	return o
}

func entryPaintFromC(o C.n8v_entry_paint) EntryPaint {
	return EntryPaint{
		Background:        rgbaFromC(o.background),
		TextColor:         rgbaFromC(o.text_color),
		PlaceholderColor:  rgbaFromC(o.placeholder_color),
		BorderColor:       rgbaFromC(o.border_color),
		BorderWidth:       float64(o.border_width),
		Outlined:          bool(o.outlined),
		LabelColor:        rgbaFromC(o.label_color),
		LabelFontSize:     int(o.label_font_size),
		TransitionSeconds: float64(o.transition_seconds),
		CornerRadius:      cornersFromC(o.corner_radius),
		Padding:           insetsFromC(o.padding),
		Font:              Font(o.font),
		FontSize:          int(o.font_size),
	}
}

// DropdownPaint is how a dropdown is drawn.
type DropdownPaint struct {
	Background             Rgba
	TextColor              Rgba
	PlaceholderColor       Rgba
	PopupBackground        Rgba
	ItemHoverBackground    Rgba
	ItemSelectedBackground Rgba
	CornerRadius           Corners
	Padding                Insets
	Font                   Font
	FontSize               int
	TransitionSeconds      float64
	LabelColor             Rgba
	LabelFontSize          int
	IndicatorColor         Rgba
	IndicatorWidth         float64
}

func (p DropdownPaint) toC() C.n8v_dropdown_paint {
	var o C.n8v_dropdown_paint
	o.background = p.Background.toC()
	o.text_color = p.TextColor.toC()
	o.placeholder_color = p.PlaceholderColor.toC()
	o.popup_background = p.PopupBackground.toC()
	o.item_hover_background = p.ItemHoverBackground.toC()
	o.item_selected_background = p.ItemSelectedBackground.toC()
	o.corner_radius = p.CornerRadius.toC()
	o.padding = p.Padding.toC()
	o.font = C.n8v_font_family(p.Font)
	o.font_size = C.uint16_t(p.FontSize)
	o.transition_seconds = C.float(p.TransitionSeconds)
	o.label_color = p.LabelColor.toC()
	o.label_font_size = C.uint16_t(p.LabelFontSize)
	o.indicator_color = p.IndicatorColor.toC()
	o.indicator_width = C.float(p.IndicatorWidth)
	return o
}

func dropdownPaintFromC(o C.n8v_dropdown_paint) DropdownPaint {
	return DropdownPaint{
		Background:             rgbaFromC(o.background),
		TextColor:              rgbaFromC(o.text_color),
		PlaceholderColor:       rgbaFromC(o.placeholder_color),
		PopupBackground:        rgbaFromC(o.popup_background),
		ItemHoverBackground:    rgbaFromC(o.item_hover_background),
		ItemSelectedBackground: rgbaFromC(o.item_selected_background),
		CornerRadius:           cornersFromC(o.corner_radius),
		Padding:                insetsFromC(o.padding),
		Font:                   Font(o.font),
		FontSize:               int(o.font_size),
		TransitionSeconds:      float64(o.transition_seconds),
		LabelColor:             rgbaFromC(o.label_color),
		LabelFontSize:          int(o.label_font_size),
		IndicatorColor:         rgbaFromC(o.indicator_color),
		IndicatorWidth:         float64(o.indicator_width),
	}
}

// SliderPaint is how a slider is drawn.
type SliderPaint struct {
	TrackColor       Rgba
	FillColor        Rgba
	ThumbColor       Rgba
	TrackHeight      float64
	ThumbWidth       float64
	ThumbHeight      float64
	ThumbBorderColor Rgba
	ThumbBorderWidth float64
	TrackGap         float64
}

func (p SliderPaint) toC() C.n8v_slider_paint {
	var o C.n8v_slider_paint
	o.track_color = p.TrackColor.toC()
	o.fill_color = p.FillColor.toC()
	o.thumb_color = p.ThumbColor.toC()
	o.track_height = C.float(p.TrackHeight)
	o.thumb_width = C.float(p.ThumbWidth)
	o.thumb_height = C.float(p.ThumbHeight)
	o.thumb_border_color = p.ThumbBorderColor.toC()
	o.thumb_border_width = C.float(p.ThumbBorderWidth)
	o.track_gap = C.float(p.TrackGap)
	return o
}

func sliderPaintFromC(o C.n8v_slider_paint) SliderPaint {
	return SliderPaint{
		TrackColor:       rgbaFromC(o.track_color),
		FillColor:        rgbaFromC(o.fill_color),
		ThumbColor:       rgbaFromC(o.thumb_color),
		TrackHeight:      float64(o.track_height),
		ThumbWidth:       float64(o.thumb_width),
		ThumbHeight:      float64(o.thumb_height),
		ThumbBorderColor: rgbaFromC(o.thumb_border_color),
		ThumbBorderWidth: float64(o.thumb_border_width),
		TrackGap:         float64(o.track_gap),
	}
}

// ImagePaint is how a image is drawn.
type ImagePaint struct {
	CornerRadius Corners
}

func (p ImagePaint) toC() C.n8v_image_paint {
	var o C.n8v_image_paint
	o.corner_radius = p.CornerRadius.toC()
	return o
}

func imagePaintFromC(o C.n8v_image_paint) ImagePaint {
	return ImagePaint{
		CornerRadius: cornersFromC(o.corner_radius),
	}
}

// IconPaint is how a icon is drawn.
type IconPaint struct {
	Tint        Rgba
	DefaultSize float64
}

func (p IconPaint) toC() C.n8v_icon_paint {
	var o C.n8v_icon_paint
	o.tint = p.Tint.toC()
	o.default_size = C.float(p.DefaultSize)
	return o
}

func iconPaintFromC(o C.n8v_icon_paint) IconPaint {
	return IconPaint{
		Tint:        rgbaFromC(o.tint),
		DefaultSize: float64(o.default_size),
	}
}

// SidebarPaint is how a sidebar is drawn.
type SidebarPaint struct {
	Background            Rgba
	BorderColor           Rgba
	BorderWidth           float64
	CornerRadius          Corners
	Padding               Insets
	RowGap                float64
	RowBackground         Rgba
	RowHoverBackground    Rgba
	RowSelectedBackground Rgba
	RowTextColor          Rgba
	RowSelectedTextColor  Rgba
	RowCornerRadius       float64
	RowPadding            Insets
}

func (p SidebarPaint) toC() C.n8v_sidebar_paint {
	var o C.n8v_sidebar_paint
	o.background = p.Background.toC()
	o.border_color = p.BorderColor.toC()
	o.border_width = C.float(p.BorderWidth)
	o.corner_radius = p.CornerRadius.toC()
	o.padding = p.Padding.toC()
	o.row_gap = C.float(p.RowGap)
	o.row_background = p.RowBackground.toC()
	o.row_hover_background = p.RowHoverBackground.toC()
	o.row_selected_background = p.RowSelectedBackground.toC()
	o.row_text_color = p.RowTextColor.toC()
	o.row_selected_text_color = p.RowSelectedTextColor.toC()
	o.row_corner_radius = C.float(p.RowCornerRadius)
	o.row_padding = p.RowPadding.toC()
	return o
}

func sidebarPaintFromC(o C.n8v_sidebar_paint) SidebarPaint {
	return SidebarPaint{
		Background:            rgbaFromC(o.background),
		BorderColor:           rgbaFromC(o.border_color),
		BorderWidth:           float64(o.border_width),
		CornerRadius:          cornersFromC(o.corner_radius),
		Padding:               insetsFromC(o.padding),
		RowGap:                float64(o.row_gap),
		RowBackground:         rgbaFromC(o.row_background),
		RowHoverBackground:    rgbaFromC(o.row_hover_background),
		RowSelectedBackground: rgbaFromC(o.row_selected_background),
		RowTextColor:          rgbaFromC(o.row_text_color),
		RowSelectedTextColor:  rgbaFromC(o.row_selected_text_color),
		RowCornerRadius:       float64(o.row_corner_radius),
		RowPadding:            insetsFromC(o.row_padding),
	}
}

// PanelPaint is how a panel is drawn.
type PanelPaint struct {
	Background   Rgba
	BorderColor  Rgba
	BorderWidth  float64
	CornerRadius Corners
	Padding      Insets
}

func (p PanelPaint) toC() C.n8v_panel_paint {
	var o C.n8v_panel_paint
	o.background = p.Background.toC()
	o.border_color = p.BorderColor.toC()
	o.border_width = C.float(p.BorderWidth)
	o.corner_radius = p.CornerRadius.toC()
	o.padding = p.Padding.toC()
	return o
}

func panelPaintFromC(o C.n8v_panel_paint) PanelPaint {
	return PanelPaint{
		Background:   rgbaFromC(o.background),
		BorderColor:  rgbaFromC(o.border_color),
		BorderWidth:  float64(o.border_width),
		CornerRadius: cornersFromC(o.corner_radius),
		Padding:      insetsFromC(o.padding),
	}
}

// CustomPaint draws the widgets of the Custom style. Leave a field nil to keep the Plain style's look for that widget.
// Only the SDL2 and HTML backends use it; native toolkits draw their own widgets. Build one from BuiltinPaint to change just a few.
type CustomPaint struct {
	Button           func(style ButtonStyle, hovered bool, pressed bool) ButtonPaint
	Text             func(options TextPaintOptions) TextPaint
	Checkbox         func(checked bool, hovered bool, pressed bool) CheckboxPaint
	Radio            func(selected bool, hovered bool, pressed bool) RadioPaint
	Toggle           func(on bool, hovered bool, pressed bool) TogglePaint
	Entry            func(focused bool, hasValue bool) EntryPaint
	Dropdown         func(open bool, hasSelection bool, hovered bool, pressed bool) DropdownPaint
	Slider           func(hovered bool, pressed bool) SliderPaint
	Image            func() ImagePaint
	Icon             func() IconPaint
	Sidebar          func() SidebarPaint
	Panel            func(role PanelRole, hovered bool, pressed bool) PanelPaint
	WindowBackground func() Rgba
}

var customPaint cgo.Handle

// SetCustomPaint installs p and switches to the Custom style.
func SetCustomPaint(p CustomPaint) {
	h := cgo.NewHandle(&p)
	var vt C.n8v_custom_paint_vtable
	if p.Button != nil {
		vt.button = C.n8v_paint_button_fn(C.n8v_go_paint_button)
	}
	if p.Text != nil {
		vt.text = C.n8v_paint_text_fn(C.n8v_go_paint_text)
	}
	if p.Checkbox != nil {
		vt.checkbox = C.n8v_paint_checkbox_fn(C.n8v_go_paint_checkbox)
	}
	if p.Radio != nil {
		vt.radio = C.n8v_paint_radio_fn(C.n8v_go_paint_radio)
	}
	if p.Toggle != nil {
		vt.toggle = C.n8v_paint_toggle_fn(C.n8v_go_paint_toggle)
	}
	if p.Entry != nil {
		vt.entry = C.n8v_paint_entry_fn(C.n8v_go_paint_entry)
	}
	if p.Dropdown != nil {
		vt.dropdown = C.n8v_paint_dropdown_fn(C.n8v_go_paint_dropdown)
	}
	if p.Slider != nil {
		vt.slider = C.n8v_paint_slider_fn(C.n8v_go_paint_slider)
	}
	if p.Image != nil {
		vt.image = C.n8v_paint_image_fn(C.n8v_go_paint_image)
	}
	if p.Icon != nil {
		vt.icon = C.n8v_paint_icon_fn(C.n8v_go_paint_icon)
	}
	if p.Sidebar != nil {
		vt.sidebar = C.n8v_paint_sidebar_fn(C.n8v_go_paint_sidebar)
	}
	if p.Panel != nil {
		vt.panel = C.n8v_paint_panel_fn(C.n8v_go_paint_panel)
	}
	if p.WindowBackground != nil {
		vt.window_background = C.n8v_paint_window_background_fn(C.n8v_go_paint_window_background)
	}
	C.n8v_set_custom_paint(&vt, C.n8v_go_paint_handle(C.uintptr_t(h)))
	if customPaint != 0 {
		customPaint.Delete()
	}
	customPaint = h
}

func paintOf(userdata unsafe.Pointer) *CustomPaint {
	return cgo.Handle(uintptr(userdata)).Value().(*CustomPaint)
}

// BuiltinPaint is the paint of one of the bundled styles (Plain, Material, Cupertino or Fluent), whichever is active. A custom style
// calls it for the widgets, or the parts of a widget, it does not want to change. WindowBackground is nil: the bundled styles have no query for it.
func BuiltinPaint(family Style) CustomPaint {
	f := C.n8v_style_family(family)
	return CustomPaint{
		Button: func(style ButtonStyle, hovered bool, pressed bool) ButtonPaint {

			return buttonPaintFromC(C.n8v_style_button_paint(f, C.n8v_button_style(style), C.bool(hovered), C.bool(pressed)))
		},
		Text: func(options TextPaintOptions) TextPaint {
			c, free := options.toC()
			defer free()
			return textPaintFromC(C.n8v_style_text_paint(f, c))
		},
		Checkbox: func(checked bool, hovered bool, pressed bool) CheckboxPaint {

			return checkboxPaintFromC(C.n8v_style_checkbox_paint(f, C.bool(checked), C.bool(hovered), C.bool(pressed)))
		},
		Radio: func(selected bool, hovered bool, pressed bool) RadioPaint {

			return radioPaintFromC(C.n8v_style_radio_paint(f, C.bool(selected), C.bool(hovered), C.bool(pressed)))
		},
		Toggle: func(on bool, hovered bool, pressed bool) TogglePaint {

			return togglePaintFromC(C.n8v_style_toggle_paint(f, C.bool(on), C.bool(hovered), C.bool(pressed)))
		},
		Entry: func(focused bool, hasValue bool) EntryPaint {

			return entryPaintFromC(C.n8v_style_entry_paint(f, C.bool(focused), C.bool(hasValue)))
		},
		Dropdown: func(open bool, hasSelection bool, hovered bool, pressed bool) DropdownPaint {

			return dropdownPaintFromC(C.n8v_style_dropdown_paint(f, C.bool(open), C.bool(hasSelection), C.bool(hovered), C.bool(pressed)))
		},
		Slider: func(hovered bool, pressed bool) SliderPaint {

			return sliderPaintFromC(C.n8v_style_slider_paint(f, C.bool(hovered), C.bool(pressed)))
		},
		Image: func() ImagePaint {

			return imagePaintFromC(C.n8v_style_image_paint(f))
		},
		Icon: func() IconPaint {

			return iconPaintFromC(C.n8v_style_icon_paint(f))
		},
		Sidebar: func() SidebarPaint {

			return sidebarPaintFromC(C.n8v_style_sidebar_paint(f))
		},
		Panel: func(role PanelRole, hovered bool, pressed bool) PanelPaint {

			return panelPaintFromC(C.n8v_style_panel_paint(f, C.n8v_panel_role(role), C.bool(hovered), C.bool(pressed)))
		},
	}
}
