/*
    
    File: diskselectionwidget.cpp

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
#include "diskselectionwidget.hpp"
#include <QHeaderView>
#include <QMessageBox>
#include <QFont>
#include <QFileDialog>
#include <QApplication>

DiskSelectionWidget::DiskSelectionWidget(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(nullptr),
      m_tableView(nullptr),
      m_model(nullptr),
      m_refreshBtn(nullptr),
      m_openImageBtn(nullptr),
      m_proceedBtn(nullptr),
      m_quitBtn(nullptr)
{
    setupUi();
    applyTheme();
    refreshDisks();
}

void DiskSelectionWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    m_titleLabel = new QLabel(tr("Select Disk"), this);
    QFont titleFont;
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHorizontalHeaderLabels({tr("Device"), tr("Size"), tr("Perm"), tr("Model")});

    m_tableView = new QTableView(this);
    m_tableView->setModel(m_model);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableView->setShowGrid(false);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->setMinimumHeight(250);
    m_tableView->setColumnWidth(0, 240);
    m_tableView->setColumnWidth(1, 140);
    m_tableView->setColumnWidth(2, 60);
    mainLayout->addWidget(m_tableView);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);
    btnLayout->addStretch();

    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    m_openImageBtn = new QPushButton(tr("Open Image"), this);
    m_proceedBtn = new QPushButton(tr("Proceed"), this);
    m_quitBtn = new QPushButton(tr("Quit"), this);

    m_proceedBtn->setEnabled(false);

    btnLayout->addWidget(m_refreshBtn);
    btnLayout->addWidget(m_openImageBtn);
    btnLayout->addWidget(m_proceedBtn);
    btnLayout->addWidget(m_quitBtn);
    mainLayout->addLayout(btnLayout);

    setMinimumSize(640, 480);

    connect(m_refreshBtn, &QPushButton::clicked, this, &DiskSelectionWidget::onRefresh);
    connect(m_openImageBtn, &QPushButton::clicked, this, &DiskSelectionWidget::onOpenImage);
    connect(m_proceedBtn, &QPushButton::clicked, this, &DiskSelectionWidget::onProceed);
    connect(m_quitBtn, &QPushButton::clicked, this, &DiskSelectionWidget::quitRequested);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &DiskSelectionWidget::onSelectionChanged);
}

void DiskSelectionWidget::applyTheme()
{
    setStyleSheet(QStringLiteral("DiskSelectionWidget { background-color: #2E3440; }")
        + Theme::globalStyleSheet());
}

void DiskSelectionWidget::refreshDisks()
{
    m_model->removeRows(0, m_model->rowCount());
    m_disks = Disk::enumerateSystem();

    for (int i = 0; i < m_disks.size(); i++) {
        const Disk &d = m_disks[i];
        QList<QStandardItem*> row;
        row.append(new QStandardItem(d.device()));
        row.append(new QStandardItem(formatSize(d.totalSize())));
        row.append(new QStandardItem(d.accessMode() == 0 ? "RO" : "RW"));
        row.append(new QStandardItem(d.model().isEmpty() ? d.description() : d.model()));
        for (auto *item : row)
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_model->appendRow(row);
    }

    if (m_disks.isEmpty()) {
        QMessageBox::warning(this, tr("No Disks"),
            tr("No physical disks detected. Please run with appropriate permissions."));
    }
}

void DiskSelectionWidget::onSelectionChanged()
{
    m_proceedBtn->setEnabled(m_tableView->selectionModel()->hasSelection());
}

void DiskSelectionWidget::onRefresh()
{
    refreshDisks();
}

void DiskSelectionWidget::onProceed()
{
    QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return;
    int row = sel.first().row();
    if (row >= 0 && row < m_disks.size())
        emit diskSelected(m_disks[row]);
}

void DiskSelectionWidget::onOpenImage()
{
    QString path = QFileDialog::getOpenFileName(this,
        tr("Open Disk Image"),
        QString(),
        tr("Disk Images (*.img *.dd *.raw *.dsk *.vhd *.vmdk *.vdi *.e01 *.s01 *.aff *.hdd *.qcow *.qcow2 *.bin *.iso);;All Files (*)"));
    if (path.isEmpty())
        return;

    Disk img = Disk::openDevice(path, TESTDISK_O_RDONLY);
    if (!img.isValid()) {
        QMessageBox::warning(this, tr("Open Failed"),
            tr("Could not open or read the disk image:\n%1").arg(path));
        return;
    }
    emit diskSelected(img);
}
