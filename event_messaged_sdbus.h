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
#ifdef __cplusplus
}
#endif

#endif
