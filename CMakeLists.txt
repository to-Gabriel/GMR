cmake_minimum_required(VERSION 3.16)
project(GabrielMapReduce C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)

# Disable installation for fetched sub-modules
set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)

option(USE_SYSTEM_GRPC "Use system installed gRPC" OFF)

if(USE_SYSTEM_GRPC)
  find_package(Protobuf REQUIRED)
  find_package(gRPC CONFIG REQUIRED)
  # Define the variables
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  set(_GRPC_GRPCPP gRPC::grpc++)
  set(_REFLECTION gRPC::grpc++_reflection)
  set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
else()
  include(FetchContent)

  # Fix for uintptr_t header issue
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-include> $<$<COMPILE_LANGUAGE:CXX>:cstdint>)
  endif()

  FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG v1.60.0 # Standard stable version
    )

  set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  set(ABSL_PROPAGATE_CXX_STD ON CACHE BOOL "" FORCE)

  set(FETCHCONTENT_QUIET OFF)
  FetchContent_MakeAvailable(gRPC)

  # In FetchContent mode, these targets are available directly
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
  set(_GRPC_GRPCPP grpc++)
  set(_REFLECTION grpc++_reflection)
  set(_PROTOBUF_LIBPROTOBUF libprotobuf)
endif()

# Proto file setup
get_filename_component(hw_proto "./protos/mapreduce.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.grpc.pb.h")

add_custom_command(
      OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${hw_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${hw_proto}"
      DEPENDS "${hw_proto}")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Library
add_library(hw_grpc_proto
  ${hw_grpc_srcs} ${hw_grpc_hdrs}
  ${hw_proto_srcs} ${hw_proto_hdrs})

# Compile with -fPIC flag to allow client programs plugin to be Position Independent
set_target_properties(hw_grpc_proto PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(hw_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Executables
foreach(_target coordinator worker)
  add_executable(${_target} "./src/${_target}.cc")
  target_link_libraries(${_target}
    hw_grpc_proto
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize
    ${CMAKE_DL_LIBS})
endforeach()

add_library(wc SHARED "./client_programs/word_count.cc")
target_link_libraries(wc 
  hw_grpc_proto 
  ${_GRPC_GRPCPP}
)
