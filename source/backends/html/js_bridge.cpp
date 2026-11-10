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

EM_JS(void, n8vHtmlPickFiles, (int id, int multiple, int folder, const char *accept), {
  var input = document.createElement('input');
  input.type = 'file';
  input.multiple = !!multiple;
  if (folder) input.webkitdirectory = true;
  var acceptStr = UTF8ToString(accept);
  if (acceptStr) input.accept = acceptStr;
  input.style.display = 'none';
  document.body.appendChild(input);

  var finished = false;
  function finish(paths) {
    if (finished) return;
    finished = true;
    if (input.parentNode) input.parentNode.removeChild(input);
    Module.ccall('n8v_html_file_dialog_done', null, ['number', 'string'], [id, paths.join('\n')]);
  }

  input.addEventListener('cancel', function() { finish([]); });
  input.addEventListener('change', function() {
    var files = Array.prototype.slice.call(input.files || []);
    if (!files.length) { finish([]); return; }
    var root = '/n8v-uploads/' + id;
    var roots = {};
    Promise.all(files.map(function(file) {
      var rel = (folder && file.webkitRelativePath) ? file.webkitRelativePath : file.name;
      var full = root + '/' + rel;
      return file.arrayBuffer().then(function(buf) {
        FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
        FS.writeFile(full, new Uint8Array(buf));
        roots[folder ? root + '/' + rel.split('/')[0] : full] = true;
      });
    })).then(function() { finish(Object.keys(roots)); }, function() { finish([]); });
  });
  input.click();
});
// clang-format on

extern "C" void n8vHtmlInstallListenersBridge() { n8vHtmlInstallListeners(); }
extern "C" void n8vHtmlPickFilesBridge(int id, int multiple, int folder, const char *accept) { n8vHtmlPickFiles(id, multiple, folder, accept); }
