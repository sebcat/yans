#ifndef TCPSRC_DRIVER_H__
#define TCPSRC_DRIVER_H__

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioccom.h>

#define TCPSRCF_BLOCKING (1 << 0) /* */

struct tcpsrc_conn {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} u;
};

struct tcpsrc_range {
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} first;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} last;

};

#define TCPSRC_MAGIC '8'
#define TCPSRC_CONNECT  _IOW(TCPSRC_MAGIC, 0, struct tcpsrc_conn)
#define TCPSRC_DISALLOW_RANGE  _IOW(TCPSRC_MAGIC, 0, struct tcpsrc_range)

#endif
