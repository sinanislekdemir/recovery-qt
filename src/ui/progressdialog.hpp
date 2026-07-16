/*
    
    File: progressdialog.hpp

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
#ifndef PROGRESSDIALOG_HPP
#define PROGRESSDIALOG_HPP

#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QString>
#include <QVector>
#include <QPair>
#include <cstdint>

class ProgressDialog : public QDialog {
  Q_OBJECT
public:
  explicit ProgressDialog(QWidget *parent = nullptr);

  void setWindowTitle(const QString &title);
  void setStatusText(const QString &text);
  void setFileName(const QString &text);
  void showFileName(bool visible);
  void setDetailText(const QString &text);
  void showCancelButton(bool visible);
  void setIndeterminate(bool on);
  void setFinished(bool ok, const QString &summary);

public slots:
  void updateProgress(int percent, const QString &status);
  void addLogEntry(const QString &file, bool ok);

signals:
  void cancelled();

private:
  void setupUi();
  void applyTheme();

  QLabel *m_titleLabel;
  QProgressBar *m_progressBar;
  QLabel *m_statusLabel;
  QLabel *m_fileNameLabel;
  QLabel *m_detailLabel;
  QPushButton *m_cancelBtn;
  bool m_finished;
};

#endif // PROGRESSDIALOG_HPP
