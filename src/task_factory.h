#include "tasks/map_task.h"

#include <memory>

std::unique_ptr<MapReduceTask> TaskFactory(int32_t type) {
  switch (type) {
  case 1:
    return std::make_unique<MapTask>();
  default:
    return nullptr;
  }
}
