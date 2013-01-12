ifdef BSNES_VERSION
OBJECTS=core.$(OBJECT_SUFFIX)
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
else
OBJECTS=dummy.$(OBJECT_SUFFIX)
endif

BSNES_LIBRARY=bsnes/out/libsnes.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS) ../../../$(BSNES_LIBRARY)
	$(REALLD) -r -o $@ $^
	echo $(BSNES_LDFLAGS) ../$(BSNES_LIBRARY) >__all__.ldflags

../../../$(BSNES_LIBRARY): forcelook
	$(MAKE) -C ../../../bsnes $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) $@

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) -c -o $@ $< -I../../../include -I../../../bsnes $(CFLAGS) $(BSNES_CFLAGS)

forcelook:
	@true

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
	make -C ../../../bsnes clean
