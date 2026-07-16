/*
    
    File: formatselectordialog.hpp

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
#ifndef FORMATSELECTORDIALOG_HPP
#define FORMATSELECTORDIALOG_HPP

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVector>
#include "wrappers/signatureregistry.hpp"

class FormatSelectorDialog : public QDialog {
  Q_OBJECT
public:
  explicit FormatSelectorDialog(QWidget *parent = nullptr);

  void setExtensions(const QVector<SignatureInfo> &sigs);
  QStringList selectedExtensions() const;

signals:
  void accepted();

private slots:
  void onSelectAll();
  void onDeselectAll();
  void onFilterChanged(const QString &text);
  void onQuickPhoto();
  void onQuickDocuments();
  void onQuickArchives();
  void onQuickAudio();
  void onOk();

private:
  void setupUi();
  void applyTheme();

  QLineEdit *m_filterEdit;
  QListWidget *m_listWidget;
  QPushButton *m_selectAllBtn;
  QPushButton *m_deselectAllBtn;
  QPushButton *m_okBtn;
  QPushButton *m_photoBtn;
  QPushButton *m_docBtn;
  QPushButton *m_archiveBtn;
  QPushButton *m_audioBtn;
  QVector<SignatureInfo> m_signatures;
};

#endif // FORMATSELECTORDIALOG_HPP
