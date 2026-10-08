// Windows 11 style application shell for MainWindow: navigation pane on the
// left, content pages on the right. The original pages (remotes, jobs, tasks)
// and their logic are kept; this file only rearranges and restyles them.

#include "main_window.h"
#include "remote_widget.h"
#include "theme.h"
#include "utils.h"

namespace {

enum NavRole {
  KindRole = Qt::UserRole + 1, // "page" or "remote"
  TargetRole,                  // page index or remote name
  TypeRole                     // remote type
};

enum Page { HomePage = 0, TransfersPage = 1, TasksPage = 2 };

QLabel *makeLabel(const QString &text, const char *objectName) {
  auto *l = new QLabel(text);
  l->setObjectName(objectName);
  return l;
}

// Title + subtitle on the left, optional widgets on the right.
QWidget *makePageHeader(const QString &title, const QString &subtitle,
                        QLabel **subtitleOut, const QList<QWidget *> &right) {
  auto *header = new QWidget;
  auto *h = new QHBoxLayout(header);
  h->setContentsMargins(0, 0, 0, 8);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(2);
  texts->addWidget(makeLabel(title, "PageTitle"));
  auto *sub = makeLabel(subtitle, "PageSubtitle");
  sub->setWordWrap(true);
  texts->addWidget(sub);
  if (subtitleOut) {
    *subtitleOut = sub;
  }
  h->addLayout(texts, 1);
  for (QWidget *w : right) {
    h->addWidget(w, 0, Qt::AlignBottom);
  }
  return header;
}

// Home page remote card: icon tile, name, remote type.
class RemoteCardDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(164, 126);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(opt.rect).adjusted(6.5, 6.5, -6.5, -6.5);
    const bool hover = opt.state & QStyle::State_MouseOver;
    const bool selected = opt.state & QStyle::State_Selected;

    p->setPen(QPen(selected ? Theme::color("accentText") : Theme::color("cardStroke"), 1));
    p->setBrush(hover || selected ? Theme::color("cardHover") : Theme::color("card"));
    p->drawRoundedRect(card, 8, 8);

    // icon tile
    const QRectF tile(card.center().x() - 22, card.top() + 14, 44, 44);
    QColor tint = Theme::color("accentText");
    tint.setAlpha(Theme::isDark() ? 40 : 26);
    p->setPen(Qt::NoPen);
    p->setBrush(tint);
    p->drawRoundedRect(tile, 8, 8);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(p, tile.adjusted(10, 10, -10, -10).toRect());

    // name + type
    QFont f = opt.font;
    f.setWeight(QFont::DemiBold);
    p->setFont(f);
    p->setPen(Theme::color("text"));
    const QRectF nameRect(card.left() + 8, tile.bottom() + 6, card.width() - 16, 20);
    const QString name = QFontMetrics(f).elidedText(index.data(Qt::DisplayRole).toString(),
                                                    Qt::ElideRight, int(nameRect.width()));
    p->drawText(nameRect, Qt::AlignHCenter | Qt::AlignVCenter, name);

    QFont typeFont = opt.font;
    typeFont.setPointSizeF(typeFont.pointSizeF() * 0.9);
    p->setFont(typeFont);
    p->setPen(Theme::color("text2"));
    p->drawText(QRectF(nameRect.left(), nameRect.bottom(), nameRect.width(), 18),
                Qt::AlignHCenter | Qt::AlignVCenter,
                index.data(Qt::UserRole).toString());
    p->restore();
  }
};

void setPageMargins(QLayout *l) {
  l->setContentsMargins(32, 28, 32, 16);
  l->setSpacing(10);
}

} // namespace

void MainWindow::buildShell() {
  setWindowTitle(IsPortableMode() ? tr("Rclone Browser (portable)")
                                  : tr("Rclone Browser"));
  setMinimumSize(900, 580);
  if (!GetSettings()->contains("MainWindow/geometry")) {
    resize(1180, 760);
  }

  // The classic menu bar is replaced by the navigation pane footer.
  ui.menuBar->hide();

  // ---------------------------------------------------- navigation pane --
  auto *nav = new QWidget;
  nav->setObjectName("NavPane");
  nav->setFixedWidth(250);
  auto *navLayout = new QVBoxLayout(nav);
  navLayout->setContentsMargins(4, 10, 4, 8);
  navLayout->setSpacing(4);

  auto *appTitle = new QWidget;
  auto *at = new QHBoxLayout(appTitle);
  at->setContentsMargins(14, 0, 8, 6);
  at->setSpacing(10);
  auto *appIcon = new QLabel;
  appIcon->setPixmap(qApp->windowIcon().pixmap(18, 18));
  at->addWidget(appIcon);
  auto *appName = new QLabel(tr("Rclone Browser"));
  appName->setProperty("secondary", true);
  at->addWidget(appName, 1);
  navLayout->addWidget(appTitle);

  mNav = new QListWidget;
  mNav->setObjectName("NavList");
  mNav->setIconSize(QSize(20, 20));
  mNav->setFrameShape(QFrame::NoFrame);
  mNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  mNav->setContextMenuPolicy(Qt::CustomContextMenu);
  mNav->setTextElideMode(Qt::ElideRight);
  navLayout->addWidget(mNav, 1);

  auto addPage = [&](const QString &icon, const QString &text, int page) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, mNav);
    it->setData(KindRole, "page");
    it->setData(TargetRole, page);
    return it;
  };
  addPage("home", tr("Home"), HomePage);
  mNavTransfers = addPage("transfers", tr("Transfers"), TransfersPage);
  addPage("tasks", tr("Tasks"), TasksPage);

  auto *header = new QListWidgetItem(tr("Remotes"), mNav);
  header->setFlags(Qt::NoItemFlags);
  QFont hf = header->font();
  hf.setWeight(QFont::DemiBold);
  hf.setPointSizeF(hf.pointSizeF() * 0.92);
  header->setFont(hf);
  header->setSizeHint(QSize(0, 40));
  mNavRemotesHeader = header;

  auto footerButton = [&](const QString &icon, const QString &text) {
    auto *b = new QToolButton;
    b->setObjectName("NavFooter");
    b->setIcon(Theme::icon(icon));
    b->setIconSize(QSize(20, 20));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setCursor(Qt::PointingHandCursor);
    return b;
  };

  auto *more = footerButton("more", tr("More"));
  auto *moreMenu = new QMenu(more);
  // Help menu (rclone updates, about) is assembled in setupRcloneUpdater();
  // mirror it here so it stays in sync.
  QObject::connect(moreMenu, &QMenu::aboutToShow, this, [=]() {
    moreMenu->clear();
    for (QAction *a : ui.menuHelp->actions()) {
      moreMenu->addAction(a);
    }
    moreMenu->addSeparator();
    moreMenu->addAction(ui.quit);
  });
  more->setMenu(moreMenu);
  more->setPopupMode(QToolButton::InstantPopup);
  navLayout->addWidget(more);

  auto *settingsBtn = footerButton("settings", tr("Settings"));
  QObject::connect(settingsBtn, &QToolButton::clicked, ui.preferences,
                   &QAction::trigger);
  navLayout->addWidget(settingsBtn);

  ui.about->setIcon(Theme::icon("info"));
  ui.quit->setIcon(Theme::icon("quit"));
  ui.preferences->setIcon(Theme::icon("settings"));

  // ------------------------------------------------------ content area --
  auto *content = new QWidget;
  content->setObjectName("ContentArea");
  content->setAttribute(Qt::WA_StyledBackground);
  auto *cl = new QVBoxLayout(content);
  cl->setContentsMargins(1, 1, 0, 0);
  cl->setSpacing(0);

  ui.tabs->setParent(content);
  ui.tabs->tabBar()->hide();
  ui.tabs->setDocumentMode(true);
  cl->addWidget(ui.tabs);

  auto *central = new QWidget;
  auto *hl = new QHBoxLayout(central);
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(0);
  hl->addWidget(nav);
  hl->addWidget(content, 1);
  QWidget *old = takeCentralWidget();
  setCentralWidget(central);
  if (old) {
    old->deleteLater();
  }

  // ---------------------------------------------------------- Home page --
  {
    QWidget *page = ui.tabs->widget(HomePage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);

    ui.config->setText(tr("New remote"));
    ui.config->setProperty("accent", true);
    ui.config->setToolTip(tr("Add or edit remotes with rclone config"));
    ui.refresh->setText(QString());
    ui.refresh->setIcon(Theme::icon("refresh"));
    ui.refresh->setToolTip(tr("Reload remotes"));
    ui.open->hide();

    QLabel *sub = nullptr;
    v->insertWidget(0, makePageHeader(tr("Remotes"),
                                      tr("Your cloud storage, as configured in rclone."),
                                      &sub, {ui.refresh, ui.config}));
    mHomeSubtitle = sub;

    ui.remotes->setViewMode(QListView::IconMode);
    ui.remotes->setMovement(QListView::Static);
    ui.remotes->setResizeMode(QListView::Adjust);
    ui.remotes->setWrapping(true);
    ui.remotes->setUniformItemSizes(true);
    ui.remotes->setGridSize(QSize(170, 132));
    ui.remotes->setIconSize(QSize(40, 40));
    ui.remotes->setWordWrap(true);
    ui.remotes->setSpacing(0);
    ui.remotes->setFrameShape(QFrame::NoFrame);
    ui.remotes->setSelectionRectVisible(false);
    ui.remotes->setItemDelegate(new RemoteCardDelegate(ui.remotes));
    ui.remotes->setMouseTracking(true);
    ui.remotes->setCursor(Qt::PointingHandCursor);

    mHomeEmpty = new QLabel(
        tr("<b>No remotes yet</b><br><br>Click <b>New remote</b> to connect a "
           "cloud drive, server or folder with rclone's setup assistant."));
    mHomeEmpty->setAlignment(Qt::AlignCenter);
    mHomeEmpty->setProperty("secondary", true);
    mHomeEmpty->hide();
    v->insertWidget(2, mHomeEmpty, 1);
  }

  // ----------------------------------------------------- Transfers page --
  {
    QWidget *page = ui.tabs->widget(TransfersPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Transfers"),
                                      tr("Running and finished copies, syncs, "
                                         "mounts and streams."),
                                      nullptr, {}));
    ui.jobsArea->setFrameShape(QFrame::NoFrame);
    ui.jobs->setSpacing(8);
    ui.noJobsAvailable->setText(tr("No transfers yet.\n\nUploads, downloads, "
                                   "mounts and tasks you run will appear here."));
    ui.noJobsAvailable->setAlignment(Qt::AlignCenter);
    ui.noJobsAvailable->setMinimumHeight(160);
  }

  // --------------------------------------------------------- Tasks page --
  {
    QWidget *page = ui.tabs->widget(TasksPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Tasks"),
                                      tr("Saved transfers you can run again "
                                         "with one click."),
                                      nullptr, {}));
    ui.tasksArea->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setSpacing(0);
    ui.tasksListWidget->setIconSize(QSize(20, 20));

    ui.buttonRunTask->setProperty("accent", true);
    ui.buttonDryrunTask->setIcon(Theme::icon("dryrun"));
    ui.buttonEditTask->setIcon(Theme::icon("edit"));
    ui.buttonDeleteTask->setIcon(Theme::icon("delete"));
  }

  mUploadIcon = Theme::icon("upload");
  mDownloadIcon = Theme::icon("download");

  // ------------------------------------------------------- navigation ----
  QObject::connect(mNav, &QListWidget::currentItemChanged, this,
                   [this](QListWidgetItem *item) {
                     if (!item || mSyncingNav) {
                       return;
                     }
                     if (item->data(KindRole).toString() == "page") {
                       ui.tabs->setCurrentIndex(item->data(TargetRole).toInt());
                     } else {
                       openRemote(item->data(TargetRole).toString(),
                                  item->data(TypeRole).toString());
                     }
                   });
  QObject::connect(ui.tabs, &QTabWidget::currentChanged, this,
                   [this](int) { syncNavToCurrentPage(); });

  QObject::connect(mNav, &QWidget::customContextMenuRequested, this,
                   [this](const QPoint &pos) {
                     QListWidgetItem *item = mNav->itemAt(pos);
                     if (!item || item->data(KindRole).toString() != "remote") {
                       return;
                     }
                     const QString name = item->data(TargetRole).toString();
                     QMenu menu;
                     menu.addAction(Theme::icon("open"), tr("Open"), this, [=]() {
                       openRemote(name, item->data(TypeRole).toString());
                     });
                     QAction *close = menu.addAction(Theme::icon("close"), tr("Close"), this,
                                                     [=]() { closeRemote(name); });
                     close->setEnabled(remoteTab(name) >= 0);
                     menu.addSeparator();
                     menu.addAction(Theme::icon("refresh"), tr("Reload remotes"), this,
                                    &MainWindow::rcloneListRemotes);
                     menu.exec(mNav->viewport()->mapToGlobal(pos));
                   });

  mNav->setCurrentRow(0);
}

int MainWindow::remoteTab(const QString &name) const {
  for (int i = TasksPage + 1; i < ui.tabs->count(); ++i) {
    if (ui.tabs->widget(i)->property("remoteName").toString() == name) {
      return i;
    }
  }
  return -1;
}

void MainWindow::openRemote(const QString &name, const QString &type) {
  int index = remoteTab(name);
  if (index < 0) {
    const bool isLocal = type == "local";
    const bool isGoogle = type == "drive";
    auto *remote = new RemoteWidget(&mIcons, name, isLocal, isGoogle, ui.tabs);
    remote->setProperty("remoteName", name);
    remote->setRemoteType(type);
    QObject::connect(remote, &RemoteWidget::addMount, this, &MainWindow::addMount);
    QObject::connect(remote, &RemoteWidget::addStream, this, &MainWindow::addStream);
    QObject::connect(remote, &RemoteWidget::addTransfer, this, &MainWindow::addTransfer);
    QObject::connect(remote, &RemoteWidget::closeRequested, this,
                     [this, name]() { closeRemote(name); });
    index = ui.tabs->addTab(remote, name);
  }
  ui.tabs->setCurrentIndex(index);
}

void MainWindow::closeRemote(const QString &name) {
  int index = remoteTab(name);
  if (index < 0) {
    return;
  }
  QWidget *w = ui.tabs->widget(index);
  const bool wasCurrent = ui.tabs->currentIndex() == index;
  ui.tabs->removeTab(index);
  w->deleteLater();
  if (wasCurrent) {
    ui.tabs->setCurrentIndex(HomePage);
  }
  syncNavToCurrentPage();
}

void MainWindow::syncNavToCurrentPage() {
  if (!mNav) {
    return;
  }
  const int index = ui.tabs->currentIndex();
  const QString remote =
      index > TasksPage ? ui.tabs->widget(index)->property("remoteName").toString()
                        : QString();
  for (int i = 0; i < mNav->count(); ++i) {
    QListWidgetItem *it = mNav->item(i);
    const bool match =
        remote.isEmpty()
            ? (it->data(KindRole).toString() == "page" &&
               it->data(TargetRole).toInt() == index)
            : (it->data(KindRole).toString() == "remote" &&
               it->data(TargetRole).toString() == remote);
    if (match) {
      mSyncingNav = true;
      mNav->setCurrentItem(it);
      mSyncingNav = false;
      return;
    }
  }
}

void MainWindow::rebuildNavRemotes() {
  if (!mNav) {
    return;
  }
  // drop old remote entries
  for (int i = mNav->count() - 1; i >= 0; --i) {
    if (mNav->item(i)->data(KindRole).toString() == "remote") {
      delete mNav->takeItem(i);
    }
  }
  const int n = ui.remotes->count();
  for (int i = 0; i < n; ++i) {
    QListWidgetItem *src = ui.remotes->item(i);
    auto *it = new QListWidgetItem(src->icon(), src->text(), mNav);
    it->setData(KindRole, "remote");
    it->setData(TargetRole, src->text());
    it->setData(TypeRole, src->data(Qt::UserRole));
    it->setToolTip(src->text() + " · " + src->data(Qt::UserRole).toString());

  }
  mNavRemotesHeader->setHidden(n == 0);
  ui.remotes->setVisible(n > 0);
  mHomeEmpty->setVisible(n == 0);
  mHomeSubtitle->setText(
      n == 0 ? tr("Your cloud storage, as configured in rclone.")
             : (n == 1 ? tr("1 remote configured in rclone. Double-click to browse.")
                       : tr("%1 remotes configured in rclone. Double-click to "
                            "browse.").arg(n)));
  syncNavToCurrentPage();
}

void MainWindow::setJobsTabText(const QString &text) {
  ui.tabs->setTabText(TransfersPage, text);
  if (mNavTransfers) {
    QString t = text;
    t.replace("Jobs", tr("Transfers"));
    mNavTransfers->setText(t);
  }
}
