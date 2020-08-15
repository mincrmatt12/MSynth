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
	// 		This must _immediately_ preceede a RunModule instruction
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

	template<typename ResultAllocator>
	bool assemble(std::vector<uint16_t, ResultAllocator> &result, std::ranges::view auto instructions) {
	}
}
