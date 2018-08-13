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
 * Turtle client for multithreaded access to a elevation data handled by a
 * turtle_stack.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"
#include "stack.h"
#include "error.h"
#include "turtle.h"

/* Management routine(s). */
static enum turtle_return client_release(
    struct turtle_client * client, int lock);

/* Create a new stack client. */
enum turtle_return turtle_client_create(
    struct turtle_stack * stack, struct turtle_client ** client)
{
        /* Check that one has a valid stack. */
        *client = NULL;
        if ((stack == NULL) || (stack->lock == NULL))
                TURTLE_ERROR_MESSAGE(TURTLE_RETURN_BAD_ADDRESS,
                    turtle_client_create, "invalid stack or missing lock");

        /* Allocate the new client and initialise it. */
        *client = malloc(sizeof(**client));
        if (*client == NULL) return TURTLE_ERROR_MEMORY(turtle_client_create);
        (*client)->stack = stack;
        (*client)->tile = NULL;
        (*client)->index_la = INT_MIN;
        (*client)->index_lo = INT_MIN;

        return TURTLE_RETURN_SUCCESS;
}

/* Destroy a client. */
enum turtle_return turtle_client_destroy(struct turtle_client ** client)
{
        /* Check if the client has already been released. */
        if ((client == NULL) || (*client == NULL)) return TURTLE_RETURN_SUCCESS;

        /* Release any active tile. */
        enum turtle_return rc = client_release(*client, 1);
        if (rc == TURTLE_RETURN_LOCK_ERROR)
                return TURTLE_ERROR_LOCK(turtle_client_destroy);
        else if (rc == TURTLE_RETURN_UNLOCK_ERROR)
                return TURTLE_ERROR_UNLOCK(turtle_client_destroy);

        /* Free the memory and return. */
        free(*client);
        *client = NULL;

        return TURTLE_RETURN_SUCCESS;
}

/* Clear the client's memory. */
enum turtle_return turtle_client_clear(struct turtle_client * client)
{
        enum turtle_return rc = client_release(client, 1);
        if (rc == TURTLE_RETURN_LOCK_ERROR)
                return TURTLE_ERROR_LOCK(turtle_client_clear);
        else if (rc == TURTLE_RETURN_UNLOCK_ERROR)
                return TURTLE_ERROR_UNLOCK(turtle_client_clear);
        else
                return TURTLE_RETURN_SUCCESS;
}

/* Supervised access to the elevation data. */
enum turtle_return turtle_client_elevation(struct turtle_client * client,
    double latitude, double longitude, double * elevation, int * inside)
{
        if (inside != NULL) *inside = 0;

        /* Get the proper tile. */
        struct tile * current = client->tile;
        double hx = 0., hy = 0.; /* Patch a non relevant warning. */
        int ix, iy;

        if (current != NULL) {
                /* First let's check the current tile. */
                hx = (longitude - current->x0) / current->dx;
                hy = (latitude - current->y0) / current->dy;

                if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
                    (hy <= current->ny))
                        goto interpolate;
        } else if (((int)latitude == client->index_la) &&
            ((int)longitude == client->index_lo)) {
                if (inside != NULL) {
                        return TURTLE_RETURN_SUCCESS;
                } else {
                        return TURTLE_ERROR_MISSING_DATA(
                            turtle_client_elevation, client->stack);
                }
        }

        /* Lock the stack. */
        struct turtle_stack * stack = client->stack;
        if ((stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_ERROR_LOCK(turtle_client_elevation);
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;

        /* The requested coordinates are not in the current tile. Let's check
         * the full stack.
         */
        current = stack->head;
        while (current != NULL) {
                if (current == client->tile) {
                        current = current->prev;
                        continue;
                }
                hx = (longitude - current->x0) / current->dx;
                hy = (latitude - current->y0) / current->dy;
                if ((hx >= 0.) && (hx <= current->nx) && (hy >= 0.) &&
                    (hy <= current->ny)) {
                        tile_touch(stack, current);
                        goto update;
                }
                current = current->prev;
        }

        /* No valid tile was found. Let's try to load it. */
        if ((rc = tile_load(stack, latitude, longitude)) !=
            TURTLE_RETURN_SUCCESS) {
                /* The requested tile is not available. Let's record this.  */
                client_release(client, 0);
                client->index_la = (int)latitude;
                client->index_lo = (int)longitude;
                goto unlock;
        }
        current = stack->head;
        hx = (longitude - current->x0) / current->dx;
        hy = (latitude - current->y0) / current->dy;

/* Update the client. */
update:
        if ((rc = client_release(client, 0)) != TURTLE_RETURN_SUCCESS)
                goto unlock;
        current->clients++;
        client->tile = current;
        client->index_la = INT_MIN;
        client->index_lo = INT_MIN;

/* Unlock the stack. */
unlock:
        if ((stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_ERROR_UNLOCK(turtle_client_elevation);
        if (rc != TURTLE_RETURN_SUCCESS) {
                *elevation = 0.;
                if (rc == TURTLE_RETURN_PATH_ERROR) {
                        if (inside != NULL) {
                                return TURTLE_RETURN_SUCCESS;
                        } else {
                                return TURTLE_ERROR_MISSING_DATA(
                                    turtle_client_elevation, client->stack);
                        }
                } else if (rc == TURTLE_RETURN_LOCK_ERROR) {
                        return TURTLE_ERROR_LOCK(turtle_client_elevation);
                } else if (rc == TURTLE_RETURN_UNLOCK_ERROR) {
                        return TURTLE_ERROR_UNLOCK(turtle_client_elevation);
                } else {
                        return TURTLE_ERROR_UNEXPECTED(
                            rc, turtle_client_elevation);
                }
        }

/* Interpolate the elevation. */
interpolate:
        ix = (int)hx;
        iy = (int)hy;
        hx -= ix;
        hy -= iy;
        const int nx = client->tile->nx;
        const int ny = client->tile->ny;
        if (ix < 0) ix = 0;
        if (iy < 0) iy = 0;
        int ix1 = (ix >= nx - 1) ? nx - 1 : ix + 1;
        int iy1 = (iy >= ny - 1) ? ny - 1 : iy + 1;
        struct tile * tile = client->tile;
        tile_cb * zm = tile->z;
        *elevation = zm(tile, ix, iy) * (1. - hx) * (1. - hy) +
            zm(tile, ix, iy1) * (1. - hx) * hy +
            zm(tile, ix1, iy) * hx * (1. - hy) + zm(tile, ix1, iy1) * hx * hy;

        if (inside != NULL) *inside = 1;
        return TURTLE_RETURN_SUCCESS;
}

/* Release any active tile. */
static enum turtle_return client_release(
    struct turtle_client * client, int lock)
{
        if (client->tile == NULL) return TURTLE_RETURN_SUCCESS;

        /* Lock the stack. */
        struct turtle_stack * stack = client->stack;
        if (lock && (stack->lock != NULL) && (stack->lock() != 0))
                return TURTLE_RETURN_LOCK_ERROR;
        enum turtle_return rc = TURTLE_RETURN_SUCCESS;

        /* Update the reference count. */
        struct tile * tile = client->tile;
        client->tile = NULL;
        tile->clients--;
        if (tile->clients < 0) {
                tile->clients = 0;
                rc = TURTLE_RETURN_LIBRARY_ERROR;
                goto unlock;
        }

        /* Remove the tile if it is unused and if there is a stack overflow. */
        if ((tile->clients == 0) && (stack->size > stack->max_size))
                tile_destroy(stack, tile);

/* Unlock and return. */
unlock:
        if (lock && (stack->unlock != NULL) && (stack->unlock() != 0))
                return TURTLE_RETURN_UNLOCK_ERROR;
        return rc;
}
