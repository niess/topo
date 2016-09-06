/*
 * Topographic Utilities for Rendering The eLEvation (TURTLE).
 */

#ifndef TURTLE_DATUM_H
#define TURTLE_DATUM_H

#include <stdint.h>

enum datum_format {
	DATUM_FORMAT_NONE = -1,
	DATUM_FORMAT_ASTER_GDEM2,
	N_DATUM_FORMATS
};

/* Container for a data tile. */
struct datum_tile {
	/* Meta data */
	struct datum_tile * prev, * next;
	int clients;

	/* Map data. */
	int nx, ny;
	double x0, y0;
	double dx, dy;
	int16_t z[];
};

/* Container for a geodetic datum. */
struct turtle_datum {
	/* The stack of loaded tiles. */
	struct datum_tile * stack;
	int stack_size, max_size;

	/* Data format. */
	enum datum_format format;

	/* Callbacks for lock handling. */
	turtle_datum_cb * lock;
	turtle_datum_cb * unlock;

	/* Buffer for I/Os. */
	char * buffer;
	char path[]; /* Placeholder for the path string. */
};

/* Tile utility routines. */
void datum_tile_touch(struct turtle_datum * datum, struct datum_tile * tile);
void datum_tile_destroy(struct turtle_datum * datum, struct datum_tile * tile);
enum turtle_return datum_tile_load(struct turtle_datum * datum, int latitude,
	int longitude);

#endif
