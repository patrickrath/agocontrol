DIRS = resolver rpc messagesend timer scenario event datalogger lua security drain

BUILDDIRS = $(DIRS:%=build-%)
INSTALLDIRS = $(DIRS:%=install-%)
CLEANDIRS = $(DIRS:%=clean-%)

BUILDFILES = agocontrol-init-script.sh

all: $(BUILDDIRS) $(BUILDFILES)
$(DIRS): $(BUILDDIRS)
$(BUILDDIRS):
	$(MAKE) -C $(@:build-%=%)

agocontrol-init-script.sh: agocontrol-init-script.sh.in
	sed -e 's|@BINDIR@|$(BINDIR)|g' $< > $@

install: $(INSTALLDIRS) all
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL) agodrain.py $(DESTDIR)$(BINDIR)
	$(INSTALL) myavahi.py $(DESTDIR)$(BINDIR)
	$(INSTALL) agoctrl.sh $(DESTDIR)$(BINDIR)

$(INSTALLDIRS): 
	$(MAKE) -C $(@:install-%=%) install

clean: $(CLEANDIRS)
	-rm $(BUILDFILES)

$(CLEANDIRS): 
	$(MAKE) -C $(@:clean-%=%) clean

.PHONY: subdirs $(DIRS)
.PHONY: subdirs $(BUILDDIRS)
.PHONY: subdirs $(INSTALLDIRS)
.PHONY: subdirs $(CLEANDIRS)
.PHONY: all install clean
