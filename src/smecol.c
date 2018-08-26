// Bootstrap code/Glue for SMeCoL.

#include "../include/smecol/smecol.h"
#include <stdlib.h>

int smecol_bootstrap(void) {
	// As of right now, we have no need to do anything at all.
	return 0;
}

smecol_ctx* smecol_new(void) {
	return calloc(1, sizeof(smecol_ctx));
}

int smecol_init(smecol_ctx* ctx) {
	// Initialize filters.
	for (uint i = 0; i < ctx->filter_count; i++)
		ctx->filters[i].fctx = ctx->filters[i].init(ctx->fargs[i]);

	// Calculate offset.
	ctx->offset = smecol_chain_offset(ctx);

	return 0;
}
void smecol_deinit(smecol_ctx* ctx) {
	// TODO: Deinitialize backend.

	// Deinitialize filters.
	for (uint i = 0; i < ctx->filter_count; i++)
		ctx->filters[i].deinit(ctx->filters[i].fctx);
}
