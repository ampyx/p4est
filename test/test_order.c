/*
  This file is part of p4est.
  p4est is a C library to manage a parallel collection of quadtrees and/or
  octrees.

  Copyright (C) 2007 Carsten Burstedde, Lucas Wilcox.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <p4est_base.h>
#include <p4est_algorithms.h>
#include <p4est_mesh.h>

#include <math.h>

typedef struct
{
  int32_t             a;
  int64_t             sum;
}
user_data_t;

static int          refine_level = 6;

static void
init_fn (p4est_t * p4est, int32_t which_tree, p4est_quadrant_t * quadrant)
{
  user_data_t        *data = quadrant->user_data;

  data->a = which_tree;
  data->sum = quadrant->x + quadrant->y + quadrant->level;
}

static int
refine_fn (p4est_t * p4est, int32_t which_tree, p4est_quadrant_t * quadrant)
{
  if (quadrant->level >= (refine_level - (which_tree % 3))) {
    return 0;
  }
  if (quadrant->level == 1 && p4est_quadrant_child_id (quadrant) == 3) {
    return 1;
  }
  if (quadrant->x == P4EST_LAST_OFFSET (2) &&
      quadrant->y == P4EST_LAST_OFFSET (2)) {
    return 1;
  }
  if (quadrant->x >= P4EST_QUADRANT_LEN (2)) {
    return 0;
  }

  return 1;
}

static int
weight_one (p4est_t * p4est, int32_t which_tree, p4est_quadrant_t * quadrant)
{
  return 1;
}

typedef struct p4est_vert
{
  double              x, y, z;
}
p4est_vert_t;

static int
p4est_vert_compare (const void *a, const void *b)
{
  const p4est_vert_t *v1 = a;
  const p4est_vert_t *v2 = b;
  const double        eps = 1e-15;
  double              xdiff, ydiff, zdiff;
  int                 retval = 0;

  xdiff = fabs (v1->x - v2->x);
  if (xdiff < eps) {
    ydiff = fabs (v1->y - v2->y);
    if (ydiff < eps) {
      zdiff = fabs (v1->z - v2->z);
      if (zdiff < eps) {
        retval = 0;
      }
      else {
        retval = (v1->z < v2->z) ? -1 : 1;
      }
    }
    else {
      retval = (v1->y < v2->y) ? -1 : 1;
    }
  }
  else {
    retval = (v1->x < v2->x) ? -1 : 1;
  }

  return retval;
}

static void
p4est_check_local_order (p4est_t * p4est, p4est_connectivity_t * connectivity)
{
  int                 i, j;
  int32_t             num_uniq_local_vertices;
  int32_t            *quadrant_to_local_vertex;
  p4est_quadrant_t   *quad;
  p4est_tree_t       *tree;
  int32_t            *tree_to_vertex;
  double             *vertices;
  int32_t             first_local_tree;
  int32_t             last_local_tree;
  int32_t             inth;
  double              h, eta1, eta2;
  double              v0x, v0y, v0z, v1x, v1y, v1z, v2x, v2y, v2z, v3x, v3y,
    v3z;
  double              w0x, w0y, w0z, w1x, w1y, w1z, w2x, w2y, w2z, w3x, w3y,
    w3z;
  int32_t             v0, v1, v2, v3;
  int32_t             lv0, lv1, lv2, lv3;
  double              intsize = 1.0 / P4EST_ROOT_LEN;
  p4est_array_t      *trees;
  p4est_array_t      *quadrants;
  int                 num_quads, quad_count;
  int                 identify_periodic;
  p4est_vert_t       *vert_locations;

  quadrant_to_local_vertex = P4EST_ALLOC (int32_t,
                                          4 * p4est->local_num_quadrants);
  P4EST_CHECK_ALLOC (quadrant_to_local_vertex);

  identify_periodic = 1;
  p4est_order_local_vertices (p4est, identify_periodic,
                              &num_uniq_local_vertices,
                              quadrant_to_local_vertex);

  vert_locations = P4EST_ALLOC (p4est_vert_t, num_uniq_local_vertices);
  P4EST_CHECK_ALLOC (vert_locations);

  tree_to_vertex = connectivity->tree_to_vertex;
  vertices = connectivity->vertices;
  first_local_tree = p4est->first_local_tree;
  last_local_tree = p4est->last_local_tree;
  trees = p4est->trees;

  for (j = first_local_tree, quad_count = 0; j <= last_local_tree; ++j) {
    tree = p4est_array_index (trees, j);
    /*
     * Note that we switch from right-hand-rule order for tree_to_vertex
     * to pixel order for v
     */
    v0 = tree_to_vertex[j * 4 + 0];
    v1 = tree_to_vertex[j * 4 + 1];
    v2 = tree_to_vertex[j * 4 + 3];
    v3 = tree_to_vertex[j * 4 + 2];

    v0x = vertices[v0 * 3 + 0];
    v0y = vertices[v0 * 3 + 1];
    v0z = vertices[v0 * 3 + 2];
    v1x = vertices[v1 * 3 + 0];
    v1y = vertices[v1 * 3 + 1];
    v1z = vertices[v1 * 3 + 2];
    v2x = vertices[v2 * 3 + 0];
    v2y = vertices[v2 * 3 + 1];
    v2z = vertices[v2 * 3 + 2];
    v3x = vertices[v3 * 3 + 0];
    v3y = vertices[v3 * 3 + 1];
    v3z = vertices[v3 * 3 + 2];

    quadrants = &tree->quadrants;
    num_quads = quadrants->elem_count;

    /* loop over the elements in the tree */
    for (i = 0; i < num_quads; ++i, ++quad_count) {
      quad = p4est_array_index (quadrants, i);
      inth = P4EST_QUADRANT_LEN (quad->level);
      h = intsize * inth;
      eta1 = quad->x * intsize;
      eta2 = quad->y * intsize;

      w0x = v0x * (1.0 - eta1) * (1.0 - eta2)
        + v1x * (eta1) * (1.0 - eta2)
        + v2x * (1.0 - eta1) * (eta2)
        + v3x * (eta1) * (eta2);

      w0y = v0y * (1.0 - eta1) * (1.0 - eta2)
        + v1y * (eta1) * (1.0 - eta2)
        + v2y * (1.0 - eta1) * (eta2)
        + v3y * (eta1) * (eta2);

      w0z = v0z * (1.0 - eta1) * (1.0 - eta2)
        + v1z * (eta1) * (1.0 - eta2)
        + v2z * (1.0 - eta1) * (eta2)
        + v3z * (eta1) * (eta2);

      w1x = v0x * (1.0 - eta1 - h) * (1.0 - eta2)
        + v1x * (eta1 + h) * (1.0 - eta2)
        + v2x * (1.0 - eta1 - h) * (eta2)
        + v3x * (eta1 + h) * (eta2);

      w1y = v0y * (1.0 - eta1 - h) * (1.0 - eta2)
        + v1y * (eta1 + h) * (1.0 - eta2)
        + v2y * (1.0 - eta1 - h) * (eta2)
        + v3y * (eta1 + h) * (eta2);

      w1z = v0z * (1.0 - eta1 - h) * (1.0 - eta2)
        + v1z * (eta1 + h) * (1.0 - eta2)
        + v2z * (1.0 - eta1 - h) * (eta2)
        + v3z * (eta1 + h) * (eta2);

      w2x = v0x * (1.0 - eta1) * (1.0 - eta2 - h)
        + v1x * (eta1) * (1.0 - eta2 - h)
        + v2x * (1.0 - eta1) * (eta2 + h)
        + v3x * (eta1) * (eta2 + h);

      w2y = v0y * (1.0 - eta1) * (1.0 - eta2 - h)
        + v1y * (eta1) * (1.0 - eta2 - h)
        + v2y * (1.0 - eta1) * (eta2 + h)
        + v3y * (eta1) * (eta2 + h);

      w2z = v0z * (1.0 - eta1) * (1.0 - eta2 - h)
        + v1z * (eta1) * (1.0 - eta2 - h)
        + v2z * (1.0 - eta1) * (eta2 + h)
        + v3z * (eta1) * (eta2 + h);

      w3x = v0x * (1.0 - eta1 - h) * (1.0 - eta2 - h)
        + v1x * (eta1 + h) * (1.0 - eta2 - h)
        + v2x * (1.0 - eta1 - h) * (eta2 + h)
        + v3x * (eta1 + h) * (eta2 + h);

      w3y = v0y * (1.0 - eta1 - h) * (1.0 - eta2 - h)
        + v1y * (eta1 + h) * (1.0 - eta2 - h)
        + v2y * (1.0 - eta1 - h) * (eta2 + h)
        + v3y * (eta1 + h) * (eta2 + h);

      w3z = v0z * (1.0 - eta1 - h) * (1.0 - eta2 - h)
        + v1z * (eta1 + h) * (1.0 - eta2 - h)
        + v2z * (1.0 - eta1 - h) * (eta2 + h)
        + v3z * (eta1 + h) * (eta2 + h);

      lv0 = quadrant_to_local_vertex[4 * quad_count + 0];
      lv1 = quadrant_to_local_vertex[4 * quad_count + 1];
      lv2 = quadrant_to_local_vertex[4 * quad_count + 2];
      lv3 = quadrant_to_local_vertex[4 * quad_count + 3];

      vert_locations[lv0].x = w0x;
      vert_locations[lv0].y = w0y;
      vert_locations[lv0].z = w0z;

      vert_locations[lv1].x = w1x;
      vert_locations[lv1].y = w1y;
      vert_locations[lv1].z = w1z;

      vert_locations[lv2].x = w2x;
      vert_locations[lv2].y = w2y;
      vert_locations[lv2].z = w2z;

      vert_locations[lv3].x = w3x;
      vert_locations[lv3].y = w3y;
      vert_locations[lv3].z = w3z;
    }
  }

  qsort (vert_locations, num_uniq_local_vertices, sizeof (p4est_vert_t),
         p4est_vert_compare);

  /* Check to make sure that we don't have any duplicates in the list */
  for (i = 0; i < num_uniq_local_vertices - 1; ++i) {
    P4EST_CHECK_ABORT (p4est_vert_compare (vert_locations + i,
                                           vert_locations + i + 1) != 0,
                       "local ordering not unique");
  }

  P4EST_FREE (quadrant_to_local_vertex);
  P4EST_FREE (vert_locations);
}

static int          weight_counter;
static int          weight_index;

static int
weight_once (p4est_t * p4est, int32_t which_tree, p4est_quadrant_t * quadrant)
{
  if (weight_counter++ == weight_index) {
    return 1;
  }

  return 0;
}

int
main (int argc, char **argv)
{
  int                 rank = 0;
#ifdef HAVE_MPI
  int                 mpiret;
#endif
  MPI_Comm            mpicomm;
  p4est_t            *p4est;
  p4est_connectivity_t *connectivity;

  mpicomm = MPI_COMM_NULL;
#ifdef HAVE_MPI
  mpiret = MPI_Init (&argc, &argv);
  P4EST_CHECK_MPI (mpiret);
  mpicomm = MPI_COMM_WORLD;
  mpiret = MPI_Comm_rank (mpicomm, &rank);
  P4EST_CHECK_MPI (mpiret);
#endif
  p4est_init (stdout, rank, NULL, NULL);

  /* create connectivity and forest structures */
  connectivity = p4est_connectivity_new_star ();
  p4est = p4est_new (mpicomm, connectivity, sizeof (user_data_t), init_fn);

  /* refine to make the number of elements interesting */
  p4est_refine (p4est, refine_fn, init_fn);

  /* balance the forest */
  p4est_balance (p4est, init_fn);

  /* do a uniform partition, include the weight function for testing */
  p4est_partition (p4est, weight_one);

  p4est_check_local_order (p4est, connectivity);

  /* do a weighted partition with many zero weights */
  weight_counter = 0;
  weight_index = (rank == 1) ? 1342 : 0;
  p4est_partition (p4est, weight_once);

  p4est_check_local_order (p4est, connectivity);

  /* clean up */
  p4est_destroy (p4est);
  p4est_connectivity_destroy (connectivity);

  /* create connectivity and forest structures */
  connectivity = p4est_connectivity_new_periodic ();
  p4est = p4est_new (mpicomm, connectivity, sizeof (user_data_t), init_fn);

  /* refine to make the number of elements interesting */
  p4est_refine (p4est, refine_fn, init_fn);

  /* balance the forest */
  p4est_balance (p4est, init_fn);

  /* do a uniform partition, include the weight function for testing */
  p4est_partition (p4est, weight_one);

  p4est_check_local_order (p4est, connectivity);

  /* clean up and exit */
  p4est_destroy (p4est);
  p4est_connectivity_destroy (connectivity);
  p4est_memory_check ();

#ifdef HAVE_MPI
  mpiret = MPI_Finalize ();
  P4EST_CHECK_MPI (mpiret);
#endif

  return 0;
}

/* EOF simple.c */