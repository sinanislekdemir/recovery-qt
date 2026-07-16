# File: Makefile
#
# Copyright (C) 2026 Sinan Islekdemir <sinan@islekdemir.com>
# GPL-2.0-or-later
#
# Convenience wrapper around the CMake (Linux) and Docker (Windows) builds.

.PHONY: help linux win format lint clean

help:
	@echo "recovery-qt build targets:"
	@echo ""
	@echo "  make linux   - native Linux build into build/ (build/recovery-qt)"
	@echo "  make win     - Windows cross build via Docker into build-win/ (build-win/recovery-qt.exe)"
	@echo "  make format  - run clang-format over the sources"
	@echo "  make lint    - run clang-tidy (needs a configured build/)"
	@echo "  make clean   - remove build/ and build-win/"

linux:
	cmake -B build -S .
	cmake --build build -j

win:
	./win/build.sh

format:
	scripts/format.sh

lint:
	scripts/lint.sh

clean:
	rm -rf build build-win
