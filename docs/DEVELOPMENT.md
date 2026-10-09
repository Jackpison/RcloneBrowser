# Development notes

Notes for maintainers and contributors. State: **v4.1.0** (October 2026).

## Project at a glance
- A revival of the abandoned [kapitainsky/RcloneBrowser](https://github.com/kapitainsky/RcloneBrowser): a Qt 6 GUI for [rclone](https://rclone.org/), maintained by [Jackpison](https://github.com/Jackpison).
- Original by Martins Mozeiko ([mmozeiko/RcloneBrowser](https://github.com/mmozeiko/RcloneBrowser)), later DinCahill and kapitainsky. MIT license.
- Website: https://jackpison.github.io/RcloneBrowser/ (source in `site/`).
- The name stays **Rclone Browser**; "rclone GUI" is used as a search keyword in the website, README and PDF metadata.

## Platform policy
- **Windows 10/11 is the primary, tested platform.** The maintainer tests every release there.
- **Linux (x86_64 AppImage) exists because users asked for it.** It is built from the same code and must pass the same CI self-test, but it is not hand-tested on every distribution. Linux problems are handled from user reports.
- Decided against for now: native Wayland, desktop portals, Flatpak, GTK4, a rewrite in another language. Reasons: no demand yet, extra risk for the working Linux build, and the risky work (network, credentials, file data) happens inside rclone, which is written in Go. Revisit only if users ask.

## Layout
| Path | What |
|---|---|
| `src/` | C++20 / Qt 6 source |
| `src/icons/fluent/` | Fluent UI System Icons (MIT); every SVG is compiled in as `:/fluent/<name>.svg` |
| `src/icons/folder`, `src/icons/drive` | Pre-rendered folder and drive icons |
| `icons/remotes/` | Bundled service logos (256 px PNG named after the rclone type); shipped in both packages |
| `tests/` | Unit tests for rclone output parsing (`ctest`) |
| `docs/` | Screenshots, `remote-icons.md`, PDF guide, `guide/build_guide.py`, these notes |
| `site/` | Website (single static page, no build step) |
| `packaging/linux/` | `.desktop` file, icon, AppStream metadata for the AppImage |
| `.github/workflows/` | `update.yml` (builds and releases), `pages.yml` (website) |

Key source files:
- `theme.cpp/.h`: all look and feel: colour tokens, the style sheet, Fluent icons, file-type icons, remote tiles, logo lookup, font self-test.
- `main_window_shell.cpp`: sidebar, page stack, remote tabs, theme button.
- `remote_widget.cpp`: browser page (command bar, breadcrumbs, Icons and Details views).
- `item_model.cpp`: lazy folder model; one `rclone lsjson` per folder.
- `rclone_output.cpp`: all parsing of rclone output (unit-tested).
- `rclone_updater.cpp`: rclone download, verification and install.
- `job_widget.cpp`: transfer cards, pause and resume.
- `main.cpp`: startup (theme is applied before any window exists) and the `--selftest` mode.

## Text sizes: rules to keep
Windows keeps its own font per widget class (small, 9 pt) and can reset them while the app runs. Patching single widgets caused the same bug to return again and again. Since 4.1.0:
1. The **style sheet** sets font family and size for every widget (`* { ... }` at the top of `kStyleSheet` in `theme.cpp`). To change a size, add or edit a style sheet rule. **Do not call `setFont()` on widgets.**
2. A guard (`FontGuard`) re-applies the fonts if the system resets them.
3. `Theme::selfTestFonts()` is run by `--selftest` in CI on both platforms and checks 17 kinds of widget at startup, after a simulated font reset and after theme switches. If you add a new kind of widget that needs a special size, add a probe there.
4. Anything that stores a theme-coloured pixmap must redraw on `Theme::notifier()->changed()`.

The font is Trebuchet MS. It ships with Windows; its license does not allow bundling, so on Linux it is used when installed and otherwise a similar font (Ubuntu, Fira Sans, Noto Sans, DejaVu Sans) is chosen.

## Building
Windows: Visual Studio 2022+ (C++), CMake 3.21+, Ninja, Qt 6.10+ for MSVC 64-bit incl. Qt SVG.
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\msvc2022_64
cmake --build build
ctest --test-dir build
```
Linux: C++20 compiler, CMake, Ninja, Qt 6.10+ (`qt6-base-dev qt6-svg-dev` on Ubuntu).

Checks before a release:
- `ctest --test-dir build` (parser tests)
- `RcloneBrowser --selftest` (platform plugin, image formats, TLS, fonts); CI runs it on the packaged app
- Windows: look at dark and light mode, open a remote in both views, switch the theme and compare

## CI and releases
`update.yml` (workflow name **Update**) has three jobs:
- `windows`: Qt via aqtinstall, MSVC build with Control Flow Guard and CET, `windeployqt` into `qt/plugins` with `qt.conf`, bundled rclone (checksum-verified) and VC++ runtime, self-test of the packaged folder, zip.
- `linux`: Ubuntu build, AppImage via linuxdeploy (tool downloads pinned by SHA-256), bundled rclone, self-test under Xvfb.
- `release` (tags `v*` only): builds `SHA256SUMS.txt`, takes the release notes from the matching `CHANGELOG.md` section, publishes the release.

All actions are pinned to full commit SHAs. Python is pinned to 3.13 (aqtinstall has problems with 3.14).

**Release procedure**
1. Bump `VERSION` and `src/resources.rc` (they must match).
2. Add `## [x.y.z] - date` to `CHANGELOG.md` (its text becomes the release notes).
3. If the UI changed: retake screenshots in `docs/` and run `python docs/guide/build_guide.py` (needs `pip install reportlab`).
4. Push to `main` and wait for a green **Update** run.
5. `git tag -a vX.Y.Z -m "Rclone Browser X.Y.Z"` and push the tag.

Commits that only touch documentation can include `[skip ci]` in the message to avoid a needless build.

## Qt version
- Builds use **Qt 6.10.3** (Windows and Linux).
- Newer exists: Qt 6.11 (March 2026; 6.11.3 is the last release of that series) and Qt 6.12 (LTS, planned for October 2026). When tested, the CI Qt installer could not yet read 6.11 packages.
- To upgrade: change `version:` for the Qt install steps in `update.yml` (both jobs), check that the installer can resolve it, build, run the self-test, test on Windows, release as a patch or minor version. Prefer the 6.12 LTS once available.

## Ideas, not scheduled
- winget or Scoop manifest for Windows.
- Web version on top of `rclone rcd` (separate repository, own design).
- More bundled service logos. Still colour tiles: `swift`, `oos`, `qingstor`, `sia`, `netstorage`, `cloudinary`, `imagekit`, `internetarchive`, `sugarsync`, `huaweidrive`, `quatrix`, `shade`, `filefabric`, `linkbox`. See `docs/remote-icons.md` for file names.
- Google Search Console: submit `site/sitemap.xml` once the website is live.
- Housekeeping in the Actions tab: disable the old "Windows" and "Windows portable" workflow entries (history only).

## Version history (short)
| Version | Highlights |
|---|---|
| 1.9.0 | Qt 6 port, parser fixes for rclone 1.56+, updater, themes |
| 2.0.0 | Windows 11 redesign, pause and resume, Fluent icons, tabs |
| 3.0.0 | Black-and-gold theme, Icons view, `lsjson` loading, PDF guide, C++20 |
| 3.1 to 3.6 | Font fixes, bundled logos, adaptive command bar, icon sizes |
| 3.7.0 | File Explorer style tabs, tidier folder, website, CI self-test |
| 4.0.0 | Linux AppImage, extra columns, sortable remotes, 34 logos |
| 4.0.1 | Fast single-file delete, clearer delete dialog, popup fonts |
| 4.1.0 | Root-cause text-size fix and font self-test, footer order, breadcrumb colour |

Full details: [CHANGELOG.md](../CHANGELOG.md).
