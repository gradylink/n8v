#include "static_backend.hpp"

#include "html_assets.hpp"
#include "html_builder.hpp"

#include "core/style.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

namespace n8v::detail {
namespace {

using html::Element;
using html::HtmlBuilder;

std::string escapeText(std::string_view in) {
  std::string out;
  for (char c : in) {
    if (c == '&') out += "&amp;";
    else if (c == '<') out += "&lt;";
    else if (c == '>') out += "&gt;";
    else out += c;
  }
  return out;
}

std::string prettify(const std::string &html) {
  std::string out;
  out.reserve(html.size() + html.size() / 8);
  for (size_t i = 0; i < html.size(); ++i) {
    out += html[i];
    if (html[i] == '>' && i + 1 < html.size() && html[i + 1] == '<') out += '\n';
  }
  return out;
}

class StaticHtmlBackend final : public Backend, public HtmlBuilder {
public:
  bool initialize(int width, int height, std::string_view title) override {
    width_ = width;
    height_ = height;
    title_ = title;
    if (const char *out = std::getenv("N8V_STATIC_OUT"); out && *out) outPath_ = out;
    pretty_ = std::getenv("N8V_STATIC_PRETTY") != nullptr;

    rootEl_ = Element::create("div");
    rootEl_.setAttr("id", "n8v-root");
    root_.outer = rootEl_;
    root_.content = rootEl_;
    installRootStyle();
    styleGeneration_ = activeStyleGeneration();
    return true;
  }

  bool pumpEvents() override {
    if (!framed_) {
      framed_ = true;
      return true;
    }
    write();
    return false;
  }

  bool pointerDown() const override { return false; }
  Clay_Dimensions windowSize() const override { return {(float)width_, (float)height_}; }
  Clay_Dimensions measureText(std::string_view, FontFamily, uint16_t, bool, bool) const override { return {0, 0}; }

  bool rendersFromClay() const override { return false; }
  tree::TreeOps *treeOps() override { return this; }
  void beginFrame() override {}
  void present(Clay_RenderCommandArray) override {}
  void setCursor(CursorKind) override {}
  void commitFrame() override {
    if (styleGeneration_ != activeStyleGeneration()) restyleAll();
  }

  void shutdown() override { write(); }

protected:
  void cssAppend(const std::string &css) override { dynamicCss_ += css; }
  void cssReset() override { dynamicCss_.clear(); }

  std::string fontUrl(const std::string &fileName, const std::vector<unsigned char> &bytes) override {
    return addAsset("assets/fonts/" + fileName, std::vector<unsigned char>(bytes));
  }

  std::string imageUrl(const DecodedImage &image) override {
    const std::vector<uint8_t> &png = html_assets::imagePng(image);
    return addAsset("assets/images/" + html_assets::contentHash(png.data(), png.size()) + ".png", std::vector<unsigned char>(png.begin(), png.end()));
  }

  std::string imagePathUrl(const std::string &path, bool) override {
    if (path.find("://") != std::string::npos) return path;
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const std::string name = std::filesystem::path(path).filename().string();
    return addAsset("assets/images/" + html_assets::contentHash(bytes.data(), bytes.size()).substr(0, 8) + "-" + name, std::move(bytes));
  }

private:
  std::string addAsset(const std::string &relativePath, std::vector<unsigned char> bytes) {
    assets_.emplace(relativePath, std::move(bytes));
    return relativePath;
  }

  std::string document() const {
    std::string out = "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>" +
                      escapeText(title_) + "</title><style id=\"n8v-base\">" + baseCss() + "</style><style id=\"n8v-dynamic\">" + dynamicCss_ + "</style></head><body>" +
                      rootEl_.html() + "</body></html>\n";
    return pretty_ ? prettify(out) : out;
  }

  void write() {
    if (written_) return;
    written_ = true;
    if (outPath_.empty()) {
      const std::string page = document();
      std::fwrite(page.data(), 1, page.size(), stdout);
      return;
    }
    namespace fs = std::filesystem;
    fs::path file = outPath_;
    if (file.extension() != ".html") file /= "index.html";
    const fs::path dir = file.parent_path();
    std::error_code ec;
    fs::create_directories(dir, ec);
    for (const auto &[relative, bytes] : assets_) {
      const fs::path target = dir / relative;
      fs::create_directories(target.parent_path(), ec);
      std::ofstream(target, std::ios::binary).write((const char *)bytes.data(), (std::streamsize)bytes.size());
    }
    std::ofstream(file, std::ios::binary) << document();
  }

  Element rootEl_;
  std::string title_;
  std::string outPath_;
  std::string dynamicCss_;
  std::map<std::string, std::vector<unsigned char>> assets_;
  int width_ = 0, height_ = 0;
  bool framed_ = false;
  bool written_ = false;
  bool pretty_ = false;
};

} // namespace

std::unique_ptr<Backend> makeStaticHtmlBackend() {
  setenv("N8V_THEME", "light", 0);
  return std::make_unique<StaticHtmlBackend>();
}

} // namespace n8v::detail
