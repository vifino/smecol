// SMeCoL allocator abstraction.

#ifndef __SMECOL_ALLOCATOR__
#define __SMECOL_ALLOCATOR__

#include "types.h"

// Allocates a chunk of memory for messages. Uncleared. Make sure to free it.
static inline void* smecol_alloc(smecol_ctx* ctx, size_t size) {
	return ctx->backend.alloc(ctx->backend.bctx, size);
}
// Free a blob previously returned by read/readb or allocated by smecol_alloc. It is important.
static inline void smecol_free(smecol_ctx* ctx, void* ptr) {
	ctx->backend.free(ctx->backend.bctx, ptr);
}

void* smecol_alloc_buf(smecol_ctx* ctx, size_t msgsize, size_t* bufsize);

#endif
