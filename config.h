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
#if !defined __BEOS__ && !defined _WIN32
#define USE_MMAP
#endif

#if defined(_WIN32)
#pragma warning(disable: 4996)
#endif

#if defined(_WIN64)
#define _OFF_T_DEFINED
typedef unsigned long long off_t;
#endif

/* Use memory alignment workaround or not */
#ifdef __ia64__
#define ALIGNMENT_WORKAROUND
#endif

#endif
