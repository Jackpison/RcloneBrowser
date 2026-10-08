#pragma once

#include "pch.h"

// Windows 11 (Fluent / WinUI 3) look & feel for the whole application.
//
// The look is produced by Fusion + a token-driven style sheet rather than the
// native style, so it is identical on every Windows 10/11 build and can be
// switched between light and dark at runtime. Colours follow the WinUI 3
// design tokens and the user's Windows accent colour. On Windows, title bars
// are tinted to match and popups get native Windows 11 rounded corners.
namespace Theme {

enum Mode { System = 0, Light = 1, Dark = 2 };

Mode load();
void save(Mode mode);

// Apply the saved theme to qApp. Safe to call again after settings change.
void apply();

// True when the dark variant is active right now.
bool isDark();

// Kept for older code paths (remote type icons use "_inv" in dark mode).
bool usingFusionDark();

QString displayName(Mode mode);

// Accent colour as used for text/indicators in the current mode.
QColor accent();

// A Fluent UI System icon from :/fluent (see src/icons/fluent). The icon is
// re-tinted automatically for light/dark and disabled state. Pass a colour to
// force one (e.g. on accent-coloured buttons).
QIcon icon(const QString &name, const QColor &color = QColor());

// A design-token colour for custom painting, e.g. "card", "cardHover",
// "cardStroke", "text", "text2", "accentText".
QColor color(const char *token);

// Colour-coded file type icon (folders, images, video, audio, archives...).
QIcon fileIcon(const QString &fileName, bool isFolder);

// Colour-coded tile identifying a remote's storage service. Users can
// override any type with icons\remotes\<type>.png|svg|ico next to the exe.
QIcon remoteIcon(const QString &type);

// Status text styling for job cards: "running", "done" or "error".
void setStatus(QWidget *w, const char *status);

} // namespace Theme
