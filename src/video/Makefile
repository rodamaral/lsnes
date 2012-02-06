OBJECTS=$(patsubst %.cpp,%.$(OBJECT_SUFFIX),$(wildcard *.cpp)) 

#Threads
ifdef THREADS                                                                                  
ifeq ($(THREADS), NATIVE)
VIDEO_CFLAGS += -DNATIVE_THREADS
else
ifeq ($(THREADS), BOOST)
VIDEO_CFLAGS += -DBOOST_THREADS
ifdef BOOST_THREAD_LIB
VIDEO_LDFLAGS += -l$(BOOST_THREAD_LIB)
else
VIDEO_LDFLAGS += -lboost_thread-mt
endif
else
$(error "Bad value for THREADS (expected NATIVE or BOOST)")
endif
endif
endif


.PRECIOUS: %.$(OBJECT_SUFFIX)

__all__.$(OBJECT_SUFFIX): $(OBJECTS)
	$(REALLD) -r -o $@ $^
	echo $(VIDEO_LDFLAGS) >__all__.ldflags

%.$(OBJECT_SUFFIX): %.cpp
	$(REALCC) $(CFLAGS) -c -o $@ $< -I../../include $(VIDEO_CLFAGS)

precheck:
	@true

clean:
	rm -f *.$(OBJECT_SUFFIX) *.ldflags
