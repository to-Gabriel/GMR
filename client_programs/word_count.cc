#include <grpcpp/grpcpp.h>

#include "mapreduce.pb.h"

using mapreduce::KeyValue;

extern "C" {
std::vector<KeyValue> Map(std::string str, std::string contents) {
  // Separate into list of words
  std::stringstream ss(contents);
  std::vector<std::string> words{};
  std::string word;

  while (ss >> word) {
    words.push_back(word);
  }

  std::vector<KeyValue> kva{};
  for (const std::string word : words) {
    KeyValue kv;
    kv.set_key(word);
    kv.set_value("1");
    kva.push_back(kv);
  }
  return kva;
}

std::string Reduce(std::string key, std::vector<std::string> values) {
  return std::to_string(values.size());
}
}
