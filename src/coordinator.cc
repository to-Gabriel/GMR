#include <grpcpp/grpcpp.h>

#include <memory>
#include <stdio.h>
#include <string>
#include <thread>
#include <unordered_map>

#include "grpcpp/security/server_credentials.h"
#include "mapreduce.grpc.pb.h"
#include "mapreduce.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using mapreduce::Coordinator;
using mapreduce::GetTaskArgs;
using mapreduce::GetTaskReply;

class CoordinatorServiceImplementation final : public Coordinator::Service {
public:
  explicit CoordinatorServiceImplementation(std::vector<std::string> files,
                                            int nReduce)
      : n_reduce(nReduce) {

    std::cout << "Running Map Reduce on ";
    for (const std::string &file : files) {
      std::cout << file << std::endl;
    }

    this->phase = mapreduce::STATE_WAITING;

    // Initialize Reduce Tasks
    for (int i = 0; i < this->n_reduce; i++) {
      mapreduce::Task task;
      task.set_status(mapreduce::STATE_IDLE);
      this->reduce_tasks[i] = task;
      std::cout << "Reduce Task ID initialized to: " << i << std::endl;
    }

    // Create MapTasks
    for (int i = 0; i < files.size(); i++) {
      mapreduce::Task task;
      task.set_status(mapreduce::STATE_IDLE);
      task.add_files_to_process(files[i]);
      this->map_tasks[i] = task;
    }
  }

  void checkState() {
    for (const auto &kv : this->map_tasks) {
      if (kv.second.status() == mapreduce::STATE_IDLE ||
          kv.second.status() == mapreduce::STATE_IN_PROGRESS) {
        this->phase = mapreduce::STATE_MAP;
        return;
      }
    }

    // All map tasks have been completed
    this->phase = mapreduce::STATE_REDUCE;

    for (const auto &kv : this->reduce_tasks) {
      if (kv.second.status() == mapreduce::STATE_IDLE ||
          kv.second.status() == mapreduce::STATE_IN_PROGRESS) {
        return;
      }
    }

    // All reduce tasks have been completed
    this->phase = mapreduce::STATE_DONE;
  }

  // Convert proto int64 start time to duration in seconds
  std::chrono::seconds getElapsedSeconds(int64_t start_time) {
    std::chrono::system_clock::time_point start_time_point{
        std::chrono::seconds(start_time)};
    auto now = std::chrono::system_clock::now();

    return std::chrono::duration_cast<std::chrono::seconds>(now -
                                                            start_time_point);
  }

  bool Done() {
    std::lock_guard<std::mutex> lock(this->mu);
    this->checkState();
    return this->phase == mapreduce::STATE_DONE;
  }

  Status getTask(ServerContext *context, const GetTaskArgs *args,
                 GetTaskReply *reply) override {
    std::lock_guard<std::mutex> lock(this->mu);
    this->checkState();

    if (this->phase == mapreduce::STATE_MAP) {
      for (const auto &kv : this->map_tasks) {
        mapreduce::Task maptask = kv.second;

        auto duration = this->getElapsedSeconds(maptask.start_time());

        if (maptask.status() == mapreduce::STATE_IDLE ||
            (maptask.status() == mapreduce::STATE_IN_PROGRESS &&
             duration > std::chrono::seconds(10))) {
          reply->mutable_files_to_process()->CopyFrom(
              maptask.files_to_process());
          reply->set_task_type(mapreduce::TASK_MAP);
          reply->set_task_id(kv.first);
          reply->set_n_reduce(this->n_reduce);
          maptask.set_start_time(
              std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count());
          maptask.set_status(mapreduce::STATE_IN_PROGRESS);

          this->map_tasks[kv.first] = maptask;
          return Status::OK;
        }
      }
      // Still in the Map phase but all tasks are in progress
      reply->set_task_type(mapreduce::TASK_WAIT);

    } else if (this->phase == mapreduce::STATE_REDUCE) {
      for (const auto &kv : this->reduce_tasks) {
        mapreduce::Task reduce_task = kv.second;

        auto duration = this->getElapsedSeconds(reduce_task.start_time());

        if (reduce_task.status() == mapreduce::STATE_IDLE ||
            (reduce_task.status() == mapreduce::STATE_IN_PROGRESS &&
             duration > std::chrono::seconds(10))) {
          reply->set_task_type(mapreduce::TASK_REDUCE);
          reply->mutable_files_to_process()->CopyFrom(
              reduce_task.files_to_process());
          reply->set_task_id(kv.first);
          reply->set_n_reduce(this->n_reduce);
          reduce_task.set_start_time(
              std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count());
          reduce_task.set_status(mapreduce::STATE_IN_PROGRESS);

          this->reduce_tasks[kv.first] = reduce_task;
          return Status::OK;
        }
      }
      // In the Reduce phase but all tasks are in progress
      reply->set_task_type(mapreduce::TASK_WAIT);

    } else {
      reply->set_task_type(mapreduce::TASK_EXIT);
    }
    return Status::OK;
  }

private:
  std::mutex mu;
  mapreduce::JobState phase;
  int n_reduce;
  std::unordered_map<int, mapreduce::Task> map_tasks;
  std::unordered_map<int, mapreduce::Task> reduce_tasks;
};

// Wrapper class to manage the coordinator server
class CoordinatorServer {
public:
  CoordinatorServer(std::vector<std::string> files, int nReduce) {
    std::string server_address("0.0.0.0:50051");

    // Create Map Reduce Job
    this->service_ =
        std::make_unique<CoordinatorServiceImplementation>(files, nReduce);

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(this->service_.get());

    this->server_ = builder.BuildAndStart();
    std::cout << "Coordinator listening on port: " << server_address
              << std::endl;

    // Keep server alive in background thread
    this->server_thread_ = std::thread([this]() { this->server_->Wait(); });
  }

  bool Done() { return this->service_->Done(); }

  ~CoordinatorServer() {
    this->server_->Shutdown();
    if (this->server_thread_.joinable()) {
      this->server_thread_.join();
    }
  }

private:
  std::unique_ptr<CoordinatorServiceImplementation> service_;
  std::unique_ptr<Server> server_;
  std::thread server_thread_;
};

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: coordinator inputfiles...\n");
    return 1;
  }

  std::vector<std::string> files{};
  for (int i = 1; i < argc; i++) {
    std::string file_name(argv[i]);
    files.push_back(file_name);
  }

  int nReduce = 3;

  CoordinatorServer coor_server(files, nReduce);

  while (!coor_server.Done()) {
    sleep(1);
  }

  std::cout << "Shutting down." << std::endl;
  return 0;
}
