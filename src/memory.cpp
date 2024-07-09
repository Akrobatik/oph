#include "oph/memory.hpp"

namespace oph {
std::string DumpStore::GetFileVersion(std::string_view file_path) {
  std::string version;

  DWORD size = GetFileVersionInfoSizeA(file_path.data(), NULL);
  if (size == 0) {
    return version;
  }

  uint8_t* data = new uint8_t[size];
  if (GetFileVersionInfoA(file_path.data(), 0, size, data)) {
    VS_FIXEDFILEINFO* info;
    UINT info_size;
    if (VerQueryValueA(data, "\\", (LPVOID*)&info, &info_size)) {
      version = std::format("{}.{}.{}.{}",
                            HIWORD(info->dwFileVersionMS),
                            LOWORD(info->dwFileVersionMS),
                            HIWORD(info->dwFileVersionLS),
                            LOWORD(info->dwFileVersionLS));
    }
  }
  delete[] data;

  return version;
}
}  // namespace oph
