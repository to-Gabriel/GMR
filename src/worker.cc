#include <dlfcn.h>
#include <exception>
#include <functional>
#include <grpcpp/grpcpp.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdio.h>
#include <string>
#include <system_error>

#include "grpcpp/create_channel.h"
#include "grpcpp/security/credentials.h"
#include "mapreduce.grpc.pb.h"
#include "mapreduce.pb.h"

namespace fs = std::filesystem;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mapreduce::Coordinator;
using mapreduce::GetTaskArgs;
using mapreduce::GetTaskReply;
using mapreduce::KeyValue;

// Funciton pointers for expectations on Map/Reduce functions
typedef std::vector<KeyValue> (*MapFunc)(std::string, std::string);
typedef std::string (*ReduceFunc)(std::string, std::vector<std::string>);

class WorkerClient {
public:
  WorkerClient(std::shared_ptr<Channel> channel)
      : stub_(Coordinator::NewStub(channel)) {}

  std::optional<GetTaskReply> getTask(const GetTaskArgs &args) {
    GetTaskReply reply;

    ClientContext context;

    Status status = stub_->getTask(&context, args, &reply);

    if (status.ok()) {
      return reply;
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return std::nullopt;
    }
  }

private:
  std::unique_ptr<Coordinator::Stub> stub_;
};

class Client {
public:
  // The id number for the client, incremented at call site
  static int id_;

  Client(std::string target_address, MapFunc mapf, ReduceFunc reducef)
      : mapf_(mapf), reducef_(reducef) {
    this->channel_ = std::make_unique<WorkerClient>(grpc::CreateChannel(
        target_address, grpc::InsecureChannelCredentials()));
  }

  bool Work() {
    std::optional<GetTaskReply> reply;
    GetTaskArgs args;
    args.set_worker_id(std::to_string(id_));

    reply = this->channel_->getTask(args);

    if (!reply.has_value()) {
      // RPC failure
      throw std::system_error(
          std::make_error_code(std::errc::connection_aborted),
          "Failed to retrieve reply from GetTask rpc call");
    }

    std::cout << "Starting " << reply->task_type()
              << " task with ID: " << reply->task_id()
              << "\nFor input file(s): " << std::endl;
    for (const auto &file : reply->files_to_process()) {
      std::cout << file << std::endl;
    }

    switch (reply->task_type()) {
    case mapreduce::TASK_MAP: {
      std::vector<std::string> intermediate_files = this->Map(&reply.value());
      break;
    }
    case mapreduce::TASK_REDUCE: {
      // TODO: Implement Reduce handler
      // std::string output = this->Reduce(&reply.value());
      break;
    }
    case mapreduce::TASK_WAIT: {
      sleep(5);
      break;
    }
    case mapreduce::TASK_EXIT: {
      return true;
      break;
    }
    }
    return false;
  }

  std::vector<std::string> Map(GetTaskReply *reply) {
    // Assuming one input file per map task
    std::string filePath = reply->files_to_process(0);
    std::ifstream file(filePath);

    if (!file.is_open()) {
      throw std::system_error(
          std::make_error_code(std::errc::connection_aborted),
          "Failed to open file: " + filePath);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        (std::istreambuf_iterator<char>()));

    std::vector<KeyValue> kva = this->mapf_(filePath, content);

    // Create temp files and writes to given files
    // intermediate files will total to M x N
    // where M is the amount of Map tasks and
    // N is the amount of reduce tasks (nReduce)
    std::vector<std::unique_ptr<std::ofstream>> buckets;
    buckets.reserve(reply->n_reduce());
    std::vector<std::string> intermediate_files(reply->n_reduce());

    std::map<std::string, std::string> file_name_map;

    for (int i = 0; i < reply->n_reduce(); ++i) {
      std::string file_name =
          "mr-" + std::to_string(reply->task_id()) + "-" + std::to_string(i);
      std::string temp_file_name =
          "temp-" + std::to_string(reply->task_id()) + "-" + std::to_string(i);

      fs::path temp_path = fs::current_path() / temp_file_name;

      auto file_ptr = std::make_unique<std::ofstream>(
          temp_path, std::ios::out | std::ios::trunc);

      if (!file_ptr->is_open()) {
        throw std::system_error(std::make_error_code(std::errc::io_error),
                                "Error: Could not create temp file");
      }
      buckets.push_back(std::move(file_ptr));
      file_name_map[temp_file_name] = file_name;
    }
  }

private:
  std::unique_ptr<WorkerClient> channel_;
  MapFunc mapf_;
  ReduceFunc reducef_;
};

void Worker(MapFunc mapf, ReduceFunc reducef) {
  std::string target_address("0.0.0.0:50051");
  Client client(target_address, mapf, reducef);
  Client::id_++;
  while (true) {
    try {
      bool done = client.Work();
      if (done) {
        std::cout << "Coordinator dispatched complete state" << std::endl;
        return;
      }

    } catch (std::exception e) {
      std::cout << "Error: " << e.what() << std::endl;
      return;
    }
  }
}

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  // Instantiates the client
  WorkerClient client(
      // Channel from which RPCs are made - endpoint is the target_address
      grpc::CreateChannel(target_address,
                          // Indicate when channel is not authenticated
                          grpc::InsecureChannelCredentials()));

  std::optional<GetTaskReply> response;
  GetTaskArgs args;
  args.set_worker_id("1");

  response = client.getTask(args);

  if (response.has_value()) {
    std::cout << "Task Type: " << response->task_type() << std::endl;
  } else {
    std::cout << "Failed to retrieve task" << std::endl;
  }
}

std::tuple<std::function<std::vector<KeyValue>(std::string, std::string)>,
           std::function<std::string(std::string, std::vector<std::string>)>>
loadPlugin(std::string filename) {
  // Open shared library
  void *handle = dlopen(filename.c_str(), RTLD_LAZY);
  if (!handle) {
    fprintf(stderr, "Error: %s\n", dlerror());
    exit(1);
  }

  MapFunc map_ptr = (MapFunc)dlsym(handle, "Map");
  if (!map_ptr) {
    fprintf(stderr, "Error %s\n", dlerror());
    exit(1);
  }

  ReduceFunc reduce_ptr = (ReduceFunc)dlsym(handle, "Reduce");
  if (!reduce_ptr) {
    fprintf(stderr, "Error: %s\n", dlerror());
    exit(1);
  }
  return {
      std::function<std::vector<KeyValue>(std::string, std::string)>(map_ptr),
      std::function<std::string(std::string, std::vector<std::string>)>(
          reduce_ptr)};
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cout << "Usage: worker xxx.so" << std::endl;
    return 1;
  }

  auto [mapf, reducef] = loadPlugin(argv[1]);

  Worker(mapf, reducef);

  return 0;
}
