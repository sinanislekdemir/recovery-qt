/*
    
    File: browserwidget.cpp

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
#include "browserwidget.hpp"
#include "wrappers/signatureregistry.hpp"
#include <QHeaderView>
#include <QKeyEvent>
#include <QInputDialog>
#include <QMessageBox>
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QDebug>
#include <QStandardItemModel>
#include "common/theme.hpp"

NameFilterProxyModel::NameFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent) {
  setRecursiveFilteringEnabled(true);
}

void NameFilterProxyModel::setFilterText(const QString &text) {
  m_filterText = text;
  invalidateFilter();
}

QString NameFilterProxyModel::filterText() const {
  return m_filterText;
}

bool NameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
  if (m_filterText.isEmpty())
    return true;

  QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  QString name = sourceModel()->data(idx, Qt::DisplayRole).toString();

  if (name.contains(m_filterText, Qt::CaseInsensitive))
    return true;

  if (hasAcceptedChild(sourceRow, sourceParent))
    return true;

  return false;
}

bool NameFilterProxyModel::hasAcceptedChild(int sourceRow, const QModelIndex &sourceParent) const {
  QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
  int rows = sourceModel()->rowCount(idx);
  for (int i = 0; i < rows; i++) {
    if (filterAcceptsRow(i, idx))
      return true;
  }
  return false;
}

DeletedFileFilterProxy::DeletedFileFilterProxy(QObject *parent) : QSortFilterProxyModel(parent), m_deletedOnly(false) {}

void DeletedFileFilterProxy::setFilterDeletedOnly(bool enabled) {
  m_deletedOnly = enabled;
  invalidateFilter();
}

bool DeletedFileFilterProxy::filterDeletedOnly() const {
  return m_deletedOnly;
}

bool DeletedFileFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
  if (!m_deletedOnly)
    return true;
  QModelIndex srcIdx = sourceModel()->index(sourceRow, 0, sourceParent);
  QVariant deleted = sourceModel()->data(srcIdx, FileTreeModel::DeletedRole);
  QVariant orphan = sourceModel()->data(srcIdx, FileTreeModel::OrphanRole);
  QVariant bmod = sourceModel()->data(srcIdx, FileTreeModel::BackupModifiedRole);
  if (deleted.toBool() || orphan.toBool() || bmod.toBool())
    return true;
  int rows = sourceModel()->rowCount(srcIdx);
  for (int i = 0; i < rows; i++) {
    if (filterAcceptsRow(i, srcIdx))
      return true;
  }
  return false;
}

BrowserWidget::BrowserWidget(QWidget *parent)
    : QWidget(parent), m_pathDisplay(nullptr), m_searchEdit(nullptr), m_treeView(nullptr), m_statusBar(nullptr),
      m_proxyModel(nullptr), m_fileModel(nullptr), m_markBtn(nullptr), m_unmarkBtn(nullptr), m_unmarkAllBtn(nullptr),
      m_expandBtn(nullptr), m_collapseBtn(nullptr), m_searchBtn(nullptr), m_restoreBtn(nullptr), m_previewBtn(nullptr),
      m_quitBtn(nullptr), m_filterBtn(nullptr) {
  setupUi();
  applyTheme();
}

void BrowserWidget::setupUi() {
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(8, 8, 8, 8);
  mainLayout->setSpacing(4);

  m_pathDisplay = new QLineEdit(this);
  m_pathDisplay->setReadOnly(true);
  m_pathDisplay->setPlaceholderText(tr("/"));
  mainLayout->addWidget(m_pathDisplay);

  m_proxyModel = new NameFilterProxyModel(this);
  m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

  m_treeView = new QTreeView(this);
  m_treeView->setModel(m_proxyModel);
  m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  m_treeView->setAlternatingRowColors(true);
  m_treeView->setHeaderHidden(false);
  m_treeView->setUniformRowHeights(true);
  m_treeView->setAnimated(true);
  m_treeView->setIndentation(20);
  mainLayout->addWidget(m_treeView, 1);

  QHBoxLayout *toolbarLayout = new QHBoxLayout();
  toolbarLayout->setSpacing(6);

  m_markBtn = new QPushButton(tr("Mark"), this);
  m_unmarkBtn = new QPushButton(tr("Unmark"), this);
  m_unmarkAllBtn = new QPushButton(tr("Unmark All"), this);
  m_expandBtn = new QPushButton(tr("Expand All"), this);
  m_collapseBtn = new QPushButton(tr("Collapse All"), this);
  m_filterBtn = new QPushButton(tr("All/Deleted"), this);
  m_previewBtn = new QPushButton(tr("Preview (Enter)"), this);
  m_previewBtn->setVisible(false);
  m_restoreBtn = new QPushButton(tr("Restore (F5)"), this);
  m_quitBtn = new QPushButton(tr("Quit (F10)"), this);

  toolbarLayout->addWidget(m_markBtn);
  toolbarLayout->addWidget(m_unmarkBtn);
  toolbarLayout->addWidget(m_unmarkAllBtn);
  toolbarLayout->addWidget(m_expandBtn);
  toolbarLayout->addWidget(m_collapseBtn);
  toolbarLayout->addWidget(m_filterBtn);
  toolbarLayout->addWidget(m_previewBtn);
  toolbarLayout->addStretch();
  toolbarLayout->addWidget(m_restoreBtn);
  toolbarLayout->addWidget(m_quitBtn);
  mainLayout->addLayout(toolbarLayout);

  m_searchEdit = new QLineEdit(this);
  m_searchEdit->setPlaceholderText(tr("Filter files by name..."));
  m_searchEdit->setClearButtonEnabled(true);
  mainLayout->addWidget(m_searchEdit);

  m_statusBar = new QLabel(tr("Marked: 0 files (0 B)"), this);
  m_statusBar->setStyleSheet("background-color: #88C0D0; color: #2E3440; padding: 4px 8px; font-weight: bold;");
  mainLayout->addWidget(m_statusBar);

  setMinimumSize(640, 480);

  connect(m_markBtn, &QPushButton::clicked, this, &BrowserWidget::onMark);
  connect(m_unmarkBtn, &QPushButton::clicked, this, &BrowserWidget::onUnmark);
  connect(m_unmarkAllBtn, &QPushButton::clicked, this, &BrowserWidget::onUnmarkAll);
  connect(m_expandBtn, &QPushButton::clicked, this, &BrowserWidget::onExpandAll);
  connect(m_collapseBtn, &QPushButton::clicked, this, &BrowserWidget::onCollapseAll);
  connect(m_filterBtn, &QPushButton::clicked, this, &BrowserWidget::onToggleFilter);
  connect(m_previewBtn, &QPushButton::clicked, this, &BrowserWidget::onPreview);
  connect(m_restoreBtn, &QPushButton::clicked, this, &BrowserWidget::restoreRequested);
  connect(m_quitBtn, &QPushButton::clicked, this, &BrowserWidget::quitRequested);

  connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
    m_proxyModel->setFilterText(text);
    m_treeView->expandAll();
  });

  /*
     * Selection change handler: updates path display bar and toggles
     * the Preview button visibility based on file extension.
     *
     * DUPLICATION: The path-building from parent chain (lines 227-232)
     * duplicates the same logic in restore_orphan() (prestore.c) and
     * restore_file() (prestore.c) and FileTreeModel::nodePath().
     * All four locations walk a parent chain to build "/path/to/file".
     * Consider extracting to tree_get_path() in ptree.c.
     */
  connect(m_treeView->selectionModel(), &QItemSelectionModel::currentChanged, this,
          [this](const QModelIndex &current, const QModelIndex &) {
            if (!m_fileModel || !current.isValid()) {
              m_previewBtn->setVisible(false);
              return;
            }
            QModelIndex srcIdx = m_proxyModel->mapToSource(current);
            QVariant v = m_proxyModel->sourceModel()->data(srcIdx, FileNodeRole);
            if (v.isValid()) {
              file_node_t *node = (file_node_t *)v.value<quintptr>();
              if (node && node->name) {
                char pathBuf[4096];
                tree_get_path(node, m_fileModel->tree()->root, pathBuf, sizeof(pathBuf));
                m_pathDisplay->setText(QString::fromUtf8(pathBuf));

                const char *ext = strrchr(node->name, '.');
                bool show = ext && SignatureRegistry::isPreviewableImage(QString::fromLatin1(ext + 1));
                m_previewBtn->setVisible(show);
                return;
              }
            }
            m_pathDisplay->clear();
            m_previewBtn->setVisible(false);
          });

  setFocusProxy(m_treeView);
}

void BrowserWidget::applyTheme() {
  setStyleSheet(QStringLiteral("BrowserWidget { background-color: #2E3440; }") + Theme::globalStyleSheet());
}

void BrowserWidget::setSourceModelAndExpand(QAbstractItemModel *sourceModel) {
  m_proxyModel->setSourceModel(sourceModel);
  m_treeView->expandAll();
  if (m_treeView->header() && m_treeView->header()->count() >= 4) {
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_treeView->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
  }
}

void BrowserWidget::setModel(QAbstractItemModel *model) {
  m_fileModel = nullptr;
  setSourceModelAndExpand(model);
}

/*
 * setFileModel / resetModel: Both set the FileTreeModel pointer and rebuild
 * display. setFileModel uses toStandardModel() (rebuilds entire tree as
 * QStandardItemModel), while resetModel uses FileTreeModel directly as
 * QAbstractItemModel.
 *
 * DUPLICATION: These two functions are nearly identical. setFileModel is
 * used from MainWindow::showBrowser(), resetModel appears unused in current
 * code paths.
 */
void BrowserWidget::setFileModel(FileTreeModel *model) {
  m_fileModel = model;
  if (model && model->tree() && model->tree()->root) {
    setSourceModelAndExpand(model->toStandardModel(this));
  } else {
    setSourceModelAndExpand(nullptr);
  }
  if (model)
    connect(model, &FileTreeModel::markedChanged, this, &BrowserWidget::onMarkedChanged);
}

void BrowserWidget::resetModel(FileTreeModel *model) {
  m_fileModel = model;
  qDebug() << "resetModel: rowCount=" << (model ? model->rowCount(QModelIndex()) : -1);
  if (model) {
    setSourceModelAndExpand(model);
    connect(model, &FileTreeModel::markedChanged, this, &BrowserWidget::onMarkedChanged);
  }
}

FileTreeModel *BrowserWidget::model() const {
  return m_fileModel;
}

/*
 * onMark/onUnmark/markSelected: Three near-identical loops that iterate
 * selected rows, map proxy→source, extract file_node_t*, and set marked flag.
 * DUPLICATION: All three share the proxy→source→FileNodeRole→node→marked
 * pattern. Only difference: onMark sets marked=1, onUnmark sets marked=0,
 * markSelected does the same as onMark but is called externally (from
 * onRestoreFromBrowser). Consider a common applyToSelected(bool mark) helper.
 */
void BrowserWidget::applyToSelected(bool mark) {
  QModelIndexList sel = m_treeView->selectionModel()->selectedRows();
  if (sel.isEmpty() || !m_fileModel || !m_fileModel->tree())
    return;

  for (const QModelIndex &idx : sel) {
    QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
    QVariant v = m_proxyModel->sourceModel()->data(srcIdx, FileNodeRole);
    if (v.isValid()) {
      file_node_t *node = (file_node_t *)v.value<quintptr>();
      if (node && node->type == NODE_FILE)
        node->marked = mark ? 1 : 0;
    }
  }
  refreshFromFileModel();
}

void BrowserWidget::onMark() {
  applyToSelected(true);
}

void BrowserWidget::onUnmark() {
  applyToSelected(false);
}

void BrowserWidget::onUnmarkAll() {
  if (m_fileModel)
    m_fileModel->unmarkAll();
  refreshFromFileModel();
}

void BrowserWidget::onToggleMark(const QModelIndex &idx) {
  if (!m_fileModel || !m_fileModel->tree())
    return;
  QModelIndex srcIdx = m_proxyModel->mapToSource(idx);
  QVariant v = m_proxyModel->sourceModel()->data(srcIdx, FileNodeRole);
  if (v.isValid()) {
    file_node_t *node = (file_node_t *)v.value<quintptr>();
    if (node && node->type == NODE_FILE) {
      node->marked = node->marked ? 0 : 1;
      refreshFromFileModel();
    }
  }
}

/*
 * refreshFromFileModel: Rebuilds QStandardItemModel from the C tree and
 * sets it as the proxy source. Called after every mark/unmark operation.
 *
 * PERFORMANCE ISSUE: toStandardModel() traverses the ENTIRE C tree and
 * creates new QStandardItem allocations for every visible node. This is
 * O(n) in tree size and runs after every Space keystroke. With large trees
 * (millions of files from deep scan), this causes UI lag.
 *
 * The FileTreeModel QAbstractItemModel supports direct manipulation without
 * rebuilding (setMark + dataChanged signal), but is not used because
 * BrowserWidget uses toStandardModel() for displayed colors.
 * Consider using FileTreeModel directly and moving color logic to a
 * QStyledItemDelegate instead.
 */
void BrowserWidget::refreshFromFileModel() {
  if (!m_fileModel || !m_fileModel->tree())
    return;
  QStandardItemModel *sm = m_fileModel->toStandardModel(this);
  if (sm)
    setSourceModelAndExpand(sm);
  onMarkedChanged(m_fileModel->markedCount(), m_fileModel->markedTotalSize());
}

void BrowserWidget::markSelected() {
  applyToSelected(true);
}

void BrowserWidget::onExpandAll() {
  m_treeView->expandAll();
}

void BrowserWidget::onCollapseAll() {
  m_treeView->collapseAll();
}

void BrowserWidget::onSearch() {
  m_searchEdit->setFocus();
  m_searchEdit->selectAll();
}

void BrowserWidget::onToggleFilter() {
  if (!m_fileModel)
    return;
  m_fileModel->setShowDeletedOnly(!m_fileModel->showDeletedOnly());
  if (m_fileModel->showDeletedOnly())
    m_filterBtn->setText(tr("Deleted Only"));
  else
    m_filterBtn->setText(tr("All Files"));
  if (m_fileModel->tree() && m_fileModel->tree()->root) {
    setSourceModelAndExpand(m_fileModel->toStandardModel(this));
  }
}

void BrowserWidget::onMarkedChanged(int count, uint64_t totalSize) {
  m_statusBar->setText(tr("Marked: %1 files (%2)").arg(count).arg(formatSize(totalSize)));
}

void BrowserWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    if (!m_searchEdit->text().isEmpty()) {
      m_searchEdit->clear();
      return;
    }
  }
  if (event->key() == Qt::Key_Space) {
    QModelIndexList sel = m_treeView->selectionModel()->selectedRows();
    if (!sel.isEmpty())
      onToggleMark(sel.first());
    return;
  }
  if (event->key() == Qt::Key_F5) {
    emit restoreRequested();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    QModelIndexList sel = m_treeView->selectionModel()->selectedRows();
    if (!sel.isEmpty()) {
      QModelIndex proxyIdx = sel.first();
      QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
      emit previewRequested(srcIdx);
    }
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_F10) {
    emit quitRequested();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Slash) {
    onSearch();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Tab) {
    onToggleFilter();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void BrowserWidget::onPreview() {
  QModelIndexList sel = m_treeView->selectionModel()->selectedRows();
  if (sel.isEmpty())
    return;
  QModelIndex proxyIdx = sel.first();
  QModelIndex srcIdx = m_proxyModel->mapToSource(proxyIdx);
  emit previewRequested(srcIdx);
}

void BrowserWidget::setStatusMessage(const QString &msg) {
  m_statusBar->setText(msg);
}
