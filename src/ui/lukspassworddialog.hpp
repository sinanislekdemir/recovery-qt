/*
    
    File: lukspassworddialog.hpp

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
#ifndef LUKSPASSWORDDIALOG_HPP
#define LUKSPASSWORDDIALOG_HPP

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class LUKSPasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit LUKSPasswordDialog(QWidget *parent = nullptr);

    QString password() const;

private:
    void setupUi();
    void applyTheme();

    QLabel *m_infoLabel;
    QLineEdit *m_passwordEdit;
    QPushButton *m_showPwdBtn;
    QPushButton *m_decryptBtn;
    QPushButton *m_cancelBtn;
};

#endif // LUKSPASSWORDDIALOG_HPP
