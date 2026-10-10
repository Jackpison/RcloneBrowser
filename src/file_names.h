#pragma once

#include <QString>

// File extension without the dot, same result as QFileInfo(name).suffix():
// "report.tar.gz" -> "gz", ".bashrc" -> "bashrc", "name." and "name" -> "".
// Building a QFileInfo for every row of a large folder on every repaint and
// sort was a measurable cost; this is a plain string operation.
inline QString fileExtension(const QString &name) {
  const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
  const qsizetype slash = name.lastIndexOf(QLatin1Char('/'));
  if (dot < 0 || dot < slash) {
    return QString();
  }
  return name.mid(dot + 1);
}
