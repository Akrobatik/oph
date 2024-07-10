#pragma once

// C standard
#include <Windows.h>
// Redefinition guard
#include <TlHelp32.h>

// C++ standard
#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef UNICODE
#undef Process32First
#undef Process32Next
#undef PROCESSENTRY32
#undef Module32First
#undef Module32Next
#undef MODULEENTRY32
#endif  // !UNICODE

namespace oph {
class Section {
 public:
  uint64_t GetVA() const { return va_; }

  uint64_t GetRVA() const { return rva_; }

  std::span<const uint8_t> GetDump() const { return dump_; }

 private:
  friend class std::pair<const std::string, Section>;

  Section(const Section&) = delete;
  Section(Section&&) noexcept = delete;
  Section& operator=(const Section&) = delete;
  Section& operator=(Section&&) noexcept = delete;

  Section(uint64_t va, uint64_t rva, std::span<const uint8_t> dump)
      : va_(va), rva_(rva), dump_(dump) {}

  uint64_t va_;
  uint64_t rva_;
  std::span<const uint8_t> dump_;
};

class Module {
 public:
  const std::string& GetVersion() const { return version_; }

  uint64_t GetBaseAddr() const { return base_addr_; }

  std::span<const uint8_t> GetDump() const { return dump_; }

  bool Contains(const std::string& section_name) const {
    auto iter = sections_.find(section_name);
    return iter != sections_.end();
  }

  const Section& GetSection(const std::string& section_name) const {
    auto iter = sections_.find(section_name);
    if (iter == sections_.end()) {
      throw std::runtime_error(std::format("oph/memory: section that does not exit: {}", section_name));
    }
    return iter->second;
  }

 private:
  friend class std::pair<const std::string, Module>;

  Module(const Module&) = delete;
  Module(Module&&) noexcept = delete;
  Module& operator=(const Module&) = delete;
  Module& operator=(Module&&) noexcept = delete;

  Module(const std::string& version, uint64_t base_addr, std::vector<uint8_t>&& dump)
      : version_(version), base_addr_(base_addr), dump_(std::move(dump)) {
    const uint8_t* data = dump_.data();
    auto nt_header = (PIMAGE_NT_HEADERS)(data + ((PIMAGE_DOS_HEADER)data)->e_lfanew);

    size_t optional_header_offset;
    if (nt_header->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
      optional_header_offset = FIELD_OFFSET(IMAGE_NT_HEADERS64, OptionalHeader);
    } else {
      optional_header_offset = FIELD_OFFSET(IMAGE_NT_HEADERS32, OptionalHeader);
    }

    auto section_header =
        (PIMAGE_SECTION_HEADER)((uintptr_t)nt_header +
                                optional_header_offset +
                                nt_header->FileHeader.SizeOfOptionalHeader);

    for (int i = 0; i < nt_header->FileHeader.NumberOfSections; i++, section_header++) {
      const char* section_name = (const char*)section_header->Name;
      size_t section_name_size = [](const char* str) {
        size_t n = 0;
        for (; n < IMAGE_SIZEOF_SHORT_NAME && str[n] != '\0'; n++);
        return n;
      }(section_name);

      uint64_t section_rva = section_header->VirtualAddress;
      auto section_data = std::span<const uint8_t>(dump_).subspan(section_rva, section_header->Misc.VirtualSize);

      sections_.emplace(std::piecewise_construct,
                        std::forward_as_tuple(section_name, section_name_size),
                        std::forward_as_tuple(base_addr_ + section_rva, section_rva, section_data));
    }
  }

  std::string version_;
  uint64_t base_addr_;
  std::vector<uint8_t> dump_;
  std::unordered_map<std::string, Section> sections_;
};

class DumpStore {
 public:
  DumpStore() {}

  void DumpModule(const std::string& process_name, const std::vector<std::string>& module_names = {}) {
    DWORD process_id = GetProcessId(process_name);
    if (process_id == 0) {
      return;
    }

    HANDLE process_handle = OpenProcess(PROCESS_VM_READ, FALSE, process_id);
    if (process_handle != INVALID_HANDLE_VALUE) {
      HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, process_id);
      if (snapshot_handle != INVALID_HANDLE_VALUE) {
        DumpModule(process_handle, snapshot_handle, process_name);
        for (const auto& module_name : module_names) {
          DumpModule(process_handle, snapshot_handle, module_name);
        }

        CloseHandle(snapshot_handle);
      }
      CloseHandle(process_handle);
    }
  }

  bool Contains(const std::string& module_name) const {
    auto iter = modules_.find(module_name);
    return iter != modules_.end();
  }

  bool Contains(const std::string& module_name, const std::string& section_name) const {
    auto iter = modules_.find(module_name);
    return iter != modules_.end() ? iter->second.Contains(section_name) : false;
  }

  const Module& GetModule(const std::string& module_name) const {
    auto iter = modules_.find(module_name);
    if (iter == modules_.end()) {
      throw std::runtime_error(std::format("oph/memory: module that does not exit: {}", module_name));
    }
    return iter->second;
  }

  const Section& GetSection(const std::string& module_name, const std::string& section_name) const {
    auto iter = modules_.find(module_name);
    if (iter == modules_.end()) {
      throw std::runtime_error(std::format("oph/memory: module that does not exit: {}", module_name));
    }
    return iter->second.GetSection(section_name);
  }

 private:
  DumpStore(const DumpStore&) = delete;
  DumpStore(DumpStore&&) noexcept = delete;
  DumpStore& operator=(const DumpStore&) = delete;
  DumpStore& operator=(DumpStore&&) noexcept = delete;

  void DumpModule(const HANDLE process_handle, const HANDLE snapshot_handle, const std::string& module_name) {
    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32);

    if (Module32First(snapshot_handle, &me32)) {
      do {
        if (module_name.compare(me32.szModule) == 0) {
          std::vector<uint8_t> dump;
          dump.resize(me32.modBaseSize);

          if (ReadProcessMemory(process_handle, me32.modBaseAddr, dump.data(), dump.size(), NULL)) {
            modules_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(module_name),
                             std::forward_as_tuple(GetFileVersion(me32.szExePath), (uint64_t)me32.modBaseAddr, std::move(dump)));
          }

          return;
        }
      } while (Module32Next(snapshot_handle, &me32));
    }
  }

  static DWORD GetProcessId(std::string_view process_name) {
    DWORD process_id = 0;

    HANDLE snapshot_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot_handle != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 pe32;
      pe32.dwSize = sizeof(PROCESSENTRY32);

      if (Process32First(snapshot_handle, &pe32)) {
        do {
          if (process_name.compare(pe32.szExeFile) == 0) {
            process_id = pe32.th32ProcessID;
            break;
          }
        } while (Process32Next(snapshot_handle, &pe32));
      }

      CloseHandle(snapshot_handle);
    }

    return process_id;
  }

  static std::string GetFileVersion(std::string_view file_path);

  std::unordered_map<std::string, Module> modules_;
};
}  // namespace oph

#ifdef UNICODE
#define Process32First Process32FirstW
#define Process32Next Process32NextW
#define PROCESSENTRY32 PROCESSENTRY32W
#define Module32First Module32FirstW
#define Module32Next Module32NextW
#define MODULEENTRY32 MODULEENTRY32W
#endif  // !UNICODE
