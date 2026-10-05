#pragma once
#include <cstddef>
#include <CoriumAddrSpace.h>

// Register blob at FrameHandle + 64, sized per platform by CORIUM_FRAME_REG_BLOB_SIZE. RIP is stored
// because the switch jumps, never returns. Offsets must match every CoriumFrame_ContextSwitch implementation.
namespace Corium::Core::Frame::Internal {
	inline constexpr size_t kRegBlobSize = Corium::Memory::Internal::FrameRegBlobSize;

#if defined(_WIN32)
	// Windows x64 callee-saved registers.
	inline constexpr size_t kOffsetRip   = 0;
	inline constexpr size_t kOffsetRsp   = 8;
	inline constexpr size_t kOffsetRbx   = 16;
	inline constexpr size_t kOffsetRbp   = 24;
	inline constexpr size_t kOffsetRdi   = 32;
	inline constexpr size_t kOffsetRsi   = 40;
	inline constexpr size_t kOffsetR12   = 48;
	inline constexpr size_t kOffsetR13   = 56;
	inline constexpr size_t kOffsetR14   = 64;
	inline constexpr size_t kOffsetR15   = 72;
	inline constexpr size_t kOffsetMxcsr = 80;
	inline constexpr size_t kOffsetXmm6  = 96;
	inline constexpr size_t kOffsetXmm7  = 112;
	inline constexpr size_t kOffsetXmm8  = 128;
	inline constexpr size_t kOffsetXmm9  = 144;
	inline constexpr size_t kOffsetXmm10 = 160;
	inline constexpr size_t kOffsetXmm11 = 176;
	inline constexpr size_t kOffsetXmm12 = 192;
	inline constexpr size_t kOffsetXmm13 = 208;
	inline constexpr size_t kOffsetXmm14 = 224;
	inline constexpr size_t kOffsetXmm15 = 240;
	inline constexpr size_t kRegBlobUsed = 256;

	CORIUM_STATIC_ASSERT(kRegBlobUsed == kRegBlobSize, "Win64 register blob layout must exactly fill CORIUM_FRAME_REG_BLOB_SIZE");
#else
	// Linux x64 SysV callee-saved registers.
	inline constexpr size_t kOffsetRip   = 0;
	inline constexpr size_t kOffsetRsp   = 8;
	inline constexpr size_t kOffsetRbx   = 16;
	inline constexpr size_t kOffsetRbp   = 24;
	inline constexpr size_t kOffsetR12   = 32;
	inline constexpr size_t kOffsetR13   = 40;
	inline constexpr size_t kOffsetR14   = 48;
	inline constexpr size_t kOffsetR15   = 56;
	inline constexpr size_t kOffsetMxcsr = 64;
	inline constexpr size_t kRegBlobUsed = 68;

	CORIUM_STATIC_ASSERT(kRegBlobUsed <= kRegBlobSize, "SysV register blob layout must fit within CORIUM_FRAME_REG_BLOB_SIZE");
#endif

	// Saves into p_Self, resumes p_Incoming; on first dispatch p_Incoming is the trampoline's ctx.
	extern "C" void CoriumFrame_ContextSwitch(void* p_Self, void* p_Incoming);
}
