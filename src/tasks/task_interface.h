#include <string>

class MapReduceTask {
public:
  virtual ~MapReduceTask() = default;
  virtual void execute(const std::string &input_file) = 0;
};
