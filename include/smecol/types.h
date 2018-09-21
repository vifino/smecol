// Type aliases and definitions.

#ifndef __SMECOL_TYPES__
#define __SMECOL_TYPES__

#include <stddef.h>

#define MAX_BACKENDS 16
#define MAX_FILTERS 32
#define MAX_CTX_FILTERS 8

typedef unsigned int uint;
typedef unsigned char byte;
typedef void* smecol_fctx;
typedef void* smecol_bctx;

// Filters and backends.
typedef struct {
	char* name;
	smecol_fctx fctx;

	int (*apply)(smecol_fctx fctx, void* buffer, size_t* size);
	int (*unapply)(smecol_fctx fctx, void* buffer, size_t* size);

	smecol_fctx (*init)(void* arg);
	void (*deinit)(smecol_fctx fctx);

	int (*get_overhead)(smecol_fctx fctx, size_t size);
	size_t offset;
} smecol_filter;

typedef struct {
	char* name;
	smecol_bctx bctx;
	size_t max_size;
	size_t offset;
	size_t overhead;

	void* (*alloc)(smecol_bctx bctx, size_t size);
	void (*free)(smecol_bctx bctx, void*);

	smecol_bctx (*init)(void* arg);
	void (*deinit)(smecol_bctx bctx);

	int (*connect)(smecol_bctx bctx);
	void (*disconnect)(smecol_bctx bctx);

	int (*send)(smecol_bctx bctx, void* to, void* buffer, size_t size);

	void* (*readb)(smecol_bctx bctx, void** from, size_t* size);
	void* (*read)(smecol_bctx bctx, void** from, size_t* size, uint msec_timeout);
	size_t (*available)(smecol_bctx bctx);
} smecol_backend;

// SMeCoL context
typedef struct {
	char* backend_name;
	smecol_backend backend;
	smecol_filter filters[MAX_CTX_FILTERS];
	uint filter_count;
	char* fargs[MAX_CTX_FILTERS];

	size_t offset;
} smecol_ctx;

#endif
