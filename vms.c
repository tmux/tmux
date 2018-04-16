/* $XTermId: vms.c,v 1.13 2010/10/11 08:05:35 tom Exp $ */

/*  vms.c
 *
 * This module contains the VMS version of the routine SPAWN (from the module
 * MAIN.C) and the routines that do IO to the pseudo terminal.
 *
 * Modification History:
 * Stephan Jansen 1-Mar-1990  Original version
 * Hal R. Brand   5-Sep-1990  Added code to propagate DECW$DISPLAY
 * Aaron Leonard 11-Sep-1990  Fix string descriptor lengths
 * Stephan Jansen 2-Dec-1991  Modify to use new Pseudo terminal drivers
 *                            (patterned after photo.c by Forrest A. Kenney)
 * Patrick Mahan  7-Jan-1991  Removed reference to <dvidef.h> from VMS.C
 *			      Forced device type to be VT102 since that is
 *			      what we are emulating.
 */

#include <libdef.h>
#include <lnmdef.h>

#include <stdio.h>
#include <string.h>

#include "xterm.h"
#include "data.h"
#include "vms.h"

#define PTD$C_SEND_XON		0	/* Pseudo Terminal Driver event      */
#define PTD$C_SEND_BELL		1
#define PTD$C_SEND_XOFF 	2
#define PTD$C_STOP_OUTPUT	3
#define PTD$C_RESUME_OUTPUT 	4
#define PTD$C_CHAR_CHANGED 	5
#define PTD$C_ABORT_OUTPUT 	6
#define PTD$C_START_READ 	7
#define PTD$C_MIDDLE_READ 	8
#define PTD$C_END_READ 		9
#define PTD$C_ENABLE_READ 	10
#define PTD$C_DISABLE_READ 	11
#define PTD$C_MAX_EVENTS 	12

#define	BUFFERS		        6
#define	PAGE			512

typedef struct	tt_buffer
{
unsigned int	flink;
unsigned int	blink;
short	int	status;
short	int	length;
char		data[VMS_TERM_BUFFER_SIZE];
} TT_BUF_STRUCT;

TT_BUF_STRUCT		*tt_w_buff;
struct	q_head		 _align(QUADWORD)	buffer_queue = (0,0);
struct	q_head		 _align(QUADWORD)	read_queue = (0,0);

static char          tt_name[64];
static $DESCRIPTOR   (tt_name_desc, &tt_name);

static char          ws_name[64];
static $DESCRIPTOR   (ws_name_desc, &ws_name);

static struct        tt_char {
   char        class;
   char        type;
   short int   page_width;
   char        characteristics[3];
   char        length;
   int         extended;
 } tt_mode, tt_chars, orig_tt_chars;

struct mem_region
{
  TT_BUF_STRUCT *start;
  TT_BUF_STRUCT *end;
} ret_addr;

int read_stopped = False;
int write_stopped = False;

int tt_width;
int tt_length;
int tt_changed;
int tt_pasting=False;         /* drm */
int tt_new_output=False;      /* Cleared by flushlog(), set whenever something new
   goes to the screen through tt_write */

int trnlnm(char *in,int id,char *out);
void spawn (void);

static void tt_echo_ast(TT_BUF_STRUCT *buff_addr);
static void tt_read_ast(TT_BUF_STRUCT *buff_addr);

/*
static void tt_start_read(void);
*/
void tt_start_read(void);
int tt_read(char *buffer);
static void send_xon(void);
static void send_xoff(void);
static void send_bell(void);
static void char_change(void);
static void freeBuff (TT_BUF_STRUCT *buff_addr);
TT_BUF_STRUCT *getBuff(void);
static void CloseDown(int exit_status);
static void mbx_read_ast(void);
static void mbx_read(void);



#define DESCRIPTOR(name,string) struct dsc$descriptor_s name = \
{ strlen(string), DSC$K_DTYPE_T, DSC$K_CLASS_S, string }

int trnlnm(char *in, int id, char *out)
{
  int status, num, len, attr = LNM$M_CASE_BLIND, foo = id;
  short outlen;
  struct itemlist
    {
      short buffer_length;
      short item_code;
      char  *buffer_addr;
      int   *return_length;
    } itmlst[] =
      {
	4  , LNM$_INDEX    , &foo, 0,
	255, LNM$_STRING   , out , &outlen,
	4  , LNM$_MAX_INDEX, &num, &len,
	0  , 0
	};
  DESCRIPTOR(lognam,in);
  DESCRIPTOR(tabnam,"LNM$DCL_LOGICAL");

  status = sys$trnlnm(&attr,&tabnam,&lognam,0,itmlst);
  if(status != SS$_NORMAL) return(-1);   /* error status */
  out[outlen] = 0;         /* terminate the output string */
  return(++num);         /* return number of translations */
}

static int           pty;
static int           Xsocket;

void spawn (void)
{
  int                  status;
  static $DESCRIPTOR   (dtime, "0 00:00:00.01");
  static int           delta[2];
  register TScreen     *screen = TScreenOf(term);
  static struct IOSB   iosb;
  static unsigned int  flags;
  static unsigned int  uic;
  static char          imagename[64];
  static int           privs;
  static $DESCRIPTOR(device, "FTA0:");
  static int           type;
  static int           class;
  static int           devdepend;
  static int           mem_size;
  int                  i;

  /* if pid and mbx_chan are nonzero then close them in CloseDown() */
  pid = 0;
  mbx_chan = 0;

  status = SYS$EXPREG (BUFFERS, &ret_addr, 0, 0);
  if(!(status & SS$_NORMAL)) lib$signal(status);

  tt_w_buff = (char *)ret_addr.end - PAGE + 1;

  /* use one buffer for writing, the reset go in the free buffer queue */
  for(i=0; i < BUFFERS-1; i++)
    {
      freeBuff((char *)ret_addr.start +i*PAGE);
    }

  /* avoid double MapWindow requests, for wm's that care... */
  XtSetMappedWhenManaged( screen->TekEmu ? XtParent(tekWidget) :
			 XtParent(term), False );
  /* Realize the Tek or VT widget, depending on which mode we're in.
     If VT mode, this calls VTRealize (the widget's Realize proc) */
  XtRealizeWidget (screen->TekEmu ? XtParent(tekWidget) :
		   XtParent(term));

  /* get the default device characteristics of the pseudo terminal */

  itemlist[0].buflen      = 4;
  itemlist[0].code        = DVI$_DEVTYPE;
  itemlist[0].buffer      = &type;
  itemlist[0].return_addr = &tt_name_desc.dsc$w_length;

  itemlist[1].buflen      = 4;
  itemlist[1].code        = DVI$_DEVCLASS;
  itemlist[1].buffer      = &class;
  itemlist[1].return_addr = &tt_name_desc.dsc$w_length;

  itemlist[2].buflen      = 4;
  itemlist[2].code        = DVI$_DEVDEPEND;
  itemlist[2].buffer      = &devdepend;
  itemlist[2].return_addr = &tt_name_desc.dsc$w_length;

  itemlist[3].buflen      = 4;
  itemlist[3].code        = DVI$_DEVDEPEND2;
  itemlist[3].buffer      = &tt_chars.extended;
  itemlist[3].return_addr = &tt_name_desc.dsc$w_length;

  itemlist[4].buflen      = 0;
  itemlist[4].code        = 0;


  status = sys$getdviw(0,0,&device,&itemlist,&iosb,0,0,0);
  if(!(status & SS$_NORMAL)) lib$signal(status);
  if(!(iosb.status & SS$_NORMAL)) lib$signal(iosb.status);

  tt_chars.type        = DT$_VT102; /* XTerm supports VT102 mode */
  tt_chars.class       = class;
  tt_chars.page_width  = screen->max_col+1;
  tt_chars.length      = screen->max_row+1;

  /* copy the default char's along with the created window size */

  bcopy(&devdepend, &tt_chars.characteristics, 3);

  tt_chars.extended |= TT2$M_ANSICRT | TT2$M_AVO | TT2$M_DECCRT;


  /* create the pseudo terminal with the proper char's */
  status = ptd$create(&tt_chan,0,&tt_chars,12,0,0,0,&ret_addr);
  if(!(status & SS$_NORMAL)) lib$signal(status);


  /* get the device name of the Pseudo Terminal */

  itemlist[0].buflen      = 64;
  itemlist[0].code        = DVI$_DEVNAM;
  itemlist[0].buffer      = &tt_name;
  itemlist[0].return_addr = &tt_name_desc.dsc$w_length;

  /* terminate the list */
  itemlist[1].buflen      = 0;
  itemlist[1].code        = 0;

  status = sys$getdviw(0,tt_chan,0,&itemlist,&iosb,0,0,0);
  if(!(status & SS$_NORMAL)) CloseDown(status);
  if(!(iosb.status & SS$_NORMAL)) CloseDown(iosb.status);

  /*
   * set up AST's for XON, XOFF, BELL and characteristics change.
   */

  status = ptd$set_event_notification(tt_chan,&send_xon,0,0,PTD$C_SEND_XON);
  if(!(status & SS$_NORMAL)) CloseDown(status);

  status = ptd$set_event_notification(tt_chan,&send_xoff,0,0,PTD$C_SEND_XOFF);
  if(!(status & SS$_NORMAL)) CloseDown(status);

  status = ptd$set_event_notification(tt_chan,&send_bell,0,0,PTD$C_SEND_BELL);
  if(!(status & SS$_NORMAL)) CloseDown(status);

  status = ptd$set_event_notification(tt_chan,&char_change,0,0,PTD$C_CHAR_CHANGED);
  if(!(status & SS$_NORMAL)) CloseDown(status);

  /* create a mailbox for the detached process to detect hangup */

  status = sys$crembx(0,&mbx_chan,ACC$K_TERMLEN,0,255,0,0);
  if(!(status & SS$_NORMAL)) CloseDown(status);


  /*
   * get the device unit number for created process completion
   * status to be sent to.
   */

  itemlist[0].buflen      = 4;
  itemlist[0].code        = DVI$_UNIT;
  itemlist[0].buffer      = &mbxunit;
  itemlist[0].return_addr = 0;

  /* terminate the list */
  itemlist[1].buflen      = 0;
  itemlist[1].code        = 0;

  status = sys$getdviw(0,mbx_chan,0,&itemlist,&iosb,0,0,0);
  if(!(status & SS$_NORMAL)) CloseDown(status);
  if(!(iosb.status & SS$_NORMAL)) CloseDown(iosb.status);


  tt_start_read();

  /*
   * find the current process's UIC so that it can be used in the
   * call to sys$creprc
   */
  itemlist[0].buflen = 4;
  itemlist[0].code = JPI$_UIC;
  itemlist[0].buffer = &uic;
  itemlist[0].return_addr = 0;

  /* terminate the list */
  itemlist[1].buflen      = 0;
  itemlist[1].code        = 0;

  status = sys$getjpiw(0,0,0,&itemlist,0,0,0);
  if(!(status & SS$_NORMAL)) CloseDown(status);

  /* Complete a descriptor for the WS (DECW$DISPLAY) device */

  trnlnm("DECW$DISPLAY",0,ws_name);
  ws_name_desc.dsc$w_length = strlen(ws_name);

  /* create the process */
  /*  Set sys$error to be the WS (DECW$DISPLAY) device. LOGINOUT  */
  /*  has special code for DECWINDOWS that will:                  */
  /*    1) do a DEFINE/JOB DECW$DISPLAY 'f$trnlnm(sys$error)'     */
  /*    2) then redefine SYS$ERROR to match SYS$OUTPUT!           */
  /*  This will propogate DECW$DISPLAY to the XTERM process!!!    */
  /*  Thanks go to Joel M Snyder who posted this info to INFO-VAX */

  flags = PRC$M_INTER | PRC$M_NOPASSWORD | PRC$M_DETACH;
  status = sys$creprc(&pid,&image,&tt_name_desc,&tt_name_desc,
		      &ws_name_desc,0,0,0,4,uic,mbxunit,flags);
  if(!(status & SS$_NORMAL)) CloseDown(status);


  /* hang a read on the mailbox waiting for completion */
  mbx_read();


/* set time value and schedule a periodic wakeup (every 1/100 of a second)
 * this is used to prevent the controlling process from using up all the
 * CPU.  The controlling process will hibernate at strategic points in
 * the program when it is just waiting for input.
 */

  status = sys$bintim(&dtime,&delta);
  if (!(status & SS$_NORMAL)) CloseDown(status);

  status = sys$schdwk(0,0,&delta,&delta);
  if (!(status & SS$_NORMAL)) CloseDown(status);


  /*
   * This is rather funky, but it saves me from having to totally
   * rewrite some parts of the code (namely in_put in module CHARPROC.C)
   */
  pty = 1;
  screen->respond = pty;
  pty_mask = 1 << pty;
  Select_mask = pty_mask;
  X_mask = 1 << Xsocket;

}


/*
 * This routine handles completion of write with echo.  It takes the
 * echo buffer and puts it on the read queue.  It will then be processed
 * by the routine tt_read.  If the echo buffer is empty, it is put back
 * on the free buffer queue.
 */

static void tt_echo_ast(TT_BUF_STRUCT *buff_addr)
{
  int status;

  if (buff_addr->length != 0)
    {
      status = LIB$INSQTI(buff_addr, &read_queue);
      if((status != SS$_NORMAL) && (status != LIB$_ONEENTQUE))
	{
	  CloseDown(status);
	}
    }
  else
    {
      freeBuff(buff_addr);
    }
}


/*
 * This routine writes to the pseudo terminal.  If there is a free
 * buffer then write with an echo buffer completing asyncronously, else
 * write syncronously using the buffer reserved for writing.  All errors
 *  are fatal, except DATAOVERUN and DATALOST,these errors can be ignored.

 CAREFUL! Whatever calls this must NOT pass more than VMS_TERM_BUFFER_SIZE
 bytes at a time.  This definition has been moved to VMS.H

 */

int tt_write(const char *tt_write_buf, int size)
{
  int status;
  TT_BUF_STRUCT *echoBuff;

  /* if writing stopped, return 0 until Xon */
  if(write_stopped) return (0);

  memmove(&tt_w_buff->data,tt_write_buf,size);

  echoBuff = getBuff();
  if (echoBuff != LIB$_QUEWASEMP)
    {
      status = PTD$WRITE (tt_chan, &tt_echo_ast, echoBuff,
			  &tt_w_buff->status, size,
			  &echoBuff->status, VMS_TERM_BUFFER_SIZE);
    }
  else
    {
      status = PTD$WRITE (tt_chan, 0, 0, &tt_w_buff->status, size, 0, 0);
    }
  if (status & SS$_NORMAL)
    {
      if ((tt_w_buff->status != SS$_NORMAL) &&
	  (tt_w_buff->status != SS$_DATAOVERUN) &&
	  (tt_w_buff->status != SS$_DATALOST))
	{
	  CloseDown(tt_w_buff->status);
	}
    }
  else
    {
      CloseDown(status);
    }

  return(size);
}


/*
 * This routine is called when a read to the pseudo terminal completes.
 * Put the newly read buffer onto the read queue.  It will be processed
 * and freed in the routine tt_read.
 */

static void tt_read_ast(TT_BUF_STRUCT *buff_addr)
{
  int status;

  if (buff_addr->status & SS$_NORMAL)
    {
      status = LIB$INSQTI(buff_addr, &read_queue);
      if ((status != SS$_NORMAL) && (status != LIB$_ONEENTQUE))
	{
	  CloseDown(status);
	}
    }
  else
    CloseDown(buff_addr->status);

  tt_start_read();
  sys$wake(0,0);
  return;
}


/*
 * If there is a free buffer on the buffer queue then Start a read from
 * the pseudo terminal, otherwise set a flag, the reading will be restarted
 * in the routine freeBuff when a buffer is freed.
 */

void tt_start_read(void)
{
  int status;
  static int size;
  TT_BUF_STRUCT *buff_addr;

  buff_addr = getBuff();
  if (buff_addr != LIB$_QUEWASEMP)
    {
      if(!tt_pasting){
      status = PTD$READ (0, tt_chan, &tt_read_ast, buff_addr,
			 &buff_addr->status, VMS_TERM_BUFFER_SIZE);
      if ((status & SS$_NORMAL) != SS$_NORMAL)
	{
	  CloseDown(status);
	}
      }
      }
  else
    {
      read_stopped = True;
    }
  return;
}


/*
 * Get data from the pseudo terminal.  Return the data from the first item
 * on the read queue, and put that buffer back onto the free buffer queue.
 * Return the length or zero if the read queue is empty.
 *
 */

int tt_read(char *buffer)
{
  TT_BUF_STRUCT *read_buff;
  int status;
  int len;

   status = LIB$REMQHI(&read_queue, &read_buff);
   if(status == LIB$_QUEWASEMP){
     return(0);
   }
   else if (status & SS$_NORMAL)
     {
       len = read_buff->length;
       memmove(buffer,&read_buff->data,len);
       freeBuff(read_buff);
       tt_new_output=True; /* DRM something will be written */
     }
   else
     CloseDown(status);

   return(len);
}


/*
 * if xon then it is safe to start writing again.
 */

static void send_xon(void)
{
  write_stopped = False;
}


/*
 * If Xoff then stop writing to the pseudo terminal until you get Xon.
 */
static void send_xoff(void)
{
  write_stopped = True;
}



/*
 * Beep the terminal to let the user know data will be lost because
 * of too much data.
 */

static void send_bell(void)
{
   Bell(term);
}

/*
 * if the pseudo terminal's characteristics change, check to see if the
 * page size changed.  If it did, resize the widget, otherwise, ignore
 * it!  This routine just gets the new term dimensions and sets a flag
 * to indicate the term chars have changed.  The widget gets resized in
 * the routine in_put in the module CHARPROC.C.  You cant resize the
 * widget in this routine because this is an AST and X is not reenterent.
 */

static void char_change(void)
{
  int status;

  /*
   * Dont do anything if in Tek mode
   */

  if(!(TScreenOf(term)->TekEmu))
    {
      status = sys$qiow(0,tt_chan,IO$_SENSEMODE,0,0,0,&tt_mode,8,0,0,0,0);
      if(!(status & SS$_NORMAL)) CloseDown(status);

      if((TScreenOf(term)->max_row != tt_mode.length) ||
	 (TScreenOf(term)->max_col != tt_mode.page_width))
	{
	  tt_length = tt_mode.length;
	  tt_width =  tt_mode.page_width;

	  tt_changed = True;

	}
    }
}


/*
 * Put a free buffer back onto the buffer queue.  If reading was
 * stopped for lack of free buffers, start reading again.
 */

static void freeBuff (TT_BUF_STRUCT *buff_addr)
{
  int ast_stat;
  int status;

  ast_stat = SYS$SETAST(0);
  if (!read_stopped)
    {
      LIB$INSQHI(buff_addr, &buffer_queue);
    }
  else
    {
      status = PTD$READ (0, tt_chan, &tt_read_ast, buff_addr,
			 &buff_addr->status, VMS_TERM_BUFFER_SIZE);
      if (status & SS$_NORMAL)
	{
	  read_stopped = False;
	}
      else
	{
	  CloseDown(status);
	}
    }
  if (ast_stat == SS$_WASSET) ast_stat = SYS$SETAST(1);
}


/*
 * return a free buffer from the buffer queue.
 */

TT_BUF_STRUCT *getBuff(void)
{
  int status;
  TT_BUF_STRUCT *buff_addr;

  status = LIB$REMQHI(&buffer_queue, &buff_addr);
  if (status & SS$_NORMAL)
    {
      return(buff_addr);
    }
  else
    {
      return(status);
    }
}


/*
 * Close down and exit.  Kill the detached process (if it still
 * exists), deassign mailbox channell (if assigned), cancel any
 * waiting IO to the pseudo terminal and delete it, exit with any
 * status information.
 */

static void CloseDown(int exit_status)
{
  int status;

  /* if process has not terminated, do so now! */
  if(pid != 0)
    {
      status = sys$forcex(&pid,0,0);
      if(!(status & SS$_NORMAL)) lib$signal(status);
    }

  /* if mbx_chan is assigned, deassign it */
  if(mbx_chan != 0)
    {
      sys$dassgn(mbx_chan);
    }

  /* cancel pseudo terminal IO requests */
  status = ptd$cancel(tt_chan);
  if(!(status & SS$_NORMAL)) lib$signal(status);

  /* delete pseudo terminal */
  status = ptd$delete(tt_chan);
  if(!(status & SS$_NORMAL)) lib$signal(status);

  if(!(exit_status & SS$_NORMAL)) lib$signal(exit_status);

  exit(1);

}


/*
 * This routine gets called when the detached process terminates (for
 * whatever reason).  The mailbox buffer has final exit status.  Close
 * down and exit.
 */

static void mbx_read_ast(void)
{
  int status;

  pid = 0;

  status = mbx_read_iosb.status;
  if (!(status & SS$_NORMAL)) CloseDown(status);

  status = (unsigned long int) mbx_buf.acc$l_finalsts;
  if (!(status & SS$_NORMAL)) CloseDown(status);

  CloseDown(1);

}


/*
 * This routine starts a read on the mailbox associated with the detached
 * process.  The AST routine gets called when the detached process terminates.
 */

static void mbx_read(void)
{
int status;
static int size;

   size = ACC$K_TERMLEN;
   status = sys$qio(0,mbx_chan,
          IO$_READVBLK,
          &mbx_read_iosb,
          &mbx_read_ast,
          0,
          &mbx_buf,
          size,0,0,0,0);

   if (!(status & SS$_NORMAL)) CloseDown(status);

   return;
}
