metfiles = catalog met.dtd met.xsl jacobs.xml snowie.xml woolsey.xml zadeh.xml
metdir = $(pkgdatadir)/met
met_DATA = $(metfiles)
EXTRA_DIST = $(metfiles)

# Create symlinks to all the necessary parts of $(srcdir), so that gnubg
# can run happily in the build directory.
all-local:
	@for i in $(met_DATA); do [ -s $$i ] || if [ -s $(srcdir)/$$i ]; \
	then $(LN_S) $(srcdir)/$$i .; fi; done
