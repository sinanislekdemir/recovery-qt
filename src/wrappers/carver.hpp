#ifndef PHOTOREC_CARVER_HPP
#define PHOTOREC_CARVER_HPP

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

class Carver : public WorkerBase {
    Q_OBJECT
public:
    explicit Carver(QObject *parent = nullptr);

    void start(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
               const QString &extFilter, bool deepScan);

signals:
    void progressUpdated(uint64_t scannedBytes, uint64_t totalBytes,
                         unsigned int fileCount, uint64_t recoveredSize);
    void finished(int totalFiles);
    void errorOccurred(const QString &message);
};

#endif
