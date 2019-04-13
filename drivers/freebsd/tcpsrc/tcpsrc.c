#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/sockopt.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/filedesc.h>
#include <netinet/tcp.h>

#include "tcpsrc.h"

#define DISALLOWED_RLOCK()   rw_rlock(&disallowed_ranges_lock_)
#define DISALLOWED_RUNLOCK() rw_runlock(&disallowed_ranges_lock_)
#define DISALLOWED_WLOCK()   rw_wlock(&disallowed_ranges_lock_)
#define DISALLOWED_WUNLOCK() rw_wunlock(&disallowed_ranges_lock_)

static d_ioctl_t     tcpsrc_ioctl;
static d_ioctl_t     tcpctl_ioctl;

struct range_entry {
	SLIST_ENTRY(range_entry) link;
	struct tcpsrc_range range;
};

static struct cdev *tcpsrc_dev_;
static struct cdev *tcpctl_dev_;
static SLIST_HEAD(, range_entry) disallowed_ranges_;
static struct rwlock disallowed_ranges_lock_;
static MALLOC_DEFINE(M_TCPSRC, "tcpsrc", "tcpsrc");

static struct cdevsw tcpsrc_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl   = tcpsrc_ioctl,
	.d_name    = "tcpsrc",
};

static struct cdevsw tcpctl_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl   = tcpctl_ioctl,
	.d_name    = "tcpctl",
};

static int
add_disallowed_range(struct tcpsrc_range *range)
{
	struct range_entry *re;

	if (range->first.sa.sa_family != range->last.sa.sa_family)
		return (EINVAL);

	if (range->first.sa.sa_family != AF_INET &&
			range->first.sa.sa_family != AF_INET6)
		return (EINVAL);

	re = malloc(sizeof(*re), M_TCPSRC, M_WAITOK);
	if (re == NULL)
		return (ENOMEM);

	re->range = *range;
	DISALLOWED_WLOCK();
	SLIST_INSERT_HEAD(&disallowed_ranges_, re, link);
	DISALLOWED_WUNLOCK();
	return 0;
}

static void
clear_disallowed_ranges()
{
	struct range_entry *re;
	struct range_entry *tmpre;

	DISALLOWED_WLOCK();
	SLIST_FOREACH_SAFE(re, &disallowed_ranges_, link, tmpre)
		free(re, M_TCPSRC);

	DISALLOWED_WUNLOCK();
}

static int
range_matches(struct tcpsrc_range *range, struct tcpsrc_conn *conn)
{
	int af;
	int cmp;

	if (range->first.sa.sa_family != range->last.sa.sa_family)
		return 0;
	else if (range->first.sa.sa_family != conn->u.sa.sa_family)
		return 0;

	af = conn->u.sa.sa_family;
	if (af == AF_INET) {
		cmp = memcmp(&conn->u.sin.sin_addr,
				&range->last.sin.sin_addr,
				sizeof(struct in_addr));
		if (cmp > 0)
			return 0;

		cmp = memcmp(&conn->u.sin.sin_addr,
				&range->first.sin.sin_addr,
				sizeof(struct in_addr));
		if (cmp >= 0)
			return 1;
	} else if (af == AF_INET6) {
		cmp = memcmp(&conn->u.sin6.sin6_addr,
				&range->last.sin6.sin6_addr,
				sizeof(range->last.sin6.sin6_addr));
		if (cmp > 0)
			return 0;

		cmp = memcmp(&conn->u.sin6.sin6_addr,
				&range->first.sin6.sin6_addr,
				sizeof(range->first.sin6.sin6_addr));
		if (cmp >= 0)
			return 1;
	}

	return 0;
}

static int
is_allowed_conn(struct tcpsrc_conn *conn)
{
	struct range_entry *re;

	DISALLOWED_RLOCK();
	SLIST_FOREACH(re, &disallowed_ranges_, link) {
		if (range_matches(&re->range, conn)) {
			DISALLOWED_RUNLOCK();
			return 0;
		}
	}

	DISALLOWED_RUNLOCK();
	return 1;
}

static int
_setsockopt(struct socket *so, int level, int name, int val)
{
	struct sockopt opt = {
		.sopt_dir = SOPT_SET,
		.sopt_level = level,
		.sopt_name = name,
		.sopt_val = &val,
		.sopt_valsize = sizeof(val),
	};

	return sosetopt(so, &opt);
}

/* create a non-blocking socket w/ O_CLOEXEC set and connect(2) it */
static int
_connect(struct thread *td, struct sockaddr *sa, int *outfd)
{
	struct socket *so;
	struct file *fp;
	int fd;
	int fflag = FNONBLOCK; /* NB: non-blocking socket by default */
	int ret;

	/* the falloc,finit,&c are defined in sys/filedesc.h and
	 * implemented in kern/kern_descrip.c. The so* functions are
	 * defined in sys/socketvar.h and implemented in
	 * kern/uipc_socket.c. Their functionality is documented there. */

	/* allocate a struct file and a file descriptor w/ O_CLOEXEC
	 * set */
	ret = falloc(td, &fp, &fd, O_CLOEXEC);
	if (ret != 0)
		return (ret);

	/* alloc and initialize a struct socket. Put it in 'so' */
	ret = socreate(sa->sa_family, &so, SOCK_STREAM, IPPROTO_TCP,
            td->td_ucred, td);
	if (ret != 0) {
		fdclose(td, fp, fd);
		goto done;
	}

	/* associate the socket with the file, set FNONBLOCK */
	finit(fp, FREAD | FWRITE | FNONBLOCK, DTYPE_SOCKET, so, &socketops);
	fo_ioctl(fp, FIONBIO, &fflag, td->td_ucred, td);

	/* Set SO_REUSEADDR to 1 - allowing us to rebind on addr:port
         * pairs in TIME_WAIT state */
	ret = _setsockopt(so, SOL_SOCKET, SO_REUSEADDR, 1);
	if (ret != 0) {
		fdclose(td, fp, fd);
		goto done;
	}

	/* Set TCP_NODELAY to 1.
         * If the connection has non-ACKed data in transit, the TCP
         * stack may buffer data passed from userspace before sending it,
	 * if the size of the data in the send buffer is less than the MSS.
	 * By turning TCP_NODELAY on, we disable this functionality
	 * (Nagle's algorithm) and data passed to the kernel will be sent
	 * ASAP.
	 * We assume that userspace does not do a lot of calls to write(2)
	 * with partial data. Userspace should put the data in a buffer
         * first and/or use writev(2) on all available, relevant data */
	ret = _setsockopt(so, IPPROTO_TCP, TCP_NODELAY, 1);
	if (ret != 0) {
		fdclose(td, fp, fd);
		goto done;
	}

	/* connect! */
	ret = soconnect(so, sa, td);
	if (ret != 0) {
		so->so_state &= ~SS_ISCONNECTING;
		fdclose(td, fp, fd);
		goto done;
	}

	*outfd = fd;
done:
	fdrop(fp, td);
	return (ret);
}

static int
tcpsrc_connect(struct thread *td, struct tcpsrc_conn *conn, int *fd)
{
	/* validate address family and patch up sa_len */
	if (conn->u.sa.sa_family == AF_INET)
		conn->u.sa.sa_len = sizeof(struct sockaddr_in);
	else if (conn->u.sa.sa_family == AF_INET6)
		conn->u.sa.sa_len = sizeof(struct sockaddr_in6);
	else
		return (EINVAL);

	/* check permissions */
	if (!is_allowed_conn(conn))
		return (EACCES);

	return _connect(td, &conn->u.sa, fd);
}

static int
tcpsrc_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
		struct thread *td)
{
	int fd;
	int ret;

	switch (cmd) {
	case TCPSRC_CONNECT:
		ret = tcpsrc_connect(td, (struct tcpsrc_conn*)addr, &fd);
		if (ret == 0)
			td->td_retval[0] = fd;
		return ret;
	}

	return (EINVAL);
}

static int
tcpctl_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
		struct thread *td)
{
	switch (cmd) {
	case TCPSRC_DISALLOW_RANGE:
		return add_disallowed_range((struct tcpsrc_range *)addr);
	}

	return (EINVAL);
}

static int
tcpsrc_loader(struct module *m, int what, void *arg)
{
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		rw_init(&disallowed_ranges_lock_, "tcpsrc disallowed lock");

		error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
				&tcpsrc_dev_, &tcpsrc_cdevsw, 0,
				UID_ROOT, GID_WHEEL, 0660, "tcpsrc");
		if (error != 0)
			break;

		error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
				&tcpctl_dev_, &tcpctl_cdevsw, 0,
				UID_ROOT, GID_WHEEL, 0600, "tcpctl");
		if (error != 0)
			break;

		printf("tcpsrc: module loaded\n");
		break;
	case MOD_UNLOAD:
		if (tcpctl_dev_) {
			destroy_dev(tcpctl_dev_);
			tcpctl_dev_ = NULL;
		}

		if (tcpsrc_dev_) {
			destroy_dev(tcpsrc_dev_);
			tcpsrc_dev_ = NULL;
		}

		clear_disallowed_ranges();
		rw_destroy(&disallowed_ranges_lock_);
		printf("tcpsrc: module unloaded\n");
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

DEV_MODULE(tcpsrc, tcpsrc_loader, NULL);
