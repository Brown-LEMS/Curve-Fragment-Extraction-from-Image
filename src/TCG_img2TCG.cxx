//:
// \file
// \author Yuliang Guo (yuliang_guo@brown.edu)
// \date 05/30/2017
//
//        An algorithm running the Multi-stage contour extraction
//		  Usage: ./compute_contours_from_img img_file output_cem_file nContours e_sigma e_thresh
//				 nContours: # of top ranking contours to output
//				 e_sigma: edge detection filter sigma
//				 e_thresh: edge threshold
//      
// \verbatim
//   Modifications
//  
// \endverbatim

//
//

#include <vcl_iostream.h>
#include <vul/vul_file.h>
#include <vil/vil_image_resource_sptr.h>
#include <vil/vil_load.h>
//#include <dborl/algo/dborl_utilities.h>
#include <vul/vul_timer.h>
#include <vcl_set.h>

#include "core/vidpro1_image_storage_sptr.h"
#include "core/vidpro1_image_storage.h"
#include "core/vidpro1_save_cem_process.h"
//#include "core/vidpro1_save_con_process.h"
//#include "core/vidpro1_save_image_process.h"

//#include "dbdet_third_order_edge_detector_process.h"
#include "core/dbdet_third_order_color_edge_detector_process.h"
#include "core/dbdet_sel_process.h"
//#include "dbdet_generic_linker_process.h>
//#include "dbdet_sel_extract_contours_process.h>
//#include "dbdet_contour_tracer_process.h>
//#include "dbdet_prune_curves_process.h>
//#include "core/dbdet_save_edg_process.h"
#include "core/dbdet_load_edg_process.h"
#include "core/dbdet_edgemap_storage.h"
//#include "dbdet_save_cvlet_map_process.h>
//#include "core/dbdet_load_cem_process.h"
#include "core/dbdet_save_cem_process.h"
//#include "dbdet_prune_fragments_Logistic_Regression.h>
#include "core/dbdet_sel_storage_sptr.h"
#include "core/dbdet_sel_storage.h"

#include "core/dbdet_contour_breaker_geometric_process.h"
#include "core/dbdet_graphical_model_contour_merge_process.h"
#include "core/dbdet_contour_ranker_process.h"


int main(int argc, char *argv[]) {

    // Let time how long this takes
    // Start timer
    vul_timer t;

    int nContours = 0;
    double e_sigma = 2;
    double e_thresh = 1;

    //if(atoi(argv[0])>=3)
    	nContours = atoi(argv[3]);
    //if(atoi(argv[0])>=4)
    	e_sigma = atof(argv[4]);
    //if(atoi(argv[0])>=5)
    	e_thresh = atof(argv[5]);


	vcl_cout<<"************* load in image *********"<<vcl_endl;
	//load the input image
	vcl_string img_file = argv[1];

	if (!vul_file::exists(img_file))
	{
		vcl_cerr << "Cannot find image: " << img_file << vcl_endl;
		return 1;
	}

	vil_image_resource_sptr img_sptr = vil_load_image_resource(img_file.c_str());
	vidpro1_image_storage_sptr input_img = new vidpro1_image_storage();
	input_img->set_image(img_sptr);
	if (!img_sptr)
	{
		vcl_cerr << "Cannot load image: " << img_file << vcl_endl;
		return 1;
	}



    vcl_cout<<"************ Edge Detection   ************"<<vcl_endl;

	vcl_vector<bpro1_storage_sptr> edge_det_results;
    // Perform color third order edge detection
    if (img_sptr->nplanes() == 3)
    {

        dbdet_third_order_color_edge_detector_process pro_color_edg;
    	pro_color_edg.parameters()->set_value("-sigma",e_sigma);
    	pro_color_edg.parameters()->set_value("-thresh",e_thresh);
        //set_process_parameters_of_bpro1(*params,
                                        //pro_color_edg,
                                        //params->
                                        //tag_color_edge_detection_);

        //vcl_vector<bpro1_param*> pars  = pro_color_edg.parameters()->get_param_list();
		//for (unsigned i = 0; i < pars.size(); i++)
		//{
	    //    	if(pars[i]->name()=="-thresh")
		//	color_thresh_ = pars[i]->value_str();
		//}
        // Before we start the process lets clean input output
        pro_color_edg.clear_input();
        pro_color_edg.clear_output();

        pro_color_edg.add_input(input_img);
        bool to_c_status = pro_color_edg.execute();
        pro_color_edg.finish();

        // Grab output from color third order edge detection
        // if process did not fail
        if ( to_c_status )
        {
            edge_det_results = pro_color_edg.get_output();
        }
        else
        {
        	vcl_cerr << "Error in edge detection" << vcl_endl;
        	return 1;
        }
        //Clean up after ourselves
        pro_color_edg.clear_input();
        pro_color_edg.clear_output();
    }
    else
    {
    	vcl_cerr << "Input image are required to be RGB" << vcl_endl;
    	return 1;
    }


    dbdet_edgemap_storage_sptr input_edg;
    input_edg.vertical_cast(edge_det_results[0]);
    dbdet_edgemap_sptr edgemap_sptr = input_edg->get_edgemap();



    //******************** Edge Linking *********************************
    // Perform sel linking if we are not doing contour tracing

    // Set up storage for sel results
    vcl_vector<bpro1_storage_sptr> el_results;
	vcl_cout<<"************ Symbolic Edge Linking     ************"<<vcl_endl;
	dbdet_sel_process sel_pro;
//	set_process_parameters_of_bpro1(*params, sel_pro, params->tag_edge_linking_);
	                                  
	// Before we start the process lets clean input output
	sel_pro.clear_input();
	sel_pro.clear_output();

	// Use input from edge detection
	sel_pro.add_input(edge_det_results[0]);
	bool el_status = sel_pro.execute();
	sel_pro.finish();

	// Grab output from symbolic edge linking
	// if process did not fail
	if ( el_status )
	{
	    el_results = sel_pro.get_output();
	}

	//Clean up after ourselves
	sel_pro.clear_input();
	sel_pro.clear_output();

	if (el_results.size() != 1) 
	{
	    vcl_cerr << "Process output does not contain a sel data structure"
	             << vcl_endl;
	    return 1;
	}


	// Grab curve fragments
	dbdet_sel_storage_sptr input_sel = new dbdet_sel_storage();
	input_sel.vertical_cast(el_results[0]);
	dbdet_curve_fragment_graph& CFG = input_sel->CFG();

    //*********************** TODO convert matlab code to CXX *********************************
	
    // 1. ******************* contour_breaker_at_conner ***********************************

    // 2.******************** contour_fill_gaps_DP *****************************************

    // 3.******************** break_contours_at_T_junctions.m ******************************

    // 4.******************** prune_noise_curves.m *****************************************

    // 5.******************** merge_cfrags_graphical_model_geom.m **************************

    // 6.******************** classify_junction_type_wrt_graph_BP.m ************************

    // 7. rerun 1: ********** contour_breaker_at_conner.m **********************************

    // 8. retun 4: ********** prune_noise_curves.m *****************************************






    //******************** Save Contours  *********************************
    // Change to the dbdet version by Yuliang
    vcl_cout<<"************ Saving Contours  ************"<<vcl_endl;

    vcl_string output_file;
    output_file = argv[2];

    bool write_status(false);
    vcl_cout << "output: " << output_file << vcl_endl;
    
    bpro1_filepath output(output_file, ".cem");

    // In this everything else, is .cem, .cemv , .cfg, etc
    dbdet_save_cem_process save_cem_pro;
    save_cem_pro.parameters()->set_value("-cem_filename",output);

    // Before we start the process lets clean input output
    save_cem_pro.clear_input();
    save_cem_pro.clear_output();

    // Kick of process
    //save_cem_pro.add_input(el_results[0]);
    save_cem_pro.add_input(el_results[0]);
    write_status = save_cem_pro.execute();
    save_cem_pro.finish();

    //Clean up after ourselves
    save_cem_pro.clear_input();
    save_cem_pro.clear_output();


    double vox_time = t.real()/1000.0;
    t.mark();
    vcl_cout<<vcl_endl;
    vcl_cout<<"************ Time taken: "<<vox_time<<" sec"<<vcl_endl;

    // Just to be safe lets flush everything
    vcl_cerr.flush();
    vcl_cout.flush();

    //Success we made it this far
    return 0;
}

