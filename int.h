/** @file trap.h
 *
 *  @brief Important info used for INT/TRAP. 
 *         Plan to modify and re-use this file for trap
 *         in the future projects.
 *
 *  @author Yuhang Jiang(yuhangj)
 */
#ifndef _INT_H_
#define _INT_H_

/*******************************************************
 * Define Hardware IRQ numbers
 * Used for invoking relative handlers 
 *******************************************************/
/* The same as TIMER_IDT_ENTRY in 'x86/timer_defines.h' */
#define IRQ_TIMER  0x20 
/* The same as KEY_IDT_ENTRY in 'x86/keyhelp.h' */
#define IRQ_KBD    0x21

/*******************************************************
 * Segment type used for defining a gate descriptor
 *
 * Type: D = 1, D|110 = 0xE : 32-bit interrupt
 *              D|111 = 0xF : 32-bit trap  
 *******************************************************/
#define INT_32   0xE            /* 32-bit interrupt */
#define TRP_32   0xF            /* 32-bit trap */
                                   
#endif