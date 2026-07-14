/*
    
    File: progressdialog.cpp

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
#include "progressdialog.hpp"
#include <QFont>
#include <QScrollArea>
#include <QApplication>
#include <QTimer>
#include "common/theme.hpp"

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent),
      m_titleLabel(nullptr),
      m_progressBar(nullptr),
      m_statusLabel(nullptr),
      m_fileNameLabel(nullptr),
      m_detailLabel(nullptr),
      m_cancelBtn(nullptr),
      m_finished(false)
{
    setMinimumSize(560, 280);
    setModal(true);
    setupUi();
    applyTheme();
}

void ProgressDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(8);

    m_titleLabel = new QLabel(this);
    QFont titleFont;
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFixedHeight(24);
    mainLayout->addWidget(m_progressBar);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_statusLabel);

    m_fileNameLabel = new QLabel(this);
    m_fileNameLabel->setWordWrap(true);
    QFont fnFont;
    fnFont.setItalic(true);
    m_fileNameLabel->setFont(fnFont);
    m_fileNameLabel->hide();
    mainLayout->addWidget(m_fileNameLabel);

    m_detailLabel = new QLabel(this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setVisible(false);
    mainLayout->addWidget(m_detailLabel);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setMinimumWidth(100);
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_cancelBtn, &QPushButton::clicked, this, &ProgressDialog::cancelled);
}

void ProgressDialog::applyTheme()
{
    setStyleSheet(QStringLiteral("ProgressDialog { background-color: #2E3440; }")
        + Theme::globalStyleSheet());
}

void ProgressDialog::setWindowTitle(const QString &title)
{
    m_titleLabel->setText(title);
    QDialog::setWindowTitle(title);
}

void ProgressDialog::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void ProgressDialog::setFileName(const QString &text)
{
    m_fileNameLabel->setText(text);
}

void ProgressDialog::showFileName(bool visible)
{
    m_fileNameLabel->setVisible(visible);
}

void ProgressDialog::setDetailText(const QString &text)
{
    m_detailLabel->setText(text);
    m_detailLabel->setVisible(!text.isEmpty());
}

void ProgressDialog::showCancelButton(bool visible)
{
    m_cancelBtn->setVisible(visible);
}

void ProgressDialog::setIndeterminate(bool on)
{
    if (on) {
        m_progressBar->setRange(0, 0);
    } else {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(0);
    }
}

void ProgressDialog::setFinished(bool ok, const QString &summary)
{
    if (!m_progressBar || !m_cancelBtn || !m_statusLabel)
        return;

    m_finished = true;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(ok ? 100 : 100);
    m_cancelBtn->setText(tr("Close"));
    m_cancelBtn->setEnabled(true);
    disconnect(m_cancelBtn, &QPushButton::clicked, this, &ProgressDialog::cancelled);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::accept);

    if (ok) {
        m_progressBar->setStyleSheet(
            "QProgressBar { border: 1px solid #4C566A; background: #3B4252;"
            " text-align: center; color: #ECEFF4; border-radius: 3px; }"
            "QProgressBar::chunk { background: #A3BE8C; border-radius: 2px; }");
    } else {
        m_progressBar->setStyleSheet(
            "QProgressBar { border: 1px solid #4C566A; background: #3B4252;"
            " text-align: center; color: #ECEFF4; border-radius: 3px; }"
            "QProgressBar::chunk { background: #BF616A; border-radius: 2px; }");
    }
    m_statusLabel->setText(summary);

    QTimer::singleShot(0, this, &QDialog::accept);
}

void ProgressDialog::updateProgress(int percent, const QString &status)
{
    if (m_finished) return;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    m_progressBar->setValue(percent);
    m_statusLabel->setText(status);
    QApplication::processEvents();
}

void ProgressDialog::addLogEntry(const QString &file, bool ok)
{
    Q_UNUSED(file);
    Q_UNUSED(ok);
    // detail label handles log accumulation externally
}
