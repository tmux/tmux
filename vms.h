/* $XFree86: xc/programs/xterm/vms.h,v 1.1 2000/02/08 17:19:45 dawes Exp $ */

/* vms.h
 */
#include <ssdef.h>
#include <iodef.h>
#include <msgdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <jpidef.h>
#include <prcdef.h>
#include <dcdef.h>
#include <ttdef.h>
#include <tt2def.h>
#include <accdef.h>
#include <prvdef.h>

struct IOSB
{
	short int status;
	short int len;
	int unused;
} mbx_read_iosb,iosb;

#define MAXITEMLIST   5

short int	tt_chan;    /* channel to the Pseudo terminal */
short int	mbx_chan;   /* channel to the mailbox */
struct accdef	mbx_buf;    /* mailbox buffer */
short int	mbxunit;    /* mailbox unit number */
int		pid;		/* PID of created process */
static $DESCRIPTOR  (image, "SYS$SYSTEM:LOGINOUT.EXE");

static struct	    items {
	short int	buflen;
	short int	code;
	int		buffer;
	int		return_addr;
} itemlist[MAXITEMLIST];

int tt_write(const char *tt_write_buf,int size);
