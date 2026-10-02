// Windows x64 CoriumFrame_ContextSwitch for clang-cl; offsets per CoriumAsm.h.
#include "CoriumAsm.h"
#include "CoriumCompiler.h"

#if CORIUM_COMPILER_CLANG && defined(_WIN32)

// rcx = p_Self, rdx = p_Incoming.
// RSP switching is not unwindable; exceptions cannot cross the frame boundary.
extern "C" __attribute__((naked)) void CoriumFrame_ContextSwitch(void* /*p_Self*/, void* /*p_Incoming*/) {
	__asm__ volatile(
		// Save current context.
		"movq (%rsp), %rax\n\t"
		"movq %rax, 64(%rcx)\n\t"          // RIP
		"leaq 8(%rsp), %rax\n\t"
		"movq %rax, 72(%rcx)\n\t"          // RSP
		"movq %rbx, 80(%rcx)\n\t"
		"movq %rbp, 88(%rcx)\n\t"
		"movq %rdi, 96(%rcx)\n\t"
		"movq %rsi, 104(%rcx)\n\t"
		"movq %r12, 112(%rcx)\n\t"
		"movq %r13, 120(%rcx)\n\t"
		"movq %r14, 128(%rcx)\n\t"
		"movq %r15, 136(%rcx)\n\t"
		"stmxcsr 144(%rcx)\n\t"
		"movaps %xmm6,  160(%rcx)\n\t"
		"movaps %xmm7,  176(%rcx)\n\t"
		"movaps %xmm8,  192(%rcx)\n\t"
		"movaps %xmm9,  208(%rcx)\n\t"
		"movaps %xmm10, 224(%rcx)\n\t"
		"movaps %xmm11, 240(%rcx)\n\t"
		"movaps %xmm12, 256(%rcx)\n\t"
		"movaps %xmm13, 272(%rcx)\n\t"
		"movaps %xmm14, 288(%rcx)\n\t"
		"movaps %xmm15, 304(%rcx)\n\t"

		// Load incoming context.
		"ldmxcsr 144(%rdx)\n\t"
		"movaps 160(%rdx), %xmm6\n\t"
		"movaps 176(%rdx), %xmm7\n\t"
		"movaps 192(%rdx), %xmm8\n\t"
		"movaps 208(%rdx), %xmm9\n\t"
		"movaps 224(%rdx), %xmm10\n\t"
		"movaps 240(%rdx), %xmm11\n\t"
		"movaps 256(%rdx), %xmm12\n\t"
		"movaps 272(%rdx), %xmm13\n\t"
		"movaps 288(%rdx), %xmm14\n\t"
		"movaps 304(%rdx), %xmm15\n\t"
		"movq 80(%rdx),  %rbx\n\t"
		"movq 88(%rdx),  %rbp\n\t"
		"movq 96(%rdx),  %rdi\n\t"
		"movq 104(%rdx), %rsi\n\t"
		"movq 112(%rdx), %r12\n\t"
		"movq 120(%rdx), %r13\n\t"
		"movq 128(%rdx), %r14\n\t"
		"movq 136(%rdx), %r15\n\t"
		"movq 72(%rdx), %rax\n\t"          // incoming RSP
		"movq %rax, %rsp\n\t"
		"movq 64(%rdx), %rax\n\t"          // incoming RIP
		"movq %rdx, %rcx\n\t"              // Set trampoline context.
		"jmpq *%rax\n\t"
	);
}

#endif // CORIUM_COMPILER_CLANG
