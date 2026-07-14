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
