#include <string.h>
#include "ploader.h"

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

/* timeouts in milliseconds */
#define ACK_TIMEOUT                 25
#define CHECKSUM_TIMEOUT            10000   // big because it includes program loading
#define EEPROM_PROGRAMMING_TIMEOUT  5000
#define EEPROM_VERIFICATION_TIMEOUT 2000

#define CHECKSUM_RETRIES            (CHECKSUM_TIMEOUT / ACK_TIMEOUT)
#define EEPROM_PROGRAMMING_RETRIES  (EEPROM_PROGRAMMING_TIMEOUT / ACK_TIMEOUT)
#define EEPROM_VERIFICATION_RETRIES (EEPROM_VERIFICATION_TIMEOUT / ACK_TIMEOUT)

static int WaitForAck(PL_state *state, int retries);
static void SerialInit(PL_state *state);
static void TByte(PL_state *state, uint8_t x);
static void TLong(PL_state *state, uint32_t x);
static void TComm(PL_state *state);
static int RBit(PL_state *state, int timeout);
static int IterateLFSR(PL_state *state);

void PL_Init(PL_state *state)
{
    memset(state, 0, sizeof(PL_state));
}

/* PL_Shutdown - shutdown the loader */
void PL_Shutdown(PL_state *state)
{
    TLong(state, LOAD_TYPE_SHUTDOWN);
    TComm(state);
}

/* PL_LoadSpinBinary - load a spin binary using the rom loader */
int PL_LoadSpinBinary(PL_state *state, int loadType, uint8_t *image, int size)
{
    int i, sts;
    
    TLong(state, loadType);
    TLong(state, size / sizeof(uint32_t));
    
    /* download the spin binary */
    for (i = 0; i < size; i += 4) {
        
        /* report load progress */
        if (state->progress && (i % 1024) == 0)
            (*state->progress)(state->progressData, LOAD_PHASE_PROGRAM, i, size);
    
        /* transmit the next long */
        uint32_t data = image[i] | (image[i + 1] << 8) | (image[i + 2] << 16) | (image[i + 3] << 24);
        TLong(state, data);
    }
    TComm(state);
    
    /* wait for an ACK indicating a successful load */
    if ((sts = WaitForAck(state, CHECKSUM_RETRIES)) < 0)
        return LOAD_STS_TIMEOUT;
    else if (sts == 0)
        return LOAD_STS_ERROR;
    
    /* wait for eeprom programming and verification */
    if (loadType == LOAD_TYPE_EEPROM || loadType == LOAD_TYPE_EEPROM_RUN) {
    
        /* report the start of the eeprom writing phase */
        if (state->progress)
            (*state->progress)(state->progressData, LOAD_PHASE_EEPROM_WRITE, 0, size);

        /* wait for an ACK indicating a successful EEPROM programming */
        if ((sts = WaitForAck(state, EEPROM_PROGRAMMING_RETRIES)) < 0)
            return LOAD_STS_TIMEOUT;
        else if (sts == 0)
            return LOAD_STS_ERROR;
    
        /* report the start of the eeprom verification phase */
        if (state->progress)
            (*state->progress)(state->progressData, LOAD_PHASE_EEPROM_VERIFY, 0, size);

        /* wait for an ACK indicating a successful EEPROM verification */
        if ((sts = WaitForAck(state, EEPROM_VERIFICATION_RETRIES)) < 0)
            return LOAD_STS_TIMEOUT;
        else if (sts == 0)
            return LOAD_STS_ERROR;
    }
    
    /* report load completion */
    if (state->progress)
        (*state->progress)(state->progressData, LOAD_PHASE_DONE, 0, size);

    return LOAD_STS_OK;
}

static int WaitForAck(PL_state *state, int retries)
{
    uint8_t buf[1];
    while (--retries >= 0) {
        TByte(state, 0xf9);
        TComm(state);
        if ((*state->rx_timeout)(state->serialData, buf, 1, ACK_TIMEOUT) > 0)
            return buf[0] == 0xfe;
    }
    return -1; // timeout
}

/* this code is adapted from Chip Gracey's PNut IDE */

int PL_HardwareFound(PL_state *state, int *pVersion)
{
    int version, i;
        
    /* initialize the serial buffers */
    SerialInit(state);
    
    /* report the start of the handshake phase */
    if (state->progress)
        (*state->progress)(state->progressData, LOAD_PHASE_HANDSHAKE, 0, 0);

    /* reset the propeller (includes post-reset delay of 100ms) */
    (*state->reset)(state->serialData);
    
    /* transmit the calibration pulses */
    TByte(state, 0xf9);
    
    /* transmit the handshake pattern */
    state->lfsr = 'P';
    for (i = 0; i < 250; ++i)
        TByte(state, IterateLFSR(state) | 0xfe);

    /* transmit calibration pulses to clock out the connection response and the version byte */
    for (i = 0; i < 250 + 8; ++i)
        TByte(state, 0xf9);
        
    /* flush the transmit buffer */
    TComm(state);
    
    /* report the start of the handshake response phase */
    if (state->progress)
        (*state->progress)(state->progressData, LOAD_PHASE_RESPONSE, 0, 0);

    /* receive the connection response */
    for (i = 0; i < 250; ++i) {
        int bit = RBit(state, 100);
        if (bit < 0)
            return LOAD_STS_TIMEOUT;
        else if (bit != IterateLFSR(state))
            return LOAD_STS_ERROR;
    }
        
    /* report the start of the version phase */
    if (state->progress)
        (*state->progress)(state->progressData, LOAD_PHASE_VERSION, 0, 0);

    /* receive the chip version */
    for (version = i = 0; i < 8; ++i) {
        int bit = RBit(state, 50);
        if (bit < 0)
            return LOAD_STS_TIMEOUT;
        version = ((version >> 1) & 0x7f) | (bit << 7);
    }
    *pVersion = version;
        
    /* report handshake completion */
    if (state->progress)
        (*state->progress)(state->progressData, LOAD_PHASE_HANDSHAKE_DONE, 0, 0);

    /* return successfully */
    return LOAD_STS_OK;
}

/* SerialInit - initialize the serial buffers */
static void SerialInit(PL_state *state)
{
    state->txcnt = 0;
    state->rxnext = 0;
    state->rxcnt = 0;
}

/* TByte - add a byte to the transmit buffer */
static void TByte(PL_state *state, uint8_t x)
{
    state->txbuf[state->txcnt++] = x;
    if (state->txcnt >= sizeof(state->txbuf))
        TComm(state);
}

/* TLong - add a long to the transmit buffer */
static void TLong(PL_state *state, uint32_t x)
{
    int i;
    for (i = 0; i < 11; ++i) {

#if 0
        // p2 code
        uint8_t byte = 0x92
                     | ((i == 10 ? -1 : 0) & 0x60)
                     | ((x >> 31) & 1)
                     | (((x >> 30) & 1) << 3)
                     | (((x >> 29) & 1) << 6);
        TByte(state, byte);
        x <<= 3;
#else
        // p1 code
        uint8_t byte = 0x92
                     | ((i == 10 ? -1 : 0) & 0x60)
                     |  (x & 1)
                     | ((x & 2) << 2)
                     | ((x & 4) << 4);
        TByte(state, byte);
        x >>= 3;
#endif

    }
}

/* TComm - write the transmit buffer to the port */
static void TComm(PL_state *state)
{
    (*state->tx)(state->serialData, state->txbuf, state->txcnt);
    state->txcnt = 0;
}

/* RBit - receive a bit with a timeout */
static int RBit(PL_state *state, int timeout)
{
    int result;
    for (;;) {
        if (state->rxnext >= state->rxcnt) {
            state->rxcnt = (*state->rx_timeout)(state->serialData, state->rxbuf, sizeof(state->rxbuf), timeout);
            if (state->rxcnt <= 0) {
                /* hardware lost */
                return -1;
            }
            state->rxnext = 0;
        }
        result = state->rxbuf[state->rxnext++] - 0xfe;
        if ((result & 0xfe) == 0)
            return result;
        /* checksum error */
    }
}

/* IterateLFSR - get the next bit in the lfsr sequence */
static int IterateLFSR(PL_state *state)
{
    uint8_t lfsr = state->lfsr;
    int result = lfsr & 1;
    state->lfsr = ((lfsr << 1) & 0xfe) | (((lfsr >> 7) ^ (lfsr >> 5) ^ (lfsr >> 4) ^ (lfsr >> 1)) & 1);
    return result;
}

/* end of code adapted from Chip Gracey's PNut IDE */
