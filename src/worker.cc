#include <dlfcn.h>
#include <functional>
#include <grpcpp/grpcpp.h>

#include <optional>

#include "mapreduce.grpc.pb.h"

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

  return 0;
}
