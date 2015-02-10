#ifndef __PLOADER_H__
#define __PLOADER_H__

#ifdef __cplusplus
extern "C" 
{
#endif

#include <stdint.h>

#define LOAD_PHASE_HANDSHAKE            0
#define LOAD_PHASE_RESPONSE             1
#define LOAD_PHASE_VERSION              2
#define LOAD_PHASE_HANDSHAKE_DONE       3
#define LOAD_PHASE_PROGRAM              4
#define LOAD_PHASE_EEPROM_WRITE         5
#define LOAD_PHASE_EEPROM_VERIFY        6
#define LOAD_PHASE_DONE                 7

#define LOAD_STS_OK                     0
#define LOAD_STS_ERROR                  -1
#define LOAD_STS_TIMEOUT                -2

#define LOAD_TYPE_SHUTDOWN              0
#define LOAD_TYPE_RUN                   1
#define LOAD_TYPE_EEPROM                2
#define LOAD_TYPE_EEPROM_RUN            3

/* constants taken from the Propeller Tool loader */
#define BaudRate                        115200  // Likely Baud rate of Propeller communication
#define ResetPulsePeriod                25      // Reset pulse size (in ms)
#define MinResetDelay                   60      // Minimum post-reset delay
#define MaxResetDelay                   500     // Maximum post-reset delay

/* Transmit buffer size is 32K / 4 = 8K longs * 11 bytes per long plus two longs for the command and size */
#define TxBufSize                       ((((1024 * 32) / 4) + 2) * 11)

/* Receive buffer is large enough to receive max possible bytes during reset + 250 bytes for handshake response */
#define RxBufSize                       (((BaudRate / 10 * (ResetPulsePeriod + MaxResetDelay) / 1000) & 0xFFFFFFFE) + 258)

/* loader state structure - filled in by the loader functions */
typedef struct {

    /* serial driver interface */
    void (*reset)(void *data);
    int (*tx)(void *data, uint8_t* buf, int n);
    int (*rx_timeout)(void *data, uint8_t* buf, int n, int timeout);
    void (*msleep)(void *data, int msecs);
    void *serialData;
    
    /* load progress interface */
    void (*progress)(void *data, int phase);
    void *progressData;
    
    /* internal variables */
    uint8_t txbuf[TxBufSize];
    int txcnt;
    uint8_t rxbuf[RxBufSize];
    int rxnext;
    int rxcnt;
    uint8_t lfsr;
} PL_state;

/* PL_Init - Initializes the loader state structure. */
void PL_Init(PL_state *state);

/* PL_HardwareFound - Sends the handshake sequence and returns non-zero if a Propeller
   chip is found on the serial interface and also sets the version parameter to the
   chip version.
*/
int PL_HardwareFound(PL_state *state, int *pVersion);

/* PL_LoadSpinBinary - Loads a Spin binary image. Must be called immediatel following a
   successful call to PL_HardwareFound.
*/
int PL_LoadSpinBinary(PL_state *state, int loadType, uint8_t *image, int size);

/* PL_Shutdown - Shutdown the loader.*/
void PL_Shutdown(PL_state *state);

#ifdef __cplusplus
}
#endif

#endif
