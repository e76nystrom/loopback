#if !defined(CFG_H)
#define CFG_H
#include <stdint.h>

#if defined(CLIENT)
#define HOST_NAME CLIENT_NAME
#endif	/* RTK_SEND */

#define SERVER_NAME "srv3"

#if defined(SERVER)
#define HOST_NAME SERVER_NAME
#endif	/* RTK_RECV */

#define PORT 8088

#endif
