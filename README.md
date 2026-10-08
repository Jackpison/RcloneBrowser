<p align="center"><img src="docs/logo.png" width="96" alt="Rclone Browser logo"></p>

<h1 align="center">Rclone Browser for Windows</h1>

<p align="center">A modern Windows 11 app for browsing and transferring files on all your cloud storage, powered by <a href="https://rclone.org/">rclone</a>.<br>Portable, nothing to install: unzip it anywhere (even on a USB stick) and run.</p>

![Browsing a remote, with tabs](docs/browser.png)

## Features

- **Windows 11 design**: navigation pane, Explorer-style tabs, command bar, breadcrumb address bar, Fluent icons, light and dark themes, follows your Windows accent colour
- **All your clouds in one place**: Google Drive, OneDrive, Dropbox, pCloud, S3, SFTP, encrypted remotes and 70+ more, using your existing rclone configuration. Each service gets its own colour-coded tile.
- **Tabs**: open several remotes, or the same remote several times (Ctrl+T)
- Upload, download, create folders, rename, move and delete; drag and drop files from File Explorer to upload
- **Transfers you can pause and resume**, with live progress, speed and time left for every file
- Saved tasks: store a transfer once, run it again with one click (or as a dry run)
- Filter the current folder by name (Ctrl+F)
- Mount remotes as drive letters (requires [WinFsp](https://winfsp.dev/)) and stream media to a player such as [VLC](https://www.videolan.org/)
- Folder size, folder tree, export of file lists, public links
- **Built-in rclone updater**: rclone is bundled, kept up to date from inside the app, and every download is checksum-verified
- Optional minimise to the system tray with notifications when transfers finish

![Remotes on the Home page](docs/home.png)

![Transfers in dark mode](docs/transfers-dark.png)

## Getting started

1. Download `RcloneBrowser-<version>-windows-x64-portable.zip` from the [Releases](../../releases) page.
2. Extract it to any folder and run `RcloneBrowser.exe`.
3. Windows may show "Windows protected your PC" because the app is not code-signed. Click **More info → Run anyway**.

Your existing remotes appear automatically. If you have none yet, click **New remote** on the Home page to set one up with rclone's setup assistant.

## Portable mode

The download is portable: `RcloneBrowser.ini` next to the exe switches portable mode on, and all settings and saved tasks are stored in that folder.

| What | Where |
|---|---|
| rclone | `rclone\rclone.exe` (updated from inside the app) |
| rclone configuration | `%APPDATA%\rclone\rclone.conf` by default. Copy it to `rclone\rclone.conf` to take your remotes with you. |
| Settings | `RcloneBrowser.ini` |
| Saved tasks | `tasks.bin` |

Paths in portable mode are relative to the app folder, so it keeps working after being moved or copied to another PC.

To use the app in non-portable mode instead, delete `RcloneBrowser.ini`. Settings are then stored in the registry under `HKEY_CURRENT_USER\Software\rclone-browser\rclone-browser` and tasks in `%LOCALAPPDATA%\rclone-browser\rclone-browser`.

## Custom service icons

Each remote is shown with a colour-coded tile for its storage type. To use your own picture for a type instead, create a folder `icons\remotes` next to `RcloneBrowser.exe` and put a `.png`, `.svg` or `.ico` file in it, named after the rclone type, for example `drive.png`, `onedrive.png`, `pcloud.png` or `s3.png`. Restart the app (or use **Reload remotes**) to apply.

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+F | Filter the current folder |
| F5 | Refresh |
| F7 | New folder |
| F2 | Rename |
| Del | Delete |
| Ctrl+U / Ctrl+D | Upload / Download |
| Alt+Up | Go to parent folder |
| Ctrl+T | Open the current remote in a new tab |
| Ctrl+W | Close the current tab |

## rclone updates

The status bar shows the rclone version in use. Once a day the app checks rclone.org (with GitHub as fallback) and offers to install a new version. Updates can also be checked manually via **More → Check for rclone updates…**, and automatic checks can be switched off there.

Each download is verified against rclone's published SHA-256 checksums before it is installed. Updating is safe while transfers or mounts are running; they finish using the previous version.

## Building from source

The app is built automatically by GitHub Actions (`.github/workflows/windows-portable.yml`) on every push. Pushing a tag such as `v2.0.0` also publishes a release.

To build locally:

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with "Desktop development with C++", [CMake](https://cmake.org/) and [Qt 6.8](https://www.qt.io/download-open-source) (MSVC 2022 64-bit, including Qt SVG).
2. From a "x64 Native Tools Command Prompt for VS 2022":

   ```
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
   cmake --build build
   C:\Qt\6.8.3\msvc2022_64\bin\windeployqt.exe --release build\build\RcloneBrowser.exe
   ```

## Credits

Based on Rclone Browser by [mmozeiko](https://github.com/mmozeiko/RcloneBrowser), [DinCahill](https://github.com/DinCahill/RcloneBrowser) and [kapitainsky](https://github.com/kapitainsky/RcloneBrowser). Interface icons from [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) by Microsoft (MIT). Licensed under the MIT license.
