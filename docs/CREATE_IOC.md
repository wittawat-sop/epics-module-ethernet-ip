# สร้าง IOC ใหม่ด้วย `makeBaseApp.pl`

เอกสารนี้อธิบายการสร้าง EPICS IOC ใหม่ที่ใช้ `genericEIP` เป็น device support

## 1. Build module

กำหนด path ใน `configure/RELEASE` ของ module นี้ แล้ว build ก่อนสร้าง IOC:

```sh
cd /path/to/ethernet_ip
make
```

คำสั่งนี้ build bundled EIPScanner, `libgenericEIPSupport` และ example IOC
ตามลำดับ

## 2. สร้าง application และ IOC

```sh
mkdir -p /path/to/azdIOC
cd /path/to/azdIOC
makeBaseApp.pl -t example -a azdApp
makeBaseApp.pl -t ioc -i -p azdApp
```

ถ้า `makeBaseApp.pl` ไม่อยู่ใน `PATH` ให้ใช้:

```sh
/usr/local/epics/base/bin/linux-x86_64/makeBaseApp.pl -t example -a azdApp
/usr/local/epics/base/bin/linux-x86_64/makeBaseApp.pl -t ioc -i -p azdApp
```

โครงสร้างหลัก:

```text
azdIOC/
├── configure/RELEASE
├── azdApp/src/Makefile
└── iocBoot/iocazdApp/st.cmd
```

## 3. ตั้งค่า `configure/RELEASE`

แก้ไฟล์ `configure/RELEASE` ของ IOC ใหม่:

```make
EPICS_BASE=/usr/local/epics/base
ASYN=/usr/local/epics/support/asyn-R4-44-2
ETHERNET_IP=/path/to/ethernet_ip
```

`ETHERNET_IP` ต้องชี้ไปที่ root ของ repository นี้ ซึ่งมี DBD, library และ
database files ของ genericEIP

## 4. เพิ่ม genericEIP ใน Makefile

แก้ `azdApp/src/Makefile` โดยใช้ชื่อ executable `azdApp`:

```make
TOP=../..
include $(TOP)/configure/CONFIG

PROD_IOC = azdApp
azdApp_DBD += base.dbd
azdApp_DBD += $(ETHERNET_IP)/dbd/genericEIP.dbd
azdApp_SRCS += azdApp_registerRecordDeviceDriver.cpp
azdApp_SRCS_DEFAULT += azdAppMain.cpp

azdApp_LIBS += genericEIPSupport
azdApp_LIBS += asyn
azdApp_LIBS += $(EPICS_BASE_IOC_LIBS)
genericEIPSupport_DIR = $(ETHERNET_IP)/lib/$(EPICS_HOST_ARCH)

include $(TOP)/configure/RULES
```

ถ้า EPICS version ไม่รองรับ absolute path ใน `_DBD` ให้ copy DBD เข้า app:

```sh
cp /path/to/ethernet_ip/dbd/genericEIP.dbd azdApp/src/
```

แล้วใช้:

```make
azdApp_DBD += genericEIP.dbd
```

## 5. แก้ `st.cmd`

ตัวอย่างส่วนสำคัญของ `iocBoot/iocazdApp/st.cmd`:

```iocsh
#!../../bin/linux-x86_64/azdApp
< envPaths
cd "$(TOP)"

epicsEnvSet("EIP_PORT", "EIP1")
epicsEnvSet("EIP_IP", "192.0.2.10")
epicsEnvSet("EIP_DB", "/path/to/ethernet_ip/db")

dbLoadDatabase "$(TOP)/dbd/azdApp.dbd"
azdApp_registerRecordDeviceDriver(pdbbase)

genericEipConfigure("$(EIP_PORT)", "$(EIP_IP)", 0.05,
                    0, 0, 0, 0, 101, 100, 40, 56, 10000, 10000)
genericEipConnect("$(EIP_PORT)")

dbLoadRecords("$(EIP_DB)/genericEIPRaw.db",
              "P=EIP:,PORT=$(EIP_PORT),ADDR=0,IN_SIZE=56,OUT_SIZE=40")
dbLoadRecords("$(EIP_DB)/orientalEIPSupport.template",
              "P=MOTOR:,R=M1:,PORT=$(EIP_PORT)")
iocInit
```

เปลี่ยน `EIP_IP`, assembly, size และ RPI ให้ตรงกับอุปกรณ์จริง โดย
`192.0.2.10` เป็น IP สำหรับเอกสารเท่านั้น

## 6. Build และ run

```sh
make
make -C iocBoot/iocazdApp
cd iocBoot/iocazdApp
../../bin/linux-x86_64/azdApp st.cmd
```

ตรวจสอบ PV:

```sh
caget MOTOR:M1:ConnectionState
caget MOTOR:M1:DetectPos
```

สถานะ connection คือ `CONNECTING`, `CONNECTED` หรือ `DISCONNECTED` และ
driver จะ retry เมื่ออุปกรณ์หลุดหรือยังไม่พร้อม

## ปัญหาที่พบบ่อย

- `genericEIP.dbd not found`: ตรวจสอบ `ETHERNET_IP` หรือ copy DBD เข้า
  `azdApp/src`
- `libgenericEIPSupport.so not found`: ตรวจสอบ `genericEIPSupport_DIR` และ
  runtime library path
- device support ไม่พบ: ตรวจสอบว่า `azdApp.dbd` รวม `genericEIP.dbd`
- input ไม่เปลี่ยน: ตรวจสอบ assembly/size/RPI และใช้ `SCAN="I/O Intr"`
