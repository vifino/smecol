// Modules, rather internal.

#ifndef __SMECOL_MODULES__
#define __SMECOL_MODULES__
#include "types.h"

// Gets the count of the loaded backends/filters.
unsigned int smecol_backend_count(void);
unsigned int smecol_filter_count(void);

// Get the backend/filter by number.
smecol_backend* smecol_backend_get(unsigned int number);
smecol_filter* smecol_filter_get(unsigned int number);

// Set the backend/filter by number.
void smecol_backend_set(unsigned int number, const smecol_backend* backend);
void smecol_filter_set(unsigned int number, const smecol_filter* filter);

// These register new backends/filters, copying them into the module struct.
// Returns the slot number.
int smecol_register_backend(const smecol_backend* backend);
int smecol_register_filter(const smecol_filter* filter);

// Look up a backend/filter slot number by name.
// Returns -1 on failure.
smecol_backend* smecol_backend_lookup(const char* name);
smecol_filter* smecol_filter_lookup(const char* name);

#endif
