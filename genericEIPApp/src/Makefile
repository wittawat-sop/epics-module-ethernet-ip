TOP=../..
include $(TOP)/configure/CONFIG
USR_CPPFLAGS += -DUSE_TYPED_DSET -DUSE_TYPED_RSET

USR_INCLUDES += -I$(EIPSCANNER)/src

DBD += genericEIP.dbd

LIBRARY_IOC += genericEIPSupport
genericEIPSupport_SRCS += GenericEIPDriver.cpp
genericEIPSupport_SRCS += EipSession.cpp
genericEIPSupport_SRCS += devGenericEIP.cpp
genericEIPSupport_LIBS += asyn
genericEIPSupport_LIBS += $(EPICS_BASE_IOC_LIBS)
genericEIPSupport_LDFLAGS += -L$(EIPSCANNER)/build
genericEIPSupport_SYS_LIBS += EIPScanner

PROD_IOC += genericEIP
genericEIP_SRCS_DEFAULT += genericEIP_registerRecordDeviceDriver.cpp genericEIPMain.cpp
genericEIP_LIBS += genericEIPSupport asyn
genericEIP_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
