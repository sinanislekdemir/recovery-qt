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
