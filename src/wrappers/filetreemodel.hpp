/*
    
    File: filetreemodel.hpp

    Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>

    This software is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write the Free Software Foundation, Inc., 51
    Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */
#ifndef PHOTOREC_FILETREEMODEL_HPP
#define PHOTOREC_FILETREEMODEL_HPP

#include <QAbstractItemModel>
#include <QColor>
#include <QIcon>
#include <cstdint>
#include <ctime>

class QStandardItemModel;

#ifdef __cplusplus
extern "C" {
#endif
#include "recovery.h"
#ifdef __cplusplus
}
#endif

static constexpr int FileNodeRole = Qt::UserRole + 100;

class FileTreeModel : public QAbstractItemModel {
  Q_OBJECT
public:
  enum Column { ColName = 0, ColSize, ColModified, ColStatus, ColCount };
  enum Role {
    MarkedRole = Qt::UserRole + 1,
    DeletedRole,
    OrphanRole,
    BackupModifiedRole,
    FileSizeRole,
    FirstSectorRole,
    SectorCountRole,
    SortRole
  };

  explicit FileTreeModel(QObject *parent = nullptr);
  ~FileTreeModel() override;

  void setTree(scan_tree_t *tree);
  void clear();

  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &child) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

  file_node_t *nodeFromIndex(const QModelIndex &index) const;
  QModelIndex indexFromNode(file_node_t *node) const;
  bool isMarked(const QModelIndex &index) const;
  void toggleMark(const QModelIndex &index);
  void setMark(const QModelIndex &index, bool mark);
  void markSubtree(const QModelIndex &index, bool mark);
  void unmarkAll();
  void invertMarks();
  int markedCount() const;
  uint64_t markedTotalSize() const;
  QString nodePath(const QModelIndex &index) const;
  QString formatSize(uint64_t bytes) const;
  QModelIndex findBySubstring(const QModelIndex &start, const QString &text) const;

  void setShowDeletedOnly(bool show);
  bool showDeletedOnly() const;

  QColor colorForNode(const file_node_t *node, int role = Qt::DisplayRole) const;
  scan_tree_t *tree() const {
    return m_tree;
  }

  QStandardItemModel *toStandardModel(QObject *parent = nullptr) const;
  static QString formatSizeStatic(uint64_t bytes);

signals:
  void markedChanged(int count, uint64_t totalSize);

private:
  scan_tree_t *m_tree;
  bool m_ownTree;
  bool m_showDeletedOnly;

  static int childrenCount(const file_node_t *node);
  static int rowOfChild(const file_node_t *node);
  static file_node_t *childNode(const file_node_t *parent, int row);
  void collectLeafPaths(const QModelIndex &parent, QVector<QModelIndex> &results) const;
};

#endif // PHOTOREC_FILETREEMODEL_HPP
