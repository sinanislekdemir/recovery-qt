/*
    
    File: aboutdialog.cpp

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
#include "aboutdialog.hpp"
#include <QVBoxLayout>
#include <QFont>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi();
    applyTheme();
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About recovery-qt"));
    setMinimumSize(640, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 32, 32, 32);
    mainLayout->setSpacing(12);

    QLabel *titleLabel = new QLabel(tr("recovery-qt v7.3"), this);
    QFont titleFont;
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(tr("Data Recovery & File Carving Tool"), this);
    QFont subFont;
    subFont.setPointSize(14);
    subtitleLabel->setFont(subFont);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(subtitleLabel);

    mainLayout->addSpacing(20);

    QLabel *line1 = new QLabel(
        tr("Derived from PhotoRec by Christophe GRENIER\n"
           "Qt6 port and extensions by Sinan Islekdemir"), this);
    line1->setAlignment(Qt::AlignCenter);
    line1->setWordWrap(true);
    mainLayout->addWidget(line1);

    mainLayout->addSpacing(8);

    QLabel *line2 = new QLabel(tr("Linux only"), this);
    line2->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(line2);

    QLabel *buildLabel = new QLabel(tr("Build: ") + QString::fromLatin1(BUILD_UTC), this);
    buildLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(buildLabel);

    mainLayout->addSpacing(16);

    QLabel *featuresHeader = new QLabel(tr("Feature Additions"), this);
    QFont fhFont;
    fhFont.setPointSize(11);
    fhFont.setBold(true);
    featuresHeader->setFont(fhFont);
    featuresHeader->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(featuresHeader);

    QLabel *line3 = new QLabel(
        tr("• Qt6 graphical interface with Nord dark theme\n"
           "• Modular scan/carve/restore architecture with background threads\n"
           "• Filesystem scan: directory walk + MFT/INDX lookup\n"
           "• Deep FS scan: FAT free clusters, EXT inode table, NTFS INDX\n"
           "• Raw file carving: 300+ signatures, sector-aligned\n"
           "• NTFS $ATTRIBUTE_LIST processing for multi-record MFT files\n"
           "• Selective per-file mark and restore\n"
           "• LUKS1/LUKS2 encrypted volume decryption\n"
           "• Filesystem backup/restore to .dsk index files\n"
           "• Disk image support: .img, .dd, .vhd, .vmdk, .e01, .qcow2, .iso\n"
           "• Format selector with quick-select categories\n"
           "• Thread-safe progress reporting with cancel support\n\n"
           "Licensed under GNU General Public License v2+"), this);
    line3->setAlignment(Qt::AlignCenter);
    line3->setWordWrap(true);
    mainLayout->addWidget(line3);

    mainLayout->addStretch();

    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setMinimumWidth(120);
    okBtn->setDefault(true);
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void AboutDialog::applyTheme()
{
    setStyleSheet(QString(
        "AboutDialog { background-color: #2E3440; }"
        "QLabel { color: #ECEFF4; }"
        "QPushButton {"
        "  background-color: #434C5E;"
        "  color: #ECEFF4;"
        "  border: 1px solid #4C566A;"
        "  padding: 8px 24px;"
        "}"
        "QPushButton:hover { background-color: #4C566A; }"
        "QPushButton:default {"
        "  border: 2px solid #88C0D0;"
        "}"
    ));
}
