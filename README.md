# recovery-qt

A Qt6-based data recovery tool derived from **TestDisk & PhotoRec 7.3-WIP** by
[Christophe GRENIER](https://www.cgsecurity.org). Linux only.

recovery-qt provides a graphical interface for browsing deleted files, selective
file restoration, raw file carving, filesystem backup/restore, and LUKS-encrypted
volume support.

## Features
<img width="900" height="654" alt="Screenshot_20260714_110515" src="https://github.com/user-attachments/assets/2d1d3a19-ebb9-4068-9f04-f4e2a0deabb8" />
<img width="900" height="654" alt="Screenshot_20260714_110757" src="https://github.com/user-attachments/assets/c46ea400-edb6-481e-9675-e4665a824900" />
<img width="900" height="654" alt="Screenshot_20260714_110830" src="https://github.com/user-attachments/assets/2c26a855-5faf-47d0-8f12-ceb5cae94d3b" />
<img width="900" height="650" alt="Screenshot_20260714_110415" src="https://github.com/user-attachments/assets/eae52f2e-2465-4e92-beda-df472e04c5fc" />
<img width="900" height="654" alt="Screenshot_20260714_110858" src="https://github.com/user-attachments/assets/44c6c22a-cec7-4911-bcb1-8760324131c3" />

### Recovery Operations

| Operation | Description |
|-----------|-------------|
| **Scan** | Fast filesystem scan — directory walk, MFT lookup, FAT directory enumeration |
| **Deep FS Scan** | Byte-level scan of free clusters: NTFS INDX records, FAT free clusters, EXT2/3/4 inode table |
| **Carve** | Raw sector-by-sector scan matching 300+ file signature headers (JPEG, PDF, DOCX, ZIP, etc.) |
| **Backup** | Creates `.dsk` index backup of filesystem metadata and file data |
| **Restore** | Restores files from `.dsk` backup, or selectively restores marked files from scan results |

### Qt6 Interface
- Nord dark theme
- QTreeView file browser with color-coded entries (red=deleted, green=marked)
- Multi-select (Ctrl/Shift), name search, deleted-only view toggle
- Format selector with quick-select categories (Documents, Images, Archives, etc.)
- Thread-safe progress dialogs with cancel support, real-time file count/size

### LUKS Support
- Automatic LUKS1/LUKS2 detection
- On-demand decryption via `cryptsetup` + `losetup`
- Orphaned mapper and loop device cleanup at startup

### Disk Image Support
Open disk images directly: `.img`, `.dd`, `.raw`, `.dsk`, `.vhd`, `.vmdk`, `.vdi`,
`.e01`, `.qcow2`, `.iso`, `.bin`

### Filesystem Support
FAT12/16/32, exFAT, NTFS, ext2/3/4, HFS+, APFS, ReFS, Btrfs, XFS, ZFS,
ReiserFS, and others

## What's New vs. Original PhotoRec

This fork adds significant functionality beyond the original PhotoRec:

| Addition | Detail |
|----------|--------|
| **Qt6 GUI** | Complete modernization from ncurses to Qt6 Widgets |
| **Modular architecture** | Separated scanner, carver, tree, and restore into independent C modules (`pscanner.c`, `pcarver.c`, `ptree.c`, `prestore.c`) |
| **Deep FS Scan** | NTFS INDX scan, FAT free cluster scan, EXT inode table — finds files missed by fast scan |
| **NTFS $ATTRIBUTE_LIST** | `process_attribute_list()` in `ntfs_udl.c` — handles highly fragmented files spanning multiple MFT records |
| **LUKS decryption** | Full LUKS1/LUKS2 support via `cryptsetup`+`losetup` (`luksnc.c`) |
| **Backup/Restore** | Create and restore `.dsk` filesystem index backups (`pbackup.c`) |
| **Selective restore** | Mark individual files for recovery; restore only what you need |
| **Thread-safe engine** | All heavy operations run in QThread workers with progress bridging |
| **Improved carver** | Sector-aligned header matching (fewer false positives), same-format proximity filter, footer detection priority system |
| **Fixed MOV carver** | Handles `calculated_file_size == 0` edge case that caused premature header rejection |
| **Fixed JPEG carver** | Removed overly restrictive thumbnail check that rejected valid JPEGs |
| **Format selector** | Checkable format list with quick-select by category |
| **Cancel support** | All long-running operations can be cancelled mid-flight |

## Build

```bash
cmake -S . -B build && cmake --build build -- -j$(nproc)
```

Binary is at `build/recovery-qt`. Requires Qt6 (Core + Widgets), libntfs-3g,
libext2fs, zlib, and libuuid.

## Carve vs. Deep FS Scan

**Carve (sector-aligned)** — checks for file headers at every 512-byte
sector boundary. Usually produces **better results** because:

- Container-format internal markers (MOV atoms, ZIP entries, EXIF
  thumbnails, PDF streams) almost never land on sector-aligned offsets
  within the file, so the carver naturally skips them.
- Fewer false positive header matches — 1/512th the byte positions
  reduces random collisions with compressed binary data.

**Deep FS Scan** — performs a full deep filesystem scan including
NTFS INDX scan, FAT free cluster scan, and EXT inode table scan.
Use this when the fast scan doesn't find enough deleted files.

The generic **same-format proximity filter** prevents container-format
internal markers from splitting files.

## User Flow

```
Disk Selection → Partition Selection
                        │
          ┌─────────────┼─────────────┐
       "Scan"        "Carve"    "Backup/Restore"
          │              │              │
    Scanner(QThread) Carver(QThread)  QThread
          │              │              │
          └──────┬───────┘              │
                 │                      │
          File Browser ←────────────────┘
                 │
            F5 → Restore
```

## License

This is a derivative work of TestDisk & PhotoRec, licensed under the
**GNU General Public License v2+**. See [COPYING](COPYING).

Original work copyright Christophe GRENIER <grenier@cgsecurity.org>.
Qt port and modifications copyright Sinan Islekdemir <sinan@islekdemir.com>.
