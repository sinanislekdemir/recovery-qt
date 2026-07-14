# AGENTS.md — recovery-qt Development Guide

## Project Overview

recovery-qt is a standalone fork of testdisk-7.3-WIP providing a Qt6-based
deleted-file browser, selective restore tool, raw file carver, filesystem
backup/restore, and LUKS decryption support. It no longer includes the
original `testdisk`, `photorec`, `fidentify`, or `qphotorec` binaries —
only `recovery-qt`.

**Binary**: `build/recovery-qt` (Linux-only, Qt6 Widgets interface)

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
└────────────────────┬─────────────────────────────────────────┘
                     │ signals/slots
┌────────────────────┴─────────────────────────────────────────┐
│                C++ Wrapper Layer (RAII, QThread)              │
│  Disk │ PartitionList │ Scanner │ Carver │ Restorer │ LUKS   │
│  FileTreeModel │ SignatureRegistry │ ProgressCallback        │
└────────────────────┬─────────────────────────────────────────┘
                     │ extern "C"
┌────────────────────┴─────────────────────────────────────────┐
│               C Core (unchanged, C90)                         │
│  hdaccess │ partgpt │ fat_dir │ filegen │ ~350 file_*.c      │
│  ptree │ pscanner │ pcarver │ prestore │ luksnc │ pbackup    │
└──────────────────────────────────────────────────────────────┘
```

## Project Files

### Qt UI Layer (`src/ui/`)
| File | Purpose |
|------|---------|
| `mainwindow.hpp/cpp` | QStackedWidget orchestrator: disk→partition→scan/carve/browse |
| `diskselectionwidget.hpp/cpp` | Disk table with Device/Size/Perm/Model columns |
| `partitionselectionwidget.hpp/cpp` | Partition list with Scan/Carve/Deep Scan/Backup/Restore |
| `browserwidget.hpp/cpp` | QTreeView file browser + mark/restore/search toolbar |
| `progressdialog.hpp/cpp` | Unified progress dialog for all operations |
| `formatselectordialog.hpp/cpp` | Checkable file format selector with quick-select categories |
| `lukspassworddialog.hpp/cpp` | LUKS passphrase prompt |
| `aboutdialog.hpp/cpp` | Attribution dialog |

### C++ Wrappers (`src/wrappers/`)
| File | Purpose |
|------|---------|
| `workerbase.hpp/cpp` | Base class for async workers: QThread lifecycle, progress connection mgmt |
| `disk.hpp/cpp` | RAII wrapper around `disk_t`, disk enumeration, sector I/O |
| `partitionlist.hpp/cpp` | Partition table detection, `PartitionInfo` struct |
| `filetreemodel.hpp/cpp` | `QAbstractItemModel` wrapping `file_node_t`/`scan_tree_t` |
| `progresscallback.hpp/cpp` | Thread-safe C↔Qt progress bridge via `QMetaObject::invokeMethod` |
| `scanner.hpp/cpp` | `Scanner` worker: runs `scanner_run()` in QThread (extends WorkerBase) |
| `carver.hpp/cpp` | `Carver` worker: runs `carver_run()` in QThread (extends WorkerBase) |
| `restorer.hpp/cpp` | `Restorer` worker: runs `restore_files()` in QThread (extends WorkerBase) |
| `luksmanager.hpp/cpp` | LUKS detection, decrypt via `cryptsetup`+`losetup` |
| `signatureregistry.hpp/cpp` | File format registry wrapping `array_file_enable[]` |

### Shared Utilities (`src/common/`)
| File | Purpose |
|------|---------|
| `format_utils.hpp` | Single `formatSize(bytes)` function for human-readable byte display |
| `theme.hpp` | Centralized Nord dark theme CSS via `Theme::globalStyleSheet()` |

### C Core (preserved from original, `src/`)
| File | Purpose |
|------|---------|
| `photorec_nc.h` | `file_node_t`, `scan_tree_t`, public C API |
| `ptree.c` | Tree construction, `tree_add_path()`, `tree_count_marked()` |
| `pscanner.c` | FS scanner: directories, FAT deep scan, NTFS MFT, EXT inode |
| `pcarver.c` | Raw file carving: header/footer detection, 300+ signatures |
| `prestore.c` | File restore (FS-specific copy + raw sector reads) |
| `pbackup.c` | Backup/restore of live filesystems to `.dsk` files |
| `luksnc.c` | `luks_open()`/`luks_close()` via cryptsetup |
| `filegen.c` | Signature registration, `init_file_stats()` |
| `hdaccess.c` | Disk enumeration, `disk_t` with `pread`/`pwrite` function pointers |
| `progress_cb.h/c` | Callback function pointers set by Qt wrappers |

### Excluded from build
`intrfn.c`, `pbrowser.c`, `phncmain.c`, `pbanner.c`, `askloc.c`, `hiddenn.c`,
`chgarchn.c`, `geometryn.c`, `nodisk.c` — all ncurses-dependent, replaced by Qt.

## Thread Model

| Operation | Thread | Progress Reporting |
|-----------|--------|--------------------|
| Scanner | QThread | `g_scanner_progress` callback → `QMetaObject::invokeMethod` → Qt signal |
| Carver | QThread | `g_carver_progress` callback → `QMetaObject::invokeMethod` → Qt signal |
| Restorer | QThread | `g_restorer_progress` callback → `QMetaObject::invokeMethod` → Qt signal |
| Backup/Restore | QThread | Status bar message, window disabled during operation |

All C core operations run in background threads. Progress callbacks bridge to
the Qt main thread via `QMetaObject::invokeMethod` with `Qt::QueuedConnection`.
The main thread never blocks — all long-running work is asynchronous.

## Progress Callback Mechanism

C files call through global function pointers defined in `progress_cb.h`:
```c
extern carver_progress_fn g_carver_progress;   // set by Carver worker
extern progress_cancel_fn g_carver_cancel;
extern scanner_progress_fn g_scanner_progress; // set by Scanner worker
extern restorer_progress_fn g_restorer_progress; // set by Restorer worker
```

These are installed by `ProgressCallback::install*Callbacks()` before each
operation and uninstalled after. The static C bridge functions use
`QMetaObject::invokeMethod` to safely emit Qt signals from worker threads.

## Color Theme (Nord)

Applied via Qt stylesheet in `MainWindow::applyTheme()`:

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
| Magenta | `#B48EAD` | LUKS/encryption |

## Key Data Structures

### file_node_t (photorec_nc.h)
```c
typedef struct file_node {
    struct td_list_head siblings;   // linked list in parent dir
    struct td_list_head children;   // child list head
    struct file_node *parent;
    char *name;
    uint64_t size;
    uint64_t first_sector;
    uint64_t num_sectors;
    time_t mtime;
    unsigned int sector_size;
    unsigned int type    : 1;       // NODE_FILE=0, NODE_DIR=1
    unsigned int marked  : 1;
    unsigned int deleted : 1;
    unsigned int expanded : 1;
    unsigned int orphan  : 1;       // carved file, no FS metadata
    unsigned int backup_restored : 1;
    unsigned int backup_modified : 1;
    uint64_t *cluster_list;
    uint32_t  cluster_count;
    uint32_t  cluster_size;
} file_node_t;
```

### FileTreeModel (wrappers/filetreemodel.hpp)
Wraps `scan_tree_t` as `QAbstractItemModel`. Uses `file_node_t*` as
`QModelIndex::internalPointer()`. Columns: Name, Size, Modified, Status.
Custom roles: `MarkedRole`, `DeletedRole`, `OrphanRole`, `FileSizeRole`.

## User Flow

```
main() → DiskSelectionWidget → PartitionSelectionWidget
                                       │
                        ┌──────────────┼──────────────┐
                     "Scan"         "Carve"       "Backup/Restore"
                        │              │                │
                  Scanner(QThread) Carver(QThread)   QThread
                        │              │                │
                  scanner_run()  carver_run()   backup_*()
                        │              │                │
                        └──────┬───────┘                │
                               │                        │
                        BrowserWidget ←─────────────────┘
                               │
                         F5 → Restorer(QThread)
                               │
                         restore_files()
```

## Carve Mode

- Scans entire partition by raw sector, matching file headers against 300+ signatures
- Sector-aligned (carve): checks 1 of 512 byte positions
- Deep FS Scan: byte-by-byte scanning of free clusters (INDX, FAT, EXT inode)
- Minimum signature length: 3 bytes (was 4, caused JPEG to be skipped)
- LUKS: prompts "Decrypt LUKS volume before carving? (y/N)"
- Files stored under `/ORPHAN/` in tree, marked with `node->orphan = 1`

### File-End Detection (pcarver.c)

| Priority | Trigger | Mechanism |
|----------|---------|-----------|
| 1 | Footer match | `carver_check_footer()` — IEND/FF D9/EOCD/0x3B/%%EOF/OggS/size-from-header |
| 2 | Next file header | Closes current file at match offset |
| 3 | Constant sector + gap | All-0x00 or all-0xFF sector hit while file exceeds gap limit |
| 4 | End of partition | Extends to partition boundary |

### Same-Format Proximity Filter
- Known file end: same-format matches before footer are skipped (embedded objects)
- Unknown file end: same-format matches within 64 KB of start are skipped

## Browser (QTreeView)

- Uses QStandardItemModel populated from `file_node_t` tree via `toStandardModel()`
- Each item stores `file_node_t*` pointer in `FileNodeRole` (Qt::UserRole + 100) for mark/restore
- Deleted-only filtering via `FileTreeModel::setShowDeletedOnly()`
- Multi-select: Ctrl+click, Shift+click, Shift+arrows (ExtendedSelection mode)
- Clicking Restore auto-marks selected files if none are marked
- Key bindings: Space=Mark, F5=Restore, Tab=View toggle, /=Search, *=Invert, F10=Quit
- Status bar: mark count + size

## Restore Screen

- Unified `ProgressDialog` used for all operations (scan, carve, restore, backup)
- QProgressBar + current file label + ok/fail counts
- Cancel button — disconnects progress signals before closing to prevent use-after-free
- Dialog auto-closes on completion
- Final summary with pass/fail counts

## Disk Selection

- QTableView with Device/Size/Perm/Model columns
- Human-readable sizes (GB/TB)
- Selection uses cyan highlight
- Refresh/Open Image/Proceed/Quit buttons
- Open Image supports: `.img`, `.dd`, `.raw`, `.dsk`, `.vhd`, `.vmdk`, `.vdi`, `.e01`, `.qcow2`, `.iso`, `.bin`

## Partition Selection

- QTableView with Name/Filesystem/Size/Type columns
- "Whole disk" row prepended at top (bold)
- Human-readable partition types (NTFS, FAT32, EXT4, etc.)
- LUKS partitions highlighted in magenta
- Scan/Deep FS Scan/Carve/Backup/Restore/Back buttons

## LUKS Support

- Detection: `LUKSManager::isEncrypted()` via `check_LUKS()`
- Scan mode: auto-prompts for passphrase
- Carve mode: asks "Decrypt first? (y/N)"
- Decrypt: `luks_open()` via `losetup` + `cryptsetup luksOpen`
- Cleanup: `luks_cleanup_orphans()` at startup for orphaned mappings

## Common Pitfalls

- **Logging**: Log file is `recovery-qt.log` in CWD, opened at startup via `log_open()`.
  Use `log_info()`/`log_error()`/`log_warning()` in C code, `qDebug()` in C++ wrappers.
  Never create log files in `/tmp` or elsewhere — all logs go to this single file.
  Never use `printf()` or `fprintf(stderr, ...)` for diagnostics; always use the log.
- **C90**: all declarations at top of block, no compound literals, no mixed statements
- **`carver_init()`**: sets `array_file_enable[].enable`, then calls `init_file_stats()`
  which registers header checks via `register_header_check()`. Must call
  `carver_deinit()` → `free_header_check()` to clean up.
- **Min signature length**: `carver_check_header()` skips checks with `length < 3`
- **`tree_add_path()`**: live dir + deleted entry → only `deleted` flag updated,
  dir metadata NOT overwritten; file metadata IS overwritten.
- **Progress callbacks**: must be installed before C operation and uninstalled after.
  `ProgressCallback::uninstallAllCallbacks()` sets all globals back to NULL.
  Worker threads must NOT call `uninstallAllCallbacks()` — only the main thread
  after the dialog closes, to prevent use-after-free of `s_*Instance` pointers.
- **`check_LUKS()` return value**: returns `0` when LUKS IS detected, `1` on
  failure/not-detected. This is inverted from typical boolean convention.
- **Thread safety**: C core operations access `scan_tree_t`, `disk_t`, `partition_t`
  only from the worker thread. Qt model access is read-only during scans.
- **Signal safety**: always save `QMetaObject::Connection` handles and disconnect
  progress signal connections in the `finished` handler before calling `dlg.accept()`.
  Pending queued signals can fire on a destroyed dialog otherwise.
- **Disk lifecycle**: `Disk` uses `shared_ptr<DiskData>`, does NOT auto-free
  `disk_t` — managed by `list_disk_t` from `hd_parse()` or explicitly by caller.
- **PartitionList cleanup**: destructor calls `part_free_list()` + `free()`.
  Re-detection frees old allocations before creating new ones.
