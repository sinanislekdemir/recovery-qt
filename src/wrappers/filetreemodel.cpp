/*
    
    File: filetreemodel.cpp

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
#include "filetreemodel.hpp"
#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QDebug>
#include <QStandardItemModel>

FileTreeModel::FileTreeModel(QObject *parent)
    : QAbstractItemModel(parent), m_tree(nullptr), m_ownTree(false), m_showDeletedOnly(false) {}

FileTreeModel::~FileTreeModel()
{
    if (m_ownTree && m_tree)
        tree_free(m_tree);
}

void FileTreeModel::setTree(scan_tree_t *tree)
{
    beginResetModel();
    if (m_ownTree && m_tree)
        tree_free(m_tree);
    m_tree = tree;
    m_ownTree = false;
    endResetModel();
}

void FileTreeModel::clear()
{
    beginResetModel();
    if (m_ownTree && m_tree)
        tree_free(m_tree);
    m_tree = nullptr;
    m_ownTree = false;
    endResetModel();
    emit markedChanged(0, 0);
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!m_tree || !m_tree->root) {
        qDebug() << "index(" << row << "," << column << "): tree null";
        return QModelIndex();
    }
    file_node_t *parentNode = parent.isValid()
        ? static_cast<file_node_t*>(parent.internalPointer())
        : m_tree->root;
    file_node_t *child = childNode(parentNode, row);
    if (!parent.isValid() && row < 3)
        qDebug() << "index(root," << row << "," << column
                 << "): parentNode=" << parentNode->name
                 << " child=" << (child ? child->name : "(null)");
    if (child)
        return createIndex(row, column, child);
    return QModelIndex();
}

QModelIndex FileTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();
    file_node_t *node = static_cast<file_node_t*>(child.internalPointer());
    file_node_t *parent = node->parent;
    if (!parent || parent == m_tree->root)
        return QModelIndex();
    int row = rowOfChild(parent);
    if (row >= 0)
        return createIndex(row, 0, parent);
    return QModelIndex();
}

int FileTreeModel::rowCount(const QModelIndex &parent) const
{
    if (!m_tree || !m_tree->root) {
        static int once = 0;
        if (!once++) qDebug() << "FileTreeModel::rowCount: m_tree is null!";
        return 0;
    }
    file_node_t *node = parent.isValid()
        ? static_cast<file_node_t*>(parent.internalPointer())
        : m_tree->root;
    if (m_showDeletedOnly && node->type == NODE_DIR) {
        int count = 0;
        struct td_list_head *pos;
        td_list_for_each(pos, &node->children) {
            file_node_t *c = td_list_entry(pos, file_node_t, siblings);
            if (c->deleted || c->orphan)
                count++;
        }
        return count;
    }
    int cnt = childrenCount(node);
    if (!parent.isValid()) {
        static int rootOnce = 0;
        if (!rootOnce++) qDebug() << "FileTreeModel::rowCount(root)=" << cnt << "m_tree=" << (void*)m_tree;
    }
    return cnt;
}

int FileTreeModel::columnCount(const QModelIndex &) const { return ColCount; }

QVariant FileTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    file_node_t *node = static_cast<file_node_t*>(index.internalPointer());
    if (!node)
        return QVariant();

    if (role == Qt::DisplayRole || role == SortRole) {
        switch (index.column()) {
        case ColName: {
            QString name = QString::fromUtf8(node->name);
            if (index.row() < 3 && !index.parent().isValid())
                qDebug() << "data row" << index.row() << "name=" << name << "type=" << node->type;
            return name;
        }
        case ColSize:
            if (node->type == NODE_FILE)
                return QVariant::fromValue(static_cast<qulonglong>(node->size));
            return QVariant();
        case ColModified:
            if (node->mtime > 0) {
                QDateTime dt = QDateTime::fromSecsSinceEpoch(node->mtime);
                if (role == Qt::DisplayRole)
                    return dt.toString("yyyy-MM-dd hh:mm:ss");
                return QVariant::fromValue(static_cast<qulonglong>(node->mtime));
            }
            return QVariant();
        case ColStatus:
            if (role == Qt::DisplayRole) {
                if (node->backup_modified && !node->deleted)
                    return QStringLiteral("modified");
                if (node->deleted)
                    return QStringLiteral("deleted");
                if (node->orphan)
                    return QStringLiteral("orphan");
                return QStringLiteral("ok");
            }
            return QVariant();
        default:
            return QVariant();
        }
    }

    if (role == Qt::SizeHintRole) {
        return formatSize(node->size);
    }

    if (role == Qt::FontRole && node->type == NODE_DIR) {
        QFont f;
        f.setBold(true);
        return f;
    }

    if (role == Qt::ForegroundRole) {
        return colorForNode(node);
    }

    if (role == MarkedRole) return node->marked != 0;
    if (role == DeletedRole) return node->deleted != 0;
    if (role == OrphanRole) return node->orphan != 0;
    if (role == BackupModifiedRole) return node->backup_modified != 0;
    if (role == FileSizeRole) return QVariant::fromValue(static_cast<qulonglong>(node->size));
    if (role == Qt::ToolTipRole && node->orphan) {
        return QString("Orphan/carved file at sector %1, %2 sectors")
            .arg(node->first_sector).arg(node->num_sectors);
    }

    return QVariant();
}

bool FileTreeModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;
    file_node_t *node = static_cast<file_node_t*>(index.internalPointer());
    if (!node)
        return false;

    if (role == MarkedRole) {
        bool newMark = value.toBool();
        if (node->marked != (unsigned int)newMark) {
            node->marked = newMark ? 1 : 0;
            uint64_t sz = 0;
            int cnt = static_cast<int>(tree_count_marked(m_tree->root, &sz));
            emit markedChanged(cnt, sz);
            emit dataChanged(index, index, {Qt::ForegroundRole});
            return true;
        }
    }
    return false;
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    file_node_t *node = static_cast<file_node_t*>(index.internalPointer());
    if (node->type == NODE_DIR)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();
    switch (section) {
    case ColName:     return QStringLiteral("Name");
    case ColSize:     return QStringLiteral("Size");
    case ColModified: return QStringLiteral("Modified");
    case ColStatus:   return QStringLiteral("Status");
    default: return QVariant();
    }
}

file_node_t* FileTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<file_node_t*>(index.internalPointer());
}

QModelIndex FileTreeModel::indexFromNode(file_node_t *node) const
{
    if (!node || !m_tree)
        return QModelIndex();
    if (node == m_tree->root)
        return QModelIndex();
    file_node_t *parent = node->parent;
    if (!parent)
        return QModelIndex();
    int row = 0;
    struct td_list_head *pos;
    td_list_for_each(pos, &parent->children) {
        file_node_t *c = td_list_entry(pos, file_node_t, siblings);
        if (c == node)
            return createIndex(row, 0, node);
        row++;
    }
    return QModelIndex();
}

bool FileTreeModel::isMarked(const QModelIndex &index) const
{
    file_node_t *node = nodeFromIndex(index);
    return node ? node->marked != 0 : false;
}

void FileTreeModel::toggleMark(const QModelIndex &index)
{
    file_node_t *node = nodeFromIndex(index);
    if (!node)
        return;
    setData(index, !node->marked, MarkedRole);
}

void FileTreeModel::setMark(const QModelIndex &index, bool mark)
{
    setData(index, mark, MarkedRole);
}

void FileTreeModel::markSubtree(const QModelIndex &index, bool mark)
{
    file_node_t *node = nodeFromIndex(index);
    if (!node)
        return;

    auto markRecursive = [mark, this](file_node_t *n, auto&& self) -> void {
        if (n->type == NODE_FILE)
            n->marked = mark ? 1 : 0;
        struct td_list_head *pos;
        td_list_for_each(pos, &n->children) {
            file_node_t *c = td_list_entry(pos, file_node_t, siblings);
            self(c, self);
        }
    };
    markRecursive(node, markRecursive);

    uint64_t sz = 0;
    int cnt = static_cast<int>(tree_count_marked(m_tree->root, &sz));
    emit markedChanged(cnt, sz);
    emit dataChanged(QModelIndex(), QModelIndex(), {Qt::ForegroundRole});
}

void FileTreeModel::invertMarks()
{
    if (!m_tree || !m_tree->root)
        return;

    auto invertRecursive = [](file_node_t *n, auto&& self) -> void {
        if (n->type == NODE_FILE)
            n->marked = n->marked ? 0 : 1;
        struct td_list_head *pos;
        td_list_for_each(pos, &n->children) {
            file_node_t *c = td_list_entry(pos, file_node_t, siblings);
            self(c, self);
        }
    };
    invertRecursive(m_tree->root, invertRecursive);

    uint64_t sz = 0;
    int cnt = static_cast<int>(tree_count_marked(m_tree->root, &sz));
    emit markedChanged(cnt, sz);
    emit dataChanged(QModelIndex(), QModelIndex(), {Qt::ForegroundRole});
}

void FileTreeModel::unmarkAll()
{
    if (!m_tree || !m_tree->root)
        return;

    auto unmarkRecursive = [](file_node_t *n, auto&& self) -> void {
        n->marked = 0;
        struct td_list_head *pos;
        td_list_for_each(pos, &n->children) {
            file_node_t *c = td_list_entry(pos, file_node_t, siblings);
            self(c, self);
        }
    };
    unmarkRecursive(m_tree->root, unmarkRecursive);

    emit markedChanged(0, 0);
    emit dataChanged(QModelIndex(), QModelIndex(), {Qt::ForegroundRole});
}

int FileTreeModel::markedCount() const
{
    if (!m_tree)
        return 0;
    uint64_t sz = 0;
    return static_cast<int>(tree_count_marked(m_tree->root, &sz));
}

uint64_t FileTreeModel::markedTotalSize() const
{
    if (!m_tree)
        return 0;
    uint64_t sz = 0;
    tree_count_marked(m_tree->root, &sz);
    return sz;
}

void FileTreeModel::setShowDeletedOnly(bool show)
{
    m_showDeletedOnly = show;
    beginResetModel();
    endResetModel();
}

bool FileTreeModel::showDeletedOnly() const
{
    return m_showDeletedOnly;
}

/*
 * toStandardModel: Converts the C file_node_t tree into a QStandardItemModel.
 * Recursively visits every node, creates QStandardItem for each, applies
 * color coding. Used by BrowserWidget for display because QStandardItemModel
 * makes color/formatting easier than QAbstractItemModel's data() roles.
 *
 * PERFORMANCE: Full O(n) tree rebuild. Called after every mark/unmark
 * operation (BrowserWidget::refreshFromFileModel) and on filter toggle.
 * For large trees (millions of nodes from deep FS scan), this causes
 * significant UI lag because every QStandardItem is re-allocated.
 *
 * ALTERNATIVE: Use FileTreeModel directly (QAbstractItemModel) + a
 * QStyledItemDelegate that reads color from FileTreeModel::colorForNode().
 * Then mark/unmark only needs beginResetModel()/endResetModel() or
 * targeted dataChanged() signals instead of full rebuild.
 */
QStandardItemModel *FileTreeModel::toStandardModel(QObject *parent) const
{
    if (!m_tree || !m_tree->root)
        return nullptr;

    QStandardItemModel *model = new QStandardItemModel(parent);
    model->setHorizontalHeaderLabels({"Name", "Size", "Modified", "Status"});

    struct td_list_head *pos;

    auto addItem = [](QStandardItemModel *m, QStandardItem *parentItem, file_node_t *node, bool filter, auto&& self) -> void {
        if (!node) return;

        if (filter && !node->deleted && !node->orphan && !node->backup_modified) {
            if (node->type == NODE_DIR) {
                struct td_list_head *pos2;
                td_list_for_each(pos2, &node->children) {
                    file_node_t *c = td_list_entry(pos2, file_node_t, siblings);
                    self(m, parentItem, c, filter, self);
                }
            }
            return;
        }

        QList<QStandardItem*> row;
        QStandardItem *ni = new QStandardItem(QString::fromUtf8(node->name ? node->name : ""));
        ni->setEditable(false);
        ni->setData(QVariant::fromValue((quintptr)node), FileNodeRole);
        if (node->marked)
            ni->setForeground(QColor("#A3BE8C"));
        else if (node->deleted)
            ni->setForeground(QColor("#BF616A"));
        else if (node->orphan)
            ni->setForeground(QColor("#B48EAD"));
        else if (node->backup_modified)
            ni->setForeground(QColor("#5E81AC"));

        if (node->type == NODE_DIR) {
            QFont f = ni->font();
            f.setBold(true);
            ni->setFont(f);
        }

        QStandardItem *si = new QStandardItem();
        si->setEditable(false);
        if (node->type == NODE_FILE) {
            char buf[32];
            si->setText(QString::fromLatin1(tree_format_size(node->size, buf, sizeof(buf))));
        }

        QStandardItem *di = new QStandardItem();
        di->setEditable(false);
        if (node->mtime > 0)
            di->setText(QDateTime::fromSecsSinceEpoch(node->mtime).toString("yyyy-MM-dd hh:mm:ss"));

        QString st;
        if (node->backup_modified && !node->deleted)
            st = "modified";
        else if (node->deleted)
            st = "deleted";
        else if (node->orphan)
            st = "orphan";
        else
            st = "ok";
        QStandardItem *sti = new QStandardItem(st);
        sti->setEditable(false);

        row << ni << si << di << sti;

        if (parentItem)
            parentItem->appendRow(row);
        else
            m->appendRow(row);

        if (node->type == NODE_DIR) {
            struct td_list_head *pos2;
            td_list_for_each(pos2, &node->children) {
                file_node_t *c = td_list_entry(pos2, file_node_t, siblings);
                self(m, ni, c, filter, self);
            }
        }
    };

    td_list_for_each(pos, &m_tree->root->children) {
        file_node_t *c = td_list_entry(pos, file_node_t, siblings);
        addItem(model, nullptr, c, m_showDeletedOnly, addItem);
    }

    return model;
}

QString FileTreeModel::formatSizeStatic(uint64_t bytes)
{
    char buf[32];
    return QString::fromLatin1(tree_format_size(bytes, buf, sizeof(buf)));
}

QString FileTreeModel::nodePath(const QModelIndex &index) const
{
    file_node_t *node = nodeFromIndex(index);
    if (!node)
        return QString();
    char buf[4096];
    tree_get_path(node, m_tree->root, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

QString FileTreeModel::formatSize(uint64_t bytes) const
{
    char buf[32];
    return QString::fromLatin1(tree_format_size(bytes, buf, sizeof(buf)));
}

QModelIndex FileTreeModel::findBySubstring(const QModelIndex &start, const QString &text) const
{
    QModelIndex next = start.isValid() ? start : index(0, 0);
    while (next.isValid()) {
        file_node_t *node = nodeFromIndex(next);
        if (node && node->name &&
            QString::fromUtf8(node->name).contains(text, Qt::CaseInsensitive))
            return next;

        QModelIndex child = next.model()->index(0, 0, next);
        if (child.isValid()) {
            next = child;
            continue;
        }
        QModelIndex sib = next.sibling(next.row() + 1, 0);
        while (!sib.isValid() && next.parent().isValid()) {
            next = next.parent();
            sib = next.sibling(next.row() + 1, 0);
        }
        next = sib;
    }
    return QModelIndex();
}

QColor FileTreeModel::colorForNode(const file_node_t *node, int) const
{
    if (!node)
        return QColor();
    if (node->marked)
        return QColor("#A3BE8C");
    if (node->deleted)
        return QColor("#BF616A");
    if (node->backup_modified)
        return QColor("#5E81AC");
    return QColor("#ECEFF4");
}

int FileTreeModel::childrenCount(const file_node_t *node)
{
    if (!node)
        return 0;
    int count = 0;
    struct td_list_head *pos;
    td_list_for_each(pos, &node->children) {
        (void)pos;
        count++;
    }
    return count;
}

int FileTreeModel::rowOfChild(const file_node_t *node)
{
    if (!node || !node->parent)
        return -1;
    int row = 0;
    struct td_list_head *pos;
    td_list_for_each(pos, &node->parent->children) {
        file_node_t *c = td_list_entry(pos, file_node_t, siblings);
        if (c == node)
            return row;
        row++;
    }
    return -1;
}

file_node_t* FileTreeModel::childNode(const file_node_t *parent, int row)
{
    if (!parent)
        return nullptr;
    int i = 0;
    struct td_list_head *pos;
    td_list_for_each(pos, &parent->children) {
        if (i == row)
            return td_list_entry(pos, file_node_t, siblings);
        i++;
    }
    return nullptr;
}

void FileTreeModel::collectLeafPaths(const QModelIndex &parent, QVector<QModelIndex> &results) const
{
    int rows = rowCount(parent);
    for (int r = 0; r < rows; r++) {
        QModelIndex child = index(r, 0, parent);
        if (rowCount(child) == 0)
            results.append(child);
        else
            collectLeafPaths(child, results);
    }
}
