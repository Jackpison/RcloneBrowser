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
  TypeRole,                    // remote type
  BadgeRole                    // number shown on the right (0 = none)
};

// Sidebar entry: rounded highlight, gold text and icon when selected,
// optional count badge on the right.
class NavDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(200, 42);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QRectF r = QRectF(opt.rect).adjusted(6, 3, -6, -3);
    if (selected || hover) {
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("navSel") : Theme::color("subtleHover"));
      p->drawRoundedRect(r, 10, 10);
    }
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const QRect iconRect(int(r.left()) + 14, int(r.center().y()) - 11, 22, 22);
    icon.paint(p, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);

    QFont f = opt.font;
    f.setPointSizeF(f.pointSizeF() * 1.04);
    if (selected) {
      f.setWeight(QFont::DemiBold);
    }
    p->setFont(f);
    p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));

    const int badge = index.data(BadgeRole).toInt();
    QRectF textRect(iconRect.right() + 14, r.top(), r.right() - iconRect.right() - 24, r.height());
    if (badge > 0) {
      const QString b = QString::number(badge);
      QFont bf = opt.font;
      bf.setPointSizeF(bf.pointSizeF() * 0.85);
      bf.setWeight(QFont::DemiBold);
      const qreal w = qMax<qreal>(22, QFontMetricsF(bf).horizontalAdvance(b) + 12);
      const QRectF pill(r.right() - 12 - w, r.center().y() - 11, w, 22);
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("accentBtn") : Theme::color("tile"));
      p->drawRoundedRect(pill, 11, 11);
      p->setFont(bf);
      p->setPen(selected ? Theme::color("onAccent") : Theme::color("text2"));
      p->drawText(pill, Qt::AlignCenter, b);
      p->setFont(f);
      p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));
      textRect.setRight(pill.left() - 8);
    }
    const QString text = QFontMetrics(f).elidedText(
        index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(textRect.width()));
    p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    p->restore();
  }
};

enum Page { HomePage = 0, TransfersPage = 1, TasksPage = 2, BrowserPage = 3 };

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

    // service tile
    const QRectF tile(card.center().x() - 22, card.top() + 14, 44, 44);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(p, tile.toRect());

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
  setWindowTitle(IsPortableMode() ? tr("Rclone Browser Portable")
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
  nav->setFixedWidth(280);
  auto *navLayout = new QVBoxLayout(nav);
  navLayout->setContentsMargins(10, 18, 10, 14);
  navLayout->setSpacing(2);

  // brand
  auto *brand = new QWidget;
  auto *bl = new QHBoxLayout(brand);
  bl->setContentsMargins(14, 0, 8, 14);
  bl->setSpacing(12);
  auto *logo = new QLabel;
  logo->setPixmap(qApp->windowIcon().pixmap(32, 32));
  bl->addWidget(logo);
  auto *appName = new QLabel(tr("Rclone Browser"));
  appName->setObjectName("AppTitle");
  bl->addWidget(appName, 1);
  navLayout->addWidget(brand);

  auto setupList = [&](QListWidget *list, const char *name) {
    list->setObjectName(name);
    list->setItemDelegate(new NavDelegate(list));
    list->setIconSize(QSize(22, 22));
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMouseTracking(true);
    list->setCursor(Qt::PointingHandCursor);
    list->setUniformItemSizes(true);
  };

  // main pages
  mNav = new QListWidget;
  setupList(mNav, "NavList");
  auto addPage = [&](const QString &icon, const QString &text, int page) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, mNav);
    it->setData(KindRole, "page");
    it->setData(TargetRole, page);
    return it;
  };
  addPage("home", tr("Home"), HomePage);
  mNavTransfers = addPage("transfers", tr("Transfers"), TransfersPage);
  addPage("tasks", tr("Tasks"), TasksPage);
  mNav->setFixedHeight(3 * 42 + 4);
  mNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navLayout->addWidget(mNav);

  // remotes section header: title, count, add button
  mNavRemotesHeader = new QWidget;
  auto *rh = new QHBoxLayout(mNavRemotesHeader);
  rh->setContentsMargins(0, 18, 8, 6);
  rh->setSpacing(8);
  auto *rt = new QLabel(tr("Remotes"));
  rt->setObjectName("NavSection");
  rh->addWidget(rt);
  mNavRemoteCount = new QLabel;
  mNavRemoteCount->setObjectName("NavCount");
  rh->addWidget(mNavRemoteCount);
  rh->addStretch(1);
  auto *addRemote = new QToolButton;
  addRemote->setObjectName("NavAdd");
  addRemote->setIcon(Theme::icon("add"));
  addRemote->setToolTip(tr("New remote"));
  QObject::connect(addRemote, &QToolButton::clicked, ui.config, &QPushButton::click);
  rh->addWidget(addRemote);
  navLayout->addWidget(mNavRemotesHeader);

  mNavRemotes = new QListWidget;
  setupList(mNavRemotes, "NavRemotes");
  mNavRemotes->setContextMenuPolicy(Qt::CustomContextMenu);
  navLayout->addWidget(mNavRemotes, 1);

  // footer
  auto *divider = new QFrame;
  divider->setObjectName("NavDivider");
  navLayout->addSpacing(6);
  navLayout->addWidget(divider);
  navLayout->addSpacing(6);

  auto footerButton = [&](const QString &icon, const QString &text) {
    auto *b = new QToolButton;
    b->setObjectName("NavFooter");
    b->setIcon(Theme::icon(icon));
    b->setIconSize(QSize(20, 20));
    b->setText(text);
    b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    b->setCursor(Qt::PointingHandCursor);
    navLayout->addWidget(b);
    return b;
  };

  auto *settingsBtn = footerButton("settings", tr("Settings"));
  QObject::connect(settingsBtn, &QToolButton::clicked, ui.preferences,
                   &QAction::trigger);

  mThemeButton = footerButton(Theme::isDark() ? "sun" : "moon",
                              Theme::isDark() ? tr("Light mode") : tr("Dark mode"));
  mThemeButton->setToolTip(tr("Switch between light and dark"));
  QObject::connect(mThemeButton, &QToolButton::clicked, this, [this]() {
    Theme::toggle();
    updateThemeButton();
  });

  auto *more = footerButton("more", tr("More"));
  auto *moreMenu = new QMenu(more);
  QObject::connect(moreMenu, &QMenu::aboutToShow, this, [=, this]() {
    moreMenu->clear();
    for (QAction *a : ui.menuHelp->actions()) {
      moreMenu->addAction(a);
    }
    moreMenu->addSeparator();
    moreMenu->addAction(ui.quit);
  });
  more->setMenu(moreMenu);
  more->setPopupMode(QToolButton::InstantPopup);

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

  // Home, Transfers and Tasks are pages of their own; the tab widget holds
  // only the open remotes (Explorer-style tabs).
  mPages = new QStackedWidget;
  mPages->setObjectName("Pages");
  QWidget *pageWidgets[3] = {ui.tabs->widget(0), ui.tabs->widget(1), ui.tabs->widget(2)};
  for (int i = 2; i >= 0; --i) {
    ui.tabs->removeTab(i);
  }
  for (QWidget *w : pageWidgets) {
    mPages->addWidget(w);
  }
  mPages->addWidget(ui.tabs);
  cl->addWidget(mPages);

  ui.tabs->setAttribute(Qt::WA_StyledBackground);
  ui.tabs->setDocumentMode(true);
  ui.tabs->setTabsClosable(true);
  ui.tabs->setMovable(true);
  ui.tabs->setElideMode(Qt::ElideRight);
  ui.tabs->setUsesScrollButtons(true);
  ui.tabs->tabBar()->setExpanding(false);
  ui.tabs->tabBar()->setIconSize(QSize(18, 18));

  auto *newTab = new QToolButton;
  newTab->setIcon(Theme::icon("add"));
  newTab->setToolTip(tr("New tab (Ctrl+T)"));
  QObject::connect(newTab, &QToolButton::clicked, this, [this]() {
    QWidget *w = ui.tabs->currentWidget();
    if (w && w->property("remoteName").isValid()) {
      openRemote(w->property("remoteName").toString(),
                 w->property("remoteType").toString(), true);
    }
  });
  ui.tabs->setCornerWidget(newTab, Qt::TopRightCorner);

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
    QWidget *page = mPages->widget(HomePage);
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
    QWidget *page = mPages->widget(TransfersPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Transfers"),
                                      tr("Running and finished copies, syncs, "
                                         "mounts and streams."),
                                      nullptr, {}));
    ui.jobsArea->setFrameShape(QFrame::NoFrame);
    ui.jobsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.jobs->setSpacing(8);
    ui.noJobsAvailable->setText(tr("No transfers yet.\n\nUploads, downloads, "
                                   "mounts and tasks you run will appear here."));
    ui.noJobsAvailable->setAlignment(Qt::AlignCenter);
    ui.noJobsAvailable->setMinimumHeight(160);
  }

  // --------------------------------------------------------- Tasks page --
  {
    QWidget *page = mPages->widget(TasksPage);
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
  auto onNav = [this](QListWidgetItem *item) {
    if (!item || mSyncingNav) {
      return;
    }
    if (item->data(KindRole).toString() == "page") {
      showPage(item->data(TargetRole).toInt());
    } else {
      openRemote(item->data(TargetRole).toString(),
                 item->data(TypeRole).toString());
    }
  };
  QObject::connect(mNav, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNav, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(ui.tabs, &QTabWidget::currentChanged, this,
                   [this](int) { syncNavToCurrentPage(); });
  QObject::connect(mPages, &QStackedWidget::currentChanged, this,
                   [this](int) { syncNavToCurrentPage(); });
  QObject::connect(ui.tabs, &QTabWidget::tabCloseRequested, this,
                   [this](int i) { closeRemoteWidget(ui.tabs->widget(i)); });
  // middle-click closes a tab, as in File Explorer and browsers
  ui.tabs->tabBar()->installEventFilter(this);

  QObject::connect(mNavRemotes, &QWidget::customContextMenuRequested, this,
                   [this](const QPoint &pos) {
                     QListWidgetItem *item = mNavRemotes->itemAt(pos);
                     if (!item || item->data(KindRole).toString() != "remote") {
                       return;
                     }
                     const QString name = item->data(TargetRole).toString();
                     QMenu menu;
                     menu.addAction(Theme::icon("open"), tr("Open"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString());
                     });
                     menu.addAction(Theme::icon("new_tab"), tr("Open in new tab"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString(), true);
                     });
                     QAction *close = menu.addAction(Theme::icon("close"), tr("Close all tabs"), this,
                                                     [=, this]() { closeRemote(name); });
                     close->setEnabled(remoteTab(name) >= 0);
                     menu.addSeparator();
                     menu.addAction(Theme::icon("refresh"), tr("Reload remotes"), this,
                                    &MainWindow::rcloneListRemotes);
                     menu.exec(mNavRemotes->viewport()->mapToGlobal(pos));
                   });

  mNav->setCurrentRow(0);
}

int MainWindow::remoteTab(const QString &name) const {
  for (int i = 0; i < ui.tabs->count(); ++i) {
    if (ui.tabs->widget(i)->property("remoteName").toString() == name) {
      return i;
    }
  }
  return -1;
}

void MainWindow::openRemote(const QString &name, const QString &type,
                            bool newTab) {
  int index = newTab ? -1 : remoteTab(name);
  if (index < 0) {
    const bool isLocal = type == "local";
    const bool isGoogle = type == "drive";
    auto *remote = new RemoteWidget(&mIcons, name, isLocal, isGoogle, ui.tabs);
    remote->setProperty("remoteName", name);
    remote->setProperty("remoteType", type);
    remote->setRemoteType(type);
    QObject::connect(remote, &RemoteWidget::addMount, this, &MainWindow::addMount);
    QObject::connect(remote, &RemoteWidget::addStream, this, &MainWindow::addStream);
    QObject::connect(remote, &RemoteWidget::addTransfer, this, &MainWindow::addTransfer);
    QObject::connect(remote, &RemoteWidget::closeRequested, this,
                     [this, remote]() { closeRemoteWidget(remote); });
    QObject::connect(remote, &RemoteWidget::newTabRequested, this,
                     [this, name, type]() { openRemote(name, type, true); });
    index = ui.tabs->addTab(remote, Theme::remoteIcon(type), name);
    ui.tabs->setTabToolTip(index, QString("%1 · %2").arg(name, type));
  }
  ui.tabs->setCurrentIndex(index);
  showPage(BrowserPage);
}

void MainWindow::showPage(int page) {
  if (!mPages) {
    return;
  }
  if (page == BrowserPage && ui.tabs->count() == 0) {
    page = HomePage;
  }
  mPages->setCurrentIndex(page);
  syncNavToCurrentPage();
}

void MainWindow::closeRemoteWidget(QWidget *w) {
  const int index = ui.tabs->indexOf(w);
  if (index < 0) {
    return;
  }
  ui.tabs->removeTab(index); // Qt selects the neighbouring tab
  w->deleteLater();
  if (ui.tabs->count() == 0 && mPages->currentIndex() == BrowserPage) {
    showPage(HomePage);
  }
  syncNavToCurrentPage();
}

void MainWindow::closeRemote(const QString &name) {
  int index;
  while ((index = remoteTab(name)) >= 0) {
    closeRemoteWidget(ui.tabs->widget(index));
  }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e) {
  if (o == ui.tabs->tabBar() && e->type() == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(e);
    if (me->button() == Qt::MiddleButton) {
      const int i = ui.tabs->tabBar()->tabAt(me->position().toPoint());
      if (i >= 0) {
        closeRemoteWidget(ui.tabs->widget(i));
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(o, e);
}

void MainWindow::syncNavToCurrentPage() {
  if (!mNav) {
    return;
  }
  const int index = mPages->currentIndex();
  QWidget *tab = ui.tabs->currentWidget();
  const QString remote = (index == BrowserPage && tab)
                             ? tab->property("remoteName").toString()
                             : QString();
  mSyncingNav = true;
  QListWidget *active = remote.isEmpty() ? mNav : mNavRemotes;
  QListWidget *other = remote.isEmpty() ? mNavRemotes : mNav;
  other->clearSelection();
  other->setCurrentItem(nullptr);
  for (int i = 0; i < active->count(); ++i) {
    QListWidgetItem *it = active->item(i);
    const bool match = remote.isEmpty() ? it->data(TargetRole).toInt() == index
                                        : it->data(TargetRole).toString() == remote;
    if (match) {
      active->setCurrentItem(it);
      break;
    }
  }
  mSyncingNav = false;
}

void MainWindow::rebuildNavRemotes() {
  if (!mNavRemotes) {
    return;
  }
  mSyncingNav = true;
  mNavRemotes->clear();
  mSyncingNav = false;
  const int n = ui.remotes->count();
  for (int i = 0; i < n; ++i) {
    QListWidgetItem *src = ui.remotes->item(i);
    auto *it = new QListWidgetItem(src->icon(), src->text(), mNavRemotes);
    it->setData(KindRole, "remote");
    it->setData(TargetRole, src->text());
    it->setData(TypeRole, src->data(Qt::UserRole));
    it->setToolTip(src->text() + " · " + src->data(Qt::UserRole).toString());
  }
  mNavRemoteCount->setText(QString::number(n));
  mNavRemoteCount->setVisible(n > 0);
  ui.remotes->setVisible(n > 0);
  mHomeEmpty->setVisible(n == 0);
  mHomeSubtitle->setText(
      n == 0 ? tr("Your cloud storage, as configured in rclone.")
             : (n == 1 ? tr("1 remote configured in rclone. Double-click to browse.")
                       : tr("%1 remotes configured in rclone. Double-click to "
                            "browse.").arg(n)));
  syncNavToCurrentPage();
}

void MainWindow::updateThemeButton() {
  if (mThemeButton) {
    mThemeButton->setIcon(Theme::icon(Theme::isDark() ? "sun" : "moon"));
    mThemeButton->setText(Theme::isDark() ? tr("Light mode") : tr("Dark mode"));
  }
  if (mNav) {
    mNav->viewport()->update();
    mNavRemotes->viewport()->update();
  }
}

void MainWindow::setJobsTabText(const QString &text) {
  if (mNavTransfers) {
    static const QRegularExpression rx(R"(\((\d+)\))");
    const auto m = rx.match(text);
    mNavTransfers->setData(BadgeRole, m.hasMatch() ? m.captured(1).toInt() : 0);
  }
}
