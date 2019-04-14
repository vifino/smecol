// IRC backend for SMeCoL
// It is crappy.
//
// It connects to IRC and sends base64 encoded messages.
//
// NEEDS WORK:
//   - Needs more error handling.
//   - Needs some publish/subscribe handling. After all, we have IRC channels.

#include "irc.h"
#include "../../include/smecol/types.h"
#include "../../include/smecol/macros.h"
#include "../../include/smecol/modules.h"
#include "../../include/smecol/b64.h"

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

#define MAX_LENGTH 512

#define SET_NONBLOCK(ctx) ctx->flags |= O_NONBLOCK
#define GET_BLOCK(ctx) (ctx->flags & O_NONBLOCK)
#define SET_BLOCK(ctx) ctx->flags &= ~O_NONBLOCK
#define APPLY_FLAGS(ctx) fcntl(ctx->socket, F_SETFL, ctx->flags)

typedef struct irc_ctx {
	int socket;
	fd_set fds;
	int flags;
	struct sockaddr_in sin;
	char buf[MAX_LENGTH];
	size_t pos;
	size_t size;
} irc_ctx;

#define SIZE(ctx) ntohl(ctx->sizebuf)

// malloc/free wrappers, yay
static void* irc_alloc(smecol_bctx UNUSED(bctx), size_t size) {
	return malloc(size);
}
static void irc_free(smecol_bctx UNUSED(bctx), void* ptr) {
	free(ptr);
}

// Get maximum message size in privmsg.
static size_t get_max_size(smecol_bctx UNUSED(bctx), void* to) {
	// In IRC, the max command length is 512.
	// PRIVMSG <TO> :<MESSAGE>\r\n
	// We convert the message to base64, so we loose 1/4th-ish.
	return BASE64_DECODED_SIZE(MAX_LENGTH - (10 + 2) - strlen(to));
}

static size_t get_overhead(smecol_bctx UNUSED(bctx), void* UNUSED(to), size_t size) {
	return (10 + 2) + BASE64_SIZE(size) - size;
}

static size_t get_offset(smecol_bctx UNUSED(bctx), void* to, size_t UNUSED(size)) {
	// PRIVMSG <to> :<MESSAGE>
	return 11 + strlen(to);
}

static smecol_bctx irc_init(void* arg) {
	// Option parsing stuff.
	if (!arg)
		return NULL;

	irc_opts* opts = arg;
	irc_ctx* ctx = calloc(1, sizeof(irc_ctx));

	if (opts->port == 0) {
		eprintf("irc_init: arg doesn't contain a valid port\n");
		return NULL;
	}

	struct hostent* host;
	host = gethostbyname(opts->server);
	if (host == NULL) {
		eprintf("irc_init: no such host: %s\n", opts->server);
		return NULL;
	}

	// Init socket.
	// Return context, on heap.

	memset((char*) &ctx->sin, 0, sizeof(struct sockaddr_in));
	ctx->sin.sin_family = AF_INET;
	memcpy(host->h_addr, &ctx->sin.sin_addr.s_addr, host->h_length);
	ctx->sin.sin_port = htons(opts->port);

	if ((ctx->socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		eprintf("irc_init: failed to create socket\n");
		return NULL;
	}

	// Set set the socket nonblocking.
	ctx->flags = fcntl(ctx->socket, F_GETFL, 0);
	SET_NONBLOCK(ctx);
	APPLY_FLAGS(ctx);

	return ctx;
}

static void irc_deinit(smecol_bctx bctx) {
	free(bctx);
}

// connect/disconnect
static int irc_connect(smecol_bctx bctx) {
	irc_ctx* ctx = bctx;
	int retcode = 0;

	// Connect.
	do {
		retcode = connect(ctx->socket, (struct sockaddr*) &ctx->sin, sizeof(struct sockaddr_in));
		if (retcode != 0 && (errno != EINPROGRESS || errno != EALREADY)) {
			if (errno != EINPROGRESS && errno != EALREADY) {
				eprintf("irc_connect: failed to connect: %s\n", strerror(errno));
				return 1;
			}
			select(1, &ctx->fds, NULL, NULL, NULL);
			FD_SET(ctx->socket, &ctx->fds);
		}
	} while (retcode == EINPROGRESS);

	char cbuf[MAX_LENGTH];

	return 0;
}

static void irc_disconnect(smecol_bctx bctx) {
	irc_ctx* ctx = bctx;
	close(ctx->socket);
}

// Communications.
// This is by far the most complex part of this file.
// We need to handle blocking and nonblocking reads, too.
static int irc_send(smecol_bctx bctx, void* to, void* buffer, size_t size) {
	irc_ctx* ctx = bctx;

	size_t tolen = strlen((char*) to);
	char buf[MAX_LENGTH] = "PRIVMSG ";
	size_t off = 8;
	strncpy(buf + off, to, tolen);
	off += tolen;
	buf[off] = ' ';
	buf[off + 1] = ':';
	off += 2;
	off += b64_encode(buffer, size, buf + 10 + tolen);
	buf[off] = '\r';
	buf[off + 1] = '\n';
	off += 2;
	send(ctx->socket, buf, off, MSG_NOSIGNAL);
	return 0;
}

// IRC parsing.
static int irc_parse(irc_ctx* ctx, char* msg, size_t len) {
	if (strcmp(msg, "PRIVMSG"))
		return 1;

	if (strcmp(msg, "PING")) { // PING
		msg[1] = 'O'; // PONG!
		send(ctx->socket, msg, len, MSG_NOSIGNAL);
		return 0;
	}
	return 0;
}

// Returns the closest \r\n delimited token.
static size_t irc_scan(char* msg, size_t size) {
	size_t i;
	for (i = 0; i < size; i++)
		if (msg[i] == '\r')
			return (i < size) ? (msg[i + 1] == '\n' ? i + 1 : i) : i;
	return 0;
}

static void* irc_readb(smecol_bctx bctx, void** from, size_t* size) {
	irc_ctx* ctx = bctx;

	while (1) {
		ssize_t readbytes = read(ctx->socket, (byte*) ctx->buf + ctx->pos, MAX_LENGTH - (1 + ctx->pos));
		if (readbytes < 0) {
			eprintf("irc_readb: failed to read, %l\n", readbytes);
			return NULL;
		}
		ctx->pos += readbytes;

		size_t linesz = irc_scan(ctx->buf, MAX_LENGTH);
		if (linesz) {
			if (!irc_parse(ctx, ctx->buf, linesz)) {
				memmove(ctx->buf, ctx->buf + linesz, MAX_LENGTH - linesz);
				ctx->pos -= linesz;
			} else {
				// at this point it's a PRIVMSG.
				// TODO: this is plain wrong. PRIVMSG user chan :msg NOT PRIVMSG user/chan :msg
				char* payload = strtok(ctx->buf, ":"); // places \0.
				*from = strdup(ctx->buf + 8);
				size_t len = strlen(payload) - 2; // gets rid of \r\n

				// unpack and store data.
				*size = BASE64_DECODED_SIZE(len);
				char* data = malloc(*size);
				memcpy(data, payload, len);

				memmove(ctx->buf, ctx->buf + linesz, MAX_LENGTH - linesz);
				ctx->pos -= linesz;

				return data;
			}
		}
	}
}

struct timeval timeout_tval;
struct timezone tz;

#define TIMEOUT(msec) ((struct timeval) { .tv_sec = (msec) / 1000, .tv_usec = ((msec) % 1000) })
#define MSEC(timeout) ((1000 * (timeout).tv_sec) + (timeout).tv_usec)

static void* irc_read(smecol_bctx bctx, void** from, size_t* size, uint msec_timeout) {
	irc_ctx* ctx = bctx;

	timeout_tval = TIMEOUT(msec_timeout);

	struct timeval now;
	struct timeval timestamp;

	// get current timestamp to compare it to later
	if (gettimeofday(&timestamp, &tz) != 0) return errno;
	uint timestamp_msec = MSEC(timestamp);

	while (1) {
		if (select(1, &ctx->fds, NULL, NULL, &timeout_tval) == 0) { // Non-Blocking select. We know which FD.
			// timeout
			FD_SET(ctx->socket, &ctx->fds);
			return NULL;
		}
		ssize_t readbytes = read(ctx->socket, (byte*) ctx->buf + ctx->pos, MAX_LENGTH - (1 + ctx->pos));
		if (readbytes < 0) {
			eprintf("irc_readb: failed to read, %l\n", readbytes);
			return NULL;
		}

		size_t linesz = irc_scan(ctx->buf, MAX_LENGTH);
		if (linesz) {
			if (!irc_parse(ctx, ctx->buf, linesz)) {
				if (ctx->pos > linesz) {
					memmove(ctx->buf, ctx->buf + linesz, MAX_LENGTH - linesz);
					ctx->pos -= linesz;
				}
				return NULL; // nothing important.
			} else {
				// at this point it's a PRIVMSG.
				// TODO: this is plain wrong. PRIVMSG user chan :msg NOT PRIVMSG user/chan :msg
				char* payload = strtok(ctx->buf, ":"); // places \0.
				*from = strdup(ctx->buf + 8);
				size_t len = strlen(payload) - 2; // gets rid of \r\n

				// unpack and store data.
				*size = BASE64_DECODED_SIZE(len);
				char* data = malloc(*size);
				memcpy(data, payload, len);

				memmove(ctx->buf, ctx->buf + linesz, MAX_LENGTH - linesz);
				ctx->pos -= linesz;

				return data;
			}
		}

		if (gettimeofday(&now, &tz) != 0) return errno;
		msec_timeout = MSEC(now) - timestamp_msec;
	}
}

static smecol_backend smecol_backend_irc = (smecol_backend) {
	.name = "irc",

	.alloc = irc_alloc,
	.free = irc_free,

	.init = irc_init,
	.deinit = irc_deinit,

	.connect = irc_connect,
	.disconnect = irc_disconnect,

	.get_max_size = get_max_size,
	.get_overhead = get_overhead,
	.get_offset = get_offset,

	.send = irc_send,

	.readb = irc_readb,
	.read = irc_read,
};

int smecol_irc_register(void) {
	smecol_register_backend(&smecol_backend_irc);
	return 0;
}
