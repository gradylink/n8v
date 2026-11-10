#include "file_dialog.hpp"

#include <windows.h>

#include <shobjidl.h>

#include <string>
#include <vector>

namespace n8v::detail {
namespace {

std::wstring widen(const std::string &utf8) {
  if (utf8.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

std::string narrow(const wchar_t *wide) {
  int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return {};
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
  return out;
}

template <typename T> struct ComPtr {
  T *p = nullptr;
  ComPtr() = default;
  ComPtr(const ComPtr &) = delete;
  ~ComPtr() {
    if (p) p->Release();
  }
  T **operator&() { return &p; }
  T *operator->() { return p; }
};

std::string pathOf(IShellItem *item) {
  PWSTR raw = nullptr;
  std::string out;
  if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &raw)) && raw) out = narrow(raw);
  CoTaskMemFree(raw);
  return out;
}

std::vector<std::string> run(const FileDialogRequest &request) {
  const bool save = request.kind == N8V_FILE_DIALOG_SAVE_FILE;
  ComPtr<IFileDialog> dialog;
  HRESULT hr = save ? CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))
                    : CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
  if (FAILED(hr)) return {};

  DWORD options = 0;
  dialog->GetOptions(&options);
  options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST;
  if (request.kind == N8V_FILE_DIALOG_OPEN_FOLDER) options |= FOS_PICKFOLDERS;
  if (request.kind != N8V_FILE_DIALOG_SAVE_FILE) options |= FOS_FILEMUSTEXIST;
  if (save) options |= FOS_OVERWRITEPROMPT;
  if (request.multiple) options |= FOS_ALLOWMULTISELECT;
  dialog->SetOptions(options);

  if (!request.title.empty()) dialog->SetTitle(widen(request.title).c_str());

  std::vector<std::wstring> names, specs;
  std::vector<COMDLG_FILTERSPEC> filterSpecs;
  if (request.kind != N8V_FILE_DIALOG_OPEN_FOLDER && !request.filters.empty()) {
    names.reserve(request.filters.size());
    specs.reserve(request.filters.size());
    for (const FileDialogFilter &f : request.filters) {
      std::string joined;
      for (const std::string &p : f.patterns) joined += (joined.empty() ? "" : ";") + p;
      names.push_back(widen(f.name));
      specs.push_back(widen(joined));
    }
    for (size_t i = 0; i < names.size(); ++i) filterSpecs.push_back({names[i].c_str(), specs[i].c_str()});
    dialog->SetFileTypes(static_cast<UINT>(filterSpecs.size()), filterSpecs.data());
    dialog->SetFileTypeIndex(1);
    if (save) {
      std::string ext = globExtension(request.filters[0].patterns.empty() ? "" : request.filters[0].patterns[0]);
      if (!ext.empty()) dialog->SetDefaultExtension(widen(ext).c_str());
    }
  }

  if (!request.initialDir.empty()) {
    ComPtr<IShellItem> folder;
    if (SUCCEEDED(SHCreateItemFromParsingName(widen(request.initialDir).c_str(), nullptr, IID_PPV_ARGS(&folder)))) dialog->SetFolder(folder.p);
  }
  if (save && !request.defaultName.empty()) dialog->SetFileName(widen(request.defaultName).c_str());

  HWND owner = GetActiveWindow();
  if (!owner) owner = GetForegroundWindow();
  if (FAILED(dialog->Show(owner))) return {};

  std::vector<std::string> paths;
  if (request.multiple && !save) {
    ComPtr<IFileOpenDialog> open;
    ComPtr<IShellItemArray> items;
    if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&open))) && SUCCEEDED(open->GetResults(&items))) {
      DWORD count = 0;
      items->GetCount(&count);
      for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        if (SUCCEEDED(items->GetItemAt(i, &item))) {
          std::string path = pathOf(item.p);
          if (!path.empty()) paths.push_back(std::move(path));
        }
      }
    }
  } else {
    ComPtr<IShellItem> item;
    if (SUCCEEDED(dialog->GetResult(&item))) {
      std::string path = pathOf(item.p);
      if (!path.empty()) paths.push_back(std::move(path));
    }
  }
  return paths;
}

} // namespace

bool startPlatformFileDialog(const FileDialogRequest &request, FileDialogDone done) {
  const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(init) && init != RPC_E_CHANGED_MODE) return false;

  std::vector<std::string> paths = run(request);

  if (SUCCEEDED(init)) CoUninitialize();
  done({paths.empty() ? FileDialogResult::Status::Cancelled : FileDialogResult::Status::Done, std::move(paths)});
  return true;
}

void pollPlatformFileDialog() {}

} // namespace n8v::detail
