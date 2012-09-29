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
#include <signal.h>
#include <sndfile.h>
#include "config.h"


Convproc      *conv = 0;
unsigned int   rate = 0;
unsigned int   ninp = 0;
unsigned int   nout = 0;
unsigned int   size = 0;


static const char *options = "hvM";
static bool v_opt = false;
static bool M_opt = false;
static bool stop = false;


static void help (void)
{
    fprintf (stderr, "\nFconv %s\n", VERSION);
    fprintf (stderr, "(C) 2006-2007 Fons Adriaensen  <fons@kokkinizita.net>\n\n");
    fprintf (stderr, "Usage: fconv <configuration file> <input file> <output file>\n");
    fprintf (stderr, "Options:\n");
    fprintf (stderr, "  -h                 Display this text\n");
    fprintf (stderr, "  -v                 Print partition list to stdout [off]\n");
    fprintf (stderr, "  -M                 Use the FFTW_MEASURE option [off]\n");   
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
        case 'M' : M_opt = true; break;
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
    SNDFILE        *Finp, *Fout;
    SF_INFO        Sinp, Sout;
    unsigned int   i, j, k, n;
    unsigned long  nf;
    float          *buff, *p, *q;

    procoptions (ac, av, "On command line:");
    if (ac != optind + 3) help ();
    av += optind;

    Finp = sf_open (av [1], SFM_READ, &Sinp);
    if (! Finp)
    {
	fprintf (stderr, "Unable to read input file '%s'\n", av [1]);
        return 1;
    } 
    rate = Sinp.samplerate;

    conv = new Convproc;
    if (M_opt) conv->set_fftwopt (FFTW_MEASURE);
    if (config (av [0], 0))
    {
	conv->cleanup ();
	sf_close (Finp);
        return 1;
    }
    if (v_opt) conv->print ();

    if (Sinp.channels != (int) ninp)
    {
	fprintf (stderr, "Number of inputs (%d) in config file doesn't match input file (%d)'\n",
                 ninp, Sinp.channels);
	conv->cleanup ();
	sf_close (Finp);
        return 1;
    }

    Sout.samplerate = rate;
    Sout.channels = nout;
    Sout.format = (nout > 2) ? SF_FORMAT_WAVEX : SF_FORMAT_WAV;
    Sout.format |= SF_FORMAT_PCM_24;
    Sout.frames = Sinp.frames + size;
    Sout.sections = 1;
    Fout = sf_open (av [2], SFM_WRITE, &Sout);
    if (! Fout)
    {
	fprintf (stderr, "Unable to create output file '%s'\n", av [2]);
	conv->cleanup ();
	sf_close (Finp);
        return 1;
    } 

    nf = Sinp.frames + size;
    n = Convproc::MAXQUANT;
    buff = new float [n * ((ninp > nout) ? ninp : nout)];
    signal (SIGINT, sigint_handler); 

    conv->start_process (0);
    while (nf)
    {    
	if (stop) break;
	k = sf_readf_float (Finp, buff, n);
	if (k < n) memset (buff + k * ninp, 0, (n - k) * ninp * sizeof (float));
	for (i = 0; i < ninp; i++)
	{
	    p = buff + i;
	    q = conv->inpdata (i);
	    for (j = 0; j < n; j++) q [j] = p [j * ninp];
	}
	conv->process ();
	k = n;
	if (k > nf) k = nf;
	for (i = 0; i < nout; i++)
	{
	    p = conv->outdata (i);
	    q = buff + i;
	    for (j = 0; j < n; j++) q [j * nout] = p [j];
	}
        sf_writef_float (Fout, buff, k);
	nf -= k;
    }
    conv->stop_process ();
    conv->cleanup ();

    sf_close (Finp);
    sf_close (Fout);
    if (stop) unlink (av [2]);
    delete[] buff;
    delete[] conv;

    return stop ? 1 : 0;
}

