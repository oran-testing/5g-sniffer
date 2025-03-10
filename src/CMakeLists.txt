set(BINARY ${CMAKE_PROJECT_NAME})

file(GLOB_RECURSE ALL_SOURCES LIST_DIRECTORIES true *.h *.cc)



set(CELL_SEARCH_SOURCES cell_search.cc args_manager.cc)
set(SNIFFER_SOURCES config.cc main.cc file_sink.cc file_source.cc sdr.cc pss.cc sss.cc common_checks.cc dsp.cc syncer.cc phy.cc sniffer.cc ofdm.cc symbol.cc channel_mapper.cc ssb_mapper.cc worker.cc pbch.cc dmrs.cc pn_sequences.cc flow.cc rotator.cc pdcch.cc dci.cc coreset.cc bandwidth_part.cc shifter.cc flow_pool.cc)

# Add the executables
add_executable(replay-agent ${SNIFFER_SOURCES})
add_dependencies(replay-agent srsran_phy srsran_common srsran_rf)

target_link_libraries(replay-agent
srsran_phy srsran_common srsran_rf spdlog::spdlog volk liquid zmq)
