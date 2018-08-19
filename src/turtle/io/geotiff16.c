/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for Rendering The eLEvation (TURTLE)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/*
 * I/O's for geotiff files providing a reader for 16b data, e.g. ASTER-GDEM2
 * or SRTM tiles
 */

/* C89 standard library */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* TIFF library */
#include <tiffio.h>
/* TURTLE library */
#include "turtle/io.h"

/* GEOTIFF tags */
#define TIFFTAG_GEOPIXELSCALE 33550
#define TIFFTAG_INTERGRAPH_MATRIX 33920
#define TIFFTAG_GEOTIEPOINTS 33922
#define TIFFTAG_GEOKEYDIRECTORY 34735
#define TIFFTAG_GEODOUBLEPARAMS 34736
#define TIFFTAG_GEOASCIIPARAMS 34737
#define TIFFTAG_GDAL_METADATA 42112
#define TIFFTAG_GDAL_NODATA 42113

static const TIFFFieldInfo field_info[] = { { TIFFTAG_GEOPIXELSCALE, -1, -1,
                                                TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
                                                "GeoPixelScale" },
        { TIFFTAG_INTERGRAPH_MATRIX, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "Intergraph TransformationMatrix" },
        { TIFFTAG_GEOTIEPOINTS, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "GeoTiePoints" },
        { TIFFTAG_GEOKEYDIRECTORY, -1, -1, TIFF_SHORT, FIELD_CUSTOM, 1, 1,
            "GeoKeyDirectory" },
        { TIFFTAG_GEODOUBLEPARAMS, -1, -1, TIFF_DOUBLE, FIELD_CUSTOM, 1, 1,
            "GeoDoubleParams" },
        { TIFFTAG_GEOASCIIPARAMS, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GeoASCIIParams" },
        { TIFFTAG_GDAL_METADATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GDAL_METADATA" },
        { TIFFTAG_GDAL_NODATA, -1, -1, TIFF_ASCII, FIELD_CUSTOM, 1, 0,
            "GDAL_NODATA" } };

/* Extender for GEOTIFF tags */
static TIFFExtendProc parent_extender = NULL;

static void default_directory(TIFF * tiff)
{
        TIFFMergeFieldInfo(
            tiff, field_info, sizeof(field_info) / sizeof(field_info[0]));
        if (parent_extender != NULL) (*parent_extender)(tiff);
}

/* Register the tag extender to libtiff */
void turtle_geotiff16_register_(void)
{
        parent_extender = TIFFSetTagExtender(default_directory);
        TIFFSetErrorHandler(NULL); /* Mute error messages */
}

/* Data for accessing a GEOTIFF file */
struct geotiff16_io {
        /* Base io object */
        struct turtle_io base;

        /* Internal data for the GEOTIFF format */
        TIFF * tiff;
        const char * path;
};

static enum turtle_return geotiff16_open(struct turtle_io * io,
    const char * path, const char * mode, struct turtle_error_context * error_)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;
        if (geotiff16->tiff == NULL) io->close(io);

        if (mode[0] != 'r') {
                /* Write mode is not yet implemented */
                return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                    "invalid write format for file`%s'", path);
        }

        /* Initialise the io object */
        io->meta.nx = io->meta.ny = 0;
        io->meta.x0 = io->meta.y0 = 0.;
        io->meta.dx = io->meta.dy = 0.;
        io->meta.z0 = -32767.;
        io->meta.dz = 1.;
        io->meta.projection.type = PROJECTION_NONE;

        /* Open the TIFF file */
        geotiff16->tiff = TIFFOpen(path, "r");
        if (geotiff16->tiff == NULL) {
                return TURTLE_ERROR_VREGISTER(
                    TURTLE_RETURN_PATH_ERROR, "could not open file `%s'", path);
        }

        /* Initialise the new io and return */
        uint32_t height;
        TIFFGetField(geotiff16->tiff, TIFFTAG_IMAGELENGTH, &height);
        io->meta.ny = height;
        tsize_t size = TIFFScanlineSize(geotiff16->tiff);
        io->meta.nx = size / sizeof(int16);
        int count = 0;
        double * data = NULL;
        TIFFGetField(geotiff16->tiff, TIFFTAG_GEOPIXELSCALE, &count, &data);
        if (count == 3) {
                io->meta.dx = data[0];
                io->meta.dy = data[1];
        }
        TIFFGetField(geotiff16->tiff, TIFFTAG_GEOTIEPOINTS, &count, &data);
        if (count == 6) {
                io->meta.x0 = data[3];
                io->meta.y0 = data[4] + (1. - io->meta.ny) * io->meta.dy;
        }

        geotiff16->path = path;
        return TURTLE_RETURN_SUCCESS;
}

static void geotiff16_close(struct turtle_io * io)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;
        if (geotiff16->tiff != NULL) {
                TIFFClose(geotiff16->tiff);
                geotiff16->tiff = NULL;
                geotiff16->path = NULL;
        }
}

static double get_z(const struct turtle_map * map, int ix, int iy)
{
        return (int16_t)map->data[iy * map->meta.nx + ix];
}

static void set_z(struct turtle_map * map, int ix, int iy, double z)
{
        map->data[iy * map->meta.nx + ix] = (int16_t)z;
}

static enum turtle_return geotiff16_read(struct turtle_io * io,
    struct turtle_map * map, struct turtle_error_context * error_)
{
        struct geotiff16_io * geotiff16 = (struct geotiff16_io *)io;

        /* Unpack the data */
        uint16_t * buffer = map->data;
        buffer += io->meta.nx * (io->meta.ny - 1);
        int i;
        for (i = 0; i < io->meta.ny; i++, buffer -= io->meta.nx) {
                if (TIFFReadScanline(geotiff16->tiff, buffer, i, 0) != 1) {
                        return TURTLE_ERROR_VREGISTER(TURTLE_RETURN_BAD_FORMAT,
                            "a libtiff error occured when reading file `%s'",
                            geotiff16->path);
                }
        }

        return TURTLE_RETURN_SUCCESS;
}

enum turtle_return turtle_io_geotiff16_create_(
    struct turtle_io ** io_p, struct turtle_error_context * error_)
{
        /* Allocate the geotiff16 io manager */
        struct geotiff16_io * geotiff16 = malloc(sizeof(*geotiff16));
        if (geotiff16 == NULL) {
                return TURTLE_ERROR_REGISTER(TURTLE_RETURN_MEMORY_ERROR,
                    "could not allocate memory for geotiff16 format");
        }
        *io_p = &geotiff16->base;

        /* Initialise the io object */
        memset(geotiff16, 0x0, sizeof(*geotiff16));
        geotiff16->tiff = NULL;
        geotiff16->path = NULL;
        geotiff16->base.meta.projection.type = PROJECTION_NONE;

        geotiff16->base.open = &geotiff16_open;
        geotiff16->base.close = &geotiff16_close;
        geotiff16->base.read = &geotiff16_read;
        geotiff16->base.write = NULL;

        geotiff16->base.meta.get_z = &get_z;
        geotiff16->base.meta.set_z = &set_z;

        return TURTLE_RETURN_SUCCESS;
}
