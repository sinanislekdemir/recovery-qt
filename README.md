# recovery-qt

A Qt6-based data recovery tool derived from **TestDisk & PhotoRec 7.3-WIP** by
[Christophe GRENIER](https://www.cgsecurity.org). 

**Linux only.**

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

## recovery-qt vs. PhotoRec

PhotoRec is a **raw block-level file carver** — it scans disk sectors sequentially, matches
byte-level signatures, and reconstructs files. recovery-qt takes a fundamentally different
approach: it combines **filesystem-aware directory scanning** with raw carving, wrapped in
a modern Qt6 GUI for selective, preview-before-restore recovery.

### What recovery-qt Adds

| Capability | PhotoRec | recovery-qt |
|---|---|---|
| **FS-aware directory scan** | No — carving only | Walks FAT/NTFS/EXT2/exFAT trees, detects deleted entries by flag |
| **Deleted file names & paths** | Not recovered | Original filenames, directory structure, and mtimes from FS metadata |
| **NTFS MFT orphan scan** | No | Reads MFT for deleted records, infers extensions, creates /ORPHAN/ |
| **Deep FS scan** | No | Free cluster byte-by-byte scan for residual deleted directory entries |
| **Selective file restore** | All-or-nothing per format | Tick individual files/directories (Space), restore only what you need |
| **Image preview** | None | Zoomable preview (Enter) — 20 formats, reads raw bytes from disk pre-restore |
| **LUKS encrypted volumes** | No | Full LUKS1/LUKS2 decrypt via cryptsetup+losetup, password dialog, async thread |
| **Filesystem backup/restore** | No | .dsk index files, detects files changed since backup (backup_modified flag) |
| **Qt6 GUI** | Ncurses TUI / old Qt4 | Modern dark theme, QTreeView browser, filter-as-you-type format selector |
| **Disk image files** | E01 via libewf | .img, .dd, .raw, .dsk, .vhd, .vmdk, .vdi, .qcow2, .bin, .iso + E01 |
| **Thread-safe engine** | Single-threaded | WorkerBase + QThread per operation, atomic state, cancel support |
| **Cancel support** | Partial | All operations cancellable mid-flight |

### Where recovery-qt Shines

1. **Deleted file recovery on live filesystems** — reads the filesystem directory to show
   what was deleted, with original names and paths. PhotoRec only carves raw bytes by sector
   offset into `recup_dir.N/f<number>.<ext>` files.

2. **Preview before restore** — verify a file is intact before committing to restore it.
   PhotoRec has no preview capability at all.

3. **Selective recovery** — browse a tree of thousands of deleted files, mark only the
   ones you want, and restore just those. PhotoRec dumps everything matching a file type.

4. **LUKS support** — decrypt-before-scan or carve-without-decrypt workflow. PhotoRec has
   no encryption handling.

5. **Unified workflow** — disk selection → partition detection → scan/carve → browse →
   preview → restore, all in one tool. PhotoRec requires separate tools (testdisk for
   partition analysis, photorec for carving).

### Where PhotoRec Excels

PhotoRec remains superior for **forensic-grade deep carving** with features recovery-qt
does not replicate:

- **Multi-pass recovery** (5-8 passes): block alignment detection, main carve, brute-force
  fragment reassembly, and "save everything" pass
- **File validation**: per-format `data_check()`/`file_check()` callbacks that validate
  content as it's read — catches false positives recovery-qt's simpler header check misses
- **Session save/resume**: `photorec.ses` files allow pausing and resuming multi-day scans
- **DFXML forensic output**: Digital Forensics XML for tool chain integration
- **FAT unformat mode**: reconstructs deleted FAT directory structures (expert mode)
- **Remaining data image dump**: saves all untagged data blocks to `image_remaining.dd`
- **fidentify companion tool**: CLI tool using the same signature DB for file identification
- **Brute-force fragmented file recovery**: matches file fragments across non-contiguous blocks

## Build

### Dependencies

| Dependency | Debian/Ubuntu package | Purpose |
|-----------|----------------------|---------|
| Qt6 Core + Widgets | `qt6-base-dev` | GUI framework |
| libntfs-3g | `ntfs-3g-dev` | NTFS filesystem access |
| libext2fs | `libext2fs-dev` | ext2/3/4 filesystem access |
| com_err | `comerr-dev` | Error reporting for ext2fs |
| zlib | `zlib1g-dev` | Compression support |
| libuuid | `uuid-dev` | UUID generation |
| cryptsetup | `cryptsetup-bin`, `libcryptsetup-dev` | LUKS decryption (runtime) |
| CMake | `cmake` | Build system |
| C/C++ compiler | `build-essential` | GCC, make, headers |
| pkg-config | `pkg-config` | Library detection |

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev \
  ntfs-3g-dev libext2fs-dev comerr-dev zlib1g-dev uuid-dev \
  libcryptsetup-dev
```

### Compile

```bash
cmake -S . -B build && cmake --build build -- -j$(nproc)
```

Binary is at `build/recovery-qt`.

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
