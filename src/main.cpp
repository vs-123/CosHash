#include <cstdint>
#include <iostream>
#include <sstream>
#include <vector>

#include "coshash.hpp"

std::string bytes_to_hex_string(const std::vector<uint8_t> &bytes) {
  std::stringstream ss;
  ss << std::hex;
  for (const auto &byte : bytes) {
    ss.width(2);
    ss.fill('0');
    ss << static_cast<int>(byte);
  }
  return ss.str();
}

int main() {
  std::string input = "";
  std::cout << "[INPUT] Enter input string to CosHash\n> ";
  std::getline(std::cin, input);

  std::vector<uint8_t> input_bytes(input.begin(), input.end());
  std::vector<uint8_t> hash_bytes = cos_hash(input_bytes);
  std::string hash_hex = bytes_to_hex_string(hash_bytes);

  std::cout << "[OUTPUT] Hashed: " << hash_hex << std::endl;

  return 0;
}
