cmake_minimum_required (VERSION 3.9)

project (covault)
include(cmake/project-settings.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NAME "covault")
set(project_name "covault")

# assume built-in pthreads on MacOS
IF(APPLE)
	set(CMAKE_THREAD_LIBS_INIT "-lpthread")
	set(CMAKE_HAVE_THREADS_LIBRARY 1)
	set(CMAKE_USE_WIN32_THREADS_INIT 0)
	set(CMAKE_USE_PTHREADS_INIT 1)
	set(THREADS_PREFER_PTHREAD_FLAG ON)
ENDIF()

find_path(CMAKE_FOLDER NAMES cmake/emp-tool-config.cmake)
include(${CMAKE_FOLDER}/cmake/common.cmake)
#include(${CMAKE_FOLDER}/cmake/source_of_randomness.cmake)
#include(${CMAKE_FOLDER}/cmake/threading.cmake)

find_package(emp-ot REQUIRED)
include_directories(${EMP-OT_INCLUDE_DIRS})

# Installation
install(FILES cmake/covault-config.cmake DESTINATION cmake/)
install(DIRECTORY covault DESTINATION include/)

# Test cases
macro (add_test _name)
	add_executable(${_name} test/${_name}.cpp)
	target_link_libraries(${_name} ${EMP-OT_LIBRARIES})
endmacro()

# Executables
macro (add_emp_executable _name _source_name)
	add_executable(${_name} ${_source_name})
	target_link_libraries(${_name} ${EMP-OT_LIBRARIES})
	add_dependencies(${_name} rapidjson)
endmacro()

# Standard compiler warnings
# Link this 'library' to use the warnings specified in compiler-warnings.cmake
add_library(project_warnings INTERFACE)
include(cmake/compiler-warnings.cmake)
set_project_warnings(project_warnings)

# Add Keccak libraries for MACs dependencies
add_library(Keccak_f /usr/local/include/emp-tool/circuits/files/bristol_fashion/Keccak_f.txt.cpp)
target_include_directories(Keccak_f PUBLIC ./include)

# Add aes_128 libraries for path queries dependencies
add_library(aes_128 /usr/local/include/emp-tool/circuits/files/bristol_fashion/aes_128.txt.cpp)
target_include_directories(aes_128 PUBLIC ./include)

# Sanitizer options if supported by compiler
include(cmake/sanitizers.cmake)
enable_sanitizers(project_options)

# Allow for static analysis options
include(cmake/static-analyzers.cmake)

# Add external folders
include(ExternalProject)
include("${CMAKE_SOURCE_DIR}/third_party/rapidjson.cmake")

# Include libraries
include_directories(${RAPIDJSON_INCLUDE_DIR})
include_directories(include)

# Add tests
add_test (sort_test)
add_test (encounter_test)
add_test (secret_test)
add_test (sortEncounters)
add_test(hexbin_test)
add_test(verify_test)
add_test(verify_nmal_test)
add_test(multGF2_test)
add_test(ingress_mac_test)
add_test(ingress_tile_test)
add_test(macs_test)
add_test (dualex_reveal_test)
add_test (swappable_ram_test)
add_test (store_path_encounter_test)
add_test (big_shuffle_test)
add_test (flow_path_query_test)
add_test (contact_path_query_test)
add_test (intersect_query_test)

add_test(network_test)
add_test(garble_redis_test)
add_test(dualex_eq_test)
add_test(ingress_btest)
add_test(node_q2)
add_test(top_1)
add_test(node_q1)
add_test(kmac)
add_test(dp_noise)
add_test(microbm)
add_test(cross_microbm)
add_test(generate_circuits)
add_test(ag2pc_benchmark)

# Add executables
add_emp_executable(L1reducer src/L1reducer.cpp)
add_emp_executable(L2reducer src/L2reducer.cpp)
add_emp_executable(reducer_eq src/reducer_eq.cpp)
add_emp_executable(eq src/eq.cpp)
add_emp_executable(ingress src/ingress.cpp)
add_emp_executable(ingress_gv src/ingress_gv.cpp)
add_emp_executable(ingress_mv src/ingress_mv.cpp)
add_emp_executable(mapper src/mapper.cpp)
add_emp_executable(mapper_gv src/mapper_gv.cpp)
add_emp_executable(primitives src/primitives.cpp)

# Add rapidjson dependency
add_dependencies(network_test rapidjson)
add_dependencies(garble_redis_test rapidjson)
add_dependencies(ingress_mac_test rapidjson)

# Add boost asio dependency
find_package(Boost 1.58.0 REQUIRED COMPONENTS system filesystem)
target_link_libraries(node_q2 ${Boost_LIBRARIES})

# Add boost dependecies to last-stage reducer
target_link_libraries(node_q2 ${Boost_LIBRARIES} rt)
target_link_libraries(node_q1 ${Boost_LIBRARIES} rt)
target_link_libraries(reducer_eq ${Boost_LIBRARIES} rt)
target_link_libraries(microbm ${Boost_LIBRARIES} rt)
target_link_libraries(cross_microbm ${Boost_LIBRARIES} rt)

# Add hiredis dependency
find_path(HIREDIS_HEADER hiredis)
target_include_directories(ingress PUBLIC ${HIREDIS_HEADER})
target_include_directories(ingress_gv PUBLIC ${HIREDIS_HEADER})
target_include_directories(ingress_mv PUBLIC ${HIREDIS_HEADER})
target_include_directories(ingress_btest PUBLIC ${HIREDIS_HEADER})
target_include_directories(mapper PUBLIC ${HIREDIS_HEADER})
target_include_directories(mapper_gv PUBLIC ${HIREDIS_HEADER})
target_include_directories(garble_redis_test PUBLIC ${HIREDIS_HEADER})
target_include_directories(node_q2 PUBLIC ${HIREDIS_HEADER})
target_include_directories(node_q1 PUBLIC ${HIREDIS_HEADER})
target_include_directories(microbm PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
target_link_libraries(ingress ${HIREDIS_LIB})
target_link_libraries(ingress_gv ${HIREDIS_LIB})
target_link_libraries(ingress_mv ${HIREDIS_LIB})
target_link_libraries(ingress_btest ${HIREDIS_LIB})
target_link_libraries(mapper ${HIREDIS_LIB})
target_link_libraries(mapper_gv ${HIREDIS_LIB})
target_link_libraries(garble_redis_test ${HIREDIS_LIB})
target_link_libraries(node_q2 ${HIREDIS_LIB})
target_link_libraries(node_q1 ${HIREDIS_LIB})
target_link_libraries(microbm ${HIREDIS_LIB})
target_link_libraries(generate_circuits ${HIREDIS_LIB})

# Add Keccak dependencies to circuits requiring MACs
target_link_libraries(encounter_test Keccak_f)
target_link_libraries(ingress Keccak_f)
target_link_libraries(ingress_gv Keccak_f)
target_link_libraries(secret_test Keccak_f)
target_link_libraries(dualex_reveal_test Keccak_f)
target_link_libraries(swappable_ram_test Keccak_f)
target_link_libraries(store_path_encounter_test Keccak_f)
target_link_libraries(big_shuffle_test Keccak_f)
target_link_libraries(flow_path_query_test Keccak_f)
target_link_libraries(contact_path_query_test Keccak_f)
target_link_libraries(intersect_query_test Keccak_f)
target_link_libraries(kmac Keccak_f)
target_link_libraries(garble_redis_test Keccak_f)
target_link_libraries(node_q2 Keccak_f)
target_link_libraries(microbm Keccak_f)

# Add aes_128 dependencies for path queries
target_link_libraries(dualex_reveal_test aes_128)
target_link_libraries(swappable_ram_test aes_128)
target_link_libraries(store_path_encounter_test aes_128)
target_link_libraries(big_shuffle_test aes_128)
target_link_libraries(flow_path_query_test aes_128)
target_link_libraries(contact_path_query_test aes_128)
target_link_libraries(intersect_query_test aes_128)

