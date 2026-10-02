// Linux x64 SysV CoriumFrame_ContextSwitch; offsets per CoriumAsm.h.
#include "CoriumAsm.h"
#include "CoriumCompiler.h"

#if CORIUM_COMPILER_CLANG && !defined(_WIN32)

// rdi = p_Self, rsi = p_Incoming.
// RSP switching is not unwindable; exceptions cannot cross the frame boundary.
extern "C" __attribute__((naked)) void CoriumFrame_ContextSwitch(void* /*p_Self*/, void* /*p_Incoming*/) {
	__asm__ volatile(
		// Save current context.
		"movq (%rsp), %rax\n\t"
		"movq %rax, 64(%rdi)\n\t"      // RIP
		"leaq 8(%rsp), %rax\n\t"
		"movq %rax, 72(%rdi)\n\t"      // RSP
		"movq %rbx, 80(%rdi)\n\t"
		"movq %rbp, 88(%rdi)\n\t"
		"movq %r12, 96(%rdi)\n\t"
		"movq %r13, 104(%rdi)\n\t"
		"movq %r14, 112(%rdi)\n\t"
		"movq %r15, 120(%rdi)\n\t"
		"stmxcsr 128(%rdi)\n\t"

		// Load incoming context.
		"ldmxcsr 128(%rsi)\n\t"
		"movq 80(%rsi),  %rbx\n\t"
		"movq 88(%rsi),  %rbp\n\t"
		"movq 96(%rsi),  %r12\n\t"
		"movq 104(%rsi), %r13\n\t"
		"movq 112(%rsi), %r14\n\t"
		"movq 120(%rsi), %r15\n\t"
		"movq 72(%rsi), %rax\n\t"      // incoming RSP
		"movq %rax, %rsp\n\t"
		"movq 64(%rsi), %rax\n\t"      // incoming RIP
		"movq %rsi, %rdi\n\t"          // Set trampoline context.
		"jmpq *%rax\n\t"
	);
}

#endif // CORIUM_COMPILER_CLANG && !defined(_WIN32)
