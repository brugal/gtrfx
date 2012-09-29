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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include "jclient.h"
#include "config.h"


//extern Jclient     *jclient;
extern unsigned int frag;
extern unsigned int part;


static char *inp_name [Convproc::MAXINP];
static char *out_name [Convproc::MAXOUT];
static char *inp_conn [Convproc::MAXINP];
static char *out_conn [Convproc::MAXOUT];


int convnew (const char *line)
{
    memset (inp_name, 0, Convproc::MAXINP * sizeof (char *));
    memset (inp_conn, 0, Convproc::MAXINP * sizeof (char *));
    memset (out_name, 0, Convproc::MAXOUT * sizeof (char *));
    memset (out_conn, 0, Convproc::MAXOUT * sizeof (char *));

    if (sscanf (line, "%u %u %u %u", &ninp, &nout, &part, &size) != 4) return ERR_PARAM;
    if ((ninp == 0) || (ninp > Convproc::MAXINP))
    {
        fprintf (stderr, "Number of inputs (%d) is out of range\n", ninp);
        return ERR_OTHER;
    }
    if ((nout == 0) || (nout > Convproc::MAXOUT))
    {
        fprintf (stderr, "Number of outputs (%d) is out of range\n", nout);
        return ERR_OTHER;
    }
    if  ((part & (part -1)) || (part < Convproc::MINPART) || (part > Convproc::MAXPART))
    {
        fprintf (stderr, "Partition size (%d) is illegal\n", part);
        return ERR_OTHER;
    }
    if (part > Convproc::MAXDIVIS * frag)
    {
        part = Convproc::MAXDIVIS * frag; 
        fprintf (stderr, "Warning: partition size adjusted to %d\n", part);
    }
    if (part < frag)
    {
        part = frag; 
        fprintf (stderr, "Warning: partition size adjusted to %d\n", part);
    }
    if (size > MAXSIZE)
    {
        fprintf (stderr, "Convolver size (%d) is out of range\n", size);
        return ERR_OTHER;
    }

    if (conv->configure (ninp, nout, size, frag, part, Convproc::MAXPART))
    {   
        fprintf (stderr, "Can't initialise convolver engine\n");
        fprintf(stderr, "ninp %d, nout %d, size %d, frag %d, part %d, Convproc::MAXPART %d\n", ninp, nout, size, frag, part, Convproc::MAXPART);
        return ERR_OTHER;
    }

    return 0;
}


int inpname (const char *line)
{
#if 0
    unsigned int  i, r;
    char          s1 [256];
    char          s2 [256];

    r = sscanf (line, "%u %s %s", &i, s1, s2);
    if (r  < 2) return ERR_PARAM;
    if (--i >= ninp) return ERR_IONUM;
    inp_name [i] = strdup (s1);
    if (r > 2) inp_conn [i] = strdup (s2);
#endif

    return 0;
}


int outname (const char *line)
{
#if 0
    unsigned int  i, r;
    char          s1 [256];
    char          s2 [256];

    r = sscanf (line, "%u %s %s", &i, s1, s2);
    if (r  < 2) return ERR_PARAM;
    if (--i >= nout) return ERR_IONUM;
    out_name [i] = strdup (s1);
    if (r > 2) out_conn [i] = strdup (s2);
#endif

    return 0;
}


#if 0
void makeports (void)
{
    unsigned int  i;
    char          s [16];

    for (i = 0; i < ninp; i++)
    {
	if (inp_name [i])
	{
            jclient->add_input_port (inp_name [i], inp_conn [i]);
	}
	else
	{
            sprintf (s, "In-%d", i + 1);
            jclient->add_input_port (s);
	}
	free (inp_name [i]);
	free (inp_conn [i]);
    }
    for (i = 0; i < nout; i++)
    {
	if (out_name [i])
	{
            jclient->add_output_port (out_name [i], out_conn [i]);
	}
	else
	{
            sprintf (s, "Out-%d", i + 1);
            jclient->add_output_port (s);
	}
	free (out_name [i]);
	free (out_conn [i]);
    }
}

#endif
