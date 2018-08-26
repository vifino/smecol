// Context management.

#include "../include/smecol/types.h"
#include "../include/smecol/modules.h"
#include "../include/smecol/alloc.h"
#include "../include/smecol/chain.h"

#include <string.h>

int smecol_addfilter(smecol_ctx* ctx, const char* filter, void* arg) {
	if (ctx->filter_count < MAX_CTX_FILTERS) {
		smecol_filter* f = smecol_filter_lookup(filter);
		if (f == NULL) return 1;
		ctx->filters[ctx->filter_count] = *f;
		ctx->fargs[ctx->filter_count++] = arg;
	}
	return 0;
}

int smecol_connect(smecol_ctx* ctx, const char* backend, void* arg) {
	smecol_backend* b = smecol_backend_lookup(backend);
	if (backend == NULL) return 1;

	ctx->backend = *b;
	ctx->backend.bctx = ctx->backend.init(arg);
	if (ctx->backend.bctx == NULL) return 2;

	return ctx->backend.connect(ctx->backend.bctx);
}

void smecol_disconnect(smecol_ctx* ctx) {
	ctx->backend.disconnect(ctx->backend.bctx);
}

size_t smecol_available(smecol_ctx* ctx) {
	return ctx->backend.available(ctx->backend.bctx);
}

// Sending and receiving
int smecol_send(smecol_ctx* ctx, void* buf, size_t len) {
	// Get chain overhead to allocate a properly sized buffer.
	size_t blen = len + smecol_chain_overhead(ctx, len);
	void* bbuf = smecol_alloc(ctx, blen);

	// Unfortunately, we have to copy here to avoid
	// using a ptr that might not be valid for long.
	// The buffer will also be not big enough, perhaps.
	// With enough logic one could differentiate between
	// different buffers, but that would make code look
	// just plain awful. So copying it is.
	memcpy((byte*) bbuf + ctx->offset, buf, len);

	smecol_chain_apply(ctx, bbuf, len);
	int ret = ctx->backend.send(ctx->backend.bctx, bbuf, blen);

	smecol_free(ctx, bbuf);
	return ret;
}

void* smecol_readb(smecol_ctx* ctx, size_t* size) {
	// readb/read returns an allocated buffer.
	// The user needs to smecol_free it later.
	void* msg = ctx->backend.readb(ctx->backend.bctx, size);
	smecol_chain_unapply(ctx, msg, *size);
	return msg;
}

void* smecol_read(smecol_ctx* ctx, size_t* size, uint msec_timeout) {
	// readb/read returns an allocated buffer.
	// The user needs to smecol_free it later.
	void* msg = ctx->backend.read(ctx->backend.bctx, size, msec_timeout);
	smecol_chain_unapply(ctx, msg, *size);
	return msg;
}
