// Module management, simple-ish.

#include "../include/smecol/types.h"
#include "../include/smecol/modules.h"
#include <string.h>

static smecol_backend backends[MAX_BACKENDS];
static uint backend_count = 0;

static smecol_filter filters[MAX_FILTERS];
static uint filter_count = 0;

// Backend/Filter registration.
uint smecol_backend_count(void) {
	return backend_count;
}
uint smecol_filter_count(void) {
	return filter_count;
}

smecol_backend* smecol_backend_get(uint number) {
	if (number < MAX_BACKENDS)
		return &backends[number];
	return NULL;
}
smecol_filter* smecol_filter_get(uint number) {
	if (number < MAX_FILTERS)
		return &filters[number];
	return NULL;
}

void smecol_backend_set(uint number, const smecol_backend* backend) {
	if (number < MAX_BACKENDS)
		backends[number] = *backend;
}
void smecol_filter_set(uint number, const smecol_filter* filter) {
	if (number < MAX_FILTERS)
		filters[number] = *filter;
}

int smecol_register_backend(const smecol_backend* backend) {
	if (backend_count > MAX_BACKENDS)
		return -1;

	smecol_backend_set(backend_count - 1, backend);
	return (backend_count++) - 1;
}
int smecol_register_filter(const smecol_filter* filter) {
	if (backend_count > MAX_FILTERS)
		return -1;

	smecol_filter_set(filter_count - 1, filter);
	return (filter_count++) - 1;
}

// Finding
smecol_backend* smecol_backend_lookup(const char* name) {
	for (uint count = 0; count < backend_count; count++) {
		if (strcmp(name, backends[count].name) == 0)
			return &backends[count];
	}
	return NULL;
}
smecol_filter* smecol_filter_lookup(const char* name) {
	for (uint count = 0; count < backend_count; count++) {
		if (strcmp(name, filters[count].name) == 0)
			return &filters[count];
	}
	return NULL;
}
