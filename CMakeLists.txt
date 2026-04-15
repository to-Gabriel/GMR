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
  set(gRPC_INSTALL OFF CACHE BOOL "" FORCE)
  set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
  set(ABSL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
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
get_filename_component(mr_proto "./protos/mapreduce.proto" ABSOLUTE)
get_filename_component(mr_proto_path "${mr_proto}" PATH)

set(mr_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.pb.cc")
set(mr_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.pb.h")
set(mr_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.grpc.pb.cc")
set(mr_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/mapreduce.grpc.pb.h")

add_custom_command(
      OUTPUT "${mr_proto_srcs}" "${mr_proto_hdrs}" "${mr_grpc_srcs}" "${mr_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${mr_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${mr_proto}"
      DEPENDS "${mr_proto}")

  include_directories("${CMAKE_CURRENT_BINARY_DIR}")


    # gRPC C++ proto library
add_library(mr_grpc_proto
  ${mr_grpc_srcs} ${mr_grpc_hdrs}
  ${mr_proto_srcs} ${mr_proto_hdrs})

target_link_libraries(mr_grpc_proto
  PUBLIC
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

target_include_directories(mr_grpc_proto PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

# Library for tasks to keep build organized
add_library(tasks_lib
  src/tasks/map_task.cc
  src/tasks/map_task.h
  src/tasks/task_factory.h
  src/tasks/task_interface.h
)
target_link_libraries(tasks_lib PUBLIC mr_grpc_proto)
target_include_directories(tasks_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Core GMR library for clients
add_library(gabriel_map_reduce
  src/gabriel_map_reduce.h
  src/gabriel_map_reduce.cc
)
target_link_libraries(gabriel_map_reduce PUBLIC
  tasks_lib
  mr_grpc_proto)

target_compile_options(gabriel_map_reduce PUBLIC -include cstdint)

# Export inlcude paths so client_programs can find gabriel_map_reduce.h
target_include_directories(gabriel_map_reduce PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/client_programs")

# Executables
foreach(_target coordinator worker)
  add_executable(${_target} "./src/services/${_target}.cc")
  target_link_libraries(${_target}
    gabriel_map_reduce
    absl::check
    absl::flags
    absl::flags_parse
    absl::log
    absl::log_initialize)
endforeach()

# Create Executables for client client_programs
foreach(_target letter_count)
  add_executable(${_target} "./client_programs/${_target}.cc")
  target_link_libraries(${_target}
  gabriel_map_reduce)
endforeach()
