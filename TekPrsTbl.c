/* $XTermId: TekPrsTbl.c,v 1.8 2006/02/13 01:14:57 tom Exp $ */

/*
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 *
 *                         All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital Equipment
 * Corporation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior permission.
 *
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
/* $XFree86: xc/programs/xterm/TekPrsTbl.c,v 3.5 2006/02/13 01:14:57 dickey Exp $ */

#include <Tekparse.h>

Const int Talptable[] =		/* US (^_) normal alpha mode */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_ESC_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_SP,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	$		%		&		'	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	(		)		*		+	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	,		-		.		/	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	0		1		2		3	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	4		5		6		7	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	8		9		:		;	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	<		=		>		?	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	@		A		B		C	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	D		E		F		G	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	H		I		J		K	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	L		M		N		O	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	P		Q		R		S	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	T		U		V		W	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	X		Y		Z		[	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	\		]		^		_	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	`		a		b		c	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	d		e		f		g	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	h		i		j		k	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	l		m		n		o	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	p		q		r		s	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	t		u		v		w	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	x		y		z		{	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*	|		}		~		DEL	*/
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_IGNORE,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      currency        yen             brokenbar       section         */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      notsign         hyphen          registered      macron          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      acute           mu              paragraph       periodcentered  */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      agrave          aacute          acircumflex     atilde          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      eth             ntilde          ograve          oacute          */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
CASE_PRINT,
};

Const int Tbestable[] =		/* ESC while in bypass state */
{
/*	NUL		SOH		STX		ETX	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_VT_MODE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_BYP_STATE,
CASE_REPORT,
CASE_BYP_STATE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_IGNORE,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_PAGE,
CASE_IGNORE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	DLE		DC1		DC2		DC3	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	DC4		NAK		SYN		ETB	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_COPY,
/*	CAN		EM		SUB		ESC	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_GIN,
CASE_IGNORE,
/*	FS		GS		RS		US	*/
CASE_SPT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	$		%		&		'	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	(		)		*		+	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	,		-		.		/	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	0		1		2		3	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	4		5		6		7	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	8		9		:		;	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	<		=		>		?	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	@		A		B		C	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	D		E		F		G	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	H		I		J		K	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	L		M		N		O	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	P		Q		R		S	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	T		U		V		W	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	X		Y		Z		[	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	\		]		^		_	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	`		a		b		c	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	d		e		f		g	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	h		i		j		k	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	l		m		n		o	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	p		q		r		s	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	t		u		v		w	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	x		y		z		{	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*	|		}		~		DEL	*/
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_IGNORE,
CASE_BYP_STATE,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      currency        yen             brokenbar       section         */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      notsign         hyphen          registered      macron          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      acute           mu              paragraph       periodcentered  */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      eth             ntilde          ograve          oacute          */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
CASE_BYP_STATE,
};

Const int Tbyptable[] =		/* ESC CAN (^X) bypass state */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BES_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	$		%		&		'	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	(		)		*		+	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	,		-		.		/	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	0		1		2		3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	4		5		6		7	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	8		9		:		;	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	<		=		>		?	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	@		A		B		C	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	D		E		F		G	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	H		I		J		K	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	L		M		N		O	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	P		Q		R		S	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	T		U		V		W	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	X		Y		Z		[	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	\		]		^		_	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	`		a		b		c	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	d		e		f		g	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	h		i		j		k	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	l		m		n		o	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	p		q		r		s	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	t		u		v		w	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	x		y		z		{	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	|		}		~		DEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};

Const int Tesctable[] =		/* ESC */
{
/*	NUL		SOH		STX		ETX	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_VT_MODE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_CURSTATE,
CASE_REPORT,
CASE_CURSTATE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_IGNORE,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_PAGE,
CASE_IGNORE,
CASE_APL,
CASE_ASCII,
/*	DLE		DC1		DC2		DC3	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	DC4		NAK		SYN		ETB	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_COPY,
/*	CAN		EM		SUB		ESC	*/
CASE_BYP_STATE,
CASE_CURSTATE,
CASE_GIN,
CASE_IGNORE,
/*	FS		GS		RS		US	*/
CASE_SPT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	$		%		&		'	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	(		)		*		+	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	,		-		.		/	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	0		1		2		3	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	4		5		6		7	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	8		9		:		;	*/
CASE_CHAR_SIZE,
CASE_CHAR_SIZE,
CASE_CHAR_SIZE,
CASE_CHAR_SIZE,
/*	<		=		>		?	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	@		A		B		C	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	D		E		F		G	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	H		I		J		K	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	L		M		N		O	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	P		Q		R		S	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	T		U		V		W	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	X		Y		Z		[	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	\		]		^		_	*/
CASE_CURSTATE,
CASE_OSC,
CASE_CURSTATE,
CASE_CURSTATE,
/*	`		a		b		c	*/
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
/*	d		e		f		g	*/
CASE_BEAM_VEC,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_BEAM_VEC,
/*	h		i		j		k	*/
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
/*	l		m		n		o	*/
CASE_BEAM_VEC,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_BEAM_VEC,
/*	p		q		r		s	*/
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
CASE_BEAM_VEC,
/*	t		u		v		w	*/
CASE_BEAM_VEC,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_BEAM_VEC,
/*	x		y		z		{	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
CASE_CURSTATE,
/*	|		}		~		DEL	*/
CASE_CURSTATE,
CASE_CURSTATE,
CASE_IGNORE,
CASE_CURSTATE,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};

Const int Tipltable[] =		/* RS (^^) incremental plot */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_ESC_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_PENUP,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	$		%		&		'	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	(		)		*		+	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	,		-		.		/	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	0		1		2		3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	4		5		6		7	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	8		9		:		;	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	<		=		>		?	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	@		A		B		C	*/
CASE_IGNORE,
CASE_IPL_POINT,
CASE_IPL_POINT,
CASE_IGNORE,
/*	D		E		F		G	*/
CASE_IPL_POINT,
CASE_IPL_POINT,
CASE_IPL_POINT,
CASE_IGNORE,
/*	H		I		J		K	*/
CASE_IPL_POINT,
CASE_IPL_POINT,
CASE_IPL_POINT,
CASE_IGNORE,
/*	L		M		N		O	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	P		Q		R		S	*/
CASE_PENDOWN,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	T		U		V		W	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	X		Y		Z		[	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	\		]		^		_	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	`		a		b		c	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	d		e		f		g	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	h		i		j		k	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	l		m		n		o	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	p		q		r		s	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	t		u		v		w	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	x		y		z		{	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	|		}		~		DEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};

Const int Tplttable[] =		/* GS (^]) graph (plot) mode */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_ESC_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	$		%		&		'	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	(		)		*		+	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	,		-		.		/	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	0		1		2		3	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	4		5		6		7	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	8		9		:		;	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	<		=		>		?	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	@		A		B		C	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	D		E		F		G	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	H		I		J		K	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	L		M		N		O	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	P		Q		R		S	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	T		U		V		W	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	X		Y		Z		[	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	\		]		^		_	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	`		a		b		c	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	d		e		f		g	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	h		i		j		k	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	l		m		n		o	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	p		q		r		s	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	t		u		v		w	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	x		y		z		{	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*	|		}		~		DEL	*/
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
CASE_PLT_VEC,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};

Const int Tpttable[] =		/* FS (^\) point plot mode */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_ESC_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	$		%		&		'	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	(		)		*		+	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	,		-		.		/	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	0		1		2		3	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	4		5		6		7	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	8		9		:		;	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	<		=		>		?	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	@		A		B		C	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	D		E		F		G	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	H		I		J		K	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	L		M		N		O	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	P		Q		R		S	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	T		U		V		W	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	X		Y		Z		[	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	\		]		^		_	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	`		a		b		c	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	d		e		f		g	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	h		i		j		k	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	l		m		n		o	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	p		q		r		s	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	t		u		v		w	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	x		y		z		{	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*	|		}		~		DEL	*/
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
CASE_PT_POINT,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};

Const int Tspttable[] =		/* ESC FS (^\) special point plot */
{
/*	NUL		SOH		STX		ETX	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	EOT		ENQ		ACK		BEL	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_BEL,
/*	BS		HT		NL		VT	*/
CASE_BS,
CASE_TAB,
CASE_LF,
CASE_UP,
/*	NP		CR		SO		SI	*/
CASE_IGNORE,
CASE_CR,
CASE_IGNORE,
CASE_IGNORE,
/*	DLE		DC1		DC2		DC3	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	DC4		NAK		SYN		ETB	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*	CAN		EM		SUB		ESC	*/
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_ESC_STATE,
/*	FS		GS		RS		US	*/
CASE_PT_STATE,
CASE_PLT_STATE,
CASE_IPL_STATE,
CASE_ALP_STATE,
/*	SP		!		"		#	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	$		%		&		'	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	(		)		*		+	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	,		-		.		/	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	0		1		2		3	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	4		5		6		7	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	8		9		:		;	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	<		=		>		?	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	@		A		B		C	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	D		E		F		G	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	H		I		J		K	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	L		M		N		O	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	P		Q		R		S	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	T		U		V		W	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	X		Y		Z		[	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	\		]		^		_	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	`		a		b		c	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	d		e		f		g	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	h		i		j		k	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	l		m		n		o	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	p		q		r		s	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	t		u		v		w	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	x		y		z		{	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*	|		}		~		DEL	*/
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
CASE_SPT_POINT,
/*      0x80            0x81            0x82            0x83    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x84            0x85            0x86            0x87    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x88            0x89            0x8a            0x8b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x8c            0x8d            0x8e            0x8f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x90            0x91            0x92            0x93    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x94            0x95            0x96            0x97    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x99            0x99            0x9a            0x9b    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      0x9c            0x9d            0x9e            0x9f    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      nobreakspace    exclamdown      cent            sterling        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      currency        yen             brokenbar       section         */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      diaeresis       copyright       ordfeminine     guillemotleft   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      notsign         hyphen          registered      macron          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      degree          plusminus       twosuperior     threesuperior   */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      acute           mu              paragraph       periodcentered  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      cedilla         onesuperior     masculine       guillemotright  */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      onequarter      onehalf         threequarters   questiondown    */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Agrave          Aacute          Acircumflex     Atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Adiaeresis      Aring           AE              Ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Egrave          Eacute          Ecircumflex     Ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Igrave          Iacute          Icircumflex     Idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Eth             Ntilde          Ograve          Oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ocircumflex     Otilde          Odiaeresis      multiply        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Ooblique        Ugrave          Uacute          Ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      Udiaeresis      Yacute          Thorn           ssharp          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      agrave          aacute          acircumflex     atilde          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      adiaeresis      aring           ae              ccedilla        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      egrave          eacute          ecircumflex     ediaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      igrave          iacute          icircumflex     idiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      eth             ntilde          ograve          oacute          */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      ocircumflex     otilde          odiaeresis      division        */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      oslash          ugrave          uacute          ucircumflex     */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
/*      udiaeresis      yacute          thorn           ydiaeresis      */
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
CASE_IGNORE,
};
