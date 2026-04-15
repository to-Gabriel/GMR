#include <grpcpp/grpcpp.h>

#include <string>

#include "mapreduce.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapreduce::Coordinator;
using mapreduce::Worker;
using mapreduce::WorkerTask;

class CoordinatorServiceImplementation final : public Coordinator::Service {
  Status requestTask(ServerContext *context, const Worker *worker,
                     WorkerTask *reply) override {
    std::cout << "Assigning X task to Worker " << worker->id() << std::endl;

    std::string input_file_name =
        "input.txt"; // Only one file for basic implementation

    reply->set_type(1);
    reply->set_file_in(input_file_name);
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

int main(int argc, char **argv) {
  RunServer();
  return 0;
}
