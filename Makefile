all: libgambatte/__all_files__

libgambatte/__all_files__: forcelook
	$(MAKE) -C libgambatte

clean:  forcelook
	$(MAKE) -C libgambatte clean

forcelook:
	@true
