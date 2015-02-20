#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "port.h"
#include "ploader.h"
#include "osint.h"

/* CheckPort state structure */
typedef struct {
    PL_state *state;
    int baud;
    int verbose;
    char *actualport;
} CheckPortInfo;

void ShowPorts(PL_state *state, char *prefix);
int InitPort(PL_state *state, char *prefix, char *port, int baud, int verbose, char *actualport);

static int ShowPort(const char *port, void *data);
static int CheckPort(const char *port, void *data);
static int OpenPort(PL_state *state, const char *port, int baud);
static void cb_reset(void *data);
static int cb_tx(void *data, uint8_t* buf, int n);
static int cb_rx_timeout(void *data, uint8_t* buf, int n, int timeout);
static void cb_msleep(void *data, int msecs);
static void cb_progress(void *data, int phase);

void InitPortState(PL_state *state)
{
    PL_Init(state);
    state->reset = cb_reset;
    state->tx = cb_tx;
    state->rx_timeout = cb_rx_timeout;
    state->progress = cb_progress;
    state->msleep = cb_msleep;
#ifdef RASPBERRY_PI
{
    char cmd[20] = "gpio,17,0";
    // use_reset_method uses strtok to parse the string so it can't be a constant
    use_reset_method(cmd);
}
#endif
}

void ShowPorts(PL_state *state, char *prefix)
{
    serial_find(prefix, ShowPort, state);
}

int InitPort(PL_state *state, char *prefix, char *port, int baud, int verbose, char *actualport)
{
    int result;
    
    if (port) {
        if (actualport) {
            strncpy(actualport, port, PATH_MAX - 1);
            actualport[PATH_MAX - 1] = '\0';
        }
        result = OpenPort(state, port, baud);
    }
    else {
        CheckPortInfo info;
        info.state = state;
        info.baud = baud;
        info.verbose = verbose;
        info.actualport = actualport;
        if (serial_find(prefix, CheckPort, &info) == 0)
            result = CHECK_PORT_OK;
        else
            result = CHECK_PORT_NO_PROPELLER;
    }
        
    return result;
}

static int ShowPort(const char *port, void *data)
{
    printf("%s\n", port);
    return 1;
}

static int CheckPort(const char *port, void *data)
{
    CheckPortInfo *info = (CheckPortInfo*)data;
    int rc;
    if (info->verbose)
        printf("Trying %s                    \n", port); fflush(stdout);
    if ((rc = OpenPort(info->state, port, info->baud)) != CHECK_PORT_OK)
        return rc;
    if (info->actualport) {
        strncpy(info->actualport, port, PATH_MAX - 1);
        info->actualport[PATH_MAX - 1] = '\0';
    }
    return 0;
}

static int OpenPort(PL_state *state, const char *port, int baud)
{
    /* open the port */
    if (serial_init(port, baud) == 0)
        return CHECK_PORT_OPEN_FAILED;
        
    /* check for a propeller on this port */
    if (PL_HardwareFound(state, &state->version) != LOAD_STS_OK) {
        serial_done();
        return CHECK_PORT_NO_PROPELLER;
    }

    return CHECK_PORT_OK;
}

static void cb_reset(void *data)
{
    hwreset();
}

static int cb_tx(void *data, uint8_t* buf, int n)
{
    return tx(buf, n);
}

static int cb_rx_timeout(void *data, uint8_t* buf, int n, int timeout)
{
    return rx_timeout(buf, n, timeout);
}

static void cb_msleep(void *data, int msecs)
{
    msleep(msecs);
}

static void cb_progress(void *data, int phase)
{
    switch (phase) {
    case LOAD_PHASE_HANDSHAKE:
        break;
    case LOAD_PHASE_RESPONSE:
        break;
    case LOAD_PHASE_VERSION:
        break;
    case LOAD_PHASE_HANDSHAKE_DONE:
        break;
    case LOAD_PHASE_PROGRAM:
        printf("Loading hub memory ... ");
        fflush(stdout);
        break;
    case LOAD_PHASE_EEPROM_WRITE:
        printf("OK\nWriting EEPROM ... ");
        fflush(stdout);
        break;
    case LOAD_PHASE_EEPROM_VERIFY:
        printf("OK\nVerifying EEPROM ... ");
        fflush(stdout);
        break;
    case LOAD_PHASE_DONE:
        break;
    default:
        break;
    }
}
