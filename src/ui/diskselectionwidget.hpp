/*
    
    File: diskselectionwidget.hpp

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
#ifndef DISKSELECTIONWIDGET_HPP
#define DISKSELECTIONWIDGET_HPP

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include "wrappers/disk.hpp"
#include "common/format_utils.hpp"
#include "common/theme.hpp"

class DiskSelectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiskSelectionWidget(QWidget *parent = nullptr);

    void refreshDisks();

signals:
    void diskSelected(const Disk &disk);
    void quitRequested();

private slots:
    void onSelectionChanged();
    void onRefresh();
    void onProceed();
    void onOpenImage();

private:
    void setupUi();
    void applyTheme();

    QLabel *m_titleLabel;
    QTableView *m_tableView;
    QStandardItemModel *m_model;
    QPushButton *m_refreshBtn;
    QPushButton *m_openImageBtn;
    QPushButton *m_proceedBtn;
    QPushButton *m_quitBtn;
    QVector<Disk> m_disks;
};

#endif // DISKSELECTIONWIDGET_HPP
