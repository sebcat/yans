#include <sys/types.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockopt.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/filio.h>
#include <sys/filedesc.h>
#include <netinet/tcp.h>

#include "tcpsrc.h"

#define MODULE_NAME "tcpsrc"

static d_open_t      tcpsrc_open;
static d_close_t     tcpsrc_close;
static d_ioctl_t     tcpsrc_ioctl;

static struct cdevsw tcpsrc_cdevsw = {
	.d_version = D_VERSION,
	.d_open    = tcpsrc_open,
	.d_close   = tcpsrc_close,
	.d_ioctl   = tcpsrc_ioctl,
	.d_name    = MODULE_NAME,
};

static struct cdev *tcpsrc_dev;

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

	/* associate the socket with the file, set FNONBLOCK (needed?) */
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
		so->so_state &= ~SS_ISCONNECTING; /* really needed? */
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
	/* patch up sa_len since it's not really standard */
	if (conn->u.sa.sa_family == AF_INET) {
		conn->u.sa.sa_len = sizeof(struct sockaddr_in);
	} else if (conn->u.sa.sa_family == AF_INET6) {
		conn->u.sa.sa_len = sizeof(struct sockaddr_in6);
	}

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
tcpsrc_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	return (0);
}

static int
tcpsrc_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{

	return (0);
}

static int
tcpsrc_loader(struct module *m, int what, void *arg)
{
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
				&tcpsrc_dev, &tcpsrc_cdevsw, 0,
				UID_ROOT, GID_WHEEL, 0660, MODULE_NAME);
		if (error != 0)
			break;
		printf("tcpsrc: module loaded\n");
		break;
	case MOD_UNLOAD:
		destroy_dev(tcpsrc_dev);
		printf("tcpsrc: module unloaded\n");
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

DEV_MODULE(tcpsrc, tcpsrc_loader, NULL);
