// SMeCoL interfaces
// All of them.

#include "types.h"
#include "modules.h"
#include "alloc.h"
#include "chain.h"

// Bootstrap. CALL THIS FIRST. Without, things probably won't work.
int smecol_bootstrap(void);

// General Use
smecol_ctx* smecol_new(void); // Create a new context. Uses calloc, free it yourself.
int smecol_init(smecol_ctx* ctx); // Init filters and context itself.
void smecol_deinit(smecol_ctx* ctx); // Deinit context and filters.

void smecol_addfilter(smecol_ctx* ctx, const char* filter, void* arg); // Add a filter.

int smecol_connect(smecol_ctx* ctx, const char* backend, void* arg);
void smecol_disconnect(smecol_ctx* ctx);

int smecol_send(smecol_ctx* ctx, void* buf, size_t len); // Send a blob.
void* smecol_readb(smecol_ctx* ctx, size_t* len); // Receive a blob if available, otherwise wait and try after.
void* smecol_read(smecol_ctx* ctx, size_t* len, uint msec_timeout); // The above, but with a timeout. Set to 0 to only read if available.
size_t smecol_available(smecol_ctx* ctx); // Check if messages are available. If the output knows, it might be the available messages.
