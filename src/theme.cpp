#include "theme.h"
#include "utils.h"

#include <QSvgRenderer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Theme {

namespace {

bool gDark = false;
bool gWatching = false;
QColor gAccentBtn;  // filled accent controls
QColor gAccentText; // accent used for text, indicators, focus

// ------------------------------------------------------------ accent ----

struct AccentPalette {
  bool valid = false;
  QColor light2, light3, base, dark1, dark2;
};

AccentPalette readWindowsAccent() {
  AccentPalette p;
#ifdef Q_OS_WIN
  // Explorer\Accent\AccentPalette: 8 RGBA entries
  // (Light3, Light2, Light1, Base, Dark1, Dark2, Dark3, unused) – the same
  // shades WinUI apps use.
  BYTE data[32];
  DWORD size = sizeof(data);
  if (RegGetValueW(HKEY_CURRENT_USER,
                   L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
                   L"AccentPalette", RRF_RT_REG_BINARY, nullptr, data,
                   &size) == ERROR_SUCCESS &&
      size >= 28) {
    auto at = [&](int i) { return QColor(data[i * 4], data[i * 4 + 1], data[i * 4 + 2]); };
    p.light3 = at(0);
    p.light2 = at(1);
    p.base = at(3);
    p.dark1 = at(4);
    p.dark2 = at(5);
    p.valid = p.base.isValid();
  }
#endif
  return p;
}

void computeAccent() {
  AccentPalette p = readWindowsAccent();
  if (!p.valid) {
    // Windows default blue
    p.light2 = QColor("#60CDFF");
    p.light3 = QColor("#99EBFF");
    p.base = QColor("#0078D4");
    p.dark1 = QColor("#005FB8");
    p.dark2 = QColor("#003E92");
  }
  if (gDark) {
    gAccentBtn = p.light2;
    gAccentText = p.light2;
  } else {
    gAccentBtn = p.dark1;
    gAccentText = p.dark1;
  }
}

// ------------------------------------------------------------- icons ----

QByteArray svgSource(const QString &name) {
  static QHash<QString, QByteArray> cache;
  auto it = cache.find(name);
  if (it != cache.end()) {
    return it.value();
  }
  QFile f(":/fluent/" + name + ".svg");
  QByteArray data = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  cache.insert(name, data);
  return data;
}

QColor iconColor(const QColor &forced, QIcon::Mode mode) {
  QColor c = forced.isValid() ? forced
             : gDark          ? QColor(255, 255, 255)
                              : QColor(0, 0, 0, 228);
  if (mode == QIcon::Disabled) {
    c.setAlphaF(c.alphaF() * (gDark ? 0.36 : 0.36));
  }
  return c;
}

void renderSvg(QPainter *p, const QRectF &rect, const QString &name,
               const QColor &color) {
  QByteArray svg = svgSource(name);
  svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
  QSvgRenderer r(svg);
  p->save();
  p->setOpacity(p->opacity() * color.alphaF());
  r.render(p, rect);
  p->restore();
}

class FluentIconEngine : public QIconEngine {
public:
  FluentIconEngine(const QString &name, const QColor &color)
      : mName(name), mColor(color) {}

  void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode,
             QIcon::State) override {
    renderSvg(painter, rect, mName, iconColor(mColor, mode));
  }

  QPixmap pixmap(const QSize &size, QIcon::Mode mode,
                 QIcon::State state) override {
    return scaledPixmap(size, mode, state, 1.0);
  }

  QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State,
                       qreal scale) override {
    QPixmap pm(size * scale);
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(scale);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    renderSvg(&p, QRectF(QPointF(0, 0), QSizeF(size)), mName,
              iconColor(mColor, mode));
    return pm;
  }

  QIconEngine *clone() const override {
    return new FluentIconEngine(mName, mColor);
  }
  QString key() const override { return "fluent"; }

private:
  QString mName;
  QColor mColor;
};

// Renders an icon into a PNG for use in the style sheet (check marks,
// chevrons). Files are regenerated whenever the theme changes.
QString iconFile(const QString &name, const QColor &color, int px = 16) {
  static QTemporaryDir dir;
  const QString file =
      dir.filePath(QString("%1-%2-%3.png").arg(name).arg(color.name(QColor::HexArgb).mid(1)).arg(px));
  if (!QFileInfo::exists(file)) {
    const qreal scale = 2.0; // crisp on high-DPI screens
    QImage img(QSize(px, px) * scale, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    img.setDevicePixelRatio(scale);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    renderSvg(&p, QRectF(0, 0, px, px), name, color);
    p.end();
    img.save(file);
  }
  return QDir::fromNativeSeparators(file);
}

// ------------------------------------------------------------ tokens ----

QColor mix(const QColor &a, const QColor &b, qreal t) {
  return QColor::fromRgbF(a.redF() * t + b.redF() * (1 - t),
                          a.greenF() * t + b.greenF() * (1 - t),
                          a.blueF() * t + b.blueF() * (1 - t));
}

QMap<QString, QString> buildTokens();
QMap<QString, QString> tokens() {
  static QMap<QString, QString> cache;
  static QString cacheKey;
  const QString key = QString("%1%2").arg(gDark).arg(gAccentBtn.name());
  if (key != cacheKey) {
    cache = buildTokens();
    cacheKey = key;
  }
  return cache;
}

QMap<QString, QString> buildTokens() {
  QMap<QString, QString> t;
  auto set = [&](const char *k, const QColor &c) {
    t[k] = c.alpha() == 255 ? c.name()
                            : QString("rgba(%1,%2,%3,%4)")
                                  .arg(c.red()).arg(c.green()).arg(c.blue())
                                  .arg(c.alpha());
  };
  const bool d = gDark;
  const QColor card = d ? QColor("#2B2B2B") : QColor("#FFFFFF");
  set("bg", d ? QColor("#202020") : QColor("#F3F3F3"));
  set("layer", d ? QColor("#272727") : QColor("#F9F9F9"));
  set("card", card);
  set("cardHover", d ? QColor("#323232") : QColor("#F6F6F6"));
  set("stroke", d ? QColor("#1C1C1C") : QColor("#E5E5E5"));
  set("cardStroke", d ? QColor("#363636") : QColor("#E5E5E5"));
  set("divider", d ? QColor("#383838") : QColor("#E5E5E5"));
  set("text", d ? QColor("#FFFFFF") : QColor("#1B1B1B"));
  set("text2", d ? QColor("#CFCFCF") : QColor("#5F5F5F"));
  set("text3", d ? QColor("#8A8A8A") : QColor("#9E9E9E"));
  set("ctrl", d ? QColor("#2D2D2D") : QColor("#FFFFFF"));
  set("ctrlHover", d ? QColor("#323232") : QColor("#F9F9F9"));
  set("ctrlPressed", d ? QColor("#272727") : QColor("#F5F5F5"));
  set("ctrlDisabled", d ? QColor("#2A2A2A") : QColor("#F5F5F5"));
  set("ctrlStroke", d ? QColor("#3B3B3B") : QColor("#E5E5E5"));
  set("ctrlStrokeBottom", d ? QColor("#353535") : QColor("#CCCCCC"));
  set("inputBottom", d ? QColor("#9A9A9A") : QColor("#868686"));
  set("inputFocus", d ? QColor("#1F1F1F") : QColor("#FFFFFF"));
  set("subtleHover", d ? QColor(255, 255, 255, 15) : QColor(0, 0, 0, 10));
  set("subtlePressed", d ? QColor(255, 255, 255, 10) : QColor(0, 0, 0, 6));
  set("flyout", d ? QColor("#2C2C2C") : QColor("#F9F9F9"));
  set("flyoutStroke", d ? QColor("#1A1A1A") : QColor("#D8D8D8"));
  set("scroll", d ? QColor(255, 255, 255, 110) : QColor(0, 0, 0, 110));
  set("track", d ? QColor("#4A4A4A") : QColor("#D6D6D6"));
  set("accentBtn", gAccentBtn);
  set("accentBtnHover", d ? gAccentBtn.darker(110) : gAccentBtn.lighter(112));
  set("accentBtnPressed", d ? gAccentBtn.darker(125) : gAccentBtn.lighter(125));
  set("accentText", gAccentText);
  set("onAccent", d ? QColor("#000000") : QColor("#FFFFFF"));
  set("sel", mix(gAccentText, card, d ? 0.22 : 0.14));
  set("selHover", mix(gAccentText, card, d ? 0.28 : 0.20));
  set("success", d ? QColor("#6CCB5F") : QColor("#0F7B0F"));
  set("critical", d ? QColor("#FF99A4") : QColor("#C42B1C"));
  set("checkStroke", d ? QColor("#9A9A9A") : QColor("#878787"));

  const QColor iconC = d ? QColor(255, 255, 255, 200) : QColor(0, 0, 0, 170);
  t["imgCheck"] = iconFile("checkmark", d ? QColor("#000000") : QColor("#FFFFFF"), 14);
  t["imgRight"] = iconFile("chevron", iconC, 12);
  t["imgDown"] = iconFile("expand", iconC, 12);
  t["imgUp"] = iconFile("collapse", iconC, 12);
  t["imgDownDisabled"] = iconFile("expand", d ? QColor(255, 255, 255, 80) : QColor(0, 0, 0, 70), 12);
  return t;
}

const char *kStyleSheet = R"QSS(
* { outline: 0; }
QWidget { color: @text; }
QMainWindow, QDialog, QMessageBox { background: @bg; }
QWidget#NavPane { background: @bg; }
QWidget#ContentArea {
  background: @layer;
  border-left: 1px solid @stroke;
  border-top: 1px solid @stroke;
  border-top-left-radius: 8px;
}
QTabWidget#tabs::pane { border: none; background: transparent; }
QTabWidget#tabs > QStackedWidget > QWidget,
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }

QLabel#PageTitle { font-size: 20pt; font-weight: 600; padding: 0; }
QLabel#PageSubtitle, QLabel[secondary="true"] { color: @text2; }
QDialogButtonBox { dialogbuttonbox-buttons-have-icons: 0; }
QLabel#noJobsAvailable { color: @text2; }
QLabel#SectionHeader { font-weight: 600; padding-top: 4px; }

QWidget[card="true"], JobWidget, MountWidget, StreamWidget {
  background: @card;
  border: 1px solid @cardStroke;
  border-radius: 8px;
}
JobWidget QLineEdit, MountWidget QLineEdit, StreamWidget QLineEdit { background: transparent; border: none; padding: 1px 0; }
QToolButton[status] { border: none; font-weight: 600; padding: 2px 6px; }
QToolButton[status="running"] { color: @accentText; }
QToolButton[status="done"] { color: @success; }
QToolButton[status="error"] { color: @critical; }
QWidget#jobsArea QFrame[frameShape="4"] { border: none; background: transparent; max-height: 2px; }

/* ---------- navigation pane ---------- */
QListWidget#NavList { background: transparent; border: none; }
QListWidget#NavList::item {
  min-height: 36px; padding-left: 8px; margin: 1px 4px;
  border-radius: 5px; border: none; border-left: 3px solid transparent;
}
QListWidget#NavList::item:hover { background: @subtleHover; }
QListWidget#NavList::item:selected { background: @subtleHover; color: @text; border-left: 3px solid @accentText; }
QListWidget#NavList::item:disabled { color: @text2; background: transparent; min-height: 30px; }
QToolButton#NavFooter { text-align: left; padding: 8px 10px 8px 15px; margin: 0 4px; border-radius: 5px; }

/* ---------- buttons ---------- */
QPushButton {
  background: @ctrl; border: 1px solid @ctrlStroke; border-bottom-color: @ctrlStrokeBottom;
  border-radius: 5px; padding: 5px 14px; min-height: 20px;
}
QPushButton:hover { background: @ctrlHover; }
QPushButton:pressed { background: @ctrlPressed; color: @text2; border-bottom-color: @ctrlStroke; }
QPushButton:disabled { background: @ctrlDisabled; color: @text3; border-color: @ctrlStroke; }
QPushButton:default, QPushButton[accent="true"] {
  background: @accentBtn; color: @onAccent; border: 1px solid @accentBtn;
}
QPushButton:default:hover, QPushButton[accent="true"]:hover { background: @accentBtnHover; border-color: @accentBtnHover; }
QPushButton:default:pressed, QPushButton[accent="true"]:pressed { background: @accentBtnPressed; border-color: @accentBtnPressed; }
QPushButton:default:disabled, QPushButton[accent="true"]:disabled { background: @ctrlDisabled; color: @text3; border-color: @ctrlStroke; }

QToolButton {
  background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 5px 8px;
}
QToolButton:hover { background: @subtleHover; }
QToolButton:pressed { background: @subtlePressed; color: @text2; }
QToolButton:checked { background: @subtleHover; }
QToolButton:disabled { color: @text3; }
QToolButton::menu-indicator { image: none; width: 0; }
QToolButton[popupMode="1"] { padding-right: 18px; }
QToolButton::menu-button { border: none; width: 16px; }
QToolButton::menu-arrow { image: url(@imgDown); }

/* ---------- inputs ---------- */
QLineEdit, QAbstractSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
  background: @ctrl; border: 1px solid @ctrlStroke; border-bottom: 1px solid @inputBottom;
  border-radius: 5px; padding: 5px 9px;
  selection-background-color: @accentBtn; selection-color: @onAccent;
}
QPlainTextEdit, QTextEdit { padding: 4px; }
QLineEdit:hover, QAbstractSpinBox:hover, QComboBox:hover { background: @ctrlHover; }
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
  background: @inputFocus; border-bottom: 2px solid @accentText; padding-bottom: 4px;
}
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled { background: @ctrlDisabled; color: @text3; border-bottom-color: @ctrlStroke; }
QLineEdit:read-only { background: @ctrlDisabled; }
QComboBox { padding-right: 30px; }
QComboBox::drop-down { border: none; width: 30px; subcontrol-origin: padding; subcontrol-position: center right; }
QComboBox::down-arrow { image: url(@imgDown); width: 12px; height: 12px; }
QComboBox::down-arrow:disabled { image: url(@imgDownDisabled); }
QComboBox QAbstractItemView {
  background: @flyout; border: 1px solid @flyoutStroke; padding: 4px;
  selection-background-color: @subtleHover; selection-color: @text;
}
QComboBox QAbstractItemView::item { min-height: 30px; padding-left: 6px; border-radius: 4px; }
QAbstractSpinBox { padding-right: 26px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; width: 22px; background: transparent; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background: @subtleHover; border-radius: 3px; }
QAbstractSpinBox::up-arrow { image: url(@imgUp); width: 10px; height: 10px; }
QAbstractSpinBox::down-arrow { image: url(@imgDown); width: 10px; height: 10px; }

/* ---------- check boxes & radio buttons ---------- */
QCheckBox, QRadioButton { spacing: 8px; padding: 2px 0; }
QCheckBox::indicator, QTreeView::indicator, QListView::indicator {
  width: 18px; height: 18px; border-radius: 4px;
  border: 1px solid @checkStroke; background: @ctrl;
}
QCheckBox::indicator:hover { background: @ctrlHover; }
QCheckBox::indicator:checked, QTreeView::indicator:checked, QListView::indicator:checked {
  background: @accentBtn; border-color: @accentBtn; image: url(@imgCheck);
}
QCheckBox::indicator:disabled { border-color: @ctrlStroke; background: @ctrlDisabled; }
QRadioButton::indicator { width: 18px; height: 18px; border-radius: 10px; border: 1px solid @checkStroke; background: @ctrl; }
QRadioButton::indicator:hover { background: @ctrlHover; }
QRadioButton::indicator:checked { border: 5px solid @accentBtn; background: @onAccent; width: 10px; height: 10px; }

/* ---------- lists & trees ---------- */
QTreeView, QListView, QTableView {
  background: transparent; border: none;
  alternate-background-color: @subtlePressed;
  selection-background-color: @sel; selection-color: @text;
}
QTreeView::item, QListView::item { min-height: 30px; padding: 0 4px; border: none; }
QTreeView::item:hover, QListView::item:hover { background: @subtleHover; }
QTreeView::item:selected, QListView::item:selected { background: @sel; color: @text; }
QTreeView::item:selected:hover, QListView::item:selected:hover { background: @selHover; }
QTreeView::branch { background: transparent; }
QTreeView::branch:hover { background: @subtleHover; }
QTreeView::branch:selected { background: @sel; }
QTreeView::branch:has-children:closed { image: url(@imgRight); }
QTreeView::branch:has-children:open { image: url(@imgDown); }
QHeaderView { background: transparent; border: none; }
QHeaderView::section {
  background: transparent; color: @text2; border: none; border-bottom: 1px solid @divider;
  padding: 7px 8px;
}
QHeaderView::section:hover { background: @subtleHover; }
QHeaderView::up-arrow { image: url(@imgUp); width: 10px; height: 10px; }
QHeaderView::down-arrow { image: url(@imgDown); width: 10px; height: 10px; }

QListWidget#remotes { background: transparent; border: none; }
QListWidget#remotes::item {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px;
  margin: 6px; padding: 10px 6px 8px 6px; color: @text;
}
QListWidget#remotes::item:hover { background: @cardHover; }
QListWidget#remotes::item:selected { background: @cardHover; border: 1px solid @accentText; color: @text; }

QListWidget#tasksListWidget { background: transparent; border: none; }
QListWidget#tasksListWidget::item {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px; margin: 3px 0; padding: 8px 10px;
}
QListWidget#tasksListWidget::item:hover { background: @cardHover; }
QListWidget#tasksListWidget::item:selected { background: @cardHover; border: 1px solid @accentText; color: @text; }

/* ---------- menus, tooltips ---------- */
QMenuBar { background: @bg; }
QMenu { background: @flyout; border: 1px solid @flyoutStroke; padding: 4px; }
QMenu::item { padding: 7px 28px 7px 10px; border-radius: 4px; margin: 1px 0; }
QMenu::item:selected { background: @subtleHover; color: @text; }
QMenu::item:disabled { color: @text3; }
QMenu::separator { height: 1px; background: @divider; margin: 4px 2px; }
QMenu::icon { padding-left: 10px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }
QMenu::indicator:checked { image: url(@imgCheck); background: @accentBtn; border-radius: 3px; }
QToolTip { background: @flyout; color: @text; border: 1px solid @flyoutStroke; padding: 5px 8px; }

/* ---------- containers ---------- */
QGroupBox {
  background: @card; border: 1px solid @cardStroke; border-radius: 8px;
  margin-top: 26px; padding: 14px 10px 10px 10px;
}
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 2px; top: 2px; font-weight: 600; }
QTabWidget::pane { border: 1px solid @cardStroke; border-radius: 8px; background: @card; top: -1px; }
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
  background: transparent; color: @text2; border: none; border-bottom: 2px solid transparent;
  padding: 7px 14px; margin-right: 2px;
}
QTabBar::tab:hover { color: @text; }
QTabBar::tab:selected { color: @text; border-bottom: 2px solid @accentText; }
QSplitter::handle { background: transparent; }
QStatusBar { background: @bg; color: @text2; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @text2; padding: 0 4px; }

/* ---------- progress & scroll bars ---------- */
QProgressBar {
  background: @track; border: none; border-radius: 2px;
  min-height: 4px; max-height: 4px; color: transparent;
}
QProgressBar::chunk { background: @accentText; border-radius: 2px; }
QScrollBar:vertical { background: transparent; width: 12px; margin: 2px 2px 2px 0; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 0 2px 2px 2px; }
QScrollBar::handle:vertical { background: @scroll; border-radius: 2px; min-height: 32px; margin: 0 4px; }
QScrollBar::handle:horizontal { background: @scroll; border-radius: 2px; min-width: 32px; margin: 4px 0; }
QScrollBar::handle:vertical:hover { margin: 0 2px; border-radius: 3px; }
QScrollBar::handle:horizontal:hover { margin: 2px 0; border-radius: 3px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }
)QSS";

QString buildStyleSheet() {
  QString s = QString::fromUtf8(kStyleSheet);
  const auto t = tokens();
  // replace longest names first so "@ctrlHover" isn't eaten by "@ctrl"
  QStringList keys = t.keys();
  std::sort(keys.begin(), keys.end(),
            [](const QString &a, const QString &b) { return a.size() > b.size(); });
  for (const auto &k : keys) {
    s.replace("@" + k, t.value(k));
  }
  return s;
}

QPalette buildPalette() {
  const auto t = tokens();
  auto c = [&](const char *k) { return QColor(t.value(k)); };
  QPalette p;
  p.setColor(QPalette::Window, c("bg"));
  p.setColor(QPalette::WindowText, c("text"));
  p.setColor(QPalette::Base, c("ctrl"));
  p.setColor(QPalette::AlternateBase, c("layer"));
  p.setColor(QPalette::Text, c("text"));
  p.setColor(QPalette::Button, c("ctrl"));
  p.setColor(QPalette::ButtonText, c("text"));
  p.setColor(QPalette::ToolTipBase, c("flyout"));
  p.setColor(QPalette::ToolTipText, c("text"));
  p.setColor(QPalette::PlaceholderText, c("text3"));
  p.setColor(QPalette::Highlight, gAccentBtn);
  p.setColor(QPalette::HighlightedText, c("onAccent"));
  p.setColor(QPalette::Link, gAccentText);
  p.setColor(QPalette::LinkVisited, gAccentText);
  p.setColor(QPalette::Light, c("ctrl"));
  p.setColor(QPalette::Midlight, c("ctrlStroke"));
  p.setColor(QPalette::Mid, c("ctrlStroke"));
  p.setColor(QPalette::Dark, c("ctrlStrokeBottom"));
  p.setColor(QPalette::Shadow, gDark ? Qt::black : QColor(0, 0, 0, 60));
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    p.setColor(QPalette::Disabled, role, c("text3"));
  }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  p.setColor(QPalette::Accent, gAccentBtn);
#endif
  return p;
}

bool systemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return false;
#endif
}

// ------------------------------------------------- Windows window chrome ----

#ifdef Q_OS_WIN
void styleNativeWindow(QWidget *w) {
  HWND hwnd = reinterpret_cast<HWND>(w->winId());
  if (!hwnd) {
    return;
  }
  // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (Windows 10 2004+ / 11)
  BOOL dark = gDark ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));

  const bool popup = (w->windowFlags() & Qt::Popup) == Qt::Popup ||
                     w->windowType() == Qt::ToolTip;
  if (popup) {
    // 33 = DWMWA_WINDOW_CORNER_PREFERENCE, 2 = DWMWCP_ROUND (Windows 11)
    int corner = 3; // DWMWCP_ROUNDSMALL for menus and tooltips
    DwmSetWindowAttribute(hwnd, 33, &corner, sizeof(corner));
  } else {
    // Title bar in the window background colour (Windows 11 22000+):
    // 35 = DWMWA_CAPTION_COLOR, 36 = DWMWA_TEXT_COLOR
    QColor bg = gDark ? QColor("#202020") : QColor("#F3F3F3");
    QColor fg = gDark ? QColor("#FFFFFF") : QColor("#1B1B1B");
    COLORREF cap = RGB(bg.red(), bg.green(), bg.blue());
    COLORREF txt = RGB(fg.red(), fg.green(), fg.blue());
    DwmSetWindowAttribute(hwnd, 35, &cap, sizeof(cap));
    DwmSetWindowAttribute(hwnd, 36, &txt, sizeof(txt));
  }
}

class WindowChromeFilter : public QObject {
public:
  using QObject::QObject;
  bool eventFilter(QObject *o, QEvent *e) override {
    if (e->type() == QEvent::Show) {
      if (auto *w = qobject_cast<QWidget *>(o); w && w->isWindow()) {
        styleNativeWindow(w);
      }
    }
    return false;
  }
};
#endif

void applyChromeToAllWindows() {
#ifdef Q_OS_WIN
  static WindowChromeFilter *filter = nullptr;
  if (!filter) {
    filter = new WindowChromeFilter(qApp);
    qApp->installEventFilter(filter);
  }
  for (QWidget *w : QApplication::topLevelWidgets()) {
    if (w->isVisible()) {
      styleNativeWindow(w);
    }
  }
#endif
}

QFont fluentFont() {
  QFont f = QApplication::font();
  // Windows 11 UI font, with fallbacks for Windows 10 and other systems.
  f.setFamilies({"Segoe UI Variable Text", "Segoe UI", "Selawik", "Noto Sans",
                 "Sans Serif"});
  f.setPointSizeF(10);
  f.setHintingPreference(QFont::PreferNoHinting);
  return f;
}

} // namespace

// ------------------------------------------------------------ public ----

Mode load() {
  auto settings = GetSettings();
  if (!settings->contains("Settings/theme")) {
    if (settings->contains("Settings/darkMode")) {
      return settings->value("Settings/darkMode").toBool() ? Dark : System;
    }
    return System;
  }
  int v = settings->value("Settings/theme").toInt();
  return (v >= System && v <= Dark) ? Mode(v) : System;
}

void save(Mode mode) {
  auto settings = GetSettings();
  settings->setValue("Settings/theme", int(mode));
  settings->setValue("Settings/darkMode", mode == Dark);
}

void apply() {
  const Mode mode = load();
  gDark = mode == Dark || (mode == System && systemIsDark());
  computeAccent();

  qApp->setStyle(QStyleFactory::create("Fusion"));
  qApp->setFont(fluentFont());
  qApp->setPalette(buildPalette());
  qApp->setStyleSheet(buildStyleSheet());
  applyChromeToAllWindows();

  {
    auto settings = GetSettings();
    settings->setValue("Settings/darkModeIni", gDark);
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  if (!gWatching) {
    gWatching = true;
    QObject::connect(QGuiApplication::styleHints(),
                     &QStyleHints::colorSchemeChanged, qApp, [] {
                       if (load() == System) {
                         apply();
                       }
                     });
  }
#else
  Q_UNUSED(gWatching);
#endif
}

bool isDark() { return gDark; }
bool usingFusionDark() { return gDark; }
QColor accent() { return gAccentText; }

QIcon icon(const QString &name, const QColor &color) {
  return QIcon(new FluentIconEngine(name, color));
}

QColor color(const char *token) {
  const QString v = tokens().value(token);
  if (v.startsWith("rgba(")) {
    const auto p = v.mid(5, v.size() - 6).split(',');
    return QColor(p[0].toInt(), p[1].toInt(), p[2].toInt(), p[3].toInt());
  }
  return QColor(v);
}

void setStatus(QWidget *w, const char *status) {
  w->setProperty("status", status);
  w->style()->unpolish(w);
  w->style()->polish(w);
  w->update();
}

QString displayName(Mode mode) {
  switch (mode) {
  case Light:
    return QObject::tr("Light");
  case Dark:
    return QObject::tr("Dark");
  default:
    return QObject::tr("Same as Windows");
  }
}

} // namespace Theme
