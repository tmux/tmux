/* $OpenBSD$ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicholas.marriott@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tmux.h"

/*
 * Based on the description by Paul Williams at:
 *
 * http://vt100.net/emu/dec_ansi_parser
 *
 * With the following changes:
 *
 * - 7-bit only.
 *
 * - Support for UTF-8.
 *
 * - OSC (but not APC) may be terminated by \007 as well as ST.
 *
 * - A state for APC similar to OSC. Some terminals appear to use this to set
 *   the title.
 *
 * - A state for the screen \033k...\033\\ sequence to rename a window. This is
 *   pretty stupid but not supporting it is more trouble than it is worth.
 *
 * - Special handling for ESC inside a DCS to allow arbitrary byte sequences to
 *   be passed to the underlying terminals.
 */

/* Input parser cell. */
struct input_cell {
	struct grid_cell	cell;
	int			set;
	int			g0set;	/* 1 if ACS */
	int			g1set;	/* 1 if ACS */
};

/* Input parser context. */
struct input_ctx {
	struct window_pane     *wp;
	struct screen_write_ctx ctx;

	struct input_cell	cell;

	struct input_cell	old_cell;
	u_int 			old_cx;
	u_int			old_cy;

	u_char			interm_buf[4];
	size_t			interm_len;

	u_char			param_buf[64];
	size_t			param_len;

#define INPUT_BUF_START 32
#define INPUT_BUF_LIMIT 1048576
	u_char		       *input_buf;
	size_t			input_len;
	size_t			input_space;

	int			param_list[24];	/* -1 not present */
	u_int			param_list_len;

	struct utf8_data	utf8data;

	int			ch;
	int			flags;
#define INPUT_DISCARD 0x1

	const struct input_state *state;

	/*
	 * All input received since we were last in the ground state. Sent to
	 * control clients on connection.
	 */
	struct evbuffer	 	*since_ground;
};

/* Helper functions. */
struct input_transition;
int	input_split(struct input_ctx *);
int	input_get(struct input_ctx *, u_int, int, int);
void printflike(2, 3) input_reply(struct input_ctx *, const char *, ...);
void	input_set_state(struct window_pane *, const struct input_transition *);
void	input_reset_cell(struct input_ctx *);

/* Transition entry/exit handlers. */
void	input_clear(struct input_ctx *);
void	input_ground(struct input_ctx *);
void	input_enter_osc(struct input_ctx *);
void	input_exit_osc(struct input_ctx *);
void	input_enter_apc(struct input_ctx *);
void	input_exit_apc(struct input_ctx *);
void	input_enter_rename(struct input_ctx *);
void	input_exit_rename(struct input_ctx *);

/* Input state handlers. */
int	input_print(struct input_ctx *);
int	input_intermediate(struct input_ctx *);
int	input_parameter(struct input_ctx *);
int	input_input(struct input_ctx *);
int	input_c0_dispatch(struct input_ctx *);
int	input_esc_dispatch(struct input_ctx *);
int	input_csi_dispatch(struct input_ctx *);
void	input_csi_dispatch_rm(struct input_ctx *);
void	input_csi_dispatch_rm_private(struct input_ctx *);
void	input_csi_dispatch_sm(struct input_ctx *);
void	input_csi_dispatch_sm_private(struct input_ctx *);
void	input_csi_dispatch_winops(struct input_ctx *);
void	input_csi_dispatch_sgr_256(struct input_ctx *, int, u_int *);
void	input_csi_dispatch_sgr_rgb(struct input_ctx *, int, u_int *);
void	input_csi_dispatch_sgr(struct input_ctx *);
int	input_dcs_dispatch(struct input_ctx *);
int	input_utf8_open(struct input_ctx *);
int	input_utf8_add(struct input_ctx *);
int	input_utf8_close(struct input_ctx *);

/* Command table comparison function. */
int	input_table_compare(const void *, const void *);

/* Command table entry. */
struct input_table_entry {
	int		ch;
	const char     *interm;
	int		type;
};

/* Escape commands. */
enum input_esc_type {
	INPUT_ESC_DECALN,
	INPUT_ESC_DECKPAM,
	INPUT_ESC_DECKPNM,
	INPUT_ESC_DECRC,
	INPUT_ESC_DECSC,
	INPUT_ESC_HTS,
	INPUT_ESC_IND,
	INPUT_ESC_NEL,
	INPUT_ESC_RI,
	INPUT_ESC_RIS,
	INPUT_ESC_SCSG0_OFF,
	INPUT_ESC_SCSG0_ON,
	INPUT_ESC_SCSG1_OFF,
	INPUT_ESC_SCSG1_ON,
};

/* Escape command table. */
const struct input_table_entry input_esc_table[] = {
	{ '0', "(", INPUT_ESC_SCSG0_ON },
	{ '0', ")", INPUT_ESC_SCSG1_ON },
	{ '7', "",  INPUT_ESC_DECSC },
	{ '8', "",  INPUT_ESC_DECRC },
	{ '8', "#", INPUT_ESC_DECALN },
	{ '=', "",  INPUT_ESC_DECKPAM },
	{ '>', "",  INPUT_ESC_DECKPNM },
	{ 'B', "(", INPUT_ESC_SCSG0_OFF },
	{ 'B', ")", INPUT_ESC_SCSG1_OFF },
	{ 'D', "",  INPUT_ESC_IND },
	{ 'E', "",  INPUT_ESC_NEL },
	{ 'H', "",  INPUT_ESC_HTS },
	{ 'M', "",  INPUT_ESC_RI },
	{ 'c', "",  INPUT_ESC_RIS },
};

/* Control (CSI) commands. */
enum input_csi_type {
	INPUT_CSI_CBT,
	INPUT_CSI_CNL,
	INPUT_CSI_CPL,
	INPUT_CSI_CUB,
	INPUT_CSI_CUD,
	INPUT_CSI_CUF,
	INPUT_CSI_CUP,
	INPUT_CSI_CUU,
	INPUT_CSI_DA,
	INPUT_CSI_DA_TWO,
	INPUT_CSI_DCH,
	INPUT_CSI_DECSCUSR,
	INPUT_CSI_DECSTBM,
	INPUT_CSI_DL,
	INPUT_CSI_DSR,
	INPUT_CSI_ECH,
	INPUT_CSI_ED,
	INPUT_CSI_EL,
	INPUT_CSI_HPA,
	INPUT_CSI_ICH,
	INPUT_CSI_IL,
	INPUT_CSI_RCP,
	INPUT_CSI_RM,
	INPUT_CSI_RM_PRIVATE,
	INPUT_CSI_SCP,
	INPUT_CSI_SGR,
	INPUT_CSI_SM,
	INPUT_CSI_SM_PRIVATE,
	INPUT_CSI_TBC,
	INPUT_CSI_VPA,
	INPUT_CSI_WINOPS,
};

/* Control (CSI) command table. */
const struct input_table_entry input_csi_table[] = {
	{ '@', "",  INPUT_CSI_ICH },
	{ 'A', "",  INPUT_CSI_CUU },
	{ 'B', "",  INPUT_CSI_CUD },
	{ 'C', "",  INPUT_CSI_CUF },
	{ 'D', "",  INPUT_CSI_CUB },
	{ 'E', "",  INPUT_CSI_CNL },
	{ 'F', "",  INPUT_CSI_CPL },
	{ 'G', "",  INPUT_CSI_HPA },
	{ 'H', "",  INPUT_CSI_CUP },
	{ 'J', "",  INPUT_CSI_ED },
	{ 'K', "",  INPUT_CSI_EL },
	{ 'L', "",  INPUT_CSI_IL },
	{ 'M', "",  INPUT_CSI_DL },
	{ 'P', "",  INPUT_CSI_DCH },
	{ 'X', "",  INPUT_CSI_ECH },
	{ 'Z', "",  INPUT_CSI_CBT },
	{ 'c', "",  INPUT_CSI_DA },
	{ 'c', ">", INPUT_CSI_DA_TWO },
	{ 'd', "",  INPUT_CSI_VPA },
	{ 'f', "",  INPUT_CSI_CUP },
	{ 'g', "",  INPUT_CSI_TBC },
	{ 'h', "",  INPUT_CSI_SM },
	{ 'h', "?", INPUT_CSI_SM_PRIVATE },
	{ 'l', "",  INPUT_CSI_RM },
	{ 'l', "?", INPUT_CSI_RM_PRIVATE },
	{ 'm', "",  INPUT_CSI_SGR },
	{ 'n', "",  INPUT_CSI_DSR },
	{ 'q', " ", INPUT_CSI_DECSCUSR },
	{ 'r', "",  INPUT_CSI_DECSTBM },
	{ 's', "",  INPUT_CSI_SCP },
	{ 't', "",  INPUT_CSI_WINOPS },
	{ 'u', "",  INPUT_CSI_RCP },
};

/* Input transition. */
struct input_transition {
	int				first;
	int				last;

	int				(*handler)(struct input_ctx *);
	const struct input_state       *state;
};

/* Input state. */
struct input_state {
	const char			*name;
	void				(*enter)(struct input_ctx *);
	void				(*exit)(struct input_ctx *);
	const struct input_transition	*transitions;
};

/* State transitions available from all states. */
#define INPUT_STATE_ANYWHERE \
	{ 0x18, 0x18, input_c0_dispatch, &input_state_ground }, \
	{ 0x1a, 0x1a, input_c0_dispatch, &input_state_ground }, \
	{ 0x1b, 0x1b, NULL,		 &input_state_esc_enter }

/* Forward declarations of state tables. */
const struct input_transition input_state_ground_table[];
const struct input_transition input_state_esc_enter_table[];
const struct input_transition input_state_esc_intermediate_table[];
const struct input_transition input_state_csi_enter_table[];
const struct input_transition input_state_csi_parameter_table[];
const struct input_transition input_state_csi_intermediate_table[];
const struct input_transition input_state_csi_ignore_table[];
const struct input_transition input_state_dcs_enter_table[];
const struct input_transition input_state_dcs_parameter_table[];
const struct input_transition input_state_dcs_intermediate_table[];
const struct input_transition input_state_dcs_handler_table[];
const struct input_transition input_state_dcs_escape_table[];
const struct input_transition input_state_dcs_ignore_table[];
const struct input_transition input_state_osc_string_table[];
const struct input_transition input_state_apc_string_table[];
const struct input_transition input_state_rename_string_table[];
const struct input_transition input_state_consume_st_table[];
const struct input_transition input_state_utf8_three_table[];
const struct input_transition input_state_utf8_two_table[];
const struct input_transition input_state_utf8_one_table[];

/* ground state definition. */
const struct input_state input_state_ground = {
	"ground",
	input_ground, NULL,
	input_state_ground_table
};

/* esc_enter state definition. */
const struct input_state input_state_esc_enter = {
	"esc_enter",
	input_clear, NULL,
	input_state_esc_enter_table
};

/* esc_intermediate state definition. */
const struct input_state input_state_esc_intermediate = {
	"esc_intermediate",
	NULL, NULL,
	input_state_esc_intermediate_table
};

/* csi_enter state definition. */
const struct input_state input_state_csi_enter = {
	"csi_enter",
	input_clear, NULL,
	input_state_csi_enter_table
};

/* csi_parameter state definition. */
const struct input_state input_state_csi_parameter = {
	"csi_parameter",
	NULL, NULL,
	input_state_csi_parameter_table
};

/* csi_intermediate state definition. */
const struct input_state input_state_csi_intermediate = {
	"csi_intermediate",
	NULL, NULL,
	input_state_csi_intermediate_table
};

/* csi_ignore state definition. */
const struct input_state input_state_csi_ignore = {
	"csi_ignore",
	NULL, NULL,
	input_state_csi_ignore_table
};

/* dcs_enter state definition. */
const struct input_state input_state_dcs_enter = {
	"dcs_enter",
	input_clear, NULL,
	input_state_dcs_enter_table
};

/* dcs_parameter state definition. */
const struct input_state input_state_dcs_parameter = {
	"dcs_parameter",
	NULL, NULL,
	input_state_dcs_parameter_table
};

/* dcs_intermediate state definition. */
const struct input_state input_state_dcs_intermediate = {
	"dcs_intermediate",
	NULL, NULL,
	input_state_dcs_intermediate_table
};

/* dcs_handler state definition. */
const struct input_state input_state_dcs_handler = {
	"dcs_handler",
	NULL, NULL,
	input_state_dcs_handler_table
};

/* dcs_escape state definition. */
const struct input_state input_state_dcs_escape = {
	"dcs_escape",
	NULL, NULL,
	input_state_dcs_escape_table
};

/* dcs_ignore state definition. */
const struct input_state input_state_dcs_ignore = {
	"dcs_ignore",
	NULL, NULL,
	input_state_dcs_ignore_table
};

/* osc_string state definition. */
const struct input_state input_state_osc_string = {
	"osc_string",
	input_enter_osc, input_exit_osc,
	input_state_osc_string_table
};

/* apc_string state definition. */
const struct input_state input_state_apc_string = {
	"apc_string",
	input_enter_apc, input_exit_apc,
	input_state_apc_string_table
};

/* rename_string state definition. */
const struct input_state input_state_rename_string = {
	"rename_string",
	input_enter_rename, input_exit_rename,
	input_state_rename_string_table
};

/* consume_st state definition. */
const struct input_state input_state_consume_st = {
	"consume_st",
	NULL, NULL,
	input_state_consume_st_table
};

/* utf8_three state definition. */
const struct input_state input_state_utf8_three = {
	"utf8_three",
	NULL, NULL,
	input_state_utf8_three_table
};

/* utf8_two state definition. */
const struct input_state input_state_utf8_two = {
	"utf8_two",
	NULL, NULL,
	input_state_utf8_two_table
};

/* utf8_one state definition. */
const struct input_state input_state_utf8_one = {
	"utf8_one",
	NULL, NULL,
	input_state_utf8_one_table
};

/* ground state table. */
const struct input_transition input_state_ground_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch, NULL },
	{ 0x19, 0x19, input_c0_dispatch, NULL },
	{ 0x1c, 0x1f, input_c0_dispatch, NULL },
	{ 0x20, 0x7e, input_print,	 NULL },
	{ 0x7f, 0x7f, NULL,		 NULL },
	{ 0x80, 0xc1, NULL,		 NULL },
	{ 0xc2, 0xdf, input_utf8_open,	 &input_state_utf8_one },
	{ 0xe0, 0xef, input_utf8_open,	 &input_state_utf8_two },
	{ 0xf0, 0xf4, input_utf8_open,	 &input_state_utf8_three },
	{ 0xf5, 0xff, NULL,		 NULL },

	{ -1, -1, NULL, NULL }
};

/* esc_enter state table. */
const struct input_transition input_state_esc_enter_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch,  NULL },
	{ 0x19, 0x19, input_c0_dispatch,  NULL },
	{ 0x1c, 0x1f, input_c0_dispatch,  NULL },
	{ 0x20, 0x2f, input_intermediate, &input_state_esc_intermediate },
	{ 0x30, 0x4f, input_esc_dispatch, &input_state_ground },
	{ 0x50, 0x50, NULL,		  &input_state_dcs_enter },
	{ 0x51, 0x57, input_esc_dispatch, &input_state_ground },
	{ 0x58, 0x58, NULL,		  &input_state_consume_st },
	{ 0x59, 0x59, input_esc_dispatch, &input_state_ground },
	{ 0x5a, 0x5a, input_esc_dispatch, &input_state_ground },
	{ 0x5b, 0x5b, NULL,		  &input_state_csi_enter },
	{ 0x5c, 0x5c, input_esc_dispatch, &input_state_ground },
	{ 0x5d, 0x5d, NULL,		  &input_state_osc_string },
	{ 0x5e, 0x5e, NULL,		  &input_state_consume_st },
	{ 0x5f, 0x5f, NULL,		  &input_state_apc_string },
	{ 0x60, 0x6a, input_esc_dispatch, &input_state_ground },
	{ 0x6b, 0x6b, NULL,		  &input_state_rename_string },
	{ 0x6c, 0x7e, input_esc_dispatch, &input_state_ground },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* esc_interm state table. */
const struct input_transition input_state_esc_intermediate_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch,  NULL },
	{ 0x19, 0x19, input_c0_dispatch,  NULL },
	{ 0x1c, 0x1f, input_c0_dispatch,  NULL },
	{ 0x20, 0x2f, input_intermediate, NULL },
	{ 0x30, 0x7e, input_esc_dispatch, &input_state_ground },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* csi_enter state table. */
const struct input_transition input_state_csi_enter_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch,  NULL },
	{ 0x19, 0x19, input_c0_dispatch,  NULL },
	{ 0x1c, 0x1f, input_c0_dispatch,  NULL },
	{ 0x20, 0x2f, input_intermediate, &input_state_csi_intermediate },
	{ 0x30, 0x39, input_parameter,	  &input_state_csi_parameter },
	{ 0x3a, 0x3a, NULL,		  &input_state_csi_ignore },
	{ 0x3b, 0x3b, input_parameter,	  &input_state_csi_parameter },
	{ 0x3c, 0x3f, input_intermediate, &input_state_csi_parameter },
	{ 0x40, 0x7e, input_csi_dispatch, &input_state_ground },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* csi_parameter state table. */
const struct input_transition input_state_csi_parameter_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch,  NULL },
	{ 0x19, 0x19, input_c0_dispatch,  NULL },
	{ 0x1c, 0x1f, input_c0_dispatch,  NULL },
	{ 0x20, 0x2f, input_intermediate, &input_state_csi_intermediate },
	{ 0x30, 0x39, input_parameter,	  NULL },
	{ 0x3a, 0x3a, NULL,		  &input_state_csi_ignore },
	{ 0x3b, 0x3b, input_parameter,	  NULL },
	{ 0x3c, 0x3f, NULL,		  &input_state_csi_ignore },
	{ 0x40, 0x7e, input_csi_dispatch, &input_state_ground },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* csi_intermediate state table. */
const struct input_transition input_state_csi_intermediate_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch,  NULL },
	{ 0x19, 0x19, input_c0_dispatch,  NULL },
	{ 0x1c, 0x1f, input_c0_dispatch,  NULL },
	{ 0x20, 0x2f, input_intermediate, NULL },
	{ 0x30, 0x3f, NULL,		  &input_state_csi_ignore },
	{ 0x40, 0x7e, input_csi_dispatch, &input_state_ground },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* csi_ignore state table. */
const struct input_transition input_state_csi_ignore_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, input_c0_dispatch, NULL },
	{ 0x19, 0x19, input_c0_dispatch, NULL },
	{ 0x1c, 0x1f, input_c0_dispatch, NULL },
	{ 0x20, 0x3f, NULL,		 NULL },
	{ 0x40, 0x7e, NULL,		 &input_state_ground },
	{ 0x7f, 0xff, NULL,		 NULL },

	{ -1, -1, NULL, NULL }
};

/* dcs_enter state table. */
const struct input_transition input_state_dcs_enter_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,		  NULL },
	{ 0x19, 0x19, NULL,		  NULL },
	{ 0x1c, 0x1f, NULL,		  NULL },
	{ 0x20, 0x2f, input_intermediate, &input_state_dcs_intermediate },
	{ 0x30, 0x39, input_parameter,	  &input_state_dcs_parameter },
	{ 0x3a, 0x3a, NULL,		  &input_state_dcs_ignore },
	{ 0x3b, 0x3b, input_parameter,	  &input_state_dcs_parameter },
	{ 0x3c, 0x3f, input_intermediate, &input_state_dcs_parameter },
	{ 0x40, 0x7e, input_input,	  &input_state_dcs_handler },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* dcs_parameter state table. */
const struct input_transition input_state_dcs_parameter_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,		  NULL },
	{ 0x19, 0x19, NULL,		  NULL },
	{ 0x1c, 0x1f, NULL,		  NULL },
	{ 0x20, 0x2f, input_intermediate, &input_state_dcs_intermediate },
	{ 0x30, 0x39, input_parameter,	  NULL },
	{ 0x3a, 0x3a, NULL,		  &input_state_dcs_ignore },
	{ 0x3b, 0x3b, input_parameter,	  NULL },
	{ 0x3c, 0x3f, NULL,		  &input_state_dcs_ignore },
	{ 0x40, 0x7e, input_input,	  &input_state_dcs_handler },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* dcs_interm state table. */
const struct input_transition input_state_dcs_intermediate_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,		  NULL },
	{ 0x19, 0x19, NULL,		  NULL },
	{ 0x1c, 0x1f, NULL,		  NULL },
	{ 0x20, 0x2f, input_intermediate, NULL },
	{ 0x30, 0x3f, NULL,		  &input_state_dcs_ignore },
	{ 0x40, 0x7e, input_input,	  &input_state_dcs_handler },
	{ 0x7f, 0xff, NULL,		  NULL },

	{ -1, -1, NULL, NULL }
};

/* dcs_handler state table. */
const struct input_transition input_state_dcs_handler_table[] = {
	/* No INPUT_STATE_ANYWHERE */

	{ 0x00, 0x1a, input_input,  NULL },
	{ 0x1b, 0x1b, NULL,	    &input_state_dcs_escape },
	{ 0x1c, 0xff, input_input,  NULL },

	{ -1, -1, NULL, NULL }
};

/* dcs_escape state table. */
const struct input_transition input_state_dcs_escape_table[] = {
	/* No INPUT_STATE_ANYWHERE */

	{ 0x00, 0x5b, input_input,	  &input_state_dcs_handler },
	{ 0x5c, 0x5c, input_dcs_dispatch, &input_state_ground },
	{ 0x5d, 0xff, input_input,	  &input_state_dcs_handler },

	{ -1, -1, NULL, NULL }
};

/* dcs_ignore state table. */
const struct input_transition input_state_dcs_ignore_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,	    NULL },
	{ 0x19, 0x19, NULL,	    NULL },
	{ 0x1c, 0x1f, NULL,	    NULL },
	{ 0x20, 0xff, NULL,	    NULL },

	{ -1, -1, NULL, NULL }
};

/* osc_string state table. */
const struct input_transition input_state_osc_string_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x06, NULL,	    NULL },
	{ 0x07, 0x07, NULL,	    &input_state_ground },
	{ 0x08, 0x17, NULL,	    NULL },
	{ 0x19, 0x19, NULL,	    NULL },
	{ 0x1c, 0x1f, NULL,	    NULL },
	{ 0x20, 0xff, input_input,  NULL },

	{ -1, -1, NULL, NULL }
};

/* apc_string state table. */
const struct input_transition input_state_apc_string_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,	    NULL },
	{ 0x19, 0x19, NULL,	    NULL },
	{ 0x1c, 0x1f, NULL,	    NULL },
	{ 0x20, 0xff, input_input,  NULL },

	{ -1, -1, NULL, NULL }
};

/* rename_string state table. */
const struct input_transition input_state_rename_string_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,	    NULL },
	{ 0x19, 0x19, NULL,	    NULL },
	{ 0x1c, 0x1f, NULL,	    NULL },
	{ 0x20, 0xff, input_input,  NULL },

	{ -1, -1, NULL, NULL }
};

/* consume_st state table. */
const struct input_transition input_state_consume_st_table[] = {
	INPUT_STATE_ANYWHERE,

	{ 0x00, 0x17, NULL,	    NULL },
	{ 0x19, 0x19, NULL,	    NULL },
	{ 0x1c, 0x1f, NULL,	    NULL },
	{ 0x20, 0xff, NULL,	    NULL },

	{ -1, -1, NULL, NULL }
};

/* utf8_three state table. */
const struct input_transition input_state_utf8_three_table[] = {
	/* No INPUT_STATE_ANYWHERE */

	{ 0x00, 0x7f, NULL,		&input_state_ground },
	{ 0x80, 0xbf, input_utf8_add,	&input_state_utf8_two },
	{ 0xc0, 0xff, NULL,		&input_state_ground },

	{ -1, -1, NULL, NULL }
};

/* utf8_two state table. */
const struct input_transition input_state_utf8_two_table[] = {
	/* No INPUT_STATE_ANYWHERE */

	{ 0x00, 0x7f, NULL,	      &input_state_ground },
	{ 0x80, 0xbf, input_utf8_add, &input_state_utf8_one },
	{ 0xc0, 0xff, NULL,	      &input_state_ground },

	{ -1, -1, NULL, NULL }
};

/* utf8_one state table. */
const struct input_transition input_state_utf8_one_table[] = {
	/* No INPUT_STATE_ANYWHERE */

	{ 0x00, 0x7f, NULL,		&input_state_ground },
	{ 0x80, 0xbf, input_utf8_close, &input_state_ground },
	{ 0xc0, 0xff, NULL,		&input_state_ground },

	{ -1, -1, NULL, NULL }
};

/* Input table compare. */
int
input_table_compare(const void *key, const void *value)
{
	const struct input_ctx		*ictx = key;
	const struct input_table_entry	*entry = value;

	if (ictx->ch != entry->ch)
		return (ictx->ch - entry->ch);
	return (strcmp(ictx->interm_buf, entry->interm));
}

/* Reset cell state to default. */
void
input_reset_cell(struct input_ctx *ictx)
{
	memcpy(&ictx->cell.cell, &grid_default_cell, sizeof ictx->cell.cell);
	ictx->cell.set = 0;
	ictx->cell.g0set = ictx->cell.g1set = 0;

	memcpy(&ictx->old_cell, &ictx->cell, sizeof ictx->old_cell);
	ictx->old_cx = 0;
	ictx->old_cy = 0;
}

/* Initialise input parser. */
void
input_init(struct window_pane *wp)
{
	struct input_ctx	*ictx;

	ictx = wp->ictx = xcalloc(1, sizeof *ictx);

	ictx->input_space = INPUT_BUF_START;
	ictx->input_buf = xmalloc(INPUT_BUF_START);

	ictx->since_ground = evbuffer_new();

	input_reset(wp, 0);
}

/* Destroy input parser. */
void
input_free(struct window_pane *wp)
{
	struct input_ctx	*ictx = wp->ictx;

	free(ictx->input_buf);
	evbuffer_free(ictx->since_ground);

	free (ictx);
	wp->ictx = NULL;
}

/* Reset input state and clear screen. */
void
input_reset(struct window_pane *wp, int clear)
{
	struct input_ctx	*ictx = wp->ictx;

	input_reset_cell(ictx);

	if (clear) {
		if (wp->mode == NULL)
			screen_write_start(&ictx->ctx, wp, &wp->base);
		else
			screen_write_start(&ictx->ctx, NULL, &wp->base);
		screen_write_reset(&ictx->ctx);
		screen_write_stop(&ictx->ctx);
	}

	*ictx->interm_buf = '\0';
	ictx->interm_len = 0;

	*ictx->param_buf = '\0';
	ictx->param_len = 0;

	*ictx->input_buf = '\0';
	ictx->input_len = 0;

	ictx->state = &input_state_ground;
	ictx->flags = 0;
}

/* Return pending data. */
struct evbuffer *
input_pending(struct window_pane *wp)
{
	return (wp->ictx->since_ground);
}

/* Change input state. */
void
input_set_state(struct window_pane *wp, const struct input_transition *itr)
{
	struct input_ctx	*ictx = wp->ictx;

	if (ictx->state->exit != NULL)
		ictx->state->exit(ictx);
	ictx->state = itr->state;
	if (ictx->state->enter != NULL)
		ictx->state->enter(ictx);
}

/* Parse input. */
void
input_parse(struct window_pane *wp)
{
	struct input_ctx		*ictx = wp->ictx;
	const struct input_transition	*itr;
	struct evbuffer			*evb = wp->event->input;
	u_char				*buf;
	size_t				 len, off;

	if (EVBUFFER_LENGTH(evb) == 0)
		return;

	window_update_activity(wp->window);
	wp->flags |= PANE_CHANGED;

	/*
	 * Open the screen. Use NULL wp if there is a mode set as don't want to
	 * update the tty.
	 */
	if (wp->mode == NULL)
		screen_write_start(&ictx->ctx, wp, &wp->base);
	else
		screen_write_start(&ictx->ctx, NULL, &wp->base);
	ictx->wp = wp;

	buf = EVBUFFER_DATA(evb);
	len = EVBUFFER_LENGTH(evb);
	notify_input(wp, evb);
	off = 0;

	log_debug("%s: %%%u %s, %zu bytes: %.*s", __func__, wp->id,
	    ictx->state->name, len, (int)len, buf);

	/* Parse the input. */
	while (off < len) {
		ictx->ch = buf[off++];

		/* Find the transition. */
		itr = ictx->state->transitions;
		while (itr->first != -1 && itr->last != -1) {
			if (ictx->ch >= itr->first && ictx->ch <= itr->last)
				break;
			itr++;
		}
		if (itr->first == -1 || itr->last == -1) {
			/* No transition? Eh? */
			fatalx("no transition from state");
		}

		/*
		 * Execute the handler, if any. Don't switch state if it
		 * returns non-zero.
		 */
		if (itr->handler != NULL && itr->handler(ictx) != 0)
			continue;

		/* And switch state, if necessary. */
		if (itr->state != NULL)
			input_set_state(wp, itr);

		/* If not in ground state, save input. */
		if (ictx->state != &input_state_ground)
			evbuffer_add(ictx->since_ground, &ictx->ch, 1);
	}

	/* Close the screen. */
	screen_write_stop(&ictx->ctx);

	evbuffer_drain(evb, len);
}

/* Split the parameter list (if any). */
int
input_split(struct input_ctx *ictx)

{
	const char	*errstr;
	char		*ptr, *out;
	int		 n;

	ictx->param_list_len = 0;
	if (ictx->param_len == 0)
		return (0);

	ptr = ictx->param_buf;
	while ((out = strsep(&ptr, ";")) != NULL) {
		if (*out == '\0')
			n = -1;
		else {
			n = strtonum(out, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				return (-1);
		}

		ictx->param_list[ictx->param_list_len++] = n;
		if (ictx->param_list_len == nitems(ictx->param_list))
			return (-1);
	}

	return (0);
}

/* Get an argument or return default value. */
int
input_get(struct input_ctx *ictx, u_int validx, int minval, int defval)
{
	int	retval;

	if (validx >= ictx->param_list_len)
	    return (defval);

	retval = ictx->param_list[validx];
	if (retval == -1)
		return (defval);
	if (retval < minval)
		return (minval);
	return (retval);
}

/* Reply to terminal query. */
void
input_reply(struct input_ctx *ictx, const char *fmt, ...)
{
	va_list	ap;
	char   *reply;

	va_start(ap, fmt);
	vasprintf(&reply, fmt, ap);
	va_end(ap);

	bufferevent_write(ictx->wp->event, reply, strlen(reply));
	free(reply);
}

/* Clear saved state. */
void
input_clear(struct input_ctx *ictx)
{
	*ictx->interm_buf = '\0';
	ictx->interm_len = 0;

	*ictx->param_buf = '\0';
	ictx->param_len = 0;

	*ictx->input_buf = '\0';
	ictx->input_len = 0;

	ictx->flags &= ~INPUT_DISCARD;
}

/* Reset for ground state. */
void
input_ground(struct input_ctx *ictx)
{
	evbuffer_drain(ictx->since_ground, EVBUFFER_LENGTH(ictx->since_ground));

	if (ictx->input_space > INPUT_BUF_START) {
		ictx->input_space = INPUT_BUF_START;
		ictx->input_buf = xrealloc(ictx->input_buf, INPUT_BUF_START);
	}
}

/* Output this character to the screen. */
int
input_print(struct input_ctx *ictx)
{
	int	set;

	set = ictx->cell.set == 0 ? ictx->cell.g0set : ictx->cell.g1set;
	if (set == 1)
		ictx->cell.cell.attr |= GRID_ATTR_CHARSET;
	else
		ictx->cell.cell.attr &= ~GRID_ATTR_CHARSET;

	utf8_set(&ictx->cell.cell.data, ictx->ch);
	screen_write_cell(&ictx->ctx, &ictx->cell.cell);

	ictx->cell.cell.attr &= ~GRID_ATTR_CHARSET;

	return (0);
}

/* Collect intermediate string. */
int
input_intermediate(struct input_ctx *ictx)
{
	if (ictx->interm_len == (sizeof ictx->interm_buf) - 1)
		ictx->flags |= INPUT_DISCARD;
	else {
		ictx->interm_buf[ictx->interm_len++] = ictx->ch;
		ictx->interm_buf[ictx->interm_len] = '\0';
	}

	return (0);
}

/* Collect parameter string. */
int
input_parameter(struct input_ctx *ictx)
{
	if (ictx->param_len == (sizeof ictx->param_buf) - 1)
		ictx->flags |= INPUT_DISCARD;
	else {
		ictx->param_buf[ictx->param_len++] = ictx->ch;
		ictx->param_buf[ictx->param_len] = '\0';
	}

	return (0);
}

/* Collect input string. */
int
input_input(struct input_ctx *ictx)
{
	size_t available;

	available = ictx->input_space;
	while (ictx->input_len + 1 >= available) {
		available *= 2;
		if (available > INPUT_BUF_LIMIT) {
			ictx->flags |= INPUT_DISCARD;
			return (0);
		}
		ictx->input_buf = xrealloc(ictx->input_buf, available);
		ictx->input_space = available;
	}
	ictx->input_buf[ictx->input_len++] = ictx->ch;
	ictx->input_buf[ictx->input_len] = '\0';

	return (0);
}

/* Execute C0 control sequence. */
int
input_c0_dispatch(struct input_ctx *ictx)
{
	struct screen_write_ctx	*sctx = &ictx->ctx;
	struct window_pane	*wp = ictx->wp;
	struct screen		*s = sctx->s;

	log_debug("%s: '%c'", __func__, ictx->ch);

	switch (ictx->ch) {
	case '\000':	/* NUL */
		break;
	case '\007':	/* BEL */
		alerts_queue(wp->window, WINDOW_BELL);
		break;
	case '\010':	/* BS */
		screen_write_backspace(sctx);
		break;
	case '\011':	/* HT */
		/* Don't tab beyond the end of the line. */
		if (s->cx >= screen_size_x(s) - 1)
			break;

		/* Find the next tab point, or use the last column if none. */
		do {
			s->cx++;
			if (bit_test(s->tabs, s->cx))
				break;
		} while (s->cx < screen_size_x(s) - 1);
		break;
	case '\012':	/* LF */
	case '\013':	/* VT */
	case '\014':	/* FF */
		screen_write_linefeed(sctx, 0);
		break;
	case '\015':	/* CR */
		screen_write_carriagereturn(sctx);
		break;
	case '\016':	/* SO */
		ictx->cell.set = 1;
		break;
	case '\017':	/* SI */
		ictx->cell.set = 0;
		break;
	default:
		log_debug("%s: unknown '%c'", __func__, ictx->ch);
		break;
	}

	return (0);
}

/* Execute escape sequence. */
int
input_esc_dispatch(struct input_ctx *ictx)
{
	struct screen_write_ctx		*sctx = &ictx->ctx;
	struct screen			*s = sctx->s;
	struct input_table_entry	*entry;

	if (ictx->flags & INPUT_DISCARD)
		return (0);
	log_debug("%s: '%c', %s", __func__, ictx->ch, ictx->interm_buf);

	entry = bsearch(ictx, input_esc_table, nitems(input_esc_table),
	    sizeof input_esc_table[0], input_table_compare);
	if (entry == NULL) {
		log_debug("%s: unknown '%c'", __func__, ictx->ch);
		return (0);
	}

	switch (entry->type) {
	case INPUT_ESC_RIS:
		input_reset_cell(ictx);
		screen_write_reset(sctx);
		break;
	case INPUT_ESC_IND:
		screen_write_linefeed(sctx, 0);
		break;
	case INPUT_ESC_NEL:
		screen_write_carriagereturn(sctx);
		screen_write_linefeed(sctx, 0);
		break;
	case INPUT_ESC_HTS:
		if (s->cx < screen_size_x(s))
			bit_set(s->tabs, s->cx);
		break;
	case INPUT_ESC_RI:
		screen_write_reverseindex(sctx);
		break;
	case INPUT_ESC_DECKPAM:
		screen_write_mode_set(sctx, MODE_KKEYPAD);
		break;
	case INPUT_ESC_DECKPNM:
		screen_write_mode_clear(sctx, MODE_KKEYPAD);
		break;
	case INPUT_ESC_DECSC:
		memcpy(&ictx->old_cell, &ictx->cell, sizeof ictx->old_cell);
		ictx->old_cx = s->cx;
		ictx->old_cy = s->cy;
		break;
	case INPUT_ESC_DECRC:
		memcpy(&ictx->cell, &ictx->old_cell, sizeof ictx->cell);
		screen_write_cursormove(sctx, ictx->old_cx, ictx->old_cy);
		break;
	case INPUT_ESC_DECALN:
		screen_write_alignmenttest(sctx);
		break;
	case INPUT_ESC_SCSG0_ON:
		ictx->cell.g0set = 1;
		break;
	case INPUT_ESC_SCSG0_OFF:
		ictx->cell.g0set = 0;
		break;
	case INPUT_ESC_SCSG1_ON:
		ictx->cell.g1set = 1;
		break;
	case INPUT_ESC_SCSG1_OFF:
		ictx->cell.g1set = 0;
		break;
	}

	return (0);
}

/* Execute control sequence. */
int
input_csi_dispatch(struct input_ctx *ictx)
{
	struct screen_write_ctx	       *sctx = &ictx->ctx;
	struct screen		       *s = sctx->s;
	struct input_table_entry       *entry;
	int				n, m;
	u_int				cx;

	if (ictx->flags & INPUT_DISCARD)
		return (0);
	if (input_split(ictx) != 0)
		return (0);
	log_debug("%s: '%c' \"%s\" \"%s\"",
	    __func__, ictx->ch, ictx->interm_buf, ictx->param_buf);

	entry = bsearch(ictx, input_csi_table, nitems(input_csi_table),
	    sizeof input_csi_table[0], input_table_compare);
	if (entry == NULL) {
		log_debug("%s: unknown '%c'", __func__, ictx->ch);
		return (0);
	}

	switch (entry->type) {
	case INPUT_CSI_CBT:
		/* Find the previous tab point, n times. */
		cx = s->cx;
		if (cx > screen_size_x(s) - 1)
			cx = screen_size_x(s) - 1;
		n = input_get(ictx, 0, 1, 1);
		while (cx > 0 && n-- > 0) {
			do
				cx--;
			while (cx > 0 && !bit_test(s->tabs, cx));
		}
		s->cx = cx;
		break;
	case INPUT_CSI_CUB:
		screen_write_cursorleft(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_CUD:
		screen_write_cursordown(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_CUF:
		screen_write_cursorright(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_CUP:
		n = input_get(ictx, 0, 1, 1);
		m = input_get(ictx, 1, 1, 1);
		screen_write_cursormove(sctx, m - 1, n - 1);
		break;
	case INPUT_CSI_WINOPS:
		input_csi_dispatch_winops(ictx);
		break;
	case INPUT_CSI_CUU:
		screen_write_cursorup(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_CNL:
		screen_write_carriagereturn(sctx);
		screen_write_cursordown(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_CPL:
		screen_write_carriagereturn(sctx);
		screen_write_cursorup(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_DA:
		switch (input_get(ictx, 0, 0, 0)) {
		case 0:
			input_reply(ictx, "\033[?1;2c");
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_DA_TWO:
		switch (input_get(ictx, 0, 0, 0)) {
		case 0:
			input_reply(ictx, "\033[>84;0;0c");
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_ECH:
		screen_write_clearcharacter(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_DCH:
		screen_write_deletecharacter(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_DECSTBM:
		n = input_get(ictx, 0, 1, 1);
		m = input_get(ictx, 1, 1, screen_size_y(s));
		screen_write_scrollregion(sctx, n - 1, m - 1);
		break;
	case INPUT_CSI_DL:
		screen_write_deleteline(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_DSR:
		switch (input_get(ictx, 0, 0, 0)) {
		case 5:
			input_reply(ictx, "\033[0n");
			break;
		case 6:
			input_reply(ictx, "\033[%u;%uR", s->cy + 1, s->cx + 1);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_ED:
		switch (input_get(ictx, 0, 0, 0)) {
		case 0:
			screen_write_clearendofscreen(sctx);
			break;
		case 1:
			screen_write_clearstartofscreen(sctx);
			break;
		case 2:
			screen_write_clearscreen(sctx);
			break;
		case 3:
			switch (input_get(ictx, 1, 0, 0)) {
			case 0:
				/*
				 * Linux console extension to clear history
				 * (for example before locking the screen).
				 */
				screen_write_clearhistory(sctx);
				break;
			}
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_EL:
		switch (input_get(ictx, 0, 0, 0)) {
		case 0:
			screen_write_clearendofline(sctx);
			break;
		case 1:
			screen_write_clearstartofline(sctx);
			break;
		case 2:
			screen_write_clearline(sctx);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_HPA:
		n = input_get(ictx, 0, 1, 1);
		screen_write_cursormove(sctx, n - 1, s->cy);
		break;
	case INPUT_CSI_ICH:
		screen_write_insertcharacter(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_IL:
		screen_write_insertline(sctx, input_get(ictx, 0, 1, 1));
		break;
	case INPUT_CSI_RCP:
		memcpy(&ictx->cell, &ictx->old_cell, sizeof ictx->cell);
		screen_write_cursormove(sctx, ictx->old_cx, ictx->old_cy);
		break;
	case INPUT_CSI_RM:
		input_csi_dispatch_rm(ictx);
		break;
	case INPUT_CSI_RM_PRIVATE:
		input_csi_dispatch_rm_private(ictx);
		break;
	case INPUT_CSI_SCP:
		memcpy(&ictx->old_cell, &ictx->cell, sizeof ictx->old_cell);
		ictx->old_cx = s->cx;
		ictx->old_cy = s->cy;
		break;
	case INPUT_CSI_SGR:
		input_csi_dispatch_sgr(ictx);
		break;
	case INPUT_CSI_SM:
		input_csi_dispatch_sm(ictx);
		break;
	case INPUT_CSI_SM_PRIVATE:
		input_csi_dispatch_sm_private(ictx);
		break;
	case INPUT_CSI_TBC:
		switch (input_get(ictx, 0, 0, 0)) {
		case 0:
			if (s->cx < screen_size_x(s))
				bit_clear(s->tabs, s->cx);
			break;
		case 3:
			bit_nclear(s->tabs, 0, screen_size_x(s) - 1);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		break;
	case INPUT_CSI_VPA:
		n = input_get(ictx, 0, 1, 1);
		screen_write_cursormove(sctx, s->cx, n - 1);
		break;
	case INPUT_CSI_DECSCUSR:
		n = input_get(ictx, 0, 0, 0);
		screen_set_cursor_style(s, n);
		break;
	}

	return (0);
}

/* Handle CSI RM. */
void
input_csi_dispatch_rm(struct input_ctx *ictx)
{
	u_int	i;

	for (i = 0; i < ictx->param_list_len; i++) {
		switch (input_get(ictx, i, 0, -1)) {
		case 4:		/* IRM */
			screen_write_mode_clear(&ictx->ctx, MODE_INSERT);
			break;
		case 34:
			screen_write_mode_set(&ictx->ctx, MODE_BLINKING);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
	}
}

/* Handle CSI private RM. */
void
input_csi_dispatch_rm_private(struct input_ctx *ictx)
{
	struct window_pane	*wp = ictx->wp;
	u_int			 i;

	for (i = 0; i < ictx->param_list_len; i++) {
		switch (input_get(ictx, i, 0, -1)) {
		case 1:		/* DECCKM */
			screen_write_mode_clear(&ictx->ctx, MODE_KCURSOR);
			break;
		case 3:		/* DECCOLM */
			screen_write_cursormove(&ictx->ctx, 0, 0);
			screen_write_clearscreen(&ictx->ctx);
			break;
		case 7:		/* DECAWM */
			screen_write_mode_clear(&ictx->ctx, MODE_WRAP);
			break;
		case 12:
			screen_write_mode_clear(&ictx->ctx, MODE_BLINKING);
			break;
		case 25:	/* TCEM */
			screen_write_mode_clear(&ictx->ctx, MODE_CURSOR);
			break;
		case 1000:
		case 1001:
		case 1002:
			screen_write_mode_clear(&ictx->ctx, ALL_MOUSE_MODES);
			break;
		case 1004:
			screen_write_mode_clear(&ictx->ctx, MODE_FOCUSON);
			break;
		case 1005:
			screen_write_mode_clear(&ictx->ctx, MODE_MOUSE_UTF8);
			break;
		case 1006:
			screen_write_mode_clear(&ictx->ctx, MODE_MOUSE_SGR);
			break;
		case 47:
		case 1047:
			window_pane_alternate_off(wp, &ictx->cell.cell, 0);
			break;
		case 1049:
			window_pane_alternate_off(wp, &ictx->cell.cell, 1);
			break;
		case 2004:
			screen_write_mode_clear(&ictx->ctx, MODE_BRACKETPASTE);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
	}
}

/* Handle CSI SM. */
void
input_csi_dispatch_sm(struct input_ctx *ictx)
{
	u_int	i;

	for (i = 0; i < ictx->param_list_len; i++) {
		switch (input_get(ictx, i, 0, -1)) {
		case 4:		/* IRM */
			screen_write_mode_set(&ictx->ctx, MODE_INSERT);
			break;
		case 34:
			screen_write_mode_clear(&ictx->ctx, MODE_BLINKING);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
	}
}

/* Handle CSI private SM. */
void
input_csi_dispatch_sm_private(struct input_ctx *ictx)
{
	struct window_pane	*wp = ictx->wp;
	u_int			 i;

	for (i = 0; i < ictx->param_list_len; i++) {
		switch (input_get(ictx, i, 0, -1)) {
		case 1:		/* DECCKM */
			screen_write_mode_set(&ictx->ctx, MODE_KCURSOR);
			break;
		case 3:		/* DECCOLM */
			screen_write_cursormove(&ictx->ctx, 0, 0);
			screen_write_clearscreen(&ictx->ctx);
			break;
		case 7:		/* DECAWM */
			screen_write_mode_set(&ictx->ctx, MODE_WRAP);
			break;
		case 12:
			screen_write_mode_set(&ictx->ctx, MODE_BLINKING);
			break;
		case 25:	/* TCEM */
			screen_write_mode_set(&ictx->ctx, MODE_CURSOR);
			break;
		case 1000:
			screen_write_mode_clear(&ictx->ctx, ALL_MOUSE_MODES);
			screen_write_mode_set(&ictx->ctx, MODE_MOUSE_STANDARD);
			break;
		case 1002:
			screen_write_mode_clear(&ictx->ctx, ALL_MOUSE_MODES);
			screen_write_mode_set(&ictx->ctx, MODE_MOUSE_BUTTON);
			break;
		case 1004:
			if (ictx->ctx.s->mode & MODE_FOCUSON)
				break;
			screen_write_mode_set(&ictx->ctx, MODE_FOCUSON);
			wp->flags |= PANE_FOCUSPUSH; /* force update */
			break;
		case 1005:
			screen_write_mode_set(&ictx->ctx, MODE_MOUSE_UTF8);
			break;
		case 1006:
			screen_write_mode_set(&ictx->ctx, MODE_MOUSE_SGR);
			break;
		case 47:
		case 1047:
			window_pane_alternate_on(wp, &ictx->cell.cell, 0);
			break;
		case 1049:
			window_pane_alternate_on(wp, &ictx->cell.cell, 1);
			break;
		case 2004:
			screen_write_mode_set(&ictx->ctx, MODE_BRACKETPASTE);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
	}
}

/* Handle CSI window operations. */
void
input_csi_dispatch_winops(struct input_ctx *ictx)
{
	struct window_pane	*wp = ictx->wp;
	int			 n, m;

	m = 0;
	while ((n = input_get(ictx, m, 0, -1)) != -1) {
		switch (n) {
		case 1:
		case 2:
		case 5:
		case 6:
		case 7:
		case 11:
		case 13:
		case 14:
		case 19:
		case 20:
		case 21:
		case 24:
			break;
		case 3:
		case 4:
		case 8:
			m++;
			if (input_get(ictx, m, 0, -1) == -1)
				return;
			/* FALLTHROUGH */
		case 9:
		case 10:
		case 22:
		case 23:
			m++;
			if (input_get(ictx, m, 0, -1) == -1)
				return;
			break;
		case 18:
			input_reply(ictx, "\033[8;%u;%ut", wp->sy, wp->sx);
			break;
		default:
			log_debug("%s: unknown '%c'", __func__, ictx->ch);
			break;
		}
		m++;
	}
}

/* Handle CSI SGR for 256 colours. */
void
input_csi_dispatch_sgr_256(struct input_ctx *ictx, int fgbg, u_int *i)
{
	struct grid_cell	*gc = &ictx->cell.cell;
	int			 c;

	(*i)++;
	c = input_get(ictx, *i, 0, -1);
	if (c == -1) {
		if (fgbg == 38) {
			gc->flags &= ~(GRID_FLAG_FG256|GRID_FLAG_FGRGB);
			gc->fg = 8;
		} else if (fgbg == 48) {
			gc->flags &= ~(GRID_FLAG_BG256|GRID_FLAG_BGRGB);
			gc->bg = 8;
		}
	} else {
		if (fgbg == 38) {
			gc->flags |= GRID_FLAG_FG256;
			gc->flags &= ~GRID_FLAG_FGRGB;
			gc->fg = c;
		} else if (fgbg == 48) {
			gc->flags |= GRID_FLAG_BG256;
			gc->flags &= ~GRID_FLAG_BGRGB;
			gc->bg = c;
		}
	}
}

/* Handle CSI SGR for RGB colours. */
void
input_csi_dispatch_sgr_rgb(struct input_ctx *ictx, int fgbg, u_int *i)
{
	struct grid_cell	*gc = &ictx->cell.cell;
	int			 r, g, b;

	(*i)++;
	r = input_get(ictx, *i, 0, -1);
	if (r == -1 || r > 255)
		return;
	(*i)++;
	g = input_get(ictx, *i, 0, -1);
	if (g == -1 || g > 255)
		return;
	(*i)++;
	b = input_get(ictx, *i, 0, -1);
	if (b == -1 || b > 255)
		return;

	if (fgbg == 38) {
		gc->flags &= ~GRID_FLAG_FG256;
		gc->flags |= GRID_FLAG_FGRGB;
		gc->fg_rgb.r = r;
		gc->fg_rgb.g = g;
		gc->fg_rgb.b = b;
	} else if (fgbg == 48) {
		gc->flags &= ~GRID_FLAG_BG256;
		gc->flags |= GRID_FLAG_BGRGB;
		gc->bg_rgb.r = r;
		gc->bg_rgb.g = g;
		gc->bg_rgb.b = b;
	}
}

/* Handle CSI SGR. */
void
input_csi_dispatch_sgr(struct input_ctx *ictx)
{
	struct grid_cell	*gc = &ictx->cell.cell;
	u_int			 i;
	int			 n;

	if (ictx->param_list_len == 0) {
		memcpy(gc, &grid_default_cell, sizeof *gc);
		return;
	}

	for (i = 0; i < ictx->param_list_len; i++) {
		n = input_get(ictx, i, 0, 0);

		if (n == 38 || n == 48) {
			i++;
			switch (input_get(ictx, i, 0, -1)) {
			case 2:
				input_csi_dispatch_sgr_rgb(ictx, n, &i);
				break;
			case 5:
				input_csi_dispatch_sgr_256(ictx, n, &i);
				break;
			}
			continue;
		}

		switch (n) {
		case 0:
		case 10:
			memcpy(gc, &grid_default_cell, sizeof *gc);
			break;
		case 1:
			gc->attr |= GRID_ATTR_BRIGHT;
			break;
		case 2:
			gc->attr |= GRID_ATTR_DIM;
			break;
		case 3:
			gc->attr |= GRID_ATTR_ITALICS;
			break;
		case 4:
			gc->attr |= GRID_ATTR_UNDERSCORE;
			break;
		case 5:
			gc->attr |= GRID_ATTR_BLINK;
			break;
		case 7:
			gc->attr |= GRID_ATTR_REVERSE;
			break;
		case 8:
			gc->attr |= GRID_ATTR_HIDDEN;
			break;
		case 22:
			gc->attr &= ~(GRID_ATTR_BRIGHT|GRID_ATTR_DIM);
			break;
		case 23:
			gc->attr &= ~GRID_ATTR_ITALICS;
			break;
		case 24:
			gc->attr &= ~GRID_ATTR_UNDERSCORE;
			break;
		case 25:
			gc->attr &= ~GRID_ATTR_BLINK;
			break;
		case 27:
			gc->attr &= ~GRID_ATTR_REVERSE;
			break;
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			gc->flags &= ~(GRID_FLAG_FG256|GRID_FLAG_FGRGB);
			gc->fg = n - 30;
			break;
		case 39:
			gc->flags &= ~(GRID_FLAG_FG256|GRID_FLAG_FGRGB);
			gc->fg = 8;
			break;
		case 40:
		case 41:
		case 42:
		case 43:
		case 44:
		case 45:
		case 46:
		case 47:
			gc->flags &= ~(GRID_FLAG_BG256|GRID_FLAG_BGRGB);
			gc->bg = n - 40;
			break;
		case 49:
			gc->flags &= ~(GRID_FLAG_BG256|GRID_FLAG_BGRGB);
			gc->bg = 8;
			break;
		case 90:
		case 91:
		case 92:
		case 93:
		case 94:
		case 95:
		case 96:
		case 97:
			gc->flags &= ~(GRID_FLAG_FG256|GRID_FLAG_FGRGB);
			gc->fg = n;
			break;
		case 100:
		case 101:
		case 102:
		case 103:
		case 104:
		case 105:
		case 106:
		case 107:
			gc->flags &= ~(GRID_FLAG_BG256|GRID_FLAG_BGRGB);
			gc->bg = n - 10;
			break;
		}
	}
}

/* DCS terminator (ST) received. */
int
input_dcs_dispatch(struct input_ctx *ictx)
{
	const char	prefix[] = "tmux;";
	const u_int	prefix_len = (sizeof prefix) - 1;

	if (ictx->flags & INPUT_DISCARD)
		return (0);

	log_debug("%s: \"%s\"", __func__, ictx->input_buf);

	/* Check for tmux prefix. */
	if (ictx->input_len >= prefix_len &&
	    strncmp(ictx->input_buf, prefix, prefix_len) == 0) {
		screen_write_rawstring(&ictx->ctx,
		    ictx->input_buf + prefix_len, ictx->input_len - prefix_len);
	}

	return (0);
}

/* OSC string started. */
void
input_enter_osc(struct input_ctx *ictx)
{
	log_debug("%s", __func__);

	input_clear(ictx);
}

/* OSC terminator (ST) received. */
void
input_exit_osc(struct input_ctx *ictx)
{
	u_char	*p = ictx->input_buf;
	u_int	 option;

	if (ictx->flags & INPUT_DISCARD)
		return;
	if (ictx->input_len < 1 || *p < '0' || *p > '9')
		return;

	log_debug("%s: \"%s\"", __func__, p);

	option = 0;
	while (*p >= '0' && *p <= '9')
		option = option * 10 + *p++ - '0';
	if (*p == ';')
		p++;

	switch (option) {
	case 0:
	case 2:
		screen_set_title(ictx->ctx.s, p);
		server_status_window(ictx->wp->window);
		break;
	case 12:
		if (*p != '?') /* ? is colour request */
			screen_set_cursor_colour(ictx->ctx.s, p);
		break;
	case 112:
		if (*p == '\0') /* no arguments allowed */
			screen_set_cursor_colour(ictx->ctx.s, "");
		break;
	default:
		log_debug("%s: unknown '%u'", __func__, option);
		break;
	}
}

/* APC string started. */
void
input_enter_apc(struct input_ctx *ictx)
{
	log_debug("%s", __func__);

	input_clear(ictx);
}

/* APC terminator (ST) received. */
void
input_exit_apc(struct input_ctx *ictx)
{
	if (ictx->flags & INPUT_DISCARD)
		return;
	log_debug("%s: \"%s\"", __func__, ictx->input_buf);

	screen_set_title(ictx->ctx.s, ictx->input_buf);
	server_status_window(ictx->wp->window);
}

/* Rename string started. */
void
input_enter_rename(struct input_ctx *ictx)
{
	log_debug("%s", __func__);

	input_clear(ictx);
}

/* Rename terminator (ST) received. */
void
input_exit_rename(struct input_ctx *ictx)
{
	if (ictx->flags & INPUT_DISCARD)
		return;
	if (!options_get_number(ictx->wp->window->options, "allow-rename"))
		return;
	log_debug("%s: \"%s\"", __func__, ictx->input_buf);

	window_set_name(ictx->wp->window, ictx->input_buf);
	options_set_number(ictx->wp->window->options, "automatic-rename", 0);

	server_status_window(ictx->wp->window);
}

/* Open UTF-8 character. */
int
input_utf8_open(struct input_ctx *ictx)
{
	struct utf8_data	*ud = &ictx->utf8data;

	if (utf8_open(ud, ictx->ch) != UTF8_MORE)
		fatalx("UTF-8 open invalid %#x", ictx->ch);

	log_debug("%s %hhu", __func__, ud->size);

	return (0);
}

/* Append to UTF-8 character. */
int
input_utf8_add(struct input_ctx *ictx)
{
	struct utf8_data	*ud = &ictx->utf8data;

	if (utf8_append(ud, ictx->ch) != UTF8_MORE)
		fatalx("UTF-8 add invalid %#x", ictx->ch);

	log_debug("%s", __func__);

	return (0);
}

/* Close UTF-8 string. */
int
input_utf8_close(struct input_ctx *ictx)
{
	struct utf8_data	*ud = &ictx->utf8data;

	if (utf8_append(ud, ictx->ch) != UTF8_DONE) {
		/*
		 * An error here could be invalid UTF-8 or it could be a
		 * nonprintable character for which we can't get the
		 * width. Drop it.
		 */
		return (0);
	}

	log_debug("%s %hhu '%*s' (width %hhu)", __func__, ud->size,
	    (int)ud->size, ud->data, ud->width);

	utf8_copy(&ictx->cell.cell.data, ud);
	screen_write_cell(&ictx->ctx, &ictx->cell.cell);

	return (0);
}
