PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
DATADIR ?= $(PREFIX)/share

PLUGIN_NAME = darkmode-switcher
PLUGIN_LIB = lib$(PLUGIN_NAME).so
PLUGIN_LIBDIR = $(LIBDIR)/xfce4/panel/plugins
PLUGIN_DATADIR = $(DATADIR)/xfce4/panel/plugins

PKGS = gtk+-3.0 libxfce4panel-2.0
CFLAGS ?= -O2 -g
CFLAGS += -fPIC -Wall -Wextra $(shell pkg-config --cflags $(PKGS))
LDFLAGS += -shared
LDLIBS += $(shell pkg-config --libs $(PKGS))

all: $(PLUGIN_LIB) data/$(PLUGIN_NAME).desktop

$(PLUGIN_LIB): src/darkmode-plugin.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

data/$(PLUGIN_NAME).desktop: data/$(PLUGIN_NAME).desktop.in
	sed 's#@PLUGIN_LIBDIR@#$(PLUGIN_LIBDIR)#g' $< > $@

install: all data/$(PLUGIN_NAME).desktop
	install -d '$(DESTDIR)$(PLUGIN_LIBDIR)' '$(DESTDIR)$(PLUGIN_DATADIR)'
	install -m 755 $(PLUGIN_LIB) '$(DESTDIR)$(PLUGIN_LIBDIR)/$(PLUGIN_LIB)'
	sed 's#@PLUGIN_LIBDIR@#$(PLUGIN_LIBDIR)#g' data/$(PLUGIN_NAME).desktop.in > '$(DESTDIR)$(PLUGIN_DATADIR)/$(PLUGIN_NAME).desktop'

uninstall:
	rm -f '$(DESTDIR)$(PLUGIN_LIBDIR)/$(PLUGIN_LIB)'
	rm -f '$(DESTDIR)$(PLUGIN_DATADIR)/$(PLUGIN_NAME).desktop'

clean:
	rm -f $(PLUGIN_LIB) data/$(PLUGIN_NAME).desktop

.PHONY: all install uninstall clean

test-ui: src/darkmode-plugin.c
	$(CC) $(CFLAGS) -DDARKMODE_STANDALONE -o darkmode-switcher-test $< $(shell pkg-config --libs gtk+-3.0)
