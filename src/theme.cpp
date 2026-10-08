#include "theme.h"
#include "utils.h"

namespace Theme {

namespace {

bool gFusionDark = false;
[[maybe_unused]] bool gWatching = false;

QPalette darkPalette() {
  QPalette p;
  const QColor window(32, 32, 32), base(25, 25, 25), alt(43, 43, 43),
      text(235, 235, 235), disabled(128, 128, 128), accent(0, 120, 212);
  p.setColor(QPalette::Window, window);
  p.setColor(QPalette::WindowText, text);
  p.setColor(QPalette::Base, base);
  p.setColor(QPalette::AlternateBase, alt);
  p.setColor(QPalette::ToolTipBase, alt);
  p.setColor(QPalette::ToolTipText, text);
  p.setColor(QPalette::PlaceholderText, disabled);
  p.setColor(QPalette::Text, text);
  p.setColor(QPalette::Button, alt);
  p.setColor(QPalette::ButtonText, text);
  p.setColor(QPalette::BrightText, Qt::red);
  p.setColor(QPalette::Link, QColor(96, 175, 255));
  p.setColor(QPalette::Highlight, accent);
  p.setColor(QPalette::HighlightedText, Qt::white);
  p.setColor(QPalette::Mid, QColor(60, 60, 60));
  p.setColor(QPalette::Dark, QColor(20, 20, 20));
  for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
    p.setColor(QPalette::Disabled, role, disabled);
  }
  return p;
}

bool systemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
  return false;
#endif
}

bool canUseWindows11Style() {
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  return QOperatingSystemVersion::current() >=
             QOperatingSystemVersion::Windows11 &&
         QStyleFactory::keys().contains("windows11", Qt::CaseInsensitive);
#else
  return false;
#endif
}

QString nativeStyleName() {
#if defined(Q_OS_WIN)
  if (canUseWindows11Style()) {
    return "windows11";
  }
  return "windowsvista";
#else
  return QString(); // let Qt pick the platform default
#endif
}

} // namespace

Mode load() {
  auto settings = GetSettings();
  if (!settings->contains("Settings/theme")) {
    // migrate the old boolean "dark mode" option
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
  // keep the legacy key meaningful for older builds sharing the same .ini
  settings->setValue("Settings/darkMode", mode == Dark);
}

void apply() {
  const Mode mode = load();
  const QString native = nativeStyleName();

  if (canUseWindows11Style()) {
    // Fluent style draws light and dark itself.
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    QGuiApplication::styleHints()->setColorScheme(
        mode == Dark    ? Qt::ColorScheme::Dark
        : mode == Light ? Qt::ColorScheme::Light
                        : Qt::ColorScheme::Unknown);
#endif
    qApp->setStyle(QStyleFactory::create(native));
    qApp->setPalette(qApp->style()->standardPalette());
    qApp->setStyleSheet(QString());
    gFusionDark = false;
  } else {
    const bool dark = mode == Dark || (mode == System && systemIsDark());
    if (dark) {
      qApp->setStyle(QStyleFactory::create("Fusion"));
      qApp->setPalette(darkPalette());
      gFusionDark = true;
    } else {
      if (!native.isEmpty()) {
        qApp->setStyle(QStyleFactory::create(native));
      }
      qApp->setPalette(qApp->style()->standardPalette());
      gFusionDark = false;
    }
    qApp->setStyleSheet(QString());
  }

  {
    auto settings = GetSettings();
    settings->setValue("Settings/darkModeIni", gFusionDark);
  }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  // Follow Windows switching between light and dark while we're running.
  if (!gWatching) {
    gWatching = true;
    QObject::connect(QGuiApplication::styleHints(),
                     &QStyleHints::colorSchemeChanged, qApp, [] {
                       if (load() == System) {
                         apply();
                       }
                     });
  }
#endif
}

bool usingFusionDark() { return gFusionDark; }

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
