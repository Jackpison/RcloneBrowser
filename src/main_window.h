#pragma once

#include "icon_cache.h"
#include "job_options.h"
#include "pch.h"

class RcloneUpdater;
#include "ui_main_window.h"

class JobWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow();

private slots:
  void rcloneGetVersion();
  void rcloneConfig();
  void rcloneListRemotes();
  void listTasks();

  void addTransfer(const QString &message, const QString &source,
                   const QString &dest, const QStringList &args);
  void addMount(const QString &remote, const QString &folder);
  void addStream(const QString &remote, const QString &stream);

private:
  Ui::MainWindow ui;

  QSystemTrayIcon mSystemTray;
  JobWidget *mLastFinished = nullptr;

  bool mAlwaysShowInTray;
  bool mCloseToTray;
  bool mNotifyFinishedTransfers;

  QLabel *mStatusMessage;

  IconCache mIcons;

  bool mFirstTime = true;

  // rclone download / update
  RcloneUpdater *mUpdater = nullptr;
  QLabel *mRcloneStatus = nullptr;
  QAction *mAutoCheckAction = nullptr;
  QString mRcloneVersion;      // installed, without leading "v"
  QString mLatestRclone;       // e.g. "v1.75.1", empty if unknown
  void setupRcloneUpdater();
  void updateRcloneStatus();
  void maybeAutoCheckRclone();
  void checkRcloneUpdate(bool interactive);
  void offerRcloneDownload();
  void installRclone(const QString &version);
  int mJobCount = 0;

  bool canClose();
  void closeEvent(QCloseEvent *ev) override;
  bool getConfigPassword(QProcess *p);

  void addEmptyJobsMessage();

  void runItem(JobOptionsListWidgetItem *item, bool dryrun = false);
  void editSelectedTask();
  QIcon mUploadIcon;
  QIcon mDownloadIcon;

  // Windows 11 shell (main_window_shell.cpp)
  QListWidget *mNav = nullptr;
  QListWidgetItem *mNavTransfers = nullptr;
  QListWidgetItem *mNavRemotesHeader = nullptr;
  QLabel *mHomeSubtitle = nullptr;
  QLabel *mHomeEmpty = nullptr;
  bool mSyncingNav = false;
  void buildShell();
  int remoteTab(const QString &name) const;
  void openRemote(const QString &name, const QString &type);
  void closeRemote(const QString &name);
  void syncNavToCurrentPage();
  void rebuildNavRemotes();
  void setJobsTabText(const QString &text);
};
