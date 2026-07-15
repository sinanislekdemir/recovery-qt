/*
    
    File: partitionselectionwidget.hpp

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
#ifndef PARTITIONSELECTIONWIDGET_HPP
#define PARTITIONSELECTIONWIDGET_HPP

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVector>
#include "wrappers/partitionlist.hpp"
#include "common/format_utils.hpp"

class PartitionSelectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit PartitionSelectionWidget(QWidget *parent = nullptr);

    void setPartitions(const QVector<PartitionInfo> &parts);
    int selectedPartitionIndex() const;

signals:
    void scanRequested();
    void carveRequested();
    void deepScanRequested();
    void backupRequested();
    void restoreRequested();
    void backRequested();

private:
    void setupUi();
    void applyTheme();

    QLabel *m_titleLabel;
    QTableView *m_tableView;
    QStandardItemModel *m_model;
    QPushButton *m_scanBtn;
    QPushButton *m_carveBtn;
    QPushButton *m_deepScanBtn;
    QPushButton *m_backupBtn;
    QPushButton *m_restoreBtn;
    QPushButton *m_backBtn;
    QVector<PartitionInfo> m_partitions;
};

#endif // PARTITIONSELECTIONWIDGET_HPP
