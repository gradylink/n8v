#include "notification.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on

#include <cctype>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace n8v::detail {
namespace {

constexpr UINT kTrayMessage = WM_APP + 0x4e8;
constexpr wchar_t kWindowClass[] = L"n8v_notification_window";
constexpr std::chrono::seconds kExpiry{30};

struct Entry {
  UINT iconId = 0;
  std::string id;
  NotificationDone done;
  HICON ownedIcon = nullptr;
  std::chrono::steady_clock::time_point deadline;
};

struct Event {
  UINT iconId;
  bool clicked;
  bool finished;
};

HWND window = nullptr;
UINT nextIconId = 1;
std::wstring appName = L"n8v";
std::vector<Entry> entries;
std::deque<Event> events;

std::wstring widen(const std::string &utf8) {
  if (utf8.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  if (n <= 0) return {};
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

template <size_t N> void copyTruncated(wchar_t (&dest)[N], const std::wstring &text) {
  if (text.size() < N) {
    std::wmemcpy(dest, text.c_str(), text.size() + 1);
    return;
  }
  size_t len = N - 2;
  if (len > 0 && text[len - 1] >= 0xD800 && text[len - 1] <= 0xDBFF) --len;
  std::wmemcpy(dest, text.c_str(), len);
  dest[len] = L'\u2026';
  dest[len + 1] = L'\0';
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == kTrayMessage) {
    const UINT iconId = HIWORD(lParam);
    switch (LOWORD(lParam)) {
    case NIN_BALLOONUSERCLICK:
      events.push_back({iconId, true, true});
      break;
    case NIN_BALLOONTIMEOUT:
      events.push_back({iconId, false, true});
      break;
    default:
      break;
    }
    return 0;
  }
  return DefWindowProcW(hwnd, message, wParam, lParam);
}

bool ensureWindow() {
  if (window) return true;
  const HINSTANCE instance = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = windowProc;
  wc.hInstance = instance;
  wc.lpszClassName = kWindowClass;
  if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  window = CreateWindowExW(WS_EX_TOOLWINDOW, kWindowClass, L"n8v notifications", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
  return window != nullptr;
}

NOTIFYICONDATAW baseData(UINT iconId) {
  NOTIFYICONDATAW data{};
  data.cbSize = sizeof(data);
  data.hWnd = window;
  data.uID = iconId;
  return data;
}

HICON fallbackIcon() {
  HWND owner = GetActiveWindow();
  if (!owner) owner = GetForegroundWindow();
  if (owner) {
    DWORD pid = 0;
    GetWindowThreadProcessId(owner, &pid);
    if (pid == GetCurrentProcessId()) {
      DWORD_PTR result = 0;
      if (SendMessageTimeoutW(owner, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &result) && result) return reinterpret_cast<HICON>(result);
      if (HICON icon = reinterpret_cast<HICON>(GetClassLongPtrW(owner, GCLP_HICON))) return icon;
    }
  }
  return LoadIconW(nullptr, MAKEINTRESOURCEW(32512)); // IDI_APPLICATION
}

HICON loadIconFile(const std::string &path) {
  if (path.size() < 5) return nullptr;
  std::string ext = path.substr(path.size() - 4);
  for (char &c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (ext != ".ico") return nullptr;
  return static_cast<HICON>(LoadImageW(nullptr, widen(path).c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE));
}

Entry *findById(const std::string &id) {
  if (id.empty()) return nullptr;
  for (Entry &e : entries)
    if (e.id == id) return &e;
  return nullptr;
}

void removeEntry(size_t index, bool deleteIcon) {
  Entry &entry = entries[index];
  if (deleteIcon && window) {
    NOTIFYICONDATAW data = baseData(entry.iconId);
    Shell_NotifyIconW(NIM_DELETE, &data);
  }
  if (entry.ownedIcon) DestroyIcon(entry.ownedIcon);
  entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(index));
}

void finish(size_t index, bool clicked) {
  NotificationDone done = std::move(entries[index].done);
  removeEntry(index, true);
  if (done) done(clicked);
}

} // namespace

bool platformNotificationsAvailable() { return true; }

void setPlatformNotificationApp(const std::string &name, const std::string &) {
  const std::wstring wide = widen(name);
  if (!wide.empty()) appName = wide;
}

bool platformNotify(const NotificationRequest &request, NotificationDone done) {
  if (!ensureWindow()) return false;

  Entry *existing = findById(request.id);
  const bool replacing = existing != nullptr;
  Entry fresh;
  Entry &target = replacing ? *existing : fresh;
  NotificationDone superseded;
  if (replacing) {
    superseded = std::move(target.done);
  } else {
    target.iconId = nextIconId++;
    target.id = request.id;
  }
  target.done = std::move(done);
  target.deadline = std::chrono::steady_clock::now() + kExpiry;

  if (HICON loaded = loadIconFile(request.icon)) {
    if (target.ownedIcon) DestroyIcon(target.ownedIcon);
    target.ownedIcon = loaded;
  }

  NOTIFYICONDATAW data = baseData(target.iconId);
  data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE | NIF_STATE | NIF_INFO | NIF_SHOWTIP;
  data.uCallbackMessage = kTrayMessage;
  data.hIcon = target.ownedIcon ? target.ownedIcon : fallbackIcon();
  data.dwState = NIS_HIDDEN;
  data.dwStateMask = NIS_HIDDEN;
  copyTruncated(data.szTip, appName);
  copyTruncated(data.szInfoTitle, widen(request.title));
  copyTruncated(data.szInfo, widen(request.body));

  data.dwInfoFlags = NIIF_RESPECT_QUIET_TIME;
  if (request.silent || request.urgency == NotificationUrgency::Low) data.dwInfoFlags |= NIIF_NOSOUND;
  if (target.ownedIcon) {
    data.dwInfoFlags |= NIIF_USER | NIIF_LARGE_ICON;
    data.hBalloonIcon = target.ownedIcon;
  } else {
    data.dwInfoFlags |= request.urgency == NotificationUrgency::Critical ? NIIF_WARNING : NIIF_INFO;
  }
  if (data.szInfoTitle[0] == L'\0' && data.szInfo[0] == L'\0') copyTruncated(data.szInfoTitle, appName);

  const UINT iconId = target.iconId;
  if (!replacing) {
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
      if (fresh.ownedIcon) DestroyIcon(fresh.ownedIcon);
      return false;
    }
    NOTIFYICONDATAW version = baseData(iconId);
    version.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &version);
    entries.push_back(std::move(fresh));
  }
  if (!Shell_NotifyIconW(NIM_MODIFY, &data)) {
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].iconId == iconId) {
        removeEntry(i, true);
        break;
      }
    }
    return false;
  }

  if (superseded) superseded(false);
  return true;
}

void platformCloseNotification(const std::string &id) {
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!id.empty() && entries[i].id == id) {
      finish(i, false);
      return;
    }
  }
}

void pollPlatformNotifications() {
  if (!window) return;

  MSG msg;
  while (PeekMessageW(&msg, window, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  while (!events.empty()) {
    const Event event = events.front();
    events.pop_front();
    for (size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].iconId == event.iconId) {
        finish(i, event.clicked);
        break;
      }
    }
  }

  const auto now = std::chrono::steady_clock::now();
  for (size_t i = 0; i < entries.size();) {
    if (now >= entries[i].deadline) finish(i, false);
    else ++i;
  }
}

} // namespace n8v::detail
