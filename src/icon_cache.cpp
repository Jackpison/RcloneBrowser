#include "icon_cache.h"
#include "item_model.h"
#include "theme.h"

// File type icons are built from bundled SVGs, which is instant, so this now
// runs on the GUI thread. (It used to run on a worker thread to query the
// Windows shell; creating QIcons and sharing the icon cache across threads
// was a data race.)
IconCache::IconCache(QObject *parent) : QObject(parent) {}

IconCache::~IconCache() = default;

void IconCache::getIcon(Item *item, const QPersistentModelIndex &parent) {
  // Colour-coded Fluent file type icons (same look on every PC, sharp at any
  // display scale) instead of the old shell / theme icons.
  emit iconReady(item, parent, Theme::fileIcon(item->name, false));
}
