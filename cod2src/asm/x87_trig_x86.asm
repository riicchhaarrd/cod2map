; x87_trig_x86.asm - x87 trig wrappers for x86
; Uses x87 hardware instructions to match original binary exactly.
;
; Assembled with: ml.exe /nologo /c x87_trig_x86.asm
; x86 cdecl

.386
.model flat, c
.code

; float x87_cosf_x86(float x)
x87_cosf_x86 PROC
    fld     DWORD PTR [esp+4]
    fcos
    ret
x87_cosf_x86 ENDP

; float x87_sinf_x86(float x)
x87_sinf_x86 PROC
    fld     DWORD PTR [esp+4]
    fsin
    ret
x87_sinf_x86 ENDP

; void x87_sincosf(float angle, float *outSin, float *outCos)
x87_sincosf PROC
    push    ebp
    mov     ebp, esp
    fld     DWORD PTR [ebp+8]
    fsincos
    mov     eax, [ebp+16]
    fstp    DWORD PTR [eax]
    mov     ecx, [ebp+12]
    fstp    DWORD PTR [ecx]
    pop     ebp
    ret
x87_sincosf ENDP

; double x87_atan2f_x86(float y, float x)
; fpatan computes atan(ST1/ST0), result in ST0
x87_atan2f_x86 PROC
    fld     DWORD PTR [esp+4]
    fld     DWORD PTR [esp+8]
    fpatan
    ret
x87_atan2f_x86 ENDP

END
