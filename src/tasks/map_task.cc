#include "map_task.h"

#include <iostream>
#include <string>

MapTask::MapTask() {}

void MapTask::execute(const std::string &input_file) {
  std::cout << "Reading from file: " << input_file << std::endl;
}
