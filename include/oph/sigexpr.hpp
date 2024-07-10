#pragma once

// C++ standard
#include <cstdint>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace oph {
constexpr const char* kHexTable = "0123456789ABCDEF";
constexpr const char* kReverseHexTable =
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\xff\xff\xff\xff\xff\xff"
    "\xff\x0a\x0b\x0c\x0d\x0e\x0f\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\x0a\x0b\x0c\x0d\x0e\x0f\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"
    "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";

class SigExpr {
 public:
  SigExpr(const char* expr) : SigExpr(std::string_view(expr)) {}
  SigExpr(std::string_view expr)
      : elems_(Parse(expr)),
        scan_begin_(std::distance(
            elems_.begin(),
            std::ranges::find_if(elems_, [](Elem elem) { return elem.mask == 0; }))),
        scan_end_(std::distance(
            std::ranges::find_if(elems_ | std::views::reverse, [](Elem elem) { return elem.mask == 0; }),
            elems_.rend())) {}

  bool Match(std::span<const uint8_t> buffer) const {
    if (elems_.size() > buffer.size() || scan_begin_ >= scan_end_) {
      return false;
    }

    const uint8_t* ptr = buffer.data();
    for (size_t i = scan_begin_; i < scan_end_; i++) {
      if (elems_[i].mask == 0 && elems_[i].value != ptr[i]) {
        return false;
      }
    }
    return true;
  }

  uint64_t Search(std::span<const uint8_t> buffer, size_t total, size_t peek, uint64_t base_addr = 0) const {
    if (total <= peek) {
      throw std::runtime_error("oph/sigexpr: peek-index out of range");
    }

    auto result = Search(buffer, base_addr);
    if (result.size() != total) {
      throw std::runtime_error(std::format("oph/sigexpr: unexpected search result size: expected({}), result({})", total, result.size()));
    }
    return result[peek];
  }

  std::vector<uint64_t> Search(std::span<const uint8_t> buffer, uint64_t base_addr = 0) const {
    std::vector<uint64_t> result;

    if (elems_.size() > buffer.size() || scan_begin_ >= scan_end_) {
      return result;
    }

    const uint8_t* buffer_begin = buffer.data();
    const uint8_t* buffer_end = buffer_begin + buffer.size() - elems_.size() + 1;
    for (const uint8_t* ptr = buffer_begin; ptr != buffer_end; ptr++) {
      bool matched = [&]() {
        for (size_t i = scan_begin_; i < scan_end_; i++) {
          if (elems_[i].mask == 0 && elems_[i].value != ptr[i]) {
            return false;
          }
        }
        return true;
      }();

      if (matched) {
        result.push_back(base_addr + (uint64_t)(ptr - buffer_begin));
      }
    }

    return result;
  }

 private:
  struct Elem {
    uint8_t value;
    uint8_t mask;
  };

  static std::vector<Elem> Parse(std::string_view expr) {
    std::vector<Elem> elems;

    std::string_view token;
    size_t begin = 0, end;
    while ((begin = expr.find_first_not_of(' ', begin)) != std::string_view::npos) {
      end = expr.find(' ', begin);
      token = expr.substr(begin, end - begin);
      begin = end;

      if (token.compare("?") == 0 || token.compare("??") == 0) {
        elems.emplace_back(0, 1);
      } else {
        auto byte = [&]() -> std::optional<uint8_t> {
          if (token.size() != 2) {
            return std::nullopt;
          }

          uint8_t hi = kReverseHexTable[(uint8_t)token[0]];
          uint8_t lo = kReverseHexTable[(uint8_t)token[1]];
          if ((hi | lo) & 0xf0) {
            return std::nullopt;
          }
          return (hi << 4) | lo;
        }();

        if (byte.has_value()) {
          elems.emplace_back(byte.value(), 0);
        } else {
          elems.clear();
          break;
        }
      }
    }

    return elems;
  }

  const std::vector<Elem> elems_;
  const size_t scan_begin_;
  const size_t scan_end_;
};

static inline SigExpr operator""_sig(const char* str, size_t size) {
  std::string expr;
  if (size > 0) {
    expr.resize(size * 3 - 1);
    expr[0] = kHexTable[str[0] >> 4];
    expr[1] = kHexTable[str[0] & 0x0f];
    for (size_t i = 1; i < size; i++) {
      expr[i * 3 - 1] = ' ';
      expr[i * 3] = kHexTable[str[i] >> 4];
      expr[i * 3 + 1] = kHexTable[str[i] & 0x0f];
    }
  }

  return SigExpr(expr);
}

static inline SigExpr operator""_sig(const wchar_t* str, size_t size) {
  return operator""_sig((const char*)str, size * 2);
}
}  // namespace oph
