# Each chunk registers its tests in its own chunkN.cmake, so locked registrations never change.
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test)
file(GLOB ADS_TEST_CHUNKS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/chunk*.cmake")
list(SORT ADS_TEST_CHUNKS)
foreach(chunk ${ADS_TEST_CHUNKS})
  include(${chunk})
endforeach()
