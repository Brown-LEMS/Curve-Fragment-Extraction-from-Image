// This is dbdet_yuliang_features.h
#ifndef dbdet_yuliang_features_h
#define dbdet_yuliang_features_h
#include <vnl/vnl_vector_fixed.h>
#include "cf_params.h"


#define y_params_1_size 2
typedef vnl_vector_fixed<double, y_params_1_size> y_params_1_vector;

#define y_params_0_size 9
typedef vnl_vector_fixed<double, y_params_0_size> y_params_0_vector;
typedef vnl_vector_fixed<double, y_params_0_size> y_feature_vector;

#define y_hist_size 64

typedef vnl_vector_fixed<double, y_hist_size> y_hist_vector;

//hackish solution to not use scoped enums and enforce c++11
// just to make it easier and more descriptive to index the feature vector
// rarely used since we just process the feature vec. generically
// Y_ONE is just always 1
namespace y_params_0 {

  enum {
    Y_ONE, Y_BG_GRAD, Y_SAT_GRAD, Y_HUE_GRAD, Y_ABS_K, Y_EDGE_SPARSITY, Y_WIGG, Y_GEOM, Y_TEXTURE
  };
}
namespace y_features {

  enum {
    Y_ONE, Y_BG_GRAD, Y_SAT_GRAD, Y_HUE_GRAD, Y_ABS_K, Y_EDGE_SPARSITY, Y_WIGG, Y_LEN, Y_MEAN_CONF
  };
}

class dbdet_yuliang_const {

public:
  // Values live in cf_params.h so they can be tuned in one place.
  static constexpr double diag_of_train = cf_params::diag_of_train;
  static unsigned const nbr_num_edges = cf_params::nbr_num_edges;
  static unsigned const nbr_len_th = cf_params::nbr_len_th;
  static constexpr double merge_th_sem = cf_params::merge_th_sem;
  static constexpr double merge_th_geom = cf_params::merge_th_geom;
  static constexpr double epsilon = 1e-10;
};

#endif // dbdet_yuliang_features_h
