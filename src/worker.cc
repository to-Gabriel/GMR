#include <grpcpp/grpcpp.h>

#include <optional>

#include "coordinator.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using coordinator::Coordinator;
using coordinator::Worker;
using coordinator::WorkerTask;

class WorkerClient {
public:
  WorkerClient(std::shared_ptr<Channel> channel)
      : stub_(Coordinator::NewStub(channel)) {}

  std::optional<WorkerTask> requestTask(const Worker &worker) {
    WorkerTask reply;

    ClientContext context;

    Status status = stub_->requestTask(&context, worker, &reply);

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

void RunClient() {
  std::string target_address("0.0.0.0:50051");
  // Instantiates the client
  WorkerClient client(
      // Channel from which RPCs are made - endpoint is the target_address
      grpc::CreateChannel(target_address,
                          // Indicate when channel is not authenticated
                          grpc::InsecureChannelCredentials()));

  std::optional<WorkerTask> response;
  Worker worker;
  worker.set_id("1");

  response = client.requestTask(worker);

  if (response.has_value()) {
    std::cout << "Task Type: " << response->type()
              << "\tInput File: " << response->file_in() << std::endl;
  } else {
    std::cout << "Failed to retrieve task" << std::endl;
  }
}

int main(int argc, char *argv[]) {
  RunClient();

  return 0;
}
