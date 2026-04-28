
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::printf("Usage: read_in_contents input_file.txt");
  }
  std::string target_file = argv[1];
  std::ifstream ifs(target_file);

  if (!ifs.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    return 1;
  }

  std::string contents((std::istreambuf_iterator<char>(ifs)),
                       (std::istreambuf_iterator<char>()));

  std::cout << "Contents read in\n" << contents << std::endl;
  return 0;
}
