# genericEIP

`genericEIP` is a generic EPICS/asyn EtherNet/IP driver for Class 1 implicit
I/O. It provides raw assembly buffers plus database-level field mapping and
does not depend on a particular motor vendor.

## Features

- Configurable O→T and T→O assemblies, sizes, and RPIs.
- EIPScanner-based Class 1 implicit I/O.
- EPICS I/O Intr updates when T→O data arrives.
- Field links using `DTYP="Ethernet/IP"`:

  ```text
  @etherip(PORT,offset[,bit])TYPE
  ```

- Supported types: `INT8`, `UINT8`, `INT16`, `UINT16`, `INT32`, `UINT32`,
  `INT64`, `UINT64`, `FLOAT32`, `FLOAT64`, and `BIT` (bits 0–15).
- Automatic reconnect after a connection failure or Class 1 timeout.
- Connection status PV with `CONNECTED`, `CONNECTING`, or `DISCONNECTED`.

## Dependencies

The module requires:

- EPICS Base
- asyn
- EIPScanner

Set the dependency paths in `configure/RELEASE`. The default configuration
expects these packages under `/usr/local/epics`.

## Build

From the module root:

```sh
make
```

Build products are installed under `bin/` and `lib/`; these directories are
ignored by Git.

## Example IOC

The example IOC is in `iocBoot/iocGenericEIP`. Set `EIP_IP` in
`iocBoot/iocGenericEIP/st.cmd`, then build and run:

```sh
make
cd iocBoot/iocGenericEIP
../../bin/linux-x86_64/genericEIP st.cmd
```

The example configuration uses:

```text
O→T Assembly = 101    O→T Size = 40 bytes    O→T RPI = 10000 us
T→O Assembly = 100    T→O Size = 56 bytes    T→O RPI = 10000 us
```

The target IP is configured by `EIP_IP`; the repository example currently
uses `192.168.5.246`.

## Database mapping

`db/orientalEIPSupport.template` demonstrates the AZD-KREP mapping. Input
records use `SCAN="I/O Intr"`, while output records write to the shared output
assembly buffer.

The connection state PV is:

```text
MOTOR:M1:ConnectionState
```

It is a `stringin` record using `DTYP="Ethernet/IP"` and
`INP="@etherip(EIP1)CONNECTION_STATE"`.

Raw input/output access is demonstrated by `db/genericEIPRaw.db`.

## Configuration API

```text
genericEipConfigure("EIP1", "192.168.5.246", 0.05,
                   0, 0, 0,
                   0, 101, 100,
                   40, 56,
                   10000, 10000)
genericEipConnect("EIP1")
```

`genericEipConnect` starts a background connection monitor. If the device is
unavailable, it remains in `CONNECTING`/`DISCONNECTED` and retries without
blocking IOC startup. When the device returns, the Class 1 session is opened
again and the status changes to `CONNECTED`.
