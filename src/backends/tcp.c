// TCP backend for SMeCoL
// It is hacky.
//
// It either opens a server/gateway
// or acts as a client.
// from/to are the client fds.
// Probably gonna need more datatypes.
//
// NEEDS A RETHINK:
// - All methods need to handle different sockets/buffer pairs.
// - Client addressing.
// - Have a client-listing method/rework broadcasting. Need to be able to send a message to all clients except one. (Routing)

#include "tcp.h"
#include "../../include/smecol/types.h"
#include "../../include/smecol/macros.h"
#include "../../include/smecol/modules.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <netdb.h>

#define MAXIMUM_SIZE 0xFFFFFFFF
#define OVERHEAD (sizeof(uint32_t))
#define MAX (MAXIMUM_SIZE - OVERHEAD)

#define SET_NONBLOCK(ctx) ctx->flags |= O_NONBLOCK
#define GET_BLOCK(ctx) (ctx->flags & O_NONBLOCK)
#define SET_BLOCK(ctx) ctx->flags &= ~O_NONBLOCK
#define APPLY_FLAGS(ctx) fcntl(ctx->socket, F_SETFL, ctx->flags)

typedef struct tcp_ctx {
	int server; // if != 0, it's in server mode.
	int socket;
	int* sockets; // pointer to array of fds.
	size_t socket_count;
	int flags;
	fd_set fds;
	struct sockaddr_in sin;
	uint32_t size;
	uint32_t pos;
	void* buffer; // TODO: implement buffer for concurrent connections, not just one buffer.
} tcp_ctx;

#define SIZE(ctx) ntohl(ctx->sizebuf)

// malloc/free wrappers, yay
static void* tcp_alloc(smecol_bctx UNUSED(bctx), size_t size) {
	return malloc(size);
}
static void tcp_free(smecol_bctx UNUSED(bctx), void* ptr) {
	free(ptr);
}

static smecol_bctx tcp_init(void* arg) {
	// Option parsing stuff.
	if (!arg)
		return NULL;

	tcp_opts* opts = arg;
	tcp_ctx* ctx = calloc(1, sizeof(tcp_ctx));
	ctx->server = opts->is_server;

	int port = (int) strtol(opts->port, (char* *) NULL, 10);
	if (port == 0) {
		eprintf("tcp_init: arg doesn't contain a valid port\n");
		return NULL;
	}

	struct hostent* host;
	host = gethostbyname(opts->name);
	if (host == NULL) {
		eprintf("tcp_init: no such host\n");
		return NULL;
	}

	// Init socket.
	// Differentiate between listening socket
	// and a client socket here, based on the arg string.
	// Return context, on heap.

	memset((char*) &ctx->sin, 0, sizeof(struct sockaddr_in));
	ctx->sin.sin_family = AF_INET;
	memcpy(host->h_addr, &ctx->sin.sin_addr.s_addr, host->h_length);
	ctx->sin.sin_port = htons(port);

	if ((ctx->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		eprintf("tcp_init: failed to create socket\n");
		return NULL;
	}

	// Set set the socket blocking.
	ctx->flags = fcntl(ctx->socket, F_GETFL, 0);
	SET_NONBLOCK(ctx);
	APPLY_FLAGS(ctx);

	if (ctx->server) {
		FD_ZERO(&ctx->fds);
		FD_SET(ctx->socket, &ctx->fds);
	}

	return ctx;
}

static void tcp_deinit(smecol_bctx UNUSED(bctx)) {}

// connect/disconnect
static int tcp_connect(smecol_bctx bctx) {
	tcp_ctx* ctx = bctx;
	int retcode = 0;

	if (ctx->server) {
		if (bind(ctx->socket, (struct sockaddr*) &ctx->sin, sizeof(ctx->sin))) {
			eprintf("tcp_connect: failed to bind: %s\n", strerror(errno));
			return 1;
		}

		if (listen(ctx->server, SV_QUEUE)) {
			eprintf("tcp_connect: failed to listen: %s\n", strerror(errno));
			return 1;
		}

		// TODO: wait until we got at least a single client.

		return 0;
	}

	// Client.
	do {
		retcode = connect(ctx->socket, (struct sockaddr*) &ctx->sin, sizeof(struct sockaddr_in));
		if (retcode != 0 && (errno != EINPROGRESS || errno != EALREADY)) {
			if (errno != EINPROGRESS && errno != EALREADY) {
				eprintf("tcp_connect: failed to connect: %s\n", strerror(errno));
				return 1;
			}
			select(1, &ctx->fds, NULL, NULL, NULL);
			FD_SET(ctx->socket, &ctx->fds);
		}
	} while (retcode == EINPROGRESS);

	return 0;
}

static void tcp_disconnect(smecol_bctx bctx) {
	tcp_ctx* ctx = bctx;
	close(ctx->socket);
}

// Reading.
// This is by far the most complex part of this file.
// We need to handle blocking and nonblocking reads.
static int tcp_send(smecol_bctx bctx, void* to, void* buffer, size_t size) {
	tcp_ctx* ctx = bctx;

	uint32_t csize = size;
	*(uint32_t*) buffer = htonl(csize);
	send(ctx->socket, buffer, size + OVERHEAD, MSG_NOSIGNAL);
	return 0;
}

static inline int read_bytes_blocking(tcp_ctx* ctx, void* buf, size_t size) {
	while (ctx->pos < size) {
		select(1, &ctx->fds, NULL, NULL, NULL); // Blocking select. We know which FD.
		FD_SET(ctx->socket, &ctx->fds);
		ssize_t readbytes = read(ctx->socket, (byte*) buf + ctx->pos, size - (1 + ctx->pos));
		if (readbytes < 0) {
			if (errno != EAGAIN) return errno;
		}
		ctx->pos += readbytes;
	}
	return 0;
}

struct timeval timeout_tval;
struct timezone tz;

#define TIMEOUT(msec) ((struct timeval) { .tv_sec = (msec) / 1000, .tv_usec = ((msec) % 1000) })
#define MSEC(timeout) ((1000 * (timeout).tv_sec) + (timeout).tv_usec)

static inline int read_bytes(tcp_ctx* ctx, void* buf, size_t size, uint timeout) {
	timeout_tval = TIMEOUT(timeout);

	struct timeval now;
	struct timeval timestamp;

	// get current timestamp to compare it to later
	if (gettimeofday(&timestamp, &tz) != 0) return errno;
	uint timestamp_msec = MSEC(timestamp);

	while (ctx->pos < size) {
		if (select(1, &ctx->fds, NULL, NULL, &timeout_tval) == 0) { // Non-Blocking select. We know which FD.
			// timeout
			FD_SET(ctx->socket, &ctx->fds);
			return 1;
		}
		FD_SET(ctx->socket, &ctx->fds);
		ssize_t readbytes = read(ctx->socket, (byte*) buf + ctx->pos, size - (1 + ctx->pos));
		if (readbytes < 0) {
			if (errno != EAGAIN) return errno;
		}
		ctx->pos += readbytes;

		if (gettimeofday(&now, &tz) != 0) return errno;
		timeout = MSEC(now) - timestamp_msec;
	}
	return 0;
}


static void* tcp_readb(smecol_bctx bctx, void** from, size_t* size) {
	tcp_ctx* ctx = bctx;

	int res = 0;
	// if we ran after a non-blocking thing, we might have a partial message.
	if (ctx->size) {
		*size = ctx->size;
		if ((res = read_bytes_blocking(ctx, ctx->buffer, *size)) != 0) {
			eprintf("tcp_readb: failed to read buffer (resumed from %u)\n", ctx->pos);
			return NULL;
		}
		ctx->pos = 0;
		return ctx->buffer;
	}

	// try to read size
	uint32_t sizebuf;
	if ((res = read_bytes_blocking(ctx, &sizebuf, OVERHEAD)) != 0) {
		eprintf("tcp_readb: failed to read, %i\n", res);
		return NULL;
	}
	ctx->pos = 0;

	*size = ntohl(sizebuf);
	if (*size > MAX) {
		eprintf("tcp_readb: size is over maximum? %lu\n", *size);
		return NULL;
	}
	void* buffer = malloc(*size);
	if (buffer == NULL) {
		eprintf("tcp_readb: failed to allocate buffer of size %lu\n", *size);
		return NULL;
	}

	ctx->size = *size;

	if ((res = read_bytes_blocking(ctx, buffer, *size)) != 0) {
		eprintf("tcp_readb: failed to read buffer\n");
		return NULL;
	}

	ctx->size = 0;

	return buffer;
}

static void* tcp_read(smecol_bctx bctx, void** from, size_t* size, uint msec_timeout) {
	tcp_ctx* ctx = bctx;

	int res = 0;
	// if we got the size already
	if (ctx->size) {
		*size = ctx->size;
		if ((res = read_bytes(ctx, ctx->buffer, *size, msec_timeout)) != 0) {
			if (res == 1) return NULL;
			eprintf("tcp_readb: failed to read buffer (resumed from %u)\n", ctx->pos);
			return NULL;
		}
		ctx->pos = 0;
		return ctx->buffer;
	}

	// try to read size
	uint32_t sizebuf;
	if ((res = read_bytes_blocking(ctx, &sizebuf, OVERHEAD)) != 0) {
		eprintf("tcp_readb: failed to read, %i\n", res);
		return NULL;
	}
	ctx->pos = 0;

	*size = ntohl(sizebuf);
	if (*size > MAX) {
		eprintf("tcp_readb: size is over maximum? %lu\n", *size);
		return NULL;
	}
	void* buffer = malloc(*size);
	if (buffer == NULL) {
		eprintf("tcp_readb: failed to allocate buffer of size %lu\n", *size);
		return NULL;
	}

	ctx->size = *size;

	if ((res = read_bytes_blocking(ctx, buffer, *size)) != 0) {
		if (res == 1) return NULL;
		eprintf("tcp_readb: failed to read buffer\n");
		return NULL;
	}

	return buffer;
}


static smecol_backend backend;
int smecol_tcp_register(void) {
	backend =
		(smecol_backend) {
		 .name = "tcp",
		 .max_size = MAX,
		 .offset = OVERHEAD,
		 .overhead = OVERHEAD,

		 .alloc = tcp_alloc,
		 .free = tcp_free,

		 .init = tcp_init,
		 .deinit = tcp_deinit,

		 .connect = tcp_connect,
		 .disconnect = tcp_disconnect,

		 .send = tcp_send,

		 .readb = tcp_readb,
		 .read = tcp_read,
		};

	smecol_register_backend(&backend);
	return 0;
}
