#include <emscripten.h>

// clang-format off
EM_JS(void, n8vHtmlInstallListeners, (), {
  function send(ordinal, type, value) {
    Module.ccall('n8v_html_event', null, ['number', 'string', 'string'], [ordinal, type, value]);
  }
  function ordinalOf(el) { return parseInt(el.getAttribute('data-n8v'), 10); }

  Module.n8vActivity = 0;
  function bump() { Module.n8vActivity++; }
  ['click', 'mousedown', 'mouseup', 'mouseover', 'mouseout', 'wheel', 'scroll', 'keydown', 'keyup', 'input', 'change', 'paste', 'focusin', 'focusout', 'touchstart', 'touchmove', 'touchend', 'dragover', 'drop', 'contextmenu'].forEach(function(type) {
    document.addEventListener(type, bump, {capture: true, passive: true});
  });
  window.addEventListener('resize', bump);

  document.addEventListener('click', function(e) {
    var el = e.target.closest ? e.target.closest('[data-n8v-click]') : null;
    if (!el) return;
    e.preventDefault();
    send(ordinalOf(el), 'click', '');
  });

  document.addEventListener('click', function(e) {
    if (e.defaultPrevented || e.button != 0 || e.metaKey || e.ctrlKey || e.shiftKey || e.altKey) return;
    var a = e.target.closest ? e.target.closest('a[href]') : null;
    if (!a || !a.closest('#n8v-root')) return;
    e.preventDefault();
    Module.ccall('n8v_html_open_url', null, ['string'], [a.getAttribute('href')]);
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
    } else if (el.tagName === 'TEXTAREA') {
      window.n8vAutoGrow(el);
      send(ordinalOf(el), 'input', el.value);
    }
  });

  window.n8vAutoGrow = function(el) {
    if (!el.hasAttribute('data-n8v-grow')) return;
    el.style.height = 'auto';
    el.style.height = (el.scrollHeight + el.offsetHeight - el.clientHeight) + 'px';
  };

  document.addEventListener('paste', function(e) {
    var el = e.target;
    if (!el.hasAttribute || !el.hasAttribute('data-n8v') || !e.clipboardData) return;
    var files = Array.prototype.slice.call(e.clipboardData.files || []);
    if (!files.length || e.clipboardData.getData('text')) return;
    e.preventDefault();
    var dir = '/n8v-paste';
    Promise.all(files.map(function(file, i) {
      return file.arrayBuffer().then(function(buf) {
        var name = file.name || ('image.' + ((file.type || 'image/png').split('/')[1] || 'png'));
        var full = dir + '/' + Date.now() + '-' + i + '-' + name;
        FS.mkdirTree(dir);
        FS.writeFile(full, new Uint8Array(buf));
        return full;
      });
    })).then(function(paths) { Module.ccall('n8v_html_paste_files', null, ['number', 'string'], [ordinalOf(el), paths.join('\n')]); });
  });

  document.addEventListener('keydown', function(e) {
    var el = e.target;
    if (el.hasAttribute && el.hasAttribute('data-n8v') && (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT') && !e.isComposing) {
      var keyCode = e.key === 'ArrowUp' ? 0 : e.key === 'ArrowDown' ? 1 : e.key === 'Escape' ? 2 : -1;
      if (keyCode >= 0) {
        var mods = (e.shiftKey ? 1 : 0) | (e.ctrlKey ? 2 : 0) | (e.altKey ? 4 : 0);
        if (Module.ccall('n8v_html_key', 'number', ['number', 'number', 'number'], [ordinalOf(el), keyCode, mods])) {
          e.preventDefault();
          return;
        }
      }
    }
    if (e.key === 'Enter' && el.tagName === 'INPUT' && el.type !== 'checkbox' && el.type !== 'radio' && el.hasAttribute('data-n8v')) {
      send(ordinalOf(el), 'submit', '');
    } else if (e.key === 'Enter' && el.tagName === 'TEXTAREA' && el.hasAttribute('data-n8v') && !e.isComposing) {
      var ctrlMode = el.getAttribute('data-n8v-submit') === 'ctrl';
      if (ctrlMode ? (e.ctrlKey || e.metaKey) : !e.shiftKey) {
        e.preventDefault();
        send(ordinalOf(el), 'submit', '');
      }
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

EM_JS_DEPS(n8v_html_media, "$UTF8ToString,$stringToNewUTF8");
EM_JS(char *, n8vHtmlMediaUrl, (const char *path), {
  var p = UTF8ToString(path);
  if (/^(https?|blob|data):/i.test(p)) return stringToNewUTF8(p);
  var store = Module.n8vMediaUrls || (Module.n8vMediaUrls = {});
  try {
  var stat = FS.stat(p);
  var key = stat.size + ':' + stat.mtime.getTime();
  if (store[p] && store[p].key == key) return stringToNewUTF8(store[p].url);
  if (store[p]) URL.revokeObjectURL(store[p].url);
  var bytes = FS.readFile(p);
  var type = '';
  var ext = p.lastIndexOf('.') >= 0 ? p.substring(p.lastIndexOf('.') + 1).toLowerCase() : '';
  var byExtension = {mp4: 'video/mp4', m4v: 'video/mp4', webm: 'video/webm', mov: 'video/quicktime', ogv: 'video/ogg', mp3: 'audio/mpeg', wav: 'audio/wav', ogg: 'audio/ogg', oga: 'audio/ogg', m4a: 'audio/mp4', flac: 'audio/flac', opus: 'audio/ogg'};
  if (byExtension[ext]) type = byExtension[ext];
  if (!type && bytes.length > 12) {
    var head = String.fromCharCode.apply(null, bytes.subarray(0, 12));
    if (head.substring(4, 8) == 'ftyp') type = head.substring(8, 12) == 'qt  ' ? 'video/quicktime' : 'video/mp4';
    else if (bytes[0] == 0x1A && bytes[1] == 0x45 && bytes[2] == 0xDF && bytes[3] == 0xA3) type = 'video/webm';
    else if (head.substring(0, 4) == 'OggS') type = 'audio/ogg';
    else if (head.substring(0, 4) == 'RIFF' && head.substring(8, 12) == 'WAVE') type = 'audio/wav';
    else if (head.substring(0, 3) == 'ID3' || (bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0)) type = 'audio/mpeg';
    else if (head.substring(0, 4) == 'fLaC') type = 'audio/flac';
  }
  var url = URL.createObjectURL(new Blob([bytes], {type: type}));
  store[p] = {key: key, url: url};
  return stringToNewUTF8(url);
  } catch (e) {
    return stringToNewUTF8(p);
  }
});

EM_JS(int, n8vHtmlMessage, (const char *text, int confirmDialog), {
  var message = UTF8ToString(text);
  if (confirmDialog) return window.confirm(message) ? 1 : 0;
  window.alert(message);
  return 1;
});
// clang-format on

extern "C" char *n8vHtmlMediaUrlBridge(const char *path) { return n8vHtmlMediaUrl(path); }
extern "C" int n8vHtmlMessageBridge(const char *text, int confirm) { return n8vHtmlMessage(text, confirm); }
extern "C" void n8vHtmlInstallListenersBridge() { n8vHtmlInstallListeners(); }
extern "C" void n8vHtmlPickFilesBridge(int id, int multiple, int folder, const char *accept) { n8vHtmlPickFiles(id, multiple, folder, accept); }
