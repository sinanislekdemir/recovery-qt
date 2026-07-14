#ifndef DISKSELECTIONWIDGET_HPP
#define DISKSELECTIONWIDGET_HPP

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include "wrappers/disk.hpp"
#include "common/format_utils.hpp"
#include "common/theme.hpp"

class DiskSelectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit DiskSelectionWidget(QWidget *parent = nullptr);

    void refreshDisks();

signals:
    void diskSelected(const Disk &disk);
    void quitRequested();

private slots:
    void onSelectionChanged();
    void onRefresh();
    void onProceed();
    void onOpenImage();

private:
    void setupUi();
    void applyTheme();

    QLabel *m_titleLabel;
    QTableView *m_tableView;
    QStandardItemModel *m_model;
    QPushButton *m_refreshBtn;
    QPushButton *m_openImageBtn;
    QPushButton *m_proceedBtn;
    QPushButton *m_quitBtn;
    QVector<Disk> m_disks;
};

#endif // DISKSELECTIONWIDGET_HPP
