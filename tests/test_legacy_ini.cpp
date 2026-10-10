#include "legacy_ini.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class TestLegacyIni : public QObject {
  Q_OBJECT

  static void write(const QString &path, const QString &text) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
    f.write(text.toUtf8());
  }
  static QString read(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(f.readAll()) : QString();
  }

  const QString legacyText = "[Settings]\nrclone=C:/tools/rclone.exe\ntheme=2\n";
  const QString shippedText = "[Settings]\nrclone=rclone/rclone.exe\ncheckRcloneUpdates=true\n";

private slots:
  void legacyOnlyIsCopied() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    QVERIFY(MigrateLegacyIni(d.filePath("RcloneExplorer.exe")));
    QCOMPARE(read(d.filePath("RcloneExplorer.ini")), legacyText);
    QVERIFY(QFile::exists(d.filePath("RcloneBrowser.ini"))); // kept as a backup
  }
  void unusedShippedIniIsReplaced() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    write(d.filePath("RcloneExplorer.ini"), shippedText);
    QVERIFY(MigrateLegacyIni(d.filePath("RcloneExplorer.exe")));
    QCOMPARE(read(d.filePath("RcloneExplorer.ini")), legacyText);
  }
  void usedIniIsNeverOverwritten() {
    QTemporaryDir d;
    const QString used = shippedText + "theme=1\nMainWindow/geometry=x\n";
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    write(d.filePath("RcloneExplorer.ini"), used);
    QVERIFY(!MigrateLegacyIni(d.filePath("RcloneExplorer.exe")));
    QCOMPARE(read(d.filePath("RcloneExplorer.ini")), used);
  }
  void nothingToMigrate() {
    QTemporaryDir d;
    QVERIFY(!MigrateLegacyIni(d.filePath("RcloneExplorer.exe")));
    write(d.filePath("RcloneExplorer.ini"), shippedText);
    QVERIFY(!MigrateLegacyIni(d.filePath("RcloneExplorer.exe")));
    QCOMPARE(read(d.filePath("RcloneExplorer.ini")), shippedText);
  }
  void oldExeNameIsLeftAlone() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    QVERIFY(!MigrateLegacyIni(d.filePath("RcloneBrowser.exe")));
    QCOMPARE(read(d.filePath("RcloneBrowser.ini")), legacyText);
  }
  void renamedExeGetsItsOwnIni() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    QVERIFY(MigrateLegacyIni(d.filePath("MyCloud.exe")));
    QCOMPARE(read(d.filePath("MyCloud.ini")), legacyText);
  }
  void spaceInProgramName() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    QVERIFY(MigrateLegacyIni(d.filePath("Rclone Explorer.exe")));
    QCOMPARE(read(d.filePath("Rclone Explorer.ini")), legacyText);
  }
  void fromVersionFiveZero() {
    QTemporaryDir d;
    write(d.filePath("RcloneExplorer.ini"), legacyText);
    write(d.filePath("Rclone Explorer.ini"), shippedText); // unused, shipped in the new zip
    QVERIFY(MigrateLegacyIni(d.filePath("Rclone Explorer.exe")));
    QCOMPARE(read(d.filePath("Rclone Explorer.ini")), legacyText);
  }
  void newestLegacyWins() {
    QTemporaryDir d;
    const QString newer = "[Settings]\nrclone=D:/newer/rclone.exe\n";
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    write(d.filePath("RcloneExplorer.ini"), newer);
    QVERIFY(MigrateLegacyIni(d.filePath("Rclone Explorer.exe")));
    QCOMPARE(read(d.filePath("Rclone Explorer.ini")), newer);
  }
  void migratedFileIsReadableAsSettings() {
    QTemporaryDir d;
    write(d.filePath("RcloneBrowser.ini"), legacyText);
    MigrateLegacyIni(d.filePath("RcloneExplorer.exe"));
    QSettings s(d.filePath("RcloneExplorer.ini"), QSettings::IniFormat);
    QCOMPARE(s.value("Settings/rclone").toString(), QString("C:/tools/rclone.exe"));
    QCOMPARE(s.value("Settings/theme").toInt(), 2);
  }
};

QTEST_GUILESS_MAIN(TestLegacyIni)
#include "test_legacy_ini.moc"
