#include "core/freedesktop_icon_theme.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

namespace n8v::detail {
namespace {

namespace fs = std::filesystem;

std::vector<std::string> splitCsv(const std::string &line) {
  std::vector<std::string> out;
  std::stringstream ss(line);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) out.push_back(item);
  }
  return out;
}

bool readIndexThemeKey(const fs::path &indexPath, std::string_view key, std::string &out) {
  std::ifstream file(indexPath);
  if (!file) return false;
  std::string line;
  while (std::getline(file, line)) {
    if (line.compare(0, key.size(), key) == 0 && line.size() > key.size() && line[key.size()] == '=') {
      out = line.substr(key.size() + 1);
      while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) out.pop_back();
      return true;
    }
  }
  return false;
}

std::vector<std::string> iconThemeBaseDirs() {
  std::vector<std::string> dirs;
  if (const char *home = std::getenv("HOME")) dirs.push_back(std::string(home) + "/.icons");

  if (const char *xdgDataHome = std::getenv("XDG_DATA_HOME")) {
    dirs.push_back(std::string(xdgDataHome) + "/icons");
  } else if (const char *home = std::getenv("HOME")) {
    dirs.push_back(std::string(home) + "/.local/share/icons");
  }

  std::string xdgDataDirs = "/usr/local/share/:/usr/share/";
  if (const char *env = std::getenv("XDG_DATA_DIRS")) xdgDataDirs = env;
  std::stringstream ss(xdgDataDirs);
  std::string dir;
  while (std::getline(ss, dir, ':')) {
    if (!dir.empty()) {
      if (dir.back() == '/') dir.pop_back();
      dirs.push_back(dir + "/icons");
    }
  }
  return dirs;
}

std::string detectActiveThemeName() {
  if (const char *env = std::getenv("N8V_ICON_THEME"); env && *env) return env;

  if (FILE *pipe = popen("gsettings get org.gnome.desktop.interface icon-theme 2>/dev/null", "r")) {
    char buf[256] = {0};
    std::string result;
    while (fgets(buf, sizeof(buf), pipe)) result += buf;
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) result.pop_back();
    if (result.size() >= 2 && result.front() == '\'' && result.back() == '\'') result = result.substr(1, result.size() - 2);
    if (!result.empty()) return result;
  }

  if (const char *home = std::getenv("HOME")) {
    std::string theme;
    if (readIndexThemeKey(fs::path(home) / ".config/kdeglobals", "Theme", theme) && !theme.empty()) return theme;
  }

  return "hicolor";
}

std::string findIconInDirectories(const fs::path &themeDir, const std::vector<std::string> &subdirs, std::string_view name) {
  for (const std::string &subdir : subdirs) {
    for (const char *ext : {"svg", "png"}) {
      fs::path candidate = themeDir / subdir / (std::string(name) + "." + ext);
      std::error_code ec;
      if (fs::exists(candidate, ec)) return candidate.string();
    }
  }
  return {};
}

std::string findThemeDir(const std::vector<std::string> &baseDirs, std::string_view themeName) {
  for (const std::string &base : baseDirs) {
    fs::path themeDir = fs::path(base) / std::string(themeName);
    std::error_code ec;
    if (fs::exists(themeDir / "index.theme", ec)) return themeDir.string();
  }
  return {};
}

std::string findInTheme(const std::vector<std::string> &baseDirs, std::string_view themeName, std::string_view name, std::set<std::string> &visited) {
  if (!visited.insert(std::string(themeName)).second) return {};

  std::string themeDirStr = findThemeDir(baseDirs, themeName);
  if (themeDirStr.empty()) return {};
  fs::path themeDir(themeDirStr);

  std::string directoriesLine;
  readIndexThemeKey(themeDir / "index.theme", "Directories", directoriesLine);
  std::string found = findIconInDirectories(themeDir, splitCsv(directoriesLine), name);
  if (!found.empty()) return found;

  std::string inheritsLine;
  readIndexThemeKey(themeDir / "index.theme", "Inherits", inheritsLine);
  for (const std::string &parent : splitCsv(inheritsLine)) {
    found = findInTheme(baseDirs, parent, name, visited);
    if (!found.empty()) return found;
  }
  return {};
}

} // namespace

std::string findFreedesktopIconFile(std::string_view name) {
  if (name.empty()) return {};

  static const std::vector<std::string> baseDirs = iconThemeBaseDirs();
  static const std::string activeTheme = detectActiveThemeName();

  std::set<std::string> visited;
  std::string found = findInTheme(baseDirs, activeTheme, name, visited);
  if (!found.empty()) return found;

  if (activeTheme != "hicolor") {
    std::set<std::string> hicolorVisited;
    found = findInTheme(baseDirs, "hicolor", name, hicolorVisited);
    if (!found.empty()) return found;
  }

  for (const char *ext : {"svg", "png"}) {
    fs::path candidate = fs::path("/usr/share/pixmaps") / (std::string(name) + "." + ext);
    std::error_code ec;
    if (fs::exists(candidate, ec)) return candidate.string();
  }

  return {};
}

} // namespace n8v::detail
