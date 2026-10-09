#pragma once

#include "pch.h"

struct Item;

class IconCache : public QObject {
  Q_OBJECT
public:
  IconCache(QObject *parent = nullptr);
  ~IconCache();

public slots:
  void getIcon(Item *item, const QPersistentModelIndex &parent);

signals:
  void iconReady(Item *item, const QPersistentModelIndex &parent,
                 const QIcon &icon);

};
