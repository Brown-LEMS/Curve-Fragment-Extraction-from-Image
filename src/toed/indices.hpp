#ifndef INDICES_HPP
#define INDICES_HPP
// Indexing macros for CPU arrays.
// Feature flags and algorithm parameters live in toed_config.hpp.

#include "toed_config.hpp"

// cpu
#define img(i,j)                        img[(i) * img_width + (j)]
#define Ix(i, j)                            Ix[(i) * interp_img_width + (j) ]
#define Iy(i, j)                            Iy[(i) * interp_img_width + (j) ]
#define I_grad_mag(i, j)            I_grad_mag[(i) * interp_img_width + (j) ]
#define I_orient(i, j)                I_orient[(i) * interp_img_width + (j) ]

#define subpix_pos_x_map(i,j)                 subpix_pos_x_map[(i) * interp_img_width + (j)]
#define subpix_pos_y_map(i,j)                 subpix_pos_y_map[(i) * interp_img_width + (j)]
#define subpix_grad_mag_map(i,j)           subpix_grad_mag_map[(i) * interp_img_width + (j)]
#define subpix_edge_pts_final(i,j)       subpix_edge_pts_final[(i) * num_of_edge_data + (j)]

// output edge list: [x, y, orientation, strength] per row
#define TOED_edges(i, j)                 TOED_edges[(i) * 4 + (j)]

#endif // INDICES_HPP
