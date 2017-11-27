#ifndef EVENT_MESSAGED_SDBUS_H
#define EVENT_MESSAGED_SDBUS_H

#include <stdint.h>
#include "message.H"

#ifdef __cplusplus
extern "C" {
#endif
int bus_build (EventManager* em);
void bus_cleaup (void);
void bus_mainloop (void);
void bus_on_create_log (const Log* log);
void bus_on_remove_log (const Log* log);
#ifdef __cplusplus
}
#endif

#endif
