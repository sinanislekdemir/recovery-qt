#ifndef PHOTOREC_RESTORER_HPP
#define PHOTOREC_RESTORER_HPP

#include <QObject>
#include <QString>
#include <atomic>
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

class Restorer : public WorkerBase {
    Q_OBJECT
public:
    explicit Restorer(QObject *parent = nullptr);

    void start(scan_tree_t *tree, disk_t *disk, const partition_t *partition,
               const QString &destDir, file_node_t *onlyNode = nullptr);

signals:
    void progressUpdated(int pct, const QString &currentFile, int total, int done);
    void fileRestored(const QString &path, bool ok);
    void finished(uint64_t okCount, uint64_t failCount);
    void errorOccurred(const QString &message);

private:
    std::atomic<uint64_t> m_okCount;
    std::atomic<uint64_t> m_failCount;
};

#endif
