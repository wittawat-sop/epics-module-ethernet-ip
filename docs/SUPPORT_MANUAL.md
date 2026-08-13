# genericEIP Support and Caller Manual

This manual describes how to use `genericEIP` from an EPICS IOC. It covers
module linking, IOC shell callers, database links, raw buffers, and status.

## Build and link

Build the module first:

```sh
make show-config
make
```

The build produces `libgenericEIPSupport.so`, `libgenericEIPSupport.a`,
`genericEIP.dbd`, and the bundled `libEIPScanner.a`. A new IOC must include
the DBD and link the support library:

```make
myIoc_DBD += base.dbd
myIoc_DBD += /path/to/ethernet_ip/dbd/genericEIP.dbd

myIoc_LIBS += genericEIPSupport
myIoc_LIBS += asyn
myIoc_LIBS += $(EPICS_BASE_IOC_LIBS)
genericEIPSupport_DIR = /path/to/ethernet_ip/lib/$(EPICS_HOST_ARCH)
```

The DBD registers `ai`, `ao`, `bi`, `bo`, `longin`, `longout`,
`mbbi`, `mbbo`, and `stringin` under the device type `Ethernet/IP`.
See [`CREATE_IOC.md`](CREATE_IOC.md) for a complete `makeBaseApp.pl`
workflow.

## IOC shell caller functions

### `genericEipConfigure`

```text
genericEipConfigure(portName, ipAddress, pollPeriod,
                    vendorId, productCode, revision,
                    configAssembly, outputAssembly, inputAssembly,
                    outputSize, inputSize, outputRpi, inputRpi)
```

Example:

```iocsh
genericEipConfigure("EIP1", "192.0.2.10", 0.05,
                    0, 0, 0, 0, 101, 100, 40, 56, 10000, 10000)
```

| Argument | Description |
|---|---|
| `portName` | Unique driver/asyn port, for example `EIP1`. |
| `ipAddress` | EtherNet/IP target address. |
| `pollPeriod` | Monitor period in seconds; must be positive. |
| `vendorId`, `productCode`, `revision` | Reserved identity values. |
| `configAssembly` | Configuration assembly ID; use `0` when unused. |
| `outputAssembly` | O→T assembly ID. |
| `inputAssembly` | T→O assembly ID. |
| `outputSize`, `inputSize` | Assembly sizes in bytes. |
| `outputRpi`, `inputRpi` | RPI values in microseconds. |

The assembly IDs, sizes, and RPIs must match the device.

### `genericEipConnect`

```iocsh
genericEipConnect("EIP1")
```

Starts the connection monitor asynchronously. A successful return means the
monitor started; it does not mean Forward Open has completed. Use the status
PV to determine the live connection state.

### `genericEipDisconnect`

```iocsh
genericEipDisconnect("EIP1")
```

Stops the monitor, closes the Class 1 session, and sets the state to
`DISCONNECTED`.

## Database field links

Use `DTYP="Ethernet/IP"` with this syntax:

```text
@etherip(PORT,offset[,bit])TYPE
```

Examples:

```text
field(DTYP, "Ethernet/IP")
field(INP, "@etherip(EIP1,8)INT32")
field(OUT, "@etherip(EIP1,12)UINT16")
field(INP, "@etherip(EIP1,0,3)BIT")
```

Supported types are `INT8`, `UINT8`, `INT16`, `UINT16`, `INT32`,
`UINT32`, `INT64`, `UINT64`, `FLOAT32`, `FLOAT64`, and `BIT`
(bits 0–15). Offsets are byte offsets and numeric values use little-endian
byte order. Input records should normally use `SCAN="I/O Intr"`.

`BIT` uses one byte for bits 0–7 and a two-byte little-endian word for bits
8–15. Output records share the output buffer and the Class 1 worker sends it
at the configured O→T RPI.

## Connection status PV

The template provides:

```text
MOTOR:M1:ConnectionState
```

It uses:

```text
DTYP = "Ethernet/IP"
INP  = "@etherip(EIP1)CONNECTION_STATE"
SCAN = "I/O Intr"
```

Typical values are `CONNECTING`, `CONNECTED`, and `DISCONNECTED`. A
connection error string may also be shown while the monitor continues retrying.
The driver automatically retries after a timeout or closed Class 1 connection.

## Raw buffer records

For complete assembly access, load `db/genericEIPRaw.db`:

```iocsh
dbLoadRecords("/path/to/ethernet_ip/db/genericEIPRaw.db",
              "P=EIP:,PORT=EIP1,ADDR=0,IN_SIZE=56,OUT_SIZE=40")
```

Use field links for normal operation; raw records are useful for diagnostics
and device layouts that are not yet mapped.

## Minimal startup example

```iocsh
dbLoadDatabase "$(TOP)/dbd/myIoc.dbd"
myIoc_registerRecordDeviceDriver(pdbbase)
genericEipConfigure("EIP1", "192.0.2.10", 0.05,
                    0, 0, 0, 0, 101, 100, 40, 56, 10000, 10000)
genericEipConnect("EIP1")
dbLoadRecords("/path/to/ethernet_ip/db/orientalEIPSupport.template",
              "P=MOTOR:,R=M1:,PORT=EIP1")
iocInit
```

## Troubleshooting

1. Run `make show-config` and check EPICS Base, asyn, and EIPScanner paths.
2. Verify `third_party/EIPScanner/build/libEIPScanner.a` exists.
3. Confirm the IOC DBD includes `genericEIP.dbd` and links
   `genericEIPSupport`.
4. Check `ConnectionState` before debugging field offsets.
5. Confirm target IP, assembly IDs, sizes, and RPIs.
6. Use `dbpr PV 1` to check that input records have a non-null DPVT and
   `SCAN: I/O Intr`.

