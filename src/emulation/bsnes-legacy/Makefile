ifdef BUILD_BSNES
OBJECTS=core.$(OBJECT_SUFFIX) scpu-disasm.$(OBJECT_SUFFIX) bitmap.$(OBJECT_SUFFIX) frequency.$(OBJECT_SUFFIX)
BSNES_CFLAGS=
BSNES_LDFLAGS=
ifdef BSNES_IS_COMPAT
CFLAGS += -DBSNES_IS_COMPAT
BSNES_PROFILE_STRING=profile=compatibility
else
BSNES_PROFILE_STRING=profile=accuracy
endif
include ../../../bsnes/bsnes.mk

ifdef BSNES_SUPPORTS_DEBUGGER
BSNES_PROFILE_STRING+=options=debugger
CFLAGS += -DBSNES_HAS_DEBUGGER
endif

ifeq ($(BSNES_VERSION), 087)
BSNES_TARGET_STRING=target=libsnes
else
BSNES_TARGET_STRING=ui=ui-libsnes
endif
CFLAGS += -DBSNES_VERSION=\"${BSNES_VERSION}\"
CFLAGS += -DLIBSNES_INCLUDE_FILE=\"${LIBSNES_DIR}/libsnes.hpp\"

BSNES_LIBRARY=bsnes/out/libsnes.$(ARCHIVE_SUFFIX)

.PRECIOUS: %.$(OBJECT_SUFFIX) %.files

__all__.files: $(OBJECTS) ../../../$(BSNES_LIBRARY)
	lua ../../genfilelist.lua $^ >$@
	echo $(BSNES_LDFLAGS) ../$(BSNES_LIBRARY) >__all__.ldflags

../../../$(BSNES_LIBRARY): forcelook
	mkdir -p ../../../bsnes/out ../../../bsnes/obj
	mkdir -p ../../../bsnes/snes/out ../../../bsnes/snes/obj
	$(MAKE) -C ../../../bsnes $(BSNES_PROFILE_STRING) $(BSNES_TARGET_STRING)
	$(REALRANLIB) $@

ports.inc: ports.json ../make-ports$(DOT_EXECUTABLE_SUFFIX)
	../make-ports$(DOT_EXECUTABLE_SUFFIX) <$< >$@

%.$(OBJECT_SUFFIX): %.cpp %.cpp.dep ports.inc
	$(REALCC) -c -o $@ $< -I../../../include -I../../../bsnes $(CFLAGS) $(BSNES_CFLAGS) -Wreturn-type
else

OBJECTS=
__all__.files: $(OBJECTS)
	lua ../../genfilelist.lua $^ >$@
	echo >__all__.ldflags

endif

forcelook:
	@true

precheck:
	../../../buildaux/mkdeps$(DOT_EXECUTABLE_SUFFIX) ../../../include -- *.cpp
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) __all__.files __all__.ldflags
	-make -C ../../../bsnes clean
