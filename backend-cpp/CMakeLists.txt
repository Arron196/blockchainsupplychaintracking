cmake_minimum_required(VERSION 3.20)
project(agri_gateway_cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

find_package(OpenSSL QUIET)
find_package(SQLite3 REQUIRED)

add_library(agri_gateway_core STATIC
    src/api/http_server.cpp
    src/blockchain/ethereum_rpc_blockchain_client.cpp
    src/blockchain/mock_blockchain_client.cpp
    src/ingest/ingest_service.cpp
    src/security/basic_signature_verifier.cpp
    src/storage/in_memory_telemetry_repository.cpp
    src/storage/sqlite_telemetry_repository.cpp
    src/transport/json_parser.cpp
    src/utils/hash_utils.cpp
)

target_include_directories(agri_gateway_core PUBLIC include)
target_link_libraries(agri_gateway_core PUBLIC SQLite::SQLite3)

if (OpenSSL_FOUND)
    target_compile_definitions(agri_gateway_core PUBLIC AGRI_USE_OPENSSL=1)
    target_link_libraries(agri_gateway_core PUBLIC OpenSSL::Crypto)
else()
    target_compile_definitions(agri_gateway_core PUBLIC AGRI_USE_OPENSSL=0)
    message(WARNING "OpenSSL not found. Falling back to non-cryptographic demo mode.")
endif()

add_executable(agri_gateway
    src/main.cpp
)

target_include_directories(agri_gateway PRIVATE include)
target_link_libraries(agri_gateway PRIVATE agri_gateway_core)

enable_testing()

add_executable(test_ingest_service tests/test_ingest_service.cpp)
target_link_libraries(test_ingest_service PRIVATE agri_gateway_core)
add_test(NAME ingest_service COMMAND test_ingest_service)

add_executable(test_json_parser tests/test_json_parser.cpp)
target_link_libraries(test_json_parser PRIVATE agri_gateway_core)
add_test(NAME json_parser COMMAND test_json_parser)

add_executable(test_sqlite_repository tests/test_sqlite_repository.cpp)
target_link_libraries(test_sqlite_repository PRIVATE agri_gateway_core)
add_test(NAME sqlite_repository COMMAND test_sqlite_repository)
