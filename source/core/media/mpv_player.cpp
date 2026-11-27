#include "core/media/mpv_player.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>

#ifdef N8V_HAVE_MPV
#include "n8v_mpv_names.h"

extern "C" {
#include <mpv/client.h>
#include <mpv/render.h>

int lzy_n8v_mpv_lazy_is_available(void);
}
#endif

namespace n8v::detail {

namespace {
using Clock = std::chrono::steady_clock;

enum class CmdKind { Play, Pause, Seek, Mute, Volume, Loop, Resize };
struct Cmd {
  CmdKind kind;
  double number = 0.0;
  int w = 0, h = 0;
};
} // namespace

struct MpvPlayerImpl {
  std::mutex m;
  std::condition_variable cv;
  std::atomic<bool> stop{false};
  bool wake = false;
  std::deque<Cmd> commands;
  VideoFrame frame;
  uint64_t serial = 0, taken = 0;
  MpvStatus status;

  void poke() {
    {
      std::lock_guard<std::mutex> lock(m);
      wake = true;
    }
    cv.notify_all();
  }
  void push(Cmd c) {
    {
      std::lock_guard<std::mutex> lock(m);
      commands.push_back(c);
      wake = true;
    }
    cv.notify_all();
  }
};

MpvPlayer::MpvPlayer(std::shared_ptr<MpvPlayerImpl> impl) : impl_(std::move(impl)) {}

MpvPlayer::~MpvPlayer() {
  if (!impl_) return;
  {
    std::lock_guard<std::mutex> lock(impl_->m);
    impl_->stop.store(true);
    impl_->wake = true;
  }
  impl_->cv.notify_all();
}

bool MpvPlayer::takeFrame(VideoFrame &out) {
  std::lock_guard<std::mutex> lock(impl_->m);
  if (impl_->serial == impl_->taken) return false;
  out = std::move(impl_->frame);
  impl_->frame = VideoFrame{};
  impl_->taken = impl_->serial;
  return true;
}

MpvStatus MpvPlayer::status() const {
  std::lock_guard<std::mutex> lock(impl_->m);
  return impl_->status;
}

void MpvPlayer::play() { impl_->push({CmdKind::Play}); }
void MpvPlayer::pause() { impl_->push({CmdKind::Pause}); }
void MpvPlayer::seek(double seconds) { impl_->push({CmdKind::Seek, seconds}); }
void MpvPlayer::setMuted(bool muted) { impl_->push({CmdKind::Mute, muted ? 1.0 : 0.0}); }
void MpvPlayer::setVolume(double volume) { impl_->push({CmdKind::Volume, std::clamp(volume, 0.0, 1.0)}); }
void MpvPlayer::setLoop(bool loop) { impl_->push({CmdKind::Loop, loop ? 1.0 : 0.0}); }
void MpvPlayer::setMaxSize(int width, int height) {
  Cmd c{CmdKind::Resize};
  c.w = width;
  c.h = height;
  impl_->push(c);
}

#ifndef N8V_HAVE_MPV

bool MpvPlayer::available() { return false; }
std::unique_ptr<MpvPlayer> MpvPlayer::open(const MpvPlayerOptions &) { return nullptr; }

#else

namespace {

void onRenderUpdate(void *ctx) { static_cast<MpvPlayerImpl *>(ctx)->poke(); }
void onWakeup(void *ctx) { static_cast<MpvPlayerImpl *>(ctx)->poke(); }

double getDouble(mpv_handle *h, const char *name, double fallback) {
  double v = fallback;
  return mpv_get_property(h, name, MPV_FORMAT_DOUBLE, &v) >= 0 ? v : fallback;
}
bool getFlag(mpv_handle *h, const char *name, bool fallback) {
  int v = fallback ? 1 : 0;
  return mpv_get_property(h, name, MPV_FORMAT_FLAG, &v) >= 0 ? v != 0 : fallback;
}
int64_t getInt(mpv_handle *h, const char *name) {
  int64_t v = 0;
  return mpv_get_property(h, name, MPV_FORMAT_INT64, &v) >= 0 ? v : 0;
}
void setFlag(mpv_handle *h, const char *name, bool value) {
  int v = value ? 1 : 0;
  mpv_set_property(h, name, MPV_FORMAT_FLAG, &v);
}

void runPlayer(std::shared_ptr<MpvPlayerImpl> state, MpvPlayerOptions options) {
  MpvPlayerImpl &s = *state;

  auto fail = [&] {
    std::lock_guard<std::mutex> lock(s.m);
    s.status.failed = true;
    s.status.playing = false;
  };

  mpv_handle *mpv = mpv_create();
  if (!mpv) return fail();

  mpv_set_option_string(mpv, "config", "no");
  mpv_set_option_string(mpv, "load-scripts", "no");
  mpv_set_option_string(mpv, "osc", "no");
  mpv_set_option_string(mpv, "terminal", "no");
  mpv_set_option_string(mpv, "input-default-bindings", "no");
  mpv_set_option_string(mpv, "input-vo-keyboard", "no");
  mpv_set_option_string(mpv, "ytdl", "no");
  mpv_set_option_string(mpv, "msg-level", "all=no");
  mpv_set_option_string(mpv, "audio-client-name", "n8v");
  mpv_set_option_string(mpv, "keep-open", "yes");
  mpv_set_option_string(mpv, "idle", "yes");
  mpv_set_option_string(mpv, "force-window", "no");
  mpv_set_option_string(mpv, "hwdec", "no");
  mpv_set_option_string(mpv, "vo", "libmpv");
  if (!options.video) mpv_set_option_string(mpv, "vid", "no");
  mpv_set_option_string(mpv, "loop-file", options.loop ? "inf" : "no");
  mpv_set_option_string(mpv, "pause", options.autoplay ? "no" : "yes");
  mpv_set_option_string(mpv, "mute", options.muted ? "yes" : "no");

  if (const char *extra = std::getenv("N8V_MPV_OPTIONS")) {
    std::string list = extra;
    for (size_t start = 0; start < list.size();) {
      size_t end = list.find(',', start);
      if (end == std::string::npos) end = list.size();
      const std::string item = list.substr(start, end - start);
      const size_t eq = item.find('=');
      if (eq != std::string::npos && eq > 0) mpv_set_option_string(mpv, item.substr(0, eq).c_str(), item.substr(eq + 1).c_str());
      start = end + 1;
    }
  }

  if (mpv_initialize(mpv) < 0) {
    mpv_terminate_destroy(mpv);
    return fail();
  }

  mpv_render_context *render = nullptr;
  if (options.video) {
    const char *api = MPV_RENDER_API_TYPE_SW;
    mpv_render_param createParams[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(api)}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&render, mpv, createParams) < 0) {
      mpv_terminate_destroy(mpv);
      return fail();
    }
    mpv_render_context_set_update_callback(render, onRenderUpdate, &s);
  }
  mpv_set_wakeup_callback(mpv, onWakeup, &s);

  {
    const std::string source = options.source;
    const char *loadArgs[] = {"loadfile", source.c_str(), nullptr};
    if (mpv_command(mpv, loadArgs) < 0) {
      if (render) mpv_render_context_free(render);
      mpv_terminate_destroy(mpv);
      return fail();
    }
  }

  int maxW = options.maxWidth, maxH = options.maxHeight;
  bool needRender = false;
  bool loaded = false;
  Clock::time_point lastPoll{};
  std::vector<uint8_t> buffer;

  auto renderFrame = [&] {
    if (!render) return;
    const int64_t vw = getInt(mpv, "dwidth"), vh = getInt(mpv, "dheight");
    if (vw <= 0 || vh <= 0) return;
    double scale = 1.0;
    if (maxW > 0) scale = std::min(scale, (double)maxW / (double)vw);
    if (maxH > 0) scale = std::min(scale, (double)maxH / (double)vh);
    const int w = std::max(1, (int)std::lround((double)vw * scale));
    const int h = std::max(1, (int)std::lround((double)vh * scale));
    size_t stride = (size_t)w * 4;
    buffer.assign(stride * (size_t)h, 0);
    int size[2] = {w, h};
    mpv_render_param params[] = {
      {MPV_RENDER_PARAM_SW_SIZE, size},
      {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char *>("rgb0")},
      {MPV_RENDER_PARAM_SW_STRIDE, &stride},
      {MPV_RENDER_PARAM_SW_POINTER, buffer.data()},
      {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    if (mpv_render_context_render(render, params) < 0) return;
    for (size_t i = 3; i < buffer.size(); i += 4) buffer[i] = 0xff;
    VideoFrame frame;
    frame.width = w;
    frame.height = h;
    frame.opaque = true;
    frame.rgba = std::move(buffer);
    buffer = {};
    std::lock_guard<std::mutex> lock(s.m);
    s.frame = std::move(frame);
    ++s.serial;
  };

  while (!s.stop.load()) {
    std::deque<Cmd> commands;
    {
      std::unique_lock<std::mutex> lock(s.m);
      s.cv.wait_for(lock, std::chrono::milliseconds(100), [&] { return s.wake || s.stop.load(); });
      s.wake = false;
      commands.swap(s.commands);
    }
    if (s.stop.load()) break;

    for (const Cmd &c : commands) {
      switch (c.kind) {
      case CmdKind::Play:
        if (getFlag(mpv, "eof-reached", false)) {
          const char *args[] = {"seek", "0", "absolute", nullptr};
          mpv_command(mpv, args);
        }
        setFlag(mpv, "pause", false);
        break;
      case CmdKind::Pause:
        setFlag(mpv, "pause", true);
        break;
      case CmdKind::Seek: {
        char text[48];
        std::snprintf(text, sizeof(text), "%.3f", std::max(0.0, c.number));
        const char *args[] = {"seek", text, "absolute", nullptr};
        mpv_command(mpv, args);
        needRender = true;
        break;
      }
      case CmdKind::Mute:
        setFlag(mpv, "mute", c.number != 0.0);
        break;
      case CmdKind::Volume: {
        double v = c.number * 100.0;
        mpv_set_property(mpv, "volume", MPV_FORMAT_DOUBLE, &v);
        break;
      }
      case CmdKind::Loop:
        mpv_set_property_string(mpv, "loop-file", c.number != 0.0 ? "inf" : "no");
        break;
      case CmdKind::Resize:
        maxW = c.w;
        maxH = c.h;
        needRender = true;
        break;
      }
    }

    bool shutdown = false;
    bool failed = false;
    for (;;) {
      mpv_event *event = mpv_wait_event(mpv, 0);
      if (event->event_id == MPV_EVENT_NONE) break;
      if (event->event_id == MPV_EVENT_SHUTDOWN) shutdown = true;
      if (event->event_id == MPV_EVENT_FILE_LOADED) loaded = true;
      if (event->event_id == MPV_EVENT_VIDEO_RECONFIG) needRender = true;
      if (event->event_id == MPV_EVENT_END_FILE) {
        const auto *end = static_cast<mpv_event_end_file *>(event->data);
        if (end && end->reason == MPV_END_FILE_REASON_ERROR) failed = true;
      }
    }
    if (shutdown) break;

    if (render) {
      const uint64_t flags = mpv_render_context_update(render);
      if ((flags & MPV_RENDER_UPDATE_FRAME) || needRender) {
        needRender = false;
        renderFrame();
      }
    }

    const auto now = Clock::now();
    if (failed || now - lastPoll >= std::chrono::milliseconds(80)) {
      lastPoll = now;
      MpvStatus status;
      status.loaded = loaded;
      status.failed = failed;
      if (loaded) {
        status.position = std::max(0.0, getDouble(mpv, "time-pos", 0.0));
        status.duration = std::max(0.0, getDouble(mpv, "duration", 0.0));
        status.ended = getFlag(mpv, "eof-reached", false);
        status.playing = !getFlag(mpv, "pause", true) && !status.ended;
      }
      std::lock_guard<std::mutex> lock(s.m);
      s.status = status;
      if (failed) s.status.failed = true;
    }
  }

  if (render) mpv_render_context_free(render);
  mpv_terminate_destroy(mpv);
}

} // namespace

bool MpvPlayer::available() {
  static const bool ok = lzy_n8v_mpv_lazy_is_available() != 0;
  return ok;
}

std::unique_ptr<MpvPlayer> MpvPlayer::open(const MpvPlayerOptions &options) {
  if (!available() || options.source.empty()) return nullptr;

  const char *numeric = std::setlocale(LC_NUMERIC, nullptr);
  if (!numeric || std::strcmp(numeric, "C") != 0) std::setlocale(LC_NUMERIC, "C");

  auto state = std::make_shared<MpvPlayerImpl>();
  try {
    std::thread(runPlayer, state, options).detach();
  } catch (...) {
    return nullptr;
  }
  return std::unique_ptr<MpvPlayer>(new MpvPlayer(std::move(state)));
}

#endif // N8V_HAVE_MPV

} // namespace n8v::detail
