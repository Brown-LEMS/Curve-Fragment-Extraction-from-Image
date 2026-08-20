# Curve Fragments Extraction From an Image

This repository hosts the code for contour (curve fragment) extraction from an image or a list of edges via a multi-stage approach. It takes either an image or a list of detected edges as input, and returns curve fragments represented as a sequence of ordered edges. See the reference papers below for more details of the methodology.

## Dependency: VXL
This code has been tested using [VXL](https://github.com/vxl/vxl) version 1.18.0, [VXL-1.18.0-patch](https://github.com/C-H-Chien/vxl), and [vxl-lean](https://github.com/Brown-LEMS/vxl-lean), which is the recommended version. Other versions may also work but not yet tested. Follow the standard CMake build process for intializing `build` folder:
```bash
$ cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBOXM2_USE_VOLM=OFF \
  -DBUILD_CONTRIB=ON \
  -DBUILD_RPL=OFF \
  -DBUILD_TESTING=OFF \
  -DBUILD_CORE_VIDEO=OFF \
  -DBUILD_CUL=OFF \
  -DBUILD_DOCUMENTATION=OFF \
  -DBUILD_FOR_VXL_DASHBOARD=OFF \
  -DVNL_CONFIG_LEGACY_METHODS=ON \
  -DVXL_FORCE_B3P_EXPAT=ON
```
Alternatively, using `ccmake` would enable you to visualize and control all the settings. Once CMake files are generated, compile the code with
```bash
$ cmake --build build -k -j{nproc}
```
This enables continual compilation by ignoring any past errors arising from the `contrib` as well as many other scripts. Don't worry about errors at this point since we will not use everything. `{nproc}` can be any integer depending on the number of (CPU) cores you are using.

## Dependency: OpenCV4 / OpenCV4 Contribution
1. Install or build [OpenCV4](https://github.com/opencv/opencv) with [OpenCV4 Contribution](https://github.com/opencv/opencv_contrib) (build directions are in the latter)
    - Before building, make sure (using a package manager or some other method) that `libgtk2.0-dev` is installed on Debian/Ubuntu or `gtk2-devel` on Fedora. `pkg-config` must also be installed.
3. Download the Structured Forest Edge model from [here](https://github.com/opencv/opencv_extra/blob/master/testdata/cv/ximgproc/model.yml.gz)
# show on window
$ ./build/edge_detector/edt -m <model_name> -i <input_img> -s
```

## How to Use the Code
### Build and Compile
```bash
$ cmake -S . -B build -DVXL_DIR=/path/to/vxl/build -DCMAKE_PREFIX_PATH=/path/to/opencv/build
$ cmake --build build -j{nproc}
```
Type in the path of `VXL_DIR` as the path of the VXL build folder you have made in the VXL build step. Also type in the path of your OpenCV build directory for the `CMAKE_PREFIX_PATH` flag. Once the compilation is done, you will see three executables generated under `build`: `MSEL_edges2CFs`, `MSEL_img2CFs`, and `dborl_compute_curve_frags`.

### Usage
The three executables differ in the inputs and the process but they share the same library and symbolic edge linking (SEL). The output `XX.cem` file records all returned contours represented as a seuqence of ordered edges. Although the file extension is `.cem`, it can however be treated as a regular `.txt` file.

| Executable | Required inputs | Pipeline (high level) |
|------------|-----------------|------------------------|
| `MSEL_img2CFs` | RGB image | Third-Order Edge Detection -> SEL -> Geometric Contour Break -> Graphical-Model Merge -> Contour Ranker -> Save CEM |
| `MSEL_edges2CFs` | RGB image + edge file (`.edg`) | Load edge map from file -> SEL -> Geometric Contour Break -> Graphical-Model Merge -> Contour Ranker -> Save CEM |
| `dborl_compute_curve_frags` | edge file (`.edg`) | Load edge map from file -> SEL -> Save CEM (No curve break / merge / rank) |

**Command-line quick reference** (from `build/`) you can use the examples under `example_data/`:
- **`MSEL_img2CFs`**: `<image-name>.png <cem-name>.cem <nContours> <e_sigma> <e_thresh>`
  - `nContours`: number of returned contours after ranking. Set 0 if opting for returning all ranked contours.
  - `e_sigma`: Sigma parameter for the third-order edge detection.
  - `e_thresh` Gradient threshold for the third-order edge detection.

	Example:
	```bash
	$ ./build/MSEL_img2CFs example_data/cabinet.png example_data/cabinet.cem 200 1 1
	```

- **`MSEL_edges2CFs`**: `<image-name>.png <edge-name>.edg <cem-name>.cem <nContours>`
  - `<edge-name>.edg`: an edge file generated through third-order edge detection (you can use [this repository](https://github.com/C-H-Chien/Third-Order-Edge-Detector) to generate one) or a third-party edge detector with edges structured as an `.edg` file. Although the file extension is `.edg`, it can however be treated as a regular `.txt` file.

	Example:
	```bash
	$ ./build/MSEL_edges2CFs example_data/cabinet.png example_data/cabinet.edg example_data/cabinet.cem 200
	```

- **`dborl_compute_curve_frags`**: `<edge-name>.edg <output-cem-name>.cem`
  This is primarily used in the topological contour graph code. If you opt for curve fragments extraction only (without organizing the curve into a topological contour graph), then this executable can be ignored.

- **`ocv_compute_curve_frags`**: `<image-name> <output-cem-name>.cem [<output-edg-name>.edg] [threshold=0.1]`
    - This is a replacement for `dborl_compute_curve_frags` that uses a built-in OpenCV Structured Forest Edge detector. See above for usage; the executable needs an image, an output `.cem` file, and optionally a `.edg` output file, which is helpful for visualizing and testing the edge detector seperate from the `.cem` curve fragment output. Additionally, an optional threshold can be specified (default is 0.1 if cli argument is elided) that determines at what intensity a pixel should be recorded as an edgel. Leaving this at 0.1 is generally good, although lowering this can reduce noise and raising it can boost the edge detector's sensitivity.

## Visualization
We provide a simple MATLAB code `demo_vis_io.m` for visualizing the generated curve fragments. Simply run that script with specified image and `cem` file.

## Contributors
The code was originally implemented by [Yuliang Guo](https://github.com/yuliangguo). It was updated (by fixing some memory leak issues), tested, and documented by [Chiang-Heng Chien](https://github.com/C-H-Chien). Additional changes and the code for `ocv_compute_curve_frags` was made by [Zach Mahan](https://github.com/zachMahan64).

## References
The main paper this code arises from is:
```BibTeX
@inproceedings{guo2014multi,
  title={A multi-stage approach to curve extraction},
  author={Guo, Yuliang and Kumar, Naman and Narayanan, Maruthi and Kimia, Benjamin},
  booktitle={European Conference on Computer Vision},
  pages={663--678},
  year={2014},
  organization={Springer}
}
```
Below are papers that the multi-stage approach builds upon:
```BibTex
@inproceedings{guo2012evaluating,
  title={On evaluating methods for recovering image curve fragments},
  author={Guo, Yuliang and Kimia, Benjamin},
  booktitle={2012 IEEE Computer Society Conference on Computer Vision and Pattern Recognition Workshops},
  pages={9--16},
  year={2012},
  organization={IEEE}
}
```
```BibTex
@inproceedings{tamrakar2007no,
  title={No grouping left behind: From edges to curve fragments},
  author={Tamrakar, Amir and Kimia, Benjamin B},
  booktitle={2007 IEEE 11th International Conference on Computer Vision},
  pages={1--8},
  year={2007},
  organization={IEEE}
}
```
For third-order edge detection, refer the the following paper and its self-contained implementation [here](https://github.com/C-H-Chien/Third-Order-Edge-Detector):
```BibTex
@article{kimia2018differential,
  title={Differential geometry in edge detection: accurate estimation of position, orientation and curvature},
  author={Kimia, Benjamin B and Li, Xiaoyan and Guo, Yuliang and Tamrakar, Amir},
  journal={IEEE transactions on pattern analysis and machine intelligence},
  volume={41},
  number={7},
  pages={1573--1586},
  year={2018},
  publisher={IEEE}
}
```
