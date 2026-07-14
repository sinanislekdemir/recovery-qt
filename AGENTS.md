# AGENTS.md — recovery-qt Development Guide

## Project Overview

recovery-qt is a standalone fork of testdisk-7.3-WIP providing a Qt6-based
deleted-file browser, selective restore tool, raw file carver, filesystem
backup/restore, and LUKS decryption support.

**Binary**: `build/recovery-qt` (Linux-only, Qt6 Widgets interface)

### recovery-qt vs. Original PhotoRec

PhotoRec is a raw block-level file carver with ncurses TUI. recovery-qt takes
a fundamentally different approach: **filesystem-aware directory scanning**
combined with raw carving, wrapped in a Qt6 GUI for selective recovery.

| Feature | PhotoRec | recovery-qt |
|---------|----------|-------------|
| FS-aware directory tree browsing | No | Yes — FAT/NTFS/EXT2/exFAT with original names, dirs, mtimes |
| Deleted file detection via FS | No | Yes — directory flags, MFT records, INDX scans |
| NTFS MFT orphan scan | No | Yes — reads MFT, infers extensions, creates /ORPHAN/ |
| Deep FS scan (free clusters) | No | Yes — byte-by-byte residual entry scan |
| Selective file restore | No (bulk only) | Yes — tick individual files, Space to mark |
| Image preview before restore | No | Yes — Enter, 20 formats, reads raw bytes from disk |
| LUKS encrypted volumes | No | Yes — cryptsetup+losetup, password dialog, async decrypt |
| Filesystem backup/restore (.dsk) | No | Yes — index backup, backup_modified flag |
| Qt6 dark-theme GUI | No (ncurses/old Qt4) | Yes — Nord theme, QTreeView, filter-as-you-type |
| Multi-pass carving | Yes (5-8 passes) | Single pass |
| Brute-force fragment reassembly | Yes | No |
| File validation callbacks | Yes (data_check/file_check) | Simple header check only |
| Session save/resume | Yes (photorec.ses) | No |
| DFXML forensic output | Yes | No |
| FAT unformat mode | Yes (expert) | No |
| fidentify companion tool | Yes | No |

**Key insight**: recovery-qt is a **deleted-file recovery tool** for live
filesystems. PhotoRec is a **forensic file carver** for destroyed filesystems.
They complement each other rather than competing.

## Build

```bash
cmake -S . -B build && cmake --build build -- -j$(nproc)
```

Binary is at `build/recovery-qt`. Links against Qt6::Core, Qt6::Widgets,
libntfs-3g, libext2fs, zlib, libuuid.

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                   Qt6 UI Layer (C++17)                        │
│  MainWindow │ BrowserWidget │ DiskSelection │ ProgressDialogs │
│  ImagePreviewDialog │ FormatSelectorDialog │ LUKSPasswordDialog│
└────────────────────┬─────────────────────────────────────────┘
                     │ signals/slots
┌────────────────────┴─────────────────────────────────────────┐
│                C++ Wrapper Layer (RAII, QThread)              │
│  WorkerBase (thread lifecycle, beginOperation helper)         │
│  Scanner │ Carver │ Restorer │ SimpleWorker (extend WorkerBase)│
│  Disk │ PartitionList │ FileTreeModel │ SignatureRegistry     │
│  ProgressCallback (emitToInstance template bridge)            │
└────────────────────┬─────────────────────────────────────────┘
                     │ extern "C" + global function pointers
┌────────────────────┴─────────────────────────────────────────┐
│               C Core (unchanged, C90)                         │
│  pscanner.c  — FS directory traversal + deep scan            │
│  pcarver.c   — Raw signature carving (300+ formats)          │
│  prestore.c  — File restore (FS-copy + raw sector reads)     │
│  ptree.c     — file_node_t tree + tree_get_path()            │
│  pbackup.c   — FS index backup/restore (.dsk files)          │
│  luksnc.c    — LUKS decrypt via cryptsetup+losetup           │
│  hdaccess.c  — Disk enumeration, disk_t with pread/pwrite    │
│  progress_cb.h/c — Global function pointers for callbacks    │
└──────────────────────────────────────────────────────────────┘
```

## Complete Data Flow: All Operations

### 1. SCAN (FS-aware directory traversal)

```
User clicks "Scan"
  → MainWindow::onScanRequested()
    → runScannerOperation(false)                    [mainwindow.cpp]
    1. Gets partition, handles LUKS decrypt
    2. tree_new() → allocates scan_tree_t
    3. Creates ProgressDialog, wires Scanner::progressUpdated
       and Scanner::indxProgressUpdated signals
    4. scanner->start(tree, disk, part, deep=0)     [scanner.cpp]
       → beginOperation() gets PC, resets, stores connections
       → runs scanner_run() in QThread               [pscanner.c:697]
          ├─ scanner_init_fs() — detects FAT/NTFS/ext2/exFAT
          ├─ scan_dir_recursive() — recursive directory walk
          │   Each entry: flags deleted files, calls tree_add_path()
          │   Reports: g_scanner_progress(deletedCount, totalCount, path)
          ├─ NTFS MFT orphan scan: reads MFT for deleted files
          │   Uses carver_check_header() to rename by extension
          │   Creates /ORPHAN/ entries (node->orphan=1)
          └─ Falls back to carver_run() for non-FAT/non-NTFS partitions
    5. dlg.exec() blocks UI; signals update progress
    6. On finished: uninstallAllCallbacks(), showBrowser()
    7. showBrowser() → m_fileModel->setTree(m_scanTree) → stack switches to browser
```

### 2. DEEP SCAN (FS traversal + free cluster byte-by-byte)

Same as SCAN but calls `runScannerOperation(true)` → scanner->start with deep=1.
The runScannerOperation helper deduplicates the 95% shared code.

### 3. CARVE (Raw signature scanning)

```
User clicks "Carve"
  → MainWindow::onCarveRequested()                  [mainwindow.cpp]
    1. Optional LUKS decrypt (Yes/No prompt)
    2. FormatSelectorDialog — user picks file types
    3. tree_new(), ProgressDialog wired to Carver::progressUpdated
    4. carver->start(tree, disk, part, extFilter, deepScan=false) [carver.cpp]
       → beginOperation() gets PC, resets, stores connections
       → runs carver_run() in QThread               [pcarver.c:918]
    5. dlg.exec() → uninstallAllCallbacks() → showBrowser()
```

### 4. RESTORE (Copy marked files to local disk)

```
User presses F5 in BrowserWidget
  → BrowserWidget::keyPressEvent(F5)
    → emit restoreRequested()
      → MainWindow::onRestoreFromBrowser()          [mainwindow.cpp]
        1. Auto-marks selected if none marked (via applyToSelected(true))
        2. QFileDialog for destination directory
        3. Creates ProgressDialog, wires restoreProgress and fileRestored signals
        4. restorer->start(tree, disk, part, destDir, onlyNode=nullptr) [restorer.cpp]
           → beginOperation() gets PC, resets, stores connections
           → runs restore_files() in QThread        [prestore.c:566]
              ├─ init_fs_for_restore() — table-driven FS detection
              ├─ mkdir_recursive(destDir)
              ├─ tree_count_marked() → total
              ├─ g_restorer_progress(0, "", total, 0)
              ├─ restore_node_recursive()
              │   For each marked file: restore_file() dispatches to
              │   restore_orphan() (raw sectors) or dir_data->copy_file() (FS driver)
              └─ g_restorer_progress(100, "", total, total)
        5. dlg.exec() → uninstallAllCallbacks()
```

### 5. PREVIEW (Image file preview — reads raw bytes from disk)

```
User presses Enter on image file in BrowserWidget
  → BrowserWidget::keyPressEvent(Enter)
    → emit previewRequested(srcIdx)
      → MainWindow::onPreviewRequested()             [mainwindow.cpp]
        1. Extracts file_node_t* from model role
        2. Checks SignatureRegistry::isPreviewableImage(ext)
        3. Calls read_file_bytes()                   [prestore.c:406]
           THREE DATA PATHS:
           ├─ PATH 1: node->cluster_list exists (EXT/backup)
           │   Reads clusters sequentially, 64KB chunks, up to 64 MB
           ├─ PATH 2: node->orphan with first_sector (carved files)
           │   Reads from part_offset + first_sector, max 64 MB
           └─ PATH 3: Normal FS file
               Uses memory_capture: set_memory_capture() → restore_file_node()
               → copy_file() writes to memory → clear_memory_capture()
               WARNING: does a full FS driver restore just to capture bytes
        4. Creates ImagePreviewDialog, loads QImage from raw bytes
```

### 6. BACKUP/RESTORE (Filesystem index .dsk files)

Now uses SimpleWorker (WorkerBase subclass) for consistent thread lifecycle.
Same beginOperation()→startThread→emit finished pattern as Scanner/Carver/Restorer.

## Thread Model & Lifecycle

```
┌─ Main Thread ───────────────────────────────────────────────────┐
│  MainWindow::onXxxRequested()                                    │
│    ├─ Create ProgressDialog
│    ├─ Connect signals (save QMetaObject::Connection handles)
│    ├─ worker->start(...)
│    │   └─ beginOperation()  ← checks m_running, gets PC, resets
│    │       └─ startThread(lambda)
│    │           ├─ m_running = true
│    │           ├─ QThread::create(lambda) → starts worker thread
│    │           └─ return (non-blocking)
│    ├─ dlg.exec()  ← BLOCKS MAIN THREAD
│    │   (ProgressDialog is modal, processes events normally)
│    │   (worker signals update dialog via QueuedConnection)
│    └─ dlg.exec() returns
│       └─ uninstallAllCallbacks() ← SAFE: worker thread has exited
├──────────────────────────────────────────────────────────────────┤
│  Worker Thread (runs during dlg.exec())                          │
│    ├─ install*Callbacks()  — sets C globals + s_*Instance
│    ├─ C core function runs (scanner_run/carver_run/restore_files)
│    │   ├─ C code calls g_*_progress fn pointer
│    │   │   → emitToInstance(s_*Instance, [=]{ emit signal; })
│    │   │   → signal delivered to main thread via QueuedConnection
│    │   └─ C code checks g_*_cancel fn pointer for cancellation
│    ├─ emit finished(...)  — connected to dlg handler
│    └─ m_running = false
└──────────────────────────────────────────────────────────────────┘
```

| Component | Lifecycle Owner | Notes |
|-----------|----------------|-------|
| Scanner/Carver/Restorer/SimpleWorker | MainWindow (member, never deleted) | Reused per operation |
| scan_tree_t | MainWindow (m_scanTree) | tree_free() before tree_new() each operation |
| disk_t* | Disk (shared_ptr<DiskData>) | Raw pointer borrowed, not owned by worker |
| partition_t* | PartitionList | Invalidation only on re-detection |
| ProgressCallback | MainWindow (m_progressCb) | Singleton, shared across all workers |
| QThread | WorkerBase::m_thread | Created new per operation, deleted on cleanup |

## Key Data Structures

### file_node_t (`src/photorec_nc.h`)
```c
typedef struct file_node {
    struct td_list_head siblings;      // linked list in parent's children
    struct td_list_head children;      // list head for child nodes
    struct file_node *parent;          // NULL for root
    char *name;                        // filename (no path component)
    uint64_t size;                     // file size in bytes
    uint64_t first_sector;             // start sector (orphan/carved files)
    uint64_t num_sectors;              // sector count (orphan files)
    time_t mtime;                      // modification timestamp
    unsigned int sector_size;
    unsigned int type      : 1;        // NODE_FILE=0, NODE_DIR=1
    unsigned int marked    : 1;        // user selected for restore
    unsigned int deleted   : 1;        // deleted on source filesystem
    unsigned int expanded  : 1;        // tree expansion state (UI)
    unsigned int orphan    : 1;        // carved file, no FS metadata
    unsigned int backup_restored : 1;  // from backup .dsk file
    unsigned int backup_modified : 1;  // file changed since backup
    uint64_t *cluster_list;           // EXT FS / backup cluster chain
    uint32_t  cluster_count;
    uint32_t  cluster_size;
} file_node_t;
```

### FileNodeRole Protocol

`FileNodeRole = Qt::UserRole + 100`

Every model item stores `file_node_t*` as `quintptr` via `QVariant::fromValue((quintptr)node)`.
This is the bridge between Qt's model/view system and the C tree. Used by:
- BrowserWidget mark/unmark loops (now unified via `applyToSelected(bool mark)`)
- BrowserWidget selection change handler (path display via `tree_get_path()`)
- MainWindow::onPreviewRequested (extracts node for read_file_bytes)

## Progress Callback Bridge

### C → C++ → Qt Signal Chain

```
C Core (worker thread)               C++ static bridge              Qt Main Thread
=====================                ==================             ==============

g_restorer_progress(pct, name,   →   cRestoreProgress()         →   ProgressCallback::
  total, done)                         emitToInstance(                emit restoreProgress()
    [prestore.c]                       s_restoreInstance,
                                       [=]{ emit signal; })
```

All static C bridge functions now use the `emitToInstance<Fn>` template helper
in progresscallback.hpp to eliminate the duplicated `if (!instance) return;
QMetaObject::invokeMethod(...)` pattern across 6 callback functions.

## Project Files

### Qt UI Layer (`src/ui/`)
| File | Purpose |
|------|---------|
| `mainwindow.hpp/cpp` | Operation orchestrator. Scan/deep-scan merged into runScannerOperation(). |
| `browserwidget.hpp/cpp` | QTreeView file browser. Mark/unmark unified via applyToSelected(). Path display uses tree_get_path(). |
| `progressdialog.hpp/cpp` | Modal progress dialog used by ALL operations. setFinished() auto-closes. |
| `imagepreviewdialog.hpp/cpp` | Zoomable image viewer: QScrollArea+QLabel, mouse wheel zoom. |
| `diskselectionwidget.hpp/cpp` | Disk table: Device/Size/Perm/Model. |
| `partitionselectionwidget.hpp/cpp` | Partition table + action buttons. |
| `formatselectordialog.hpp/cpp` | Checkable file format selector. |
| `lukspassworddialog.hpp/cpp` | LUKS passphrase prompt. |

### C++ Wrappers (`src/wrappers/`)
| File | Purpose |
|------|---------|
| `workerbase.hpp/cpp` | Base class: QThread lifecycle, beginOperation() helper, connection management. |
| `simpleworker.hpp/cpp` | Lightweight WorkerBase subclass for simple C function calls (backup/restore). |
| `scanner.hpp/cpp` | Wraps `scanner_run()`. Uses beginOperation() + DirectConnection. |
| `carver.hpp/cpp` | Wraps `carver_run()`. Uses beginOperation(). |
| `restorer.hpp/cpp` | Wraps `restore_files()`/`restore_file_node()`. Atomic ok/fail counters. |
| `filetreemodel.hpp/cpp` | QAbstractItemModel wrapping scan_tree_t. nodePath() uses tree_get_path(). |
| `progresscallback.hpp/cpp` | Thread-safe C↔Qt bridge. emitToInstance<T> template helper. |
| `disk.hpp/cpp` | RAII disk wrapper. |
| `partitionlist.hpp/cpp` | Partition table detection. |
| `luksmanager.hpp/cpp` | LUKS detection + async decrypt. |
| `signatureregistry.hpp/cpp` | Wraps array_file_enable[]. Hardcoded previewable image extension list. |

### C Core (`src/`)
| File | Purpose |
|------|---------|
| `photorec_nc.h` | `file_node_t`, `scan_tree_t`, public C API including `tree_get_path()` |
| `ptree.c` | Tree ops: tree_new, tree_add_path, tree_find_path, tree_get_path, tree_count_marked, tree_free |
| `pscanner.c` | FS scanner: directory traversal, MFT orphans, deep FAT/NTFS/EXT scan |
| `pcarver.c` | Raw carver: header/footer detection, 300+ signatures |
| `prestore.c` | Restore + preview with table-driven init_fs_for_restore(). Paths built via tree_get_path(). |
| `pbackup.c` | FS index backup/restore to .dsk files |
| `luksnc.c` | LUKS decrypt via cryptsetup+losetup |
| `progress_cb.h/c` | Global function pointer extern declarations |

## Refactored Duplications (now fixed)

| Issue | Fix | Impact |
|-------|-----|--------|
| onScanRequested/onDeepScanRequested 95% duplicate | Merged into `runScannerOperation(bool deep)` | ~70 lines removed |
| onMark/onUnmark/markSelected identical loops | Unified into `applyToSelected(bool mark)` | ~35 lines removed |
| 4x parent-chain path building | Added `tree_get_path()` in ptree.c, used everywhere | ~40 lines removed |
| Scanner/Carver/Restorer::start() boilerplate | Added `beginOperation()` helper to WorkerBase | ~25 lines removed |
| ProgressCallback 6x static bridge functions | Added `emitToInstance<T>` template helper | ~30 lines simplified |
| init_fs_for_restore() 7x identical blocks | Table-driven with 3-phase approach | ~50 lines removed |
| Backup/Restore raw QThread::create() | Created SimpleWorker (WorkerBase subclass) | Consistent lifecycle |

## Remaining Refactoring Opportunities

### PERFORMANCE: toStandardModel() full tree rebuild
Every mark/unmark filters through `refreshFromFileModel()` → `toStandardModel()`
which does a complete O(n) tree traversal creating new QStandardItem objects.
For trees with millions of entries, use FileTreeModel directly with a
QStyledItemDelegate that reads color via FileTreeModel::colorForNode().

### MEDIUM: read_file_bytes() Path 3 memory capture
The memory_capture mechanism runs a full FS driver restore just to capture
bytes for preview. Consider a dedicated "read bytes from FS file" path.

## Important Gotchas

- **LUKS return value**: `check_LUKS()` returns `0` when LUKS IS detected,
  `1` on failure. Inverted from typical boolean convention.

- **C90 constraints**: All declarations at top of block, no compound literals,
  no `//` comments in C files. Only `/* */` block comments.

- **carver_init()/carver_deinit()**: Must be paired. Leaking causes memory
  corruption on the next carver run.

- **tree_add_path() semantics**: Existing dir + deleted entry → only `deleted`
  flag updated. Dir metadata is NOT overwritten. File metadata IS overwritten.

- **Progress callback safety**: Save `QMetaObject::Connection` handles and
  disconnect them BEFORE calling `dlg.accept()`. Pending queued signals can
  fire on a destroyed dialog. Never call `uninstallAllCallbacks()` from
  the worker thread.

- **Disk lifecycle**: `Disk` uses `shared_ptr<DiskData>`, does NOT auto-free
  `disk_t*`. Do not call `disk_free()` on pointers obtained from Disk.

- **Logging**: Log file is `recovery-qt.log` in CWD. Use `log_info()`/
  `log_error()`/`log_warning()` in C code, `qDebug()` in C++ wrappers.

- **memory_capture (preview)**: Global state. Single caller at a time
  (safe since preview blocks UI).

- **WorkerBase::beginOperation()**: Returns ProgressCallback* after reset,
  or nullptr if m_running. Always check the return value.

## Color Theme (Nord)

| Color | Hex | Usage |
|-------|-----|-------|
| Background | `#2E3440` | Window/table backgrounds |
| Alt background | `#3B4252` | Alternating rows, menu bars |
| Text | `#ECEFF4` | Primary text |
| Cyan | `#88C0D0` | Headers, selection, active |
| Green | `#A3BE8C` | Marked files, success, progress fill |
| Red | `#BF616A` | Deleted files, errors |
| Yellow | `#EBCB8B` | Warnings |
| Blue | `#5E81AC` | Dim/secondary info |
| Magenta | `#B48EAD` | LUKS, orphan, encrypted |

## Key Bindings (BrowserWidget)

| Key | Action |
|-----|--------|
| Space | Toggle mark on selected file |
| F5 | Restore marked files |
| Enter/Return | Preview image file (if previewable extension) |
| F10 | Quit to disk selection |
| / | Focus search bar |
| Tab | Toggle All Files / Deleted Only filter |
| Escape | Clear search filter |
