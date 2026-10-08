#include "icon_cache.h"
#include "item_model.h"
#include "theme.h"
#if defined(Q_OS_WIN)
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#endif

IconCache::IconCache(QObject *parent) : QObject(parent) {
  mFileIcon = QFileIconProvider().icon(QFileIconProvider::File);

#if defined(Q_OS_WIN32)
  CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

  mThread.start();
  moveToThread(&mThread);
}

IconCache::~IconCache() {
  mThread.quit();
  mThread.wait();

#if defined(Q_OS_WIN32)
  CoUninitialize();
#endif
}

void IconCache::getIcon(Item *item, const QPersistentModelIndex &parent) {
  // Colour-coded Fluent file type icons (same look on every PC, sharp at any
  // display scale) instead of the old shell / theme icons.
  emit iconReady(item, parent, Theme::fileIcon(item->name, false));
}
