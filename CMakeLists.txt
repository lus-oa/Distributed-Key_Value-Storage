cmake_minimum_required(VERSION 3.14.0)
project(dhash)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-g")

include_directories(${PROJECT_SOURCE_DIR}/include)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(Threads)

# add libs for grpc install, version 3.18.1.0
include_directories("/usr/local/include/")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(PROTO_FILES
    proto/dhash.proto
)

add_library(dhash-proto ${PROTO_FILES})

SET(DHASH_LIB_DEPS
	protobuf::libprotobuf
	protobuf::libprotobuf-lite
	protobuf::libprotoc
    gRPC::grpc
    gRPC::grpc++
    gRPC::grpc++_reflection
    dhash-proto
    Threads::Threads
    m
)

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)
protobuf_generate(TARGET dhash-proto LANGUAGE cpp)
protobuf_generate(TARGET dhash-proto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

add_executable(node src/node.cpp)
target_link_libraries(node ${DHASH_LIB_DEPS})

