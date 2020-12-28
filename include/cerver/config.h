#ifndef _CERVER_CONFIG_H_
#define _CERVER_CONFIG_H_

#define CERVER_EXPORT 			extern	// to be used in the application
#define CERVER_PRIVATE			extern	// to be used by internal cerver methods
#define CERVER_PUBLIC 			extern	// used by private methods but can be used by the application

#ifdef __cplusplus
#define CERVER_INLINE inline
#else
#define CERVER_INLINE inline
#endif

#define DEPRECATED(func) func __attribute__ ((deprecated))

#if defined(__GNUC__) || defined(__clang__)
#define CERVER_ATTRS(x) __attribute__(x)
#else
#define CERVER_ATTRS(x)
#endif

#ifndef MIN
	#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
	#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef ARRAY_SIZE
	#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef ELEM_AT
	#define ELEM_AT(a, i, v) ((unsigned int) (i) < ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

// On some platforms, max() may already be defined
#ifndef max
	#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

// va_copy is a C99 feature. In C89 implementations, it's sometimes
// available as __va_copy. If not, memcpy() should do the trick
#ifndef va_copy
	#ifdef __va_copy
		#define va_copy __va_copy
	#else
		#define va_copy(a, b) memcpy(&(a), &(b), sizeof(va_list))
	#endif
#endif

#endif