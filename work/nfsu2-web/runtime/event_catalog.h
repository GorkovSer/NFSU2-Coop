#ifndef EVENT_CATALOG_H
#define EVENT_CATALOG_H
#include <stdint.h>
#define RC_EXT_CAREER_MAP 64u
#define RC_EVENT_CATALOG_MAX 64
typedef struct {
 uint32_t to,source,destination,serial;
 uint8_t stage,n;
 uint32_t event[RC_EVENT_CATALOG_MAX];
} rc_event_catalog;
#endif
