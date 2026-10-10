#include "legacy_ini.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

bool MigrateLegacyIni(const QString &exeFilePath) {
  const QFileInfo exe(exeFilePath);
  const QString name = exe.completeBaseName();
  if (name.isEmpty() || name == "RcloneBrowser") {
    return false; // a program with an old name keeps using its own ini
  }
  const QString current = exe.dir().filePath(name + ".ini");

  // newest first
  QString legacy;
  for (const char *old : {"RcloneExplorer.ini", "RcloneBrowser.ini"}) {
    const QString candidate = exe.dir().filePath(QString::fromLatin1(old));
    if (QFileInfo::exists(candidate) && QFileInfo(candidate) != QFileInfo(current)) {
      legacy = candidate;
      break;
    }
  }
  if (legacy.isEmpty()) {
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
