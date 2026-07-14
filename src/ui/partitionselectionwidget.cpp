/*
    
    File: partitionselectionwidget.cpp

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
#include "partitionselectionwidget.hpp"
#include <QHeaderView>
#include <QFont>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "common/theme.hpp"

PartitionSelectionWidget::PartitionSelectionWidget(QWidget *parent)
    : QWidget(parent),
      m_titleLabel(nullptr),
      m_tableView(nullptr),
      m_model(nullptr),
      m_scanBtn(nullptr),
      m_carveBtn(nullptr),
      m_deepScanBtn(nullptr),
      m_backupBtn(nullptr),
      m_restoreBtn(nullptr),
      m_backBtn(nullptr)
{
    setupUi();
    applyTheme();
}

void PartitionSelectionWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    m_titleLabel = new QLabel(tr("Select Partition"), this);
    QFont titleFont;
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    m_model = new QStandardItemModel(0, 5, this);
    m_model->setHorizontalHeaderLabels({tr("Name"), tr("Filesystem"), tr("Size"), tr("Type"), tr("")});

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
    mainLayout->addWidget(m_tableView);

    QHBoxLayout *btnLayout1 = new QHBoxLayout();
    btnLayout1->setSpacing(12);
    btnLayout1->addStretch();

    m_scanBtn = new QPushButton(tr("Scan"), this);
    m_carveBtn = new QPushButton(tr("Carve"), this);
    m_deepScanBtn = new QPushButton(tr("Deep FS Scan"), this);

    btnLayout1->addWidget(m_scanBtn);
    btnLayout1->addWidget(m_carveBtn);
    btnLayout1->addWidget(m_deepScanBtn);
    btnLayout1->addStretch();
    mainLayout->addLayout(btnLayout1);

    QHBoxLayout *btnLayout2 = new QHBoxLayout();
    btnLayout2->setSpacing(12);
    btnLayout2->addStretch();

    m_backupBtn = new QPushButton(tr("Backup"), this);
    m_restoreBtn = new QPushButton(tr("Restore"), this);
    m_backBtn = new QPushButton(tr("Back"), this);

    m_scanBtn->setEnabled(false);
    m_carveBtn->setEnabled(false);
    m_deepScanBtn->setEnabled(false);
    m_backupBtn->setEnabled(false);
    m_restoreBtn->setEnabled(false);

    btnLayout2->addWidget(m_backupBtn);
    btnLayout2->addWidget(m_restoreBtn);
    btnLayout2->addWidget(m_backBtn);
    btnLayout2->addStretch();
    mainLayout->addLayout(btnLayout2);

    setMinimumSize(640, 480);

    connect(m_scanBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::scanRequested);
    connect(m_carveBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::carveRequested);
    connect(m_deepScanBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::deepScanRequested);
    connect(m_backupBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::backupRequested);
    connect(m_restoreBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::restoreRequested);
    connect(m_backBtn, &QPushButton::clicked, this, &PartitionSelectionWidget::backRequested);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        int idx = selectedPartitionIndex();
        bool partValid = idx >= 0;
        bool diskOpsValid = (idx == -2) || (idx >= 0);
        m_scanBtn->setEnabled(partValid);
        m_carveBtn->setEnabled(diskOpsValid);
        m_deepScanBtn->setEnabled(diskOpsValid);
        m_backupBtn->setEnabled(partValid);
        m_restoreBtn->setEnabled(partValid);
    });
}

void PartitionSelectionWidget::applyTheme()
{
    setStyleSheet(QStringLiteral("PartitionSelectionWidget { background-color: #2E3440; }")
        + Theme::globalStyleSheet());
}

void PartitionSelectionWidget::setPartitions(const QVector<PartitionInfo> &parts)
{
    m_partitions = parts;
    m_model->removeRows(0, m_model->rowCount());

    QList<QStandardItem*> wholeRow;
    uint64_t totalSize = 0;
    for (int i = 0; i < parts.size(); i++)
        totalSize += parts[i].partSize;
    {
        QStandardItem *item;
        item = new QStandardItem("Whole disk");
        item->setFont(QFont(item->font().family(), -1, QFont::Bold));
        wholeRow.append(item);
        item = new QStandardItem(formatSize(totalSize));
        wholeRow.append(item);
        item = new QStandardItem("");
        wholeRow.append(item);
        item = new QStandardItem("Raw");
        wholeRow.append(item);
        item = new QStandardItem(QString("-"));
        wholeRow.append(item);
    }
    for (auto *item : wholeRow)
        item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_model->appendRow(wholeRow);

    for (int i = 0; i < parts.size(); i++) {
        const PartitionInfo &p = parts[i];
        QList<QStandardItem*> row;
        row.append(new QStandardItem(p.partname.isEmpty() ? QString("Partition %1").arg(i + 1) : p.partname));
        row.append(new QStandardItem(p.encrypted ? "LUKS" : (p.fsname.isEmpty() ? "-" : p.fsname)));
        row.append(new QStandardItem(formatSize(p.partSize)));

        QString typeStr = p.typenameStr;
        if (typeStr.isEmpty() && p.encrypted)
            typeStr = "LUKS";
        if (typeStr.isEmpty() && p.partTypeI386 > 0)
            typeStr = QString("0x%1").arg(p.partTypeI386, 2, 16, QChar('0')).toUpper();
        if (typeStr.isEmpty())
            typeStr = p.info.isEmpty() ? "Unknown" : p.info;
        row.append(new QStandardItem(typeStr));

        row.append(new QStandardItem(p.encrypted ? "(encrypted)" : ""));

        QColor fg = p.encrypted ? QColor("#B48EAD") : QColor("#ECEFF4");
        for (auto *item : row) {
            item->setForeground(fg);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        }
        m_model->appendRow(row);
    }
}

int PartitionSelectionWidget::selectedPartitionIndex() const
{
    QModelIndexList sel = m_tableView->selectionModel()->selectedRows();
    if (sel.isEmpty())
        return -1;
    int row = sel.first().row();
    if (row == 0)
        return -2;
    return row - 1;
}
