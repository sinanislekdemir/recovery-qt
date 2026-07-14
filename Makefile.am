SUBDIRS = src
docdir ?= $(datadir)/doc/$(PACKAGE)

EXTRA_DIST = AUTHORS COPYING ChangeLog INFO INSTALL NEWS README.md README_dev_photorec.txt THANKS compile.sh \
	progsreiserfs-journal.patch progsreiserfs-file-read.patch \
	documentation.html \
	.travis.yml

install-data-hook:
	$(mkinstalldirs) $(DESTDIR)$(docdir)
	$(INSTALL_DATA) $(top_srcdir)/AUTHORS $(top_srcdir)/ChangeLog $(top_srcdir)/NEWS $(top_srcdir)/README.md $(top_srcdir)/THANKS $(top_srcdir)/documentation.html $(DESTDIR)$(docdir)

uninstall-hook:
	rm -rf $(DESTDIR)$(docdir)

cppcheck:
	cppcheck --quiet --enable=all -DHAVE_CONFIG_H -I$(builddir) -I/usr/include $(srcdir)/src

DISTCLEANFILES = *~ core
