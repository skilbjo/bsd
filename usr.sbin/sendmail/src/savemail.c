# include <pwd.h>
# include "sendmail.h"

SCCSID(@(#)savemail.c	3.50		11/28/82);

/*
**  SAVEMAIL -- Save mail on error
**
**	If mailing back errors, mail it back to the originator
**	together with an error message; otherwise, just put it in
**	dead.letter in the user's home directory (if he exists on
**	this machine).
**
**	Parameters:
**		e -- the envelope containing the message in error.
**
**	Returns:
**		none
**
**	Side Effects:
**		Saves the letter, by writing or mailing it back to the
**		sender, or by putting it in dead.letter in her home
**		directory.
*/

savemail(e)
	register ENVELOPE *e;
{
	register struct passwd *pw;
	register FILE *xfile;
	char buf[MAXLINE+1];
	extern struct passwd *getpwnam();
	register char *p;
	extern char *ttypath();
	typedef int (*fnptr)();

# ifdef DEBUG
	if (tTd(6, 1))
		printf("\nsavemail\n");
# endif DEBUG

	if (bitset(EF_RESPONSE, e->e_flags))
		return;
	if (e->e_class < 0)
	{
		message(Arpa_Info, "Dumping junk mail");
		return;
	}
	ForceMail = TRUE;
	e->e_flags &= ~EF_FATALERRS;

	/*
	**  In the unhappy event we don't know who to return the mail
	**  to, make someone up.
	*/

	if (e->e_from.q_paddr == NULL)
	{
		if (parse("root", &e->e_from, 0) == NULL)
		{
			syserr("Cannot parse root!");
			ExitStat = EX_SOFTWARE;
			finis();
		}
	}
	e->e_to = NULL;

	/*
	**  If called from Eric Schmidt's network, do special mailback.
	**	Fundamentally, this is the mailback case except that
	**	it returns an OK exit status (assuming the return
	**	worked).
	**  Also, if the from address is not local, mail it back.
	*/

	if (ErrorMode == EM_BERKNET)
	{
		ExitStat = EX_OK;
		ErrorMode = EM_MAIL;
	}
	if (!bitset(M_LOCAL, e->e_from.q_mailer->m_flags))
		ErrorMode = EM_MAIL;

	/*
	**  If writing back, do it.
	**	If the user is still logged in on the same terminal,
	**	then write the error messages back to hir (sic).
	**	If not, mail back instead.
	*/

	if (ErrorMode == EM_WRITE)
	{
		p = ttypath();
		if (p == NULL || freopen(p, "w", stdout) == NULL)
		{
			ErrorMode = EM_MAIL;
			errno = 0;
		}
		else
		{
			expand("$n", buf, &buf[sizeof buf - 1], e);
			printf("\r\nMessage from %s...\r\n", buf);
			printf("Errors occurred while sending mail.\r\n");
			if (Xscript != NULL)
			{
				(void) fflush(Xscript);
				xfile = fopen(queuename(e, 'x'), "r");
			}
			else
				xfile = NULL;
			if (xfile == NULL)
			{
				syserr("Cannot open %s", queuename(e, 'x'));
				printf("Transcript of session is unavailable.\r\n");
			}
			else
			{
				printf("Transcript follows:\r\n");
				while (fgets(buf, sizeof buf, xfile) != NULL &&
				       !ferror(stdout))
					fputs(buf, stdout);
				(void) fclose(xfile);
			}
			if (ferror(stdout))
				(void) syserr("savemail: stdout: write err");
		}
	}

	/*
	**  If mailing back, do it.
	**	Throw away all further output.  Don't do aliases, since
	**	this could cause loops, e.g., if joe mails to x:joe,
	**	and for some reason the network for x: is down, then
	**	the response gets sent to x:joe, which gives a
	**	response, etc.  Also force the mail to be delivered
	**	even if a version of it has already been sent to the
	**	sender.
	*/

	if (ErrorMode == EM_MAIL)
	{
		if (e->e_errorqueue == NULL)
			sendto(e->e_from.q_paddr, (ADDRESS *) NULL,
			       &e->e_errorqueue);
		if (returntosender("Unable to deliver mail",
				   e->e_errorqueue, TRUE) == 0)
			return;
	}

	/*
	**  Save the message in dead.letter.
	**	If we weren't mailing back, and the user is local, we
	**	should save the message in dead.letter so that the
	**	poor person doesn't have to type it over again --
	**	and we all know what poor typists programmers are.
	*/

	p = NULL;
	if (e->e_from.q_mailer == LocalMailer)
	{
		if (e->e_from.q_home != NULL)
			p = e->e_from.q_home;
		else if ((pw = getpwnam(e->e_from.q_user)) != NULL)
			p = pw->pw_dir;
	}
	if (p == NULL)
	{
		syserr("Can't return mail to %s", e->e_from.q_paddr);
# ifdef DEBUG
		p = "/usr/tmp";
# endif
	}
	if (p != NULL && TempFile != NULL)
	{
		auto ADDRESS *q;
		bool oldverb = Verbose;

		/* we have a home directory; open dead.letter */
		define('z', p, e);
		expand("$z/dead.letter", buf, &buf[sizeof buf - 1], e);
		Verbose = TRUE;
		message(Arpa_Info, "Saving message in %s", buf);
		Verbose = oldverb;
		e->e_to = buf;
		q = NULL;
		sendto(buf, (ADDRESS *) NULL, &q);
		(void) deliver(e, q);
	}

	/* add terminator to writeback message */
	if (ErrorMode == EM_WRITE)
		printf("-----\r\n");
}
/*
**  RETURNTOSENDER -- return a message to the sender with an error.
**
**	Parameters:
**		msg -- the explanatory message.
**		returnto -- the queue of people to send the message to.
**		sendbody -- if TRUE, also send back the body of the
**			message; otherwise just send the header.
**
**	Returns:
**		zero -- if everything went ok.
**		else -- some error.
**
**	Side Effects:
**		Returns the current message to the sender via
**		mail.
*/

static bool	SendBody;

#define MAXRETURNS	6	/* max depth of returning messages */

returntosender(msg, returnto, sendbody)
	char *msg;
	ADDRESS *returnto;
	bool sendbody;
{
	char buf[MAXNAME];
	extern putheader(), errbody();
	register ENVELOPE *ee;
	extern ENVELOPE *newenvelope();
	ENVELOPE errenvelope;
	static int returndepth;
	register ADDRESS *q;

# ifdef DEBUG
	if (tTd(6, 1))
	{
		printf("Return To Sender: msg=\"%s\", depth=%d, CurEnv=%x,\n",
		       msg, returndepth, CurEnv);
		printf("\treturnto=");
		printaddr(returnto, TRUE);
	}
# endif DEBUG

	if (++returndepth >= MAXRETURNS)
	{
		if (returndepth != MAXRETURNS)
			syserr("returntosender: infinite recursion on %s", returnto->q_paddr);
		/* don't "unrecurse" and fake a clean exit */
		/* returndepth--; */
		return (0);
	}

	SendBody = sendbody;
	define('g', "$f", CurEnv);
	ee = newenvelope(&errenvelope);
	ee->e_puthdr = putheader;
	ee->e_putbody = errbody;
	ee->e_flags |= EF_RESPONSE;
	ee->e_sendqueue = returnto;
	(void) queuename(ee, '\0');
	for (q = returnto; q != NULL; q = q->q_next)
	{
		if (q->q_alias == NULL)
			addheader("to", q->q_paddr, ee);
	}
	addheader("subject", msg, ee);

	/* fake up an address header for the from person */
	expand("$n", buf, &buf[sizeof buf - 1], CurEnv);
	if (parse(buf, &ee->e_from, -1) == NULL)
	{
		syserr("Can't parse myself!");
		ExitStat = EX_SOFTWARE;
		returndepth--;
		return (-1);
	}

	/* push state into submessage */
	CurEnv = ee;
	define('f', "$n", ee);
	define('x', "Mail Delivery Subsystem", ee);

	/* actually deliver the error message */
	sendall(ee, SendMode);

	/* restore state */
	dropenvelope(ee);
	CurEnv = CurEnv->e_parent;
	returndepth--;

	/* should check for delivery errors here */
	return (0);
}
/*
**  ERRBODY -- output the body of an error message.
**
**	Typically this is a copy of the transcript plus a copy of the
**	original offending message.
**
**	Parameters:
**		xfile -- the transcript file.
**		fp -- the output file.
**		xdot -- if set, use the SMTP hidden dot algorithm.
**
**	Returns:
**		none
**
**	Side Effects:
**		Outputs the body of an error message.
*/

errbody(fp, m, xdot)
	register FILE *fp;
	register struct mailer *m;
	bool xdot;
{
	register FILE *xfile;
	char buf[MAXLINE];
	bool fullsmtp = bitset(M_FULLSMTP, m->m_flags);
	char *p;

	/*
	**  Output transcript of errors
	*/

	(void) fflush(stdout);
	p = queuename(CurEnv->e_parent, 'x');
	if ((xfile = fopen(p, "r")) == NULL)
	{
		syserr("Cannot open %s", p);
		fprintf(fp, "  ----- Transcript of session is unavailable -----\n");
	}
	else
	{
		fprintf(fp, "   ----- Transcript of session follows -----\n");
		if (Xscript != NULL)
			(void) fflush(Xscript);
		while (fgets(buf, sizeof buf, xfile) != NULL)
			putline(buf, fp, fullsmtp);
		(void) fclose(xfile);
	}
	errno = 0;

	/*
	**  Output text of original message
	*/

	if (NoReturn)
		fprintf(fp, "\n   ----- Return message suppressed -----\n\n");
	else if (TempFile != NULL)
	{
		if (SendBody)
		{
			fprintf(fp, "\n   ----- Unsent message follows -----\n");
			(void) fflush(fp);
			putheader(fp, m, CurEnv->e_parent);
			fprintf(fp, "\n");
			putbody(fp, m, xdot);
		}
		else
		{
			fprintf(fp, "\n  ----- Message header follows -----\n");
			(void) fflush(fp);
			putheader(fp, m, CurEnv->e_parent);
		}
	}
	else
		fprintf(fp, "\n  ----- No message was collected -----\n\n");

	/*
	**  Cleanup and exit
	*/

	if (errno != 0)
		syserr("errbody: I/O error");
}
