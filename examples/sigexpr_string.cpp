#include <iostream>

#include "oph/sigexpr.hpp"

using namespace oph;

SigExpr sig1 = "Hello, world!"_sig;
SigExpr sig2 = L"This is oph"_sig;

const char str1[] = "Hello, world! welcome.";
std::span<const uint8_t> _str1((const uint8_t*)str1, sizeof(str1));

const wchar_t str2[] = L"My name is Akrobatik. This is oph.";
std::span<const uint8_t> _str2((const uint8_t*)str2, sizeof(str2));

int main() {
  std::cout << "Match: " << str1 << ": " << std::boolalpha << sig1.Match(_str1) << std::endl << std::endl;

  std::wcout << "Search: " << str2 << ": " << std::hex << sig2.Search(_str2, 1, 0) << std::endl;

  return 0;
}
