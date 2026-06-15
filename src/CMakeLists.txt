cmake_minimum_required(VERSION 2.8)
project( sel_linker )

FIND_PACKAGE(VXL)
#FIND_PACKAGE(VXD)
#FIND_PACKAGE(LEMS)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


IF(VXL_FOUND)
    # include the standard VXL stuff
    #SET(VXL_PROVIDE_STANDARD_OPTIONS 1)
    INCLUDE(${VXL_CMAKE_DIR}/UseVXL.cmake)
    INCLUDE(${VXL_DIR}/VXLConfig.cmake)
    INCLUDE_DIRECTORIES( ${VXL_GEL_INCLUDE_DIR} )
    INCLUDE_DIRECTORIES( ${VXL_MUL_INCLUDE_DIR} )
    INCLUDE_DIRECTORIES( ${VXL_BRL_INCLUDE_DIR} )
    #MESSAGE("VXL_INCLUDE_DIR is ${VXL_CORE_INCLUDE_DIR}")
    MESSAGE("VXL_ROOT_SOURCE_DIR is ${VXL_ROOT_SOURCE_DIR}")

    set( dborl_edge_det_link_sources
  dborl_edge_det_link_main.cxx
 )

    set( MSEL_from_edge_sources
    MSEL_edges2CFs.cxx
 )

    set( MSEL_from_img_sources
    MSEL_img2CFs.cxx
 )

    add_executable (dborl_compute_curve_frags ${dborl_edge_det_link_sources} )
    target_link_libraries (dborl_compute_curve_frags core)

    add_executable (MSEL_edges2CFs ${MSEL_from_edge_sources} )
    target_link_libraries (MSEL_edges2CFs core)

    add_executable (MSEL_img2CFs ${MSEL_from_img_sources} )
    target_link_libraries (MSEL_img2CFs core)

    add_subdirectory( core )

ENDIF(VXL_FOUND)
