include(${CMAKE_SOURCE_DIR}/contrib/MIRACL/MIRACL.cmake)

include_directories(
    ${CMAKE_SOURCE_DIR}/contrib/MIRACL/include
    ${CMAKE_SOURCE_DIR}/contrib/MIRACL/source
    ${CMAKE_SOURCE_DIR}/contrib/MIRACL/source/curve/pairing
)

build_exec(
    EXECNAME ci-sgc-simulator
    EXECNAME_PREFIX scratch_
    SOURCE_FILES
        crypto/agka.cc
        crypto/utils.cc
        crypto/pki.cc
        # crypto/debug.cc
        message/message.cc
        message/bytewriter.cc
        message/bytereader.cc
        message/header.cc
        sgc/sgc.cc
        sgc/sgc-rsu.cc
        sgc/sgc-vehicle.cc
        metric.cc
        simulator.cc
        application.cc
        vehicle.cc
        utils.cc
    LIBRARIES_TO_LINK
        ${ns3-libs}
        ${ns3-contrib-libs}
        # MIRACL-core
        MIRACL-cpp
    EXECUTABLE_DIRECTORY_PATH
        ${CMAKE_OUTPUT_DIRECTORY}/scratch/CI-SGC
)
