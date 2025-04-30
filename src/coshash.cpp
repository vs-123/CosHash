#include "coshash.hpp"

#include <cstdint>
#include <vector>

const size_t BLOCK_SIZE = 64;

std::vector<uint8_t> pad_input(const std::vector<uint8_t> &input) {
  std::vector<uint8_t> padded = input;

  padded.push_back(0x80);

  // We pad with zeros until length is a multiple of BLOCK_SIZE
  while (padded.size() % BLOCK_SIZE != 0) {
    padded.push_back(0);
  }

  return padded;
}

// This simply derives the seed from the input
// [NOTE] For simplicity, we use input length and a simple sum
uint32_t derive_seed(const std::vector<uint8_t> &input) {
  uint32_t seed = static_cast<uint32_t>(input.size());
  for (size_t i = 0; i < input.size(); ++i) {
    seed ^= input[i] + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

// This is a HELPER function
// This is a simple mixing function that combines the seed and index
// It is used to initialize the state array
uint32_t initial_mix(uint32_t seed, int index) {
  // We simply mix using a combination of seed and index
  uint32_t value = seed ^ (index * 0x85ebca6b);

  // Inspired by MurmurHash
  value ^= value >> 16;
  value *= 0x85ebca6b;
  value ^= value >> 13;
  value *= 0xc2b2ae35;
  value ^= value >> 16;

  return value;
}

// This is a HELPER function
// This function derives parameters a, b, c from the block
// These parameters are used in the chaotic trigonometric mixing step
void derive_params(const std::vector<uint8_t> &block, uint32_t &a, uint32_t &b,
                   uint32_t &c) {
  // For simplicity, we derive parameters from the first 12 bytes of the block
  // If the block is not long enough, we fallback to the default values
  a = (block.size() >= 4)
          ? (block[0] << 24 | block[1] << 16 | block[2] << 8 | block[3])
          : 0x1f1f1f1f;
  b = (block.size() >= 8)
          ? (block[4] << 24 | block[5] << 16 | block[6] << 8 | block[7])
          : 0x3c3c3c3c;
  c = (block.size() >= 12)
          ? (block[8] << 24 | block[9] << 16 | block[10] << 8 | block[11])
          : 0x7a7a7a7a;
}

// This is a HELPER function
uint32_t rotate_left(uint32_t value, unsigned int shift) {
  return (value << shift) | (value >> (32 - shift));
}

// This is a HELPER function
// This function derives a permutation vector of 16 indexes from the block
std::vector<int> derive_permutation(const std::vector<uint8_t> &block) {
  // For simplicity we derive 16 indexes using a simple pseudo-random process
  std::vector<int> permutation(16);
  // We initialise the permutation array with identity permutation
  for (int i = 0; i < 16; ++i) {
    permutation[i] = i;
  }
  // We use a simple Fisher Yates shuffle seeded from the block data
  uint32_t seed = 0;
  for (size_t i = 0; i < block.size() && i < 4; ++i) {
    seed = (seed << 8) | block[i];
  }
  for (int i = 15; i > 0; --i) {
    seed = seed * 1664525 + 1013904223;
    int j = seed % (i + 1);
    std::swap(permutation[i], permutation[j]);
  }
  return permutation;
}

// This is a HELPER function
// This function compresses the state array into a 256 bit (i.e. 32 byte) hash
std::vector<uint8_t> compress_state(const std::vector<uint32_t> &state) {
  std::vector<uint8_t> hash_output;
  hash_output.reserve(16 * 4);
  // For each state word, we convert it into big endian bytes
  for (uint32_t word : state) {
    hash_output.push_back(static_cast<uint8_t>((word >> 24) & 0xFF));
    hash_output.push_back(static_cast<uint8_t>((word >> 16) & 0xFF));
    hash_output.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
    hash_output.push_back(static_cast<uint8_t>(word & 0xFF));
  }
  return hash_output;
}

// Our main CosHash function
std::vector<uint8_t> cos_hash(const std::vector<uint8_t> &input) {
  // We pad the input and split it into blocks
  std::vector<uint8_t> paddedInput = pad_input(input);
  size_t num_blocks = paddedInput.size() / BLOCK_SIZE;

  uint32_t seed = derive_seed(input);

  // We initialise the state array with 16 uint32_t values
  const int STATE_SIZE = 16;
  std::vector<uint32_t> state(STATE_SIZE);
  for (int i = 0; i < STATE_SIZE; ++i) {
    state[i] = initial_mix(seed, i);
  }

  // We process each block in the input.
  for (size_t b = 0; b < num_blocks; ++b) {
    std::vector<uint8_t> current_block(paddedInput.begin() + b * BLOCK_SIZE,
                                       paddedInput.begin() +
                                           (b + 1) * BLOCK_SIZE);

    uint32_t a, b_param, c;
    derive_params(current_block, a, b_param, c);

    // We now mix the block data with the state
    for (int i = 0; i < STATE_SIZE; ++i) {
      size_t offset = i * 4;
      uint32_t segment = 0;
      if (offset + 4 <= current_block.size()) {
        segment =
            (current_block[offset] << 24) | (current_block[offset + 1] << 16) |
            (current_block[offset + 2] << 8) | (current_block[offset + 3]);
      } else {
        // We use remaining bytes for blocks that dont have enough bytes
        for (size_t j = offset; j < current_block.size(); ++j) {
          segment = (segment << 8) | current_block[j];
        }
      }
      state[i] = (state[i] + segment) % 0x100000000; // mod 2^32
    }

    // We apply `sin` based non linearity
    for (int i = 0; i < STATE_SIZE; ++i) {
      uint32_t temp = state[i] ^ b_param;
      double sin_val = sin(a * static_cast<double>(temp));
      int32_t mix = static_cast<int32_t>(floor(sin_val * c));
      state[i] = (state[i] + static_cast<uint32_t>(mix)) % 0x100000000;
    }

    // We now permute the state array
    std::vector<int> perm = derive_permutation(current_block);
    std::vector<uint32_t> newState(STATE_SIZE);
    for (int i = 0; i < STATE_SIZE; ++i) {
      newState[i] = state[perm[i]];
    }
    state = newState;

    // We rotate and perform XOR with the neighbors
    for (int i = 0; i < STATE_SIZE; ++i) {
      uint32_t neighbor = state[(i + 1) % STATE_SIZE];
      unsigned int rot = state[i] & 0x1F;
      uint32_t rotated = rotate_left(neighbor, rot);
      state[i] = state[i] ^ rotated;
    }
  }

  // Conclusion:
  //   We now compress the state into a fixed 256 bit hash
  return compress_state(state);
}