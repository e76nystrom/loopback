#if !defined(CFG_H)
#define CFG_H
#include <stdint.h>

#if defined(RTK_SEND)
#define CLIENT_NAME "cli3"
#define HOST_NAME CLIENT_NAME
#endif	/* RTK_SEND */

#define SERVER_NAME "srv3"

#if defined(RTK_RECV)
#define HOST_NAME SERVER_NAME
#endif	/* RTK_RECV */

#define PORT 8088

#endif
