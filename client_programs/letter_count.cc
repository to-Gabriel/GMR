#include "../src/gabriel_map_reduce.h"

void MapFunc(std::string key, const std::string content) {
  std::cout << "Mapping for key: " << key << std::endl;
}

void ReduceFunc(std::string key, const std::vector<std::string> &values) {
  std::cout << "Reducing for key: " << key << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    std::cout << "Missing Arguments" << std::endl;
    return 1;
  }

  std::vector<std::string> files{};
  for (int i = 1; i < argc; i++) {
    std::string file_name(argv[i]);
    files.push_back(file_name);
  }

  auto map = MapFunc;
  auto reduce = ReduceFunc;

  GabrielMapReduce mp = GabrielMapReduce(map, reduce, 3, files);
}
