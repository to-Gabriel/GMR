
#include <bits/stdc++.h>

#ifndef GABRIELMAPREDUCE_H
#define GABRIELMAPREDUCE_H

class GabrielMapReduce {
public:
  GabrielMapReduce(
      std::function<void(std::string key, const std::string content)> map,
      std::function<void(std::string key,
                         const std::vector<std::string> &values)>
          reduce,
      int nReduce, std::vector<std::string> &input_files);
};

#endif
