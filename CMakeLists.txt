cmake_minimum_required(VERSION 3.16)
project(StringReverse C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Disable installation for fetched sub-modules
set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)

option(USE_SYSTEM_GRPC "Use system installed gRPC" OFF)

if(USE_SYSTEM_GRPC)
  find_package(gRPC CONFIG REQUIRED)
  find_package(Protobuf REQUIRED)
  # Define the variables
  set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
  set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
  set(_GRPC_GRPCPP gRPC::grpc++)
  set(_REFLECTION gRPC::grpc++_reflection)
  set(_PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
else()
  include(FetchContent)
  FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG v1.60.0 # Standard stable version
    )
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
get_filename_component(hw_proto "./protos/coordinator.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/coordinator.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/coordinator.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/coordinator.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/coordinator.grpc.pb.h")

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
    absl::log_initialize)
endforeach()
