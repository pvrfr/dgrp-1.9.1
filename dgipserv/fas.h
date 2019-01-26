/*
 * fas.h - Defines used in the Fast ASync (FAS) protocol used by Digi
 *         International EtherLite(R) Terminal Server modules.
 * 
 *****************************************************************************
 *
 * Copyright 1999-2000 Digi International (www.digi.com)
 *    Mark Schank <Mark_Schank@digi.com>
 *    Zhong_Deng <Zhong_Deng@digi.com>
 *    Jeff Randall <Jeff_Randall@digi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************
 *  
 */

#define FASCMDSZ		8

/* opcodes for async package */
#define FAS_GLOBAL		0
#define	FAS_ENABLE		1
#define	FAS_DISABLE		2
#define	FAS_SEND		3
#define	FAS_RECV		4
#define	FAS_IN_TIMERS		5
#define	FAS_OUTPUT_CTRL		6
#define FAS_INPUT_CTRL		7
#define	FAS_SET_MODEM		8
#define	FAS_STAT_CHG		9
#define	FAS_SEND_BREAK		10
#define	FAS_SET_PARAMS		11
#define	FAS_FLOW_CTRL		12
#define	FAS_RESERVE		13
#define	FAS_RELEASE		14
#define	FAS_RESET		15
#define	FAS_UNIT_STATS		16
#define	FAS_WAIT_SEND		17
#define	FAS_LINE_STATE		18
#define FAS_LINE_INQUIRY	19
#define FAS_LPT_TEST		20
#define FAS_UNIT_POLL		21
#define FAS_UNIT_UNLOCK		22
#define FAS_UNIT_INQUIRY	23
#define	FAS_EXT_PARAMS		24
#define FAS_UNIT_RESET		25
#define	FAS_CONFIG_LINK		50
#define	FAS_MUX_STATS		51
#define	PKT_END			100 /* some magic number > highest cmd num */

/* error codes */
#define	OK			0x00
#define	ERR_FAIL		0x80
#define	ERR_MULT_CMD		0x01
#define	ERR_BAD_PARAM		0x02
#define	ERR_NOT_PRESENT		0x03
#define	ERR_PARITY		0x04
#define	ERR_OVERRUN		0x05
#define	ERR_FRAMING		0x06
#define	ERR_UNKNOWN		0x07
#define	ERR_INITD		0x08
#define	ERR_BAD_CMD		0x09
#define	ERR_ABORTED		0x0a
#define	ERR_BREAK		0x0b
#define	ERR_OVERFLOW		0x0c
#define	ERR_OFFLINE		0x0d
#define	ERR_MORE		0x0e
#define ERR_BUSY		0x0f
#define ERR_MASK		0x7f


/* and for convenience the defs for my fave shorthand conventions */
#if !defined(uns8)
#define	uns8	unsigned char
#define	uns16	unsigned short
#define	uns32	unsigned long
#define	int8	char
#define	int16	short
#define	int32	long
#endif


/*
 * Structures for individual commands follow.
 */

/* FAS_ENABLE */
typedef
struct req_enable {
	uns8 opcode;
	uns8 line;
	uns8 pad[6];
} req_enable;

typedef
struct rsp_enable {
	uns8 opcode;
	uns8 line;
	uns8 status;
	uns8 pad[5];
} rsp_enable;

/* FAS_DISABLE */
#define req_disable		req_enable		/* synonym */
#define	rsp_disable		rsp_enable


/* FAS_STAT_CHG */
#define	SC_CTS	0x01
#define	SC_DSR	0x02
#define	SC_DCD	0x04
#define	SC_AUX	0x08
/* overloaded flags for parallel ports */
#define	SC_SEL	0x01
#define	SC_ERR	0x02
#define	SC_BSY	0x04
#define	SC_PE	0x08
#define	SC_IMMEDIATE	0x80	/* send immediate update */
typedef
struct req_stat_chg {
	uns8 opcode;
	uns8 line;
	uns8 flags;
	uns8 pad[5];
} req_stat_chg;

typedef
struct rsp_stat_chg {
	uns8 opcode;
	uns8 line;
	uns8 status;
	uns8 flags;
	uns8 pad[4];
} rsp_stat_chg;
/* flag defines (same as SCC uses for convenience) */

/* FAS_SEND */
typedef
struct req_send {
	uns8 opcode;
	uns8 line;
	uns8 cnt_lo;
	uns8 cnt_hi;
	uns8 pad[4];
} req_send;
#define	rsp_send		rsp_enable

/* FAS_RECV */
#define	req_recv			req_send

typedef
struct rsp_recv {
	uns8 opcode;
	uns8 line;
	uns8 status;
	uns8 pad;
	uns8 cnt_lo;
	uns8 cnt_hi;
	uns8 morepad[2];
} rsp_recv;


/* FAS_OUTPUT_CTRL */
typedef
struct req_output_ctrl {
	uns8 opcode;
	uns8 line;
	uns8 flush;
	uns8 suspend;
	uns8 resume;
	uns8 pad[3];
} req_output_ctrl;

#define	rsp_output_ctrl		rsp_enable

/* FAS_INPUT_CTRL */
#define	req_input_ctrl		req_output_ctrl
#define	rsp_input_ctrl		rsp_enable

/* FAS_SET_MODEM */
#define	MC_NO_CHG	0
#define	MC_ASSERT	1
#define	MC_DEASSERT	2
#define	MC_PERSIST	0x80	/* flag for persist thru open/close mode */

typedef
struct req_set_modem {
	uns8 opcode;
	uns8 line;
	uns8 rts_ctrl;
	uns8 dtr_ctrl;
	uns8 pad[4];
} req_set_modem;
#define	rsp_set_modem		rsp_enable

/* FAS_SET_PARAMS */
/* flag field defs */
#define	PF_RXENAB	0x01
#define	PF_NOISTRIP	0x02
#define	PF_LOOP		0x80
/* bits per character defines */
#define	C_SZ5		0		/* 5 bits/char */
#define	C_SZ6		1		/* 6 bits/char */
#define	C_SZ7		2		/* 7 bits/char */
#define	C_SZ8		3		/* 8 bits/char */
/* framing defines */
#define	C_FRAME1	0	/* 1 stop bit */
#define	C_FRAME1_5	1	/* 1.5 stop bits */
#define	C_FRAME2	2	/* 2 stop bits */
/* parity defines */
#define	NO_PARITY	0
#define	ODD_PARITY	1
#define	EVEN_PARITY	2
/* baudrate: low nibble is output, high nibble is input */
#define	FAS75		0x00
#define	FAS110		0x01
#define	FAS150		0x02
#define	FAS300		0x03
#define	FAS600		0x04
#define	FAS1200		0x05
#define	FAS1800		0x06
#define	FAS2000		0x07
#define	FAS2400		0x08
#define	FAS4800		0x09
#define	FAS9600		0x0a
#define	FAS19200	0x0b
#define	FAS38400	0x0c
#define	FAS57600	0x0d
#define	FAS76800	0x0e
#define	FAS115200	0x0f		/* end of old table */
#define FAS50		0x10
#define FAS100		0x11		/* Milspec baudrate */
#define FAS134		0x12
#define	FAS200		0x13
#define FAS900		0x14		/* HP-UX defines */
#define FAS3600		0x15		/* HP-UX defines */
#define FAS7200		0x16		/* HP-UX defines */
#define FAS153600	0x18
#define FAS230400	0x22
#define FAS307200	0x29
#define FAS460800	0x2f
#define FASNOBAUD	0xff

typedef		
struct req_set_params {
	uns8 opcode;
	uns8 line;
	uns8 c_size;
	uns8 c_frame;
	uns8 parity;
	uns8 flgs;
	union {
		uns8 baudrate_1_lo;
		uns8 baudrate_1_out;
	} baud_field1;
	union {
		uns8 baudrate_2_hi;
		uns8 baudrate_2_in;
	} baud_field2;
} req_set_params;
#define baudrate_lo		baud_field1.baudrate_1_lo
#define baudrate_hi		baud_field2.baudrate_2_hi
#define baudrate_out		baud_field1.baudrate_1_out
#define baudrate_in		baud_field2.baudrate_2_in



#define	rsp_set_params		rsp_enable

/* FAS_SEND_BREAK */
#define	req_send_break		req_enable
#define	rsp_send_break		rsp_enable

/* flag defs for FAS_GLOBAL */
#define	GLOBAL_LED_BLINK	0x01	/* blink run LED */
#define	GLOBAL_IGNORE_RESET	0x02	/* ignore SCSI reset */
#define	GLOBAL_DBUG_MODE	0x04	/* enable secret debug mode */
#define	GLOBAL_ONE_LUN_MODE	0x08	/* special SCSI mode */
#define	GLOBAL_NO_DEADMAN  	0x10	/* disables the SCSI deadman timer */
#define	GLOBAL_HIGH_BAUD  	0x20	/* Extended baudrate table used */

/* FAS_GLOBAL */
typedef
struct req_global {
	uns8 opcode;
	uns8 version;
	uns8 tick_rate_lo;
	uns8 tick_rate_hi;
	uns8 buf_size_lo;
	uns8 buf_size_hi;
	uns8 flags;
	uns8 full_err_chking;
} req_global;

typedef
struct rsp_global {
	uns8 opcode;
	uns8 version;
	uns8 status;
	uns8 pad2;
	uns16 eprom_id;
} rsp_global;


/* flow control modes */
#define FC_MODE			0x8f	/* mask for all FC mode */
#define	FC_SOFT			0x0f	/* mask for soft FC modes */
#define	FC_NONE			0x00	/* flow control disabled */
#define	FC_XON_XOFF1		0x01	/* allow chars thru between xon/xoff */
#define	FC_XON_XOFF2		0x02	/* dump intermediate chars */
#define	FC_IXANY		0x03	/* xoff stops, any char restarts */
#define	FC_NO_SWALLOW		0x40	/* don't swallow Xon/Xoff chars */
#define	FC_RTS_CTS		0x80	/* flag for hw handshaking using RTS */

/* FAS_FLOW_CTRL */
typedef
struct req_flow_ctrl {
	uns8 opcode;
	uns8 line;
	uns8 in_mode;
	uns8 out_mode;
	uns8 xon;
	uns8 xoff;
	uns8 buf_max_lo;
	uns8 buf_max_hi;
} req_flow_ctrl;
#define	rsp_flow_ctrl		rsp_enable

/* FAS_IN_TIMERS */
typedef
struct req_in_timers {
	uns8 opcode;
	uns8 line;
	uns16 timeout;
	uns8 pad[4];
} req_in_timers;
#define	rsp_in_timers		rsp_enable

/* FAS_RESERVE */
#define	req_reserve		req_enable
#define	rsp_reserve		rsp_enable

/* FAS_RELEASE */
#define	req_release		req_enable
#define	rsp_release		rsp_enable

/* FAS_RESET */
#define	req_reset		req_enable
#define	rsp_reset		rsp_enable

/* FAS_UNIT_STATS */
typedef
struct req_unit_stats {
	uns8 opcode;
	uns8 line;
	uns8 code;		/* code for which stat group to return */
	uns8 pad[5];
} req_unit_stats;
#define	rsp_unit_stats		rsp_recv

/* FAS_WAIT_SEND */
#define	req_wait_send		req_enable
#define	rsp_wait_send		rsp_enable

/* FAS_LINE_STATE */
#define	req_line_state		req_unit_stats
#define	rsp_line_state		rsp_recv

/* FAS_CONFIG_LINK */
typedef
struct req_config_link {
	uns8 opcode;
	uns8 pad;
	uns32 baudrate;
	uns8 reserved[2];
} req_config_link;
#define	rsp_config_link	rsp_enable;

/* Unit Poll Request and Response */
typedef
struct req_unit_poll {
	uns8 opcode;
	uns8 reserved[7];
} req_unit_poll;

/* Unit Unlock Request */
typedef
struct req_unit_unlock {
	uns8 opcode;
	uns8 reserved[7];
} req_unit_unlock;

/* Unit Unlock Response */
typedef
struct rsp_unit_unlock {
	uns8 opcode;
	uns8 reserved;
	uns8 challenge[6];
} rsp_unit_unlock;

/* Unit Inquiry Request */
typedef
struct req_unit_inquiry {
	uns8 opcode;
	uns8 reserved;
	uns8 key[3];
	uns8 reserved1[3];
} req_unit_inquiry;

/* Unit Inquiry Response */
typedef
struct rsp_unit_inquiry {
	uns8 opcode;
	uns8 reserved[5];
	uns8 cnt_lo;
    uns8 cnt_hi;
} rsp_unit_inquiry;

/* FAS_MUX_STATS */
#define	req_mux_stats	req_unit_stats
#define	rsp_mux_stats	rsp_recv

/* FAS_LINE_INQUIRY */
#define NO_LINE_TYPE		0	/* defines for the line_type */
#define SERIAL_LINE_TYPE	1
#define PARALLEL_LINE_TYPE	2

#define NO_LINE_MODIFIER	0	/* defines for the line modifier */
#define FAST_LINE_MODIFIER	1
typedef
struct rsp_line_inquiry {
	uns8 opcode;
	uns8 line;
	uns8 status;		
	uns8 line_type;
	uns8 line_modifier;	
	uns8 features;
	uns8 fw_major;		
	uns8 fw_minor;      
} rsp_line_inquiry;
#define	req_line_inquiry	req_enable

/*
 * Data returned by a read buffer/read unit command.
 */
typedef struct read_unit_info {
	char hardware_id[17];
	char firmware_id[7];
	uns8 features;
	uns8 max_pkt_sz_msb;
	uns8 max_pkt_sz_lsb;
	uns8 max_lines_msb;
	uns8 max_lines_lsb;
	uns8 resv[3];
} read_unit_info;

/*
 * Data returned by a read buffer/read line command.
 */
#define NO_FEATURES		0x00
#define RB_IS_2000		0x01

typedef struct read_line_info {
	char firmware_id[7];
	uns8 line_type;
	uns8 line_modifier;
	uns8 features;
	uns8 resv[22];
} read_line_info;
