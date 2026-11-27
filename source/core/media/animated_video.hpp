#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace n8v::detail {

struct AnimatedVideoState;

struct VideoFrame {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;
  bool opaque = false;
};

struct AnimatedVideoOptions {
  std::string source;
  int maxWidth = 0;
  int maxHeight = 0;
  bool loop = true;
  bool play = true;
};

class AnimatedVideo {
public:
  static bool available();

  static std::unique_ptr<AnimatedVideo> open(const AnimatedVideoOptions &options);

  ~AnimatedVideo();
  AnimatedVideo(const AnimatedVideo &) = delete;
  AnimatedVideo &operator=(const AnimatedVideo &) = delete;

  bool takeFrame(VideoFrame &out);

  void setPaused(bool paused);

  bool failed() const;

  bool running() const;

private:
  explicit AnimatedVideo(std::shared_ptr<AnimatedVideoState> state);
  std::shared_ptr<AnimatedVideoState> state_;
};

} // namespace n8v::detail
