//  -----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2009 Fons Adriaensen <fons@kokkinizita.net>
//  
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//  -----------------------------------------------------------------------------


#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include "impdata.h"
#include "config.h"


#define BSIZE  0x4000


static int check1 (unsigned int ip, unsigned int op)
{
    if (! size) return ERR_NOCONV;
    if ((ip < 1) || (ip > ninp)) return ERR_IONUM;
    if ((op < 1) || (op > nout)) return ERR_IONUM;
    return 0;
}


static int read_sndfile (const char *line, int lnum, const char *cdir, unsigned int latency)
{
    unsigned int  ip1, op1;
    float         gain;
    unsigned int  delay;
    unsigned int  offset;
    unsigned int  length;
    unsigned int  ichan, nchan; 
    int           ifram, nfram, err;
    char          file [1024];
    char          path [1024];
    Impdata       impd;
    float         *buff, *p;

    if (sscanf (line, "%u %u %f %u %u %u %u %s",
                &ip1, &op1, &gain, &delay, &offset, &length, &ichan, file) != 8) return ERR_PARAM;

    printf("reading file %s\n", file);

    if (latency)
    {
	if (delay >= latency)
	{
	    delay -= latency;
	}
	else 
	{
	    latency -= delay;
	    delay = 0;
	    offset += latency;
	    fprintf (stderr, "Warning: line %d: first %d frames removed by latency compensation,\n",
                     lnum, latency);
	}
    }		
    err = check1 (ip1, op1);
    if (err) return err;

    if (*file == '/') strcpy (path, file);
    else
    {
        strcpy (path, cdir);
        strcat (path, "/");
        strcat (path, file);
    }

    if (impd.sf_open_read (path))
    {
        fprintf (stderr, "Unable to open '%s'\n", path);
        return ERR_OTHER;
    } 

    if (impd.rate_n () != (unsigned int) rate)
    {
        fprintf (stderr, "Warning: sample rate (%d) of '%s' does not match.\n",
                 impd.rate_n (), path);
    }

    nchan = impd.n_chan ();
    nfram = impd.n_fram ();
    if ((ichan < 1) || (ichan > nchan))
    {
	fprintf (stderr, "Channel not available in line %d\n", lnum);
        impd.close ();
        return ERR_OTHER;
    } 
    if (offset && impd.seek_ext (offset))
    {
	fprintf (stderr, "Can't seek to offset in line %d\n", lnum);
        impd.close ();
        return ERR_OTHER;
    } 
    if (! length) length = nfram - offset;
    if (length > size - delay) 
    {
	length = size - delay;
   	fprintf (stderr, "Warning: data truncated in line %d\n", lnum);
    }

    try 
    {
        buff = new float [BSIZE * nchan];
    }
    catch (...)
    {
	impd.close ();
        return ERR_ALLOC;
    }

    while (length)
    {
	nfram = (length > BSIZE) ? BSIZE : length;
	nfram = impd.read_ext (nfram, buff);
	if (nfram < 0)
	{
	    fprintf (stderr, "Error reading file in line %d\n", lnum);
	    impd.close ();
	    delete[] buff;
	    return ERR_OTHER;
	}
	if (nfram)
	{
	    p = buff + ichan - 1;
	    for (ifram = 0; ifram < nfram; ifram++) p [ifram * nchan] *= gain;
            if (conv->impdata_create (ip1 - 1, op1 - 1, nchan, p, delay, delay + nfram))
            {
	        impd.close ();
                delete[] buff;
	        return ERR_ALLOC;
            }
	    delay  += nfram;
	    length -= nfram;
	}
    }

    impd.close ();
    delete[] buff;
    return 0;
}


static int impdirac (const char *buff, unsigned int latency)
{
    unsigned int  ip1, op1;
    unsigned int  delay;
    float         gain;
    int           stat;

    if (sscanf (buff, "%u %u %f %u", &ip1, &op1, &gain, &delay) != 4) return ERR_PARAM;

    stat = check1 (ip1, op1);
    if (stat) return stat;

    if (delay < latency)
    {
	fprintf (stderr, "Warning: dirac pulse removed: delay is smaller than latency.\n");
	return 0;
    }
    delay -= latency;

    if (delay < size)
    {
	if (conv->impdata_create (ip1 - 1, op1 - 1, 1, &gain, delay, delay + 1))
	{
	    return ERR_ALLOC;
	}
    }
    return 0;
}


static int impcopy (const char *buff)
{
    unsigned int ip1, op1, ip2, op2;
    int          stat;

    if (sscanf (buff, "%u %u %u %u", &ip1, &op1, &ip2, &op2) != 4) return ERR_PARAM;

    stat = check1 (ip1, op1) | check1 (ip2, op2);
    if (stat) return stat;

    if ((ip1 != ip2) || (op1 != op2))
    {
        if (conv->impdata_copy (ip2 - 1, op2 - 1, ip1 - 1, op1 - 1))
	{
	    return ERR_ALLOC;
	}
    }
    return 0;
}


int config (const char *config, unsigned int latency)
{
    FILE          *F;
    int           stat;
    int           lnum;
    char          buff [1024];
    char          cdir [1024];
    char          *p, *q;

    if (! (F = fopen (config, "r"))) 
    {
	fprintf (stderr, "Can't open '%s' for reading\n", config);
        return -1;
    } 

    strcpy (cdir, dirname ((char *) config));
    stat = 0;
    lnum = 0;

    while (! stat && fgets (buff, 1024, F))
    {
        lnum++;
        p = buff; 
        if (*p != '/')
	{
            while (isspace (*p)) p++;
            if ((*p > ' ') && (*p != '#'))
	    {
                stat = ERR_SYNTAX;
		break;
	    }
            continue;
	}
        for (q = p; (*q >= ' ') && !isspace (*q); q++);
        for (*q++ = 0; (*q >= ' ') && isspace (*q); q++);

        if (! strcmp (p, "/cd"))
        {
            if (sscanf (q, "%s", cdir) != 1) stat = ERR_PARAM;
        }	
        else if (! strcmp (p, "/convolver/new")) stat = convnew (q);
        else if (! strcmp (p, "/impulse/read"))  stat = read_sndfile (q, lnum, cdir, latency);
        else if (! strcmp (p, "/impulse/dirac")) stat = impdirac (q, latency); 
        else if (! strcmp (p, "/impulse/copy"))  stat = impcopy (q); 
        else if (! strcmp (p, "/input/name"))    stat = inpname (q); 
        else if (! strcmp (p, "/output/name"))   stat = outname (q); 
        else stat = ERR_COMMAND;
    }

    fclose (F);
    if (stat)
    {
	switch (stat)
	{
	case ERR_SYNTAX:
	    fprintf (stderr, "Syntax error in line %d\n", lnum);
	    break;
	case ERR_PARAM:
	    fprintf (stderr, "Bad or missing parameters in line %d\n", lnum);
	    break;
	case ERR_ALLOC:
	    fprintf (stderr, "Out of memory in line %d\n", lnum);
	    break;
	case ERR_CANTCD:
	    fprintf (stderr, "Can't change dir to %s\n", cdir);
	    break;
	case ERR_COMMAND:
	    fprintf (stderr, "Unknown command in line %d\n", lnum);
	    break;
	case ERR_NOCONV:
	    fprintf (stderr, "No convolver yet defined in line %d\n", lnum);
	    break;
	case ERR_IONUM:
	    fprintf (stderr, "Bad input or output number in line %d\n", lnum);
	    break;
	}
    }

    return stat;
}

