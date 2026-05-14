# YOLO Pose Detection - OAK RGBD migration build
# Author: Chris / generated OAK RGBD adapter

cmake_minimum_required(VERSION 3.20)
project(YOLOPoseDetection VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_OAK_RGBD_TARGET "Build OAK/DepthAI RGBD target" ON)
option(BUILD_INDEMIND_TARGET "Build legacy INDEMIND target" OFF)

set(DEPTHAI_CORE_ROOT "/home/chris4/workspace/from_git/depthai-core" CACHE PATH "depthai-core source root")
set(DEPTHAI_CORE_BUILD_DIR "${DEPTHAI_CORE_ROOT}/build" CACHE PATH "depthai-core build directory")

# Allow overriding output dir to avoid permission issues with root-owned build/.
if(NOT DEFINED YOLO_OUTPUT_DIR)
    set(YOLO_OUTPUT_DIR "${PROJECT_SOURCE_DIR}/build_agent_out")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message(STATUS "=========================================")
message(STATUS "  YOLO Pose Detection Standalone Build")
message(STATUS "=========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Project directory: ${PROJECT_SOURCE_DIR}")
message(STATUS "Output directory: ${YOLO_OUTPUT_DIR}")
message(STATUS "BUILD_OAK_RGBD_TARGET: ${BUILD_OAK_RGBD_TARGET}")
message(STATUS "BUILD_INDEMIND_TARGET: ${BUILD_INDEMIND_TARGET}")
message(STATUS "DEPTHAI_CORE_ROOT: ${DEPTHAI_CORE_ROOT}")
message(STATUS "DEPTHAI_CORE_BUILD_DIR: ${DEPTHAI_CORE_BUILD_DIR}")

if(WIN32)
    set(OS_WINDOWS TRUE)
    message(STATUS "Platform: Windows")
elseif(UNIX AND NOT APPLE)
    set(OS_LINUX TRUE)
    message(STATUS "Platform: Linux")
elseif(APPLE)
    set(OS_MACOS TRUE)
    message(STATUS "Platform: macOS")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")

# 1. OpenCV (Required)
find_package(OpenCV REQUIRED)
# Save the OpenCV found for the YOLO application immediately.
# depthaiConfig.cmake may call find_dependency(OpenCV) later and overwrite
# OpenCV_* variables with the OpenCV version used by depthai-core.
# We keep these saved variables for our final executable and link them again
# at the end of the link line so static depthai objects that reference cv::*
# symbols are also resolved.
set(YOLO_OPENCV_VERSION "${OpenCV_VERSION}")
set(YOLO_OPENCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}")
set(YOLO_OPENCV_LIBS "${OpenCV_LIBS}")
message(STATUS "Found OpenCV for YOLO app: ${YOLO_OPENCV_VERSION}")
message(STATUS "  Include: ${YOLO_OPENCV_INCLUDE_DIRS}")
message(STATUS "  Libraries: ${YOLO_OPENCV_LIBS}")

# 2. ONNX Runtime (Required) - prefer Conda GPU build when available.
if(DEFINED ENV{CONDA_PREFIX})
    set(CONDA_PREFIX $ENV{CONDA_PREFIX})
    message(STATUS "Detected Conda environment: ${CONDA_PREFIX}")

    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            ${CONDA_PREFIX}/include/onnxruntime
            ${CONDA_PREFIX}/include
        NO_DEFAULT_PATH
        DOC "ONNX Runtime include directory"
    )

    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS ${CONDA_PREFIX}/lib
        NO_DEFAULT_PATH
        DOC "ONNX Runtime library"
    )
endif()

if(NOT ONNXRUNTIME_INCLUDE_DIR OR NOT ONNXRUNTIME_LIB)
    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            /usr/local/include/onnxruntime
            /usr/include/onnxruntime
            C:/Program\ Files/onnxruntime/include
            ${PROJECT_SOURCE_DIR}/onnxruntime/include
        DOC "ONNX Runtime include directory"
    )

    find_library(ONNXRUNTIME_LIB
        NAMES onnxruntime
        PATHS
            /usr/local/lib
            /usr/lib
            C:/Program\ Files/onnxruntime/lib
            ${PROJECT_SOURCE_DIR}/onnxruntime/lib
        DOC "ONNX Runtime library"
    )
endif()

if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
    message(STATUS "Found ONNX Runtime")
    message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIR}")
    message(STATUS "  Library: ${ONNXRUNTIME_LIB}")
else()
    message(FATAL_ERROR "ONNX Runtime not found. Set ONNXRUNTIME_INCLUDE_DIR and ONNXRUNTIME_LIB manually.")
endif()


# Helpers for using a depthai-core build tree directly.
# Some depthaiConfig.cmake files exported from build/ contain references to
# transitive imported targets (fmt::fmt, spdlog::spdlog, yaml-cpp::yaml-cpp,
# etc.) without importing every target first. We try to import common packages,
# add a few compatibility aliases, and then remove only still-missing target
# references from the DepthAI imported target interface. This keeps the project
# usable with /path/to/depthai-core/build as well as with an installed prefix.
function(yolo_alias_interface_target alias_target real_target)
    if(NOT TARGET "${alias_target}" AND TARGET "${real_target}")
        add_library("${alias_target}" INTERFACE IMPORTED)
        set_target_properties("${alias_target}" PROPERTIES
            INTERFACE_LINK_LIBRARIES "${real_target}"
        )
        message(STATUS "Created compatibility alias ${alias_target} -> ${real_target}")
    endif()
endfunction()

function(yolo_try_find_depthai_transitive_deps)
    find_package(fmt CONFIG QUIET)
    find_package(yaml-cpp CONFIG QUIET)
    find_package(spdlog CONFIG QUIET)
    find_package(BZip2 QUIET)
    find_package(LibArchive QUIET)
    find_package(OpenSSL QUIET)
    find_package(CURL QUIET)
    find_package(Eigen3 QUIET)
    find_package(Protobuf QUIET)
    find_package(absl CONFIG QUIET)
    find_package(LibLZMA QUIET)
    find_package(liblzma CONFIG QUIET)
    find_package(lz4 CONFIG QUIET)
    find_package(httplib CONFIG QUIET)
    find_package(cpp-httplib CONFIG QUIET)
    find_package(semver CONFIG QUIET)
    find_package(magic_enum CONFIG QUIET)
    find_package(mp4v2 CONFIG QUIET)
    find_package(websocketpp CONFIG QUIET)
    find_package(apriltag CONFIG QUIET)
    find_package(cpr CONFIG QUIET)
    find_package(Backward CONFIG QUIET)

    # Compatibility aliases for common target-name differences.
    yolo_alias_interface_target(yaml-cpp::yaml-cpp yaml-cpp)
    yolo_alias_interface_target(liblzma::liblzma LibLZMA::LibLZMA)
    yolo_alias_interface_target(CURL::libcurl_static CURL::libcurl)
    yolo_alias_interface_target(httplib::httplib cpp-httplib::cpp-httplib)
    yolo_alias_interface_target(protobuf::libprotobuf protobuf::libprotobuf-lite)

    # Some packages are header-only in many builds. If they are still missing,
    # leaving a placeholder is safer than failing CMake generation before the
    # actual compiler/linker can tell us whether the symbols are needed.
    foreach(_header_only_target
        httplib::httplib
        semver::semver
        magic_enum::magic_enum
        websocketpp::websocketpp
        Backward::Backward
    )
        if(NOT TARGET "${_header_only_target}")
            add_library("${_header_only_target}" INTERFACE IMPORTED)
            message(WARNING "Created placeholder target ${_header_only_target}. If final link fails, install/export the matching DepthAI dependency package.")
        endif()
    endforeach()
endfunction()

function(yolo_strip_missing_interface_targets target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()

    get_target_property(_iface_libs "${target_name}" INTERFACE_LINK_LIBRARIES)
    if(NOT _iface_libs)
        return()
    endif()

    set(_fixed_iface_libs)
    foreach(_dep IN LISTS _iface_libs)
        set(_keep TRUE)
        set(_target_ref "")

        if("${_dep}" MATCHES "^\\$<LINK_ONLY:([^>]+)>$")
            set(_target_ref "${CMAKE_MATCH_1}")
        elseif("${_dep}" MATCHES "^[A-Za-z0-9_.+-]+::[A-Za-z0-9_.+-]+$")
            set(_target_ref "${_dep}")
        endif()

        if(_target_ref AND NOT TARGET "${_target_ref}")
            message(WARNING "Dropping missing DepthAI transitive target '${_target_ref}' from ${target_name} interface. Prefer installing depthai-core to a prefix if the later link step fails.")
            set(_keep FALSE)
        endif()

        if(_keep)
            list(APPEND _fixed_iface_libs "${_dep}")
        endif()
    endforeach()

    set_target_properties("${target_name}" PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_fixed_iface_libs}"
    )
endfunction()


# Collect Abseil libraries exported by the same depthai-core dependency tree.
# Some depthai/protobuf static builds expose Abseil symbols such as
# absl::lts_20250127::str_format_internal but do not propagate every absl
# library into the final executable link line. Adding the matching vcpkg
# Abseil libraries at the end of the link line fixes those undefined refs.
set(YOLO_DEPTHAI_EXTRA_LINK_LIBS)
function(yolo_collect_absl_link_libs out_var)
    set(_out)

    foreach(_absl_target
        absl::base
        absl::raw_logging_internal
        absl::log_severity
        absl::strings
        absl::strings_internal
        absl::str_format
        absl::str_format_internal
        absl::int128
        absl::time
        absl::time_zone
        absl::civil_time
        absl::synchronization
        absl::graphcycles_internal
        absl::kernel_timeout_internal
        absl::hash
        absl::city
        absl::low_level_hash
        absl::raw_hash_set
        absl::hashtablez_sampler
        absl::exponential_biased
        absl::status
        absl::statusor
        absl::cord
        absl::cord_internal
        absl::cordz_functions
        absl::cordz_handle
        absl::cordz_info
        absl::cordz_sample_token
        absl::crc32c
        absl::crc_cord_state
        absl::crc_cpu_detect
        absl::crc_internal
        absl::debugging_internal
        absl::demangle_internal
        absl::stacktrace
        absl::symbolize
        absl::throw_delegate
        absl::spinlock_wait
        absl::malloc_internal
        absl::log_internal_check_op
        absl::log_internal_conditions
        absl::log_internal_fnmatch
        absl::log_internal_format
        absl::log_internal_globals
        absl::log_internal_log_sink_set
        absl::log_internal_message
        absl::log_internal_nullguard
        absl::log_internal_proto
        absl::log_internal_structured_proto
        absl::log_sink
        absl::vlog_config_internal
        absl::random_internal_randen
        absl::random_internal_randen_hwaes
        absl::random_internal_randen_hwaes_impl
        absl::random_internal_randen_slow
        absl::random_internal_platform
        absl::random_internal_pool_urbg
        absl::random_internal_seed_material
        absl::random_seed_gen_exception
        absl::random_seed_sequences
    )
        if(TARGET "${_absl_target}")
            list(APPEND _out "${_absl_target}")
        endif()
    endforeach()

    # Fallback for build-tree/vcpkg cases where absl targets are not fully
    # imported but matching static archives are present. Keep these archives
    # after depthai/protobuf on the link line.
    set(_absl_globs
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed/*/lib/libabsl*.a"
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed/*/lib/libabsl*.so"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed/*/lib/libabsl*.a"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed/*/lib/libabsl*.so"
    )
    foreach(_glob IN LISTS _absl_globs)
        file(GLOB _matched_absl_libs CONFIGURE_DEPENDS "${_glob}")
        if(_matched_absl_libs)
            list(APPEND _out ${_matched_absl_libs})
        endif()
    endforeach()

    if(_out)
        list(REMOVE_DUPLICATES _out)
        message(STATUS "Extra Abseil link libraries for DepthAI/protobuf: ${_out}")
    endif()
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()


# When using a depthai-core build tree, CMake may append protobuf/absl/utf8
# static libraries as transitive dependencies after our manually added fixup
# libraries. GNU ld resolves static archives left-to-right, so protobuf may
# still see missing absl/utf8 symbols. Remove only those problematic entries
# from the depthai imported target interface and add the matching static
# archives ourselves in one final --start-group/--end-group block.
function(yolo_strip_protobuf_absl_utf8_from_interface target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()

    get_target_property(_iface_libs "${target_name}" INTERFACE_LINK_LIBRARIES)
    if(NOT _iface_libs)
        return()
    endif()

    set(_fixed_iface_libs)
    foreach(_dep IN LISTS _iface_libs)
        set(_drop FALSE)
        set(_dep_text "${_dep}")

        if(_dep_text MATCHES "protobuf" OR
           _dep_text MATCHES "absl" OR
           _dep_text MATCHES "utf8" OR
           _dep_text MATCHES "utf8_range" OR
           _dep_text MATCHES "utf8_validity")
            set(_drop TRUE)
        endif()

        if(_drop)
            message(STATUS "Moving DepthAI transitive static dependency to final link group: ${_dep_text}")
        else()
            list(APPEND _fixed_iface_libs "${_dep}")
        endif()
    endforeach()

    set_target_properties("${target_name}" PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_fixed_iface_libs}"
    )
endfunction()

function(yolo_collect_protobuf_absl_utf8_link_libs out_var)
    set(_out)

    # Prefer raw archives from the exact vcpkg tree used by this depthai-core build.
    foreach(_glob
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed/*/lib/libprotobuf*.a"
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed/*/lib/libutf8*.a"
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed/*/lib/libabsl*.a"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed/*/lib/libprotobuf*.a"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed/*/lib/libutf8*.a"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed/*/lib/libabsl*.a"
    )
        file(GLOB _matched CONFIGURE_DEPENDS "${_glob}")
        if(_matched)
            list(APPEND _out ${_matched})
        endif()
    endforeach()

    if(_out)
        list(REMOVE_DUPLICATES _out)
        list(SORT _out)
        message(STATUS "Final protobuf/utf8/absl static link group: ${_out}")
    else()
        message(WARNING "No raw protobuf/utf8/absl archives found under depthai-core vcpkg_installed. If final link fails, inspect DEPTHAI_CORE_BUILD_DIR.")
    endif()

    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()


# depthai-core v3.6.x static build may reference DynamicCalibrationNode.cpp.o.
# The matching dcl:: symbols live in the dynamic_calibration dependency, but
# build-tree depthaiConfig.cmake can expose only a dangling
# dynamic_calibration_imported target. Collect the raw library archives and
# create that imported target when possible.
function(yolo_collect_dynamic_calibration_link_libs out_var)
    set(_out)

    foreach(_root
        "${DEPTHAI_CORE_BUILD_DIR}/_deps/dynamic_calibration-build"
        "${DEPTHAI_CORE_ROOT}/build/_deps/dynamic_calibration-build"
        "${DEPTHAI_CORE_BUILD_DIR}/vcpkg_installed"
        "${DEPTHAI_CORE_ROOT}/build/vcpkg_installed"
    )
        if(EXISTS "${_root}")
            file(GLOB_RECURSE _matched_dynamic_calibration_libs CONFIGURE_DEPENDS
                "${_root}/libdynamic*.a"
                "${_root}/libdynamic*.so"
                "${_root}/lib*dcl*.a"
                "${_root}/lib*dcl*.so"
                "${_root}/lib*calibration*.a"
                "${_root}/lib*calibration*.so"
            )
            if(_matched_dynamic_calibration_libs)
                list(APPEND _out ${_matched_dynamic_calibration_libs})
            endif()
        endif()
    endforeach()

    if(_out)
        list(REMOVE_DUPLICATES _out)
        list(SORT _out)
        message(STATUS "Dynamic calibration link libraries for DepthAI: ${_out}")

        if(NOT TARGET dynamic_calibration_imported)
            add_library(dynamic_calibration_imported INTERFACE IMPORTED)
            set_target_properties(dynamic_calibration_imported PROPERTIES
                INTERFACE_LINK_LIBRARIES "${_out}"
            )
            message(STATUS "Created compatibility target dynamic_calibration_imported")
        endif()
    else()
        message(WARNING "No dynamic_calibration libraries found. If final link fails with dcl::DynamicCalibration symbols, inspect ${DEPTHAI_CORE_BUILD_DIR}/_deps/dynamic_calibration-build.")
    endif()

    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

# 3. DepthAI (Required only for OAK RGBD target)
if(BUILD_OAK_RGBD_TARGET)
    find_package(depthai CONFIG QUIET
        PATHS
            ${DEPTHAI_CORE_BUILD_DIR}
            ${DEPTHAI_CORE_BUILD_DIR}/install
            ${DEPTHAI_CORE_BUILD_DIR}/install/lib/cmake/depthai
            ${DEPTHAI_CORE_BUILD_DIR}/lib/cmake/depthai
            ${DEPTHAI_CORE_ROOT}/build
            ${DEPTHAI_CORE_ROOT}/build/install
            ${DEPTHAI_CORE_ROOT}/build/install/lib/cmake/depthai
            ${DEPTHAI_CORE_ROOT}/build/lib/cmake/depthai
        NO_DEFAULT_PATH
    )
    if(NOT depthai_FOUND)
        find_package(depthai CONFIG REQUIRED)
    endif()

    if(TARGET depthai::core)
        set(DEPTHAI_TARGET depthai::core)
    elseif(TARGET depthai::opencv)
        set(DEPTHAI_TARGET depthai::opencv)
    else()
        message(FATAL_ERROR "DepthAI package found, but neither depthai::core nor depthai::opencv target exists.")
    endif()
    yolo_try_find_depthai_transitive_deps()
    yolo_collect_dynamic_calibration_link_libs(YOLO_DEPTHAI_DYNAMIC_CALIBRATION_LINK_LIBS)
    yolo_strip_missing_interface_targets(${DEPTHAI_TARGET})
    yolo_strip_protobuf_absl_utf8_from_interface(${DEPTHAI_TARGET})
    yolo_collect_absl_link_libs(YOLO_DEPTHAI_EXTRA_LINK_LIBS)
    yolo_collect_protobuf_absl_utf8_link_libs(YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP)

    if(YOLO_DEPTHAI_DYNAMIC_CALIBRATION_LINK_LIBS)
        list(APPEND YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP ${YOLO_DEPTHAI_DYNAMIC_CALIBRATION_LINK_LIBS})
        list(REMOVE_DUPLICATES YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP)
    endif()

    message(STATUS "Found DepthAI target: ${DEPTHAI_TARGET}")
endif()

# 4. IMSEE SDK (Required only for legacy INDEMIND target)
if(BUILD_INDEMIND_TARGET)
    set(IMSEE_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
    set(IMSEE_LIB_DIR ${PROJECT_SOURCE_DIR}/lib)

    if(WIN32)
        set(IMSEE_LIB ${IMSEE_LIB_DIR}/indemind.lib)
        set(IMSEE_DLL ${IMSEE_LIB_DIR}/indemind.dll)
    else()
        set(IMSEE_LIB ${IMSEE_LIB_DIR}/libindemind.so)
    endif()

    if(EXISTS ${IMSEE_INCLUDE_DIR}/imrsdk.h AND EXISTS ${IMSEE_LIB})
        message(STATUS "Found IMSEE SDK")
        message(STATUS "  Include: ${IMSEE_INCLUDE_DIR}")
        message(STATUS "  Library: ${IMSEE_LIB}")
    else()
        message(FATAL_ERROR "IMSEE SDK not found. Required only when BUILD_INDEMIND_TARGET=ON.")
    endif()
endif()

# DepthAI v3 public umbrella headers may include PointCloudData.hpp,
# which in turn includes <pcl/point_cloud.h>. Some build-tree
# depthai::core exports do not propagate the system PCL include path,
# so add the common Ubuntu PCL/Eigen include directories defensively.
set(YOLO_EXTRA_INCLUDE_DIRS)
foreach(_inc_dir
    /usr/include/pcl-1.15
    /usr/include/pcl-1.14
    /usr/include/pcl-1.13
    /usr/include/pcl-1.12
    /usr/include/pcl-1.11
    /usr/include/eigen3
)
    if(EXISTS "${_inc_dir}")
        list(APPEND YOLO_EXTRA_INCLUDE_DIRS "${_inc_dir}")
    endif()
endforeach()
if(YOLO_EXTRA_INCLUDE_DIRS)
    message(STATUS "Extra include dirs for DepthAI/PCL headers: ${YOLO_EXTRA_INCLUDE_DIRS}")
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/app
    ${YOLO_OPENCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_INCLUDE_DIR}
    ${YOLO_EXTRA_INCLUDE_DIRS}
)

if(BUILD_INDEMIND_TARGET)
    include_directories(${IMSEE_INCLUDE_DIR})
endif()

set(COMMON_SOURCES
    yolo_pose_detector.cpp
    pose_utils.cpp
)

set(COMMON_HEADERS
    yolo_pose_detector.h
    pose_utils.h
)

set(APP_SOURCES
    app/camera_intrinsics.cpp
    app/depth_region.cpp
    app/depth_utils.cpp
    app/perf_stats.cpp
    app/runtime_state.cpp
)

set(BUILT_TARGETS)

if(BUILD_OAK_RGBD_TARGET)
    add_executable(yolo_pose_oak_rgbd
        get_pose_oak_rgbd.cpp
        app/oak_rgbd_capture.cpp
        ${COMMON_SOURCES}
        ${COMMON_HEADERS}
        ${APP_SOURCES}
    )

    set_target_properties(yolo_pose_oak_rgbd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${YOLO_OUTPUT_DIR}
        OUTPUT_NAME "yolo_pose_oak_rgbd"
    )

    target_link_libraries(yolo_pose_oak_rgbd
        PRIVATE
        ${DEPTHAI_TARGET}
        ${ONNXRUNTIME_LIB}
    )

    if(YOLO_DEPTHAI_EXTRA_LINK_LIBS)
        if(UNIX AND NOT APPLE)
            target_link_libraries(yolo_pose_oak_rgbd PRIVATE
                -Wl,--start-group
                ${YOLO_DEPTHAI_EXTRA_LINK_LIBS}
                -Wl,--end-group
            )
        else()
            target_link_libraries(yolo_pose_oak_rgbd PRIVATE ${YOLO_DEPTHAI_EXTRA_LINK_LIBS})
        endif()
    endif()

    # Link the saved YOLO OpenCV libraries at the end of the command line.
    # This is important when depthai-core is a static build: its archive also
    # contains cv::* references, and GNU ld resolves static archives left-to-right.
    target_link_libraries(yolo_pose_oak_rgbd PRIVATE ${YOLO_OPENCV_LIBS})

    if(UNIX AND NOT APPLE)
        target_link_libraries(yolo_pose_oak_rgbd PRIVATE pthread)
    endif()

    # Must be the last direct link block. protobuf from depthai-core vcpkg has
    # many references into Abseil and utf8_range; keeping protobuf + utf8 + all
    # Abseil archives in one linker group resolves cyclic static dependencies.
    if(YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP)
        if(UNIX AND NOT APPLE)
            target_link_libraries(yolo_pose_oak_rgbd PRIVATE
                -Wl,--start-group
                ${YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP}
                -Wl,--end-group
            )
        else()
            target_link_libraries(yolo_pose_oak_rgbd PRIVATE ${YOLO_DEPTHAI_FINAL_STATIC_LINK_GROUP})
        endif()

        # Some dynamic_calibration objects also use OpenCV symbols. Repeat the
        # saved OpenCV libs after the final static group so symbols introduced
        # by late static archives can still be resolved.
        target_link_libraries(yolo_pose_oak_rgbd PRIVATE ${YOLO_OPENCV_LIBS})
    endif()

    list(APPEND BUILT_TARGETS yolo_pose_oak_rgbd)
endif()

if(BUILD_INDEMIND_TARGET)
    add_executable(yolo_pose_indemind_left
        get_pose_indemind_left.cpp
        ${COMMON_SOURCES}
        ${COMMON_HEADERS}
        ${APP_SOURCES}
    )

    set_target_properties(yolo_pose_indemind_left PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${YOLO_OUTPUT_DIR}
        OUTPUT_NAME "yolo_pose_indemind_left"
    )

    target_link_libraries(yolo_pose_indemind_left
        PRIVATE
        ${IMSEE_LIB}
        ${YOLO_OPENCV_LIBS}
        ${ONNXRUNTIME_LIB}
    )

    if(UNIX AND NOT APPLE)
        target_link_libraries(yolo_pose_indemind_left PRIVATE pthread)
    endif()

    if(UNIX)
        set_target_properties(yolo_pose_indemind_left PROPERTIES
            BUILD_RPATH "${IMSEE_LIB_DIR}"
            INSTALL_RPATH "${IMSEE_LIB_DIR}:$ORIGIN/../lib"
        )
    endif()

    list(APPEND BUILT_TARGETS yolo_pose_indemind_left)
endif()

if(BUILT_TARGETS)
    install(TARGETS ${BUILT_TARGETS} RUNTIME DESTINATION bin)
endif()

message(STATUS "=========================================")
message(STATUS "  Build Configuration Summary")
message(STATUS "=========================================")
message(STATUS "Executables:")
if(BUILD_OAK_RGBD_TARGET)
    message(STATUS "  - yolo_pose_oak_rgbd       (OAK CAM_A RGBD, default)")
endif()
if(BUILD_INDEMIND_TARGET)
    message(STATUS "  - yolo_pose_indemind_left  (legacy INDEMIND)")
endif()
message(STATUS "Dependencies:")
message(STATUS "  - OpenCV ${YOLO_OPENCV_VERSION}")
message(STATUS "  - ONNX Runtime")
if(BUILD_OAK_RGBD_TARGET)
    message(STATUS "  - DepthAI: ${DEPTHAI_TARGET}")
endif()
if(BUILD_INDEMIND_TARGET)
    message(STATUS "  - IMSEE SDK")
endif()
message(STATUS "To build OAK RGBD:")
message(STATUS "  cmake -S . -B build_oak -DCMAKE_BUILD_TYPE=Release -DBUILD_OAK_RGBD_TARGET=ON -DBUILD_INDEMIND_TARGET=OFF -DDEPTHAI_CORE_ROOT=${DEPTHAI_CORE_ROOT} -DCMAKE_PREFIX_PATH=${DEPTHAI_CORE_BUILD_DIR}")
message(STATUS "  cmake --build build_oak --parallel")
message(STATUS "To run:")
message(STATUS "  export DEPTHAI_AUTOCALIBRATION=OFF")
message(STATUS "  ${YOLO_OUTPUT_DIR}/yolo_pose_oak_rgbd models/yolov8n-pose-640.onnx")
message(STATUS "=========================================")
