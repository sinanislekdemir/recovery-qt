#ifndef PHOTOREC_SCANNER_HPP
#define PHOTOREC_SCANNER_HPP

#include <QObject>
#include <QString>
#include <cstdint>
#include "wrappers/workerbase.hpp"

class ProgressCallback;

#ifdef __cplusplus
extern "C" {
#endif
#include "photorec_nc.h"
#include "types.h"
#ifdef __cplusplus
}
#endif

class Scanner : public WorkerBase {
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);

    void start(scan_tree_t *tree, disk_t *disk, const partition_t *partition, int deep = 1);

signals:
    void progressUpdated(uint64_t deletedCount, uint64_t totalCount, const QString &path);
    void indxProgressUpdated(const QString &msg, uint64_t current, uint64_t total, uint64_t found);
    void finished(int deletedFiles);
    void errorOccurred(const QString &message);

private:
    scan_tree_t *m_tree;
    disk_t *m_disk;
    const partition_t *m_partition;
};

#endif
