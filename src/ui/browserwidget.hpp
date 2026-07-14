/*
    
    File: browserwidget.hpp

    Copyright (C) 2025 Sinan Islekdemir <sinan@islekdemir.com>

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
#ifndef BROWSERWIDGET_HPP
#define BROWSERWIDGET_HPP

#include <QWidget>
#include <QTreeView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include "wrappers/filetreemodel.hpp"
#include "common/format_utils.hpp"

class NameFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit NameFilterProxyModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    QString filterText() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool hasAcceptedChild(int sourceRow, const QModelIndex &sourceParent) const;
    QString m_filterText;
};

class DeletedFileFilterProxy : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit DeletedFileFilterProxy(QObject *parent = nullptr);

    void setFilterDeletedOnly(bool enabled);
    bool filterDeletedOnly() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_deletedOnly;
};

class BrowserWidget : public QWidget {
    Q_OBJECT
public:
    explicit BrowserWidget(QWidget *parent = nullptr);

    void setModel(QAbstractItemModel *model);
    void setFileModel(FileTreeModel *model);
    void resetModel(FileTreeModel *model);
    void markSelected();
    FileTreeModel *model() const;

signals:
    void restoreRequested();
    void quitRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onMark();
    void onUnmark();
    void onUnmarkAll();
    void onExpandAll();
    void onCollapseAll();
    void onInvert();
    void onSearch();
    void onToggleFilter();
    void onMarkedChanged(int count, uint64_t totalSize);
    void onToggleMark(const QModelIndex &idx);
    void refreshFromFileModel();

private:
    void setupUi();
    void applyTheme();
    void setSourceModelAndExpand(QAbstractItemModel *sourceModel);

    QLineEdit *m_pathDisplay;
    QLineEdit *m_searchEdit;
    QTreeView *m_treeView;
    QLabel *m_statusBar;
    NameFilterProxyModel *m_proxyModel;
    FileTreeModel *m_fileModel;
    QPushButton *m_markBtn;
    QPushButton *m_unmarkBtn;
    QPushButton *m_unmarkAllBtn;
    QPushButton *m_expandBtn;
    QPushButton *m_collapseBtn;
    QPushButton *m_invertBtn;
    QPushButton *m_searchBtn;
    QPushButton *m_restoreBtn;
    QPushButton *m_quitBtn;
    QPushButton *m_filterBtn;
};

#endif // BROWSERWIDGET_HPP
