#include "dbdet_yuliang_features.h"

constexpr double dbdet_yuliang_const::diag_of_train; // ???
unsigned const dbdet_yuliang_const::nbr_num_edges;  // # of edges close to connecting points
unsigned const dbdet_yuliang_const::nbr_len_th; // short curve under this length will be grouped due to geometry.
constexpr double dbdet_yuliang_const::merge_th_sem;
constexpr double dbdet_yuliang_const::merge_th_geom;
constexpr double dbdet_yuliang_const::epsilon;
