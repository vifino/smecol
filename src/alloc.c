// SMeCoL allocator abstraction.
// Instead of stupidly always relying on malloc/free,
// we implement a wrapper that calls the related functions on the backend.

#include "../include/smecol/types.h"
#include "../include/smecol/alloc.h"
#include "../include/smecol/chain.h"

// alloc/free is defined in the header.

void* smecol_alloc_buf(smecol_ctx* ctx, size_t msgsize, size_t* bufsize) {
	size_t bsize = msgsize + smecol_chain_overhead(ctx, msgsize);
	byte* buf = ctx->backend.alloc(ctx->backend.bctx, bsize);
	*bufsize = bsize;
	return buf + ctx->offset;
}
