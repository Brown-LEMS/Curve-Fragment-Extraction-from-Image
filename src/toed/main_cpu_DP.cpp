// ======================================================================================
//> (c) LEMS, Brown University
//> Chiang-Heng Chien (chiang-heng_chien@brown.edu)
//> Change Logs
//>     Jun. 2022: Complete CPU and GPU implementation. First test on a small image.
//      Dec. 2023: Fix some bugs on CPU GPU result inconsistency issue
//      Feb. 2025: Initiate a main function for third-order edge detection (without curvelet),
//                 and it is done on CPU-only using double-precision
// ======================================================================================
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <iostream>
#include <string>
#include <string.h>
#include <vector>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#include "indices.hpp"

#if OPENCV_SUPPORT
#include <opencv2/opencv.hpp>
#endif

// cpu
#include "cpu_toed.hpp"

static bool mkdir_p(const std::string& path)
{
    if (path.empty() || path == "." || path == "/")
        return true;

    struct stat st;
    if (stat(path.c_str(), &st) == 0)
        return S_ISDIR(st.st_mode);

    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos && pos > 0) {
        if (!mkdir_p(path.substr(0, pos)))
            return false;
    }

    return (mkdir(path.c_str(), 0755) == 0) || (errno == EEXIST);
}

template<typename T>
void initialize_TOED_edges( T* &TOED_edges, int height, int width ) 
{
    TOED_edges = new T[(2*height)*(2*width)*4];
    // initialization
    for (int i = 0; i < (2*height)*(2*width); i++) {
        for (int j = 0; j < 4; j++) {
            TOED_edges(i, j)  = 0;
        }
    }
}

template<typename T>
void _write_array_to_file(std::string filename, T *wr_data, int first_dim, int second_dim,
                          const std::string& output_dir)
{
#define wr_data(i, j) wr_data[(i) * second_dim + (j)]

    std::cout << "writing data to a file " << filename << " ..." << std::endl;
    if (wr_data == nullptr || first_dim <= 0 || second_dim <= 0) {
        std::cout << "write data file skipped: invalid buffer or dimensions." << std::endl;
        return;
    }

    std::string out_file_name = output_dir;
    if (!out_file_name.empty() && out_file_name.back() != '/')
        out_file_name.push_back('/');
    out_file_name.append(filename);
    std::ofstream out_file(out_file_name);
    if (!out_file.is_open()) {
        std::cout << "write data file cannot be opened!" << std::endl;
        return;
    }

    for (int i = 0; i < first_dim; i++) {
        for (int j = 0; j < second_dim; j++) {
            out_file << wr_data(i, j) << "\t";
        }
        out_file << "\n";
    }

    out_file.close();
#undef wr_data
}

// Save third-order edges as an `.edg` file (matches MATLAB/save_edg.m)
template<typename T>
void save_edg(const std::string& filename, T *edg, int edge_cnt, int width, int height,
              const std::string& output_dir)
{
    if (edg == nullptr || edge_cnt < 0) {
        std::cout << "save_edg skipped: invalid edge buffer." << std::endl;
        return;
    }

    std::string out_file_name = output_dir;
    if (!out_file_name.empty() && out_file_name.back() != '/')
        out_file_name.push_back('/');
    out_file_name.append(filename);

    std::cout << "writing .edg file " << out_file_name << " ..." << std::endl;
    FILE *fid = fopen(out_file_name.c_str(), "w");
    if (fid == nullptr) {
        std::cout << "save_edg: cannot open file!" << std::endl;
        return;
    }

    // Header (match MATLAB/save_edg.m)
    fprintf(fid, "# EDGE_MAP v3.0\n");
    fprintf(fid, "\n");
    fprintf(fid, "# Format :  [Pixel_Pos]  Pixel_Dir Pixel_Conf  [Sub_Pixel_Pos] Sub_Pixel_Dir Sub_Pixel_Conf Sub_Pixel_Conf \n");
    fprintf(fid, "\n");
    fprintf(fid, "WIDTH=%d \n", width);
    fprintf(fid, "HEIGHT=%d \n", height);
    fprintf(fid, "EDGE_COUNT=%d \n", edge_cnt);
    fprintf(fid, "\n\n");

    // Each edge: [x,y,orient,conf] -> pixel + subpixel fields
    for (int i = 0; i < edge_cnt; i++) {
        const double x = static_cast<double>(edg[i * 4 + 0]);
        const double y = static_cast<double>(edg[i * 4 + 1]);
        const double dir = static_cast<double>(edg[i * 4 + 2]);
        const double conf = static_cast<double>(edg[i * 4 + 3]);
        fprintf(fid, "[%d, %d]    %f %f  [%f, %f]  %f %f 0 \n",
                (int)std::round(x), (int)std::round(y), dir, conf,
                x, y, dir, conf);
    }

    fclose(fid);
}

//------------------------------------------------------------------------------
int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0]
                  << " <input_image> [nthreads] [output_dir]" << std::endl;
        return 0;
    }

	//> Exit if the input image file doesn't open
	std::string filename(argv[1]);
	std::ifstream infile(filename, std::ios::binary);
	if (!infile.is_open())
	{
		std::cout << "File " << filename << " not found in directory." << std::endl;
		return 0;
	}

    // Load images
    int height, width;
#if OPENCV_SUPPORT
    cv::Mat img = cv::imread(filename, cv::IMREAD_GRAYSCALE);
    if ( img.empty() ) {
        std::cerr << "Error: Failed to load image " << filename << std::endl;
        return 0;
    }
    height = img.rows;
    width  = img.cols;
#else
    char type[10];
	int intensity;
	// -- Storing header information and copying into the new ouput images --
	infile >> type >> width >> height >> intensity;
#endif

	//> Read number of threads if passed through command line. It is 1 by default.
	int nthreads = toed_cfg::default_nthreads;
	if(argc > 2) {
	    nthreads = atoi( argv[2] );
	}

    std::string output_dir = toed_cfg::default_output_dir;
    if (argc > 3) {
        output_dir = argv[3];
    }
    if (!mkdir_p(output_dir)) {
        std::cerr << "Error: failed to create output directory: " << output_dir << std::endl;
        return 1;
    }
    std::cout << "Output directory: " << output_dir << std::endl;

	//> TOED parameters (edit toed_config.hpp)
	const int kernel_size = toed_cfg::kernel_size;
	const int sigma = toed_cfg::sigma;
    std::cout << "TOED config: sigma=" << sigma
              << " kernel_size=" << kernel_size
              << " grad_mag_thresh=" << toed_cfg::grad_mag_thresh
              << " nms_border=" << toed_cfg::nms_border << std::endl;

    // ==================================== THIRD-ORDER EDGE DETECTOR STARTS HERE ===============================================
    int edge_num;
    double *TOED_edges;
    initialize_TOED_edges<double>( TOED_edges, height, width );

    printf("############################################\n");
    printf("##         Double Precision Test          ##\n");
    printf("############################################\n");
    printf("\n ==> CPU Test (OpenMP %d threads)  \n", nthreads);
    printf("============================================\n");
    
    ThirdOrderEdgeDetectionCPU<double> toedCPU_fp64(height, width, sigma, kernel_size, nthreads);
    toedCPU_fp64.set_output_dir(output_dir);
#if OPENCV_SUPPORT
    toedCPU_fp64.preprocessing(img);
#else
    toedCPU_fp64.preprocessing(infile);
#endif
    toedCPU_fp64.convolve_img();
    edge_num = toedCPU_fp64.non_maximum_suppresion(TOED_edges);

    std::cout << "Number of edges = " << edge_num << std::endl;

    //> save the third-order edges to a file
    toedCPU_fp64.write_array_to_file(toed_cfg::edges_txt_filename, TOED_edges, edge_num, 4);
#if WriteEdgFile
    save_edg(toed_cfg::edges_edg_filename, TOED_edges, edge_num, width, height, output_dir);
#endif

    delete[] TOED_edges;

    return 0;
}
