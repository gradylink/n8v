#pragma once

#include "core/image_loader.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace n8v::detail {

enum class ImageProtocol { Blocks, Kitty, KittyDirect };

ImageProtocol detectImageProtocol();

class KittyGraphics {
public:
  void setProtocol(ImageProtocol protocol) { protocol_ = protocol; }

  uint32_t ensure(const DecodedImage *image, int cols, int rows);

  void deleteAll();

  void beginFrame();
  void endFrame();
  void restartPlacements() {
    placements_.clear();
    frameOpen_ = protocol_ == ImageProtocol::KittyDirect;
  }
  void place(uint32_t id, int x, int y, int cellX0, int cellY0, int cellX1, int cellY1, int cols, int rows);

  static std::string placeholder(int row, int col);

  static void idToRgb(uint32_t id, uint8_t &r, uint8_t &g, uint8_t &b);

private:
  struct Placement {
    uint32_t id;
    int x, y, cols, rows;
    int srcX, srcY, srcW, srcH;

    bool operator==(const Placement &o) const {
      return id == o.id && x == o.x && y == o.y && cols == o.cols && rows == o.rows && srcX == o.srcX && srcY == o.srcY && srcW == o.srcW && srcH == o.srcH;
    }
  };

  void forgetUnused();

  ImageProtocol protocol_ = ImageProtocol::Kitty;
  std::map<uint32_t, std::pair<int, int>> pixelSize_;
  std::vector<Placement> placements_;
  std::vector<Placement> shown_;
  bool frameOpen_ = false;
  uint64_t frame_ = 0;
  std::map<uint32_t, uint64_t> lastUsed_;
  std::map<std::tuple<const DecodedImage *, int, int>, uint32_t> ids_;
  std::vector<uint32_t> sent_;
  uint32_t next_ = 0;
};

} // namespace n8v::detail
