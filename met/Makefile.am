metfiles = \
catalog.xml g11.xml jac050.xml jac100.xml jacobs.xml Kazaross-Shaw.xml mec26.xml \
mec.xml catalog.dtd met.dtd met.xsl ortega.xml README snowie.xml TODO woolsey.xml zadeh.xml
metdir = $(pkgdatadir)/met
met_DATA = $(metfiles)
EXTRA_DIST = $(metfiles)

# Create symlinks to all the necessary parts of $(srcdir), so that gnubg
# can run happily in the build directory.
all-local:
	@for i in $(met_DATA); do [ -s $$i ] || if [ -s $(srcdir)/$$i ]; \
	then $(LN_S) $(srcdir)/$$i .; fi; done
