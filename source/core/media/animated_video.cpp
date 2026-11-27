#include "core/media/animated_video.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef N8V_HAVE_FFMPEG
#include "n8v_ffmpeg_names.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/rational.h>
#include <libswscale/swscale.h>

int lzy_n8v_avutil_lazy_is_available(void);
int lzy_n8v_avcodec_lazy_is_available(void);
int lzy_n8v_avformat_lazy_is_available(void);
int lzy_n8v_swscale_lazy_is_available(void);
}
#endif

namespace n8v::detail {

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kMaxLiveAnimations = 12;
std::atomic<int> g_liveAnimations{0};

} // namespace

struct AnimatedVideoState {
  std::mutex m;
  std::condition_variable cv;
  std::atomic<bool> stop{false};
  bool paused = false;
  bool failed = false;
  bool running = true;
  VideoFrame frame;
  uint64_t serial = 0;
  uint64_t taken = 0;
};

AnimatedVideo::AnimatedVideo(std::shared_ptr<AnimatedVideoState> state) : state_(std::move(state)) {}

AnimatedVideo::~AnimatedVideo() {
  if (!state_) return;
  {
    std::lock_guard<std::mutex> lock(state_->m);
    state_->stop.store(true);
  }
  state_->cv.notify_all();
}

bool AnimatedVideo::takeFrame(VideoFrame &out) {
  std::lock_guard<std::mutex> lock(state_->m);
  if (state_->serial == state_->taken) return false;
  out = std::move(state_->frame);
  state_->frame = VideoFrame{};
  state_->taken = state_->serial;
  return true;
}

void AnimatedVideo::setPaused(bool paused) {
  {
    std::lock_guard<std::mutex> lock(state_->m);
    if (state_->paused == paused) return;
    state_->paused = paused;
  }
  state_->cv.notify_all();
}

bool AnimatedVideo::failed() const {
  std::lock_guard<std::mutex> lock(state_->m);
  return state_->failed;
}

bool AnimatedVideo::running() const {
  std::lock_guard<std::mutex> lock(state_->m);
  return state_->running;
}

#ifndef N8V_HAVE_FFMPEG

bool AnimatedVideo::available() { return false; }

std::unique_ptr<AnimatedVideo> AnimatedVideo::open(const AnimatedVideoOptions &) { return nullptr; }

#else

namespace {

struct Timing {
  bool valid = false;
  Clock::time_point wall0;
  double pts0 = 0.0;
};

int interruptCallback(void *opaque) { return static_cast<AnimatedVideoState *>(opaque)->stop.load() ? 1 : 0; }

enum class Step { Frame, Eof, Error };

class Source {
public:
  explicit Source(AnimatedVideoState &state) : state_(state) {}
  ~Source() { close(); }
  Source(const Source &) = delete;
  Source &operator=(const Source &) = delete;

  bool open(const std::string &url) {
    close();
    url_ = url;

    fmt_ = avformat_alloc_context();
    if (!fmt_) return false;
    fmt_->interrupt_callback.callback = interruptCallback;
    fmt_->interrupt_callback.opaque = &state_;

    AVDictionary *options = nullptr;
    av_dict_set(&options, "rw_timeout", "15000000", 0);
    const int opened = avformat_open_input(&fmt_, url.c_str(), nullptr, &options);
    av_dict_free(&options);
    if (opened < 0) return false;

    if (avformat_find_stream_info(fmt_, nullptr) < 0) return false;
    stream_ = av_find_best_stream(fmt_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (stream_ < 0) return false;
    for (unsigned i = 0; i < fmt_->nb_streams; ++i) {
      if ((int)i != stream_) fmt_->streams[i]->discard = AVDISCARD_ALL;
    }

    AVStream *st = fmt_->streams[stream_];
    const AVCodec *decoder = avcodec_find_decoder(st->codecpar->codec_id);
    if (!decoder) return false;
    codec_ = avcodec_alloc_context3(decoder);
    if (!codec_) return false;
    if (avcodec_parameters_to_context(codec_, st->codecpar) < 0) return false;
    codec_->thread_count = 1;
    if (avcodec_open2(codec_, decoder, nullptr) < 0) return false;

    pkt_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (!pkt_ || !frame_) return false;

    timeBase_ = st->time_base;
    if (timeBase_.num <= 0 || timeBase_.den <= 0) timeBase_ = AVRational{1, 1000};
    fallbackDuration_ = 0.1;
    if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) {
      fallbackDuration_ = std::clamp(1.0 / av_q2d(st->avg_frame_rate), 0.01, 1.0);
    }
    draining_ = false;
    synthPts_ = 0.0;
    return true;
  }

  void close() {
    if (sws_) sws_freeContext(sws_);
    sws_ = nullptr;
    if (frame_) av_frame_free(&frame_);
    if (pkt_) av_packet_free(&pkt_);
    if (codec_) avcodec_free_context(&codec_);
    if (fmt_) avformat_close_input(&fmt_);
    stream_ = -1;
  }

  bool reopen() { return open(url_); }

  bool rewind() {
    if (av_seek_frame(fmt_, -1, 0, AVSEEK_FLAG_BACKWARD) < 0) return false;
    avcodec_flush_buffers(codec_);
    draining_ = false;
    synthPts_ = 0.0;
    return true;
  }

  Step next() {
    for (;;) {
      const int received = avcodec_receive_frame(codec_, frame_);
      if (received == 0) return Step::Frame;
      if (received == AVERROR_EOF) return Step::Eof;
      if (received != AVERROR(EAGAIN)) return Step::Error;
      if (draining_) return Step::Eof;

      const int read = av_read_frame(fmt_, pkt_);
      if (read < 0) {
        if (read != AVERROR_EOF) return Step::Error;
        avcodec_send_packet(codec_, nullptr);
        draining_ = true;
        continue;
      }
      if (pkt_->stream_index == stream_) avcodec_send_packet(codec_, pkt_);
      av_packet_unref(pkt_);
    }
  }

  void timing(double &pts, double &duration) {
    int64_t ts = frame_->best_effort_timestamp;
    if (ts == AV_NOPTS_VALUE) {
      pts = synthPts_;
    } else {
      pts = (double)ts * av_q2d(timeBase_);
    }
#if LIBAVUTIL_VERSION_MAJOR >= 58
    const int64_t d = frame_->duration;
#else
    const int64_t d = frame_->pkt_duration;
#endif
    duration = d > 0 ? (double)d * av_q2d(timeBase_) : 0.0;
    synthPts_ = pts + (duration > 0 ? duration : fallbackDuration_);
  }

  double fallbackDuration() const { return fallbackDuration_; }

  bool convert(VideoFrame &out, int maxWidth, int maxHeight) {
    const int w = frame_->width, h = frame_->height;
    if (w <= 0 || h <= 0) return false;
    double scale = 1.0;
    if (maxWidth > 0) scale = std::min(scale, (double)maxWidth / w);
    if (maxHeight > 0) scale = std::min(scale, (double)maxHeight / h);
    const int dw = std::max(1, (int)std::lround(w * scale));
    const int dh = std::max(1, (int)std::lround(h * scale));

    sws_ = sws_getCachedContext(sws_, w, h, (AVPixelFormat)frame_->format, dw, dh, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_) return false;

    out.width = dw;
    out.height = dh;
    out.rgba.resize((size_t)dw * (size_t)dh * 4);
    uint8_t *dst[4] = {out.rgba.data(), nullptr, nullptr, nullptr};
    int dstStride[4] = {dw * 4, 0, 0, 0};
    sws_scale(sws_, frame_->data, frame_->linesize, 0, h, dst, dstStride);
    return true;
  }

private:
  AnimatedVideoState &state_;
  std::string url_;
  AVFormatContext *fmt_ = nullptr;
  AVCodecContext *codec_ = nullptr;
  AVPacket *pkt_ = nullptr;
  AVFrame *frame_ = nullptr;
  SwsContext *sws_ = nullptr;
  int stream_ = -1;
  AVRational timeBase_{1, 1000};
  double fallbackDuration_ = 0.1;
  double synthPts_ = 0.0;
  bool draining_ = false;
};

bool paceTo(AnimatedVideoState &s, Timing &tm, double ptsSeconds) {
  for (;;) {
    {
      std::unique_lock<std::mutex> lock(s.m);
      if (s.paused && !s.stop.load()) {
        tm.valid = false;
        s.cv.wait(lock, [&] { return s.stop.load() || !s.paused; });
      }
      if (s.stop.load()) return false;
    }

    const auto now = Clock::now();
    if (!tm.valid) {
      tm.wall0 = now;
      tm.pts0 = ptsSeconds;
      tm.valid = true;
    }
    auto due = tm.wall0 + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(ptsSeconds - tm.pts0));
    if (now > due + std::chrono::milliseconds(250)) {
      tm.wall0 = now;
      tm.pts0 = ptsSeconds;
      return true;
    }
    if (now >= due) return true;

    std::unique_lock<std::mutex> lock(s.m);
    s.cv.wait_until(lock, due, [&] { return s.stop.load() || s.paused; });
  }
}

void publish(AnimatedVideoState &s, VideoFrame &&frame) {
  std::lock_guard<std::mutex> lock(s.m);
  s.frame = std::move(frame);
  ++s.serial;
}

void finish(AnimatedVideoState &s, bool failed) {
  std::lock_guard<std::mutex> lock(s.m);
  s.running = false;
  if (failed) s.failed = true;
}

void decodeThread(std::shared_ptr<AnimatedVideoState> state, AnimatedVideoOptions options, bool counted) {
  AnimatedVideoState &s = *state;
  {
    Source source(s);
    if (!source.open(options.source)) {
      finish(s, true);
    } else {
      Timing timing;
      double lastEnd = 0.0;
      int framesInPass = 0;
      bool shownAny = false;
      bool failed = false;

      for (bool done = false; !done;) {
        switch (source.next()) {
        case Step::Frame: {
          double pts = 0.0, duration = 0.0;
          source.timing(pts, duration);
          if (!paceTo(s, timing, pts)) {
            done = true;
            break;
          }
          VideoFrame converted;
          if (source.convert(converted, options.maxWidth, options.maxHeight)) {
            publish(s, std::move(converted));
            shownAny = true;
          }
          ++framesInPass;
          lastEnd = pts + (duration > 0 ? duration : source.fallbackDuration());
          if (!options.play) done = true;
          break;
        }
        case Step::Error:
          failed = !shownAny;
          done = true;
          break;
        case Step::Eof: {
          if (!options.loop || framesInPass <= 1) {
            done = true;
            break;
          }
          if (!paceTo(s, timing, lastEnd)) {
            done = true;
            break;
          }
          if (!source.rewind() && !source.reopen()) {
            failed = !shownAny;
            done = true;
            break;
          }
          framesInPass = 0;
          timing.valid = false;
          break;
        }
        }
      }
      finish(s, failed);
    }
  }
  if (counted) g_liveAnimations.fetch_sub(1);
}

bool ffmpegAvailableNow() {
  return lzy_n8v_avutil_lazy_is_available() && lzy_n8v_avcodec_lazy_is_available() && lzy_n8v_avformat_lazy_is_available() && lzy_n8v_swscale_lazy_is_available();
}

} // namespace

bool AnimatedVideo::available() {
  static const bool ok = ffmpegAvailableNow();
  return ok;
}

std::unique_ptr<AnimatedVideo> AnimatedVideo::open(const AnimatedVideoOptions &requested) {
  if (!available() || requested.source.empty()) return nullptr;

  static std::once_flag quiet;
  std::call_once(quiet, [] { av_log_set_level(AV_LOG_ERROR); });

  AnimatedVideoOptions options = requested;
  bool counted = false;
  if (options.play) {
    if (g_liveAnimations.fetch_add(1) < kMaxLiveAnimations) {
      counted = true;
    } else {
      g_liveAnimations.fetch_sub(1);
      options.play = false;
    }
  }

  auto state = std::make_shared<AnimatedVideoState>();
  try {
    std::thread(decodeThread, state, std::move(options), counted).detach();
  } catch (...) {
    if (counted) g_liveAnimations.fetch_sub(1);
    return nullptr;
  }
  return std::unique_ptr<AnimatedVideo>(new AnimatedVideo(std::move(state)));
}

#endif // N8V_HAVE_FFMPEG

} // namespace n8v::detail
