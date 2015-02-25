#include <x86/seg.h>
#include "int.h"

.global asm_timer_handler
.global asm_kbd_handler

#need not push error code within these INTs

asm_timer_handler:
	cli
	pushl $IRQ_TIMER
	jmp _int_store_regs

asm_kbd_handler:
	cli
	pushl $IRQ_KBD
	jmp _int_store_regs

_int_store_regs:
	pusha
	movw $SEGSEL_KERNEL_DS, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	pushl %esp
	call int_handler
	popl %esp
	popa
	addl $4, %esp
	iret



