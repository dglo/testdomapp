C_BIN_NAMES :=  domapp domEchoClient runMoniDataAccess
C_EXCLUDE_NAMES :=
USES_PROJECTS := hal dom-loader
USES_TOOLS := pthread

ifeq ("epxa10","$(strip $(PLATFORM))")
  USES_TOOLS := $(filter-out pthread, $(USES_TOOLS))
  C_BIN_NAMES := $(filter-out domEchoClient, $(C_BIN_NAMES))
  C_BIN_NAMES := $(filter-out runMoniDataAccess, $(C_BIN_NAMES))
  C_EXCLUDE_NAMES :=  domEchoClient runMoniDataAccess
else
  USES_PROJECTS := $(filter-out dom-loader, $(USES_PROJECTS))
endif

ifeq ("cygwin-x86","$(strip $(PLATFORM))")
  USES_TOOLS   += cygipc
endif

include ../tools/resources/standard.mk
