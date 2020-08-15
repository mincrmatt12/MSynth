#pragma once
// The synth mini-JIT
//
// This is what converts an ordered set of psuedo-instructions into actual ARM assembly stored in a std::vector<uint16_t>.

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <ranges>

#include "module.h"

namespace ms::synth::jit {
	// Pseudo instruction for the JIT.
	//
	// The Program linker first constructs a dyncfg template to setup most of the linking, then creates a stream of these pseudo instructions.
	//
	// The JIT then converts these pseudo instructions into a vector of uint16_t (the smallest instruction size on ARM thumb-2) which can then be directly 
	// jumped to.
	//
	// The JIT implements a very simple virtual machine which is specified as
	//
	// Registers:
	// 	CfgPointer: the current static configuration pointer, anywhere in memory
	// 		starts undefined
	// 	DynCfgOffset: the current offset from the dyncfg blob.
	// 		starts at 0
	//
	// Opcodes:
	// 	RunModule <proc>
	// 		Using the established CfgPointer and DynCfgOffset run the module with the procedure <proc>
	// 	LoadConfig <config_location>
	// 		Load the config_location word into the CfgPointer register
	// 		:::NOTE:::
	// 		This must _immediately_ precede a RunModule instruction
	// 	AdvanceDynConfig <offset>
	// 		Add the 16 bit offset to the DynCfgOffset register
	// 	CopyFloat <src> <target>
	// 		Copy the value at DynCfgOffset+src to DynCfgOffset+target 
	// 	LoadResult <offset>
	// 		Load the result pointer into the output specified as DynCfgOffset+offset
	//
	// All offsets are in bytes.
	struct PsuedoInstruction {
		enum : uint8_t {
			OpcodeRunModule,
			OpcodeLoadConfig,
			OpcodeAdvanceDynConfig,
			OpcodeCopyFloat,
			OpcodeLoadResult
		} opcode;

		union {
			ModuleProc  proc; // For RunModule
			int16_t     advance_or_result_offset; // For Advance or LoadResult
			struct {
				int16_t source_offset; // For CopyFloat
				int16_t target_offset;
			};
			void *      config_location; // For LoadConfig
		};
	};

	namespace insns {
		// uint32_t results are <high>, <low>

		// PROLOGUE: push callee-saved registers
		inline uint32_t push() {
			// push {r4,r5,r6,r7,r8,lr}
			return 0xe92d'41f0;
		}

		// EPILOGUE: pop callee-saved registers and return
		inline uint32_t pop() {
			// pop {r4,r5,r6,r7,r8,pc}
			return 0xe8bd'81f0;
		}

		// LOAD-IMMEDIATE: load a value from the literal pool.
		// This is _guaranteed_ to be a 16-bit instruction, and a placeholder should be
		// put into the temporary assembly if the offset is not known.
		//
		// This function takes both the address of the instruction being computed and the address
		// of the literal pool entry to load. These addresses are 2*index_in_result + result.data.
		//
		// The register to load into is passed as it's number
		uint16_t load_literal_pool(int target, uintptr_t addr_insn, uintptr_t addr_literal) {
			// Because ARM thumb mode is weird the effective PC is
			// (addr_insn + 4) word aligned
			//
			// i.e.
			//
			// (addr_insn + 4) & ~0b11
			//
			// The literal address on the other hand _isn't_ subject to this asinine stupidity
			// but we _do_ have to divide by 4 because arm likes diving by four.

			uintptr_t effective_offset = addr_literal - ((addr_insn + 4) & ~0b11u);
			effective_offset >>= 2; // encoding
			return (0b01001 << 11) /* opcode */ | (target & 0b111) << 8 /* Rt */ | effective_offset & 0xff;
		}

		uint16_t load_literal_pool_placeholder(int target) {
			return (0b101101100100'0000) | (target & 0b1111); // this is an "unpredictable instruction"
		}

		bool retrieve_literal_pool_placeholder(uint16_t insn, int &target_out) {
			if ((insn >> 4) == 0b101101100100) {
				target_out = insn & 0b1111;
				return true;
			}
			return false;
		}

		// NOTE: for r8 you have to use the word forms

		// LOAD-OFFSET: load a value from an offset from a register with 5bit offset.
		// For 12bit offset use the extended form
		//
		// The 5-bit forms prefix the value with 00 (effectively *4)
		inline uint16_t load_5bit_reg_offset(int target, int source, uint8_t offset=0) {
			return (0b01101) << 11 /* not really opcode but effectively is */ | (offset & 0b11111) << 6 |
				(source & 0b111) << 3 | (target & 0b111);
		}
		
		// LOAD-OFFSET-EXT: load a value from an offset from a register with 12bit offset.
		inline uint32_t load_12bit_reg_offset(int target, int source, uint16_t offset) {
			return (((0b1111100011010000) | source & 0b1111) << 16) | 
				((target & 0b1111) << 12) | (offset & 0b111111111111);
		}

		// STORE-OFFSET: store a value from an offset from a register with 5bit offset.
		// For 12bit offset use the extended form
		inline uint16_t store_5bit_reg_offset(int target, int source, uint8_t offset=0) {
			return (0b01100) << 11 /* not really opcode but effectively is */ | (offset & 0b11111) << 6 |
				(source & 0b111) << 3 | (target & 0b111);
		}
		
		// STORE-OFFSET-EXT: store a value from an offset from a register with 12bit offset.
		inline uint32_t store_12bit_reg_offset(int target, int source, uint16_t offset) {
			return (((0b1111100011000000) | source & 0b1111) << 16) | 
				((target & 0b1111) << 12) | (offset & 0b111111111111);
		}

		// MOV: move registers
		inline uint16_t mov(int target, int source) {
			return (0b010001100 << 7) | (((target & 0b1000) >> 3) << 7) | ((source & 0b1111) << 3) | (target & 0b111);
		}

		// MOVW: load word
		inline uint32_t movw(int target, uint16_t immediate) {
			uint8_t imm8 = immediate & 0xff;
			uint16_t i   = (immediate >> 11) & 1;
			uint16_t imm3 = (immediate >> 8) & 0b111;
			uint16_t imm4 = (immediate >> 12) & 0b1111;
			return (((0b11110 << 11) | (i << 10) | (0b100100 << 4) | imm4) << 16) | (
					  (imm3 << 12) | ((target & 0b1111) << 8) | imm8);
		}

		// ADD-REGISTER: add two registers +=
		inline uint16_t add_register(int target, int source) {
			return ((0b01000100 << 8) | ((target & 0b1000) << 3) | ((source & 0b1111) << 3) | (target & 0b111));
		}

		// ADD-IMMEDIATE-LOW: add two low registers
		inline uint16_t add_immediate_8bit(int target, uint8_t offset) {
			return (0b00110 << 11) | ((target & 0b111) << 8) | offset;
		}

		// ADD-IMMEDIATE: add immediate 12-bit
		inline uint32_t add_immediate_12bit(int target, int op2, uint16_t immediate) {
			uint8_t imm8 = immediate & 0xff;
			uint16_t i   = (immediate >> 11) & 1;
			uint16_t imm3 = (immediate >> 8) & 0b111;
			return (((0b11110 << 11) | (i << 10) | (0b100000 << 4) | (op2 & 0b1111)) << 16) | (
					  (imm3 << 12) | ((target & 0b1111) << 8) | imm8);
		}

		// BRANCH-LINK-REGISTER
		inline uint16_t blx(int reg) {
			return (0b010001111 << 7) | ((reg & 0b1111) << 3);
		}

		// AND-REGISTER: do the two-operand form of and
		inline uint16_t and_register(int target, int operand) {
			return (0b0100000000 << 6) | ((operand & 0b111) << 3) | (target & 0b111);
		}
	}

	template<typename ResultAllocator>
	void assemble(std::vector<uint16_t, ResultAllocator> &result, std::ranges::view auto instructions) {
		// Ensure the result is clear
		result.clear();

		// Setup literal pool
		std::vector<uint32_t> literalpool;
		literalpool.reserve(32);
		int distance_since_last_pool = 0;
		bool inited_r4 = false;

		auto do_literalpool = [&](){
			auto it = result.rbegin();

			result.push_back(0); // placeholder for jump
			int jumpcount = 0;
			uint16_t &jump_insn = result.back();
			if (reinterpret_cast<uintptr_t>(result.end()) & 0b11) {
				// align to word
				result.push_back(0);
				jumpcount += 1; // divided by 2
			}

			while (!literalpool.empty()) {
				uint32_t value = literalpool.back();
				literalpool.pop_back();

				// Add literal value: little-endian system so the low word goes first
				result.push_back(value & 0xffff);
				uint16_t& value_addr = result.back();
				result.push_back(value >> 16);

				// Increment jump count
				jumpcount += 2;

				// Find the instruction, it will be pointed to by it
				int target;
				while (it != result.rend() && !insns::retrieve_literal_pool_placeholder(*it, target)) {
					++it;
				}
				
				// Update the instruction with the new offset
				uint16_t &load_insn = *it;
				*it = insns::load_literal_pool(target, reinterpret_cast<uintptr_t>(&load_insn), reinterpret_cast<uintptr_t>(&value_addr));

				// Go to the next (previous) instruction
				++it;
			}
		};

		auto push_instr = [&](auto x) -> uintptr_t {
			uintptr_t ret; 
			if constexpr (std::is_same_v<decltype(x), uint16_t>) {
				result.push_back(x);
				ret = reinterpret_cast<uintptr_t>(&result.back());
				distance_since_last_pool += 2;
			}
			else {
				result.push_back(static_cast<uint16_t>(x >> 16));
				result.push_back(static_cast<uint16_t>(x));
				ret = reinterpret_cast<uintptr_t>(&result.back()) - 2;
				distance_since_last_pool += 4;
			}

			if (distance_since_last_pool > 1000 || literalpool.size() > 31) {
				do_literalpool();
			}

			return ret;
		};

		// r4 - return state
		// r5 - dyncfg pointer
		// r6,r7 - scratch
		// 
		//  r6 - copy value for CopyFloat
		//     - trampoline for RunModule
		//  r7 - offset larger than 12 bit
		//
		// r8 - output pointer

		// Create prologue
		push_instr(insns::push());
		push_instr(insns::mov(8, 0));
		push_instr(insns::mov(5, 1));

		for (const PsuedoInstruction& pins : instructions) {
			switch (pins.opcode) {
				case PsuedoInstruction::OpcodeLoadConfig:
					literalpool.push_back(reinterpret_cast<uint32_t>(pins.config_location));
					push_instr(insns::load_literal_pool_placeholder(0));
					break;
				case PsuedoInstruction::OpcodeLoadResult:
					if (pins.advance_or_result_offset <= 0b1111'1111'1111) {
						// No shift for extended form
						// Can't use 5-bit form because register 8 > 7
						push_instr(insns::store_12bit_reg_offset(8, 5, pins.advance_or_result_offset));
					}
					else {
						// No shift since it's 0
						push_instr(insns::movw(7, pins.advance_or_result_offset));
						push_instr(insns::add_register(7, 5));
						push_instr(insns::store_5bit_reg_offset(8, 7));
					}
					break;
				case PsuedoInstruction::OpcodeAdvanceDynConfig:
					if (pins.advance_or_result_offset < 256) push_instr(insns::add_immediate_8bit(5, pins.advance_or_result_offset));
					else if (pins.advance_or_result_offset <= 0b1111'1111'1111) push_instr(insns::add_immediate_12bit(5, 5, pins.advance_or_result_offset));
					else {
						push_instr(insns::movw(7, pins.advance_or_result_offset));
						push_instr(insns::add_register(5, 7));
					}
					break;
				case PsuedoInstruction::OpcodeCopyFloat:
					// LOAD TO REG 6
					if (!(pins.source_offset & 0b11) && (pins.source_offset < (256 << 2))) {
						// We can use 5-bit form
						push_instr(insns::load_5bit_reg_offset(6, 5, (pins.source_offset >> 2)));
					}
					else if (pins.source_offset <= 0b1111'1111'1111) {
						// We can use 12-bit form
						push_instr(insns::load_12bit_reg_offset(6, 5, pins.source_offset));
					}
					else {
						// Use add-form
						push_instr(insns::movw(6, pins.source_offset));
						push_instr(insns::add_register(6, 5));
						push_instr(insns::load_5bit_reg_offset(6, 6));
					}
					// STORE FROM REG 6
					if (!(pins.target_offset & 0b11) && (pins.target_offset < (256 << 2))) {
						// We can use 5-bit form
						push_instr(insns::store_5bit_reg_offset(6, 5, (pins.target_offset >> 2)));
					}
					else if (pins.target_offset <= 0b1111'1111'1111) {
						// We can use 12-bit form
						push_instr(insns::store_12bit_reg_offset(6, 5, pins.target_offset));
					}
					else {
						// Use add-form
						push_instr(insns::movw(7, pins.target_offset));
						push_instr(insns::add_register(7, 5));
						push_instr(insns::store_5bit_reg_offset(6, 7));
					}
					break;
				case PsuedoInstruction::OpcodeRunModule:
					literalpool.push_back(reinterpret_cast<uint32_t>(pins.proc) | 1); // make sure the thumb bit is set
					push_instr(insns::load_literal_pool_placeholder(6));
					push_instr(insns::mov(1, 5));
					push_instr(insns::blx(6));
					if (inited_r4) {
						push_instr(insns::and_register(4, 0));
					}
					else {
						inited_r4 = true;
						push_instr(insns::mov(4, 0));
					}
					break;
			}
		}

		do_literalpool();
	}
}
