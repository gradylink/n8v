#include "qt_backend.hpp"

#include "core/image_loader.hpp"
#include "core/native_widget_meta.hpp"
#include "core/open_url.hpp"
#include "core/text_style_flags.hpp"
#include "core/ui_core_internal.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

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

class N8VRadioButton final : public QRadioButton {
public:
  using QRadioButton::QRadioButton;
  int *selectedPtr = nullptr;
  int value = 0;
  std::function<void(int)> onChange;

protected:
  void mouseReleaseEvent(QMouseEvent *event) override {
    QRadioButton::mouseReleaseEvent(event);
    if (!selectedPtr || !isChecked() || *selectedPtr == value) return;
    *selectedPtr = value;
    if (onChange) onChange(value);
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

    measureEntry_ = new QLineEdit(window_);
    measureEntry_->setVisible(false);

    measureRadio_ = new QRadioButton(window_);
    measureRadio_->setVisible(false);

    measureCombo_ = new QComboBox(window_);
    measureCombo_->setVisible(false);

    measureSlider_ = new QSlider(Qt::Horizontal, window_);
    measureSlider_->setVisible(false);

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
    if (kind == NativeWidgetKind::Entry) {
      QSize hint = measureEntry_->sizeHint();
      return {0, (float)hint.height()};
    }
    if (kind == NativeWidgetKind::Radio) {
      measureRadio_->setText(qtext);
      QSize hint = measureRadio_->sizeHint();
      return {(float)hint.width(), (float)hint.height()};
    }
    if (kind == NativeWidgetKind::Dropdown) {
      QSize hint = measureCombo_->sizeHint();
      return {0, (float)hint.height()};
    }
    if (kind == NativeWidgetKind::Slider) {
      QSize hint = measureSlider_->sizeHint();
      return {0, (float)hint.height()};
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
    containerStack_.clear();
    touchedScrollContainers_.clear();

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

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
        auto *meta = static_cast<NativeWidgetMeta *>(command->userData);
        if (!meta) continue;
        WidgetKey key{meta->ordinal, -1};
        seenKeys.insert(key);
        QWidget *widget = ensureWidget(key, *meta);
        positionWidget(widget, command->boundingBox);
        ensureImagePixmap(static_cast<QLabel *>(widget), *meta, (int)command->boundingBox.width, (int)command->boundingBox.height, command->renderData.image.cornerRadius);
        pendingLabelTarget = nullptr;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
        ensureScrollContainer(command->id, command->boundingBox, command->renderData.clip);
        pendingLabelTarget = nullptr;
        continue;
      }

      if (command->commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
        closeScrollContainer();
        pendingLabelTarget = nullptr;
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
          } else if (pendingLabelTarget && pendingKind == NativeWidgetKind::Radio) {
            static_cast<N8VRadioButton *>(pendingLabelTarget)->setText(qtext);
          } else if (pendingLabelTarget && pendingKind != NativeWidgetKind::Entry && pendingKind != NativeWidgetKind::Dropdown) {
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
        entryStates_.erase(it->first.ordinal);
        dropdownStates_.erase(it->first.ordinal);
        sliderStates_.erase(it->first.ordinal);
        imagePixmapSources_.erase(static_cast<QLabel *>(it->second));
        it = widgets_.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = scrollContainers_.begin(); it != scrollContainers_.end();) {
      if (!touchedScrollContainers_.count(it->first)) {
        delete it->second.scrollArea;
        it = scrollContainers_.erase(it);
      } else {
        ++it;
      }
    }
  }

  void setCursor(CursorKind cursor) override {
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (cursor == CursorKind::Pointer) shape = Qt::PointingHandCursor;
    else if (cursor == CursorKind::Text) shape = Qt::IBeamCursor;
    window_->setCursor(shape);
  }

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

  struct DropdownState {
    int *selected = nullptr;
    std::function<void(int)> onChange;
    int lastSynced = -1;
  };

  void syncDropdown(QComboBox *combo, const NativeWidgetMeta &meta, DropdownState &state) {
    if (!meta.dropdownSelected) return;
    state.selected = meta.dropdownSelected;
    state.onChange = toStdFunction(meta.onDropdownChange, meta.onDropdownChangeUserdata);

    int widgetIndex = combo->currentIndex();
    if (widgetIndex != state.lastSynced) {
      *state.selected = widgetIndex;
      state.lastSynced = widgetIndex;
      if (state.onChange) state.onChange(widgetIndex);
    } else if (*state.selected != state.lastSynced) {
      combo->setCurrentIndex(*state.selected);
      state.lastSynced = *state.selected;
    }
  }

  static constexpr int sliderSteps = 10000;

  struct SliderState {
    float *value = nullptr;
    std::function<void(float)> onChange;
    float min = 0.0f;
    float max = 1.0f;
    int lastSynced = -1;
  };

  static int sliderPositionFor(float value, float min, float max) {
    float range = max - min;
    if (range <= 0.0f) return 0;
    float clamped = value < min ? min : value > max ? max : value;
    return (int)((clamped - min) / range * sliderSteps + 0.5f);
  }

  void syncSlider(QSlider *slider, const NativeWidgetMeta &meta, SliderState &state) {
    if (!meta.sliderValue) return;
    state.value = meta.sliderValue;
    state.onChange = toStdFunction(meta.onSliderChange, meta.onSliderChangeUserdata);
    state.min = meta.sliderMin;
    state.max = meta.sliderMax;

    int widgetPos = slider->value();
    if (widgetPos != state.lastSynced) {
      float range = state.max - state.min;
      float newValue = range > 0.0f ? state.min + (widgetPos / (float)sliderSteps) * range : state.min;
      *state.value = newValue;
      state.lastSynced = widgetPos;
      if (state.onChange) state.onChange(newValue);
    } else {
      int wantPos = sliderPositionFor(*state.value, state.min, state.max);
      if (wantPos != state.lastSynced) {
        slider->setValue(wantPos);
        state.lastSynced = wantPos;
      }
    }
  }

  struct EntryState {
    std::string *value = nullptr;
    std::function<void(std::string_view)> onChange;
    std::string lastSynced;
  };

  void syncEntry(QLineEdit *widget, const NativeWidgetMeta &meta, EntryState &state) {
    if (!meta.entryValue) return;
    state.value = meta.entryValue;
    state.onChange = toStdFunction(meta.onEntryChange, meta.onEntryChangeUserdata);

    std::string widgetText = widget->text().toStdString();
    if (widgetText != state.lastSynced) {
      *state.value = widgetText;
      state.lastSynced = widgetText;
      if (meta.entryBuf) ui_internal::writeToStringBuf(*state.value, *meta.entryBuf);
      if (state.onChange) state.onChange(widgetText);
    } else if (*state.value != state.lastSynced) {
      widget->setText(QString::fromStdString(*state.value));
      state.lastSynced = *state.value;
    }
  }

  void ensureImagePixmap(QLabel *label, const NativeWidgetMeta &meta, int targetW, int targetH, const Clay_CornerRadius &corner) {
    if (!meta.image) return;
    const n8v::detail::DecodedImage *image =
      n8v::detail::getOrBakeRoundedImage(meta.image, targetW, targetH, corner.topLeft, corner.topRight, corner.bottomLeft, corner.bottomRight);
    auto it = imagePixmapSources_.find(label);
    if (it != imagePixmapSources_.end() && it->second == image) return;

    QImage qImage(image->rgba, image->width, image->height, image->width * 4, QImage::Format_RGBA8888);
    label->setPixmap(QPixmap::fromImage(qImage));
    imagePixmapSources_[label] = image;
  }

  struct ContainerFrame {
    QScrollArea *scrollArea = nullptr;
    QWidget *inner = nullptr;
    float originX = 0.0f;
    float originY = 0.0f;
  };

  QWidget *currentParent() const { return containerStack_.empty() ? window_ : containerStack_.back().inner; }

  void ensureScrollContainer(uint32_t id, const Clay_BoundingBox &box, const Clay_ClipRenderData &clip) {
    auto it = scrollContainers_.find(id);
    if (it == scrollContainers_.end()) {
      auto *scrollArea = new QScrollArea(currentParent());
      auto *inner = new QWidget();
      scrollArea->setWidget(inner);
      scrollArea->setWidgetResizable(false);
      it = scrollContainers_.emplace(id, ContainerFrame{scrollArea, inner, box.x, box.y}).first;
    }
    ContainerFrame &frame = it->second;
    frame.scrollArea->setHorizontalScrollBarPolicy(clip.horizontal ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    frame.scrollArea->setVerticalScrollBarPolicy(clip.vertical ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);

    float originX = 0.0f, originY = 0.0f;
    if (!containerStack_.empty()) {
      originX = containerStack_.back().originX;
      originY = containerStack_.back().originY;
    }
    frame.scrollArea->setGeometry((int)(box.x - originX), (int)(box.y - originY), (int)box.width, (int)box.height);
    frame.scrollArea->setVisible(true);
    frame.originX = box.x;
    frame.originY = box.y;

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(Clay_ElementId{id});
    if (scrollData.found) {
      frame.inner->resize((int)scrollData.contentDimensions.width, (int)scrollData.contentDimensions.height);
    }

    containerStack_.push_back(frame);
    touchedScrollContainers_.insert(id);
  }

  void closeScrollContainer() {
    if (!containerStack_.empty()) containerStack_.pop_back();
  }

  QWidget *ensureWidget(const WidgetKey &key, const NativeWidgetMeta &meta) {
    auto it = widgets_.find(key);
    if (it != widgets_.end()) {
      if (meta.kind == NativeWidgetKind::Button) {
        callbacks_[meta.ordinal] = toStdFunction(meta.onClick, meta.onClickUserdata);
      } else if (meta.kind == NativeWidgetKind::Link) {
        static_cast<N8VLinkLabel *>(it->second)->url = meta.url ? *meta.url : std::string();
      } else if (meta.kind == NativeWidgetKind::Checkbox && meta.checked) {
        auto *checkbox = static_cast<N8VCheckBox *>(it->second);
        checkbox->checkedPtr = meta.checked;
        checkbox->onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
        checkbox->setChecked(*meta.checked);
      } else if (meta.kind == NativeWidgetKind::Entry) {
        auto *lineEdit = static_cast<QLineEdit *>(it->second);
        lineEdit->setEchoMode(meta.password ? QLineEdit::Password : QLineEdit::Normal);
        lineEdit->setPlaceholderText(meta.placeholder ? QString::fromStdString(*meta.placeholder) : QString());
        syncEntry(lineEdit, meta, entryStates_[meta.ordinal]);
      } else if (meta.kind == NativeWidgetKind::Radio && meta.radioSelected) {
        auto *radio = static_cast<N8VRadioButton *>(it->second);
        radio->selectedPtr = meta.radioSelected;
        radio->value = meta.radioValue;
        radio->onChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
        bool shouldBeChecked = *meta.radioSelected == meta.radioValue;
        if (radio->isChecked() != shouldBeChecked) radio->setChecked(shouldBeChecked);
      } else if (meta.kind == NativeWidgetKind::Dropdown) {
        syncDropdown(static_cast<QComboBox *>(it->second), meta, dropdownStates_[meta.ordinal]);
      } else if (meta.kind == NativeWidgetKind::Slider) {
        syncSlider(static_cast<QSlider *>(it->second), meta, sliderStates_[meta.ordinal]);
      }
      return it->second;
    }

    QWidget *widget = nullptr;
    if (meta.kind == NativeWidgetKind::Button) {
      auto *button = new N8VButton(currentParent());
      callbacks_[meta.ordinal] = toStdFunction(meta.onClick, meta.onClickUserdata);
      button->callback = &callbacks_[meta.ordinal];
      widget = button;
    } else if (meta.kind == NativeWidgetKind::Checkbox) {
      auto *checkbox = new N8VCheckBox(currentParent());
      checkbox->checkedPtr = meta.checked;
      checkbox->onChange = toStdFunction(meta.onChange, meta.onChangeUserdata);
      checkbox->setChecked(meta.checked && *meta.checked);
      widget = checkbox;
    } else if (meta.kind == NativeWidgetKind::Entry) {
      auto *lineEdit = new QLineEdit(currentParent());
      lineEdit->setEchoMode(meta.password ? QLineEdit::Password : QLineEdit::Normal);
      lineEdit->setPlaceholderText(meta.placeholder ? QString::fromStdString(*meta.placeholder) : QString());
      syncEntry(lineEdit, meta, entryStates_[meta.ordinal]);
      widget = lineEdit;
    } else if (meta.kind == NativeWidgetKind::Radio) {
      auto *radio = new N8VRadioButton(currentParent());
      radio->setAutoExclusive(false);
      radio->selectedPtr = meta.radioSelected;
      radio->value = meta.radioValue;
      radio->onChange = toStdFunction(meta.onRadioChange, meta.onRadioChangeUserdata);
      radio->setChecked(meta.radioSelected && *meta.radioSelected == meta.radioValue);
      if (meta.radioSelected) {
        QButtonGroup *&group = radioGroups_[meta.radioSelected];
        if (!group) group = new QButtonGroup(window_);
        group->addButton(radio);
      }
      widget = radio;
    } else if (meta.kind == NativeWidgetKind::Dropdown) {
      auto *combo = new QComboBox(currentParent());
      if (meta.dropdownItems) {
        for (const std::string &item : *meta.dropdownItems) combo->addItem(QString::fromStdString(item));
      }
      DropdownState &state = dropdownStates_[meta.ordinal];

      int initialIndex = meta.dropdownSelected && *meta.dropdownSelected >= 0 ? *meta.dropdownSelected : -1;
      combo->setCurrentIndex(initialIndex);
      state.lastSynced = initialIndex;
      syncDropdown(combo, meta, state);
      widget = combo;
    } else if (meta.kind == NativeWidgetKind::Slider) {
      auto *sliderWidget = new QSlider(Qt::Horizontal, currentParent());
      sliderWidget->setRange(0, sliderSteps);
      SliderState &state = sliderStates_[meta.ordinal];
      int initialPos = meta.sliderValue ? sliderPositionFor(*meta.sliderValue, meta.sliderMin, meta.sliderMax) : 0;
      sliderWidget->setValue(initialPos);
      state.lastSynced = initialPos;
      syncSlider(sliderWidget, meta, state);
      widget = sliderWidget;
    } else if (meta.kind == NativeWidgetKind::Image) {
      auto *label = new QLabel(currentParent());
      label->setScaledContents(true);
      widget = label;
    } else {
      auto *label = new N8VLinkLabel(currentParent());
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

    auto *label = new QLabel(currentParent());
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label->setVisible(true);
    widgets_[key] = label;
    return label;
  }

  void positionWidget(QWidget *widget, const Clay_BoundingBox &box) {
    float originX = 0.0f, originY = 0.0f;
    if (!containerStack_.empty()) {
      originX = containerStack_.back().originX;
      originY = containerStack_.back().originY;
    }
    widget->setGeometry((int)(box.x - originX), (int)(box.y - originY), (int)box.width, (int)box.height);
  }

  std::unique_ptr<QApplication> app_;
  QWidget *window_ = nullptr;
  QLabel *measureLabel_ = nullptr;
  QPushButton *measureButton_ = nullptr;
  QLabel *measureLink_ = nullptr;
  QCheckBox *measureCheckbox_ = nullptr;
  QLineEdit *measureEntry_ = nullptr;
  QRadioButton *measureRadio_ = nullptr;
  QComboBox *measureCombo_ = nullptr;
  QSlider *measureSlider_ = nullptr;

  std::map<WidgetKey, QWidget *> widgets_;
  std::unordered_map<int, std::function<void()>> callbacks_;
  std::unordered_map<int, EntryState> entryStates_;
  std::unordered_map<int *, QButtonGroup *> radioGroups_;
  std::unordered_map<int, DropdownState> dropdownStates_;
  std::unordered_map<int, SliderState> sliderStates_;
  std::unordered_map<QLabel *, const void *> imagePixmapSources_;
  std::unordered_map<uint32_t, ContainerFrame> scrollContainers_;
  std::vector<ContainerFrame> containerStack_;
  std::set<uint32_t> touchedScrollContainers_;
};

} // namespace

std::unique_ptr<Backend> makeQtBackend() { return std::make_unique<QtBackend>(); }

} // namespace n8v::detail
