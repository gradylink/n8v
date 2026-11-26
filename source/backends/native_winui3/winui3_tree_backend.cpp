#include "winui3_tree_backend.hpp"

#include "core/dialogs/file_dialog.hpp"
#include "core/dialogs/message_dialog.hpp"
#include "core/entry_lines.hpp"
#include "core/icon_loader.hpp"
#include "core/image_loader.hpp"
#include "core/markdown/image_state.hpp"
#include "core/markdown/markdown.hpp"
#include "core/open_url.hpp"
#include "core/paste_file.hpp"
#include "core/platform/utf8_path.hpp"
#include "core/style.hpp"
#include "core/theme_color.hpp"
#include "core/tree/dispatch.hpp"
#include "core/tree/persistent.hpp"
#include "core/tree/props.hpp"
#include "core/tree/reconcile.hpp"
#include "core/widgets/page_badge.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <deque>
#include <functional>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <unknwn.h>
#undef GetCurrentTime

#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Media.Core.h>
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Documents.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Interop.h>
// clang-format on

namespace n8v::detail {
namespace {

namespace wf = winrt::Windows::Foundation;
namespace wu = winrt::Windows::UI;
namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;
namespace muxp = winrt::Microsoft::UI::Xaml::Controls::Primitives;
namespace muxd = winrt::Microsoft::UI::Xaml::Documents;
namespace muxm = winrt::Microsoft::UI::Xaml::Media;

using tree::BoundValue;
using tree::OwnedProps;
using tree::PNode;
namespace dispatch = tree::dispatch;

template <typename... Ts> struct Overloaded : Ts... {
  using Ts::operator()...;
};
template <typename... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;

template <typename T> const T *propsOf(const PNode &node) { return std::get_if<T>(&node.current.props); }

constexpr int kDefaultIconSize = 16;
constexpr int kBusyFrameMs = 16;
constexpr int kIdleFrameMs = 250;
constexpr int kSettleFrames = 4;
constexpr double kStickyBottomEpsilon = 24.0;
constexpr std::chrono::milliseconds kScrollSettleTime{50};
constexpr std::chrono::milliseconds kScrollRequestTimeout{2000};
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr UINT kWakeMessage = WM_APP + 1;

winrt::hstring hs(std::string_view s) { return winrt::to_hstring(s); }
winrt::hstring hs(const char *s) { return s ? winrt::to_hstring(std::string_view(s)) : winrt::hstring(); }
std::string utf8(const winrt::hstring &s) { return winrt::to_string(s); }

void warnOnce(bool &flag, const char *message) {
  if (flag) return;
  flag = true;
  std::fprintf(stderr, "[n8v] winui3: %s\n", message);
}

uint8_t channel(float v) { return (uint8_t)std::clamp((int)std::lround(v), 0, 255); }
wu::Color colorOf(n8v_color c) { return wu::Color{channel(c.a), channel(c.r), channel(c.g), channel(c.b)}; }
muxm::SolidColorBrush brushOf(n8v_color c) { return muxm::SolidColorBrush(colorOf(c)); }

mux::Thickness thickness(const n8v_padding &p) { return mux::Thickness{(double)p.left, (double)p.top, (double)p.right, (double)p.bottom}; }

mux::HorizontalAlignment hAlignOf(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return mux::HorizontalAlignment::Center;
  case N8V_ALIGN_END:
    return mux::HorizontalAlignment::Right;
  case N8V_ALIGN_START:
    break;
  }
  return mux::HorizontalAlignment::Left;
}

mux::VerticalAlignment vAlignOf(n8v_align a) {
  switch (a) {
  case N8V_ALIGN_CENTER:
    return mux::VerticalAlignment::Center;
  case N8V_ALIGN_END:
    return mux::VerticalAlignment::Bottom;
  case N8V_ALIGN_START:
    break;
  }
  return mux::VerticalAlignment::Top;
}

void attachFraction(n8v_attach_point p, double &fx, double &fy) {
  switch (p) {
  case N8V_ATTACH_POINT_LEFT_TOP:
    fx = 0.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_LEFT_CENTER:
    fx = 0.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_LEFT_BOTTOM:
    fx = 0.0, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_CENTER_TOP:
    fx = 0.5, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_CENTER_CENTER:
    fx = 0.5, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_CENTER_BOTTOM:
    fx = 0.5, fy = 1.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_TOP:
    fx = 1.0, fy = 0.0;
    break;
  case N8V_ATTACH_POINT_RIGHT_CENTER:
    fx = 1.0, fy = 0.5;
    break;
  case N8V_ATTACH_POINT_RIGHT_BOTTOM:
    fx = 1.0, fy = 1.0;
    break;
  }
}

struct PackageVersion {
  UINT64 version;
};
using BootstrapInitialize2 = HRESULT(WINAPI *)(UINT32 majorMinorVersion, PCWSTR versionTag, PackageVersion minVersion, UINT32 options);
using BootstrapShutdown = void(WINAPI *)();

constexpr UINT32 kWindowsAppSdkMajorMinor = 0x00010008;

bool hasPackageIdentity() {
  using GetCurrentPackageFullNameFn = LONG(WINAPI *)(UINT32 *, PWSTR);
  HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
  auto *fn = kernel ? reinterpret_cast<GetCurrentPackageFullNameFn>(GetProcAddress(kernel, "GetCurrentPackageFullName")) : nullptr;
  if (!fn) return false;
  UINT32 length = 0;
  return fn(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

void offerRuntimeDownload(const wchar_t *problem, HRESULT hr) {
  if (std::getenv("N8V_WINUI3_NO_PROMPT")) return;
  wchar_t text[768];
  std::swprintf(
    text,
    sizeof text / sizeof text[0],
    L"%ls (error 0x%08lX).\n\n"
    L"This app draws its window with WinUI 3, which needs the Windows App Runtime 1.8.\n\n"
    L"Yes: open the download page, install the runtime for your PC, then start the app again.\n"
    L"No: continue now with the basic look.",
    problem,
    (unsigned long)hr
  );
  const int choice = MessageBoxW(nullptr, text, L"Windows App Runtime required", MB_YESNO | MB_ICONINFORMATION | MB_SETFOREGROUND);
  if (choice == IDYES) ShellExecuteW(nullptr, L"open", L"https://learn.microsoft.com/windows/apps/windows-app-sdk/downloads", nullptr, nullptr, SW_SHOWNORMAL);
}

struct Bootstrap {
  HMODULE module = nullptr;
  BootstrapShutdown shutdown = nullptr;

  static HMODULE load() { return LoadLibraryW(L"Microsoft.WindowsAppRuntime.Bootstrap.dll"); }

  bool initialize() {
    if (hasPackageIdentity()) return true;
    module = load();
    if (!module) {
      std::fprintf(stderr, "[n8v] winui3: Microsoft.WindowsAppRuntime.Bootstrap.dll was not found next to the executable\n");
      offerRuntimeDownload(L"Microsoft.WindowsAppRuntime.Bootstrap.dll is missing next to the program", HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
      return false;
    }
    auto init = reinterpret_cast<BootstrapInitialize2>(GetProcAddress(module, "MddBootstrapInitialize2"));
    shutdown = reinterpret_cast<BootstrapShutdown>(GetProcAddress(module, "MddBootstrapShutdown"));
    if (!init) return false;
    const HRESULT hr = init(kWindowsAppSdkMajorMinor, L"", PackageVersion{0}, 0);
    if (FAILED(hr)) {
      std::fprintf(stderr, "[n8v] winui3: the Windows App SDK 1.8 runtime could not be loaded (0x%08lx)\n", (unsigned long)hr);
      offerRuntimeDownload(L"The Windows App Runtime 1.8 is not installed", hr);
      return false;
    }
    return true;
  }

  void close() {
    if (shutdown) shutdown();
    shutdown = nullptr;
    if (module) FreeLibrary(module);
    module = nullptr;
  }
};

LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;

LONG WINAPI crashReporter(EXCEPTION_POINTERS *info) {
  const auto address = (uintptr_t)info->ExceptionRecord->ExceptionAddress;
  HMODULE module = nullptr;
  char path[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module))
    GetModuleFileNameA(module, path, sizeof path);
  std::fprintf(
    stderr,
    "[n8v] winui3: CRASH: exception 0x%08lx at %s+0x%llx (thread %lu)\n",
    (unsigned long)info->ExceptionRecord->ExceptionCode,
    path,
    (unsigned long long)(address - (uintptr_t)module),
    GetCurrentThreadId()
  );
  std::fflush(stderr);
  return g_previousFilter ? g_previousFilter(info) : EXCEPTION_CONTINUE_SEARCH;
}

std::function<void()> g_onLaunched;

bool g_trace = false;

void trace(const char *what) {
  static const auto origin = std::chrono::steady_clock::now();
  if (g_trace) std::fprintf(stderr, "[n8v] winui3: [%6.0f ms] %s\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - origin).count(), what);
}

struct N8VApp : mux::ApplicationT<N8VApp, mux::Markup::IXamlMetadataProvider> {
  N8VApp() { trace("application constructor"); }

  void OnLaunched(mux::LaunchActivatedEventArgs const &) {
    trace("OnLaunched called");
    try {
      Resources().MergedDictionaries().Append(muxc::XamlControlsResources());
      trace("control styles merged");
    } catch (winrt::hresult_error const &e) {
      std::fprintf(
        stderr,
        "[n8v] winui3: could not load the control styles (%s, 0x%08x); controls will be plain\n",
        winrt::to_string(e.message()).c_str(),
        (unsigned)e.code().value
      );
    }
    if (g_onLaunched) g_onLaunched();
  }

  mux::Markup::IXamlType GetXamlType(wu::Xaml::Interop::TypeName const &type) { return provider().GetXamlType(type); }
  mux::Markup::IXamlType GetXamlType(winrt::hstring const &fullName) { return provider().GetXamlType(fullName); }
  winrt::com_array<mux::Markup::XmlnsDefinition> GetXmlnsDefinitions() { return provider().GetXmlnsDefinitions(); }

private:
  mux::XamlTypeInfo::XamlControlsXamlMetaDataProvider &provider() {
    if (!provider_) provider_ = mux::XamlTypeInfo::XamlControlsXamlMetaDataProvider();
    return provider_;
  }

  mux::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider_{nullptr};
};

muxm::Imaging::SoftwareBitmapSource bitmapSourceOf(const DecodedImage &image) {
  const size_t count = (size_t)image.width * (size_t)image.height;
  std::vector<uint8_t> bgra(count * 4);
  for (size_t i = 0; i < count; ++i) {
    const uint8_t *in = image.rgba + i * 4;
    const unsigned alpha = in[3];
    bgra[i * 4 + 0] = (uint8_t)((in[2] * alpha + 127) / 255);
    bgra[i * 4 + 1] = (uint8_t)((in[1] * alpha + 127) / 255);
    bgra[i * 4 + 2] = (uint8_t)((in[0] * alpha + 127) / 255);
    bgra[i * 4 + 3] = (uint8_t)alpha;
  }
  winrt::Windows::Storage::Streams::DataWriter writer;
  writer.WriteBytes(bgra);
  auto bitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::CreateCopyFromBuffer(
    writer.DetachBuffer(),
    winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,
    image.width,
    image.height,
    winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied
  );
  muxm::Imaging::SoftwareBitmapSource source;
  source.SetBitmapAsync(bitmap);
  return source;
}

CornerRadius cornerRadiusFor(const n8v_rounding &rounding) {
  switch (rounding.mode) {
  case N8V_ROUNDING_STYLE_DEFAULT:
    return activePaint().image().cornerRadius;
  case N8V_ROUNDING_NONE:
    return {};
  case N8V_ROUNDING_FIXED:
    break;
  }
  return {rounding.radius.top_left, rounding.radius.top_right, rounding.radius.bottom_left, rounding.radius.bottom_right};
}

std::wstring mediaUri(const std::string &source) {
  if (source.find("://") != std::string::npos) return winrt::to_hstring(source).c_str();
  std::string out = "file:///";
  for (unsigned char c : source) {
    if (c == '\\') out += '/';
    else if (std::isalnum(c) || std::strchr("-._~/:", c)) out += (char)c;
    else {
      char hex[4];
      std::snprintf(hex, sizeof hex, "%%%02X", c);
      out += hex;
    }
  }
  return winrt::to_hstring(out).c_str();
}

enum class NativeKind {
  Root,
  Flex,
  Panel,
  Floating,
  Sidebar,
  Page,
  Button,
  Text,
  Checkbox,
  Toggle,
  Radio,
  Entry,
  Dropdown,
  Slider,
  Image,
  Icon,
  Document,
  SidebarSection,
  SidebarDivider,
  Video,
  Audio,
  Divider
};

struct ChildContext {
  bool horizontal = false;
  n8v_align hAlign = N8V_ALIGN_START;
  n8v_align vAlign = N8V_ALIGN_START;
};

struct NativeNode {
  NativeKind kind() const { return node ? (NativeKind)node->current.props.index() : NativeKind::Root; }

  PNode *node = nullptr;
  NativeNode *parentNode = nullptr;
  std::vector<NativeNode *> kids;
  ChildContext ctx;

  mux::UIElement outer{nullptr};
  mux::FrameworkElement outerFe{nullptr};
  muxc::Grid grid{nullptr};
  muxc::ScrollViewer scroll{nullptr};
  mux::FrameworkElement widget{nullptr};
  muxc::Border border{nullptr};

  // entry
  muxc::TextBox textBox{nullptr};
  muxc::PasswordBox passwordBox{nullptr};
  bool lastPassword = false;
  // floating
  muxp::Popup popup{nullptr};
  // panel
  mux::Shapes::Rectangle accentBar{nullptr};
  // sidebar
  muxc::NavigationView nav{nullptr};
  muxc::TextBlock titleText{nullptr};
  muxc::Button headerButtons[2] = {nullptr, nullptr};
  muxc::StackPanel header{nullptr};
  // page / sidebar decoration
  mux::UIElement item{nullptr};
  // media
  muxc::MediaPlayerElement player{nullptr};

  bool hasDrag = false, hasZoom = false, tapActivates = false, multiline = false, hasOnKey = false, hasPaste = false, collapsible = false, isAction = false;
  n8v_submit_mode submitMode = N8V_SUBMIT_ON_ENTER;
  int appliedCompact = -1;
  double appliedIndent = -1;
  std::string appliedLabel;
  std::atomic<unsigned> userSeq{0}, ackSeq{0};

  bool floating = false;
  bool popupWantsOpen = false;
  bool stickyBottom = false;
  bool stickyAtBottom = true;
  double stickyLastMax = 0.0;
  double percentW = 0.0;
  double percentH = 0.0;
  std::string iconKey;
  std::string lastItemsKey;
  std::string mediaSource;
  const void *imageSource = nullptr;
  std::string registeredId;
  winrt::event_token hoverEnter{}, hoverExit{};
  bool hoverWired = false;
};

n8v_sizing fitSizing() { return n8v_sizing_fit(0.0f, 0.0f); }
n8v_sizing growSizing() { return n8v_sizing_grow(0.0f, 0.0f); }

n8v_sizing sizingFor(const NativeNode &nn, bool horizontal) {
  return std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_panel_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_floating_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_document_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_icon_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_image_options &o) { return horizontal ? o.width : o.height; },
      [&](const n8v_video_options &o) {
        n8v_sizing s = horizontal ? o.width : o.height;
        if (s.mode == N8V_SIZING_FIT && s.min <= 0.0f && s.max <= 0.0f) return n8v_sizing_fixed(horizontal ? 480.0f : 270.0f);
        return s;
      },
      [&](const n8v_audio_options &o) {
        if (!horizontal) return fitSizing();
        if (o.width.mode == N8V_SIZING_FIT && o.width.min <= 0.0f && o.width.max <= 0.0f) return n8v_sizing_fixed(320.0f);
        return o.width;
      },
      [&](const n8v_divider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_sidebar_options &) { return growSizing(); },
      [&](const n8v_entry_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_dropdown_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const n8v_slider_options &) { return horizontal ? growSizing() : fitSizing(); },
      [&](const auto &) { return fitSizing(); },
    },
    nn.node->current.props
  );
}

ChildContext contextFor(const NativeNode &parent) {
  ChildContext c;
  if (!parent.node) return c;
  std::visit(
    Overloaded{
      [&](const n8v_flex_options &o) {
        c.horizontal = o.direction == N8V_DIRECTION_HORIZONTAL;
        c.hAlign = o.h_align;
        c.vAlign = o.v_align;
      },
      [&](const n8v_panel_options &o) {
        c.horizontal = o.direction == N8V_DIRECTION_HORIZONTAL;
        c.hAlign = o.h_align;
        c.vAlign = o.v_align;
      },
      [](const auto &) {},
    },
    parent.node->current.props
  );
  return c;
}

void *identityOf(const winrt::Windows::Foundation::IUnknown &object) { return winrt::get_abi(object.as<winrt::Windows::Foundation::IInspectable>()); }

bool userPending(const NativeNode &nn) { return nn.userSeq.load() != nn.ackSeq.load(); }

bool isSidebarDecoration(NativeKind kind) { return kind == NativeKind::SidebarSection || kind == NativeKind::SidebarDivider; }

bool isActionPage(const NativeNode &node) {
  const auto *o = node.node ? std::get_if<n8v_page_options>(&node.node->current.props) : nullptr;
  return o && o->action;
}

bool isContainer(NativeKind kind) {
  return kind == NativeKind::Root || kind == NativeKind::Flex || kind == NativeKind::Panel || kind == NativeKind::Floating || kind == NativeKind::Page;
}

struct PendingScroll {
  NativeScrollRequest request;
  std::chrono::steady_clock::time_point requestedAt = std::chrono::steady_clock::now();
};

struct PendingFocus {
  std::string id;
  int framesWaited = 0;
};

struct Quiet {
  explicit Quiet(int &counter) : counter_(counter) { ++counter_; }
  ~Quiet() { --counter_; }
  Quiet(const Quiet &) = delete;
  Quiet &operator=(const Quiet &) = delete;

private:
  int &counter_;
};

winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> savePastedBitmap(winrt::Windows::ApplicationModel::DataTransfer::DataPackageView view) {
  using namespace winrt::Windows::Graphics::Imaging;
  using namespace winrt::Windows::Storage;
  auto reference = co_await view.GetBitmapAsync();
  auto input = co_await reference.OpenReadAsync();
  auto decoder = co_await BitmapDecoder::CreateAsync(input);
  auto bitmap = co_await decoder.GetSoftwareBitmapAsync();

  const std::string path = newPastedImagePath("png");
  const size_t slash = path.find_last_of("\\/");
  auto folder = co_await StorageFolder::GetFolderFromPathAsync(hs(path.substr(0, slash)));
  auto file = co_await folder.CreateFileAsync(hs(path.substr(slash + 1)), CreationCollisionOption::ReplaceExisting);
  auto output = co_await file.OpenAsync(FileAccessMode::ReadWrite);
  auto encoder = co_await BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), output);
  encoder.SetSoftwareBitmap(bitmap);
  co_await encoder.FlushAsync();
  co_return hs(path);
}

winrt::fire_and_forget pasteAsync(winrt::Windows::ApplicationModel::DataTransfer::DataPackageView view, std::function<void(std::vector<std::string>)> deliver) {
  using winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats;
  std::vector<std::string> paths;
  try {
    if (view.Contains(StandardDataFormats::StorageItems())) {
      auto items = co_await view.GetStorageItemsAsync();
      for (auto const &item : items) {
        const std::string path = utf8(item.Path());
        if (!path.empty()) paths.push_back(path);
      }
    } else {
      const std::string path = utf8(co_await savePastedBitmap(view));
      if (!path.empty()) paths.push_back(path);
    }
  } catch (winrt::hresult_error const &) {
  }
  if (!paths.empty()) deliver(std::move(paths));
}

winrt::fire_and_forget pickFilesAsync(FileDialogRequest request, FileDialogDone done, winrt::Microsoft::UI::WindowId window) {
  namespace pickers = winrt::Microsoft::Windows::Storage::Pickers;
  FileDialogResult result;
  result.status = FileDialogResult::Status::Cancelled;
  try {
    auto extensionsOf = [](const FileDialogFilter &filter) {
      std::vector<winrt::hstring> out;
      for (const std::string &pattern : filter.patterns) {
        const std::string ext = globExtension(normalizeFileGlob(pattern));
        out.push_back(ext.empty() ? winrt::hstring(L"*") : hs("." + ext));
      }
      return out;
    };
    switch (request.kind) {
    case N8V_FILE_DIALOG_OPEN_FILE: {
      pickers::FileOpenPicker picker(window);
      bool any = request.filters.empty();
      for (const FileDialogFilter &filter : request.filters)
        for (const winrt::hstring &ext : extensionsOf(filter)) {
          picker.FileTypeFilter().Append(ext);
          any = any || ext == L"*";
        }
      if (any) {
        picker.FileTypeFilter().Clear();
        picker.FileTypeFilter().Append(L"*");
      }
      if (request.multiple) {
        for (auto const &file : co_await picker.PickMultipleFilesAsync()) result.paths.push_back(utf8(file.Path()));
      } else if (auto file = co_await picker.PickSingleFileAsync()) {
        result.paths.push_back(utf8(file.Path()));
      }
      break;
    }
    case N8V_FILE_DIALOG_SAVE_FILE: {
      pickers::FileSavePicker picker(window);
      if (!request.defaultName.empty()) picker.SuggestedFileName(hs(request.defaultName));
      if (!request.initialDir.empty()) picker.SuggestedFolder(hs(request.initialDir));
      for (const FileDialogFilter &filter : request.filters) {
        std::vector<winrt::hstring> extensions;
        for (const winrt::hstring &ext : extensionsOf(filter))
          if (ext != L"*") extensions.push_back(ext);
        if (!extensions.empty()) picker.FileTypeChoices().Insert(hs(filter.name), winrt::single_threaded_vector<winrt::hstring>(std::move(extensions)));
      }
      if (auto file = co_await picker.PickSaveFileAsync()) result.paths.push_back(utf8(file.Path()));
      break;
    }
    case N8V_FILE_DIALOG_OPEN_FOLDER: {
      pickers::FolderPicker picker(window);
      if (auto folder = co_await picker.PickSingleFolderAsync()) result.paths.push_back(utf8(folder.Path()));
      break;
    }
    }
    if (!result.paths.empty()) result.status = FileDialogResult::Status::Done;
  } catch (winrt::hresult_error const &e) {
    std::fprintf(stderr, "[n8v] winui3: the file picker failed: %s\n", winrt::to_string(e.message()).c_str());
    result.status = FileDialogResult::Status::Unavailable;
  }
  done(std::move(result));
}

class WinUI3TreeBackend final : public Backend, public tree::TreeOps {
public:
  ~WinUI3TreeBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    trace_ = std::getenv("N8V_WINUI3_TRACE") != nullptr || std::getenv("N8V_WINUI3_DUMP") != nullptr;
    g_trace = trace_;
    if (trace_) g_previousFilter = SetUnhandledExceptionFilter(crashReporter);
    stage("starting");
    try {
      winrt::init_apartment(winrt::apartment_type::multi_threaded);
    } catch (winrt::hresult_error const &) {
    }
    if (!bootstrap_.initialize()) return false;
    stage("bootstrap ok");

    wakeEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    readyEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    initWidth_ = width;
    initHeight_ = height;
    initTitle_ = std::string(title);
    uiRunning_ = true;
    uiThread_ = std::thread([this] { uiMain(); });
    if (WaitForSingleObject(readyEvent_, 30000) == WAIT_TIMEOUT) {
      std::fprintf(stderr, "[n8v] winui3: the XAML application did not start within 30 seconds\n");
      uiRunning_ = false;
      if (uiThread_.joinable()) uiThread_.detach();
      return false;
    }
    if (!initOk_) {
      std::fprintf(stderr, "[n8v] winui3: initialization failed: %s\n", initError_.c_str());
      if (uiThread_.joinable()) uiThread_.join();
      return false;
    }
    stage("initialized");
    return true;
  }

  bool pumpEvents() override {
    const bool busy = settleFrames_ > 0 || popoverOpen_ || !pendingScrolls_.empty() || !pendingFocus_.empty();
    const DWORD waited = WaitForSingleObject(wakeEvent_, busy ? kBusyFrameMs : kIdleFrameMs);
    const bool delivered = drainEvents();
    if (waited != WAIT_TIMEOUT || delivered) settleFrames_ = kSettleFrames;
    else if (settleFrames_ > 0) --settleFrames_;
    return open_;
  }

  bool prefersDark() const override { return dark_; }

  void wake() override {
    if (wakeEvent_) SetEvent(wakeEvent_);
  }

  bool windowFocused() const override { return hwnd_ && GetForegroundWindow() == hwnd_; }

  void raiseWindow() override {
    onUi([this] {
      if (!hwnd_) return;
      if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
      SetForegroundWindow(hwnd_);
    });
  }

  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override { return {width_, height_}; }

  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }

  bool rendersFromClay() const override { return false; }
  bool rendersNativeChrome() const override { return true; }
  tree::TreeOps *treeOps() override { return this; }
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}

  void beginFrame() override { drainEvents(); }

  void commitFrame() override {
    onUi([this] {
      if (++frames_ == 30 && std::getenv("N8V_WINUI3_DUMP")) dump();
      if (!dumpedLate_ && std::getenv("N8V_WINUI3_DUMP") && std::chrono::steady_clock::now() - started_ > std::chrono::seconds(6)) {
        dumpedLate_ = true;
        std::fprintf(stderr, "[n8v] winui3: after 6 seconds:\n");
        dump();
      }
      updatePercentSizes();
      updateFloating();
      applyStickyBottom();
      resolvePendingScrolls();
      resolvePendingFocus();
    });
  }

  bool isHovered(std::string_view id) const override { return hovered_.count(std::string(id)) != 0; }

  bool visibleChildren(std::string_view containerId, float margin, const std::function<void(std::string_view, float, float)> &visit) const override {
    struct Child {
      std::string id;
      float top, height;
    };
    std::vector<Child> found;
    bool ok = false;
    onUi([&] {
      auto container = byId_.find(std::string(containerId));
      if (container == byId_.end() || !container->second->scroll) return;
      const NativeNode &c = *container->second;
      const double viewHeight = c.scroll.ViewportHeight();
      if (viewHeight <= 0.0) return;
      ok = true;
      for (const NativeNode *kid : c.kids) {
        if (kid->floating || !kid->outer) continue;
        const char *id = tree::idOf(kid->node->current.props);
        if (!id || !*id) continue;
        const double top = kid->outer.TransformToVisual(c.scroll).TransformPoint(wf::Point{0, 0}).Y;
        const double height = kid->outerFe.ActualHeight();
        if (top + height >= -margin && top <= viewHeight + margin) found.push_back({id, (float)top, (float)height});
      }
    });
    for (const Child &child : found) visit(child.id, child.top, child.height);
    return ok;
  }

  bool scrollState(std::string_view containerId, float &offsetX, float &offsetY, float &viewWidth, float &viewHeight) const override {
    bool ok = false;
    onUi([&] {
      auto container = byId_.find(std::string(containerId));
      if (container == byId_.end() || !container->second->scroll) return;
      const muxc::ScrollViewer &scroll = container->second->scroll;
      offsetX = (float)scroll.HorizontalOffset();
      offsetY = (float)scroll.VerticalOffset();
      viewWidth = (float)scroll.ViewportWidth();
      viewHeight = (float)scroll.ViewportHeight();
      ok = viewWidth > 0.0f && viewHeight > 0.0f;
    });
    return ok;
  }

  void requestScroll(const NativeScrollRequest &request) override { pendingScrolls_.push_back(PendingScroll{request, std::chrono::steady_clock::now()}); }

  void requestFocus(std::string_view id) override { pendingFocus_.push_back(PendingFocus{std::string(id), 0}); }

  bool showFileDialog(const FileDialogRequest &request, FileDialogDone done) override {
    bool started = false;
    onUi([&] {
      if (!window_) return;
      pickFilesAsync(
        request,
        [this, done = std::move(done)](FileDialogResult result) { post([done, result = std::move(result)]() mutable { done(std::move(result)); }); },
        window_.AppWindow().Id()
      );
      started = true;
    });
    return started;
  }

  bool showMessageDialog(const MessageDialogRequest &request, MessageDialogDone done) override {
    bool shown = false;
    onUi([&] {
      if (!rootGrid_ || !rootGrid_.XamlRoot()) return;
      auto result = std::make_shared<int>(-1);
      muxc::ContentDialog dialog;
      dialog.XamlRoot(rootGrid_.XamlRoot());
      dialog.Title(winrt::box_value(hs(request.title)));
      muxc::StackPanel body;
      body.Spacing(16);
      muxc::TextBlock message;
      message.Text(hs(request.message));
      message.TextWrapping(mux::TextWrapping::Wrap);
      body.Children().Append(message);
      muxc::StackPanel row;
      row.Orientation(muxc::Orientation::Horizontal);
      row.Spacing(8);
      row.HorizontalAlignment(mux::HorizontalAlignment::Right);
      std::vector<MessageDialogButton> buttons = request.buttons;
      if (buttons.empty()) buttons.push_back(MessageDialogButton{"OK", false});
      for (size_t i = 0; i < buttons.size(); ++i) {
        muxc::Button button;
        button.Content(winrt::box_value(hs(buttons[i].label)));
        button.MinWidth(80);
        if (i == 0) {
          if (auto style = styleResource(L"AccentButtonStyle")) button.Style(style);
        }
        if (buttons[i].destructive) button.Foreground(brushOf(n8v_color{196, 43, 28, 255}));
        button.Click([dialog, result, i](auto &&, auto &&) {
          *result = (int)i;
          dialog.Hide();
        });
        row.Children().Append(button);
      }
      body.Children().Append(row);
      dialog.Content(body);
      dialog.PreviewKeyDown([dialog](auto &&, mux::Input::KeyRoutedEventArgs const &args) {
        if (args.Key() == winrt::Windows::System::VirtualKey::Escape) {
          args.Handled(true);
          dialog.Hide();
        }
      });
      dialog.Closed([this, result, done = std::move(done)](auto &&, auto &&) {
        const int pressed = *result;
        post([done, pressed] { done(pressed); });
      });
      try {
        dialog.ShowAsync();
        shown = true;
      } catch (winrt::hresult_error const &) {
      }
    });
    return shown;
  }

  void shutdown() override {
    if (shuttingDown_) return;
    shuttingDown_ = true;
    if (uiRunning_) {
      onUi([this] {
        Quiet quiet(suppress_);
        for (PNode *pn : live_) {
          NativeNode *nn = native(*pn);
          if (nn && nn->popup) nn->popup.IsOpen(false);
        }
        try {
          window_.Close();
        } catch (winrt::hresult_error const &) {
        }
        try {
          mux::Application::Current().Exit();
        } catch (winrt::hresult_error const &) {
        }
      });
    }
    if (uiThread_.joinable()) uiThread_.join();
    bootstrap_.close();
    if (wakeEvent_) CloseHandle(wakeEvent_);
    if (readyEvent_) CloseHandle(readyEvent_);
    wakeEvent_ = readyEvent_ = nullptr;
  }

  void create(PNode &node) override {
    auto *nn = new NativeNode;
    nn->node = &node;
    node.handle = nn;
    onUi(
      [&] {
        live_.insert(&node);
        build(*nn);
        apply(*nn);
        registerId(*nn);
      },
      "create " + nodeName(*nn)
    );
  }

  void update(PNode &node, const OwnedProps &, unsigned) override {
    onUi(
      [&] {
        NativeNode &nn = *native(node);
        apply(nn);
        registerId(nn);
        if (nn.parentNode && nn.kind() != NativeKind::Page && !isSidebarDecoration(nn.kind())) layoutChild(nn);
        if (isContainer(nn.kind())) {
          for (NativeNode *kid : nn.kids)
            if (kid->kind() != Kind::Page && !isSidebarDecoration(kid->kind())) layoutChild(*kid);
          relayout(nn);
        }
        if (nn.kind() == NativeKind::Sidebar) {
          for (NativeNode *kid : nn.kids)
            if (kid->kind() == NativeKind::Page) apply(*kid);
          syncSidebarSelection(nn);
        }
      },
      "update " + nodeName(*native(node))
    );
  }

  void insert(PNode &parent, PNode &child, size_t index) override {
    onUi(
      [&] {
        NativeNode &p = nativeOrRoot(parent);
        NativeNode &c = *native(child);
        c.parentNode = &p;
        p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
        attach(p, c);
        afterKidsChanged(p);
      },
      "insert " + nodeName(*native(child)) + " into " + nodeName(nativeOrRoot(parent))
    );
  }

  void move(PNode &parent, PNode &child, size_t index) override {
    onUi(
      [&] {
        NativeNode &p = nativeOrRoot(parent);
        NativeNode &c = *native(child);
        detach(p, c);
        p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
        p.kids.insert(p.kids.begin() + (ptrdiff_t)index, &c);
        attach(p, c);
        afterKidsChanged(p);
      },
      "move " + nodeName(*native(child))
    );
  }

  void remove(PNode &parent, PNode &child) override {
    onUi(
      [&] {
        NativeNode &p = nativeOrRoot(parent);
        NativeNode &c = *native(child);
        detach(p, c);
        p.kids.erase(std::find(p.kids.begin(), p.kids.end(), &c));
        c.parentNode = nullptr;
        afterKidsChanged(p);
      },
      "remove " + nodeName(*native(child))
    );
  }

  void destroy(PNode &node) override {
    NativeNode *nn = native(node);
    if (!nn || !uiRunning_) return;
    onUi([&] {
      Quiet quiet(suppress_);
      unregisterId(*nn);
      live_.erase(&node);
      if (nn->popup) nn->popup.IsOpen(false);
      if (nn->item) itemNodes_.erase(identityOf(nn->item));
      delete nn;
    });
    node.handle = nullptr;
  }

private:
  using Kind = NativeKind;

  NativeNode *native(PNode &node) const { return static_cast<NativeNode *>(node.handle); }
  NativeNode &nativeOrRoot(PNode &node) { return node.parent ? *native(node) : root_; }

  static std::string nodeName(const NativeNode &nn) {
    static const char *kinds[] = {"root",   "flex",     "panel",    "floating",        "sidebar",         "page",     "button",
                                  "text",   "checkbox", "toggle",   "radio",           "entry",           "dropdown", "slider",
                                  "image",  "icon",     "document", "sidebar section", "sidebar divider", "video",    "audio",
                                  "divider"};
    const size_t kind = (size_t)nn.kind();
    return kind < sizeof(kinds) / sizeof(kinds[0]) ? kinds[kind] : "node";
  }

  template <typename F> void onUi(F &&fn, std::string label = {}) const {
    if (GetCurrentThreadId() == uiThreadId_) {
      fn();
      return;
    }
    if (!uiRunning_ || !queue_) return;
    HANDLE done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    auto *function = &fn;
    const bool queued = queue_.TryEnqueue([function, done, label = std::move(label)] {
      const auto begun = std::chrono::steady_clock::now();
      try {
        (*function)();
      } catch (winrt::hresult_error const &e) {
        std::fprintf(
          stderr,
          "[n8v] winui3: %s failed: %s (0x%08x)\n",
          label.empty() ? "an operation" : label.c_str(),
          winrt::to_string(e.message()).c_str(),
          (unsigned)e.code().value
        );
      } catch (std::exception const &e) {
        std::fprintf(stderr, "[n8v] winui3: %s failed: %s\n", label.empty() ? "an operation" : label.c_str(), e.what());
      }
      if (g_trace) {
        const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begun).count();
        if (took >= 1.0) std::fprintf(stderr, "[n8v] winui3: %s took %.1f ms\n", label.empty() ? "an operation" : label.c_str(), took);
      }
      SetEvent(done);
    });
    if (queued) {
      while (WaitForSingleObject(done, 100) == WAIT_TIMEOUT) {
        if (!uiRunning_) break;
      }
    }
    CloseHandle(done);
  }

  void post(std::function<void()> fn) {
    {
      std::lock_guard<std::mutex> lock(eventsMutex_);
      events_.push_back(std::move(fn));
    }
    if (wakeEvent_) SetEvent(wakeEvent_);
  }

  bool drainEvents() {
    std::deque<std::function<void()>> batch;
    {
      std::lock_guard<std::mutex> lock(eventsMutex_);
      batch.swap(events_);
    }
    for (auto &fn : batch) fn();
    return !batch.empty();
  }

  void userEvent(NativeNode *nn, std::function<void(PNode &)> fn) {
    PNode *pn = nn->node;
    const unsigned seq = ++nn->userSeq;
    post([this, pn, nn, seq, fn = std::move(fn)] {
      if (!live_.count(pn)) return;
      fn(*pn);
      nn->ackSeq = seq;
      onUi(
        [this, pn, nn] {
          if (!live_.count(pn)) return;
          apply(*nn);
          if (nn->kind() == Kind::Sidebar) syncSidebarSelection(*nn);
        },
        "resync " + nodeName(*nn)
      );
    });
  }

  void postTo(PNode *pn, std::function<void(PNode &)> fn) {
    post([this, pn, fn = std::move(fn)] {
      if (live_.count(pn)) fn(*pn);
    });
  }

  bool askApp(PNode *pn, std::function<bool(PNode &)> fn) {
    auto promise = std::make_shared<std::promise<bool>>();
    auto future = promise->get_future();
    post([this, pn, fn = std::move(fn), promise] { promise->set_value(live_.count(pn) ? fn(*pn) : false); });
    if (future.wait_for(std::chrono::milliseconds(150)) != std::future_status::ready) return false;
    return future.get();
  }

  void uiMain() {
    uiThreadId_ = GetCurrentThreadId();
    stage("UI thread started");
    try {
      winrt::init_apartment(winrt::apartment_type::single_threaded);
      stage("UI thread apartment ok");
      g_onLaunched = [this] { launched(); };
      stage("calling Application::Start");
      mux::Application::Start([this](mux::ApplicationInitializationCallbackParams const &) {
        stage("Application::Start callback: creating the application");
        try {
          winrt::make<N8VApp>();
          stage("application created");
        } catch (winrt::hresult_error const &e) {
          std::fprintf(stderr, "[n8v] winui3: creating the application failed: %s (0x%08x)\n", winrt::to_string(e.message()).c_str(), (unsigned)e.code().value);
          initError_ = "creating the application failed: " + winrt::to_string(e.message());
          SetEvent(readyEvent_);
        } catch (std::exception const &e) {
          std::fprintf(stderr, "[n8v] winui3: creating the application failed: %s\n", e.what());
          initError_ = e.what();
          SetEvent(readyEvent_);
        }
      });
      stage("Application::Start returned");
    } catch (winrt::hresult_error const &e) {
      std::fprintf(stderr, "[n8v] winui3: Application::Start failed: %s (0x%08x)\n", winrt::to_string(e.message()).c_str(), (unsigned)e.code().value);
      if (!initOk_) initError_ = winrt::to_string(e.message());
    } catch (std::exception const &e) {
      std::fprintf(stderr, "[n8v] winui3: Application::Start failed: %s\n", e.what());
      if (!initOk_) initError_ = e.what();
    }
    open_ = false;
    uiRunning_ = false;
    rootGrid_ = nullptr;
    window_ = nullptr;
    queue_ = nullptr;
    if (!initOk_ && initError_.empty()) initError_ = "the XAML application ended before its window was created";
    SetEvent(readyEvent_);
    SetEvent(wakeEvent_);
  }

  void launched() {
    stage("OnLaunched");
    try {
      queue_ = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
      stage("dispatcher queue ok");
      rootGrid_ = muxc::Grid();
      rootGrid_.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
      rootGrid_.VerticalAlignment(mux::VerticalAlignment::Stretch);
      rootGrid_.Loaded([this](auto &&, auto &&) {
        stage("root Loaded");
        themeChanged();
      });
      rootGrid_.ActualThemeChanged([this](auto &&, auto &&) { themeChanged(); });
      rootGrid_.SizeChanged([this](auto &&, mux::SizeChangedEventArgs const &args) {
        width_ = args.NewSize().Width;
        height_ = args.NewSize().Height;
      });

      stage("root created");
      window_ = mux::Window();
      stage("Window created");
      window_.Title(hs(initTitle_));
      window_.Content(rootGrid_);
      stage("window content set");
      window_.Closed([this](auto &&, auto &&) {
        open_ = false;
        if (wakeEvent_) SetEvent(wakeEvent_);
      });
      auto appWindow = window_.AppWindow();
      hwnd_ = winrt::Microsoft::UI::GetWindowFromWindowId(appWindow.Id());
      stage("window handle ok");
      const double scale = hwnd_ ? GetDpiForWindow(hwnd_) / 96.0 : 1.0;
      appWindow.ResizeClient(winrt::Windows::Graphics::SizeInt32{(int32_t)std::lround(initWidth_ * scale), (int32_t)std::lround(initHeight_ * scale)});

      root_.outer = rootGrid_;
      root_.outerFe = rootGrid_;
      root_.grid = rootGrid_;
      themeChanged();
      stage("theme ok");
      window_.Activate();
      stage("window activated");
      if (g_trace) {
        struct Meter {
          std::chrono::steady_clock::time_point second = std::chrono::steady_clock::now(), last = second;
          int frames = 0;
          double longest = 0.0;
        };
        auto meter = std::make_shared<Meter>();
        muxm::CompositionTarget::Rendering([meter](auto &&, auto &&) {
          const auto now = std::chrono::steady_clock::now();
          meter->longest = std::max(meter->longest, std::chrono::duration<double, std::milli>(now - meter->last).count());
          meter->last = now;
          ++meter->frames;
          if (now - meter->second >= std::chrono::seconds(1)) {
            std::fprintf(stderr, "[n8v] winui3: %d frames in the last second, longest gap %.0f ms\n", meter->frames, meter->longest);
            meter->second = now;
            meter->frames = 0;
            meter->longest = 0.0;
          }
        });
      }
      initOk_ = true;
    } catch (winrt::hresult_error const &e) {
      std::fprintf(stderr, "[n8v] winui3: creating the window failed: %s (0x%08x)\n", winrt::to_string(e.message()).c_str(), (unsigned)e.code().value);
      initError_ = winrt::to_string(e.message());
    } catch (std::exception const &e) {
      std::fprintf(stderr, "[n8v] winui3: creating the window failed: %s\n", e.what());
      initError_ = e.what();
    }
    SetEvent(readyEvent_);
  }

  void stage(const char *what) const {
    if (trace_) std::fprintf(stderr, "[n8v] winui3: %s\n", what);
  }

  void dump() const {
    RECT client{};
    if (hwnd_) GetClientRect(hwnd_, &client);
    std::fprintf(
      stderr,
      "[n8v] winui3: window client %ldx%ld, live nodes %zu, root %.0fx%.0f, loaded %d\n",
      client.right - client.left,
      client.bottom - client.top,
      live_.size(),
      rootGrid_.ActualWidth(),
      rootGrid_.ActualHeight(),
      (int)rootGrid_.IsLoaded()
    );
    if (auto root = rootGrid_.XamlRoot()) {
      std::fprintf(stderr, "[n8v] winui3: XamlRoot size %.0fx%.0f, scale %.2f\n", root.Size().Width, root.Size().Height, root.RasterizationScale());
    } else {
      std::fprintf(stderr, "[n8v] winui3: the root has no XamlRoot\n");
    }
    dumpElement(rootGrid_, 0);
  }

  static void dumpElement(const mux::DependencyObject &object, int depth) {
    std::string line = std::string((size_t)depth * 2, ' ') + winrt::to_string(winrt::get_class_name(object));
    if (auto fe = object.try_as<mux::FrameworkElement>()) {
      char size[96];
      std::snprintf(size, sizeof size, " %.0fx%.0f%s", fe.ActualWidth(), fe.ActualHeight(), fe.Visibility() == mux::Visibility::Collapsed ? " (collapsed)" : "");
      line += size;
    }
    if (auto scroll = object.try_as<muxc::ScrollViewer>()) {
      char extent[128];
      std::snprintf(extent, sizeof extent, " [extent %.0f, viewport %.0f, offset %.0f]", scroll.ExtentHeight(), scroll.ViewportHeight(), scroll.VerticalOffset());
      line += extent;
    }
    std::fprintf(stderr, "%s\n", line.c_str());
    if (depth > 24) return;
    const int count = muxm::VisualTreeHelper::GetChildrenCount(object);
    for (int i = 0; i < count; ++i) dumpElement(muxm::VisualTreeHelper::GetChild(object, i), depth + 1);
  }

  double dpiScale() const { return hwnd_ ? GetDpiForWindow(hwnd_) / 96.0 : 1.0; }

  void themeChanged() {
    if (shuttingDown_ || !rootGrid_) return;
    const bool dark = rootGrid_.ActualTheme() == mux::ElementTheme::Dark;
    const bool changed = dark != dark_;
    dark_ = dark;
    rootGrid_.Background(themeBrush(L"ApplicationPageBackgroundThemeBrush", L"SolidBackgroundFillColorBaseBrush"));
    if (hwnd_) {
      static const auto setAttribute =
        reinterpret_cast<HRESULT(WINAPI *)(HWND, DWORD, LPCVOID, DWORD)>(reinterpret_cast<void *>(GetProcAddress(LoadLibraryW(L"dwmapi.dll"), "DwmSetWindowAttribute")));
      const BOOL immersive = dark ? TRUE : FALSE;
      if (setAttribute) setAttribute(hwnd_, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &immersive, sizeof immersive);
    }
    if (!changed) return;
    post([this] {
      onUi([this] {
        for (PNode *pn : live_) {
          NativeNode &nn = *native(*pn);
          nn.iconKey.clear();
          apply(nn);
        }
      });
    });
  }

  static mux::Style styleResource(const wchar_t *key) {
    try {
      auto resources = mux::Application::Current().Resources();
      auto value = resources.Lookup(winrt::box_value(key));
      return value.as<mux::Style>();
    } catch (winrt::hresult_error const &) {
      return nullptr;
    }
  }

  static winrt::Windows::Foundation::IInspectable lookupInDictionary(const mux::ResourceDictionary &dictionary, const wchar_t *key, const wchar_t *theme, int depth = 0) {
    if (!dictionary || depth > 8) return nullptr;
    auto name = winrt::box_value(key);
    if (dictionary.HasKey(name)) return dictionary.Lookup(name);
    auto themes = dictionary.ThemeDictionaries();
    auto themeKey = winrt::box_value(theme);
    if (themes.HasKey(themeKey)) {
      if (auto themed = themes.Lookup(themeKey).try_as<mux::ResourceDictionary>()) {
        if (themed.HasKey(name)) return themed.Lookup(name);
        for (auto const &merged : themed.MergedDictionaries())
          if (auto found = lookupInDictionary(merged, key, theme, depth + 1)) return found;
      }
    }
    for (auto const &merged : dictionary.MergedDictionaries())
      if (auto found = lookupInDictionary(merged, key, theme, depth + 1)) return found;
    return nullptr;
  }

  muxm::Brush themeBrush(const wchar_t *key, const wchar_t *fallbackKey = nullptr) const {
    const bool dark = dark_;
    const wchar_t *theme = dark ? L"Default" : L"Light";
    try {
      auto resources = mux::Application::Current().Resources();
      for (const wchar_t *candidate : {key, fallbackKey}) {
        if (!candidate) continue;
        if (auto found = lookupInDictionary(resources, candidate, theme)) {
          if (auto brush = found.try_as<muxm::Brush>()) return brush;
        }
      }
    } catch (winrt::hresult_error const &) {
    }
    return brushOf(dark ? n8v_color{32, 32, 32, 255} : n8v_color{243, 243, 243, 255});
  }

  n8v_color defaultTint() const { return prefersDark() ? n8v_color{255, 255, 255, 255} : n8v_color{0, 0, 0, 255}; }

  void attach(NativeNode &parent, NativeNode &child) {
    if (parent.kind() == Kind::Sidebar) {
      if (child.kind() == Kind::Page || isSidebarDecoration(child.kind())) {
        trace("sidebar item inserted");
        parent.nav.MenuItems().InsertAt((uint32_t)rowIndex(parent, child), child.item);
      }
      return;
    }
    if (child.floating) return;
    if (!parent.grid || !child.outer) return;
    parent.grid.Children().Append(child.outer);
    child.ctx = contextFor(parent);
    layoutChild(child);
    relayout(parent);
  }

  void detach(NativeNode &parent, NativeNode &child) {
    if (parent.kind() == Kind::Sidebar) {
      if (child.kind() == Kind::Page || isSidebarDecoration(child.kind())) {
        uint32_t index = 0;
        trace("sidebar item removed");
        if (parent.nav.MenuItems().IndexOf(child.item, index)) parent.nav.MenuItems().RemoveAt(index);
        if (child.kind() == Kind::Page && parent.nav.Content() == child.grid) parent.nav.Content(nullptr);
      }
      return;
    }
    if (child.floating) {
      if (child.popup) child.popup.IsOpen(false);
      return;
    }
    if (parent.grid && child.outer) {
      uint32_t index = 0;
      if (parent.grid.Children().IndexOf(child.outer, index)) parent.grid.Children().RemoveAt(index);
    }
    relayout(parent);
  }

  size_t rowIndex(NativeNode &sidebar, NativeNode &child) const {
    size_t index = 0;
    for (NativeNode *kid : sidebar.kids) {
      if (kid == &child) break;
      if (kid->kind() == Kind::Page || isSidebarDecoration(kid->kind())) ++index;
    }
    return index;
  }

  void afterKidsChanged(NativeNode &container) {
    if (container.kind() == Kind::Sidebar) syncSidebarSelection(container);
  }

  void layoutChild(NativeNode &c) {
    if (!c.node || !c.outerFe || c.floating || !c.parentNode) return;
    c.ctx = contextFor(*c.parentNode);
    mux::FrameworkElement fe = c.outerFe;

    auto sizeAxis = [&](const n8v_sizing &s, bool horizontal, bool stretch, double &percent) {
      percent = 0.0;
      double min = 0.0, max = kInfinity, size = kNaN;
      switch (s.mode) {
      case N8V_SIZING_FIT:
      case N8V_SIZING_GROW:
        if (s.min > 0.0f) min = s.min;
        if (s.max > 0.0f) max = s.max;
        break;
      case N8V_SIZING_FIXED:
        size = std::max(0.0, (double)s.value);
        break;
      case N8V_SIZING_PERCENT:
        percent = s.value;
        break;
      }
      (void)stretch;
      if (horizontal) {
        fe.MinWidth(min);
        fe.MaxWidth(max);
        fe.Width(size);
      } else {
        fe.MinHeight(min);
        fe.MaxHeight(max);
        fe.Height(size);
      }
    };

    const n8v_sizing sw = sizingFor(c, true), sh = sizingFor(c, false);
    sizeAxis(sw, true, sw.mode == N8V_SIZING_GROW, c.percentW);
    sizeAxis(sh, false, sh.mode == N8V_SIZING_GROW, c.percentH);

    const bool growW = sw.mode == N8V_SIZING_GROW || sw.mode == N8V_SIZING_PERCENT;
    const bool growH = sh.mode == N8V_SIZING_GROW || sh.mode == N8V_SIZING_PERCENT;
    fe.HorizontalAlignment(growW ? mux::HorizontalAlignment::Stretch : (c.ctx.horizontal ? mux::HorizontalAlignment::Left : hAlignOf(c.ctx.hAlign)));
    fe.VerticalAlignment(growH ? mux::VerticalAlignment::Stretch : (c.ctx.horizontal ? vAlignOf(c.ctx.vAlign) : mux::VerticalAlignment::Top));
  }

  void relayout(NativeNode &c) {
    if (!c.grid || c.kind() == Kind::Sidebar) return;
    const ChildContext cc = contextFor(c);
    const double gap = std::visit(
      Overloaded{
        [](const n8v_flex_options &o) { return (double)o.gap; },
        [](const n8v_panel_options &o) { return (double)o.gap; },
        [](const auto &) { return 0.0; },
      },
      c.node ? c.node->current.props : tree::Node::Props{}
    );

    c.grid.RowDefinitions().Clear();
    c.grid.ColumnDefinitions().Clear();

    auto addMain = [&](mux::GridLength length, double min, double max) {
      if (cc.horizontal) {
        muxc::ColumnDefinition d;
        d.Width(length);
        d.MinWidth(min);
        d.MaxWidth(max);
        c.grid.ColumnDefinitions().Append(d);
      } else {
        muxc::RowDefinition d;
        d.Height(length);
        d.MinHeight(min);
        d.MaxHeight(max);
        c.grid.RowDefinitions().Append(d);
      }
    };
    const mux::GridLength star1{1.0, mux::GridUnitType::Star};
    if (cc.horizontal) {
      muxc::RowDefinition across;
      across.Height(star1);
      c.grid.RowDefinitions().Append(across);
    } else {
      muxc::ColumnDefinition across;
      across.Width(star1);
      c.grid.ColumnDefinitions().Append(across);
    }

    bool anyGrow = false;
    for (NativeNode *kid : c.kids) {
      if (kid->floating) continue;
      const n8v_sizing s = sizingFor(*kid, cc.horizontal);
      if (s.mode == N8V_SIZING_GROW || s.mode == N8V_SIZING_PERCENT) anyGrow = true;
    }
    const n8v_align mainAlign = cc.horizontal ? cc.hAlign : cc.vAlign;
    const bool leading = !anyGrow && (mainAlign == N8V_ALIGN_CENTER || mainAlign == N8V_ALIGN_END);
    const bool trailing = !anyGrow && (mainAlign == N8V_ALIGN_START || mainAlign == N8V_ALIGN_CENTER);

    int index = 0;
    if (leading) {
      addMain(star1, 0.0, kInfinity);
      ++index;
    }
    bool first = true;
    for (NativeNode *kid : c.kids) {
      if (kid->floating || !kid->outer) continue;
      const n8v_sizing s = sizingFor(*kid, cc.horizontal);
      const double lead = first ? 0.0 : gap;
      switch (s.mode) {
      case N8V_SIZING_FIT:
        addMain(mux::GridLength{1.0, mux::GridUnitType::Auto}, 0.0, kInfinity);
        break;
      case N8V_SIZING_GROW:
        addMain(star1, s.min > 0.0f ? s.min : 0.0, s.max > 0.0f ? s.max : kInfinity);
        break;
      case N8V_SIZING_FIXED:
        addMain(mux::GridLength{std::max(0.0, (double)s.value) + lead, mux::GridUnitType::Pixel}, 0.0, kInfinity);
        break;
      case N8V_SIZING_PERCENT:
        addMain(mux::GridLength{std::max(0.0001, (double)s.value), mux::GridUnitType::Star}, 0.0, kInfinity);
        break;
      }
      if (cc.horizontal) {
        muxc::Grid::SetColumn(kid->outerFe, index);
        muxc::Grid::SetRow(kid->outerFe, 0);
        kid->outerFe.Margin(mux::Thickness{first ? 0.0 : gap, 0, 0, 0});
      } else {
        muxc::Grid::SetRow(kid->outerFe, index);
        muxc::Grid::SetColumn(kid->outerFe, 0);
        kid->outerFe.Margin(mux::Thickness{0, first ? 0.0 : gap, 0, 0});
      }
      first = false;
      ++index;
    }
    if (trailing) addMain(star1, 0.0, kInfinity);
  }

  void setScrolled(NativeNode &nn, bool want, bool horizontal, bool vertical) {
    if (want && !nn.scroll) {
      muxc::ScrollViewer scroll;
      swapOuter(nn, scroll);
      scroll.Content(nn.grid);
      nn.scroll = scroll;
    } else if (!want && nn.scroll) {
      nn.scroll.Content(nullptr);
      muxc::ScrollViewer old = nn.scroll;
      nn.scroll = nullptr;
      swapOuter(nn, nn.grid);
      (void)old;
    }
    if (nn.scroll) {
      nn.scroll.HorizontalScrollBarVisibility(horizontal ? muxc::ScrollBarVisibility::Auto : muxc::ScrollBarVisibility::Disabled);
      nn.scroll.VerticalScrollBarVisibility(vertical ? muxc::ScrollBarVisibility::Auto : muxc::ScrollBarVisibility::Disabled);
      nn.scroll.HorizontalScrollMode(horizontal ? muxc::ScrollMode::Enabled : muxc::ScrollMode::Disabled);
      nn.scroll.VerticalScrollMode(vertical ? muxc::ScrollMode::Enabled : muxc::ScrollMode::Disabled);
    }
  }

  void swapOuter(NativeNode &nn, mux::UIElement replacement) {
    mux::UIElement old = nn.outer;
    if (nn.parentNode && nn.parentNode->grid && old) {
      uint32_t index = 0;
      if (nn.parentNode->grid.Children().IndexOf(old, index)) nn.parentNode->grid.Children().SetAt(index, replacement);
    }
    nn.outer = replacement;
    nn.outerFe = replacement.as<mux::FrameworkElement>();
    wireHover(nn);
    if (nn.parentNode) layoutChild(nn);
    if (nn.parentNode) relayout(*nn.parentNode);
  }

  void registerId(NativeNode &nn) {
    const char *id = tree::idOf(nn.node->current.props);
    const std::string wanted = id ? id : "";
    if (wanted != nn.registeredId) {
      if (!nn.registeredId.empty()) {
        auto it = byId_.find(nn.registeredId);
        if (it != byId_.end() && it->second == &nn) byId_.erase(it);
        hovered_.erase(nn.registeredId);
      }
      nn.registeredId = wanted;
      if (!wanted.empty()) byId_[wanted] = &nn;
    }
    wireHover(nn);
  }

  void unregisterId(NativeNode &nn) {
    if (nn.registeredId.empty()) return;
    auto it = byId_.find(nn.registeredId);
    if (it != byId_.end() && it->second == &nn) byId_.erase(it);
    hovered_.erase(nn.registeredId);
    nn.registeredId.clear();
  }

  void wireHover(NativeNode &nn) {
    if (!nn.outer) return;
    const auto *flex = propsOf<n8v_flex_options>(*nn.node);
    const bool wanted = !nn.registeredId.empty() || (flex && flex->on_hover);
    if (nn.hoverWired) {
      nn.outer.PointerEntered(nn.hoverEnter);
      nn.outer.PointerExited(nn.hoverExit);
      nn.hoverWired = false;
    }
    if (!wanted) return;
    if (auto control = nn.outer.try_as<muxc::Control>(); control && !control.Background()) control.Background(brushOf(n8v_color{0, 0, 0, 0}));
    else if (auto panel = nn.outer.try_as<muxc::Panel>(); panel && !panel.Background()) panel.Background(brushOf(n8v_color{0, 0, 0, 0}));
    PNode *pn = nn.node;
    const std::string id = nn.registeredId;
    nn.hoverEnter = nn.outer.PointerEntered([this, pn, id](auto &&, auto &&) { post([this, pn, id] { setHover(pn, id, true); }); });
    nn.hoverExit = nn.outer.PointerExited([this, pn, id](auto &&, auto &&) { post([this, pn, id] { setHover(pn, id, false); }); });
    nn.hoverWired = true;
  }

  void setHover(PNode *pn, const std::string &id, bool hovered) {
    if (!live_.count(pn)) return;
    if (!id.empty()) {
      if (hovered) hovered_.insert(id);
      else hovered_.erase(id);
    }
    dispatch::hover(*pn, hovered);
  }

  void build(NativeNode &nn);
  void apply(NativeNode &nn);
  void applyFlex(NativeNode &nn, const n8v_flex_options &o);
  void applyPanel(NativeNode &nn, const n8v_panel_options &o);
  void applyFloating(NativeNode &nn, const n8v_floating_options &o);
  void applyButton(NativeNode &nn, const n8v_button_options &o);
  void applyText(NativeNode &nn, const n8v_text_options &o);
  void applyEntry(NativeNode &nn, const n8v_entry_options &o);
  void applyImage(NativeNode &nn, const n8v_image_options &o);
  void applyIcon(NativeNode &nn, const n8v_icon_options &o);
  void setItemHeight(const muxc::NavigationViewItem &item, double height);
  void applyMedia(NativeNode &nn, const char *source, bool autoplay, bool loop, bool muted, bool controls);
  void applyDocument(NativeNode &nn, const n8v_document_options &o);
  void applySidebar(NativeNode &nn, const n8v_sidebar_options &o);
  void applyPage(NativeNode &nn, const n8v_page_options &o);

  void buildSidebar(NativeNode &nn);
  void syncSidebarSelection(NativeNode &sidebar);

  mux::UIElement iconElement(const char *name, int size, n8v_color tint, bool onAccent = false);
  mux::UIElement imageElement(const DecodedImage *image, double width, double height);
  void updatePercentSizes();
  void updateFloating();
  void applyStickyBottom();
  void resolvePendingScrolls();
  void resolvePendingFocus();

  void wireClick(NativeNode &nn, const mux::UIElement &element);
  void wireEntryEvents(NativeNode &nn, muxc::TextBox box);
  static unsigned currentMods() {
    unsigned mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= N8V_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= N8V_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= N8V_MOD_ALT;
    return mods;
  }

  struct Pointer {
    bool pressed = false;
    bool dragged = false;
    wf::Point from{}, last{};
  };

  HWND hwnd_ = nullptr;
  std::atomic<bool> open_{true};
  bool shuttingDown_ = false;
  Bootstrap bootstrap_;

  std::thread uiThread_;
  DWORD uiThreadId_ = 0;
  std::atomic<bool> uiRunning_{false};
  winrt::Microsoft::UI::Dispatching::DispatcherQueue queue_{nullptr};
  HANDLE readyEvent_ = nullptr;
  HANDLE wakeEvent_ = nullptr;
  std::mutex eventsMutex_;
  std::deque<std::function<void()>> events_;
  bool initOk_ = false;
  std::string initError_;
  int initWidth_ = 800, initHeight_ = 600;
  std::string initTitle_;
  std::atomic<float> width_{0.0f}, height_{0.0f};

  mux::Window window_{nullptr};
  muxc::Grid rootGrid_{nullptr};
  NativeNode root_;

  std::unordered_set<PNode *> live_;
  std::unordered_map<std::string, NativeNode *> byId_;
  std::unordered_set<std::string> hovered_;
  std::unordered_map<void *, NativeNode *> itemNodes_;
  std::vector<PendingScroll> pendingScrolls_;
  std::vector<PendingFocus> pendingFocus_;
  int suppress_ = 0;
  int settleFrames_ = 0;
  bool popoverOpen_ = false;
  bool warnedPercent_ = false;
  bool trace_ = false;
  std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
  bool dumpedLate_ = false;
  std::atomic<bool> dark_{false};
  int frames_ = 0;
};

void WinUI3TreeBackend::wireClick(NativeNode &nn, const mux::UIElement &element) {
  NativeNode *self = &nn;
  auto state = std::make_shared<Pointer>();
  element.PointerPressed([self, state, element](auto &&, mux::Input::PointerRoutedEventArgs const &args) {
    state->pressed = true;
    state->dragged = false;
    state->from = state->last = args.GetCurrentPoint(element).Position();
    element.CapturePointer(args.Pointer());
  });
  element.PointerMoved([this, self, state, element](auto &&, mux::Input::PointerRoutedEventArgs const &args) {
    if (!state->pressed || !self->hasDrag) return;
    const wf::Point here = args.GetCurrentPoint(element).Position();
    if (!state->dragged && std::abs(here.X - state->from.X) + std::abs(here.Y - state->from.Y) < 4.0f) return;
    state->dragged = true;
    const float dx = here.X - state->last.X, dy = here.Y - state->last.Y;
    postTo(self->node, [dx, dy](PNode &p) { dispatch::drag(p, dx, dy); });
    state->last = here;
  });
  element.PointerReleased([this, self, state, element](auto &&, mux::Input::PointerRoutedEventArgs const &args) {
    if (!state->pressed) return;
    state->pressed = false;
    element.ReleasePointerCapture(args.Pointer());
    if (state->dragged) {
      state->dragged = false;
      return;
    }
    const auto fe = element.as<mux::FrameworkElement>();
    if (fe.ActualWidth() > 0 && fe.ActualHeight() > 0) {
      const wf::Point at = args.GetCurrentPoint(element).Position();
      const float x = (float)(at.X / fe.ActualWidth()), y = (float)(at.Y / fe.ActualHeight());
      postTo(self->node, [x, y](PNode &p) { dispatch::activateAt(p, x, y); });
    } else {
      postTo(self->node, [](PNode &p) { dispatch::activate(p); });
    }
  });
  element.PointerWheelChanged([this, self, element](auto &&, mux::Input::PointerRoutedEventArgs const &args) {
    if (!self->hasZoom || !(currentMods() & N8V_MOD_CTRL)) return;
    const auto fe = element.as<mux::FrameworkElement>();
    if (fe.ActualWidth() <= 0 || fe.ActualHeight() <= 0) return;
    const auto point = args.GetCurrentPoint(element);
    const float factor = std::pow(1.0015f, (float)point.Properties().MouseWheelDelta());
    const float x = (float)(point.Position().X / fe.ActualWidth()), y = (float)(point.Position().Y / fe.ActualHeight());
    postTo(self->node, [factor, x, y](PNode &p) { dispatch::zoom(p, factor, x, y); });
    args.Handled(true);
  });
}

void WinUI3TreeBackend::build(NativeNode &nn) {
  PNode &pn = *nn.node;
  NativeNode *self = &nn;
  auto finish = [&](mux::UIElement element) {
    nn.outer = element;
    nn.outerFe = element.as<mux::FrameworkElement>();
    nn.widget = nn.outerFe;
  };
  auto activate = [this, self](auto &&, auto &&) {
    if (suppress_) return;
    userEvent(self, [](PNode &p) { dispatch::activate(p); });
  };

  switch (nn.kind()) {
  case Kind::Flex: {
    muxc::Grid grid;
    nn.grid = grid;
    finish(grid);
    break;
  }
  case Kind::Floating: {
    muxc::Grid grid;
    nn.grid = grid;
    nn.border = muxc::Border();
    nn.border.Child(grid);
    nn.popup = muxp::Popup();
    nn.popup.Child(nn.border);
    nn.popup.Closed([this, self](auto &&, auto &&) {
      if (suppress_ || shuttingDown_ || !self->popupWantsOpen) return;
      userEvent(self, [](PNode &p) { dispatch::dismiss(p); });
    });
    nn.floating = true;
    nn.outer = nn.border;
    nn.outerFe = nn.border;
    break;
  }
  case Kind::Panel: {
    nn.border = muxc::Border();
    muxc::Grid holder;
    muxc::ColumnDefinition barColumn;
    barColumn.Width(mux::GridLength{1.0, mux::GridUnitType::Auto});
    muxc::ColumnDefinition contentColumn;
    contentColumn.Width(mux::GridLength{1.0, mux::GridUnitType::Star});
    holder.ColumnDefinitions().Append(barColumn);
    holder.ColumnDefinitions().Append(contentColumn);
    nn.accentBar = mux::Shapes::Rectangle();
    nn.accentBar.Width(4);
    nn.accentBar.Visibility(mux::Visibility::Collapsed);
    nn.accentBar.VerticalAlignment(mux::VerticalAlignment::Stretch);
    muxc::Grid::SetColumn(nn.accentBar, 0);
    nn.grid = muxc::Grid();
    muxc::Grid::SetColumn(nn.grid, 1);
    holder.Children().Append(nn.accentBar);
    holder.Children().Append(nn.grid);
    nn.border.Child(holder);
    finish(nn.border);
    break;
  }
  case Kind::Page: {
    nn.grid = muxc::Grid();
    nn.grid.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
    nn.grid.VerticalAlignment(mux::VerticalAlignment::Stretch);
    muxc::NavigationViewItem item;
    nn.item = item;
    itemNodes_[identityOf(nn.item)] = &nn;
    break;
  }
  case Kind::SidebarSection: {
    muxc::NavigationViewItemHeader header;
    nn.item = header;
    break;
  }
  case Kind::SidebarDivider: {
    muxc::NavigationViewItemSeparator separator;
    nn.item = separator;
    break;
  }
  case Kind::Text: {
    muxc::TextBlock text;
    text.TextWrapping(mux::TextWrapping::Wrap);
    text.Tapped([this, self](auto &&, auto &&) {
      if (self->tapActivates) userEvent(self, [](PNode &p) { dispatch::activate(p); });
    });
    finish(text);
    break;
  }
  case Kind::Button: {
    const auto *o = propsOf<n8v_button_options>(pn);
    if (o && o->checked) {
      muxp::ToggleButton toggle;
      toggle.Click(activate);
      finish(toggle);
    } else {
      muxc::Button button;
      button.Click(activate);
      finish(button);
    }
    break;
  }
  case Kind::Checkbox: {
    muxc::CheckBox box;
    auto changed = [this, self](auto &&, auto &&) {
      if (suppress_) return;
      const bool checked = self->widget.as<muxp::ToggleButton>().IsChecked().Value();
      userEvent(self, [checked](PNode &p) { dispatch::setChecked(p, checked); });
    };
    box.Checked(changed);
    box.Unchecked(changed);
    finish(box);
    break;
  }
  case Kind::Toggle: {
    muxc::ToggleSwitch toggle;
    toggle.Toggled([this, self](auto &&, auto &&) {
      if (suppress_) return;
      const bool on = self->widget.as<muxc::ToggleSwitch>().IsOn();
      userEvent(self, [on](PNode &p) { dispatch::setChecked(p, on); });
    });
    finish(toggle);
    break;
  }
  case Kind::Radio: {
    muxc::RadioButton radio;
    radio.GroupName(winrt::to_hstring((uint64_t)(uintptr_t)&pn));
    radio.Checked([this, self](auto &&, auto &&) {
      if (suppress_) return;
      userEvent(self, [](PNode &p) { dispatch::select(p, 0); });
    });
    radio.Unchecked([this, self](auto &&, auto &&) {
      if (suppress_) return;
      userEvent(self, [](PNode &) {});
    });
    finish(radio);
    break;
  }
  case Kind::Entry: {
    muxc::Grid holder;
    nn.textBox = muxc::TextBox();
    nn.passwordBox = muxc::PasswordBox();
    nn.passwordBox.Visibility(mux::Visibility::Collapsed);
    holder.Children().Append(nn.textBox);
    holder.Children().Append(nn.passwordBox);
    wireEntryEvents(nn, nn.textBox);
    nn.passwordBox.PasswordChanged([this, self](auto &&, auto &&) {
      if (suppress_) return;
      const std::string text = utf8(self->passwordBox.Password());
      userEvent(self, [text](PNode &p) {
        const auto *bound = std::get_if<std::string>(&p.bound);
        if (!bound || *bound != text) dispatch::setText(p, text);
      });
    });
    nn.passwordBox.KeyDown([this, self](auto &&, mux::Input::KeyRoutedEventArgs const &args) {
      if (args.Key() == winrt::Windows::System::VirtualKey::Enter) {
        userEvent(self, [](PNode &p) { dispatch::submit(p); });
        args.Handled(true);
      }
    });
    finish(holder);
    nn.widget = nn.textBox;
    break;
  }
  case Kind::Dropdown: {
    muxc::ComboBox combo;
    combo.SelectionChanged([this, self](auto &&, auto &&) {
      if (suppress_) return;
      const int index = self->widget.as<muxc::ComboBox>().SelectedIndex();
      if (index >= 0) userEvent(self, [index](PNode &p) { dispatch::select(p, index); });
    });
    finish(combo);
    break;
  }
  case Kind::Slider: {
    muxc::Slider slider;
    slider.ValueChanged([this, self](auto &&, muxp::RangeBaseValueChangedEventArgs const &args) {
      if (suppress_) return;
      const float value = (float)args.NewValue();
      userEvent(self, [value](PNode &p) { dispatch::setSlider(p, value); });
    });
    finish(slider);
    break;
  }
  case Kind::Image:
  case Kind::Icon: {
    muxc::Image image;
    image.HorizontalAlignment(mux::HorizontalAlignment::Left);
    image.VerticalAlignment(mux::VerticalAlignment::Top);
    finish(image);
    if (nn.kind() == Kind::Image) wireClick(nn, image);
    break;
  }
  case Kind::Divider: {
    mux::Shapes::Rectangle line;
    line.Height(1);
    finish(line);
    break;
  }
  case Kind::Audio:
  case Kind::Video: {
    nn.player = muxc::MediaPlayerElement();
    finish(nn.player);
    break;
  }
  case Kind::Document: {
    muxc::RichTextBlock text;
    text.TextWrapping(mux::TextWrapping::Wrap);
    finish(text);
    break;
  }
  case Kind::Sidebar:
    buildSidebar(nn);
    break;
  case Kind::Root:
    break;
  }
}

void WinUI3TreeBackend::wireEntryEvents(NativeNode &nn, muxc::TextBox box) {
  NativeNode *self = &nn;
  box.TextChanged([this, self](auto &&, auto &&) {
    if (suppress_) return;
    const std::string text = utf8(self->textBox.Text());
    userEvent(self, [text](PNode &p) {
      const auto *bound = std::get_if<std::string>(&p.bound);
      if (!bound || *bound != text) dispatch::setText(p, text);
    });
  });
  box.PreviewKeyDown([this, self](auto &&, mux::Input::KeyRoutedEventArgs const &args) {
    using winrt::Windows::System::VirtualKey;
    const unsigned mods = currentMods();
    const VirtualKey key = args.Key();
    if (key == VirtualKey::Up || key == VirtualKey::Down || key == VirtualKey::Escape) {
      if (!self->multiline || !self->hasOnKey) return;
      const n8v_key mapped = key == VirtualKey::Up ? N8V_KEY_UP : key == VirtualKey::Down ? N8V_KEY_DOWN : N8V_KEY_ESCAPE;
      if (askApp(self->node, [mapped, mods](PNode &p) { return dispatch::key(p, mapped, mods); })) args.Handled(true);
      return;
    }
    if (key != VirtualKey::Enter) return;
    if (!self->multiline || enterSubmits(self->submitMode, (mods & N8V_MOD_CTRL) != 0, (mods & N8V_MOD_SHIFT) != 0)) {
      userEvent(self, [](PNode &p) { dispatch::submit(p); });
      args.Handled(true);
    }
  });
  box.Paste([this, self](auto &&, muxc::TextControlPasteEventArgs const &args) {
    if (!self->hasPaste) return;
    auto view = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::GetContent();
    using winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats;
    if (!view.Contains(StandardDataFormats::StorageItems()) && !(view.Contains(StandardDataFormats::Bitmap()) && !view.Contains(StandardDataFormats::Text()))) return;
    args.Handled(true);
    PNode *pn = self->node;
    pasteAsync(view, [this, pn](std::vector<std::string> paths) { postTo(pn, [paths](PNode &p) { dispatch::pasteFiles(p, paths); }); });
  });
}

void WinUI3TreeBackend::buildSidebar(NativeNode &nn) {
  NativeNode *self = &nn;
  PNode *pnp = nn.node;
  nn.nav = muxc::NavigationView();
  nn.nav.IsBackButtonVisible(muxc::NavigationViewBackButtonVisible::Collapsed);
  nn.nav.IsSettingsVisible(false);
  nn.nav.PaneDisplayMode(muxc::NavigationViewPaneDisplayMode::Left);
  nn.nav.CompactPaneLength(64);
  nn.nav.IsTitleBarAutoPaddingEnabled(false);
  nn.nav.HorizontalAlignment(mux::HorizontalAlignment::Stretch);
  nn.nav.VerticalAlignment(mux::VerticalAlignment::Stretch);

  nn.titleText = muxc::TextBlock();
  nn.titleText.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
  nn.titleText.VerticalAlignment(mux::VerticalAlignment::Center);
  nn.header = muxc::StackPanel();
  nn.header.Orientation(muxc::Orientation::Horizontal);
  nn.header.Spacing(8);
  nn.header.Margin(mux::Thickness{12, 8, 12, 8});
  for (int right = 0; right < 2; ++right) {
    nn.headerButtons[right] = muxc::Button();
    nn.headerButtons[right].Visibility(mux::Visibility::Collapsed);
    nn.headerButtons[right].Click([this, pnp, right](auto &&, auto &&) {
      postTo(pnp, [right](PNode &p) {
        const auto *o = std::get_if<n8v_sidebar_options>(&p.current.props);
        if (!o) return;
        const n8v_sidebar_button &b = right ? o->right_button : o->left_button;
        if (b.on_click) b.on_click(b.on_click_userdata);
      });
    });
  }
  nn.header.Children().Append(nn.headerButtons[0]);
  nn.header.Children().Append(nn.titleText);
  nn.header.Children().Append(nn.headerButtons[1]);
  nn.nav.PaneHeader(nn.header);

  nn.nav.SelectionChanged([this, self](auto &&, muxc::NavigationViewSelectionChangedEventArgs const &args) {
    trace(suppress_ ? "selection changed (ignored)" : "selection changed");
    if (suppress_) return;
    auto selected = args.SelectedItem();
    if (!selected) return;
    auto it = itemNodes_.find(identityOf(selected));
    if (it == itemNodes_.end()) return;
    PNode *page = it->second->node;
    userEvent(self, [this, page](PNode &) {
      if (live_.count(page)) dispatch::selectPage(*page);
    });
  });
  nn.nav.RegisterPropertyChangedCallback(muxc::NavigationView::IsPaneOpenProperty(), [this, self, pnp](auto &&, auto &&) {
    if (suppress_ || !self->collapsible) return;
    const bool collapsed = !self->nav.IsPaneOpen();
    postTo(pnp, [collapsed](PNode &p) {
      const auto *o = std::get_if<n8v_sidebar_options>(&p.current.props);
      if (!o || !o->collapsed || *o->collapsed == collapsed) return;
      *o->collapsed = collapsed;
      if (o->on_collapse) o->on_collapse(collapsed, o->on_collapse_userdata);
    });
  });
  nn.outer = nn.nav;
  nn.outerFe = nn.nav;
  nn.widget = nn.nav;
}

void WinUI3TreeBackend::syncSidebarSelection(NativeNode &sidebar) {
  if (!sidebar.nav || userPending(sidebar)) return;
  int selected = 0;
  if (const int *bound = std::get_if<int>(&sidebar.node->bound)) selected = *bound;
  int pageNumber = 0;
  Quiet quiet(suppress_);
  for (NativeNode *kid : sidebar.kids) {
    if (kid->kind() != Kind::Page || isActionPage(*kid)) continue;
    if (pageNumber++ == selected) {
      if (sidebar.nav.SelectedItem() != kid->item) sidebar.nav.SelectedItem(kid->item);
      if (sidebar.nav.Content() != kid->grid) sidebar.nav.Content(kid->grid);
      return;
    }
  }
  sidebar.nav.Content(nullptr);
}

mux::UIElement WinUI3TreeBackend::imageElement(const DecodedImage *decoded, double width, double height) {
  muxc::Image image;
  image.Source(bitmapSourceOf(*decoded));
  image.Width(width);
  image.Height(height);
  image.Stretch(muxm::Stretch::Uniform);
  return image;
}

mux::UIElement WinUI3TreeBackend::iconElement(const char *name, int size, n8v_color tint, bool onAccent) {
  if (!name || !*name) return nullptr;
  const bool explicitTint = tint.a > 0.0f;
  const n8v_color use = explicitTint ? resolveThemeColor(tint, prefersDark())
                        : onAccent   ? (prefersDark() ? n8v_color{0, 0, 0, 255} : n8v_color{255, 255, 255, 255})
                                     : defaultTint();
  const int pixels = std::max(1, (int)std::lround(size * dpiScale()));
  const DecodedImage *decoded = getOrDecodeIcon(name, (uint16_t)pixels, n8v::Color{use.r, use.g, use.b, use.a});
  if (!decoded || decoded->width <= 0) return nullptr;
  return imageElement(decoded, size, size);
}

void WinUI3TreeBackend::applyFlex(NativeNode &nn, const n8v_flex_options &o) {
  nn.grid.Padding(thickness(o.padding));
  setScrolled(nn, o.clip_horizontal || o.clip_vertical, o.clip_horizontal, o.clip_vertical);
  nn.stickyBottom = o.stick_to_bottom && o.clip_vertical;
}

void WinUI3TreeBackend::applyPanel(NativeNode &nn, const n8v_panel_options &o) {
  const bool card = o.role == N8V_PANEL_ROLE_CARD;
  nn.border.CornerRadius(mux::CornerRadius{4, 4, 4, 4});
  nn.border.BorderThickness(mux::Thickness{1, 1, 1, 1});
  nn.border.Background(themeBrush(card ? L"CardBackgroundFillColorDefaultBrush" : L"LayerFillColorDefaultBrush", L"ControlFillColorDefaultBrush"));
  nn.border.BorderBrush(themeBrush(L"CardStrokeColorDefaultBrush", L"ControlStrokeColorDefaultBrush"));
  const double vpad = card ? 10 : 6, hpad = card ? 12 : 10;
  if (o.has_padding) nn.grid.Padding(thickness(o.padding));
  else nn.grid.Padding(mux::Thickness{hpad, vpad, hpad, vpad});
  const n8v_color accent = resolveThemeColor(o.accent, prefersDark());
  if (accent.a > 0.0f) {
    nn.accentBar.Fill(brushOf(accent));
    nn.accentBar.Visibility(mux::Visibility::Visible);
  } else {
    nn.accentBar.Visibility(mux::Visibility::Collapsed);
  }
}

void WinUI3TreeBackend::applyFloating(NativeNode &nn, const n8v_floating_options &o) {
  if (o.scrim) nn.border.Background(brushOf(n8v_color{0, 0, 0, 115}));
  else if (o.opaque) nn.border.Background(themeBrush(L"ApplicationPageBackgroundThemeBrush", L"SolidBackgroundFillColorBaseBrush"));
  else nn.border.Background(nullptr);
  nn.popup.IsLightDismissEnabled(o.on_dismiss != nullptr);
}

void WinUI3TreeBackend::applyButton(NativeNode &nn, const n8v_button_options &o) {
  auto *self = this;
  (void)self;
  auto button = nn.widget.as<muxc::ContentControl>();
  const std::string &label = nn.node->current.label;
  const bool hasIcon = o.icon && *o.icon;
  const bool hasImage = !hasIcon && o.image && *o.image;

  mux::UIElement glyph{nullptr};
  if (hasIcon) {
    glyph = iconElement(o.icon, kDefaultIconSize, n8v_color{0, 0, 0, 0}, o.style == N8V_BUTTON_STYLE_PRIMARY);
  } else if (hasImage) {
    n8v_image_options imageOptions{};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = o.image;
    if (const DecodedImage *decoded = getOrDecodeImage(imageOptions); decoded && decoded->width > 0 && decoded->height > 0) {
      const double height = o.image_height > 0 ? o.image_height : kDefaultIconSize * 1.15;
      glyph = imageElement(decoded, height * decoded->width / decoded->height, height);
    }
  }
  const std::string key = std::string(hasIcon ? o.icon : "") + (hasImage ? o.image : "") + "|" + label + "|" + std::to_string((int)o.icon_position) + "|" +
                          std::to_string((int)o.icon_variant) + "|" + std::to_string(o.image_height) + std::to_string((int)o.style) + (prefersDark() ? "d" : "l");
  if (key != nn.iconKey) {
    Quiet quiet(suppress_);
    if (glyph && label.empty()) {
      button.Content(glyph);
    } else if (glyph) {
      muxc::StackPanel row;
      row.Orientation(muxc::Orientation::Horizontal);
      row.Spacing(8);
      muxc::TextBlock text;
      text.Text(hs(label));
      if (o.icon_position == N8V_ICON_POSITION_TRAILING) {
        row.Children().Append(text);
        row.Children().Append(glyph);
      } else {
        row.Children().Append(glyph);
        row.Children().Append(text);
      }
      button.Content(row);
    } else {
      button.Content(winrt::box_value(hs(label)));
    }
    nn.iconKey = key;
  }

  Quiet quiet(suppress_);
  if (o.style == N8V_BUTTON_STYLE_PRIMARY) {
    if (auto style = styleResource(L"AccentButtonStyle")) button.Style(style);
    button.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
  } else if (o.style == N8V_BUTTON_STYLE_GHOST) {
    button.Style(nullptr);
    button.Background(brushOf(n8v_color{0, 0, 0, 0}));
    button.BorderThickness(mux::Thickness{0, 0, 0, 0});
  } else {
    button.Style(nullptr);
    button.ClearValue(muxc::Control::BackgroundProperty());
    button.ClearValue(muxc::Control::BorderThicknessProperty());
    button.FontWeight(winrt::Windows::UI::Text::FontWeights::Normal());
  }
  if (o.checked && !userPending(nn)) nn.widget.as<muxp::ToggleButton>().IsChecked(*o.checked);
}

void WinUI3TreeBackend::applyText(NativeNode &nn, const n8v_text_options &original) {
  auto block = nn.widget.as<muxc::TextBlock>();
  n8v_text_options o = original;
  o.color = resolveThemeColor(o.color, prefersDark());
  nn.tapActivates = o.on_click && o.click_unstyled;

  block.Inlines().Clear();
  muxd::Run run;
  run.Text(hs(nn.node->current.label));
  if (o.strikethrough) run.TextDecorations(winrt::Windows::UI::Text::TextDecorations::Strikethrough);

  const bool link = (o.url && *o.url) || (o.on_click && !o.click_unstyled);
  if (link) {
    muxd::Hyperlink hyperlink;
    hyperlink.Inlines().Append(run);
    NativeNode *self = &nn;
    hyperlink.Click([this, self](auto &&, auto &&) { userEvent(self, [](PNode &p) { dispatch::activate(p); }); });
    block.Inlines().Append(hyperlink);
  } else {
    block.Inlines().Append(run);
  }
  block.FontWeight(o.bold ? winrt::Windows::UI::Text::FontWeights::Bold() : winrt::Windows::UI::Text::FontWeights::Normal());
  block.FontStyle(o.italic ? winrt::Windows::UI::Text::FontStyle::Italic : winrt::Windows::UI::Text::FontStyle::Normal);
  if (o.large) block.FontSize(14.0 * 2.6);
  else if (o.small) block.FontSize(12.0);
  else block.ClearValue(muxc::TextBlock::FontSizeProperty());
  const bool isDefaultColor = o.color.a <= 0.0f || (o.color.r == 0.0f && o.color.g == 0.0f && o.color.b == 0.0f && o.color.a == 255.0f);
  if (isDefaultColor) block.ClearValue(muxc::TextBlock::ForegroundProperty());
  else block.Foreground(brushOf(o.color));
  block.Opacity(o.darken ? 0.6 : 1.0);
}

void WinUI3TreeBackend::applyEntry(NativeNode &nn, const n8v_entry_options &o) {
  Quiet quiet(suppress_);
  nn.multiline = o.multiline;
  nn.submitMode = o.submit_mode;
  nn.hasOnKey = o.on_key != nullptr;
  nn.hasPaste = o.on_paste_files != nullptr;
  const std::string *text = std::get_if<std::string>(&nn.node->bound);
  const winrt::hstring want = text ? hs(*text) : winrt::hstring();
  const bool settled = !userPending(nn);

  const bool password = o.password && !o.multiline;
  nn.textBox.Visibility(password ? mux::Visibility::Collapsed : mux::Visibility::Visible);
  nn.passwordBox.Visibility(password ? mux::Visibility::Visible : mux::Visibility::Collapsed);
  nn.textBox.PlaceholderText(hs(o.placeholder));
  nn.passwordBox.PlaceholderText(hs(o.placeholder));
  if (settled && nn.textBox.Text() != want) nn.textBox.Text(want);
  if (settled && nn.passwordBox.Password() != want) nn.passwordBox.Password(want);

  nn.textBox.AcceptsReturn(o.multiline);
  nn.textBox.TextWrapping(o.multiline ? mux::TextWrapping::Wrap : mux::TextWrapping::NoWrap);
  if (o.multiline) {
    constexpr double kLineHeight = 20.0, kChrome = 14.0;
    const EntryLines lines = entryLines(o);
    nn.textBox.MinHeight(lines.min * kLineHeight + kChrome);
    nn.textBox.MaxHeight(lines.max * kLineHeight + kChrome);
    if (lines.fixed) nn.textBox.Height(lines.min * kLineHeight + kChrome);
    else nn.textBox.ClearValue(mux::FrameworkElement::HeightProperty());
    muxc::ScrollViewer::SetVerticalScrollBarVisibility(nn.textBox, muxc::ScrollBarVisibility::Auto);
    nn.textBox.VerticalAlignment(mux::VerticalAlignment::Top);
  } else {
    nn.textBox.ClearValue(mux::FrameworkElement::HeightProperty());
    nn.textBox.ClearValue(mux::FrameworkElement::MinHeightProperty());
    nn.textBox.ClearValue(mux::FrameworkElement::MaxHeightProperty());
  }
  nn.widget = password ? mux::FrameworkElement(nn.passwordBox) : mux::FrameworkElement(nn.textBox);
}

void WinUI3TreeBackend::applyImage(NativeNode &nn, const n8v_image_options &o) {
  auto image = nn.widget.as<muxc::Image>();
  image.IsHitTestVisible(true);
  nn.hasDrag = o.on_drag != nullptr;
  nn.hasZoom = o.on_zoom != nullptr;
  const DecodedImage *decoded = getOrDecodeImage(o);
  if (!decoded) {
    image.Source(nullptr);
    nn.imageSource = nullptr;
    return;
  }
  const CornerRadius radius = cornerRadiusFor(o.rounding);
  const int targetW = o.width.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.width.value) : decoded->width;
  const int targetH = o.height.mode == N8V_SIZING_FIXED ? std::max(1, (int)o.height.value) : decoded->height;
  const DecodedImage *baked = getOrBakeRoundedImage(decoded, targetW, targetH, radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight);
  if (nn.imageSource != baked) {
    image.Source(bitmapSourceOf(*baked));
    nn.imageSource = baked;
  }
  const bool fixedW = o.width.mode == N8V_SIZING_FIXED, fixedH = o.height.mode == N8V_SIZING_FIXED;
  image.Stretch(
    fixedW && fixedH                                                                          ? muxm::Stretch::Fill
    : (fixedW || fixedH || o.width.mode != N8V_SIZING_FIT || o.height.mode != N8V_SIZING_FIT) ? muxm::Stretch::Uniform
                                                                                              : muxm::Stretch::None
  );
}

void WinUI3TreeBackend::applyIcon(NativeNode &nn, const n8v_icon_options &o) {
  auto image = nn.widget.as<muxc::Image>();
  const int size = o.width.mode == N8V_SIZING_FIXED ? (int)o.width.value : kDefaultIconSize;
  const std::string key = std::string(o.name ? o.name : "") + "@" + std::to_string(size) + (prefersDark() ? "d" : "l");
  if (key == nn.iconKey) return;
  nn.iconKey = key;
  const bool explicitTint = o.tint.a > 0.0f;
  const n8v_color use = explicitTint ? resolveThemeColor(o.tint, prefersDark()) : defaultTint();
  const int pixels = std::max(1, (int)std::lround(size * dpiScale()));
  if (const DecodedImage *decoded = getOrDecodeIcon(o.name ? o.name : "", (uint16_t)pixels, n8v::Color{use.r, use.g, use.b, use.a})) {
    image.Source(bitmapSourceOf(*decoded));
    image.Stretch(muxm::Stretch::Uniform);
    if (o.width.mode != N8V_SIZING_FIXED) image.Width(size);
    if (o.height.mode != N8V_SIZING_FIXED) image.Height(size);
  } else {
    image.Source(nullptr);
  }
}

void WinUI3TreeBackend::applyMedia(NativeNode &nn, const char *source, bool autoplay, bool loop, bool muted, bool controls) {
  const std::string wanted = source ? source : "";
  nn.player.AreTransportControlsEnabled(controls);
  nn.player.AutoPlay(autoplay);
  if (wanted != nn.mediaSource) {
    nn.mediaSource = wanted;
    if (wanted.empty()) {
      nn.player.Source(nullptr);
    } else {
      try {
        nn.player.Source(winrt::Windows::Media::Core::MediaSource::CreateFromUri(wf::Uri(mediaUri(wanted))));
      } catch (winrt::hresult_error const &e) {
        std::fprintf(stderr, "[n8v] winui3: could not open media '%s': %s\n", wanted.c_str(), winrt::to_string(e.message()).c_str());
      }
    }
  }
  if (auto player = nn.player.MediaPlayer()) {
    player.IsLoopingEnabled(loop);
    player.IsMuted(muted);
  }
}

void WinUI3TreeBackend::applyDocument(NativeNode &nn, const n8v_document_options &o) {
  auto text = nn.widget.as<muxc::RichTextBlock>();
  text.Blocks().Clear();
  const markdown::CachedDocument cached = markdown::getOrParse(o.markdown ? o.markdown : "");
  PNode *pn = nn.node;

  auto appendRuns = [&](const std::vector<markdown::InlineRun> &runs, muxd::Paragraph &paragraph, double fontSize, bool forceBold) {
    for (const markdown::InlineRun &r : runs) {
      if (!r.imageUrl.empty()) {
        const std::string path = o.image_resolver ? markdown::lookupImage(o.image_state, r.imageUrl) : std::string();
        if (!path.empty()) {
          n8v_image_options imageOptions{};
          imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
          imageOptions.path = path.c_str();
          if (const DecodedImage *decoded = getOrDecodeImage(imageOptions); decoded && decoded->height > 0) {
            const double height = r.imageSize > 0 ? r.imageSize : fontSize * 1.2;
            muxd::InlineUIContainer container;
            container.Child(imageElement(decoded, height * decoded->width / decoded->height, height));
            paragraph.Inlines().Append(container);
            continue;
          }
        }
      }
      muxd::Run run;
      run.Text(hs(r.text));
      if (r.bold || forceBold) run.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
      if (r.italic) run.FontStyle(winrt::Windows::UI::Text::FontStyle::Italic);
      if (r.strikethrough) run.TextDecorations(winrt::Windows::UI::Text::TextDecorations::Strikethrough);
      if (r.code) run.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
      if (r.linkUrl.empty()) {
        paragraph.Inlines().Append(run);
        continue;
      }
      n8v_link_style style{};
      const bool styled = o.link_style && o.link_style(r.linkUrl.c_str(), &style, o.link_style_userdata);
      if (styled) {
        if (style.foreground.a > 0.0f) run.Foreground(brushOf(resolveThemeColor(style.foreground, prefersDark())));
        if (style.bold) run.FontWeight(winrt::Windows::UI::Text::FontWeights::Bold());
        if (!style.clickable) {
          paragraph.Inlines().Append(run);
          continue;
        }
      }
      muxd::Hyperlink hyperlink;
      hyperlink.Inlines().Append(run);
      const std::string url = r.linkUrl;
      hyperlink.Click([this, pn, url](auto &&, auto &&) { openUrl(url); });
      if (styled && style.foreground.a > 0.0f) hyperlink.Foreground(brushOf(resolveThemeColor(style.foreground, prefersDark())));
      paragraph.Inlines().Append(hyperlink);
    }
  };

  std::function<void(const markdown::Block &, double)> appendBlock = [&](const markdown::Block &block, double indent) {
    using markdown::BlockKind;
    switch (block.kind) {
    case BlockKind::Heading: {
      static const double sizes[] = {28, 24, 20, 18, 16, 14};
      const double size = sizes[std::clamp(block.level, 1, 6) - 1];
      muxd::Paragraph paragraph;
      paragraph.FontSize(size);
      paragraph.Margin(mux::Thickness{indent, 8, 0, 4});
      appendRuns(block.inlines, paragraph, size, true);
      text.Blocks().Append(paragraph);
      break;
    }
    case BlockKind::Paragraph: {
      muxd::Paragraph paragraph;
      paragraph.Margin(mux::Thickness{indent, 0, 0, 8});
      appendRuns(block.inlines, paragraph, 14.0, false);
      text.Blocks().Append(paragraph);
      break;
    }
    case BlockKind::CodeBlock: {
      muxd::Paragraph paragraph;
      paragraph.FontFamily(muxm::FontFamily(L"Cascadia Mono, Consolas"));
      paragraph.Margin(mux::Thickness{indent + 8, 0, 0, 8});
      muxd::Run run;
      run.Text(hs(block.codeText));
      paragraph.Inlines().Append(run);
      text.Blocks().Append(paragraph);
      break;
    }
    case BlockKind::BulletList:
    case BlockKind::OrderedList: {
      int number = block.orderedStart;
      for (const auto &item : block.listItems) {
        muxd::Paragraph paragraph;
        paragraph.Margin(mux::Thickness{indent + 16, 0, 0, 2});
        paragraph.TextIndent(-14);
        muxd::Run marker;
        marker.Text(hs(block.kind == BlockKind::BulletList ? std::string("• ") : std::to_string(number++) + ". "));
        paragraph.Inlines().Append(marker);
        appendRuns(item, paragraph, 14.0, false);
        text.Blocks().Append(paragraph);
      }
      break;
    }
    case BlockKind::BlockQuote:
      for (const markdown::Block &child : block.children) appendBlock(child, indent + 16);
      break;
    case BlockKind::ThematicBreak: {
      muxd::Paragraph paragraph;
      muxd::Run run;
      run.Text(hs(std::string(40, '-')));
      paragraph.Inlines().Append(run);
      paragraph.Foreground(brushOf(n8v_color{128, 128, 128, 255}));
      text.Blocks().Append(paragraph);
      break;
    }
    }
  };
  for (const markdown::Block &block : *cached.document) appendBlock(block, 0.0);
}

void WinUI3TreeBackend::applySidebar(NativeNode &nn, const n8v_sidebar_options &o) {
  Quiet quiet(suppress_);
  const std::string title = o.title ? o.title : "";
  const bool collapsible = o.collapsed != nullptr;
  const bool collapsed = collapsible && *o.collapsed;
  nn.collapsible = collapsible;
  nn.titleText.Text(hs(title));
  nn.titleText.Visibility(title.empty() ? mux::Visibility::Collapsed : mux::Visibility::Visible);
  if (nn.nav.IsPaneToggleButtonVisible() != collapsible) nn.nav.IsPaneToggleButtonVisible(collapsible);
  const auto displayMode = collapsible ? muxc::NavigationViewPaneDisplayMode::LeftCompact : muxc::NavigationViewPaneDisplayMode::Left;
  if (nn.nav.PaneDisplayMode() != displayMode) nn.nav.PaneDisplayMode(displayMode);
  if (nn.nav.IsPaneOpen() == collapsed) nn.nav.IsPaneOpen(!collapsed);
  const double paneLength = o.width.mode == N8V_SIZING_FIXED ? o.width.value : 280.0;
  if (nn.nav.OpenPaneLength() != paneLength) nn.nav.OpenPaneLength(paneLength);
  if (nn.appliedCompact != (int)o.compact) {
    nn.appliedCompact = (int)o.compact;
    nn.nav.Resources().Insert(winrt::box_value(L"NavigationViewItemOnLeftMinHeight"), winrt::box_value(o.compact ? 32.0 : 40.0));
  }
  nn.header.Visibility(collapsed ? mux::Visibility::Collapsed : mux::Visibility::Visible);
  for (int right = 0; right < 2; ++right) {
    const n8v_sidebar_button &b = right ? o.right_button : o.left_button;
    const bool used = b.icon && *b.icon && !(right && collapsible);
    nn.headerButtons[right].Visibility(used ? mux::Visibility::Visible : mux::Visibility::Collapsed);
    if (!used) continue;
    if (auto glyph = iconElement(b.icon, kDefaultIconSize, n8v_color{0, 0, 0, 0})) nn.headerButtons[right].Content(glyph);
    if (b.tooltip) muxc::ToolTipService::SetToolTip(nn.headerButtons[right], winrt::box_value(hs(b.tooltip)));
  }
}

void WinUI3TreeBackend::setItemHeight(const muxc::NavigationViewItem &item, double height) {
  item.MinHeight(height);
  item.ApplyTemplate();
  trace("resizing a sidebar item");
  const double oldHeight = height < 40.0 ? 40.0 : 32.0;
  std::function<void(const mux::DependencyObject &, int)> walk = [&](const mux::DependencyObject &object, int depth) {
    if (depth > 8) return;
    const int count = muxm::VisualTreeHelper::GetChildrenCount(object);
    for (int i = 0; i < count; ++i) {
      auto child = muxm::VisualTreeHelper::GetChild(object, i);
      if (auto fe = child.try_as<mux::FrameworkElement>()) {
        const double actual = fe.ActualHeight();
        if (actual >= oldHeight - 0.5 && actual <= oldHeight + 5.0) {
          fe.MinHeight(height);
          if (!std::isnan(fe.Height())) fe.Height(height);
        }
      }
      walk(child, depth + 1);
    }
  };
  walk(item, 0);
}

void WinUI3TreeBackend::applyPage(NativeNode &nn, const n8v_page_options &o) {
  auto item = nn.item.as<muxc::NavigationViewItem>();
  nn.isAction = o.action;
  NativeNode *sidebar = nn.node->parent ? native(*nn.node->parent) : nullptr;
  const auto *sidebarProps = sidebar && sidebar->node ? propsOf<n8v_sidebar_options>(*sidebar->node) : nullptr;
  const bool compact = sidebarProps && sidebarProps->compact;
  const std::string shown = pageLabelWithBadges(o.name ? o.name : "", o.notify, o.badge);
  Quiet quiet(suppress_);
  if (shown != nn.appliedLabel) {
    item.Content(winrt::box_value(hs(shown)));
    nn.appliedLabel = shown;
  }
  if (nn.appliedCompact != (int)compact) {
    if (compact || nn.appliedCompact >= 0) setItemHeight(item, compact ? 32 : 40);
    nn.appliedCompact = (int)compact;
  }

  const bool collapsed = sidebarProps && sidebarProps->collapsed && *sidebarProps->collapsed;
  const double indent = collapsed ? 0.0 : std::max(0.0, (double)o.indent);
  if (indent != nn.appliedIndent) {
    item.Margin(mux::Thickness{indent, 0, 0, 0});
    nn.appliedIndent = indent;
  }

  const std::string key = std::string(o.image ? o.image : "") + "|" + (o.icon ? o.icon : "") + "|" + std::to_string(o.image_size) + (prefersDark() ? "d" : "l");
  if (key == nn.iconKey) return;
  nn.iconKey = key;
  if (o.image && *o.image && !(o.icon && *o.icon)) {
    const int slot = 28;
    const bool small = o.image_size > 0.0f;
    const int size = small ? std::max(8, (int)std::lround(o.image_size)) : slot;
    n8v_image_options imageOptions{};
    imageOptions.source_kind = N8V_IMAGE_SOURCE_PATH;
    imageOptions.path = o.image;
    const int pixels = std::max(1, (int)std::lround(size * dpiScale()));
    if (const DecodedImage *decoded = getOrFitImageInSquare(getOrDecodeImage(imageOptions), pixels)) {
      const CornerRadius radius = cornerRadiusFor(o.image_rounding);
      const DecodedImage *baked = getOrBakeRoundedImage(decoded, decoded->width, decoded->height, radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight);
      muxc::ImageIcon icon;
      icon.Source(bitmapSourceOf(*baked));
      icon.Width(size);
      icon.Height(size);
      item.Icon(icon);
      return;
    }
  } else if (o.icon && *o.icon) {
    const n8v_color tint = defaultTint();
    const int pixels = std::max(1, (int)std::lround(kDefaultIconSize * dpiScale()));
    if (const DecodedImage *decoded = getOrDecodeIcon(o.icon, (uint16_t)pixels, n8v::Color{tint.r, tint.g, tint.b, tint.a})) {
      muxc::ImageIcon icon;
      icon.Source(bitmapSourceOf(*decoded));
      icon.Width(kDefaultIconSize);
      icon.Height(kDefaultIconSize);
      item.Icon(icon);
      return;
    }
  }
  item.Icon(nullptr);
}

void WinUI3TreeBackend::apply(NativeNode &nn) {
  PNode &pn = *nn.node;
  Quiet quiet(suppress_);
  std::visit(
    Overloaded{
      [&](const std::monostate &) {},
      [&](const n8v_flex_options &o) { applyFlex(nn, o); },
      [&](const n8v_panel_options &o) { applyPanel(nn, o); },
      [&](const n8v_floating_options &o) { applyFloating(nn, o); },
      [&](const n8v_sidebar_options &o) { applySidebar(nn, o); },
      [&](const n8v_page_options &o) { applyPage(nn, o); },
      [&](const n8v_button_options &o) { applyButton(nn, o); },
      [&](const n8v_text_options &o) { applyText(nn, o); },
      [&](const n8v_checkbox_options &) {
        auto box = nn.widget.as<muxc::CheckBox>();
        box.Content(winrt::box_value(hs(pn.current.label)));
        if (const bool *checked = std::get_if<bool>(&pn.bound); checked && !userPending(nn)) box.IsChecked(*checked);
      },
      [&](const n8v_toggle_options &) {
        auto toggle = nn.widget.as<muxc::ToggleSwitch>();
        toggle.Header(winrt::box_value(hs(pn.current.label)));
        if (const bool *on = std::get_if<bool>(&pn.bound); on && !userPending(nn)) toggle.IsOn(*on);
      },
      [&](const n8v_radio_options &o) {
        auto radio = nn.widget.as<muxc::RadioButton>();
        radio.Content(winrt::box_value(hs(pn.current.label)));
        if (const int *selected = std::get_if<int>(&pn.bound); selected && !userPending(nn)) radio.IsChecked(*selected == o.value);
      },
      [&](const n8v_entry_options &o) { applyEntry(nn, o); },
      [&](const n8v_dropdown_options &o) {
        auto combo = nn.widget.as<muxc::ComboBox>();
        std::string key = std::string(o.placeholder ? o.placeholder : "") + '\n';
        for (size_t i = 0; i < o.item_count; ++i) key += std::string(o.items[i] ? o.items[i] : "") + '\n';
        if (key != nn.lastItemsKey) {
          combo.Items().Clear();
          for (size_t i = 0; i < o.item_count; ++i) combo.Items().Append(winrt::box_value(hs(o.items[i])));
          combo.PlaceholderText(hs(o.placeholder));
          nn.lastItemsKey = key;
        }
        const int *selected = std::get_if<int>(&pn.bound);
        const int want = selected && *selected >= 0 && (size_t)*selected < o.item_count ? *selected : -1;
        if (!userPending(nn) && combo.SelectedIndex() != want) combo.SelectedIndex(want);
      },
      [&](const n8v_slider_options &o) {
        auto slider = nn.widget.as<muxc::Slider>();
        slider.Minimum(o.min);
        slider.Maximum(o.max);
        slider.StepFrequency(std::max(1e-6, (double)(o.max - o.min) / 1000.0));
        if (const float *value = std::get_if<float>(&pn.bound); value && !userPending(nn)) {
          if (std::abs(slider.Value() - *value) > 1e-6) slider.Value(*value);
        }
      },
      [&](const n8v_image_options &o) { applyImage(nn, o); },
      [&](const n8v_divider_options &original) {
        n8v_divider_options o = original;
        o.color = resolveThemeColor(o.color, prefersDark());
        auto line = nn.widget.as<mux::Shapes::Rectangle>();
        line.Fill(o.color.a > 0.0f ? brushOf(o.color) : themeBrush(L"DividerStrokeColorDefaultBrush", L"ControlStrokeColorDefaultBrush"));
      },
      [&](const n8v_audio_options &o) { applyMedia(nn, o.source, o.autoplay, o.loop, o.muted, true); },
      [&](const n8v_video_options &o) { applyMedia(nn, o.source, o.autoplay, o.loop, o.muted, o.controls); },
      [&](const n8v_icon_options &o) { applyIcon(nn, o); },
      [&](const n8v_sidebar_section_options &o) { nn.item.as<muxc::NavigationViewItemHeader>().Content(winrt::box_value(hs(o.title))); },
      [&](const n8v_sidebar_divider_options &) {},
      [&](const n8v_document_options &o) { applyDocument(nn, o); },
    },
    pn.current.props
  );
}

void WinUI3TreeBackend::updatePercentSizes() {
  for (PNode *pn : live_) {
    NativeNode &nn = *native(*pn);
    if ((nn.percentW <= 0.0 && nn.percentH <= 0.0) || !nn.outerFe || !nn.parentNode) continue;
    warnOnce(warnedPercent_, "Sizing::percent along the main axis is approximated by weighting the space left over by the other children");
    const mux::FrameworkElement parent = nn.parentNode->kind() == Kind::Root ? mux::FrameworkElement(rootGrid_) : nn.parentNode->outerFe;
    if (!parent) continue;
    if (nn.percentW > 0.0) nn.outerFe.Width(std::max(0.0, parent.ActualWidth() * nn.percentW));
    if (nn.percentH > 0.0) nn.outerFe.Height(std::max(0.0, parent.ActualHeight() * nn.percentH));
  }
}

void WinUI3TreeBackend::updateFloating() {
  popoverOpen_ = false;
  if (!rootGrid_ || !rootGrid_.XamlRoot()) return;
  for (PNode *pn : live_) {
    NativeNode &nn = *native(*pn);
    if (!nn.floating || !nn.parentNode || !nn.popup) continue;
    const auto *o = propsOf<n8v_floating_options>(*pn);
    if (!o) continue;
    if (o->keep_on_screen) popoverOpen_ = true;

    NativeNode &host = *nn.parentNode;
    const mux::FrameworkElement hostElement = host.kind() == Kind::Root ? mux::FrameworkElement(rootGrid_) : (host.outerFe ? host.outerFe : mux::FrameworkElement(rootGrid_));
    wf::Point origin{0, 0};
    try {
      origin = hostElement.TransformToVisual(rootGrid_).TransformPoint(wf::Point{0, 0});
    } catch (winrt::hresult_error const &) {
      continue;
    }
    const double hostW = hostElement.ActualWidth(), hostH = hostElement.ActualHeight();
    const double windowW = rootGrid_.ActualWidth(), windowH = rootGrid_.ActualHeight();

    auto resolve = [&](const n8v_sizing &s, double hostExtent) -> double {
      double v = kNaN;
      switch (s.mode) {
      case N8V_SIZING_FIXED:
        v = s.value;
        break;
      case N8V_SIZING_GROW:
        v = hostExtent;
        break;
      case N8V_SIZING_PERCENT:
        v = s.value * hostExtent;
        break;
      case N8V_SIZING_FIT:
        return kNaN;
      }
      if (s.mode != N8V_SIZING_FIXED) {
        if (s.max > 0.0f) v = std::min(v, (double)s.max);
        if (s.min > 0.0f) v = std::max(v, (double)s.min);
      }
      return v;
    };
    const double wantW = resolve(o->width, hostW), wantH = resolve(o->height, hostH);
    nn.border.Width(wantW);
    nn.border.Height(wantH);
    nn.border.Measure(wf::Size{std::isnan(wantW) ? (float)windowW : (float)wantW, std::isnan(wantH) ? std::numeric_limits<float>::infinity() : (float)wantH});
    const wf::Size desired = nn.border.DesiredSize();
    const double w = std::isnan(wantW) ? desired.Width : wantW;
    const double h = std::isnan(wantH) ? desired.Height : wantH;

    double pfx = 0, pfy = 0, efx = 0, efy = 0;
    attachFraction(o->parent_attach_point, pfx, pfy);
    attachFraction(o->element_attach_point, efx, efy);
    double x = origin.X + pfx * hostW - efx * w + o->offset_x;
    double y = origin.Y + pfy * hostH - efy * h + o->offset_y;
    if (o->keep_on_screen) {
      const double margin = o->screen_margin;
      x = std::max(margin, std::min(x, windowW - w - margin));
      y = std::max(margin, std::min(y, windowH - h - margin));
    }
    if (nn.popup.XamlRoot() != rootGrid_.XamlRoot()) nn.popup.XamlRoot(rootGrid_.XamlRoot());
    nn.popup.HorizontalOffset(x);
    nn.popup.VerticalOffset(y);
    nn.popupWantsOpen = true;
    if (!nn.popup.IsOpen()) {
      Quiet quiet(suppress_);
      nn.popup.IsOpen(true);
    }
  }
}

void WinUI3TreeBackend::applyStickyBottom() {
  for (auto &[id, nn] : byId_) {
    if (!nn->stickyBottom || !nn->scroll) continue;
    const double max = nn->scroll.ScrollableHeight();
    if (max != nn->stickyLastMax) {
      if (nn->stickyAtBottom) nn->scroll.ChangeView(nullptr, max, nullptr, true);
      nn->stickyLastMax = max;
    }
    nn->stickyAtBottom = nn->scroll.VerticalOffset() >= max - kStickyBottomEpsilon;
  }
}

void WinUI3TreeBackend::resolvePendingFocus() {
  for (auto it = pendingFocus_.begin(); it != pendingFocus_.end();) {
    auto found = byId_.find(it->id);
    if (found != byId_.end() && found->second->widget) {
      if (auto control = found->second->widget.try_as<muxc::Control>()) control.Focus(mux::FocusState::Programmatic);
      it = pendingFocus_.erase(it);
    } else if (++it->framesWaited > 10) {
      it = pendingFocus_.erase(it);
    } else {
      ++it;
    }
  }
}

void WinUI3TreeBackend::resolvePendingScrolls() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = pendingScrolls_.begin(); it != pendingScrolls_.end();) {
    const NativeScrollRequest &req = it->request;
    if (now - it->requestedAt < kScrollSettleTime) {
      ++it;
      continue;
    }
    bool resolved = false;
    auto container = byId_.find(req.containerId);
    if (container != byId_.end() && container->second->scroll) {
      const muxc::ScrollViewer scroll = container->second->scroll;
      resolved = true;
      switch (req.kind) {
      case NativeScrollRequest::Kind::Bottom:
        scroll.ChangeView(nullptr, scroll.ScrollableHeight(), nullptr, true);
        break;
      case NativeScrollRequest::Kind::Top:
        scroll.ChangeView(nullptr, 0.0, nullptr, true);
        break;
      case NativeScrollRequest::Kind::Left:
        scroll.ChangeView(0.0, nullptr, nullptr, true);
        break;
      case NativeScrollRequest::Kind::Right:
        scroll.ChangeView(scroll.ScrollableWidth(), nullptr, nullptr, true);
        break;
      case NativeScrollRequest::Kind::Offset:
        scroll.ChangeView(std::clamp((double)req.x, 0.0, scroll.ScrollableWidth()), std::clamp((double)req.y, 0.0, scroll.ScrollableHeight()), nullptr, true);
        break;
      case NativeScrollRequest::Kind::Element: {
        auto element = byId_.find(req.elementId);
        if (element == byId_.end() || !element->second->outer) {
          resolved = false;
          break;
        }
        const mux::UIElement content = scroll.Content().as<mux::UIElement>();
        const wf::Point p = element->second->outer.TransformToVisual(content).TransformPoint(wf::Point{0, 0});
        scroll.ChangeView(
          std::clamp((double)p.X - req.paddingLeft, 0.0, scroll.ScrollableWidth()),
          std::clamp((double)p.Y - req.paddingTop, 0.0, scroll.ScrollableHeight()),
          nullptr,
          true
        );
        break;
      }
      }
    }
    if (resolved) {
      it = pendingScrolls_.erase(it);
    } else if (now - it->requestedAt > kScrollRequestTimeout) {
      std::fprintf(stderr, "[n8v] scroll request for container '%s' timed out (container or target element id not found)\n", req.containerId.c_str());
      it = pendingScrolls_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace

bool winui3Available() {
  if (hasPackageIdentity()) return true;
  HMODULE module = Bootstrap::load();
  if (!module) return false;
  FreeLibrary(module);
  return true;
}

std::unique_ptr<Backend> makeWinUI3TreeBackend() { return std::make_unique<WinUI3TreeBackend>(); }

} // namespace n8v::detail
