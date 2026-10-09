#include "legacy_ini.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

bool MigrateLegacyIni(const QString &exeFilePath) {
  const QFileInfo exe(exeFilePath);
  const QString name = exe.completeBaseName();
  const QString legacy = exe.dir().filePath("RcloneBrowser.ini");
  const QString current = exe.dir().filePath(name + ".ini");

  if (name.isEmpty() || name == "RcloneBrowser" || !QFileInfo::exists(legacy) ||
      QFileInfo(current) == QFileInfo(legacy)) {
    return false;
  }

  if (QFileInfo::exists(current)) {
    // The ini shipped in the zip only has these two keys; anything more means
    // the app has been used and the file must not be overwritten.
    static const QStringList shipped = {"Settings/rclone", "Settings/checkRcloneUpdates"};
    {
      QSettings existing(current, QSettings::IniFormat);
      for (const QString &key : existing.allKeys()) {
        if (!shipped.contains(key)) {
          return false;
        }
      }
    }
    if (!QFile::remove(current)) {
      return false;
    }
  }
  return QFile::copy(legacy, current);
}
