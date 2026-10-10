#pragma once

#include <QString>

// Settings migration across the renames of the Windows program.
//
// Portable mode keeps its settings in "<exe name>.ini" next to the exe:
//   4.x and older  RcloneBrowser.exe   -> RcloneBrowser.ini
//   5.0            RcloneExplorer.exe  -> RcloneExplorer.ini
//   5.1 and newer  Rclone Explorer.exe -> Rclone Explorer.ini
// If an older ini sits next to the exe (the newest one wins), copy it to the
// current name so portable mode and every setting carry over:
//  - no current ini yet                              -> copied
//  - current ini is the unused one shipped in the zip -> replaced
//  - current ini has been used                       -> never touched
// The old file is kept as a backup. Returns true if a file was written. Only
// touches files in the exe's folder.
bool MigrateLegacyIni(const QString &exeFilePath);
