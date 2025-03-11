set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE ALL_SOURCES LIST_DIRECTORIES true *.h *.cc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CELL_SEARCH_SOURCES cell_search.cc args_manager.cc)
set(SNIFFER_SOURCES config.cc main.cc file_sink.cc file_source.cc sdr.cc pss.cc sss.cc common_checks.cc dsp.cc syncer.cc phy.cc sniffer.cc ofdm.cc symbol.cc channel_mapper.cc ssb_mapper.cc worker.cc pbch.cc dmrs.cc pn_sequences.cc flow.cc rotator.cc pdcch.cc dci.cc coreset.cc bandwidth_part.cc shifter.cc flow_pool.cc srsran_exports.cc)

# Add the executables
add_executable(replay-agent ${SNIFFER_SOURCES})
add_dependencies(replay-agent srsran_phy srsran_common srsran_rf)

include_directories(
    ${Boost_INCLUDE_DIRS}
    ${SEC_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}
    ${LIQUID_INCLUDE_DIRS}
    ${ZEROMQ_INCLUDE_DIRS}
)

link_directories(
    ${Boost_LIBRARY_DIRS}
    ${SEC_LIBRARY_DIRS}
)

target_link_libraries(replay-agent fmt srsran_phy srsran_phch srsran_common srsran_rf volk liquid zmq ${Boost_LIBRARIES})
