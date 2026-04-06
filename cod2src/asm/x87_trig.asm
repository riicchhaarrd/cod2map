; x87_trig.asm - x87 trig wrappers for x64
; Uses x87 FPU fsin/fcos/fpatan instructions to produce bit-identical
; results to x86 /arch:IA32 compilation (x87 at _PC_53 = double precision).
;
; Assembled with: ml64.exe /nologo /c x87_trig.asm
; Win64 ABI: first double in XMM0, second in XMM1, return in XMM0

.code

; double x87_sin(double x)
x87_sin PROC
    sub     rsp, 16
    fnstcw  WORD PTR [rsp+8]
    mov     ax, WORD PTR [rsp+8]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+10], ax
    fldcw   WORD PTR [rsp+10]
    movsd   QWORD PTR [rsp], xmm0
    fld     QWORD PTR [rsp]
    fsin
    fstp    QWORD PTR [rsp]
    movsd   xmm0, QWORD PTR [rsp]
    fldcw   WORD PTR [rsp+8]
    add     rsp, 16
    ret
x87_sin ENDP

; double x87_cos(double x)
x87_cos PROC
    sub     rsp, 16
    fnstcw  WORD PTR [rsp+8]
    mov     ax, WORD PTR [rsp+8]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+10], ax
    fldcw   WORD PTR [rsp+10]
    movsd   QWORD PTR [rsp], xmm0
    fld     QWORD PTR [rsp]
    fcos
    fstp    QWORD PTR [rsp]
    movsd   xmm0, QWORD PTR [rsp]
    fldcw   WORD PTR [rsp+8]
    add     rsp, 16
    ret
x87_cos ENDP

; double x87_atan2(double y, double x)
; Win64: y in XMM0, x in XMM1
; FPATAN: replaces ST(1) with atan(ST(1)/ST(0)), pops ST(0)
; We need atan(y/x), so ST(1)=y, ST(0)=x
x87_atan2 PROC
    sub     rsp, 24
    fnstcw  WORD PTR [rsp+16]
    mov     ax, WORD PTR [rsp+16]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+18], ax
    fldcw   WORD PTR [rsp+18]
    movsd   QWORD PTR [rsp], xmm0      ; y
    movsd   QWORD PTR [rsp+8], xmm1    ; x
    fld     QWORD PTR [rsp]             ; ST0 = y
    fld     QWORD PTR [rsp+8]           ; ST0 = x, ST1 = y
    fpatan                               ; ST0 = atan(ST1/ST0) = atan(y/x)
    fstp    QWORD PTR [rsp]
    movsd   xmm0, QWORD PTR [rsp]
    fldcw   WORD PTR [rsp+16]
    add     rsp, 24
    ret
x87_atan2 ENDP

; float x87_dot3f_offset(float *a, float *b)
; Computes a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + b[3] on x87 at _PC_53.
; Returns float result via XMM0. Matches x86 /arch:IA32 precision.
; Win64 ABI: RCX = a, RDX = b (b has 4 floats: vec[0..2] + offset[3])
x87_dot3f_offset PROC
    sub     rsp, 16
    fnstcw  WORD PTR [rsp+8]
    mov     ax, WORD PTR [rsp+8]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+10], ax
    fldcw   WORD PTR [rsp+10]
    fld     DWORD PTR [rcx]
    fmul    DWORD PTR [rdx]
    fld     DWORD PTR [rcx+4]
    fmul    DWORD PTR [rdx+4]
    faddp   st(1), st
    fld     DWORD PTR [rcx+8]
    fmul    DWORD PTR [rdx+8]
    faddp   st(1), st
    fadd    DWORD PTR [rdx+12]
    fstp    DWORD PTR [rsp]
    movss   xmm0, DWORD PTR [rsp]
    fldcw   WORD PTR [rsp+8]
    add     rsp, 16
    ret
x87_dot3f_offset ENDP

; void x87_MatTransVec2(float *mat, float *in, float *out)
; Computes: out[0] = mat[9]*in[2] + mat[3]*in[0] + mat[6]*in[1]
;           out[1] = mat[10]*in[2] + mat[4]*in[0] + mat[7]*in[1]
;           out[2] = mat[11]*in[2] + mat[5]*in[0] + mat[8]*in[1]
; Uses x87 at _PC_53 to match original MSVC 2002 double-rounding.
; Win64 ABI: RCX = mat, RDX = in, R8 = out
; float x87_dmulf(double a, float b)
; Computes a*b on x87 at _PC_53, returns as float.
; Replicates x87 double-rounding: 77-bit product → 64-bit → 53-bit → 32-bit
; Win64 ABI: XMM0 = a (double), XMM1 = b (float as double)
x87_dmulf PROC
    sub     rsp, 16
    fnstcw  WORD PTR [rsp+8]
    mov     ax, WORD PTR [rsp+8]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+10], ax
    fldcw   WORD PTR [rsp+10]
    movsd   QWORD PTR [rsp], xmm0      ; store double a
    fld     QWORD PTR [rsp]             ; load as x87 extended
    cvtsd2ss xmm1, xmm1                ; double→float (b was passed as double via ABI)
    movss   DWORD PTR [rsp], xmm1      ; store float b
    fmul    DWORD PTR [rsp]             ; x87: extended * float → _PC_53 rounding
    fstp    DWORD PTR [rsp]             ; store as float
    movss   xmm0, DWORD PTR [rsp]      ; return via XMM0
    fldcw   WORD PTR [rsp+8]
    add     rsp, 16
    ret
x87_dmulf ENDP

x87_MatTransVec2 PROC
    sub     rsp, 16
    fnstcw  WORD PTR [rsp+8]
    mov     ax, WORD PTR [rsp+8]
    and     ax, 0FCFFh
    or      ax, 0200h
    mov     WORD PTR [rsp+10], ax
    fldcw   WORD PTR [rsp+10]

    ; Exact match of MSVC 2002 x86 /O2 code order from .cod:
    ; out[0] = (mat[3]*in[0] + mat[9]*in[2]) + mat[6]*in[1]
    fld     DWORD PTR [rcx+12]      ; mat[3]
    fmul    DWORD PTR [rdx]         ; * in[0]
    fld     DWORD PTR [rcx+36]      ; mat[9]
    fmul    DWORD PTR [rdx+8]       ; * in[2]
    faddp   st(1), st
    fld     DWORD PTR [rcx+24]      ; mat[6]
    fmul    DWORD PTR [rdx+4]       ; * in[1]
    faddp   st(1), st
    fstp    DWORD PTR [r8]          ; out[0]

    ; out[1] = (mat[4]*in[0] + mat[10]*in[2]) + mat[7]*in[1]
    fld     DWORD PTR [rcx+16]      ; mat[4]
    fmul    DWORD PTR [rdx]         ; * in[0]
    fld     DWORD PTR [rcx+40]      ; mat[10]
    fmul    DWORD PTR [rdx+8]       ; * in[2]
    faddp   st(1), st
    fld     DWORD PTR [rcx+28]      ; mat[7]
    fmul    DWORD PTR [rdx+4]       ; * in[1]
    faddp   st(1), st
    fstp    DWORD PTR [r8+4]        ; out[1]

    ; out[2] = (mat[5]*in[0] + mat[11]*in[2]) + mat[8]*in[1]
    fld     DWORD PTR [rcx+20]      ; mat[5]
    fmul    DWORD PTR [rdx]         ; * in[0]
    fld     DWORD PTR [rcx+44]      ; mat[11]
    fmul    DWORD PTR [rdx+8]       ; * in[2]
    faddp   st(1), st
    fld     DWORD PTR [rcx+32]      ; mat[8]
    fmul    DWORD PTR [rdx+4]       ; * in[1]
    faddp   st(1), st
    fstp    DWORD PTR [r8+8]        ; out[2]

    fldcw   WORD PTR [rsp+8]
    add     rsp, 16
    ret
x87_MatTransVec2 ENDP

END
