/** @file int.c 
 *
 *  @brief In oder to distinguish INT handler with Trap handler,
 *         all INT handler will be installed and handled in this file.
 *
 *  @author Yuhang Jiang(yuhangj)
 *  @bug No known bugs
 */
#include <x86/idt.h>
#include <x86/seg.h>
#include <x86/asm.h>
#include <x86/interrupt_defines.h>
#include <x86/timer_defines.h>
#include <x86/keyhelp.h>
#include <p1kern.h>
#include <simics.h>
#include <stdio.h>
#include "int.h"

/**
 * Structure of registers pushed before calling handler
 * 
 * Struct Regs can be modified into Trapframe in the future
 */

struct PushedRegs
{
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t old_esp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
} __attribute__ ((packed));

struct Regs
{
	struct PushedRegs regs;
	uint32_t irq_no;
	uint32_t err_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;
	uint32_t usr_esp;
	uint32_t ss;
};

/**
 * Structure of gate descriptor and pseudo-descriptor
 */
struct Gatedp
{
	unsigned low_0_15 : 16;     /* Low 16 bit of seg offset */
	unsigned selector : 16;   	/* seg selector */
	unsigned padding1 : 8;
	unsigned type : 4;			/* Trap or Int */
	unsigned padding2 : 1;
	unsigned dpl : 2;           /* DPL */
	unsigned present : 1;	    /* present bit */
	unsigned high_16_31 : 16;   /* high 16 bit of seg offset */
};

struct Pseudodp{
	uint16_t lim;
	uint32_t base;
} __attribute__ ((packed)); 

/**	@breif Setgate()
 *
 * 	Set the gate descriptor. In p1, set 32-bit INT as default
 *
 * 	@param gate[]: the gate array
 *         num: index of the gate array
 *         sel: selector needed to store in the gate descriptor
 *         (*handler): the ptr to the interrupt handler, need 
 *                    to store in the gate descriptor. 
 * 			    	  When an INT triggered, dispatch the stored 
 *  		  		  handler.
 * 		   dpl: the Descriptor Privilege Level supported for 
 *  	        protection machanism, kernel level as default
 * 
 *  @return void
 */
inline void SETGATE(struct Gatedp gate[], unsigned int num, unsigned int sel, 
	void (*handler), int dpl){
	struct Gatedp *desc;
 	desc = &(gate[num]);
 	desc -> low_0_15 = ((uint32_t)handler) & 0xffff;
 	desc -> selector = sel;
 	desc -> padding1 = 0;
 	desc -> type = INT_32;
 	desc -> padding2 = 0;
 	desc -> dpl = dpl;
 	desc -> present = 1;
 	desc -> high_16_31 = ((uint32_t)handler) >> 16;
}

/* Define the limit of idt used to load itd[] */
#define IDT_LIIMIT (IDT_ENTS*(sizeof(struct Gatedp) - 1))
/* Define the bufsize used for keyboard buf */
#define MAX_BUF_SZ 512

/* Interrupt Descriptor Table */
struct Gatedp idt[IDT_ENTS] = { {0} };

/* Timer counter */
static uint32_t timer_ticks = 0;

/* Declared in the assembly file interrupt.S */
extern void asm_timer_handler();
extern void asm_kbd_handler();

/* Helper functions of timer handler */
static void (*timer_callback)(unsigned int);
static void timer_handler(struct Regs* regs);
static void timer_install(void(*tickback)(unsigned int));

/* Helper functions of keyboard handler */
static void kbd_handler(struct Regs* regs);
static void kbd_handler(struct Regs* regs);
static void writebuf(char ch);
static uint8_t readbuf();

int readchar(void);
void int_handler(struct Regs *regs);

/*******************************************************
 * Helper functions used for initialize the idt and INTs:
 *
 * (1)load_idt()
 * (2)int_init()
 * (3)handler_install()
 * (4)int_handler
 *******************************************************/

/** @breif Load idt()
 * 
 *  load the idt address into register
 *
 *  @param void
 *  @return void
 */
void load_idt(){
 	lidt(&idt, IDT_LIIMIT);
}

/** @breif int_init()
 * 
 *  Fill the gate of an IDT entry
 *
 *  @param void
 *  @return void
 */
void int_init(){
 	SETGATE(idt, IRQ_TIMER, SEGSEL_KERNEL_CS, asm_timer_handler, 0);
 	SETGATE(idt, IRQ_KBD, SEGSEL_KERNEL_CS, asm_kbd_handler, 0);
}

/** @breif handler_install()
 * 
 *  Install the handler in three steps:
 *  (1) load idt; (2) Set idt gate; (3) install timer handler
 *
 *  @param (*tickback)(...): function needed in timer handler
 *  @return 0
 */
int handler_install(void(*tickback)(unsigned int)){
	load_idt();
	int_init();
	timer_install(tickback);
	return 0;
}

/** @breif int_handler()
 * 
 *  Called from interrupt.S. This function is an interrupt dispatch 
 *  function which can dispatch relevant different handler according 
 *  to the interrupt number pushed in interrupt.S
 *
 *  @param Regs *regs: saved registered by us for handler dispatch
 *             
 *  @return void
 */
void int_handler(struct Regs *regs){
	/* Dispatch timer handler */
	if(regs->irq_no == IRQ_TIMER){
		timer_handler(regs);
		outb(INT_CTL_PORT, INT_ACK_CURRENT);
		return;
	}/* Dispatch keyboard handler */
	else if(regs->irq_no == IRQ_KBD){
		kbd_handler(regs);
		outb(INT_CTL_PORT, INT_ACK_CURRENT);
		return;
	}/* Print out the info for setted INTs in 
	  * the IDT without relevant handlers */
	else{
		lprintf("Error: INT/TRAP[%d] with unimplemented handler\n",
			(int)regs->irq_no);
		return;
	}
}

/*******************************************************
 * Helper functions used for initialize the timer handler:
 *
 * (1)timer_init()
 * (2)timer_handler()
 * (3)timer_install()
 *******************************************************/

/** @breif timer_init()
 * 
 *  Timer handler initialization. Set timer frequency at first.
 *  Then set the timer mode and send the lower and higher bytes
 *  to the relevant port.
 *
 *  @param void 
 *             
 *  @return void
 */
 static void timer_init(){
	uint32_t period = TIMER_RATE / 100;

	outb(TIMER_MODE_IO_PORT, TIMER_SQUARE_WAVE);
	outb(TIMER_PERIOD_IO_PORT, period & 0xff);
	outb(TIMER_PERIOD_IO_PORT, (period >> 8) & 0xff);
}

/** @breif timer_handler()
 * 
 *  Increase the number of timer and call the function
 *  timer_callback we set before 
 *
 *  @param Regs* regs: saved registers from interrupt.S 
 *             
 *  @return void
 */
static void timer_handler(struct Regs* regs){
	timer_ticks++;
	if(timer_callback){
		timer_callback(timer_ticks);
	}
}

/** @breif timer_install()
 * 
 *  Initial the timer and set the timer_callback function
 *
 *  @param  (*tickback)(...): the function we pass and set
 *             
 *  @return void
 */
void timer_install(void(*tickback)(unsigned int)){
	timer_init();
	timer_callback = tickback;
}

/*******************************************************
 * Helper functions used for initialize the kbd handler:
 *
 * (1)kbd_handler()
 * (2)writebuf()
 * (3)readbuf()
 * (4)readchar()
 *******************************************************/
/* basic stucture for keyboard handler */
static char buf[MAX_BUF_SZ];
static int buf_sz = 0;

int head = 0;
int tail = 0;

/** @breif kbd_handler()
 * 
 *  Dispatched by int_handler() to deal with keyboard INT
 *
 *  @param  Regs* regs: saved registers from interrupt.S
 *             
 *  @return void
 */
static void kbd_handler(struct Regs* regs){
	writebuf(inb(KEYBOARD_PORT));
}

/** @breif writebuf()
 * 
 *  Write chars into keyboard buf which can be regared as 
 *  circular buf. So, we won't worry about buffer overflow.
 *
 *  @param  ch: the character need to write into the buf
 *             
 *  @return void
 */
static void writebuf(char ch){
	if (buf_sz < MAX_BUF_SZ){
		buf[head++] = ch;
		if(head == MAX_BUF_SZ)
			head = 0;
		buf_sz++;
	}
}

/** @breif readbuf()
 * 
 *  Read chars from keyboard buffer which can be regared as 
 *  circular buffer. So, we won't worry about the overflow.
 *
 *  @param  void
 *             
 *  @return void
 */
static uint8_t readbuf(){
	uint8_t ch = 0;
	if(buf_sz > 0){
		ch = buf[tail++];
		if(tail == MAX_BUF_SZ)
			tail = 0;
		buf_sz--;
	}
	return ch;
}

/** @breif readchar()
 * 
 *  Return the augmented characters.
 *
 *  @param  void
 *             
 *  @return voKH_GETCHAR(aug): next character in the kbd buffer;
 *          -1 : if no such character.
 */
int readchar(void){
	kh_type aug;
	int ch = (int)readbuf();
	aug = process_scancode(ch);
	if(KH_ISMAKE(aug) && KH_HASDATA(aug)){
		return KH_GETCHAR(aug);
	}
	return -1;
}

