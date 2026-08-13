#!../../bin/linux-x86_64/genericEIP

< envPaths

epicsEnvSet("IOCNAME", "genericEIP-example")
epicsEnvSet("EIP_PORT", "EIP1")
epicsEnvSet("EIP_IP", "192.168.5.246")

cd "$(TOP)"

# Load the module DBD; it includes base/asyn and registers Ethernet/IP device
# support. The second argument resolves nested DBD includes.
epicsEnvSet("EPICS_DBD_PATH", "$(EPICS_BASE)/dbd:$(ASYN)/dbd")
dbLoadDatabase "$(TOP)/genericEIPApp/src/genericEIP.dbd", "$(EPICS_DBD_PATH)"
genericEIP_registerRecordDeviceDriver(pdbbase)

# Connection configuration:
# configAssembly, O->T assembly, T->O assembly,
# O->T bytes, T->O bytes, O->T RPI, T->O RPI.
genericEipConfigure("$(EIP_PORT)", "$(EIP_IP)", 0.05, 0, 0, 0, 0, 101, 100, 40, 56, 10000, 10000)
genericEipConnect("$(EIP_PORT)")

# Raw byte records; device-specific mapping belongs in separate templates.
dbLoadRecords("db/genericEIPRaw.db", "P=EIP:,PORT=$(EIP_PORT),ADDR=0,IN_SIZE=56,OUT_SIZE=40")
dbLoadRecords("db/genericEIPExample.template", "P=EIP:,PORT=$(EIP_PORT)")
dbLoadRecords("db/orientalEIPSupport.template", "P=MOTOR:,R=M1:,PORT=$(EIP_PORT)")

iocInit

## Apply safe default output values after records and IOC are initialized.
## Keep START/TRIG/STOP low so IOC startup never starts motion.
epicsThreadSleep 3
dbpf MOTOR:M1:CmdFlags 0
dbpf MOTOR:M1:OpNum 0
dbpf MOTOR:M1:OpType 2
dbpf MOTOR:M1:TargetPos 0
dbpf MOTOR:M1:OpSpeed 1000
dbpf MOTOR:M1:StartSpeed 1000000
dbpf MOTOR:M1:AccelRate 1000000
dbpf MOTOR:M1:TorqueLimit 1000
dbpf MOTOR:M1:ForwardData 0
dbpf MOTOR:M1:ReverseData 0
dbpf MOTOR:M1:FixedIO_FwJog 0
dbpf MOTOR:M1:FixedIO_RvJog 0
dbpf MOTOR:M1:FixedIO_Start 0
dbpf MOTOR:M1:FixedIO_Zhome 0
dbpf MOTOR:M1:FixedIO_Stop 0
dbpf MOTOR:M1:FixedIO_Free 0
dbpf MOTOR:M1:FixedIO_AlmRst 0
dbpf MOTOR:M1:FixedIO_Trig 0
dbpf MOTOR:M1:FixedIO_TrigMode 0
dbpf MOTOR:M1:FixedIO_FwJogP 0
dbpf MOTOR:M1:FixedIO_RvJogP 0
dbpf MOTOR:M1:FixedIO_FwPos 0
dbpf MOTOR:M1:FixedIO_RvPos 0
