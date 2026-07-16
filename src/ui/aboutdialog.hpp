/*
    
    File: aboutdialog.hpp

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
#ifndef ABOUTDIALOG_HPP
#define ABOUTDIALOG_HPP

#include <QDialog>
#include <QLabel>
#include <QPushButton>

class AboutDialog : public QDialog {
  Q_OBJECT
public:
  explicit AboutDialog(QWidget *parent = nullptr);

private:
  void setupUi();
  void applyTheme();
};

#endif // ABOUTDIALOG_HPP
