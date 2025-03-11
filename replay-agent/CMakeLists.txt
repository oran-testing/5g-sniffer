set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SNIFFER_SOURCES LIST_DIRECTORIES true src/*.cc)
include_directories(hdr)
include_directories(
    ${LIQUID_INCLUDE_DIRS}
    ${ZEROMQ_INCLUDE_DIRS}
)

add_executable(replay-agent ${SNIFFER_SOURCES})
add_dependencies(replay-agent srsran_phy srsran_common srsran_rf)

target_link_libraries(replay-agent
                      fmt srsran_phy srsran_phch srsran_common
                      srsran_rf volk liquid zmq)

install(TARGETS replay-agent DESTINATION /usr/local/bin OPTIONAL)
