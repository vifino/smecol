// Filter-Backend Chain and associated calculations.

#ifndef __SMECOL_CHAIN__
#define __SMECOL_CHAIN__

#include "types.h"

// Calculate the offset the message is located in in the big buffer.
size_t smecol_chain_offset(smecol_ctx* ctx);
size_t smecol_chain_overhead(smecol_ctx* ctx, size_t msgsize);

// Applies the filters to a message so that SMeCoL can send them.
// Returns 0 if successful.
int smecol_chain_apply(smecol_ctx* ctx, void* buffer, size_t msgsize);
int smecol_chain_unapply(smecol_ctx* ctx, void* buffer, size_t msgsize);

#endif
