/*****************************************************************
* File: w3_userio.h
* Copyright (c) 2016 Mango Communications, all rights reseved
* Released under the WARP License
* See http://warpproject.org/license for details
*****************************************************************/

/*! \file w3_userio.h

\mainpage
This is the driver for the w3_userio core, which provides access to all the user IO resources on WARP v3 boards. These resources include
user LEDs, RF LEDs, hex displays, push buttons and a DIP switch.

This driver only implements macros for reading/writing registers in the w3_userio core hardware. Macros are also provided to read the Virtex-6 device DNA.

@version 1.03.a
@copyright (c) 2011-2016 Mango Communications, Inc. All rights reserved.<br>
Released under the WARP open source license (see http://warpproject.org/license)

*/
	/* Address map:
		HDL is coded [31:0], adopting Xilinx's convention for AXI IPIF cores
		All registers are 32-bits
			regX[31]  maps to 0x80000000 in C driver
			regX[0]   maps to 0x00000001 in C driver

	0: Control RW
		[31:30] = Reserved
		[   29] = Left hex data mode (0=user supplies bit-per-segment; 1=user supplies 4-bit hex)	0x20000000
		[   28] = Right hex data mode (0=user supplies bit-per-segment; 1=user supplies 4-bit hex)	0x10000000
	  Control source for LEDs: 0=software controlled, 1=usr_ port controlled
		[27:24] = {rfb_red rfb_green rfa_red rfa_green}	0x0F000000
		[23:16] = {leds_red leds_green}					0x00FF0000
		[15: 8] = {hexdisp_right{a b c d e f g dp}}		0x0000FF00
		[ 7: 0] = {hexdisp_left{a b c d e f g dp}}		0x000000FF !! Left/Right are swapped in user_logic.v comments
	1: Left hex display RW
		[31: 9] = reserved
		[    8] = DP (controlled directly; doesn't depend on data mode)	0x100
		[ 6: 0] = Data value ([6:4] ignored when data mode = 1)			0x03F
	2: Right hex display RW
		[31: 9] = reserved
		[    8] = DP (controlled directly; doesn't depend on data mode)	0x100
		[ 6: 0] = Data value ([6:4] ignored when data mode = 1)			0x03F
 	3: Red user LEDs RW
		[31: 4] = reserved
		[ 3: 0] = Data value (1=LED illuminated) 0xF, with 0x1 mapped to lowest LED
 	4: Green user LEDs RW
		[31: 4] = reserved
		[ 3: 0] = Data value (1=LED illuminated) 0xF, with 0x1 mapped to lowest LED
  	5: RF LEDs RW
		[31: 4] = reserved
		[    3] = rfb_red	0x8
		[    2] = rfb_green	0x4
		[    1] = rfa_red	0x2
		[    0] = rfa_green	0x1
  	6: Switch/button inputs RO
		[31: 7] = reserved
		[    6] = pb_up			0x40
		[    5] = pb_mid		0x20
		[    4] = pb_down		0x10
		[ 3: 0] = DIP switch	0x0F (with 0x1 mapped to right-most switch)

	7: PWM Gen Param: PWM period RW
		[31:29] = reserved
		[28: 0] = PWN period

	8: PWM Gen Param: PWM output deassert thresh RW
		[31:29] = reserved
		[28: 0] = PWM output deassert thresh
		
	9: PWM Gen Param: PWM ramp step RW
		[31]    = ramp enabled
		[30:20] = reserved
		[19: 0] = PWM thresh ramp step

	10: HW Output control sel RW
		[31:28] = Reserved
	  HW Control source for LEDs: 0=usr_ ports, 1=pwm gen (same ctrlSrc masks as reg0)
		[27:24] = {rfb_red rfb_green rfa_red rfa_green}	0x0F000000
		[23:16] = {leds_red leds_green}					0x00FF0000
		[15: 8] = {hexdisp_left{a b c d e f g dp}}		0x0000FF00
		[ 7: 0] = {hexdisp_right{a b c d e f g dp}}		0x000000FF

	11: FPGA DNA LSB
		[31: 0] = 32LSB of FPGA DNA

	12: FPGA DNA MSB
		[31:25] = reserved
		[24: 0] = FPGA DNA 25MSB

	13: Debug Header IO
	    [31:15] = Reserved
		[15: 0] = IOB data. Bits corresponding to inputs contain input values. Bits
		           corresponding to outputs contain output values. Bits corresponding to
				   unused pins contain would-be output values (whatever software last wrote).

   14: Debug Header IO control
		[31:16] = IOB direction control, 1 bit per pin; only C_DBG_HDR_WIDTH LSB are used
	    [15: 0] = Debug header output source select (0=sw via reg13, 1=usr_dbg_hdr_out port)
*/

#ifndef W3_USERIO_H
#define W3_USERIO_H

#include "xil_io.h"

/// @cond EXCLUDE_FROM_DOCS
// Address offset for each slave register; exclude from docs, as users never use these directly
#define W3_USERIO_USER_SLV_SPACE_OFFSET (0x00000000)
#define W3_USERIO_SLV_REG0_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000000)
#define W3_USERIO_SLV_REG1_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000004)
#define W3_USERIO_SLV_REG2_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000008)
#define W3_USERIO_SLV_REG3_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x0000000C)
#define W3_USERIO_SLV_REG4_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000010)
#define W3_USERIO_SLV_REG5_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000014)
#define W3_USERIO_SLV_REG6_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000018)
#define W3_USERIO_SLV_REG7_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x0000001C)
#define W3_USERIO_SLV_REG8_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000020)
#define W3_USERIO_SLV_REG9_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000024)
#define W3_USERIO_SLV_REG10_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000028)
#define W3_USERIO_SLV_REG11_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x0000002C)
#define W3_USERIO_SLV_REG12_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000030)
#define W3_USERIO_SLV_REG13_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000034)
#define W3_USERIO_SLV_REG14_OFFSET (W3_USERIO_USER_SLV_SPACE_OFFSET + 0x00000038)
/// @endcond

/** \defgroup control_reg Control registers
 *	\addtogroup control_reg
<b>Hardware vs. software control</b>:
Every LED and hex display segment can be controlled either via software or hardware:
<ul>
<li><b>Software</b>: user code sets LED state by writing a 1 to the corresponding register bit
<li><b>Hardware</b>: Two modes:<ul>
   <li><b>Port mode</b>: LED state is controlled by corresponding usr_* port
   <li><b>PWM mode</b>:  LED state is controlled by internal PWM waveform generator
   </ul>
</ul>

The WARP reference designs use hardware/port control for the RF LEDs (to indicate real-time Tx/Rx state of each RF interface) and software control
for all other LED/hex display outputs.

The control source (hw or sw) for each output bit is set by the control register described below.

Examples:
\code{.c}
//Assumes user code sets USERIO_BASEADDR to base address of w3_userio core, as set in xparameters.h

//------------------------------------------------------------------------------
// Set both hex dipslays to map 4-bit to 7-segment values automatically
userio_write_control(USERIO_BASEADDR, (W3_USERIO_HEXDISP_L_MAPMODE | W3_USERIO_HEXDISP_R_MAPMODE));

//------------------------------------------------------------------------------
// Select software control of all outputs
userio_set_ctrlSrc_sw(USERIO_BASEADDR, (W3_USERIO_CTRLSRC_LEDS_RF | W3_USERIO_CTRLSRC_LEDS | W3_USERIO_CTRLSRC_HEXDISPS));

//------------------------------------------------------------------------------
// Select hardware/port control of RF LEDs
userio_set_ctrlSrc_hw(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_RF);

//------------------------------------------------------------------------------
// Enable hardware control of green user LEDs, software control of red user LEDs
userio_set_ctrlSrc_hw(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_GREEN);
userio_set_ctrlSrc_sw(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_RED);

//------------------------------------------------------------------------------
// Use the PWM generator to slowly blink the green LEDs
userio_set_ctrlSrc_hw(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_GREEN);
userio_set_hw_ctrl_mode_pwm(USERIO_BASEADDR, (W3_USERIO_CTRLSRC_LEDS_GREEN);
userio_set_pwm_ramp_en(USERIO_BASEADDR, 0);
userio_set_pwm_period(USERIO_BASEADDR, 65530);
userio_set_pwm_thresh(USERIO_BASEADDR, 65530/2);

//------------------------------------------------------------------------------
// Use the PWM generator to show a "sleep" pattern on red LEDs
userio_set_ctrlSrc_hw(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_RED);
userio_set_hw_ctrl_mode_pwm(USERIO_BASEADDR, W3_USERIO_CTRLSRC_LEDS_RED);

//Use fast period so blinking is not visable
// (fast blink with low duty cycle looks like a dim constant brightness)
userio_set_pwm_period(USERIO_BASEADDR, 500);

//Ramp must be disabled when changing ramp params
userio_set_pwm_ramp_en(USERIO_BASEADDR, 0);
userio_set_pwm_ramp_min(USERIO_BASEADDR, 2);
userio_set_pwm_ramp_max(USERIO_BASEADDR, 250);
userio_set_pwm_ramp_en(USERIO_BASEADDR, 1);
\endcode

 * @{
 */

#define userio_read_control(baseaddr)		Xil_In32(baseaddr+W3_USERIO_SLV_REG0_OFFSET) //!< Returns the value of the control register
#define userio_write_control(baseaddr, x)		Xil_Out32(baseaddr+W3_USERIO_SLV_REG0_OFFSET, x) //!< Sets the control register to x

#define userio_set_ctrlSrc_sw(baseaddr, ioMask) Xil_Out32(baseaddr+W3_USERIO_SLV_REG0_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG0_OFFSET) & ~(ioMask)))//!< Sets selected outputs to software control (register writes)
#define userio_set_ctrlSrc_hw(baseaddr, ioMask) Xil_Out32(baseaddr+W3_USERIO_SLV_REG0_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG0_OFFSET) | (ioMask))) //!< Sets selected outputs to hardware control (usr_ ports)

#define userio_set_hw_ctrl_mode_pwm(baseaddr, ioMask) Xil_Out32(baseaddr+W3_USERIO_SLV_REG10_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG10_OFFSET) | (ioMask))) //!< Sets selected outputs to use PWM generator for hardware/PWM control
#define userio_set_hw_ctrl_mode_port(baseaddr, ioMask) Xil_Out32(baseaddr+W3_USERIO_SLV_REG10_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG10_OFFSET) & (~ioMask))) //!< Sets selected outputs to use PWM generator for hardware/port control

//PWM config macros
#define userio_set_pwm_period(baseaddr, p) Xil_Out32(baseaddr+W3_USERIO_SLV_REG7_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG7_OFFSET) & 0x0000FFFF) | (((p) & 0xFFFF)<<16)) //!< Sets the PWM period; larger periods result in slower blinking
#define userio_set_pwm_thresh(baseaddr, t) Xil_Out32(baseaddr+W3_USERIO_SLV_REG7_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG7_OFFSET) & 0xFFFF0000) | ((t) & 0xFFFF)) //!< Sets the PWM duty cycle threshold; threshold be greater than 1 and less than the PWM period. This threshold is ignored when the threshold ramp is enabled
#define userio_set_pwm_ramp_en(baseaddr, d) Xil_Out32(baseaddr+W3_USERIO_SLV_REG9_OFFSET, ( (Xil_In32(baseaddr+W3_USERIO_SLV_REG9_OFFSET) & 0x7FFFFFFF) | ((d&0x1)<<31))) //!< Enables and disables the PWM threshold ramp logic. Ramp must be disabled when changing ramp min/max params
#define userio_set_pwm_ramp_max(baseaddr, m) Xil_Out32(baseaddr+W3_USERIO_SLV_REG9_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG9_OFFSET) & 0xFFFF0000) | ((m) & 0xFFFF)) //!< Sets the max value of the ramped PWM threshold; must be greater than the ramp min value and less than the PWM period
#define userio_set_pwm_ramp_min(baseaddr, m) Xil_Out32(baseaddr+W3_USERIO_SLV_REG9_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG9_OFFSET) & 0x8000FFFF) | (((m) & 0x7FFF)<<16)) //!< Sets the min value of the ramped PWM threshold; must be greater than 1 and less than the ramp max value

//reg0 masks
#define W3_USERIO_HEXDISP_L_MAPMODE		0x20000000 //!< Enables 4-bit to 7-segment mapping for left hex display
#define W3_USERIO_HEXDISP_R_MAPMODE		0x10000000 //!< Enables 4-bit to 7-segment mapping for right hex display
#define W3_USERIO_CTRLSRC_LED_RFB_RED	0x08000000 //!< Control source selection mask for red LED near RF B
#define W3_USERIO_CTRLSRC_LED_RFB_GREEN	0x04000000 //!< Control source selection mask for green LED near RF B
#define W3_USERIO_CTRLSRC_LED_RFA_RED	0x02000000 //!< Control source selection mask for red LED near RF A
#define W3_USERIO_CTRLSRC_LED_RFA_GREEN	0x01000000 //!< Control source selection mask for green LED near RF A
#define W3_USERIO_CTRLSRC_LEDS_RED		0x000F0000 //!< Control source selection mask for the red user LEDs
#define W3_USERIO_CTRLSRC_LEDS_GREEN	0x00F00000 //!< Control source selection mask for the green user LEDs
#define W3_USERIO_CTRLSRC_HEXDISP_R		0x0000FF00 //!< Control source selection mask for the left hex display (includes decimal point)
#define W3_USERIO_CTRLSRC_HEXDISP_L		0x000000FF //!< Control source selection mask for the right hex display (includes decimal point)
#define W3_USERIO_CTRLSRC_HEXDISP_DP_R	0x00008000 //!< Control source selection mask for the left hex display decimal point
#define W3_USERIO_CTRLSRC_HEXDISP_DP_L	0x00000080 //!< Control source selection mask for the right hex display decimal point

#define W3_USERIO_CTRLSRC_LEDS_RFA		(W3_USERIO_CTRLSRC_LED_RFA_RED | W3_USERIO_CTRLSRC_LED_RFA_GREEN) //!< Control source selection masks for both LEDs near RF A
#define W3_USERIO_CTRLSRC_LEDS_RFB		(W3_USERIO_CTRLSRC_LED_RFB_RED | W3_USERIO_CTRLSRC_LED_RFB_GREEN) //!< Control source selection masks for both LEDs near RF B
#define W3_USERIO_CTRLSRC_LEDS_RF		(W3_USERIO_CTRLSRC_LEDS_RFA | W3_USERIO_CTRLSRC_LEDS_RFB) //!< Control source selection masks for all RF LEDs
#define W3_USERIO_CTRLSRC_LEDS			(W3_USERIO_CTRLSRC_LEDS_RED | W3_USERIO_CTRLSRC_LEDS_GREEN) //!< Control source selection masks for all user LEDs
#define W3_USERIO_CTRLSRC_HEXDISPS		(W3_USERIO_CTRLSRC_HEXDISP_L | W3_USERIO_CTRLSRC_HEXDISP_R) //!< Control source selection masks for both hex displays

#define W3_USERIO_CTRLSRC_ALL_OUTPUTS	(W3_USERIO_CTRLSRC_LEDS_RF | W3_USERIO_CTRLSRC_LEDS  | W3_USERIO_CTRLSRC_HEXDISPS) //!< Control source selection masks for all outputs
/** @}*/


/** \defgroup userio_read Reading user IO
<b>Note on output state</b>: The macros for reading the current state of user outputs (LEDs, hex displays) can only access outputs configured for software control. Attempts to read the state
of outputs configured for hardware control (i.e. outputs with corresponding CTRLSRC_* asserted in control reg) will not reflect actual output state.

Examples:
\code{.c}
//Assumes user code sets USERIO_BASEADDR to base address of w3_userio core, as set in xparameters.h

//Check if middle push button is being pressed
if(userio_read_inputs(USERIO_BASEADDR) & W3_USERIO_PB_M) {...}

//Read 4-bit DIP switch value
u8 x = userio_read_inputs(USERIO_BASEADDR) & W3_USERIO_DIPSW;
\endcode

 *	\addtogroup userio_read
 * @{
 */
#define userio_read_inputs(baseaddr)		Xil_In32(baseaddr+W3_USERIO_SLV_REG6_OFFSET) //!< Returns the current state of the user inputs (buttons and DIP switch)
#define userio_read_hexdisp_left(baseaddr)	Xil_In32(baseaddr+W3_USERIO_SLV_REG1_OFFSET) //!< Returns the current state of the left hex display outputs
#define userio_read_hexdisp_right(baseaddr)	Xil_In32(baseaddr+W3_USERIO_SLV_REG2_OFFSET) //!< Returns the current state of the right hex display outputs
#define userio_read_leds_red(baseaddr) 		Xil_In32(baseaddr+W3_USERIO_SLV_REG3_OFFSET) //!< Returns the current state of the red user LEDs
#define userio_read_leds_green(baseaddr)	Xil_In32(baseaddr+W3_USERIO_SLV_REG4_OFFSET) //!< Returns the current state of the green user LEDs
#define userio_read_leds_rf(baseaddr)		Xil_In32(baseaddr+W3_USERIO_SLV_REG5_OFFSET) //!< Returns the current state of the RF LEDs
/** @}*/

/** \defgroup userio_write Setting user outputs

<b>Hex display notes:</b>
The w3_userio core implements logic to map 4-bit values to the 7-segment representation of the corresponding hex value. When this mode
is enabled via the control register (W3_USERIO_HEXDISP_x_MAPMODE is asserte), user code should write 4-bit values via the hex display macros below. When map
mode is disabled, the user value is driven directly to the 7-segments of the hex display.

The decimal point on each hex dipslay is controlled by OR'ing 4 bit (in map mode) or 7 bit (in non-map mode) value with W3_USERIO_HEXDISP_DP.

Examples:
\code{.c}
//Assumes user code sets USERIO_BASEADDR to base address of w3_userio core, as set in xparameters.h

//Display "B" on the left hex dipslay (assumes map mode is enabled; see control register docs)
userio_write_hexdisp_left(USERIO_ADDR, 0xB);

//Display "4" on the right hex dipslay and light the decimal point (assumes map mode is enabled; see control register docs)
userio_write_hexdisp_right(USERIO_ADDR, (0x4 | W3_USERIO_HEXDISP_DP) );

//Turn off all four green user LEDs
userio_write_leds_green(USERIO_ADDR, 0);

//Toggle the 2 LSB of the red user LEDs
userio_toggle_leds_red(USERIO_ADDR, 0x3);

\endcode
 *	\addtogroup userio_write
 * @{
 */
#define userio_write_hexdisp_left(baseaddr, x)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG1_OFFSET, x) //!< Sets the left hex dispaly
#define userio_write_hexdisp_right(baseaddr, x)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG2_OFFSET, x) //!< Sets the right hex dispaly
#define userio_write_leds_red(baseaddr, x) 		Xil_Out32(baseaddr+W3_USERIO_SLV_REG3_OFFSET, x) //!< Sets the 4 red LEDs when configured for software control (software control is default)
#define userio_write_leds_green(baseaddr, x)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG4_OFFSET, x) //!< Sets the 4 green LEDs when configured for software control (software control is default)
#define userio_write_leds_rf(baseaddr, x)		Xil_Out32(baseaddr+W3_USERIO_SLV_REG5_OFFSET, x) //!< Sets the 4 RF LEDs when configured for software control (hardware control is default)
#define userio_toggle_hexdisp_left(baseaddr, mask)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG1_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG1_OFFSET) ^ mask)) //!< Toggles the state of bits selected by mask on left hex display
#define userio_toggle_hexdisp_right(baseaddr, mask)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG2_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG2_OFFSET) ^ mask)) //!< Toggles the state of bits selected by mask on right hex display
#define userio_toggle_leds_red(baseaddr, mask)		Xil_Out32(baseaddr+W3_USERIO_SLV_REG3_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG3_OFFSET) ^ mask)) //!< Toggles the state of bits selected by mask in red LEDs
#define userio_toggle_leds_green(baseaddr, mask)	Xil_Out32(baseaddr+W3_USERIO_SLV_REG4_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG4_OFFSET) ^ mask)) //!< Toggles the state of bits selected by mask in green LEDs
#define userio_toggle_leds_rf(baseaddr, mask)		Xil_Out32(baseaddr+W3_USERIO_SLV_REG5_OFFSET, (Xil_In32(baseaddr+W3_USERIO_SLV_REG5_OFFSET) ^ mask)) //!< Toggles the state of bits selected by mask in RF LEDs
/** @}*/

/** \defgroup userio_masks Masks for user IO elements
 *	\addtogroup userio_masks
 * @{
 */

//reg1/reg2 masks
#define W3_USERIO_HEXDISP_DP			0x100 //!< Mask for decimal point LEDs on hex displays

//reg5 masks
#define W3_USERIO_RFA_LED_GREEN			0x1 //!< Mask for green LED near RF A
#define W3_USERIO_RFA_LED_RED			0x2 //!< Mask for red LED near RF A
#define W3_USERIO_RFB_LED_GREEN			0x4 //!< Mask for green LED near RF B
#define W3_USERIO_RFB_LED_RED			0x8 //!< Mask for red LED near RF B

//reg6 masks
#define W3_USERIO_PB_U	0x40 //!< Mask for up push button
#define W3_USERIO_PB_M	0x20 //!< Mask for middle push button
#define W3_USERIO_PB_D	0x10 //!< Mask for down push button
#define W3_USERIO_DIPSW	0x0F //!< Mask for 4 positions of DIP switch
/** @}*/

/** \defgroup dbg_hdr Debug Header I/O
The userio core implements an array of bi-directional buffers that can be routed to the WARP v3 board's debug header. The direction of each
pin is configured at run time. This is similar to the Xilinx axi_gpio core. However the w3_userio implmenetaiton supports reading the current
value of both inputs (like axi_gpi) and outputs (unlike axi_gpio). This allows read-modify-write of output bits, permitting simultaneous use
of the debug header pins from multiple CPUs.

 *	\addtogroup dbg_hdr
 * @{
 */
//reg14 masks
#define W3_USERIO_DBG_HDR_DIR_MASK		0xFFFF0000 //!< Mask for IOB direction control bits (1 per pin)
#define W3_USERIO_DBG_HDR_CTRLSRC_MASK	0X0000FFFF //!< Mask for IOB control source (1=hw, 0=sw) bits
#define W3_USERIO_DBG_HDR_VAL_MASK		0x0000FFFF //!< Mask for IOB data value bits (1 per pin)

#define DBG_HDR_DIR_OUTPUT				0x0 //!< Flag for setting debug header IO as output via dir argument to userio_set_dbg_hdr_io_dir()
#define DBG_HDR_DIR_INPUT				0x1 //!< Flag for setting debug header IO as input via dir argument to userio_set_dbg_hdr_io_dir()

#define DBG_HDR_CTRLSRC_SW				0x0 //!< Flag for setting debug header IO as software-controlled output via ctrlsrc argument to userio_set_dbg_hdr_out_ctrlsrc()
#define DBG_HDR_CTRLSRC_HW				0x1 //!< Flag for setting debug header IO as hardware-controlled output via ctrlsrc argument to userio_set_dbg_hdr_out_ctrlsrc()

#define userio_set_dbg_hdr_io_dir(baseaddr, dir, pin_mask) Xil_Out32((baseaddr)+W3_USERIO_SLV_REG14_OFFSET, \
	(dir) ? \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG14_OFFSET) |  (((pin_mask) << 16) & W3_USERIO_DBG_HDR_DIR_MASK)) : \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG14_OFFSET) & ~(((pin_mask) << 16) & W3_USERIO_DBG_HDR_DIR_MASK)))  //!< Configure IOB direction for debug header pins

#define userio_set_dbg_hdr_out(baseaddr, pin_mask) Xil_Out32((baseaddr)+W3_USERIO_SLV_REG13_OFFSET, \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG13_OFFSET) | ((pin_mask) & W3_USERIO_DBG_HDR_VAL_MASK)))   //!< Assert selected software-controlled debug header output pins

#define userio_clear_dbg_hdr_out(baseaddr, pin_mask) Xil_Out32((baseaddr)+W3_USERIO_SLV_REG13_OFFSET, \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG13_OFFSET) & ~((pin_mask) & W3_USERIO_DBG_HDR_VAL_MASK))) //!< Clear selected software-controlled debug header output pins

#define userio_write_dbg_hdr_out(baseaddr, val) Xil_Out32((baseaddr)+W3_USERIO_SLV_REG13_OFFSET, \
	((Xil_In32(baseaddr + W3_USERIO_SLV_REG13_OFFSET) & ~W3_USERIO_DBG_HDR_VAL_MASK) | ((val) & W3_USERIO_DBG_HDR_VAL_MASK))) //!< Write value of all software-controlled debug header output pins
	
#define userio_read_dbg_hdr_io(baseaddr) (Xil_In32((baseaddr)+W3_USERIO_SLV_REG13_OFFSET) & W3_USERIO_DBG_HDR_VAL_MASK) //!< Read value of all debug header pins
	
#define userio_set_dbg_hdr_out_ctrlsrc(baseaddr, ctrlsrc, pin_mask) Xil_Out32((baseaddr)+W3_USERIO_SLV_REG14_OFFSET, \
	(ctrlsrc) ? \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG14_OFFSET) |  ((pin_mask) & W3_USERIO_DBG_HDR_CTRLSRC_MASK)) : \
	(Xil_In32(baseaddr + W3_USERIO_SLV_REG14_OFFSET) & ~((pin_mask) & W3_USERIO_DBG_HDR_CTRLSRC_MASK))) //!< Configure output pins as controlled by software or hardware
 
 /** @}*/

 
/** \defgroup dna_read Reading FPGA DNA
Every Virtex-6 FPGA has a unique "DNA" value embedded in the device. The w3_userio core implements logic to read this value into software-accessible registers. The
DNA value is 56 bits, so two 32-bit registers are used to store the full value.

<b>Hardware requirements:</b>
<ul>
<li>A clock signal slower than 100MHz must be connected to the w3_userio core DNA_Port_Clk port
<li>The w3_userio core parameter INCLUDE_DNA_READ_LOGIC must be enabled
</ul>
If both requirements aren't met the DNA register values are undefined.

The FPGA DNA value is also stored in the WARP v3 board EEPROM. Refer to the user guide EEPROM page for details.
 *	\addtogroup dna_read
 * @{
 */
#define userio_read_fpga_dna_lsb(baseaddr)	Xil_In32(baseaddr+W3_USERIO_SLV_REG12_OFFSET) //!< Returns the 32 LSB of the FPGA DNA
#define userio_read_fpga_dna_msb(baseaddr)	Xil_In32(baseaddr+W3_USERIO_SLV_REG12_OFFSET) //!< Returns the 24 MSB of the FPGA DNA
/** @}*/

#endif /** W3_USERIO_H */

