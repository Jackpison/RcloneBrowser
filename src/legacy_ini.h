#pragma once

#include <QString>

// Settings migration for the rename Rclone Browser -> Rclone Explorer.
//
// Windows portable mode keeps its settings in "<exe name>.ini" next to the exe.
// Releases up to 4.x used RcloneBrowser.ini. If such a file sits next to the
// new exe, copy it to the new name so portable mode and every setting carry
// over:
//  - no new ini yet                      -> legacy file is copied
//  - new ini is the unused one that ships in the zip -> replaced by the legacy file
//  - new ini has been used               -> never touched
// Returns true if a file was written. Only touches files in the exe's folder.
bool MigrateLegacyIni(const QString &exeFilePath);
