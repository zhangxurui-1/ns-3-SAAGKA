# scratch/CI-SGC/CMakeLists.txt

# Step 1: 声明 MIRACL（仅对本 scratch 生效）
add_library(MIRACL STATIC IMPORTED)

set_target_properties(MIRACL PROPERTIES
    IMPORTED_LOCATION
        ${PROJECT_SOURCE_DIR}/contrib/MIRACL/build/libMIRACL.a
    INTERFACE_INCLUDE_DIRECTORIES
        ${PROJECT_SOURCE_DIR}/contrib/MIRACL/include
)

# Step 2: 构建你的仿真程序
build_exec(
    EXECNAME ci-sgc-simulator
    EXECNAME_PREFIX scratch_
    SOURCE_FILES
        simulator.cc
        application.cc
        vehicle.cc
        utils.cc
    LIBRARIES_TO_LINK
        ${ns3-libs}
        ${ns3-contrib-libs}
        MIRACL
    EXECUTABLE_DIRECTORY_PATH
        ${CMAKE_OUTPUT_DIRECTORY}/scratch/CI-SGC
)
