#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "port.h"
#include "ploader.h"
#include "osint.h"

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif

/* port prefix */
#if defined(CYGWIN) || defined(WIN32) || defined(MINGW)
  #define PORT_PREFIX ""
#elif defined(LINUX)
  #ifdef RASPBERRY_PI
    #define PORT_PREFIX "ttyAMA"
  #else
    #define PORT_PREFIX "ttyUSB"
  #endif
#elif defined(MACOSX)
  #define PORT_PREFIX "cu.usbserial-"
#else
  #define PORT_PREFIX ""
#endif

/* defaults */
#define BAUD_RATE       115200

/* constants */
#define HUB_MEMORY_SIZE 32768

static PL_state state;

static void Usage(void);
static uint8_t *ReadEntireFile(char *name, long *pSize);

int main(int argc, char *argv[])
{
    char actualPort[PATH_MAX], *var, *val, *port, *p;
    int baudRate, baudRate2, verbose, terminalMode, pstMode, i;
    int loadType = LOAD_TYPE_RUN;
    int loadTypeOptionSeen = FALSE;
    char *file = NULL;
    long imageSize;
    uint8_t *image;
    
    /* initialize */
    baudRate = baudRate2 = BAUD_RATE;
    verbose = terminalMode = pstMode = FALSE;
    port = NULL;
    
    /* initialize the loader port */
    InitPortState(&state);
    
    /* process the position-independent arguments */
    for (i = 1; i < argc; ++i) {

        /* handle switches */
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'b':
                if (argv[i][2])
                    p = &argv[i][2];
                else if (++i < argc)
                    p = argv[i];
                else
                    Usage();
                if (*p != ':')
                    baudRate = baudRate2 = atoi(p);
                if ((p = strchr(p, ':')) != NULL) {
                    if (*++p != ':' && *p != '\0')
                        baudRate2 = atoi(p);
                }
                break;
            case 'D':
                if (argv[i][2])
                    p = &argv[i][2];
                else if (++i < argc)
                    p = argv[i];
                else
                    Usage();
                if ((var = strtok(p, "=")) != NULL) {
                    if ((val = strtok(NULL, "")) != NULL) {
                        if (strcmp(var, "reset") == 0)
                            use_reset_method(val);
                        else
                            Usage();
                    }
                    else
                        Usage();
                }
                else
                    Usage();
                break;
            case 'e':
                if (!loadTypeOptionSeen) {
                    loadTypeOptionSeen = TRUE;
                    loadType = 0;
                }
                loadType |= LOAD_TYPE_EEPROM;
                break;
            case 'p':
                if (argv[i][2])
                    port = &argv[i][2];
                else if (++i < argc)
                    port = argv[i];
                else
                    Usage();
#if defined(CYGWIN) || defined(WIN32) || defined(LINUX)
                if (isdigit((int)port[0])) {
#if defined(CYGWIN) || defined(WIN32)
                    static char buf[10];
                    sprintf(buf, "COM%d", atoi(port));
                    port = buf;
#endif
#if defined(LINUX)
                    static char buf[64];
                    sprintf(buf, "/dev/%s%d", PORT_PREFIX, atoi(port));
                    port = buf;
#endif
                }
#endif
#if defined(MACOSX)
                if (port[0] != '/') {
                    static char buf[64];
                    sprintf(buf, "/dev/%s-%s", PORT_PREFIX, port);
                    port = buf;
                }
#endif
                break;
            case 'P':
                ShowPorts(&state, PORT_PREFIX);
                break;
            case 'r':
                if (!loadTypeOptionSeen) {
                    loadTypeOptionSeen = TRUE;
                    loadType = 0;
                }
                loadType |= LOAD_TYPE_RUN;
                break;
            case 'T':
                pstMode = TRUE;
                // fall through
            case 't':
                terminalMode = TRUE;
                break;
            case 'v':
                verbose = TRUE;
                break;
            case '?':
                /* fall through */
            default:
                Usage();
                break;
            }
        }
    
        /* handle the input filename */
        else {
            if (file)
                Usage();
            file = argv[i];
        }
    }
    
    /* complain about nothing to do */
    if (!file && !terminalMode) {
        printf("error: must specify either a file to load or -t\n");
        return 1;
    }
        
    /* open the serial port */
    switch (InitPort(&state, PORT_PREFIX, port, baudRate, verbose, actualPort)) {
    case CHECK_PORT_OK:
        printf("Found propeller version %d on %s\n", state.version, actualPort);
        break;
    case CHECK_PORT_OPEN_FAILED:
        printf("error: opening serial port '%s'\n", port);
        perror("Error is ");
        return 1;
    case CHECK_PORT_NO_PROPELLER:
        if (port)
            printf("error: no propeller chip on port '%s'\n", port);
        else
            printf("error: can't find a port with a propeller chip\n");
        return 1;
    }
    
    /* check for a file to load */
    if (file) {
    
        /* read the entire file into a buffer */
        if (!(image = ReadEntireFile(file, &imageSize))) {
            printf("error: reading '%s'\n", file);
            return 1;
        }
        
        /* make sure the file isn't too big for hub memory */
        if (imageSize > HUB_MEMORY_SIZE) {
            printf("error: image too big for hub memory\n");
            return 1;
        }
        
        /* load the file from the memory buffer */
        printf("Loading '%s' (%ld bytes)\n", file, imageSize);
        switch (PL_LoadSpinBinary(&state, loadType, image, imageSize)) {
        case LOAD_STS_OK:
            printf("OK\n");
            break;
        case LOAD_STS_ERROR:
            printf("Error\n");
            return 1;
        case LOAD_STS_TIMEOUT:
            printf("Timeout\n");
            return 1;
        default:
            printf("Internal error\n");
            return 1;
        }
    }
    
    /* enter terminal mode if requested */
    if (terminalMode) {
        printf("[ Entering terminal mode. Type ESC or Control-C to exit. ]\n");
        fflush(stdout);
        if (baudRate2 != baudRate)
            serial_baud(baudRate2);
        terminal_mode(FALSE, pstMode);
    }

    return 0;
}

/* Usage - display a usage message and exit */
static void Usage(void)
{
printf("\
p1load - a simple loader for the propeller - %s, %s\n\
usage: p1load\n\
         [ -b baud ]               baud rate (default is %d)\n\
         [ -D var=val ]            set variable value\n\
         [ -e ]                    write a bootable image to EEPROM\n\
         [ -p port ]               serial port (default is to auto-detect the port)\n\
         [ -P ]                    list available serial ports\n\
         [ -r ]                    run the program after loading (default)\n\
         [ -t ]                    enter terminal mode after running the program\n\
         [ -T ]                    enter PST-compatible terminal mode\n\
         [ -v ]                    verbose output\n\
         [ -? ]                    display a usage message and exit\n\
         file                      file to load\n", VERSION, __DATE__, BAUD_RATE);
#ifdef RASPBERRY_PI
printf("\
\n\
This version supports resetting the Propeller with a GPIO pin with option: -Dreset=gpio,pin,level\n\
where \"pin\" is the GPIO number to use and \"level\" is the logic level, 0 or 1. This defaults to\n\
GPIO 17 and level 0.\n\
");
#endif
    exit(1);
}

/* ReadEntireFile - read an entire file into an allocated buffer */
static uint8_t *ReadEntireFile(char *name, long *pSize)
{
    uint8_t *buf;
    long size;
    FILE *fp;

    /* open the file in binary mode */
    if (!(fp = fopen(name, "rb")))
        return NULL;

    /* get the file size */
    fseek(fp, 0L, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    
    /* allocate a buffer for the file contents */
    if (!(buf = (uint8_t *)malloc(size))) {
        fclose(fp);
        return NULL;
    }
    
    /* read the contents of the file into the buffer */
    if (fread(buf, 1, size, fp) != size) {
        free(buf);
        fclose(fp);
        return NULL;
    }
    
    /* close the file ad return the buffer containing the file contents */
    *pSize = size;
    fclose(fp);
    return buf;
}
