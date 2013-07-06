ifdef BSNES_VERSION
OBJECTS=core.$(OBJECT_SUFFIX) scpu-disasm.$(OBJECT_SUFFIX)
BSNES_CFLAGS=
BSNES_LDFLAGS=
ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
BSNES_PROFILE_STRING=profile=compatibility
else
BSNES_PROFILE_STRING=profile=accuracy
endif
ifeq ($(BSNES_VERSION), 084)
BSNES_PROFILE_STRING+=options=debugger
CFLAGS += -DBSNES_HAS_DEBUGGER
else
ifeq ($(BSNES_VERSION), 085)
BSNES_PROFILE_STRING+=options=debugger
CFLAGS += -DBSNES_HAS_DEBUGGER
endif
endif
ifeq ($(BSNES_VERSION), 087)
BSNES_TARGET_STRING=target=libsnes
else
BSNES_TARGET_STRING=ui=ui-libsnes
endif
CFLAGS += -DBSNES_V${BSNES_VERSION}
CFLAGS += -DBSNES_VERSION=\"${BSNES_VERSION}\"

BSNES_LIBRARY=bsnes/out/libsnes.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS) ../../../$(BSNES_LIBRARY)
	lua ../../genfilelist.lua $^ >$@
	echo $(BSNES_LDFLAGS) ../$(BSNES_LIBRARY) >__all__.ldflags

../../../$(BSNES_LIBRARY): forcelook
	$(MAKE) -C ../../../bsnes $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) $@

ports.inc: ports.def ../make-ports.lua
	lua ../make-ports.lua $< >$@

%.$(OBJECT_SUFFIX): %.cpp ports.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../bsnes $(CFLAGS) $(BSNES_CFLAGS)
else

OBJECTS=
__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo >__all__.ldflags

endif

forcelook:
	@true

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.files __all__.ldflags
	-make -C ../../../bsnes clean
