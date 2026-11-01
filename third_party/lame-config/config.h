/* Silente: hand-written config for building LAME 3.100's libmp3lame with CMake (macOS
   arm64 + x86_64) without running its autotools configure. Encoder only (no mpglib). */
#ifndef SILENTE_LAME_CONFIG_H
#define SILENTE_LAME_CONFIG_H
#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_LIMITS_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define HAVE_IEEE754_FLOAT32_T 1
typedef float ieee754_float32_t;
#define PACKAGE "lame"
#define VERSION "3.100"
#endif
