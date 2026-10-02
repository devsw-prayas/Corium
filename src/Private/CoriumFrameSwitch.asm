; Windows x64 CoriumFrame_ContextSwitch. rcx = p_Self, rdx = p_Incoming; offsets per CoriumAsm.h.
; RSP is loaded from memory, which .pdata unwind can't describe: exceptions can't cross a frame boundary.

.code

PUBLIC CoriumFrame_ContextSwitch

CoriumFrame_ContextSwitch PROC
    ; Save current context.
    mov     rax, [rsp]              ; return address = this context's resume RIP
    mov     [rcx+64],  rax          ; RIP
    lea     rax, [rsp+8]            ; RSP as if we had returned
    mov     [rcx+72],  rax          ; RSP
    mov     [rcx+80],  rbx
    mov     [rcx+88],  rbp
    mov     [rcx+96],  rdi
    mov     [rcx+104], rsi
    mov     [rcx+112], r12
    mov     [rcx+120], r13
    mov     [rcx+128], r14
    mov     [rcx+136], r15
    stmxcsr dword ptr [rcx+144]
    movaps  [rcx+160], xmm6
    movaps  [rcx+176], xmm7
    movaps  [rcx+192], xmm8
    movaps  [rcx+208], xmm9
    movaps  [rcx+224], xmm10
    movaps  [rcx+240], xmm11
    movaps  [rcx+256], xmm12
    movaps  [rcx+272], xmm13
    movaps  [rcx+288], xmm14
    movaps  [rcx+304], xmm15

    ; Load incoming context.
    ldmxcsr dword ptr [rdx+144]
    movaps  xmm6,  [rdx+160]
    movaps  xmm7,  [rdx+176]
    movaps  xmm8,  [rdx+192]
    movaps  xmm9,  [rdx+208]
    movaps  xmm10, [rdx+224]
    movaps  xmm11, [rdx+240]
    movaps  xmm12, [rdx+256]
    movaps  xmm13, [rdx+272]
    movaps  xmm14, [rdx+288]
    movaps  xmm15, [rdx+304]
    mov     rbx, [rdx+80]
    mov     rbp, [rdx+88]
    mov     rdi, [rdx+96]
    mov     rsi, [rdx+104]
    mov     r12, [rdx+112]
    mov     r13, [rdx+120]
    mov     r14, [rdx+128]
    mov     r15, [rdx+136]
    mov     rax, [rdx+72]           ; incoming RSP
    mov     rsp, rax
    mov     rax, [rdx+64]           ; incoming RIP
    mov     rcx, rdx                ; trampoline ctx on first dispatch
    jmp     rax
CoriumFrame_ContextSwitch ENDP

END
