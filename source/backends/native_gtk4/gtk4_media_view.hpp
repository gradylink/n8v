#pragma once

#include <gtk/gtk.h>

#include <functional>
#include <memory>
#include <string>

namespace n8v::detail {

struct MediaViewOptions {
  std::string source;
  bool autoplay = false;
  bool loop = false;
  bool muted = false;
  bool fullscreenButton = false;
  bool isFullscreen = false;
};

class MediaView {
public:
  MediaView(bool video, std::function<void(bool)> requestFullscreen);
  ~MediaView();
  MediaView(const MediaView &) = delete;
  MediaView &operator=(const MediaView &) = delete;

  GtkWidget *widget() const;
  void update(const MediaViewOptions &options);

  static MediaView *of(GtkWidget *widget);

  struct Impl;

private:
  std::unique_ptr<Impl> impl_;
};

} // namespace n8v::detail
