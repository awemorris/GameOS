cmake_minimum_required(VERSION 3.16)
project(s3os LANGUAGES C ASM)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER   i686-linux-gnu-gcc)
set(CMAKE_LINKER       i686-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER i686-linux-gnu-gcc)

set(CMAKE_C_FLAGS          "-nostdlib -nostdinc -static -no-pie -O0 -I${CMAKE_CURRENT_SOURCE_DIR}/sys/include -fno-builtin -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-exceptions -ffreestanding")
set(CMAKE_AS_FLAGS         "-I${CMAKE_CURRENT_SOURCE_DIR}/include -I${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CMAKE_EXE_LINKER_FLAGS "-nostdlib -static -no-pie -O0 -e text_start -T ${CMAKE_CURRENT_SOURCE_DIR}/sections.lds -Wl,-Map mapfile.lst")

set(KERN_SRC)

#
# CPU: i386
#
if(KERN_CPU_I386)
  set(KERN_CPU HAL_CPU_80386)
  list(APPEND KERN_SRC
    sys/hal/i386/locore.S
    sys/hal/i386/dispatch.S
    sys/hal/i386/locore.S
    sys/hal/i386/trap.S
    sys/hal/i386/asm.c
    sys/hal/i386/cpu.c
    sys/hal/i386/int.c
    sys/hal/i386/irq.c
    sys/hal/i386/pmem.c
    sys/hal/i386/task.c
    sys/hal/i386/univ.c
  )
endif()

#
# Board: IBM PC/AT
#
if(KERN_BOARD_PCAT)
  set(KERN_BOARD HAL_BOARD_PCAT)
  list(APPEND KERN_SRC
    sys/hal/pcat/clock.c
    sys/hal/pcat/cons.c
    sys/hal/pcat/pic.c
  )
endif()

#
# Board: NEC PC-9800
#
if(KERN_BOARD_PC98)
  set(KERN_BOARD HAL_BOARD_PC98)
  list(APPEND KERN_SRC
    sys/hal/pc98/clock.c
    sys/hal/pc98/cons.c
    sys/hal/pc98/pic.c
  )
endif()

#
# Kernel C Runtime
#
list(APPEND KERN_SRC
  sys/kcrt/crtmalloc.c
  sys/kcrt/crtmisc.c
  sys/kcrt/crtprintf.c
  sys/kcrt/crtstring.c
)

#
# Scheduler
#
list(APPEND KERN_SRC
  sys/kern/sched.c
)

#
# Test Main
#
list(APPEND KERN_SRC
  sys/testmain.c
)

#
# Kernel
#
add_executable(
  kernel
  ${KERN_SRC}
)

target_include_directories(
  kernel
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sys/include
)

target_compile_definitions(
  kernel
  PRIVATE
  ${KERN_CPU}
  ${KERN_BOARD}
)
