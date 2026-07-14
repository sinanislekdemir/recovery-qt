/*
    
    File: imagepreviewdialog.cpp

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
#include "imagepreviewdialog.hpp"
#include <QVBoxLayout>
#include <QToolBar>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QShowEvent>
#include <QImage>

ImagePreviewDialog::ImagePreviewDialog(QWidget *parent)
    : QDialog(parent)
    , m_imageLabel(nullptr)
    , m_infoLabel(nullptr)
    , m_toolbar(nullptr)
    , m_zoomInAction(nullptr)
    , m_zoomOutAction(nullptr)
    , m_origAction(nullptr)
    , m_fitAction(nullptr)
    , m_scroll(nullptr)
    , m_zoomFactor(1.0)
    , m_fitToWindow(true)
{
    setWindowTitle(tr("Image Preview"));
    setMinimumSize(400, 300);
    resize(900, 650);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(
        "QToolBar { background-color: #3B4252; border: none; spacing: 4px; padding: 2px 4px; }"
    );

    m_zoomOutAction = m_toolbar->addAction(tr("-"));
    m_zoomOutAction->setToolTip(tr("Zoom Out (Ctrl+-)"));
    connect(m_zoomOutAction, &QAction::triggered, this, &ImagePreviewDialog::onZoomOut);

    m_zoomInAction = m_toolbar->addAction(tr("+"));
    m_zoomInAction->setToolTip(tr("Zoom In (Ctrl++)"));
    connect(m_zoomInAction, &QAction::triggered, this, &ImagePreviewDialog::onZoomIn);

    m_origAction = m_toolbar->addAction(tr("1:1"));
    m_origAction->setToolTip(tr("Original Size"));
    connect(m_origAction, &QAction::triggered, this, &ImagePreviewDialog::onOriginalSize);

    m_fitAction = m_toolbar->addAction(tr("Fit"));
    m_fitAction->setToolTip(tr("Fit to Window (F)"));
    m_fitAction->setCheckable(true);
    m_fitAction->setChecked(true);
    connect(m_fitAction, &QAction::triggered, this, &ImagePreviewDialog::onFitToWindow);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("color: #ECEFF4; padding: 2px 8px; font-size: 11pt;");
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);
    m_toolbar->addWidget(m_infoLabel);

    mainLayout->addWidget(m_toolbar);

    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(false);
    m_scroll->setAlignment(Qt::AlignCenter);
    m_scroll->setStyleSheet(
        "QScrollArea { background-color: #2E3440; border: none; }");

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_scroll->setWidget(m_imageLabel);

    mainLayout->addWidget(m_scroll);
}

bool ImagePreviewDialog::loadFromData(const QByteArray &data)
{
    if (data.isEmpty())
        return false;

    QImage image;
    if (!image.loadFromData(data))
        return false;

    m_originalPixmap = QPixmap::fromImage(image);
    if (m_originalPixmap.isNull())
        return false;

    m_fitToWindow = true;
    m_fitAction->setChecked(true);
    m_zoomFactor = 1.0;

    m_infoLabel->setText(QString("%1 x %2  |  100%")
        .arg(m_originalPixmap.width())
        .arg(m_originalPixmap.height()));

    return true;
}

void ImagePreviewDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_fitToWindow && !m_originalPixmap.isNull())
        applyZoom();
}

void ImagePreviewDialog::applyZoom()
{
    if (m_originalPixmap.isNull())
        return;

    QPixmap scaled;
    if (m_fitToWindow) {
        QSize viewSize = m_scroll->viewport()->size();
        if (viewSize.isEmpty())
            return;
        scaled = m_originalPixmap.scaled(viewSize, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
        m_zoomFactor = (double)scaled.width() / m_originalPixmap.width();
    } else {
        QSize sz = m_originalPixmap.size() * m_zoomFactor;
        scaled = m_originalPixmap.scaled(sz, Qt::KeepAspectRatio,
            Qt::SmoothTransformation);
    }

    m_imageLabel->setPixmap(scaled);
    m_imageLabel->resize(scaled.size());

    m_infoLabel->setText(QString("%1 x %2  |  %3%")
        .arg(m_originalPixmap.width())
        .arg(m_originalPixmap.height())
        .arg((int)(m_zoomFactor * 100)));
}

void ImagePreviewDialog::onZoomIn()
{
    m_fitToWindow = false;
    m_fitAction->setChecked(false);
    m_zoomFactor *= 1.25;
    if (m_zoomFactor > 8.0)
        m_zoomFactor = 8.0;
    applyZoom();
}

void ImagePreviewDialog::onZoomOut()
{
    m_fitToWindow = false;
    m_fitAction->setChecked(false);
    m_zoomFactor /= 1.25;
    if (m_zoomFactor < 0.05)
        m_zoomFactor = 0.05;
    applyZoom();
}

void ImagePreviewDialog::onFitToWindow()
{
    m_fitToWindow = !m_fitToWindow;
    m_fitAction->setChecked(m_fitToWindow);
    if (m_fitToWindow)
        applyZoom();
}

void ImagePreviewDialog::onOriginalSize()
{
    m_fitToWindow = false;
    m_fitAction->setChecked(false);
    m_zoomFactor = 1.0;
    applyZoom();
}

void ImagePreviewDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        event->accept();
        close();
        return;
    }
    if (event->key() == Qt::Key_F) {
        onFitToWindow();
        return;
    }
    if (event->matches(QKeySequence::ZoomIn)) {
        onZoomIn();
        return;
    }
    if (event->matches(QKeySequence::ZoomOut)) {
        onZoomOut();
        return;
    }
    QDialog::keyPressEvent(event);
}

void ImagePreviewDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    if (m_fitToWindow)
        applyZoom();
}

void ImagePreviewDialog::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        int delta = event->angleDelta().y();
        if (delta > 0)
            onZoomIn();
        else if (delta < 0)
            onZoomOut();
        event->accept();
        return;
    }
    QDialog::wheelEvent(event);
}