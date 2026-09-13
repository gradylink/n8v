#include "html_backend_impl.hpp"

#include <emscripten.h>

namespace n8v::detail {
namespace {

HtmlBackend *g_instance = nullptr;

} // namespace

void n8vHtmlSetInstance(HtmlBackend *backend) { g_instance = backend; }

HtmlBackend *n8vHtmlInstance() { return g_instance; }

} // namespace n8v::detail

extern "C" {

EMSCRIPTEN_KEEPALIVE void n8v_html_pointer_move(float x, float y) {
  if (n8v::detail::HtmlBackend *b = n8v::detail::n8vHtmlInstance()) b->onPointerMove(x, y);
}

EMSCRIPTEN_KEEPALIVE void n8v_html_pointer_down(int down) {
  if (n8v::detail::HtmlBackend *b = n8v::detail::n8vHtmlInstance()) b->onPointerDown(down != 0);
}

EMSCRIPTEN_KEEPALIVE void n8v_html_entry_input(int ordinal, const char *value) {
  if (n8v::detail::HtmlBackend *b = n8v::detail::n8vHtmlInstance()) b->onEntryInput(ordinal, value ? value : "");
}

} // extern "C"

// clang-format off
EM_JS(void, n8vHtmlInstallListeners, (), {
  function reportPos(e) {
    var x = (e.clientX !== undefined) ? e.clientX : (e.touches && e.touches.length ? e.touches[0].clientX : 0);
    var y = (e.clientY !== undefined) ? e.clientY : (e.touches && e.touches.length ? e.touches[0].clientY : 0);
    Module.ccall('n8v_html_pointer_move', null, ['number', 'number'], [x, y]);
  }
  window.addEventListener('mousemove', reportPos, {passive: true});
  window.addEventListener('mousedown', function(e) {
    if (e.button !== 0) return;
    reportPos(e);
    Module.ccall('n8v_html_pointer_down', null, ['number'], [1]);
  });
  window.addEventListener('mouseup', function(e) {
    if (e.button !== 0) return;
    Module.ccall('n8v_html_pointer_down', null, ['number'], [0]);
  });
  window.addEventListener('touchstart', function(e) {
    reportPos(e);
    Module.ccall('n8v_html_pointer_down', null, ['number'], [1]);
  }, {passive: true});
  window.addEventListener('touchmove', reportPos, {passive: true});
  window.addEventListener('touchend', function(e) {
    Module.ccall('n8v_html_pointer_down', null, ['number'], [0]);
  }, {passive: true});
  document.addEventListener('input', function(e) {
    var el = e.target;
    if (el && el.tagName === 'INPUT' && el.hasAttribute('data-n8v-ordinal')) {
      var ordinal = parseInt(el.getAttribute('data-n8v-ordinal'), 10);
      Module.ccall('n8v_html_entry_input', null, ['number', 'string'], [ordinal, el.value]);
    }
  });
  document.addEventListener('click', function(e) {
    if (e.target.closest('a[data-n8v-link]')) e.preventDefault();
  });
});
// clang-format on
