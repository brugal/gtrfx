//  -----------------------------------------------------------------------------
//
//  Copyright (C) 2007 Fons Adriaensen <fons@kokkinizita.net>
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
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <getopt.h>
#include "impdata.h"


static char *options = "hA";
static bool  A_opt = false;



static void help (void)
{
    fprintf (stderr, "\nmkwavex %s\n", VERSION);
    fprintf (stderr, "(C) 2007 Fons Adriaensen  <fons@kokkinizita.net>\n\n");
    fprintf (stderr, "Usage: mkwavex <options> <input files> <output file>\n");
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  -h    Display this text\n");
    fprintf (stderr, "  -A    Create Ambisonics file\n");
    exit (1);
}


static void procoptions (int ac, char *av [], const char *where)
{
    int k;
    
    optind = 1;
    opterr = 0;
    while ((k = getopt (ac, av, options)) != -1)
    {
        if (optarg && (*optarg == '-'))
        {
            fprintf (stderr, "\n%s\n", where);
	    fprintf (stderr, "  Missing argument for '-%c' option.\n", k); 
            fprintf (stderr, "  Use '-h' to see all options.\n");
            exit (1);
        }
	switch (k)
	{
        case 'h' : help (); exit (0);
 	case 'A' : A_opt = true;  break;
        case '?':
            fprintf (stderr, "\n%s\n", where);
            if (optopt != ':' && strchr (options, optopt)) fprintf (stderr, "  Missing argument for '-%c' option.\n", optopt); 
            else if (isprint (optopt)) fprintf (stderr, "  Unknown option '-%c'.\n", optopt);
            else fprintf (stderr, "  Unknown option character '0x%02x'.\n", optopt & 255);
            fprintf (stderr, "  Use '-h' to see all options.\n");
            exit (1);
        default:
            abort ();
 	}
    }
}


#define BLOCK 0x4000


int main (int ac, char *av [])
{
    int            c, i, j, n, r;
    unsigned int   k;
    float          *p, *q;
    int            nip, nch, sec;
    unsigned int   len, type;
    Impdata       *inp;
    Impdata        out;

    procoptions (ac, av, "On command line:");
    if (optind >= ac) help ();
    nip = ac - optind - 1;
    if (nip < 1) help ();

    inp = new Impdata [nip];
    nch = 0;
    len = 0;
    for (i = 0; i < nip; i++)
    {
	r = inp [i].sf_open_read (av [optind + i]);
        if (r)
	{
	    fprintf (stderr, "Can't read '%s'\n", av [optind + i]);
	    delete[] inp;
	    return 1;
	}
        k = inp [i].n_fram ();
   	if (len < k) len = k;
	nch += inp [i].n_chan ();
        inp [i].set_n_sect ((k + BLOCK - 1) / BLOCK);
        inp [i].set_n_fram (BLOCK);
	inp [i].alloc ();
    }

    type = Impdata::TYPE_RAW + nch;
    if (A_opt) 
    {
	switch (nch)
	{
	case 3: type = Impdata::TYPE_AMB_B3; break;
	case 4: type = Impdata::TYPE_AMB_B4; break;
	}
    }
    out.set_type (type);
    out.set_rate (inp [0].rate_n (), inp [0].rate_d ());
    out.set_n_sect ((len + BLOCK - 1) / BLOCK);
    out.set_n_fram (BLOCK);
    r = out.sf_open_write (av [optind + nip]);
    if (r)
    {
	fprintf (stderr, "Can't create '%s'\n", av [optind + nip]);
	delete[] inp;
	out.close ();
	return 1;
    }
    out.alloc ();

    sec = 0;
    while (len)
    {
	k = (len < BLOCK) ? len : BLOCK;
	c = 0;
	out.clear ();
	for (i = 0; i < nip; i++)
	{
	    n = inp [i].n_chan ();
	    r = inp [i].read_sect (sec);
	    if (r > 0)
	    {
	        p = inp [i].data (0);
  	        q = out.data (c);
	        while (r--)
	        {
		    for (j = 0; j < n; j++) q [j] = p [j];
		    p += n;
		    q += nch;
	        }
	    }
            c += n;
	} 
	out.write_sect (sec++);
	len -= k;
    }

    out.close ();
    out.deall ();
    delete[] inp;
    return 0;
}

