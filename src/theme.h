#pragma once

#include "pch.h"

// Application look & feel.
//  System: follow the Windows light/dark setting, live.
//  Light / Dark: force one.
// On Windows 11 with Qt >= 6.7 the native "windows11" (Fluent) style is used,
// which handles dark mode itself. Elsewhere dark mode falls back to Fusion
// with a dark palette.
namespace Theme {

enum Mode { System = 0, Light = 1, Dark = 2 };

Mode load();
void save(Mode mode);

// Apply the saved theme to qApp. Safe to call again after settings change.
void apply();

// True when the Fusion dark fallback is active (some icon sizing in the
// remote view depends on Fusion's metrics).
bool usingFusionDark();

QString displayName(Mode mode);

} // namespace Theme
