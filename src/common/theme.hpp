/*
    
    File: theme.hpp

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
#ifndef THEME_HPP
#define THEME_HPP

#include <QString>

namespace Theme {

inline QString globalStyleSheet() {
  return QStringLiteral("QTableView, QTreeView {"
                        "  background-color: #2E3440;"
                        "  alternate-background-color: #3B4252;"
                        "  color: #ECEFF4;"
                        "  gridline-color: #4C566A;"
                        "  border: 1px solid #4C566A;"
                        "  selection-background-color: #88C0D0;"
                        "  selection-color: #2E3440;"
                        "}"
                        "QTableView::item { padding: 4px 8px; }"
                        "QHeaderView::section {"
                        "  background-color: #88C0D0;"
                        "  color: #2E3440;"
                        "  font-weight: bold;"
                        "  padding: 4px 8px;"
                        "  border: 1px solid #4C566A;"
                        "}"
                        "QPushButton {"
                        "  background-color: #434C5E;"
                        "  color: #ECEFF4;"
                        "  border: 1px solid #4C566A;"
                        "  padding: 5px 12px;"
                        "}"
                        "QPushButton:hover { background-color: #4C566A; }"
                        "QPushButton:disabled { color: #5E81AC; }"
                        "QProgressBar {"
                        "  background-color: #3B4252;"
                        "  border: 1px solid #4C566A;"
                        "  text-align: center;"
                        "  color: #ECEFF4;"
                        "}"
                        "QProgressBar::chunk { background-color: #A3BE8C; }"
                        "QLineEdit {"
                        "  background-color: #3B4252;"
                        "  color: #ECEFF4;"
                        "  border: 1px solid #4C566A;"
                        "  padding: 4px;"
                        "}"
                        "QLabel { color: #ECEFF4; }"
                        "QGroupBox {"
                        "  color: #88C0D0;"
                        "  border: 1px solid #4C566A;"
                        "  margin-top: 12px;"
                        "  padding-top: 12px;"
                        "}"
                        "QGroupBox::title {"
                        "  subcontrol-origin: margin;"
                        "  left: 10px;"
                        "  padding: 0 5px;"
                        "  color: #88C0D0;"
                        "}");
}

inline QString listWidgetStyle() {
  return QStringLiteral("QListWidget {"
                        "  background-color: #3B4252;"
                        "  color: #ECEFF4;"
                        "  border: 1px solid #4C566A;"
                        "}"
                        "QListWidget::item { padding: 3px 6px; }");
}

} // namespace Theme

#endif
