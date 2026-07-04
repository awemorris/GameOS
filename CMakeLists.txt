cmake_minimum_required(VERSION 3.10)
project(vmunix LANGUAGES C ASM)

set(CMAKE_C_COMPILER   i686-linux-gnu-gcc)
set(CMAKE_LINKER       i686-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER i686-linux-gnu-gcc)

set(CMAKE_C_FLAGS          "-nostdlib -nostdinc -static -no-pie -O0 -I${CMAKE_CURRENT_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/user -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -ffreestanding")
set(CMAKE_AS_FLAGS         "-I${CMAKE_CURRENT_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/user")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -no-pie -O0 -e text_start -T ${CMAKE_CURRENT_SOURCE_DIR}/sections.lds -Wl,-Map mapfile.lst")

add_executable(
    gravity.elf
    sys/arch/x86/locore.S
    sys/arch/x86/dispatch.S
    sys/arch/x86/locore.S
    sys/arch/x86/trap.S
    sys/arch/x86/asm.c
    sys/arch/x86/clock.c
    sys/arch/x86/cons.c
    sys/arch/x86/cpu.c
    sys/arch/x86/int.c
    sys/arch/x86/irq.c
    sys/arch/x86/pic.c
    sys/arch/x86/pmem.c
    sys/arch/x86/task.c
    sys/arch/x86/univ.c
    sys/sched.c
    sys/testmain.c
    sys/crt/crtmalloc.c
    sys/crt/crtmisc.c
    sys/crt/crtprintf.c
    sys/crt/crtstring.c
)
