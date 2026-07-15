/*
    
    File: formatselectordialog.cpp

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
#include "formatselectordialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFont>
#include <QSet>
#include <algorithm>
#include "common/theme.hpp"

FormatSelectorDialog::FormatSelectorDialog(QWidget *parent)
    : QDialog(parent),
      m_filterEdit(nullptr),
      m_listWidget(nullptr),
      m_selectAllBtn(nullptr),
      m_deselectAllBtn(nullptr),
      m_okBtn(nullptr),
      m_photoBtn(nullptr),
      m_docBtn(nullptr),
      m_archiveBtn(nullptr),
      m_audioBtn(nullptr)
{
    setupUi();
    applyTheme();
}

void FormatSelectorDialog::setupUi()
{
    setWindowTitle(tr("Select File Formats to Recover"));
    setModal(true);
    setMinimumSize(640, 480);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(8);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Filter extensions..."));
    mainLayout->addWidget(m_filterEdit);

    m_listWidget = new QListWidget(this);
    mainLayout->addWidget(m_listWidget, 1);

    QHBoxLayout *topBtnLayout = new QHBoxLayout();
    topBtnLayout->setSpacing(8);
    m_selectAllBtn = new QPushButton(tr("Select All"), this);
    m_deselectAllBtn = new QPushButton(tr("Deselect All"), this);
    m_okBtn = new QPushButton(tr("OK"), this);
    m_okBtn->setDefault(true);
    topBtnLayout->addWidget(m_selectAllBtn);
    topBtnLayout->addWidget(m_deselectAllBtn);
    topBtnLayout->addStretch();
    topBtnLayout->addWidget(m_okBtn);
    mainLayout->addLayout(topBtnLayout);

    QGroupBox *quickGroup = new QGroupBox(tr("Quick Select"), this);
    QHBoxLayout *quickLayout = new QHBoxLayout(quickGroup);
    quickLayout->setSpacing(8);
    m_photoBtn = new QPushButton(tr("Photos/Videos"), this);
    m_docBtn = new QPushButton(tr("Documents"), this);
    m_archiveBtn = new QPushButton(tr("Archives"), this);
    m_audioBtn = new QPushButton(tr("Audio"), this);
    quickLayout->addWidget(m_photoBtn);
    quickLayout->addWidget(m_docBtn);
    quickLayout->addWidget(m_archiveBtn);
    quickLayout->addWidget(m_audioBtn);
    mainLayout->addWidget(quickGroup);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &FormatSelectorDialog::onFilterChanged);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onSelectAll);
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onDeselectAll);
    connect(m_okBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onOk);
    connect(m_photoBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onQuickPhoto);
    connect(m_docBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onQuickDocuments);
    connect(m_archiveBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onQuickArchives);
    connect(m_audioBtn, &QPushButton::clicked, this, &FormatSelectorDialog::onQuickAudio);
}

void FormatSelectorDialog::applyTheme()
{
    setStyleSheet(QStringLiteral("FormatSelectorDialog { background-color: #2E3440; }")
        + Theme::globalStyleSheet()
        + Theme::listWidgetStyle()
        + "QPushButton:default { border: 2px solid #88C0D0; }");
}

void FormatSelectorDialog::setExtensions(const QVector<SignatureInfo> &sigs)
{
    m_signatures = sigs;
    std::sort(m_signatures.begin(), m_signatures.end(),
              [](const SignatureInfo &a, const SignatureInfo &b) {
                  return a.priority < b.priority;
              });

    m_listWidget->clear();
    for (int i = 0; i < m_signatures.size(); i++) {
        const SignatureInfo &s = m_signatures[i];
        QString label = QString("[%1] %2 - %3")
            .arg(s.extension.toUpper())
            .arg(s.description)
            .arg(s.maxFilesize > 0 ? QString::number(s.maxFilesize) : QString("unlimited"));
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(s.enabledByDefault ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, s.extension);
        m_listWidget->addItem(item);
    }
}

QStringList FormatSelectorDialog::selectedExtensions() const
{
    QStringList result;
    int count = m_listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item && item->checkState() == Qt::Checked)
            result.append(item->data(Qt::UserRole).toString());
    }
    return result;
}

void FormatSelectorDialog::onSelectAll()
{
    int count = m_listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item)
            item->setCheckState(Qt::Checked);
    }
}

void FormatSelectorDialog::onDeselectAll()
{
    int count = m_listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (item)
            item->setCheckState(Qt::Unchecked);
    }
}

void FormatSelectorDialog::onFilterChanged(const QString &text)
{
    int count = m_listWidget->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = m_listWidget->item(i);
        if (!item)
            continue;
        bool match = item->text().contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

static void selectFormats(const QSet<QString> &exts, QListWidget *list)
{
    int count = list->count();
    for (int i = 0; i < count; i++) {
        QListWidgetItem *item = list->item(i);
        if (!item)
            continue;
        item->setCheckState(
            exts.contains(item->data(Qt::UserRole).toString()) ? Qt::Checked : Qt::Unchecked);
    }
}

void FormatSelectorDialog::onQuickPhoto()
{
    QSet<QString> exts = {"jpg","jpeg","mov","png","gif","bmp","avi","mp4","mkv","webp","svg",
                          "mts","m2ts","3gp","wmv","flv","ico","tif","tiff","psd","cr2","nef",
                          "orf","dng","raw","arw","heic","jp2"};
    selectFormats(exts, m_listWidget);
}

void FormatSelectorDialog::onQuickDocuments()
{
    QSet<QString> exts = {"pdf","doc","docx","xls","xlsx","ppt","pptx","txt","rtf","csv",
                          "html","xml","json","odt","ods","odp","parquet"};
    selectFormats(exts, m_listWidget);
}

void FormatSelectorDialog::onQuickArchives()
{
    QSet<QString> exts = {"zip","rar","7z","tar","gz","bz2","xz","lz","lzh","cab",
                          "iso","arj","ace"};
    selectFormats(exts, m_listWidget);
}

void FormatSelectorDialog::onQuickAudio()
{
    QSet<QString> exts = {"mp3","wav","flac","ogg","wma","m4a","aac","aiff","au",
                          "mid","ape","wv"};
    selectFormats(exts, m_listWidget);
}

void FormatSelectorDialog::onOk()
{
    emit accepted();
    accept();
}
