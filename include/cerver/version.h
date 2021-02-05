#ifndef _CERVER_VERSION_H_
#define _CERVER_VERSION_H_

#include "cerver/config.h"

#define CERVER_VERSION					"1.7b-6"
#define CERVER_VERSION_NAME				"Beta 1.7b-6"
#define CERVER_VERSION_DATE				"05/02/2021"
#define CERVER_VERSION_TIME				"07:51 CST"
#define CERVER_VERSION_AUTHOR			"Erick Salas"

#ifdef __cplusplus
extern "C" {
#endif

// print full cerver version information
CERVER_EXPORT void cerver_version_print_full (void);

// print the version id
CERVER_EXPORT void cerver_version_print_version_id (void);

// print the version name
CERVER_EXPORT void cerver_version_print_version_name (void);

#ifdef __cplusplus
}
#endif

#endif