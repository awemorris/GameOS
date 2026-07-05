cmake_minimum_required(VERSION 3.10)
project(s3os LANGUAGES C ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER   i686-linux-gnu-gcc)
set(CMAKE_LINKER       i686-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER i686-linux-gnu-gcc)

set(CMAKE_C_FLAGS          "-nostdlib -nostdinc -static -no-pie -O0 -I${CMAKE_CURRENT_SOURCE_DIR}/include -I${CMAKE_CURRENT_SOURCE_DIR}/src -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -ffreestanding")
set(CMAKE_AS_FLAGS         "-I${CMAKE_CURRENT_SOURCE_DIR}/include -I${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -no-pie -O0 -e text_start -T ${CMAKE_CURRENT_SOURCE_DIR}/sections.lds -Wl,-Map mapfile.lst")

add_executable(
    gravity

    src/sys/hal/i386/locore.S
    src/sys/hal/i386/dispatch.S
    src/sys/hal/i386/locore.S
    src/sys/hal/i386/trap.S
    src/sys/hal/i386/asm.c
    src/sys/hal/i386/clock.c
    src/sys/hal/i386/cons.c
    src/sys/hal/i386/cpu.c
    src/sys/hal/i386/int.c
    src/sys/hal/i386/irq.c
    src/sys/hal/i386/pic.c
    src/sys/hal/i386/pmem.c
    src/sys/hal/i386/task.c
    src/sys/hal/i386/univ.c

    src/sys/kcrt/crtmalloc.c
    src/sys/kcrt/crtmisc.c
    src/sys/kcrt/crtprintf.c
    src/sys/kcrt/crtstring.c

    src/sys/sched.c

    src/sys/testmain.c
)
