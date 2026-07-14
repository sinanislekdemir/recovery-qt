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
