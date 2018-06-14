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
  * Interface to geotiff files providing a reader for 16b data, e.g. ASTER-GDEM2
  * or SRTM tiles.
  */
#ifndef TURTLE_READER_GEOTIFF16_H
#define TURTLE_READER_GEOTIFF16_H

#include <stdint.h>
#include <tiffio.h>

/* Data for reading a geotiff 16b file. */
struct geotiff16_reader {
        uint32_t width, height;
        double scale[3], tiepoint[2][3];
        TIFF * tiff;
};

/* Register the geotiff tags to libtiff. */
void geotiff16_register();

/* Manage a geotiff 16b file reader. */
int geotiff16_open(const char * path, struct geotiff16_reader * reader);
void geotiff16_close(struct geotiff16_reader * reader);
int geotiff16_readinto(struct geotiff16_reader * reader, int16_t * buffer);

#endif
