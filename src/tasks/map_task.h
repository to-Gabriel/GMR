#pragma once
#include "task_interface.h"

#include <string>

class MapTask : public MapReduceTask {
public:
  explicit MapTask();

  void execute(const std::string &input_file) override;
};
