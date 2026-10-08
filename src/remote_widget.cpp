#include "remote_widget.h"
#include "theme.h"
#include "rclone_output.h"
#include "export_dialog.h"
#include "icon_cache.h"
#include "item_model.h"
#include "list_of_job_options.h"
#include "progress_dialog.h"
#include "transfer_dialog.h"
#include "utils.h"

RemoteWidget::RemoteWidget(IconCache *iconCache, const QString &remote,
                           bool isLocal, bool isGoogle, QWidget *parent)
    : QWidget(parent) {
  ui.setupUi(this);

QString root = isLocal ? "/" : QString();

#ifndef Q_OS_WIN
  isLocal = false;
#endif

  auto settings = GetSettings();
  QString rcloneVersion = settings->value("Settings/rcloneVersion").toString();
  settings->setValue("Settings/driveShared", Qt::Unchecked);
  ui.tree->setAlternatingRowColors(
      settings->value("Settings/rowColors", false).toBool());
  ui.checkBoxShared->setChecked(false);
  ui.checkBoxShared->setDisabled(!isGoogle);
  // hide checkBoxShared for non Google remotes
  if (!isGoogle) {
    ui.checkBoxShared->hide();
  }

  ui.buttonRefresh->setDefaultAction(ui.refresh);
  ui.buttonMkdir->setDefaultAction(ui.mkdir);
  ui.buttonRename->setDefaultAction(ui.rename);
  ui.buttonMove->setDefaultAction(ui.move);
  ui.buttonPurge->setDefaultAction(ui.purge);
  ui.buttonMount->setDefaultAction(ui.mount);
  ui.buttonStream->setDefaultAction(ui.stream);
  ui.buttonUpload->setDefaultAction(ui.upload);
  ui.buttonDownload->setDefaultAction(ui.download);
  ui.buttonTree->setDefaultAction(ui.getTree);
  ui.buttonLink->setDefaultAction(ui.link);
  ui.buttonSize->setDefaultAction(ui.getSize);
  ui.buttonExport->setDefaultAction(ui.export_);

  ui.tree->sortByColumn(0, Qt::AscendingOrder);
  ui.tree->header()->setSectionsMovable(false);

  ItemModel *model = new ItemModel(iconCache, remote, this);
  ui.tree->setModel(model);
  QTimer::singleShot(0, ui.tree, SLOT(setFocus()));
  buildFluentUi(model, remote);

  QObject::connect(model, &QAbstractItemModel::layoutChanged, this, [=]() {
    ui.tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui.tree->resizeColumnToContents(1);
    ui.tree->resizeColumnToContents(2);
  });

  QObject::connect(
      ui.tree->selectionModel(), &QItemSelectionModel::selectionChanged, this,
      [=](const QItemSelection &selection) {
        // only this widget's own actions, not those inside child widgets
        // (e.g. the filter box's clear button)
        for (auto child :
             findChildren<QAction *>(QString(), Qt::FindDirectChildrenOnly)) {
          child->setDisabled(selection.isEmpty());
        }

        if (selection.isEmpty()) {
          ui.path->clear();
          return;
        }

        QModelIndex index = selection.indexes().front();

        bool topLevel = model->isTopLevel(index);
        bool isFolder = model->isFolder(index);

        QDir path;
        if (model->isLoading(index)) {
          ui.refresh->setDisabled(true);
          ui.move->setDisabled(true);
          ui.rename->setDisabled(true);
          ui.purge->setDisabled(true);
          ui.mount->setDisabled(true);
          ui.stream->setDisabled(true);
          ui.upload->setDisabled(true);
          ui.download->setDisabled(true);
          ui.checkBoxShared->setDisabled(true);
          path = model->path(model->parent(index));
        } else {
          ui.refresh->setDisabled(false);
          bool driveShared = ui.checkBoxShared->checkState();
          ui.mkdir->setDisabled(driveShared);
          ui.rename->setDisabled(topLevel || driveShared);
          ui.move->setDisabled(topLevel || driveShared);
          ui.purge->setDisabled(topLevel || driveShared);
          ui.upload->setDisabled(driveShared);

#if defined(Q_OS_WIN32)
          // check if required version
          unsigned int result =
              compareVersion(rcloneVersion.toStdString(), "1.50");
          if (result == 2) {
            ui.mount->setDisabled(true);
          } else {
            ui.mount->setDisabled(!isFolder);
          };
#else
// mount is not supported by rclone on these systems
#if defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
          ui.mount->setDisabled(true);
#else
          ui.mount->setDisabled(!isFolder);
#endif
#endif

          ui.stream->setDisabled(isFolder);
          ui.checkBoxShared->setDisabled(!isGoogle);
          path = model->path(index);
        }

        ui.getSize->setDisabled(!isFolder);
        ui.getTree->setDisabled(!isFolder);
        ui.export_->setDisabled(!isFolder);
        ui.path->setText(isLocal ? QDir::toNativeSeparators(path.path())
                                 : path.path());
      });

  QObject::connect(ui.refresh, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    model->refresh(index);
  });

  QObject::connect(ui.mkdir, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    if (!model->isFolder(index)) {
      index = index.parent();
    }
    QDir path = model->path(index);
    QString pathMsg =
        isLocal ? QDir::toNativeSeparators(path.path()) : path.path();
    QString name = QInputDialog::getText(
        this, "New Folder", QString("Create folder in %1").arg(pathMsg));
    if (!name.isEmpty()) {
      QString folder = path.filePath(name);
      QString folderMsg = isLocal ? QDir::toNativeSeparators(folder) : folder;

      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(QStringList() << "mkdir" << GetRcloneConf()
                                         << GetDriveSharedWithMe()
                                         << GetDefaultRcloneOptionsList()
                                         << remote + ":" + folder);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("New Folder", "Creating...", folderMsg, &process,
                              this);
      if (progress.exec() == QDialog::Accepted) {
        model->refresh(index);
      }
    }
  });

  QObject::connect(ui.rename, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QString name = model->data(index, Qt::DisplayRole).toString();
    name = QInputDialog::getText(this, "Rename",
                                 QString("New name for %1").arg(pathMsg),
                                 QLineEdit::Normal, name);
    if (!name.isEmpty()) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(
          QStringList() << "moveto" << GetRcloneConf() << GetDriveSharedWithMe()
                        << GetDefaultRcloneOptionsList() << remote + ":" + path
                        << remote + ":" +
                               model->path(index.parent()).filePath(name));
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Rename", "Renaming...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        model->rename(index, name);
      }
    }
  });

  QObject::connect(ui.move, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QString name = model->path(index.parent()).path() + "/";
    name = QInputDialog::getText(this, "Move",
                                 QString("New location for %1").arg(pathMsg),
                                 QLineEdit::Normal, name);
    if (!name.isEmpty()) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(
          QStringList() << "move" << GetRcloneConf() << GetDriveSharedWithMe()
                        << GetDefaultRcloneOptionsList() << remote + ":" + path
                        << remote + ":" + name);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Move", "Moving...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        model->refresh(index);
      }
    }
  });

  QObject::connect(ui.purge, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    int button = QMessageBox::question(
        this, "Delete",
        QString("Are you sure you want to delete %1 ?").arg(pathMsg),
        QMessageBox::Yes | QMessageBox::No);
    if (button == QMessageBox::Yes) {
      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(QStringList()
                           << (model->isFolder(index) ? "purge" : "delete")
                           << GetRcloneConf() << GetDriveSharedWithMe()
                           << GetDefaultRcloneOptionsList()
                           << remote + ":" + path);
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Delete", "Deleting...", pathMsg, &process, this);
      if (progress.exec() == QDialog::Accepted) {
        QModelIndex parent = index.parent();
        QModelIndex next = parent.model()->index(index.row() + 1, 0);
        ui.tree->selectionModel()->select(next.isValid() ? next : parent,
                                          QItemSelectionModel::SelectCurrent);
        model->removeRow(index.row(), parent);
      }
    }
  });

  QObject::connect(ui.mount, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

#if defined(Q_OS_WIN32)
    QString folder =
        QInputDialog::getText(this, "Mount",
                              QString("(Make sure you have WinFsp-FUSE "
                                      "installed)\n\nDrive to mount %1 to")
                                  .arg(remote),
                              QLineEdit::Normal, "Z:");
#else
        QString folder = QFileDialog::getExistingDirectory(this, QString("Mount %1").arg(remote));
#endif

    if (!folder.isEmpty()) {
      emit addMount(remote + ":" + path, folder);
    }
  });

  QObject::connect(ui.stream, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QString path = model->path(index).path();

    bool streamConfirmed =
        settings->value("Settings/streamConfirmed", false).toBool();
    QString stream = settings->value("Settings/stream", "mpv -").toString();
    if (!streamConfirmed) {
      QString result = QInputDialog::getText(
          this, "Stream",
          "Enter stream command (file will be passed in STDIN):",
          QLineEdit::Normal, stream);
      if (result.isEmpty()) {
        return;
      }

      stream = result;

      settings->setValue("Settings/stream", stream);
      settings->setValue("Settings/streamConfirmed", true);
    }

    emit addStream(remote + ":" + path, stream);
  });

  QObject::connect(ui.checkBoxShared, &QCheckBox::toggled, ui.shared,
                   &QAction::toggled);

  QObject::connect(ui.shared, &QAction::toggled, this, [=](const bool checked) {
    auto settings = GetSettings();
    settings->setValue("Settings/driveShared", checked);
    ui.checkBoxShared->setChecked(checked);

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QModelIndex top = index;
    while (!model->isTopLevel(top)) {
      top = top.parent();
    }
    ui.tree->selectionModel()->clear();
    ui.tree->selectionModel()->select(top, QItemSelectionModel::Select |
                                               QItemSelectionModel::Rows);
    model->refresh(top);
  });

  QObject::connect(ui.link, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "link" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Fetch Public Link", "Fetching link for...",
                            pathMsg, &process, this, false, true);
    progress.expand();
    progress.allowToClose();
    progress.exec();
  });

  QObject::connect(ui.upload, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    if (!model->isFolder(index)) {
      index = index.parent();
    }
    QDir path = model->path(index);

    TransferDialog t(false, false, remote, path, true, this);
    if (t.exec() == QDialog::Accepted) {
      QString src = t.getSource();
      QString dst = t.getDest();

      QStringList args = t.getOptions();
      emit addTransfer(QString("%1 from %2").arg(t.getMode()).arg(src), src,
                       dst, args);
    }
  });

  QObject::connect(ui.download, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QDir path = model->path(index);

    TransferDialog t(true, false, remote, path, model->isFolder(index), this);
    if (t.exec() == QDialog::Accepted) {
      QString src = t.getSource();
      QString dst = t.getDest();

      QStringList args = t.getOptions();
      emit addTransfer(QString("%1 %2").arg(t.getMode()).arg(src), src, dst,
                       args);
    }
  });

  QObject::connect(ui.getTree, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));
    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "tree"
                      << "-d" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Show directories tree", "Processing...", pathMsg,
                            &process, this, false);
    progress.expand();
    progress.allowToClose();
    progress.resize(1000, 600);
    progress.exec();
  });

  QObject::connect(ui.getSize, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));
    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();

    QString path = model->path(index).path();
    QString pathMsg = isLocal ? QDir::toNativeSeparators(path) : path;

    QProcess process;
    UseRclonePassword(&process);
    process.setProgram(GetRclone());
    process.setArguments(
        QStringList() << "size" << GetRcloneConf() << GetDriveSharedWithMe()
                      << GetDefaultRcloneOptionsList() << remote + ":" + path);
    process.setProcessChannelMode(QProcess::MergedChannels);
    ProgressDialog progress("Get Size", "Calculating...", pathMsg, &process,
                            this, false);
    progress.expand();
    progress.allowToClose();
    progress.exec();
  });

  QObject::connect(ui.export_, &QAction::triggered, this, [=]() {
    auto settings = GetSettings();
    bool driveShared = ui.checkBoxShared->checkState();
    (driveShared ? settings->setValue("Settings/driveShared", Qt::Checked)
                 : settings->setValue("Settings/driveShared", Qt::Unchecked));

    QModelIndex index = ui.tree->selectionModel()->selectedRows().front();
    QDir path = model->path(index);
    ExportDialog e(remote, path, this);
    if (e.exec() == QDialog::Accepted) {
      QString dst = e.getDestination();
      bool txt = e.onlyFilenames();

      QFile *file = new QFile(dst);
      if (!file->open(QFile::WriteOnly)) {
        QMessageBox::warning(
            this, "Error",
            QString("Cannot open file '%1' for writing!").arg(dst));
        delete file;
        return;
      }

      QProcess process;
      UseRclonePassword(&process);
      process.setProgram(GetRclone());
      process.setArguments(QStringList()
                           << GetRcloneConf() << GetDriveSharedWithMe()
                           << GetDefaultRcloneOptionsList() << e.getOptions());
      process.setProcessChannelMode(QProcess::MergedChannels);

      ProgressDialog progress("Export", "Exporting...", dst, &process, this);
      file->setParent(&progress);

      // Output arrives in arbitrary chunks; buffer partial lines so an entry
      // split across two reads is not dropped from the export.
      auto buffer = std::make_shared<RcloneOutput::LineBuffer>();
      auto out = std::make_shared<QTextStream>(file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
      out->setCodec("UTF-8");
#endif

      auto writeLine = [=](const QString &line) {
        const auto entry = RcloneOutput::parseLslLine(line);
        if (!entry.valid) {
          return;
        }
        if (txt) {
          *out << entry.name << '\n';
        } else {
          QString name = entry.name;
          if (name.contains(' ') || name.contains(',') || name.contains('"')) {
            name = '"' + name.replace("\"", "\"\"") + '"';
          }
          *out << name << ',' << '"' << entry.modified << '"' << ','
               << entry.size << '\n';
        }
      };

      QObject::connect(&progress, &ProgressDialog::outputAvailable, this,
                       [=](const QString &output) {
                         for (const auto &line : buffer->append(output)) {
                           writeLine(line);
                         }
                       });

      progress.exec();
      writeLine(buffer->flush());
      out->flush();
      // QWidget deletes children (the file) before its connections release
      // the stream, so detach now to avoid a flush into a deleted device.
      out->setDevice(nullptr);
    }
  });

  QObject::connect(
      model, &ItemModel::drop, this,
      [=](const QDir &path, const QModelIndex &parent) {
        auto settings = GetSettings();
        bool driveShared = ui.checkBoxShared->checkState();
        (driveShared
             ? settings->setValue("Settings/driveShared", Qt::Checked)
             : settings->setValue("Settings/driveShared", Qt::Unchecked));

        qApp->setActiveWindow(this);
        QDir destPath = model->path(parent);
        QString dest = QFileInfo(path.path()).isDir()
                           ? destPath.filePath(path.dirName())
                           : destPath.path();

        TransferDialog t(false, true, remote, dest, true, this);
        t.setSource(path.path());

        if (t.exec() == QDialog::Accepted) {
          QString src = t.getSource();
          QString dst = t.getDest();

          QStringList args = t.getOptions();
          emit addTransfer(QString("%1 from %2").arg(t.getMode()).arg(src), src,
                           dst, args);
        }
      });

  QObject::connect(
      ui.tree, &QWidget::customContextMenuRequested, this,
      [=](const QPoint &pos) {
        auto settings = GetSettings();
        bool driveShared = ui.checkBoxShared->checkState();
        (driveShared
             ? settings->setValue("Settings/driveShared", Qt::Checked)
             : settings->setValue("Settings/driveShared", Qt::Unchecked));

        QMenu menu;
        menu.addAction(ui.refresh);
        menu.addAction(ui.getSize);
        menu.addAction(ui.getTree);
        menu.addAction(ui.export_);
        menu.addSeparator();
        menu.addAction(ui.mkdir);
        menu.addAction(ui.rename);
        menu.addAction(ui.move);
        menu.addAction(ui.purge);
        menu.addSeparator();
        menu.addAction(ui.mount);
        menu.addAction(ui.stream);
        menu.addAction(ui.upload);
        menu.addAction(ui.download);
        menu.addAction(ui.link);
        menu.exec(ui.tree->viewport()->mapToGlobal(pos));
      });

  if (isLocal) {
    QHash<QString, QPersistentModelIndex> drives;

    // QDir::drives is fast
    for (const auto &drive : QDir::drives()) {
      QString path = drive.path();
      QModelIndex index = model->addRoot(QDir::toNativeSeparators(path), path);
      drives.insert(path, index);
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)) && !(defined Q_OS_WIN)
    QThread *thread = new QThread(this);
    thread->start();

    QObject *worker = new QObject();
    worker->moveToThread(thread);

    QTimer::singleShot(0, worker, [=]() {
      QStorageInfo info;
      info.refresh();

      // QStorageInfo::mountedVolumes is slow :(
      for (const auto &volume : info.mountedVolumes()) {
        QString name = volume.name();
        if (!name.isEmpty()) {
          QString path = volume.rootPath();
          QString item =
              QString("%1 (%2)").arg(QDir::toNativeSeparators(path)).arg(name);
          QTimer::singleShot(0, this,
                             [=]() { model->rename(drives[path], item); });
        }
      }

      thread->quit();
      thread->deleteLater();
      worker->deleteLater();
    });
#endif

    ui.tree->selectionModel()->selectionChanged(QItemSelection(),
                                                QItemSelection());
  } else {
    QModelIndex index = model->addRoot(remote, root);
    ui.tree->selectionModel()->select(
        index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
    ui.tree->expand(index);
  }

  QShortcut *close = new QShortcut(QKeySequence::Close, this);
  QObject::connect(close, &QShortcut::activated, this,
                   &RemoteWidget::closeRequested);
}

RemoteWidget::~RemoteWidget() {}

// ---------------------------------------------------------------------------
// Windows 11 style page: header, command bar, breadcrumb bar, filter
// ---------------------------------------------------------------------------

void RemoteWidget::setRemoteType(const QString &type) {
  if (mTypeLabel) {
    mTypeLabel->setText(type);
  }
}

void RemoteWidget::buildFluentUi(ItemModel *model, const QString &remote) {
  auto *v = qobject_cast<QVBoxLayout *>(ui.layout->layout());
  v->setContentsMargins(24, 20, 24, 8);
  v->setSpacing(8);
  layout()->setContentsMargins(0, 0, 0, 0);

  // ---- header: name, type, filter box, close
  auto *header = new QWidget;
  auto *hh = new QHBoxLayout(header);
  hh->setContentsMargins(4, 0, 0, 4);
  hh->setSpacing(10);
  auto *titles = new QVBoxLayout;
  titles->setSpacing(0);
  auto *title = new QLabel(remote);
  title->setObjectName("PageTitle");
  titles->addWidget(title);
  mTypeLabel = new QLabel;
  mTypeLabel->setProperty("secondary", true);
  titles->addWidget(mTypeLabel);
  hh->addLayout(titles, 1);

  mFilter = new QLineEdit;
  mFilter->setPlaceholderText(tr("Filter this folder"));
  mFilter->setClearButtonEnabled(true);
  mFilter->addAction(Theme::icon("search"), QLineEdit::LeadingPosition);
  mFilter->setFixedWidth(260);
  mFilter->setToolTip(tr("Show only items in the current folder whose name "
                         "contains this text (Ctrl+F)"));
  hh->addWidget(mFilter, 0, Qt::AlignVCenter);

  auto *close = new QToolButton;
  close->setIcon(Theme::icon("close"));
  close->setToolTip(tr("Close %1 (Ctrl+W)").arg(remote));
  QObject::connect(close, &QToolButton::clicked, this,
                   &RemoteWidget::closeRequested);
  hh->addWidget(close, 0, Qt::AlignVCenter);
  v->insertWidget(0, header);

  // ---- command bar
  struct A { QAction *a; const char *icon; QString text; };
  const QList<A> actions = {
      {ui.mkdir, "new_folder", tr("New folder")},
      {ui.upload, "upload", tr("Upload")},
      {ui.download, "download", tr("Download")},
      {ui.rename, "rename", tr("Rename")},
      {ui.move, "move", tr("Move")},
      {ui.purge, "delete", tr("Delete")},
      {ui.mount, "mount", tr("Mount")},
      {ui.stream, "stream", tr("Stream")},
      {ui.refresh, "refresh", tr("Refresh")},
      {ui.getSize, "size", tr("Folder size")},
      {ui.getTree, "tree", tr("Folder tree")},
      {ui.export_, "export", tr("Export file list")},
      {ui.link, "link", tr("Copy public link")},
  };
  for (const A &x : actions) {
    x.a->setIcon(Theme::icon(x.icon));
    x.a->setText(x.text);
    x.a->setIconText(x.text);
    if (!x.a->shortcut().isEmpty()) {
      x.a->setToolTip(QString("%1 (%2)").arg(x.text, x.a->shortcut().toString(
                                                    QKeySequence::NativeText)));
    } else {
      x.a->setToolTip(x.text);
    }
  }
  ui.upload->setShortcut(QKeySequence(tr("Ctrl+U")));
  ui.download->setShortcut(QKeySequence(tr("Ctrl+D")));

  auto *bar = qobject_cast<QHBoxLayout *>(ui.buttons->layout());
  // take everything out and lay it out again in command-bar order
  while (QLayoutItem *it = bar->takeAt(0)) {
    delete it;
  }
  bar->setContentsMargins(0, 0, 0, 0);
  bar->setSpacing(2);

  auto separator = [&]() {
    auto *f = new QFrame;
    f->setFrameShape(QFrame::NoFrame);
    f->setFixedSize(1, 20);
    f->setAutoFillBackground(true);
    f->setStyleSheet("background: palette(mid);");
    bar->addSpacing(4);
    bar->addWidget(f, 0, Qt::AlignVCenter);
    bar->addSpacing(4);
  };
  auto primary = [&](QToolButton *b) {
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setIconSize(QSize(18, 18));
    bar->addWidget(b);
  };
  primary(ui.buttonMkdir);
  primary(ui.buttonUpload);
  primary(ui.buttonDownload);
  separator();
  primary(ui.buttonRename);
  primary(ui.buttonMove);
  primary(ui.buttonPurge);
  separator();
  primary(ui.buttonMount);
  bar->addStretch(1);
  bar->addWidget(ui.checkBoxShared);
  ui.checkBoxShared->setText(tr("Shared with me"));

  ui.buttonRefresh->setToolButtonStyle(Qt::ToolButtonIconOnly);
  ui.buttonRefresh->setIconSize(QSize(18, 18));
  bar->addWidget(ui.buttonRefresh);

  auto *more = new QToolButton;
  more->setIcon(Theme::icon("more"));
  more->setIconSize(QSize(18, 18));
  more->setToolTip(tr("More options"));
  more->setPopupMode(QToolButton::InstantPopup);
  auto *moreMenu = new QMenu(more);
  moreMenu->addAction(ui.stream);
  moreMenu->addSeparator();
  moreMenu->addAction(ui.getSize);
  moreMenu->addAction(ui.getTree);
  moreMenu->addAction(ui.export_);
  moreMenu->addSeparator();
  moreMenu->addAction(ui.link);
  more->setMenu(moreMenu);
  bar->addWidget(more);

  for (QToolButton *b : {ui.buttonStream, ui.buttonSize, ui.buttonTree,
                         ui.buttonExport, ui.buttonLink}) {
    b->hide();
  }

  // ---- breadcrumb address bar (replaces the plain path text box)
  auto *address = new QWidget;
  address->setProperty("card", true);
  address->setAttribute(Qt::WA_StyledBackground);
  auto *ah = new QHBoxLayout(address);
  ah->setContentsMargins(4, 2, 8, 2);
  ah->setSpacing(2);

  mUp = new QToolButton;
  mUp->setIcon(Theme::icon("up"));
  mUp->setToolTip(tr("Up to parent folder (Alt+Up)"));
  mUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
  ah->addWidget(mUp);

  mCrumbs = new QWidget;
  auto *ch = new QHBoxLayout(mCrumbs);
  ch->setContentsMargins(0, 0, 0, 0);
  ch->setSpacing(0);
  ah->addWidget(mCrumbs, 1);

  auto *copyPath = new QToolButton;
  copyPath->setIcon(Theme::icon("copy"));
  copyPath->setToolTip(tr("Copy path"));
  QObject::connect(copyPath, &QToolButton::clicked, this, [this]() {
    QGuiApplication::clipboard()->setText(ui.path->text());
  });
  ah->addWidget(copyPath);

  ui.path->hide(); // still updated by the existing code; used for "Copy path"
  v->insertWidget(v->indexOf(ui.path), address);

  ui.tree->setAnimated(true);
  ui.tree->setIconSize(QSize(20, 20));
  ui.tree->setFrameShape(QFrame::NoFrame);
  ui.tree->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  ui.tree->header()->setHighlightSections(false);

  // ---- behaviour
  QObject::connect(mUp, &QToolButton::clicked, this, [this]() {
    QModelIndex cur = ui.tree->currentIndex();
    if (!cur.isValid()) {
      return;
    }
    QModelIndex parent = cur.parent();
    if (parent.isValid()) {
      ui.tree->setCurrentIndex(parent);
      ui.tree->scrollTo(parent);
    }
  });

  QObject::connect(ui.tree->selectionModel(),
                   &QItemSelectionModel::currentChanged, this,
                   [this, model, remote]() {
                     updateBreadcrumbs(model, remote);
                     applyFilter(model);
                   });
  QObject::connect(mFilter, &QLineEdit::textChanged, this,
                   [this, model]() { applyFilter(model); });
  QObject::connect(model, &QAbstractItemModel::rowsInserted, this,
                   [this, model](const QModelIndex &parent) {
                     if (!mFilter->text().isEmpty() && parent == mFilterFolder) {
                       applyFilter(model);
                     }
                   });
  QObject::connect(model, &QAbstractItemModel::layoutChanged, this,
                   [this, model]() {
                     if (!mFilter->text().isEmpty()) {
                       applyFilter(model);
                     }
                   });

  auto *find = new QShortcut(QKeySequence::Find, this);
  QObject::connect(find, &QShortcut::activated, this, [this]() {
    mFilter->setFocus();
    mFilter->selectAll();
  });
  auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), mFilter);
  escape->setContext(Qt::WidgetShortcut);
  QObject::connect(escape, &QShortcut::activated, this, [this]() {
    mFilter->clear();
    ui.tree->setFocus();
  });

  updateBreadcrumbs(model, remote);
}

void RemoteWidget::updateBreadcrumbs(ItemModel *model, const QString &remote) {
  QLayout *l = mCrumbs->layout();
  while (QLayoutItem *it = l->takeAt(0)) {
    delete it->widget();
    delete it;
  }

  // folder chain of the current item (a file shows its containing folder)
  QModelIndex cur = ui.tree->currentIndex().siblingAtColumn(0);
  if (cur.isValid() && !model->isFolder(cur)) {
    cur = cur.parent();
  }
  QList<QPersistentModelIndex> chain;
  for (QModelIndex i = cur; i.isValid(); i = i.parent()) {
    chain.prepend(i);
  }
  mUp->setEnabled(ui.tree->currentIndex().parent().isValid());

  auto *h = qobject_cast<QHBoxLayout *>(l);
  if (chain.isEmpty()) {
    auto *b = new QToolButton;
    b->setText(remote);
    h->addWidget(b);
  }
  for (int i = 0; i < chain.size(); ++i) {
    const QPersistentModelIndex idx = chain[i];
    QString text = model->data(idx, Qt::DisplayRole).toString();
    if (i == 0 && (text == "/" || text.isEmpty())) {
      text = remote;
    }
    if (i > 0) {
      auto *sep = new QLabel;
      sep->setPixmap(Theme::icon("chevron").pixmap(12, 12));
      h->addWidget(sep);
    }
    auto *b = new QToolButton;
    b->setText(text);
    b->setCursor(Qt::PointingHandCursor);
    if (i == chain.size() - 1) {
      QFont f = b->font();
      f.setWeight(QFont::DemiBold);
      b->setFont(f);
    }
    QObject::connect(b, &QToolButton::clicked, this, [this, idx]() {
      if (idx.isValid()) {
        ui.tree->setCurrentIndex(idx);
        ui.tree->expand(idx);
        ui.tree->scrollTo(idx);
        ui.tree->setFocus();
      }
    });
    h->addWidget(b);
  }
  h->addStretch(1);
}

void RemoteWidget::applyFilter(ItemModel *model) {
  // the folder being filtered: the current folder, or a file's parent
  QModelIndex folder = ui.tree->currentIndex().siblingAtColumn(0);
  if (folder.isValid() && !model->isFolder(folder)) {
    folder = folder.parent();
  }

  // un-hide everything in the previously filtered folder
  if (mFilterFolder.isValid() && mFilterFolder != folder) {
    for (int r = 0; r < model->rowCount(mFilterFolder); ++r) {
      ui.tree->setRowHidden(r, mFilterFolder, false);
    }
  }
  mFilterFolder = folder;
  if (!folder.isValid()) {
    return;
  }

  const QString text = mFilter->text().trimmed();
  const QModelIndex current = ui.tree->currentIndex();
  for (int r = 0; r < model->rowCount(folder); ++r) {
    const QModelIndex child = model->index(r, 0, folder);
    const QString name = model->data(child, Qt::DisplayRole).toString();
    const bool hide = !text.isEmpty() && child != current.siblingAtColumn(0) &&
                      !name.contains(text, Qt::CaseInsensitive) &&
                      !model->isLoading(child);
    ui.tree->setRowHidden(r, folder, hide);
  }
  if (!text.isEmpty()) {
    ui.tree->expand(folder);
  }
}
