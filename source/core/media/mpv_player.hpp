#pragma once

#include "core/media/animated_video.hpp"

#include <memory>
#include <string>

namespace n8v::detail {

struct MpvPlayerImpl;

struct MpvPlayerOptions {
  std::string source;
  bool video = true;
  int maxWidth = 0;
  int maxHeight = 0;
  bool loop = false;
  bool autoplay = false;
  bool muted = false;
};

struct MpvStatus {
  bool loaded = false;
  bool playing = false;
  bool ended = false;
  bool failed = false;
  double position = 0.0;
  double duration = 0.0;
};

class MpvPlayer {
public:
  static bool available();
  static std::unique_ptr<MpvPlayer> open(const MpvPlayerOptions &options);

  ~MpvPlayer();
  MpvPlayer(const MpvPlayer &) = delete;
  MpvPlayer &operator=(const MpvPlayer &) = delete;

  bool takeFrame(VideoFrame &out);
  MpvStatus status() const;

  void play();
  void pause();
  void seek(double seconds);
  void setMuted(bool muted);
  void setVolume(double volume);
  void setLoop(bool loop);
  void setMaxSize(int width, int height);

private:
  explicit MpvPlayer(std::shared_ptr<MpvPlayerImpl> impl);
  std::shared_ptr<MpvPlayerImpl> impl_;
};

} // namespace n8v::detail
