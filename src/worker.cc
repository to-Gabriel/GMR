#include <grpcpp/grpcpp.h>

#include <optional>

#include "mapreduce.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using mapreduce::Coordinator;
using mapreduce::GetTaskArgs;
using mapreduce::GetTaskReply;

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

int main(int argc, char *argv[]) {
  RunClient();

  return 0;
}
