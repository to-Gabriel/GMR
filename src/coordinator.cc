#include <grpcpp/grpcpp.h>

#include <memory>
#include <stdio.h>
#include <string>

#include "mapreduce.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapreduce::Coordinator;
using mapreduce::GetTaskArgs;
using mapreduce::GetTaskReply;

class CoordinatorServiceImplementation final : public Coordinator::Service {
  Status getTask(ServerContext *context, const GetTaskArgs *args,
                 GetTaskReply *reply) override {
    std::cout << "Assigning X task to Worker " << args->worker_id()
              << std::endl;

    std::string input_file_name =
        "input.txt"; // Only one file for basic implementation

    reply->set_task_type(mapreduce::TASK_MAP);
    reply->set_task_id(1);
    reply->set_n_reduce(3);
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:50051");
  CoordinatorServiceImplementation service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which
  // communication with client takes place
  builder.RegisterService(&service);

  // Assembling the server
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Coordinator listening on port: " << server_address << std::endl;

  server->Wait();
}

std::unique_ptr<Server> MakeCoordinator(std::vector<std::string> input_files,
                                        int nReduce) {}

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: coordinator inputfiles...\n");
    return 1;
  }

  std::vector<std::string> files{};
  for (int i = 0; i < argc; i++) {
    std::string file_name(argv[i]);
    files.push_back(file_name);
  }

  int nReduce = 3;

  std::unique_ptr<Server> coor_server = MakeCoordinator(files, nReduce);
  RunServer();
  return 0;
}
