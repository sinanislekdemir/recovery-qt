/*
    
    File: imagepreviewdialog.hpp

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
#ifndef IMAGEPREVIEWDIALOG_HPP
#define IMAGEPREVIEWDIALOG_HPP

#include <QDialog>
#include <QLabel>
#include <QToolBar>
#include <QScrollArea>
#include <QByteArray>
#include <QPixmap>

class ImagePreviewDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImagePreviewDialog(QWidget *parent = nullptr);

    bool loadFromData(const QByteArray &data);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onZoomIn();
    void onZoomOut();
    void onFitToWindow();
    void onOriginalSize();

private:
    void applyZoom();

    QLabel *m_imageLabel;
    QLabel *m_infoLabel;
    QToolBar *m_toolbar;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_origAction;
    QAction *m_fitAction;
    QScrollArea *m_scroll;
    QPixmap m_originalPixmap;
    double m_zoomFactor;
    bool m_fitToWindow;
};

#endif // IMAGEPREVIEWDIALOG_HPP