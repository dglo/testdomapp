ifeq ("cygwin-x86","$(strip $(PLATFORM))")
  XTRA_TOOLS_LIBDIRS += /usr/local/lib
  EXTRA_DEP_INC_DIRS += /usr/local/include
  C_FLAGS     += -DCYGWIN
endif

ifeq (Linux-i386, $(strip $(PLATFORM)))
   XTRA_TOOLS_LIBDIRS := /usr/lib
   C_FLAGS     += -DLINUX
endif
