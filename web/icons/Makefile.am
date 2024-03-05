pkgdatadir = $(datadir)/gmerlin-control/web/icons

files = \
controlcenter_96.png \
controlcenter_48.png

pkgdata_DATA = $(files)
dist_pkgdata_DATA = $(files)

install-data-hook:
	test -z $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps || $(MKDIR_P) $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps
	ln -sf $(DESTDIR)$(pkgdatadir)/controlcenter_48.png $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-controlcenter.png

uninstall-local:
	-rm -rf $(DESTDIR)$(prefix)/share/icons/hicolor/48x48/apps/gmerlin-controlcenter.png
