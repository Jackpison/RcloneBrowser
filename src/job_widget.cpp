#include "job_widget.h"
#include "rclone_output.h"
#include "utils.h"

JobWidget::JobWidget(QProcess *process, const QString &info,
                     const QStringList &args, const QString &source,
                     const QString &dest, QWidget *parent)
    : QWidget(parent), mProcess(process) {
  ui.setupUi(this);

  mArgs.append(QDir::toNativeSeparators(GetRclone()));
  mArgs.append(GetRcloneConf());
  mArgs.append(args);

  ui.source->setText(source);
  ui.dest->setText(dest);
  ui.info->setText(info);

  ui.details->setVisible(false);

  ui.output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  ui.output->setVisible(false);

  QObject::connect(
      ui.showDetails, &QToolButton::toggled, this, [=](bool checked) {
        ui.details->setVisible(checked);
        ui.showDetails->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
      });

  QObject::connect(
      ui.showOutput, &QToolButton::toggled, this, [=](bool checked) {
        ui.output->setVisible(checked);
        ui.showOutput->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
      });

  ui.cancel->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogCloseButton));

  QObject::connect(ui.cancel, &QToolButton::clicked, this, [=]() {
    if (mRunning) {
      int button = QMessageBox::question(
          this, "Transfer",
          QString("rclone process is still running. Do you want to cancel it?"),
          QMessageBox::Yes | QMessageBox::No);
      if (button == QMessageBox::Yes) {
        cancel();
      }
    } else {
      emit closed();
    }
  });

  ui.copy->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_FileLinkIcon));

  QObject::connect(ui.copy, &QToolButton::clicked, this, [=]() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(mArgs.join(" "));
  });

  QObject::connect(mProcess, &QProcess::readyRead, this, [=]() {
    using RcloneOutput::StatsLine;

    while (mProcess->canReadLine()) {
      QString line = QString::fromUtf8(mProcess->readLine()).trimmed();
      if (++mLines == 10000) {
        ui.output->clear();
        mLines = 1;
      }
      ui.output->appendPlainText(line);

      if (line.isEmpty()) {
        // end of a stats block: drop progress bars of files that finished
        for (auto it = mActive.begin(), eit = mActive.end(); it != eit;
             /* empty */) {
          auto label = it.value();
          if (mUpdated.contains(label)) {
            ++it;
          } else {
            it = mActive.erase(it);
            ui.progress->removeWidget(label->buddy());
            ui.progress->removeWidget(label);
            delete label->buddy();
            delete label;
          }
        }
        mUpdated.clear();
        continue;
      }

      const StatsLine s = RcloneOutput::parseStatsLine(line);
      switch (s.kind) {
      case StatsLine::Bytes:
        if (s.total.isEmpty()) { // rclone <= 1.42
          ui.size->setText(s.done);
        } else {
          ui.size->setText(s.done + ", " + s.percent);
          ui.totalsize->setText(s.total);
          ui.eta->setText(s.eta);
        }
        ui.bandwidth->setText(s.speed);
        break;
      case StatsLine::Files:
        ui.transferred->setText(s.text);
        break;
      case StatsLine::Checks:
        ui.checks->setText(s.text);
        break;
      case StatsLine::Errors:
        ui.errors->setText(s.text);
        break;
      case StatsLine::Elapsed:
        ui.elapsed->setText(s.text);
        break;
      case StatsLine::FileProgress: {
        QLabel *label;
        QProgressBar *bar;
        auto it = mActive.find(s.name);
        if (it == mActive.end()) {
          label = new QLabel();
          label->setText(s.name.length() > 47
                             ? s.name.left(25) + "..." + s.name.right(19)
                             : s.name);
          bar = new QProgressBar();
          bar->setMinimum(0);
          bar->setMaximum(100);
          bar->setTextVisible(true);
          label->setBuddy(bar);
          ui.progress->addRow(label, bar);
          mActive.insert(s.name, label);
        } else {
          label = it.value();
          bar = static_cast<QProgressBar *>(label->buddy());
        }
        bar->setValue(s.filePercent);
        bar->setToolTip("File name: " + s.name + "\n" + s.detail);
        mUpdated.insert(label);
        break;
      }
      case StatsLine::None:
        break;
      }
    }
  });

  QObject::connect(mProcess,
                   static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                       &QProcess::finished),
                   this, [=](int status, QProcess::ExitStatus) {
                     mProcess->deleteLater();
                     for (auto label : mActive) {
                       ui.progress->removeWidget(label->buddy());
                       ui.progress->removeWidget(label);
                       delete label->buddy();
                       delete label;
                     }

                     mRunning = false;
                     if (status == 0) {
                       ui.showDetails->setStyleSheet(
                           "QToolButton { border: 0; color: black; }");
                       ui.showDetails->setText("Finished");
                     } else {
                       ui.showDetails->setStyleSheet(
                           "QToolButton { border: 0; color: red; }");
                       ui.showDetails->setText("Error");
                     }

                     ui.cancel->setToolTip("Close");

                     emit finished(ui.info->text());
                   });

  ui.showDetails->setStyleSheet("QToolButton { border: 0; color: green; }");
  ui.showDetails->setText("Running");
}

JobWidget::~JobWidget() {}

void JobWidget::showDetails() { ui.showDetails->setChecked(true); }

void JobWidget::cancel() {
  if (!mRunning) {
    return;
  }

  mProcess->kill();
  mProcess->waitForFinished();

  emit closed();
}
