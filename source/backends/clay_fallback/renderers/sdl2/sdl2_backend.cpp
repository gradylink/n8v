#include "sdl2_backend.hpp"

#include "backends/clay_fallback/text/line_layout.hpp"
#include "core/text_style_flags.hpp"

#include <SDL2/SDL.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace n8v::detail {
namespace {

class Sdl2Backend final : public Backend {
public:
  ~Sdl2Backend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      std::fprintf(stderr, "[n8v] SDL_Init failed: %s\n", SDL_GetError());
      return false;
    }

    window_ = SDL_CreateWindow(std::string(title).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      std::fprintf(stderr, "[n8v] SDL_CreateWindow failed: %s\n", SDL_GetError());
      return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) {
      std::fprintf(stderr, "[n8v] SDL_CreateRenderer failed: %s\n", SDL_GetError());
      return false;
    }

    return true;
  }

  bool pumpEvents() override {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_QUIT:
        return false;
      case SDL_MOUSEMOTION:
        pointerX_ = (float)event.motion.x;
        pointerY_ = (float)event.motion.y;
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (event.button.button == SDL_BUTTON_LEFT) pointerDown_ = true;
        break;
      case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT) pointerDown_ = false;
        break;
      default:
        break;
      }
    }
    return true;
  }

  Clay_Dimensions windowSize() const override {
    int width = 0, height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    return {(float)width, (float)height};
  }

  Clay_Dimensions measureText(std::string_view text, uint16_t fontSize, bool bold, bool italic) const override { return measureLine(text, fontSize, bold, italic); }

  void beginFrame() override {
    Clay_SetPointerState({pointerX_, pointerY_}, pointerDown_);
    SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
    SDL_RenderClear(renderer_);
  }

  void present(Clay_RenderCommandArray commands) override {
    for (int32_t i = 0; i < commands.length; ++i) {
      Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
      switch (command->commandType) {
      case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
        const Clay_Color &color = command->renderData.rectangle.backgroundColor;
        if (color.a <= 0.0f) break;
        drawFilledRect(command->boundingBox, color);
        break;
      }
      case CLAY_RENDER_COMMAND_TYPE_TEXT: {
        drawText(*command);
        break;
      }
      default:
        break;
      }
    }
    SDL_RenderPresent(renderer_);
  }

  void shutdown() override {
    if (renderer_) {
      SDL_DestroyRenderer(renderer_);
      renderer_ = nullptr;
    }
    if (window_) {
      SDL_DestroyWindow(window_);
      window_ = nullptr;
    }
    SDL_Quit();
  }

private:
  void uploadAtlas(FontGeneration &gen) {
    std::vector<uint8_t> rgba((size_t)gen.atlasWidth * (size_t)gen.atlasHeight * 4);
    for (size_t i = 0; i < gen.pixels.size(); ++i) {
      rgba[i * 4 + 0] = 255;
      rgba[i * 4 + 1] = 255;
      rgba[i * 4 + 2] = 255;
      rgba[i * 4 + 3] = gen.pixels[i];
    }

    if (gen.backendHandle) {
      SDL_DestroyTexture((SDL_Texture *)gen.backendHandle);
      gen.backendHandle = nullptr;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, gen.atlasWidth, gen.atlasHeight);
    if (texture) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      SDL_UpdateTexture(texture, nullptr, rgba.data(), gen.atlasWidth * 4);
    }

    gen.backendHandle = texture;
    gen.destroyBackendHandle = [](void *handle) { SDL_DestroyTexture((SDL_Texture *)handle); };
    gen.dirty = false;
  }

  void drawText(const Clay_RenderCommand &command) {
    const Clay_TextRenderData &textData = command.renderData.text;
    auto *flags = static_cast<TextStyleFlags *>(command.userData);
    bool bold = flags && flags->bold;
    bool italic = flags && flags->italic;
    bool underline = flags && flags->underline;

    LineLayoutResult layout;
    std::string_view text(textData.stringContents.chars, (size_t)textData.stringContents.length);
    if (!layoutLine(text, textData.fontSize, bold, italic, layout) || !layout.generation) {
      Clay_Color block = textData.textColor;
      block.a = 160;
      drawFilledRect(command.boundingBox, block);
      return;
    }

    if (layout.generation->dirty) uploadAtlas(*layout.generation);
    auto *texture = (SDL_Texture *)layout.generation->backendHandle;
    if (!texture) return;

    SDL_Color tint{(Uint8)textData.textColor.r, (Uint8)textData.textColor.g, (Uint8)textData.textColor.b, (Uint8)textData.textColor.a};

    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;
    vertices.reserve(layout.quads.size() * 4);
    indices.reserve(layout.quads.size() * 6);

    for (const GlyphQuad &quad : layout.quads) {
      float x0 = command.boundingBox.x + quad.x0 * layout.scale;
      float y0 = command.boundingBox.y + quad.y0 * layout.scale + layout.generation->ascent * layout.scale;
      float x1 = command.boundingBox.x + quad.x1 * layout.scale;
      float y1 = command.boundingBox.y + quad.y1 * layout.scale + layout.generation->ascent * layout.scale;

      int base = (int)vertices.size();
      vertices.push_back({{x0, y0}, tint, {quad.s0, quad.t0}});
      vertices.push_back({{x1, y0}, tint, {quad.s1, quad.t0}});
      vertices.push_back({{x1, y1}, tint, {quad.s1, quad.t1}});
      vertices.push_back({{x0, y1}, tint, {quad.s0, quad.t1}});
      indices.insert(indices.end(), {base, base + 1, base + 2, base + 2, base + 3, base});
    }

    if (!vertices.empty()) {
      SDL_RenderGeometry(renderer_, texture, vertices.data(), (int)vertices.size(), indices.data(), (int)indices.size());
    }

    if (underline) {
      SDL_SetRenderDrawColor(renderer_, tint.r, tint.g, tint.b, tint.a);
      float underlineY = command.boundingBox.y + command.boundingBox.height - 1.0f;
      SDL_RenderDrawLine(renderer_, (int)command.boundingBox.x, (int)underlineY, (int)(command.boundingBox.x + layout.width), (int)underlineY);
    }
  }

  void drawFilledRect(const Clay_BoundingBox &box, const Clay_Color &color) {
    SDL_SetRenderDrawColor(renderer_, (Uint8)color.r, (Uint8)color.g, (Uint8)color.b, (Uint8)color.a);
    SDL_Rect rect{(int)box.x, (int)box.y, (int)box.width, (int)box.height};
    SDL_RenderFillRect(renderer_, &rect);
  }

  SDL_Window *window_ = nullptr;
  SDL_Renderer *renderer_ = nullptr;
  float pointerX_ = 0.0f, pointerY_ = 0.0f;
  bool pointerDown_ = false;
};

} // namespace

std::unique_ptr<Backend> makeSdl2Backend() { return std::make_unique<Sdl2Backend>(); }

} // namespace n8v::detail
