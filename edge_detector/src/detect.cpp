#include "detect.hpp"

#include "opencv2/highgui.hpp"
#include <iostream>
#include <opencv2/ximgproc.hpp>

namespace edt {
Mat grayscale(const Mat& img_orig) {
    Mat img_gray;
    cvtColor(img_orig, img_gray, COLOR_BGR2GRAY);
    return img_gray;
}

Mat blur(const Mat& img_orig) {
    Mat img_blur;
    GaussianBlur(img_orig, img_blur, Size(3, 3), 0);
    return img_blur;
}

Mat sobel(const Mat& img_orig, SobelDir dir) {
    uint8_t dx = 0;
    uint8_t dy = 0;
    switch (dir) {
    case SobelDir::dx:
        dx = 1;
        break;
    case SobelDir::dy:
        dy = 1;
        break;
    case SobelDir::dxdy:
        dx = 1;
        dy = 1;
        break;
    }
    Mat sobel;
    Sobel(img_orig, sobel, CV_64F, dx, dy, 5);
    return sobel;
}

Mat canny(const Mat& img_orig) {
    Mat edges;
    Canny(img_orig, edges, 100, 200, 3, false);
    return edges;
}

void display_and_wait_on_window(const std::string& win_name, const Mat& img) {
    imshow(win_name, img);
    waitKey(0);
}

int run_sobel_canny_demo(int argc, char** argv) {
    constexpr auto DEFAULT_IMG = "images/fish.jpg";
    const char* image = DEFAULT_IMG;

    if (argc > 1) {
        image = argv[1];
    }

    edt::sobel_canny_demo(image);
    return 0;
}

void sobel_canny_demo(const char* img_path) {
    using namespace cv;
    using namespace edt;
    // reading image
    Mat img = imread(img_path);

    constexpr auto win_name =
        "Edge-detect demo | Press [Enter] to see next slide";

    cv::namedWindow(win_name, cv::WINDOW_NORMAL);

    cv::resizeWindow(win_name, 800, 600);

    // display original image
    display_and_wait_on_window(win_name, img);

    // convert to graycsale
    Mat img_gray = grayscale(img);

    // blur the image for better edge detection
    Mat img_blur = blur(img_gray);

    // sobel edge detection
    Mat sobel_x = sobel(img_blur, SobelDir::dx);
    Mat sobel_y = sobel(img_blur, SobelDir::dy);
    Mat sobel_xy = sobel(img_blur, SobelDir::dxdy);

    // display sobel edge detection images
    imshow(win_name, sobel_x);
    waitKey(0);
    imshow(win_name, sobel_y);
    waitKey(0);
    imshow(win_name, sobel_xy);
    waitKey(0);

    // canny edge detection
    Mat edges = canny(img_blur);
    // display canny edge detected image
    imshow(win_name, edges);
    waitKey(0);

    destroyAllWindows();
}

/// The following code is adapted from the OpenCV Contrib implementation demo:
/// found here:
/// https://github.com/opencv/opencv_contrib/blob/115e941b7a2a95a2855a7cfa7332ea64707a8f3b/modules/ximgproc/samples/structured_edge_detection.cpp#L53
int structured_edges_demo(int argc, char** argv) {

    /**************************************************************************************
    The structured forests for fast edge detection demo requires you to provide
    a model. This model can be found at the opencv_extra repository on Github on
    the following link:
    https://github.com/opencv/opencv_extra/blob/master/testdata/cv/ximgproc/model.yml.gz
    ***************************************************************************************/

    using namespace cv::ximgproc;

    auto print_usage = +[] {
        std::cout
            << "usage: -m <model_name> -i <input_img> [-o <output_name>] | "
               "[-s]\n"
            << "                                      ^ output to a file   ^ "
               "show edges on window"
            << '\n'
            << "note: argument order must match above\n";
    };

    // parse args ~~~~~~~~~~

    if (argc < 5 + 1) {
        print_usage();
        return -1;
    }

    if (String{argv[1]} != String{"-m"}) {
        print_usage();
        return -1;
    }
    String modelFilename{argv[2]};

    if (String{argv[3]} != String{"-i"}) {
        print_usage();
        return -1;
    }
    String inFilename{argv[4]};

    if ((String{argv[5]} == String{"-o"}) && argc != 7) {
        print_usage();
        return -1;
    }

    String outFilename{};
    if (String{argv[5]} == String{"-o"}) {
        outFilename = argv[6];
    } else if (String{argv[5]} != String{"-s"}) {
        print_usage();
        return -1;
    }

    // ^^^^^^^^^^^^^^^^^^^^^^

    //! [imread]
    Mat image = imread(inFilename, IMREAD_COLOR);
    if (image.empty())
        CV_Error(Error::StsError,
                 String("Cannot read image file: ") + inFilename);
    //! [imread]

    if (modelFilename.size() == 0)
        CV_Error(Error::StsError, String("Empty model name"));

    //! [convert]
    image.convertTo(image, DataType<float>::type, 1 / 255.0);
    //! [convert]

    TickMeter tm;
    tm.start();
    //! [create]
    Ptr<StructuredEdgeDetection> pDollar =
        createStructuredEdgeDetection(modelFilename);
    //! [create]

    tm.stop();
    std::cout << "createStructuredEdgeDetection() time : " << tm << '\n';

    tm.reset();
    tm.start();
    //! [detect]
    Mat edges;
    pDollar->detectEdges(image, edges);
    //! [detect]
    tm.stop();
    std::cout << "detectEdges() time : " << tm << '\n';

    tm.reset();
    tm.start();
    //! [nms]
    // computes orientation from edge map
    Mat orientation_map;
    pDollar->computeOrientation(edges, orientation_map);

    // suppress edges
    Mat edge_nms;
    pDollar->edgesNms(edges, orientation_map, edge_nms, 2, 0, 1, true);
    //! [nms]

    tm.stop();
    std::cout << "nms time : " << tm << '\n';

    constexpr auto edges_name = "edges";
    cv::namedWindow(edges_name, cv::WINDOW_NORMAL);
    cv::resizeWindow(edges_name, 800, 600);

    constexpr auto edges_nms_name = "edges nms";
    cv::namedWindow(edges_nms_name, cv::WINDOW_NORMAL);
    cv::resizeWindow(edges_nms_name, 800, 600);

    //! [imshow]
    if (outFilename.size() == 0) {
        imshow(edges_name, edges);
        imshow(edges_nms_name, edge_nms);
        waitKey(0);
    } else
        imwrite(outFilename, 255 * edges);
    //! [imshow]

    return 0;
}

} // namespace edt
