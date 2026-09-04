/*
 * Copyright (c) 2019-2026 Airbus Commercial Aircraft
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include <openssl/rand.h>

#include "cr_randomstuff.h"
#include "messages.h"

//----------------------------------------------------------------------
//exists
//in: element Item that is searched for
//in: arr Array Container of items being searched in
//in: size Number of items in array

static gboolean cr_exists(gsize element, const gsize arr[], gint size)
{
  for (gint i = 0; i < size; ++i)
    {
      if (arr[i] == element)
        {
          return TRUE;
        }
    }
  return FALSE;
}



//----------------------------------------------------------------------
// cr_distincRandomEz
// Provides an array of pseudo random integer numbers
// Memory is allocated here and the caller is responsible to free it.
// Note: There might be better random number generators, but here, no
// additional dependencies e.g. to gsl library is prefered.
// in range:  Range of random numbers is in closed interval [0, range]
//            range < RAND_MAX
// in k: Count of random numbers. Because each number exists only once
//       in array, k <= (range + 1)
// in seed: Initialization seed of random generator.
//
// return Pointer to array of k numbers of type gsize or NULL in
//        case of ERROR

gsize *cr_distinctRandomEz(gsize range, gint k, gint seed)
{
  (void) seed;
  if ((gsize)RAND_MAX <= range)
    {
      msg_warning("Random range is out of bounds",
                  evt_tag_printf("range", "%zu", range),
                  evt_tag_printf("max_range", "%d", RAND_MAX));
      return NULL;
    }

  if ((k < 0) || ((gsize)k > (range + 1U)))
    {
      msg_warning("Invalid random selection parameters",
                  evt_tag_printf("k", "%d", k),
                  evt_tag_printf("range", "%zu", range));
      return NULL;
    }

  /* allocate array for k numbers */
  gsize *k_random = (gsize *) g_malloc0(((gsize)k) * sizeof(gsize));
  if (k_random == NULL)
    {
      msg_error("Failed to allocate memory for k_random",
                evt_tag_printf("count", "%d", k),
                evt_tag_printf("element_size", "%zu", sizeof(gsize)));
      return NULL;
    }
  /* mark entries as invalid */
  memset(k_random, -1, ((gsize)k) * sizeof(gsize));

  gint i = 0;

  /* Avoid modulo bias: draw random values in [0, limit-1] where limit is
   * a multiple of upperBound, then reduce modulo upperBound. */
  const gsize upperBound = range + 1U;
  const gsize limit = G_MAXSIZE - (G_MAXSIZE % upperBound);

  while (i < k)
    {
      gsize r;

      do
        {
          if (RAND_bytes((unsigned char *)&r, sizeof(r)) != 1)
            {
              msg_error("RAND_bytes failed",
                        evt_tag_printf("random_size", "%zu", sizeof(r)));
              g_free(k_random);
              return NULL;
            }
        }
      while (r >= limit);

      r = r % upperBound;

      if (cr_exists(r, k_random, k))
        {
          continue;
        }

      k_random[i] = r;
      i++;
    }

  return k_random;
}
