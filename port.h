#ifndef __PORT_H__
#define __PORT_H__

#include "ploader.h"

/* CheckPort result codes */
enum {
    CHECK_PORT_OK,
    CHECK_PORT_OPEN_FAILED,
    CHECK_PORT_NO_PROPELLER
};

/* prototypes */
void InitPortState(PL_state *state);
void ShowPorts(PL_state *state, char *prefix);
int InitPort(PL_state *state, char *prefix, char *port, int baud, int verbose, char *actualport);

#endif