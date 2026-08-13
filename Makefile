TOP=.
include $(TOP)/configure/CONFIG

# Edit configure/RELEASE to set EPICS_BASE, ASYN, and EIPSCANNER before
# building. EPICS requires EPICS_BASE to be defined in that file.
# Set CMAKE below or on the command line if CMake is not on PATH.

CMAKE ?= cmake

DIRS += genericEIPApp
include $(TOP)/configure/RULES_DIRS

EIPSCANNER_BUILD = $(EIPSCANNER)/build

.PHONY: eipscanner ioc show-config

# Build the bundled static EIPScanner library before the EPICS support
# library is compiled and linked.
eipscanner:
	@if test ! -f "$(EIPSCANNER_BUILD)/CMakeCache.txt"; then \
		$(CMAKE) -S "$(EIPSCANNER)" -B "$(EIPSCANNER_BUILD)" \
			-DENABLE_VENDOR_SRC=ON -DTEST_ENABLED=OFF -DEXAMPLE_ENABLED=OFF; \
	fi
	$(CMAKE) --build "$(EIPSCANNER_BUILD)"

genericEIPApp: eipscanner
genericEIPApp.install: eipscanner

ioc: genericEIPApp
	$(MAKE) -C $(TOP)/iocBoot/iocGenericEIP

show-config:
	@echo "EPICS_BASE = $(EPICS_BASE)"
	@echo "ASYN       = $(ASYN)"
	@echo "EIPSCANNER = $(EIPSCANNER)"
