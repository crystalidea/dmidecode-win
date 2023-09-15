/*
 * Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Default memory device file */
#if defined(__BEOS__) || defined(__HAIKU__)
#define DEFAULT_MEM_DEV "/dev/misc/mem"
#else
#ifdef __sun
#define DEFAULT_MEM_DEV "/dev/xsvc"
#else
#define DEFAULT_MEM_DEV "/dev/mem"
#endif
#endif

/* Use mmap or not */
#ifndef __BEOS__
#define USE_MMAP
#endif

/* Use memory alignment workaround or not */
#ifdef __ia64__
#define ALIGNMENT_WORKAROUND
#endif

/* Avoid unaligned memcpy on /dev/mem */
#ifdef __aarch64__
#define USE_SLOW_MEMCPY
#endif

#if defined(_MSC_VER)
#pragma warning(disable: 4996)
#endif

#if defined(_WIN32)
#undef USE_MMAP
#define __i386__
#endif

#if defined(_WIN64)
#define __x86_64__
#endif

#endif
