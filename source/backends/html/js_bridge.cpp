#include <emscripten.h>

// clang-format off
EM_JS(void, n8vHtmlInstallListeners, (), {
  function send(ordinal, type, value) {
    Module.ccall('n8v_html_event', null, ['number', 'string', 'string'], [ordinal, type, value]);
  }
  function ordinalOf(el) { return parseInt(el.getAttribute('data-n8v'), 10); }

  document.addEventListener('click', function(e) {
    var el = e.target.closest ? e.target.closest('[data-n8v-click]') : null;
    if (!el) return;
    e.preventDefault();
    send(ordinalOf(el), 'click', '');
  });

  document.addEventListener('change', function(e) {
    var el = e.target;
    if (!el.hasAttribute || !el.hasAttribute('data-n8v')) return;
    var value = el.value;
    if (el.type === 'checkbox' || el.type === 'radio') value = el.checked ? '1' : '0';
    else if (el.tagName === 'SELECT') value = String(el.selectedIndex);
    send(ordinalOf(el), 'change', value);
  });

  document.addEventListener('input', function(e) {
    var el = e.target;
    if (!el.hasAttribute || !el.hasAttribute('data-n8v')) return;
    if (el.type === 'range') {
      var min = parseFloat(el.min), max = parseFloat(el.max);
      var pct = max > min ? ((parseFloat(el.value) - min) / (max - min)) * 100 : 0;
      el.style.setProperty('--n8v-p', pct + '%');
      send(ordinalOf(el), 'input', el.value);
    } else if (el.tagName === 'INPUT') {
      send(ordinalOf(el), 'input', el.value);
    }
  });

  document.addEventListener('keydown', function(e) {
    var el = e.target;
    if (e.key === 'Enter' && el.tagName === 'INPUT' && el.type !== 'checkbox' && el.type !== 'radio' && el.hasAttribute('data-n8v')) {
      send(ordinalOf(el), 'submit', '');
    }
  });

  function watchedChain(el) {
    var out = [];
    for (var node = el && el.closest ? el.closest('[data-n8v-hover]') : null; node; node = node.parentElement ? node.parentElement.closest('[data-n8v-hover]') : null) out.push(node);
    return out;
  }
  document.addEventListener('mouseover', function(e) {
    watchedChain(e.target).forEach(function(node) {
      if (!e.relatedTarget || !node.contains(e.relatedTarget)) send(parseInt(node.getAttribute('data-n8v-hover'), 10), 'hover', '1');
    });
  });
  document.addEventListener('mouseout', function(e) {
    watchedChain(e.target).forEach(function(node) {
      if (!e.relatedTarget || !node.contains(e.relatedTarget)) send(parseInt(node.getAttribute('data-n8v-hover'), 10), 'hover', '0');
    });
  });
});
// clang-format on

extern "C" void n8vHtmlInstallListenersBridge() { n8vHtmlInstallListeners(); }
