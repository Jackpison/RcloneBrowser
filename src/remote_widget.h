#pragma once

#include "pch.h"
#include "ui_remote_widget.h"

class IconCache;

class RemoteWidget : public QWidget {
  Q_OBJECT

public:
  RemoteWidget(IconCache *icons, const QString &remote, bool isLocal,
               bool isGoogle, QWidget *parent = nullptr);
  ~RemoteWidget();

  // Shown under the remote name in the page header (e.g. "drive").
  void setRemoteType(const QString &type);

signals:
  void addTransfer(const QString &message, const QString &source,
                   const QString &remote, const QStringList &args);
  void addMount(const QString &remote, const QString &folder);
  void addStream(const QString &remote, const QString &stream);
  void closeRequested();

private:
  Ui::RemoteWidget ui;
  QLabel *mTypeLabel = nullptr;
  QLineEdit *mFilter = nullptr;
  QToolButton *mUp = nullptr;
  QWidget *mCrumbs = nullptr;
  QPersistentModelIndex mFilterFolder;

  void buildFluentUi(class ItemModel *model, const QString &remote);
  void updateBreadcrumbs(class ItemModel *model, const QString &remote);
  void applyFilter(class ItemModel *model);
};
