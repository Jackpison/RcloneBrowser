#include "rclone_output.h"

#include <QtTest>

using namespace RcloneOutput;

class TestRcloneOutput : public QObject {
  Q_OBJECT

private slots:
  // ---- stats: rclone 1.56+ (captured from rclone v1.75.1) ----
  void bytesModern() {
    auto r = parseStatsLine(
        "Transferred:   \t  120.215 MiB / 300.000 MiB, 40%, 60.231 MiB/s, ETA 2s");
    QCOMPARE(r.kind, StatsLine::Bytes);
    QCOMPARE(r.done, QString("120.215 MiB"));
    QCOMPARE(r.total, QString("300.000 MiB"));
    QCOMPARE(r.percent, QString("40%"));
    QCOMPARE(r.speed, QString("60.231 MiB/s"));
    QCOMPARE(r.eta, QString("2s"));
  }
  void bytesModernIdle() {
    auto r = parseStatsLine(
        "Transferred:   \t   60.215 MiB / 300.000 MiB, 20%, 0 B/s, ETA -");
    QCOMPARE(r.kind, StatsLine::Bytes);
    QCOMPARE(r.speed, QString("0 B/s"));
    QCOMPARE(r.eta, QString("-"));
  }
  void bytesModernEmpty() {
    auto r = parseStatsLine("Transferred:   \t          0 B / 0 B, -, 0 B/s, ETA -");
    QCOMPARE(r.kind, StatsLine::Bytes);
    QCOMPARE(r.percent, QString("-"));
  }
  void filesModern() {
    auto r = parseStatsLine("Transferred:            2 / 3, 67%");
    QCOMPARE(r.kind, StatsLine::Files);
    QCOMPARE(r.text, QString("2 / 3, 67%"));
  }
  void checksModernListed() {
    auto r = parseStatsLine("Checks:                 0 / 0, -, Listed 5");
    QCOMPARE(r.kind, StatsLine::Checks);
    QCOMPARE(r.text, QString("0 / 0, -, Listed 5"));
  }
  void errorsModern() {
    auto r = parseStatsLine("Errors:                 1 (retrying may help)");
    QCOMPARE(r.kind, StatsLine::Errors);
    QCOMPARE(r.text, QString("1 (retrying may help)"));
  }
  void elapsed() {
    auto r = parseStatsLine("Elapsed time:         2.0s");
    QCOMPARE(r.kind, StatsLine::Elapsed);
    QCOMPARE(r.text, QString("2.0s"));
  }
  void fileModern() {
    auto r = parseStatsLine(" * big.bin: 40% / 300 MiB, 60.251 MiB/s, 2s");
    QCOMPARE(r.kind, StatsLine::FileProgress);
    QCOMPARE(r.name, QString("big.bin"));
    QCOMPARE(r.filePercent, 40);
  }
  void fileModernStalled() {
    auto r = parseStatsLine(" * big.bin: 20% / 300 MiB, 0 B/s, -");
    QCOMPARE(r.kind, StatsLine::FileProgress);
    QCOMPARE(r.filePercent, 20);
  }
  void fileNameWithColonAndPath() {
    auto r = parseStatsLine(
        " * dir/we:ird name: 12% .txt: 75% / 3 KiB, 1 KiB/s, 1h2m3s");
    QCOMPARE(r.kind, StatsLine::FileProgress);
    QCOMPARE(r.name, QString("dir/we:ird name: 12% .txt"));
    QCOMPARE(r.filePercent, 75);
  }

  // ---- stats: rclone 1.43 - 1.55 ----
  void bytes143() {
    auto r = parseStatsLine(
        "Transferred:   \t   10.000M / 100.000 MBytes, 10%, 5.000 MBytes/s, ETA 18s");
    QCOMPARE(r.kind, StatsLine::Bytes);
    QCOMPARE(r.done, QString("10.000M"));
    QCOMPARE(r.total, QString("100.000 MBytes"));
    QCOMPARE(r.eta, QString("18s"));
  }
  void checks143() {
    auto r = parseStatsLine("Checks:                 1 / 2, 50%");
    QCOMPARE(r.kind, StatsLine::Checks);
    QCOMPARE(r.text, QString("1 / 2, 50%"));
  }
  void file139() {
    auto r = parseStatsLine(" *  big.bin: 40% /300M, 60M/s, 2s");
    QCOMPARE(r.kind, StatsLine::FileProgress);
    QCOMPARE(r.name, QString("big.bin"));
    QCOMPARE(r.filePercent, 40);
  }

  // ---- stats: rclone <= 1.42 / 1.38 ----
  void bytesOld() {
    auto r = parseStatsLine("Transferred:   1.234 MBytes (500 kBytes/s)");
    QCOMPARE(r.kind, StatsLine::Bytes);
    QCOMPARE(r.done, QString("1.234 MBytes"));
    QCOMPARE(r.speed, QString("500 kBytes/s"));
  }
  void filesOld() {
    QCOMPARE(parseStatsLine("Transferred:   3").kind, StatsLine::Files);
    QCOMPARE(parseStatsLine("Checks:   3").kind, StatsLine::Checks);
    QCOMPARE(parseStatsLine("Errors:   0").text, QString("0"));
  }
  void fileOld() {
    auto r = parseStatsLine("* big.bin: 40% done, 60 MBytes/s, ETA: 2s)");
    QCOMPARE(r.kind, StatsLine::FileProgress);
    QCOMPARE(r.filePercent, 40);
  }

  // ---- things that must NOT be treated as stats ----
  void nonStats() {
    QCOMPARE(parseStatsLine("").kind, StatsLine::None);
    QCOMPARE(parseStatsLine("Transferring:").kind, StatsLine::None);
    QCOMPARE(parseStatsLine("2026/10/08 15:13:55 INFO  : ").kind, StatsLine::None);
    QCOMPARE(parseStatsLine("2026/10/08 15:13:54 INFO  : big.bin: Copied (new)").kind,
             StatsLine::None);
  }

  // ---- listings (captured from rclone v1.75.1) ----
  void lsd() {
    auto e = parseLsdLine("        4096 2026-10-08 15:13:54        -1 a dir\n");
    QVERIFY(e.valid);
    QVERIFY(e.isDir);
    QCOMPARE(e.name, QString("a dir"));
    QCOMPARE(e.modified, QString("2026-10-08 15:13:54"));
  }
  void lsdBucket() {
    auto e = parseLsdLine("          -1 2000-01-01 00:00:00        -1 bucket");
    QVERIFY(e.valid);
    QCOMPARE(e.name, QString("bucket"));
  }
  void lslSpaces() {
    // Leading and trailing spaces in names must survive (old code trimmed them
    // and then failed to open/download such files).
    auto e = parseLslLine("        0 2026-10-08 15:13:54.291248807  lead \r\n");
    QVERIFY(e.valid);
    QCOMPARE(e.name, QString(" lead "));
    QCOMPARE(e.size, quint64(0));
  }
  void lslLarge() {
    auto e = parseLslLine("314572800 2026-10-08 15:13:54.287248807 big.bin");
    QVERIFY(e.valid);
    QCOMPARE(e.size, quint64(314572800));
    QCOMPARE(e.modified, QString("2026-10-08 15:13:54"));
  }
  void lslNoFraction() {
    auto e = parseLslLine("       12 2026-10-08 15:13:54 x.txt");
    QVERIFY(e.valid);
    QCOMPARE(e.name, QString("x.txt"));
  }
  void lslGarbage() {
    QVERIFY(!parseLslLine("2026/10/08 NOTICE: something").valid);
    QVERIFY(!parseLsdLine("").valid);
  }

  // ---- line buffering for chunked process output ----
  void lineBuffer() {
    LineBuffer b;
    QCOMPARE(b.append("one\ntw"), QStringList{"one"});
    QCOMPARE(b.append("o\r\nthr"), QStringList{"two"});
    QCOMPARE(b.append("ee"), QStringList{});
    QCOMPARE(b.flush(), QString("three"));
    QCOMPARE(b.flush(), QString());
  }
};

QTEST_GUILESS_MAIN(TestRcloneOutput)
#include "test_rclone_output.moc"
