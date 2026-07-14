#ifndef PARTITIONSELECTIONWIDGET_HPP
#define PARTITIONSELECTIONWIDGET_HPP

#include <QWidget>
#include <QTableView>
#include <QLabel>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVector>
#include "wrappers/partitionlist.hpp"
#include "common/format_utils.hpp"

class PartitionSelectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit PartitionSelectionWidget(QWidget *parent = nullptr);

    void setPartitions(const QVector<PartitionInfo> &parts);
    int selectedPartitionIndex() const;

signals:
    void scanRequested();
    void carveRequested();
    void deepScanRequested();
    void backupRequested();
    void restoreRequested();
    void backRequested();

private:
    void setupUi();
    void applyTheme();

    QLabel *m_titleLabel;
    QTableView *m_tableView;
    QStandardItemModel *m_model;
    QPushButton *m_scanBtn;
    QPushButton *m_carveBtn;
    QPushButton *m_deepScanBtn;
    QPushButton *m_backupBtn;
    QPushButton *m_restoreBtn;
    QPushButton *m_backBtn;
    QVector<PartitionInfo> m_partitions;
};

#endif // PARTITIONSELECTIONWIDGET_HPP
