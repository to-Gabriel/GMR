#include "gabriel_map_reduce.h"

GabrielMapReduce::GabrielMapReduce(
    std::function<void(std::string key, const std::string content)> map,
    std::function<void(std::string key, const std::vector<std::string> &values)>
        reduce,
    int nReduce, std::vector<std::string> &input_files) {
  std::cout << "Starting Map Reduce Operation" << std::endl;
}
