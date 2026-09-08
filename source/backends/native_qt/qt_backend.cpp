#include "qt_backend.hpp"

#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

const char qt_version_tag = 0;

namespace n8v::detail {
namespace {

class N8VButton final : public QPushButton {
public:
  using QPushButton::QPushButton;
  std::function<void()> *callback = nullptr;

protected:
  void mousePressEvent(QMouseEvent *event) override {
    QPushButton::mousePressEvent(event);
    if (callback && *callback) (*callback)();
  }
};

class N8VLinkLabel final : public QLabel {
public:
  using QLabel::QLabel;
  std::string url;

protected:
  void mousePressEvent(QMouseEvent *event) override {
    QLabel::mousePressEvent(event);
    n8v::detail::openUrl(url);
  }
};

class N8VCheckBox final : public QCheckBox {
public:
  using QCheckBox::QCheckBox;
  bool *checkedPtr = nullptr;
  std::function<void(bool)> onChange;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    QCheckBox::mouseReleaseEvent(event);
    if (!checkedPtr) return;
    bool newValue = isChecked();
    if (newValue != *checkedPtr) {
      *checkedPtr = newValue;
      if (onChange) onChange(newValue);
    }
  }
};

class QtBackend final : public Backend {
public:
  ~QtBackend() override { shutdown(); }

  bool initialize(int width, int height, std::string_view title) override {
    static int argc = 0;
    static char *argv[] = {nullptr};
    app_ = std::make_unique<QApplication>(argc, argv);

    window_ = new QWidget();
    window_->setWindowTitle(QString::fromUtf8(title.data(), (int)title.size()));
    window_->resize(width, height);
    window_->show();

    measureLabel_ = new QLabel(window_);
    measureLabel_->setVisible(false);

    measureButton_ = new QPushButton(window_);
    measureButton_->setVisible(false);

    measureLink_ = new QLabel(window_);
    measureLink_->setTextFormat(Qt::RichText);
    measureLink_->setVisible(false);

    measureCheckbox_ = new QCheckBox(window_);
    measureCheckbox_->setVisible(false);

    return true;
  }

  bool pumpEvents() override {
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    return window_->isVisible();
  }

  bool pointerDown() const override { return false; }

  Clay_Dimensions windowSize() const override { return {(float)window_->width(), (float)window_->height()}; }

  Clay_Dimensions measureText(std::string_view text, FontFamily, uint16_t, bool, bool) const override {
    QString qtext = QString::fromUtf8(text.data(), (int)text.size());
    QFontMetrics fm(measureLabel_->font());
    QSize size = fm.size(0, qtext);
    return {(float)size.width(), (float)size.height()};
  }

  Clay_Dimensions measureNativeChrome(NativeWidgetKind kind, std::string_view text, uint16_t) const override {
    QString qtext = QString::fromUtf8(text.data(), (int)text.size());
    if (kind == NativeWidgetKind::Button) {
      measureButton_->setText(qtext);
      QSize hint = measureButton_->sizeHint();
      return {(float)hint.width(), (float)hint.height()};
    }
    if (kind == NativeWidgetKind::Checkbox) {
      measureCheckbox_->setText(qtext);
      QSize hint = measureCheckbox_->sizeHint();
      return {(float)hint.width(), (float)hint.height()};
    }
    measureLink_->setText(linkHtml(qtext, "about:blank"));
    QSize hint = measureLink_->sizeHint();
    return {(float)hint.width(), (float)hint.height()};
  }

  void beginFrame() override {}

  void present(Clay_RenderCommandArray commands) override {
    std::map<int, int> wrapLineCounts;
    std::set<WidgetKey> seenKeys;
    QWidget *pendingLabelTarget = nullptr;
    NativeWidgetKind pendingKind = NativeWidgetKind::Button;
    std::string pendingLinkUrl;

    for (int32_t i = 0; i < commands.length; ++i) {
      Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta) {
          pendingLabelTarget = nullptr;
          continue;
        }
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        QWidget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        pendingLabelTarget = widget;
        pendingKind = meta->kind;
        pendingLinkUrl = meta->kind == NativeWidgetKind::Link && meta->url ? *meta->url : std::string();
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
        auto *flags = static_cast<TextStyleFlags *>(command->userData);
        std::string text(command->renderData.text.stringContents.chars, (size_t)command->renderData.text.stringContents.length);
        QString qtext = QString::fromUtf8(text.data(), (int)text.size());

        if (flags && flags->ownedByWidget) {
          if (pendingLabelTarget && pendingKind == NativeWidgetKind::Button) {
            static_cast<N8VButton *>(pendingLabelTarget)->setText(qtext);
          } else if (pendingLabelTarget && pendingKind == NativeWidgetKind::Checkbox) {
            static_cast<N8VCheckBox *>(pendingLabelTarget)->setText(qtext);
          } else if (pendingLabelTarget) {
            static_cast<N8VLinkLabel *>(pendingLabelTarget)->setText(linkHtml(qtext, QString::fromStdString(pendingLinkUrl)));
          }
          pendingLabelTarget = nullptr;
          continue;
        }

        int ordinal = flags ? flags->ordinal : -1;
        int wrapLineIndex = wrapLineCounts[ordinal]++;
        WidgetKey key{ordinal, wrapLineIndex};
        seenKeys.insert(key);
        QLabel *label = ensureLabel(key);
        label->setText(qtext);
        positionWidget(label, command->boundingBox);
        pendingLabelTarget = nullptr;
        continue;
      }

      pendingLabelTarget = nullptr;
    }

    for (auto it = widgets_.begin(); it != widgets_.end();) {
      if (!seenKeys.count(it->first)) {
        delete it->second;
        callbacks_.erase(it->first.ordinal);
        it = widgets_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void setCursor(CursorKind cursor) override { window_->setCursor(cursor == CursorKind::Pointer ? Qt::PointingHandCursor : Qt::ArrowCursor); }

  void shutdown() override {
    if (window_) {
      delete window_;
      window_ = nullptr;
    }
    app_.reset();
  }

private:
  struct WidgetKey {
    int ordinal;
    int subIndex; // -1 for a button/link. 0, 1, 2... per wrapped line of standalone text
    bool operator<(const WidgetKey &other) const { return ordinal != other.ordinal ? ordinal < other.ordinal : subIndex < other.subIndex; }
  };

  static QString linkHtml(const QString &text, const QString &url) { return QStringLiteral("<a href=\"%1\">%2</a>").arg(url, text.toHtmlEscaped()); }

  QWidget *ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) {
      if (meta.kind == NativeWidgetKind::Button) {
        callbacks_[meta.ordinal] = meta.onClick ? *meta.onClick : std::function<void()>{};
      } else if (meta.kind == NativeWidgetKind::Link) {
        static_cast<N8VLinkLabel *>(it->second)->url = meta.url ? *meta.url : std::string();
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        auto *checkbox = static_cast<N8VCheckBox *>(it->second);
        checkbox->checkedPtr = meta.checked;
        checkbox->onChange = meta.onChange ? *meta.onChange : std::function<void(bool)>{};
        checkbox->setChecked(*meta.checked);
      }
      return it->second;
    }

    QWidget *widget = nullptr;
    if (meta.kind == NativeWidgetKind::Button) {
      auto *button = new N8VButton(window_);
      callbacks_[meta.ordinal] = meta.onClick ? *meta.onClick : std::function<void()>{};
      button->callback = &callbacks_[meta.ordinal];
      widget = button;
    } else if (meta.kind == NativeWidgetKind::Checkbox) {
      auto *checkbox = new N8VCheckBox(window_);
      checkbox->checkedPtr = meta.checked;
      checkbox->onChange = meta.onChange ? *meta.onChange : std::function<void(bool)>{};
      checkbox->setChecked(meta.checked && *meta.checked);
      widget = checkbox;
    } else {
      auto *label = new N8VLinkLabel(window_);
      label->setTextFormat(Qt::RichText);
      label->url = meta.url ? *meta.url : std::string();
      widget = label;
    }

    widget->setVisible(true);
    widgets_[key] = widget;
    return widget;
  }

  QLabel *ensureLabel(const WidgetKey &key) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) return static_cast<QLabel *>(it->second);

    auto *label = new QLabel(window_);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setVisible(true);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(QWidget *widget, const Clay_BoundingBox &box) { widget->setGeometry((int)box.x, (int)box.y, (int)box.width, (int)box.height); }

  std::unique_ptr<QApplication> app_;
  QWidget *window_ = nullptr;
  QLabel *measureLabel_ = nullptr;
  QPushButton *measureButton_ = nullptr;
  QLabel *measureLink_ = nullptr;
  QCheckBox *measureCheckbox_ = nullptr;

  std::map<WidgetKey, QWidget *> widgets_;
  std::unordered_map<int, std::function<void()>> callbacks_;
};

} // namespace

std::unique_ptr<Backend> makeQtBackend() { return std::make_unique<QtBackend>(); }

} // namespace n8v::detail
