Kt: The "Kate" Kernel Toolkit
=============================

Kt aims to be a portable, permissively licensed library for preserving
and supporting historical computer architectures and devices across
multiple operating systems. It is intended to reduce the maintenance
cost of long-tail and historical architecture ports, while allowing
performance-critical architectures to continue using native kernel
implementations.

Kt consists of `HAL: Hardware Abstraction Layer` and the `DDL: Device
Driver Library`. HAL is also an acronym for `Historical Architecture
Library`.

HAL provides a small set of low-level primitives for CPU context
management, virtual memory, interrupts, timers, I/O, and machine
initialization. Operating systems retain control over scheduling,
process management, filesystems, drivers, and other policy decisions.

DDL provides device drivers for historical major and minor devices
through its abstracted driver framework. Operating systems are able to
implement concrete device drivers through `DDL`.

The deliverables of this project may be shared by operating systems
and execution environments including Linux, BSD systems, standalone
diagnostic environments, and independent POSIX-compatible kernels.
HCL exists to preserve hardware support independently of any single
kernel's internal architecture.

Kt also implements an original BSD implementation for testing,
KateBSD.

Our slogan is "Hardware fades. Knowledge lasts."
