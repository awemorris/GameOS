Historical Computing Library
============================

HCL aims to be a portable, permissively licensed library for
preserving and supporting historical computer architectures and
devices across multiple operating systems. It is intended to reduce
the maintenance cost of long-tail and historical architecture ports,
while allowing performance-critical architectures to continue using
native kernel implementations.

HCL consists of `Low-Level HAL` and the `Bibliotheca Device Driver
Framework`.

HAL provides a small set of low-level primitives for CPU context
management, virtual memory, interrupts, timers, I/O, and machine
initialization. Operating systems retain control over scheduling,
process management, filesystems, drivers, and other policy decisions.

BDF provides device drivers for historical major and minor devices
through its abstracted driver framework. Operating systems are
able to implement concrete device drivers through `BDF`.

The deliverables of this project may be shared by operating systems
and execution environments including Linux, BSD systems, standalone
diagnostic environments, and independent POSIX-compatible kernels.
HCL exists to preserve hardware support independently of any single
kernel's internal architecture.

Our slogan is "Hardware fades. Knowledge lasts."
