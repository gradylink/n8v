#include "gtk4_media_view.hpp"

#include "core/media/mpv_player.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

namespace n8v::detail {

namespace {

using Clock = std::chrono::steady_clock;

constexpr const char *kViewKey = "n8v-media-view";

struct PlayerStatus {
  bool loaded = false;
  bool playing = false;
  bool ended = false;
  bool failed = false;
  double position = 0.0;
  double duration = 0.0;
};

class Engine {
public:
  virtual ~Engine() = default;
  virtual void play() = 0;
  virtual void pause() = 0;
  virtual void seek(double seconds) = 0;
  virtual void setMuted(bool muted) = 0;
  virtual void setVolume(double volume) = 0;
  virtual void setLoop(bool loop) = 0;
  virtual void setMaxSize(int, int) {}
  virtual PlayerStatus status() = 0;
  virtual bool takeFrame(VideoFrame &) { return false; }
  virtual GdkPaintable *paintable() { return nullptr; }
};

class MpvEngine final : public Engine {
public:
  explicit MpvEngine(std::unique_ptr<MpvPlayer> player) : player_(std::move(player)) {}
  void play() override { player_->play(); }
  void pause() override { player_->pause(); }
  void seek(double seconds) override { player_->seek(seconds); }
  void setMuted(bool muted) override { player_->setMuted(muted); }
  void setVolume(double volume) override { player_->setVolume(volume); }
  void setLoop(bool loop) override { player_->setLoop(loop); }
  void setMaxSize(int w, int h) override { player_->setMaxSize(w, h); }
  PlayerStatus status() override {
    const MpvStatus s = player_->status();
    return {s.loaded, s.playing, s.ended, s.failed, s.position, s.duration};
  }
  bool takeFrame(VideoFrame &out) override { return player_->takeFrame(out); }

private:
  std::unique_ptr<MpvPlayer> player_;
};

class GstEngine final : public Engine {
public:
  explicit GstEngine(GtkMediaStream *stream) : stream_(stream) {}
  ~GstEngine() override {
    GThread *thread = g_thread_try_new(
      "n8v-media-release",
      [](gpointer data) -> gpointer {
        g_object_unref(data);
        return nullptr;
      },
      stream_,
      nullptr
    );
    if (thread) g_thread_unref(thread);
    else g_object_unref(stream_);
  }
  void play() override {
    if (gtk_media_stream_get_ended(stream_)) gtk_media_stream_seek(stream_, 0);
    gtk_media_stream_play(stream_);
  }
  void pause() override { gtk_media_stream_pause(stream_); }
  void seek(double seconds) override { gtk_media_stream_seek(stream_, (gint64)(std::max(0.0, seconds) * 1e6)); }
  void setMuted(bool muted) override { gtk_media_stream_set_muted(stream_, muted); }
  void setVolume(double volume) override { gtk_media_stream_set_volume(stream_, volume); }
  void setLoop(bool loop) override { gtk_media_stream_set_loop(stream_, loop); }
  PlayerStatus status() override {
    PlayerStatus s;
    s.failed = gtk_media_stream_get_error(stream_) != nullptr;
    s.loaded = gtk_media_stream_is_prepared(stream_);
    s.ended = gtk_media_stream_get_ended(stream_);
    s.playing = gtk_media_stream_get_playing(stream_) && !s.ended;
    s.position = (double)gtk_media_stream_get_timestamp(stream_) / 1e6;
    s.duration = (double)gtk_media_stream_get_duration(stream_) / 1e6;
    return s;
  }
  GdkPaintable *paintable() override { return GDK_PAINTABLE(stream_); }

private:
  GtkMediaStream *stream_;
};

void ensureStyles(GtkWidget *widget) {
  static bool installed = false;
  if (installed) return;
  installed = true;
  static const char *css = R"css(
.n8v-media-surface { background: black; }
.n8v-media-bar { padding: 4px 8px; }
.n8v-media-bar.overlay {
  background-image: linear-gradient(to top, rgba(0,0,0,0.78), rgba(0,0,0,0));
  padding: 22px 10px 8px 10px;
}
.n8v-media-bar.overlay button, .n8v-media-bar.overlay label { color: white; }
.n8v-media-bar.overlay label { text-shadow: 0 1px 2px rgba(0,0,0,0.6); }
.n8v-media-bar button {
  min-width: 30px; min-height: 30px; padding: 0; border-radius: 999px;
  background: none; box-shadow: none; border: none; text-shadow: none; -gtk-icon-shadow: none;
}
.n8v-media-bar.overlay button:hover { background: rgba(255,255,255,0.20); }
.n8v-media-bar:not(.overlay) button:hover { background: alpha(currentColor, 0.12); }
.n8v-media-bar button:disabled { opacity: 0.4; }
.n8v-media-bar label.time { font-size: 12px; font-feature-settings: "tnum"; min-width: 36px; }
.n8v-media-bar scale { min-height: 18px; padding: 0; }
.n8v-media-bar scale trough { min-height: 4px; border-radius: 2px; background: alpha(currentColor, 0.25); }
.n8v-media-bar scale highlight { min-height: 4px; border-radius: 2px; background: currentColor; }
.n8v-media-bar.overlay scale trough { background: rgba(255,255,255,0.32); }
.n8v-media-bar.overlay scale highlight { background: white; }
.n8v-media-bar scale slider {
  min-width: 12px; min-height: 12px; margin: -4px; border-radius: 999px; padding: 0;
  background: currentColor; border: none; box-shadow: none; outline: none; opacity: 0;
}
.n8v-media-bar.overlay scale slider { background: white; }
.n8v-media-bar scale:hover slider, .n8v-media-bar scale:active slider { opacity: 1; }
.n8v-media-bar:not(.overlay) { background: alpha(currentColor, 0.07); border-radius: 10px; }
.n8v-media-bar scale.volume { min-width: 64px; }
)css";
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(gtk_widget_get_display(widget), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

void setIcon(GtkWidget *button, const char *iconName, const char *fallbackText) {
  GtkIconTheme *theme = gtk_icon_theme_get_for_display(gtk_widget_get_display(button));
  if (gtk_icon_theme_has_icon(theme, iconName)) {
    gtk_button_set_icon_name(GTK_BUTTON(button), iconName);
  } else {
    gtk_button_set_label(GTK_BUTTON(button), fallbackText);
  }
}

void formatTime(double seconds, char *out, size_t size) {
  const long total = seconds > 0 ? (long)seconds : 0;
  if (total >= 3600) std::snprintf(out, size, "%ld:%02ld:%02ld", total / 3600, (total / 60) % 60, total % 60);
  else std::snprintf(out, size, "%ld:%02ld", total / 60, total % 60);
}

} // namespace

struct MediaView::Impl {
  bool video;
  std::function<void(bool)> requestFullscreen;

  GtkWidget *root = nullptr;
  GtkWidget *picture = nullptr;
  GtkWidget *revealer = nullptr;
  GtkWidget *bar = nullptr;
  GtkWidget *playButton = nullptr, *muteButton = nullptr, *fullscreenButton = nullptr;
  GtkWidget *position = nullptr, *durationLabel = nullptr;
  GtkWidget *seek = nullptr, *volume = nullptr;

  std::unique_ptr<Engine> engine;
  MediaViewOptions applied;
  bool haveApplied = false;
  bool muted = false;
  bool isFullscreen = false;
  double volumeLevel = 1.0;

  bool shownPlaying = false, shownMuted = false, shownFullscreen = false, shownFailed = false, shownLoaded = false;
  long shownPosition = -1, shownDuration = -1;
  double seekMax = 0.0;
  Clock::time_point holdSeekUntil{};
  int sentW = -1, sentH = -1;
  guint hideTimer = 0;
  bool pointerInside = false;
  bool destroying = false;

  Impl(bool v, std::function<void(bool)> fs) : video(v), requestFullscreen(std::move(fs)) {}
  ~Impl() {
    destroying = true;
    if (hideTimer) g_source_remove(hideTimer);
  }

  static Impl *from(gpointer data) { return static_cast<Impl *>(data); }

  void togglePlay() {
    if (!engine) return;
    const PlayerStatus s = engine->status();
    if (s.playing) engine->pause();
    else engine->play();
  }
  void setMuted(bool value) {
    muted = value;
    if (engine) engine->setMuted(muted);
  }

  void updateBarVisibility() {
    if (!revealer) return;
    const bool want = !shownPlaying || pointerInside;
    if (gtk_revealer_get_reveal_child(GTK_REVEALER(revealer)) != want) gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), want);
  }
  static gboolean onHideTimer(gpointer data) {
    Impl *self = from(data);
    self->hideTimer = 0;
    self->pointerInside = false;
    self->updateBarVisibility();
    return G_SOURCE_REMOVE;
  }
  void poke() {
    pointerInside = true;
    updateBarVisibility();
    if (hideTimer) g_source_remove(hideTimer);
    hideTimer = g_timeout_add(2200, &Impl::onHideTimer, this);
  }

  static void onMotion(GtkEventControllerMotion *, gdouble, gdouble, gpointer data) { from(data)->poke(); }
  static void onLeave(GtkEventControllerMotion *, gpointer data) {
    Impl *self = from(data);
    if (self->hideTimer) g_source_remove(self->hideTimer);
    self->hideTimer = g_timeout_add(600, &Impl::onHideTimer, self);
  }
  static void onPictureClick(GtkGestureClick *, int, double, double, gpointer data) { from(data)->togglePlay(); }
  static void onPlayClicked(GtkButton *, gpointer data) { from(data)->togglePlay(); }
  static void onMuteClicked(GtkButton *, gpointer data) { from(data)->setMuted(!from(data)->muted); }
  static void onFullscreenClicked(GtkButton *, gpointer data) {
    Impl *self = from(data);
    self->isFullscreen = !self->isFullscreen;
    if (self->requestFullscreen) self->requestFullscreen(self->isFullscreen);
  }
  static gboolean onSeekChange(GtkRange *, GtkScrollType, gdouble value, gpointer data) {
    Impl *self = from(data);
    if (self->engine) self->engine->seek(value);
    self->holdSeekUntil = Clock::now() + std::chrono::milliseconds(600);
    return FALSE;
  }
  static void onVolumeChanged(GtkRange *range, gpointer data) {
    Impl *self = from(data);
    self->volumeLevel = gtk_range_get_value(range);
    if (self->engine) self->engine->setVolume(self->volumeLevel);
  }

  static gboolean onTick(GtkWidget *widget, GdkFrameClock *, gpointer) {
    auto *view = MediaView::of(widget);
    if (!view) return G_SOURCE_REMOVE;
    view->impl_->tick();
    return G_SOURCE_CONTINUE;
  }

  void tick() {
    if (!engine) return;

    if (picture) {
      VideoFrame frame;
      if (engine->takeFrame(frame)) {
        const gsize size = (gsize)frame.rgba.size();
        auto *pixels = new std::vector<uint8_t>(std::move(frame.rgba));
        GBytes *bytes = g_bytes_new_with_free_func(pixels->data(), size, +[](gpointer d) { delete static_cast<std::vector<uint8_t> *>(d); }, pixels);
        GdkTexture *texture = gdk_memory_texture_new(frame.width, frame.height, GDK_MEMORY_R8G8B8A8, bytes, (gsize)frame.width * 4);
        g_bytes_unref(bytes);
        gtk_picture_set_paintable(GTK_PICTURE(picture), GDK_PAINTABLE(texture));
        g_object_unref(texture);
      }
      const int scale = std::max(1, gtk_widget_get_scale_factor(picture));
      const int w = gtk_widget_get_width(picture) * scale, h = gtk_widget_get_height(picture) * scale;
      if (w > 0 && h > 0 && (std::abs(w - sentW) > 8 || std::abs(h - sentH) > 8)) {
        sentW = w;
        sentH = h;
        engine->setMaxSize(w, h);
      }
    }

    const PlayerStatus s = engine->status();

    if (s.playing != shownPlaying) {
      shownPlaying = s.playing;
      setIcon(playButton, shownPlaying ? "media-playback-pause-symbolic" : "media-playback-start-symbolic", shownPlaying ? "\u275A\u275A" : "\u25B6");
      gtk_widget_set_tooltip_text(playButton, shownPlaying ? "Pause" : "Play");
      updateBarVisibility();
    }
    if (s.failed != shownFailed || s.loaded != shownLoaded) {
      shownFailed = s.failed;
      shownLoaded = s.loaded;
      gtk_widget_set_sensitive(seek, s.loaded && !s.failed);
      gtk_widget_set_sensitive(playButton, !s.failed);
      if (s.failed) gtk_label_set_text(GTK_LABEL(position), "Can\u2019t play");
      updateBarVisibility();
    }
    if (muted != shownMuted) {
      shownMuted = muted;
      setIcon(muteButton, muted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic", muted ? "\u00D7" : "\u266A");
      gtk_widget_set_tooltip_text(muteButton, muted ? "Unmute" : "Mute");
    }
    if (isFullscreen != shownFullscreen) {
      shownFullscreen = isFullscreen;
      setIcon(fullscreenButton, isFullscreen ? "view-restore-symbolic" : "view-fullscreen-symbolic", isFullscreen ? "\u2921" : "\u26F6");
      gtk_widget_set_tooltip_text(fullscreenButton, isFullscreen ? "Leave fullscreen" : "Fullscreen");
    }
    if (s.failed) return;

    char text[24];
    const long position_s = (long)std::max(0.0, s.position);
    if (position_s != shownPosition) {
      shownPosition = position_s;
      formatTime(s.position, text, sizeof(text));
      gtk_label_set_text(GTK_LABEL(position), text);
    }
    const long duration_s = (long)std::max(0.0, s.duration);
    if (duration_s != shownDuration) {
      shownDuration = duration_s;
      formatTime(s.duration, text, sizeof(text));
      gtk_label_set_text(GTK_LABEL(durationLabel), duration_s > 0 ? text : "");
    }
    if (s.duration > 0 && std::fabs(s.duration - seekMax) > 0.01) {
      seekMax = s.duration;
      gtk_range_set_range(GTK_RANGE(seek), 0.0, seekMax);
    }
    if (Clock::now() >= holdSeekUntil && s.duration > 0) {
      if (std::fabs(gtk_range_get_value(GTK_RANGE(seek)) - s.position) > 0.05) gtk_range_set_value(GTK_RANGE(seek), s.position);
    }
  }

  void build() {
    GtkWidget *tmp = gtk_button_new();
    ensureStyles(tmp);
    g_object_ref_sink(tmp);
    g_object_unref(tmp);

    bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(bar, "n8v-media-bar");
    if (video) gtk_widget_add_css_class(bar, "overlay");

    auto iconButton = [&](const char *icon, const char *fallback, GCallback callback) {
      GtkWidget *button = gtk_button_new();
      setIcon(button, icon, fallback);
      gtk_widget_add_css_class(button, "flat");
      gtk_widget_add_css_class(button, "circular");
      gtk_widget_set_focus_on_click(button, FALSE);
      g_signal_connect_data(button, "clicked", callback, this, nullptr, (GConnectFlags)0);
      return button;
    };
    auto timeLabel = [&]() {
      GtkWidget *label = gtk_label_new("0:00");
      gtk_widget_add_css_class(label, "time");
      return label;
    };

    playButton = iconButton("media-playback-start-symbolic", "\u25B6", G_CALLBACK(&Impl::onPlayClicked));
    gtk_widget_set_tooltip_text(playButton, "Play");
    position = timeLabel();
    seek = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(seek), FALSE);
    gtk_widget_set_hexpand(seek, TRUE);
    gtk_widget_set_size_request(seek, 80, -1);
    gtk_widget_set_valign(seek, GTK_ALIGN_CENTER);
    gtk_widget_set_sensitive(seek, FALSE);
    g_signal_connect_data(seek, "change-value", G_CALLBACK(&Impl::onSeekChange), this, nullptr, (GConnectFlags)0);
    durationLabel = timeLabel();
    gtk_label_set_text(GTK_LABEL(durationLabel), "");
    muteButton = iconButton("audio-volume-high-symbolic", "\u266A", G_CALLBACK(&Impl::onMuteClicked));
    gtk_widget_set_tooltip_text(muteButton, "Mute");
    volume = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.05);
    gtk_scale_set_draw_value(GTK_SCALE(volume), FALSE);
    gtk_widget_add_css_class(volume, "volume");
    gtk_widget_set_size_request(volume, 72, -1);
    gtk_widget_set_hexpand(volume, FALSE);
    gtk_widget_set_valign(volume, GTK_ALIGN_CENTER);
    gtk_range_set_value(GTK_RANGE(volume), 1.0);
    g_signal_connect_data(volume, "value-changed", G_CALLBACK(&Impl::onVolumeChanged), this, nullptr, (GConnectFlags)0);
    fullscreenButton = iconButton("view-fullscreen-symbolic", "\u26F6", G_CALLBACK(&Impl::onFullscreenClicked));
    gtk_widget_set_tooltip_text(fullscreenButton, "Fullscreen");
    gtk_widget_set_visible(fullscreenButton, FALSE);

    for (GtkWidget *child : {playButton, position, seek, durationLabel, muteButton, volume, fullscreenButton}) gtk_box_append(GTK_BOX(bar), child);

    if (video) {
      root = gtk_overlay_new();
      gtk_widget_set_overflow(root, GTK_OVERFLOW_HIDDEN);
      gtk_widget_add_css_class(root, "n8v-media-surface");
      picture = gtk_picture_new();
      gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
      gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
      gtk_widget_set_hexpand(picture, TRUE);
      gtk_widget_set_vexpand(picture, TRUE);
      gtk_overlay_set_child(GTK_OVERLAY(root), picture);

      revealer = gtk_revealer_new();
      gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_CROSSFADE);
      gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 180);
      gtk_revealer_set_reveal_child(GTK_REVEALER(revealer), TRUE);
      gtk_widget_set_valign(revealer, GTK_ALIGN_END);
      gtk_widget_set_halign(revealer, GTK_ALIGN_FILL);
      gtk_revealer_set_child(GTK_REVEALER(revealer), bar);
      gtk_overlay_add_overlay(GTK_OVERLAY(root), revealer);

      GtkEventController *motion = gtk_event_controller_motion_new();
      g_signal_connect_data(motion, "motion", G_CALLBACK(&Impl::onMotion), this, nullptr, (GConnectFlags)0);
      g_signal_connect_data(motion, "leave", G_CALLBACK(&Impl::onLeave), this, nullptr, (GConnectFlags)0);
      gtk_widget_add_controller(root, motion);

      GtkGesture *click = gtk_gesture_click_new();
      gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), 1);
      g_signal_connect_data(click, "released", G_CALLBACK(&Impl::onPictureClick), this, nullptr, (GConnectFlags)0);
      gtk_widget_add_controller(picture, GTK_EVENT_CONTROLLER(click));
    } else {
      root = bar;
    }
  }
};

MediaView::MediaView(bool video, std::function<void(bool)> requestFullscreen) : impl_(std::make_unique<Impl>(video, std::move(requestFullscreen))) {
  impl_->build();
  g_object_set_data_full(G_OBJECT(impl_->root), kViewKey, this, +[](gpointer data) { delete static_cast<MediaView *>(data); });
  gtk_widget_add_tick_callback(impl_->root, &Impl::onTick, nullptr, nullptr);
}

MediaView::~MediaView() = default;

GtkWidget *MediaView::widget() const { return impl_->root; }

MediaView *MediaView::of(GtkWidget *widget) { return widget ? static_cast<MediaView *>(g_object_get_data(G_OBJECT(widget), kViewKey)) : nullptr; }

void MediaView::update(const MediaViewOptions &o) {
  Impl &d = *impl_;

  gtk_widget_set_visible(d.fullscreenButton, o.fullscreenButton);
  if (!d.haveApplied || o.isFullscreen != d.applied.isFullscreen) d.isFullscreen = o.isFullscreen;

  const bool sourceChanged = !d.haveApplied || o.source != d.applied.source;
  if (sourceChanged) {
    d.engine.reset();
    d.shownPlaying = d.shownFailed = d.shownLoaded = false;
    d.shownPosition = d.shownDuration = -1;
    d.seekMax = 0.0;
    d.sentW = d.sentH = -1;
    gtk_range_set_value(GTK_RANGE(d.seek), 0.0);
    gtk_label_set_text(GTK_LABEL(d.position), "0:00");
    gtk_label_set_text(GTK_LABEL(d.durationLabel), "");
    gtk_widget_set_sensitive(d.seek, FALSE);
    gtk_widget_set_sensitive(d.playButton, TRUE);
    if (d.picture) gtk_picture_set_paintable(GTK_PICTURE(d.picture), nullptr);

    if (!o.source.empty()) {
      if (MpvPlayer::available()) {
        MpvPlayerOptions mo;
        mo.source = o.source;
        mo.video = d.video;
        mo.loop = o.loop;
        mo.autoplay = o.autoplay;
        mo.muted = o.muted;
        if (auto player = MpvPlayer::open(mo)) d.engine = std::make_unique<MpvEngine>(std::move(player));
      }
      if (!d.engine) {
        GFile *file = o.source.find("://") != std::string::npos ? g_file_new_for_uri(o.source.c_str()) : g_file_new_for_path(o.source.c_str());
        GtkMediaStream *stream = gtk_media_file_new_for_file(file);
        g_object_unref(file);
        gtk_media_stream_set_loop(stream, o.loop);
        gtk_media_stream_set_muted(stream, o.muted);
        if (o.autoplay) gtk_media_stream_play(stream);
        d.engine = std::make_unique<GstEngine>(stream);
        if (d.picture) gtk_picture_set_paintable(GTK_PICTURE(d.picture), d.engine->paintable());
      }
      d.muted = o.muted;
      d.engine->setVolume(d.volumeLevel);
    }
  } else if (d.engine) {
    if (o.loop != d.applied.loop) d.engine->setLoop(o.loop);
    if (o.muted != d.applied.muted) d.setMuted(o.muted);
  }

  d.applied = o;
  d.haveApplied = true;
}

} // namespace n8v::detail
