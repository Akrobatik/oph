#pragma once

// C++ standard
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Other library
#include <fmt/args.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace oph {
class Formatter {
 public:
  virtual ~Formatter() {}

  void WriteLineBreak() {
    inner_format_.append("\n");
  }

  virtual void WriteComment(std::string_view comment) = 0;
  virtual void WriteModule(std::string_view name, std::string_view version) = 0;
  virtual void WriteOffset(std::string_view name) = 0;
  virtual void WriteOffsets(std::string_view name) = 0;
  virtual void WriteBytes(std::string_view name) = 0;

  virtual std::string MakeOffset(uint64_t value) const = 0;
  virtual std::string MakeOffsets(std::span<const uint64_t> value) const = 0;
  virtual std::string MakeBytes(std::span<const uint8_t> value) const = 0;

  void Export(std::ostream& os, const std::vector<std::string>& args) {
    fmt::dynamic_format_arg_store<fmt::format_context> arg_store;
    arg_store.reserve(std::ranges::size(args), 0);
    for (const auto& arg : args) {
      arg_store.push_back(arg);
    }

    fmt::vprint(os, MakeFormat(), arg_store);
  }

 protected:
  std::string inner_format_;

 private:
  virtual std::string MakeFormat() const = 0;
};

class CppFormatter : public Formatter {
 public:
  virtual void WriteComment(std::string_view comment) {
    fmt::format_to(std::back_inserter(inner_format_), "// {}\n", comment);
  }

  virtual void WriteModule(std::string_view name, std::string_view version) {
    fmt::format_to(std::back_inserter(inner_format_), "/* {} - {} ver */\n", name, version);
  }

  virtual void WriteOffset(std::string_view name) {
    fmt::format_to(std::back_inserter(inner_format_), "constexpr uintptr_t {} = {{}};\n", name);
  }

  virtual void WriteOffsets(std::string_view name) {
    fmt::format_to(std::back_inserter(inner_format_), "constexpr uintptr_t {}[] = {{}};\n", name);
  }

  virtual void WriteBytes(std::string_view name) {
    fmt::format_to(std::back_inserter(inner_format_), "constexpr uint8_t {}[] = {{}};\n", name);
  }

  virtual std::string MakeOffset(uint64_t value) const {
    return fmt::format("0x{:X}", value);
  }

  virtual std::string MakeOffsets(std::span<const uint64_t> value) const {
    return fmt::format("{{0x{:X}}}", fmt::join(value, ", 0x"));
  }

  virtual std::string MakeBytes(std::span<const uint8_t> value) const {
    return fmt::format("{{0x{:02X}}}", fmt::join(value, ", 0x"));
  }

 private:
  virtual std::string MakeFormat() const {
    static constexpr fmt::format_string<const std::string&> cpp_format =
        "#pragma once\n"
        "\n"
        "// C++ standard\n"
        "#include <cstdint>\n"
        "\n"
        "{}";

    return fmt::format(cpp_format, inner_format_);
  }
};
}  // namespace oph
