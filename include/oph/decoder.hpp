#pragma once

// C++ standard
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>

// Other library
#include <Zydis/Zydis.h>

namespace oph {
class Decoder {
 public:
  Decoder(ZydisMachineMode mode, ZydisStackWidth stack_width) {
    ZydisDecoderInit(&decoder_, mode, stack_width);
  }

  ZyanStatus DecodeInstruction(const void* buffer, ZyanISize buffer_size, ZydisDecodedInstruction* instruction) {
    return ZydisDecoderDecodeInstruction(&decoder_, nullptr, buffer, buffer_size, instruction);
  }

  ZyanStatus DecodeOperands(const ZydisDecodedInstruction* instruction, ZydisDecodedOperand* operands, ZyanU8 operand_count) {
    return ZydisDecoderDecodeOperands(&decoder_, nullptr, instruction, operands, operand_count);
  }

  ZyanStatus DecodeFull(const void* buffer, ZyanISize buffer_size, ZydisDecodedInstruction* instruction, ZydisDecodedOperand* operands) {
    return ZydisDecoderDecodeFull(&decoder_, buffer, buffer_size, instruction, operands);
  }

  std::optional<uint64_t> DecodeImmValueS(std::span<const uint8_t> buffer, ZydisMnemonic mnemonic, ZyanU8 operand_index) {
    if (buffer.begin() >= buffer.end()) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (ZYAN_FAILED(DecodeFull(buffer.data(), buffer.size(), &instruction, operands))) {
      return std::nullopt;
    }

    const ZydisDecodedOperand* operand = operands + operand_index;
    if (instruction.mnemonic != mnemonic || instruction.operand_count <= operand_index ||
        operand->type != ZYDIS_OPERAND_TYPE_IMMEDIATE || !operand->imm.is_signed) {
      return std::nullopt;
    }
    return operand->imm.value.s;
  }

  std::optional<uint64_t> DecodeImmValueU(std::span<const uint8_t> buffer, ZydisMnemonic mnemonic, ZyanU8 operand_index) {
    if (buffer.begin() >= buffer.end()) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (ZYAN_FAILED(DecodeFull(buffer.data(), buffer.size(), &instruction, operands))) {
      return std::nullopt;
    }

    const ZydisDecodedOperand* operand = operands + operand_index;
    if (instruction.mnemonic != mnemonic || instruction.operand_count <= operand_index ||
        operand->type != ZYDIS_OPERAND_TYPE_IMMEDIATE || operand->imm.is_signed) {
      return std::nullopt;
    }
    return operand->imm.value.u;
  }

  std::optional<int64_t> DecodeDispValue(std::span<const uint8_t> buffer, ZydisMnemonic mnemonic, ZyanU8 operand_index) {
    if (buffer.begin() >= buffer.end()) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (ZYAN_FAILED(DecodeFull(buffer.data(), buffer.size(), &instruction, operands))) {
      return std::nullopt;
    }

    const ZydisDecodedOperand* operand = operands + operand_index;
    if (instruction.mnemonic != mnemonic || instruction.operand_count <= operand_index ||
        operand->type != ZYDIS_OPERAND_TYPE_MEMORY || operand->mem.type == ZYDIS_MEMOP_TYPE_INVALID || !operand->mem.disp.has_displacement) {
      return std::nullopt;
    }
    return operand->mem.disp.value;
  }

  std::optional<uint64_t> CalcAbsAddr(std::span<const uint8_t> buffer, uint64_t from, ZydisMnemonic mnemonic, ZyanU8 operand_index) {
    if (buffer.begin() >= buffer.end() || buffer.size() <= from) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (ZYAN_FAILED(DecodeFull(buffer.data() + from, buffer.size() - from, &instruction, operands))) {
      return std::nullopt;
    }

    if (instruction.mnemonic != mnemonic || instruction.operand_count <= operand_index) {
      return std::nullopt;
    }

    uintptr_t to;
    if (ZYAN_FAILED(ZydisCalcAbsoluteAddress(&instruction, operands + operand_index, from, &to))) {
      return std::nullopt;
    }
    return to;
  }

  std::optional<uint64_t> CalcBackAddr(std::span<const uint8_t> buffer, uint64_t from, size_t min_bytes_size) {
    if (buffer.begin() >= buffer.end() || buffer.size() <= from) {
      return std::nullopt;
    }

    uint64_t to = from;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    while (ZYAN_SUCCESS(DecodeFull(buffer.data() + to, buffer.size() - to, &instruction, operands))) {
      to += instruction.length;
      if (to - from >= min_bytes_size) {
        return to;
      }
    }
    return std::nullopt;
  }

  std::optional<uint64_t> CalcStackFrame(std::span<const uint8_t> buffer, size_t max_instructions = 20) {
    if (buffer.begin() >= buffer.end()) {
      return std::nullopt;
    }

    ZydisRegister sp_register;
    size_t stack_width;
    if (decoder_.stack_width == ZYDIS_STACK_WIDTH_64) {
      sp_register = ZYDIS_REGISTER_RSP;
      stack_width = 8;
    } else if (decoder_.stack_width == ZYDIS_STACK_WIDTH_32) {
      sp_register = ZYDIS_REGISTER_ESP;
      stack_width = 4;
    } else {
      sp_register = ZYDIS_REGISTER_SP;
      stack_width = 2;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    uint64_t stack_size_prol = 0;
    uint64_t stack_size_epil = 0;
    uint64_t offset = 0;
    for (size_t i = 0; i < max_instructions; i++) {
      if (ZYAN_FAILED(DecodeFull(buffer.data() + offset, buffer.size() - offset, &instruction, operands))) {
        break;
      }
      offset += instruction.length;

      if (instruction.mnemonic == ZYDIS_MNEMONIC_RET) {
        return stack_size_epil;
      }

      if (instruction.mnemonic == ZYDIS_MNEMONIC_SUB &&
          instruction.operand_count == 3 &&
          operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
          operands[0].reg.value == sp_register &&
          operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        return stack_size_prol + operands[1].imm.value.s;
      }

      if (instruction.mnemonic == ZYDIS_MNEMONIC_ADD &&
          instruction.operand_count == 3 &&
          operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER &&
          operands[0].reg.value == sp_register &&
          operands[1].type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
        stack_size_epil += operands[1].imm.value.s;
      } else if (instruction.mnemonic == ZYDIS_MNEMONIC_POP) {
        stack_size_epil += stack_width;
      } else if (instruction.mnemonic == ZYDIS_MNEMONIC_PUSH) {
        stack_size_prol += stack_width;
      }
    }
    return std::nullopt;
  }

  template <typename Pred>
    requires std::is_invocable_v<Pred, const ZydisDecodedInstruction&> &&
             std::is_same_v<std::invoke_result_t<Pred, const ZydisDecodedInstruction&>, bool>
  std::optional<uint64_t> FindIf(std::span<const uint8_t> buffer, uint64_t from, Pred&& pred) {
    if (buffer.begin() >= buffer.end() || buffer.size() <= from) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    uint64_t to = from;
    while (ZYAN_SUCCESS(DecodeInstruction(buffer.data() + to, buffer.size() - to, &instruction))) {
      if (std::invoke(std::forward<Pred>(pred), instruction)) {
        return to;
      }
      to += instruction.length;
    }
    return std::nullopt;
  }

  template <typename Pred>
    requires std::is_invocable_v<Pred, const ZydisDecodedInstruction&, const ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT]> &&
             std::is_same_v<std::invoke_result_t<Pred, const ZydisDecodedInstruction&, const ZydisDecodedOperand[ZYDIS_MAX_OPERAND_COUNT]>, bool>
  std::optional<uint64_t> FindIf(std::span<const uint8_t> buffer, uint64_t from, Pred&& pred) {
    if (buffer.begin() >= buffer.end()) {
      return std::nullopt;
    }

    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    uint64_t to = from;
    while (ZYAN_SUCCESS(DecodeFull(buffer.data() + to, buffer.size() - to, &instruction, operands))) {
      if (std::invoke(std::forward<Pred>(pred), instruction, operands)) {
        return to;
      }
      to += instruction.length;
    }
    return std::nullopt;
  }

 private:
  Decoder(const Decoder&) = delete;
  Decoder(Decoder&&) noexcept = delete;
  Decoder& operator=(const Decoder&) = delete;
  Decoder& operator=(Decoder&&) noexcept = delete;

  ZydisDecoder decoder_;
};

template <ZydisMnemonic Mnemonic>
struct equal_mnemonic {
  constexpr bool operator()(const ZydisDecodedInstruction& instruction) const {
    return instruction.mnemonic == Mnemonic;
  }
};
}  // namespace oph
