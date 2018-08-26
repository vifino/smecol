// Filter-Backend Chain and associated calculations for buffer size.

#include "../include/smecol/types.h"

size_t smecol_chain_offset(smecol_ctx* ctx) {
	size_t offset = 0;
	for (uint i = 0; i < ctx->filter_count; i++) {
		offset += ctx->filters[i].offset;
	}
	return offset;
}

size_t smecol_chain_overhead(smecol_ctx* ctx, size_t msgsize) {
	size_t overhead = 0;
	for (uint i = 0; i < ctx->filter_count; i++) {
		overhead += ctx->filters[i].get_overhead(&ctx->filters[i].fctx, msgsize + overhead);
	}
	return overhead;
}

int smecol_chain_apply(smecol_ctx* ctx, void* buffer, size_t msgsize) {
	// Buffer is the start pointer to the block of size size.
	// Since we're applying, the message is the only thing there at ctx->offset.
	// Since we're running filters in reverse for that stacking-style interface,
	// we have to increase the size and decrease the offset, as the offset
	// decreases to "make room" for the following filters offset and
	// the size naturally increases as the message gets closer to end of the
	// filter chain and the starting pointer is archieved again, which usually
	// should be the size of the allocated chunk.

	// Calculate overhead in a way we can undo it later.
	size_t overheads[MAX_CTX_FILTERS + 1];
	overheads[MAX_CTX_FILTERS] = 0;
	size_t total_overhead = 0;
	for (uint i = ctx->filter_count; i != 0; i--) {
		uint overhead = ctx->filters[i].get_overhead(&ctx->filters[i].fctx, msgsize + total_overhead);
		total_overhead += overhead;
		overheads[i] = total_overhead;
	}

	size_t offset = total_overhead;
	size_t sz = msgsize;
	for (uint i = ctx->filter_count; i != 0; i--) {
		offset -= (i == ctx->filter_count) ? 0 : ctx->filters[i + 1].offset - overheads[i + 1] + ctx->filters[i].offset;
		sz = msgsize + overheads[i + 1];
		int val = ctx->filters[i].apply(&ctx->filters[i].fctx, (byte*) buffer + offset, &sz);
		if (val != 0) return val;
#ifdef DEBUG
		if (sz != (size + overheads[i + 1]))
			debug("smecol_chain: Chain filter no %i calculated size wrong!", i);
#endif
	}
	return 0;
}

int smecol_chain_unapply(smecol_ctx* ctx, void* buffer, size_t size) {
	// Calculate overhead in a way we can undo it later.
	size_t overheads[MAX_CTX_FILTERS + 1];
	overheads[MAX_CTX_FILTERS] = 0;
	size_t total_overhead = 0;
	for (uint i = ctx->filter_count; i != 0; i--) {
		uint overhead = ctx->filters[i].get_overhead(&ctx->filters[i].fctx, size + total_overhead);
		total_overhead += overhead;
		overheads[i] = total_overhead;
	}

	size_t offset = total_overhead;
	size_t sz = size;
	for (uint i = 0; i < ctx->filter_count; i++) {
		offset += (i == 0) ? 0 : ctx->filters[i - 1].offset - overheads[i - 1] + ctx->filters[i].offset;
		sz = (i == 0) ? size : size - overheads[i - 1];
		int val = ctx->filters[i].unapply(&ctx->filters[i].fctx, (byte*) buffer + offset, &sz);
		if (val != 0) return val;
#ifdef DEBUG
		if (sz != (size + overheads[i - 1]))
			debug("smecol_chain: Chain filter no %i calculated size wrong!", i);
#endif
	}
	return 0;
}
