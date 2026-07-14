#ifndef THEME_HPP
#define THEME_HPP

#include <QString>

namespace Theme {

inline QString globalStyleSheet()
{
    return QStringLiteral(
        "QTableView, QTreeView {"
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
        "}"
    );
}

inline QString listWidgetStyle()
{
    return QStringLiteral(
        "QListWidget {"
        "  background-color: #3B4252;"
        "  color: #ECEFF4;"
        "  border: 1px solid #4C566A;"
        "}"
        "QListWidget::item { padding: 3px 6px; }"
    );
}

} // namespace Theme

#endif
