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
