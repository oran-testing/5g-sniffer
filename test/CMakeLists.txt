set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BINARY replay-agent_test)

file(GLOB_RECURSE SNIFFER_SOURCES LIST_DIRECTORIES false ../replay-agent/*.h ../replay-agent/*.cc)
file(GLOB_RECURSE TEST_SOURCES LIST_DIRECTORIES false *.h *.cc)

set(SOURCES ${TEST_SOURCES} ${SNIFFER_SOURCES})

add_executable(${BINARY} ${TEST_SOURCES})

add_test(NAME ${BINARY} COMMAND ${BINARY})

target_link_libraries(${BINARY} fmt gtest liquid srsran_phch srsran_common volk srsran_rf srsran_phy zmq)

add_custom_command(
  TARGET ${BINARY}
  POST_BUILD
  COMMAND "${CMAKE_BINARY_DIR}/test/${BINARY}"
  COMMENT "Running unit tests"
)
