gnubgiconsdir = $(datadir)
gnubgpixmapsdir = $(pkgdatadir)/pixmaps

ICONS_16_SCALABLE = icons/hicolor/16x16/apps/scalable/gnubg.svg
ICONS_16 = icons/hicolor/16x16/apps/gnubg.png
ICONS_22_SCALABLE = icons/hicolor/22x22/apps/scalable/gnubg.svg
ICONS_22 = icons/hicolor/22x22/apps/gnubg.png
ICONS_24_SCALABLE = icons/hicolor/24x24/apps/scalable/gnubg.svg
ICONS_24 = icons/hicolor/24x24/apps/gnubg.png
ICONS_32_SCALABLE = icons/hicolor/32x32/apps/scalable/gnubg.svg
ICONS_32 = icons/hicolor/32x32/apps/gnubg.png
ICONS_48_SCALABLE = icons/hicolor/48x48/apps/scalable/gnubg.svg
ICONS_48 = icons/hicolor/48x48/apps/gnubg.png

STOCK_MENU_IMAGES = \
		    16x16/actions/cancel_16.png \
		    16x16/actions/ok_16.png \
		    16x16/actions/hint_16.png \
		    16x16/actions/double_16.png

STOCK_BUTTON_IMAGES = \
		      24x24/actions/ok_24.png \
		      24x24/actions/cancel_24.png \
		      24x24/actions/hint_24.png \
		      24x24/actions/double_24.png

STOCK_IMAGES = \
	       $(STOCK_MENU_IMAGES) \
	       $(STOCK_BUTTON_IMAGES)

PIXMAPS = gnubg-big.png

EXTRA_DIST = \
	     gnubg-stock-pixbufs.h \
	     gnubg.ico \
	     $(STOCK_IMAGES) \
	     $(ICONS_16_SCALABLE) \
	     $(ICONS_22_SCALABLE) \
	     $(ICONS_24_SCALABLE) \
	     $(ICONS_32_SCALABLE) \
	     $(ICONS_48_SCALABLE)

stock-icons.list: $(STOCK_IMAGES) Makefile.am
	( rm -f $@; \
          for image in $(STOCK_IMAGES); do \
            echo $$image | \
              sed -e 's|.*/||' -e 's|-|_|g' -e 's|\.png$$||' >> $@; \
	    echo "  $(srcdir)/$$image" >> $@; \
	  done )
if USE_GTK
if FOUND_GDK_PIXBUF_CSOURCE
gnubg-stock-pixbufs.h: stock-icons.list
	$(GDK_PIXBUF_CSOURCE) --raw --build-list `cat $<` > $(@F)
CLEANFILES=stock-icons.list gnubg-stock-pixbufs.h
else
gnubg-stock-pixbufs.h: stock-icons.list
	@echo ' ****************************'
	@echo ' ** unable to rebuild pixmaps'
	@echo ' ****************************'
endif
endif


nobase_dist_gnubgicons_DATA = \
			      $(ICONS_16) \
			      $(ICONS_22) \
			      $(ICONS_24) \
			      $(ICONS_32) \
			      $(ICONS_48)

dist_gnubgpixmaps_DATA = $(PIXMAPS)

noinst_DATA = gnubg-stock-pixbufs.h
