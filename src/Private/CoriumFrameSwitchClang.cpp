// Windows x64 CoriumFrame_ContextSwitch for clang-cl; offsets per CoriumAsm.h.
// Intel syntax, line-for-line with CoriumFrameSwitch.asm, so both Windows switches read the same.
#include "CoriumAsm.h"
#include "CoriumCompiler.h"

#if CORIUM_COMPILER_CLANG && defined(_WIN32)

// rcx = p_Self, rdx = p_Incoming.
// RSP switching is not unwindable; exceptions cannot cross the frame boundary.
extern "C" __attribute__((naked)) void CoriumFrame_ContextSwitch(void* /*p_Self*/, void* /*p_Incoming*/) {
	__asm__ volatile(
		".intel_syntax noprefix\n\t"

		// Save current context.
		"mov     rax, [rsp]\n\t"                  // return address = this context's resume RIP
		"mov     [rcx+64],  rax\n\t"              // RIP
		"lea     rax, [rsp+8]\n\t"                // RSP as if we had returned
		"mov     [rcx+72],  rax\n\t"              // RSP
		"mov     [rcx+80],  rbx\n\t"
		"mov     [rcx+88],  rbp\n\t"
		"mov     [rcx+96],  rdi\n\t"
		"mov     [rcx+104], rsi\n\t"
		"mov     [rcx+112], r12\n\t"
		"mov     [rcx+120], r13\n\t"
		"mov     [rcx+128], r14\n\t"
		"mov     [rcx+136], r15\n\t"
		"stmxcsr dword ptr [rcx+144]\n\t"
		"movaps  [rcx+160], xmm6\n\t"
		"movaps  [rcx+176], xmm7\n\t"
		"movaps  [rcx+192], xmm8\n\t"
		"movaps  [rcx+208], xmm9\n\t"
		"movaps  [rcx+224], xmm10\n\t"
		"movaps  [rcx+240], xmm11\n\t"
		"movaps  [rcx+256], xmm12\n\t"
		"movaps  [rcx+272], xmm13\n\t"
		"movaps  [rcx+288], xmm14\n\t"
		"movaps  [rcx+304], xmm15\n\t"

		// TEB stack bounds + x87 control word: blob offsets 256-287, handle-relative 320-351.
		"mov     rax, qword ptr gs:[0x08]\n\t"    // StackBase
		"mov     r8,  qword ptr gs:[0x10]\n\t"    // StackLimit
		"mov     r9,  qword ptr gs:[0x1478]\n\t"  // DeallocationStack
		"mov     [rcx+320], rax\n\t"
		"mov     [rcx+328], r8\n\t"
		"mov     [rcx+336], r9\n\t"
		"fnstcw  word ptr [rcx+344]\n\t"

		// Load incoming context.
		"ldmxcsr dword ptr [rdx+144]\n\t"
		"fldcw   word ptr [rdx+344]\n\t"
		"movaps  xmm6,  [rdx+160]\n\t"
		"movaps  xmm7,  [rdx+176]\n\t"
		"movaps  xmm8,  [rdx+192]\n\t"
		"movaps  xmm9,  [rdx+208]\n\t"
		"movaps  xmm10, [rdx+224]\n\t"
		"movaps  xmm11, [rdx+240]\n\t"
		"movaps  xmm12, [rdx+256]\n\t"
		"movaps  xmm13, [rdx+272]\n\t"
		"movaps  xmm14, [rdx+288]\n\t"
		"movaps  xmm15, [rdx+304]\n\t"
		"mov     rbx, [rdx+80]\n\t"
		"mov     rbp, [rdx+88]\n\t"
		"mov     rdi, [rdx+96]\n\t"
		"mov     rsi, [rdx+104]\n\t"
		"mov     r12, [rdx+112]\n\t"
		"mov     r13, [rdx+120]\n\t"
		"mov     r14, [rdx+128]\n\t"
		"mov     r15, [rdx+136]\n\t"

		// Incoming TEB bounds, right before the stack switch.
		"mov     rax, [rdx+320]\n\t"
		"mov     r8,  [rdx+328]\n\t"
		"mov     r9,  [rdx+336]\n\t"
		"mov     qword ptr gs:[0x08],   rax\n\t"
		"mov     qword ptr gs:[0x10],   r8\n\t"
		"mov     qword ptr gs:[0x1478], r9\n\t"

		"mov     rax, [rdx+72]\n\t"               // incoming RSP
		"mov     rsp, rax\n\t"
		"mov     rax, [rdx+64]\n\t"               // incoming RIP
		"mov     rcx, rdx\n\t"                    // trampoline ctx on first dispatch
		"jmp     rax\n\t"

		".att_syntax prefix\n\t"
	);
}

#endif // CORIUM_COMPILER_CLANG
