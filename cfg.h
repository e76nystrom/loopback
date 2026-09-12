#if !defined(CFG_H)
#define CFG_H

#define PICO_BUILD

#if defined(RTK_SEND)
inline char CLIENT_NAME[] = "cli3";
#define HOST_NAME CLIENT_NAME
#endif	/* RTK_SEND */

inline char SERVER_NAME[] = "srv3";

#if defined(RTK_RECV)
#define HOST_NAME SERVER_NAME
#endif	/* RTK_RECV */

#define PORT 8088

#endif
