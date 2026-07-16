#ifndef SYS_TYPES_H
#define SYS_TYPES_H

/*
 * Port for i386.
 */
#if defined(HAL_ARCH_I386)

#define HAL_CONFIG_32BIT

typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef signed long long int64_t;
typedef signed long int32_t;
typedef signed short int16_t;
typedef signed char int8_t;

typedef unsigned long size_t;
typedef unsigned long clock_t;
typedef unsigned long off_t;

typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

#endif /* defined(HAL_ARCH_I386) */

/*
 * Port for amd64.
 */
#if defined(HAL_ARCH_AMD64)

#define HAL_CONFIG_64BIT

/* We don't assume long is 64-bit. */
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;

typedef signed long int64_t;
typedef signed int int32_t;
typedef signed short int16_t;
typedef signed char int8_t;

typedef unsigned long size_t;
typedef unsigned long clock_t;
typedef unsigned long off_t;

typedef uint32_t uintptr_t;
typedef int32_t intptr_t;

#endif /* defined(HAL_ARCH_I386) */

/*
 * Common
 */

#define NULL	((void*)0)

#endif
