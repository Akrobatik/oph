#include <iostream>

#include "oph/patcher.hpp"
#include "oph/sigexpr.hpp"
#include "oph/decoder.hpp"

oph::Decoder decoder = oph::Decoder(ZYDIS_MACHINE_MODE_LEGACY_32, ZYDIS_STACK_WIDTH_32);
oph::SigExpr sig = "68 E8 03 00 00 ? FF ? ? ? ? ? 80 ? ? ? 61 75 ? 6A 02";

uint64_t ScanHookPoint(const oph::DumpStore& store) {
  const auto& sec = store.GetSection("Easy_CrackMe.exe", ".text");
  auto offset = sig.Search(sec.GetDump(), 1, 0);
  return offset + sec.GetVA() + 0x11;
}

uint64_t ScanJumpTo(const oph::DumpStore& store) {
  const auto& sec = store.GetSection("Easy_CrackMe.exe", ".text");
  auto offset = sig.Search(sec.GetDump(), 1, 0);
  offset = decoder.CalcAbsAddr(sec.GetDump(), offset + 0x11, ZYDIS_MNEMONIC_JNZ, 0).value();
  return offset + sec.GetVA();
}

int main() {
  oph::Patcher patcher(oph::Patcher::kCpp);
  patcher.AddModule("Easy_CrackMe.exe");
  patcher.WriteOffset("OFFSET_HOOK_POINT", ScanHookPoint);
  patcher.WriteOffset("OFFSET_JUMP_TO", ScanJumpTo);
  patcher.Export(std::cout);

  return 0;
}
