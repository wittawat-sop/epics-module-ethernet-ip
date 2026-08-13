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
- CMake (used to build the bundled static EIPScanner library)

Set the dependency paths in `configure/RELEASE`. The default configuration
expects these packages under `/usr/local/epics`.

## Build

Before building, edit [`configure/RELEASE`](configure/RELEASE) and set the
paths for `EPICS_BASE`, `ASYN`, and `EIPSCANNER` for your environment. The
top-level [`Makefile`](Makefile) documents these settings and provides a
configuration check target.

Check the active paths with:

```sh
make show-config
```

From the module root:

```sh
make
```

The top-level build automatically performs this sequence:

1. Configure the bundled EIPScanner CMake project when needed.
2. Build `third_party/EIPScanner/build/libEIPScanner.a`.
3. Build and link the EPICS `genericEIP` support library and IOC.

To build only the bundled EIPScanner library:

```sh
make eipscanner
```

If CMake is installed at a non-standard path, set `CMAKE` when invoking make:

```sh
make CMAKE=/opt/cmake/bin/cmake
```

To also generate the example IOC startup files, use:

```sh
make ioc
```

After changing `configure/RELEASE`, run `make show-config` again to verify
the paths. Do not edit generated `O.*` directories.

Build products are installed under `bin/` and `lib/`; these directories are
ignored by Git.

## Example IOC

สำหรับการสร้าง IOC ใหม่ด้วย `makeBaseApp.pl` และเพิ่ม genericEIP เป็น module
ให้ดูที่ [docs/CREATE_IOC.md](docs/CREATE_IOC.md)

The example IOC is in `iocBoot/iocGenericEIP`. Set `EIP_IP` in
`iocBoot/iocGenericEIP/st.cmd` before running. The assembly IDs, sizes, RPIs,
and device IP are all configured in that file. Build and run:

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
uses the documentation-only example address `192.0.2.10`; replace it with
the IP address of your EtherNet/IP device.

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
genericEipConfigure("EIP1", "192.0.2.10", 0.05,
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
