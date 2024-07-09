#include <iostream>

#include "oph/sigexpr.hpp"

using namespace oph;

uint8_t data[] =
{
  // notepad.exe+11CD
  0x74, 0x14,                                      // je notepad.exe+11E3
  0x48, 0xFF, 0xC1,                                // inc rcx
  0x66, 0x44, 0x39, 0x1C, 0x4A,                    // cmp [rdx+rcx*2],r11w
  0x75, 0xF6,                                      // jne notepad.exe+11CF
  0x44, 0x8D, 0x0C, 0x4D, 0x02, 0x00, 0x00, 0x00,  // lea r9d,[rcx*2+00000002]
  0xEB, 0x07,                                      // jmp notepad.exe+11EA
  0x48, 0x8D, 0x15, 0x76, 0x69, 0x02, 0x00,        // lea rdx,[notepad.exe+27B60]
  0x48, 0x8B, 0x45, 0x57,                          // mov rax,[rbp+57]
};

SigExpr sig1 = "74 ? 48 FF C1 66 44 ? ? ?";
SigExpr sig2 = "00";
SigExpr sig3 = "75 ? ? 8D ? ? 02 00 00 00 EB ?";

int main() {
  // Match
  std::cout << "Match: " << std::boolalpha << sig1.Match(data) << std::endl << std::endl;

  // Search
  auto offsets = sig2.Search(data, 0x11CD);
  for (size_t i = 0; i < offsets.size(); i++) {
    std::cout << "Search[" << i << "]: " << std::hex << offsets[i] << std::endl;
  }
  std::cout << std::endl;

  // Search and Peek
  auto offset = sig3.Search(data, 1, 0, 0x11CD);
  std::cout << "Search: expected total 1, peek 0: " << std::hex << offset << std::endl << std::endl;

  // Search and Peek (error)
  try {
    sig3.Search(data, 2, 0);
  } catch(const std::exception& e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
