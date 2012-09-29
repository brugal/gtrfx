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
#include <ctype.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>
#include "jclient.h"
#include "config.h"


static const char   *options = "hL:MN:v";
static const char   *N_val = "jconv";
static unsigned int  L_val = 0;
static bool          M_opt = false;
static bool          v_opt = false;
static bool          stop  = false;


Jclient       *jclient = 0;
Convproc      *conv = 0;
unsigned int   rate = 0;
unsigned int   frag = 0;
unsigned int   part = 0;
unsigned int   ninp = 0;
unsigned int   nout = 0;
unsigned int   size = 0;


static void help (void)
{
    fprintf (stderr, "\nJconv %s\n", VERSION);
    fprintf (stderr, "(C) 2006-2007 Fons Adriaensen  <fons@kokkinizita.net>\n\n");
    fprintf (stderr, "Usage: jconv <options> <config file> {<connect file>}\n");
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  -h                 Display this text\n");
    fprintf (stderr, "  -v                 Print partition list to stdout [off]\n");
    fprintf (stderr, "  -L <nframes>       Try to compensate <nframes> latency\n");
    fprintf (stderr, "  -M                 Use the FFTW_MEASURE option [off]\n");   
    fprintf (stderr, "  -N <name>          Name to use as JACK client [jconv]\n");   
    exit (1);
}


static void procoptions (int ac, char *av [], const char *where)
{
    int k;
    
    optind = 1;
    opterr = 0;
    while ((k = getopt (ac, av, (char *) options)) != -1)
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
        case 'v' : v_opt = true; break;
        case 'L' : L_val = atoi (optarg); break;
        case 'M' : M_opt = true; break;
        case 'N' : N_val = optarg; break; 
        case '?':
            fprintf (stderr, "\n%s\n", where);
            if (optopt != ':' && strchr (options, optopt))
	    {
                fprintf (stderr, "  Missing argument for '-%c' option.\n", optopt); 
	    }
            else if (isprint (optopt))
	    {
                fprintf (stderr, "  Unknown option '-%c'.\n", optopt);
	    }
            else
	    {
                fprintf (stderr, "  Unknown option character '0x%02x'.\n", optopt & 255);
	    }
            fprintf (stderr, "  Use '-h' to see all options.\n");
            exit (1);
        default:
            abort ();
 	}
    }
}


static void sigint_handler (int)
{
    stop = true;
}


int main (int ac, char *av [])
{
    int flags;
     
    if (mlockall (MCL_CURRENT | MCL_FUTURE))
    {
        fprintf (stderr, "Warning: ");
        perror ("mlockall:");
    }

    procoptions (ac, av, "On command line:");
    if (ac <= optind) help ();

    conv = new Convproc;
    jclient = new Jclient (N_val, conv);
    rate = jclient->rate ();
    frag = jclient->frag ();

    if (M_opt) conv->set_fftwopt (FFTW_MEASURE);
    if (config (av [optind], L_val))
    {
	delete jclient;
        return 1;
    }
    if (v_opt) conv->print ();

    makeports ();
    jclient->start ();

    signal (SIGINT, sigint_handler); 
    while (! stop)
    {    
	usleep (100000);
	flags = jclient->flags ();

	if (flags & Jclient::FL_EXIT)
	{
	    puts ("JACK ERROR");
	    stop = true;
	}
	if (flags & Convproc::FL_LOAD)
	{
	    puts ("CPU OVERLOAD");
	    stop = true;
	}
	if (flags & Convproc::FL_LATE)
	{
	    printf ("Processing can't keep up (%04x)\n", flags & Convproc::FL_LATE);
	}
    }

    jclient->stop ();
    while (conv->state () != Convproc::ST_STOP) usleep (100000);
    delete jclient;
    delete conv;

    return 0;
}

