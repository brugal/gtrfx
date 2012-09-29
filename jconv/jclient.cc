/*
    Copyright (C) 2006-2009 Fons Adriaensen <fons@kokkinizita.net>
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "jclient.h"


Jclient::Jclient (const char *name, Convproc *conv) :
    _jack_name (name),
    _jack_client (0),
    _flags (0),
    _mode (PM_CONVOL),
    _ninp (0),
    _nout (0),
    _conv (conv)
{
    init_jack ();  
}


Jclient::~Jclient (void)
{
    if (_jack_client) close_jack ();
}


int Jclient::delete_ports (void)
{
    unsigned int k;

    if (_conv->state () != Convproc::ST_IDLE) return -1;
    _ninp = 0;
    _nout = 0;

    for (k = 0; k < Convproc::MAXINP; k++)
    {
	if (_jack_inppp [k])
	{
	    jack_port_unregister (_jack_client, _jack_inppp [k]);
	    _jack_inppp [k] = 0;
	}
    }

    for (k = 0; k < Convproc::MAXOUT; k++)
    {
	if (_jack_outpp [k])
	{
	    jack_port_unregister (_jack_client, _jack_outpp [k]);
	    _jack_outpp [k] = 0;
	}
    }

    return 0;
}


void Jclient::init_jack (void)
{
    struct sched_param  spar;
    jack_status_t       stat;

    if ((_jack_client = jack_client_open (_jack_name, (jack_options_t) 0, &stat)) == 0)
    {
        fprintf (stderr, "Can't connect to JACK.\n");
        exit (1);
    }

    if (stat & JackNameNotUnique)
    {
	_jack_name = jack_get_client_name (_jack_client);
	printf ("Connected to JACK as '%s'\n", _jack_name);
    }

    jack_on_shutdown (_jack_client, jack_static_shutdown, (void *) this);
    jack_set_freewheel_callback (_jack_client, jack_static_freewheel, (void *) this);
    jack_set_buffer_size_callback (_jack_client, jack_static_buffsize, (void *) this);
    jack_set_process_callback (_jack_client, jack_static_process, (void *) this);

    if (jack_activate (_jack_client))
    {
        fprintf(stderr, "Can't activate JACK.\n");
        exit (1);
    }

    _rate = jack_get_sample_rate (_jack_client);
    _frag = jack_get_buffer_size (_jack_client);
    if (_frag < 32)
    {
	fprintf (stderr, "Fragment size is too small\n");
	exit (0);
    }
    if (_frag > 4096)
    {
	fprintf (stderr, "Fragment size is too large\n");
	exit (0);
    }

    pthread_getschedparam (jack_client_thread_id (_jack_client), &_policy, &spar);
    _abspri = spar.sched_priority;

    memset (_jack_inppp, 0, Convproc::MAXINP * sizeof (jack_port_t *));
    memset (_jack_outpp, 0, Convproc::MAXOUT * sizeof (jack_port_t *));
}


void Jclient::close_jack ()
{
    jack_deactivate (_jack_client);
    jack_client_close (_jack_client);
}


void Jclient::jack_static_shutdown (void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->_flags = FL_EXIT;
}


void Jclient::jack_static_freewheel (int state, void *arg)
{
}


int Jclient::jack_static_buffsize (jack_nframes_t nframes, void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->stop ();
    C->_frag = nframes;
    C->_flags = FL_BUFF;
    return 0;
}


int Jclient::jack_static_process (jack_nframes_t nframes, void *arg)
{
    Jclient *C = (Jclient *) arg;

    C->jack_process ();
    return 0;
}


void Jclient::jack_process (void)
{
    unsigned int i, k;
    float        *p1, *p2, *p3;
    float        *inpp [Convproc::MAXINP];
    float        *outp [Convproc::MAXOUT];

    for (i = 0; i < _nout; i++)
    {
        outp [i] = (float *) jack_port_get_buffer (_jack_outpp [i], _frag);
    }
    for (i = 0; i < _ninp; i++)
    {
        inpp [i] = (float *) jack_port_get_buffer (_jack_inppp [i], _frag);
    }

    if (_conv->state () == Convproc::ST_WAIT) _conv->check ();

    if (_conv->state () != Convproc::ST_PROC)
    {
	for (i = 0; i < _nout; i++)
	{
            memset (outp [i], 0, _frag * sizeof (float));
	}
	return;
    }

    //printf("_mode == %d\n", _mode);

    if (_mode == PM_SIMPLE)
    {
        p1 = inpp [0];
	p2 = inpp [1];
	p3 = _conv->inpdata (0);
	if (_ninp == 2) for (k = 0; k < _frag; k++) p3 [k] = _gain2 * (p1 [k] + p2 [k]);
	else            for (k = 0; k < _frag; k++) p3 [k] = _gain2 * p1 [k];
    }
    else if (_mode == PM_REVERB)
    {
	p2 = _conv->inpdata (0);
	memcpy (p2, inpp [0], _frag * sizeof (float));
	for (i = 1; i < _ninp; i++)
	{
	    p1 = _conv->inpdata (i);
	    p3 = inpp [i];
	    for (k = 0; k < _frag; k++)
	    {
		p1 [k] = _gain1 * p3 [k];
		p2 [k] += p3 [k];
	    }
	}
        for (k = 0; k < _frag; k++) p2 [k] *= _gain2;
    }
    else 
    {
	for (i = 0; i < _ninp; i++)
	{
            memcpy (_conv->inpdata (i), inpp [i], _frag * sizeof (float));
	}
    }

    _conv->process ();

    for (i = 0; i < _nout; i++)
    {
	memcpy (outp [i], _conv->outdata (i), _frag * sizeof (float));
    }
    if (_mode == PM_SIMPLE)
    {
        p1 = inpp [0];
	p2 = outp [0];
        for (k = 0; k < _frag; k++) p2 [k] = _gain1 * p1 [k];
	if (_ninp == 2)
	{
            p1 = inpp [1];
	    p2 = outp [1];
            for (k = 0; k < _frag; k++) p2 [k] = _gain1 * p1 [k];
	}
    }
}


int Jclient::add_input_port (const char *name, const char *conn)
{
    char         s [512];
    jack_port_t  *P;

    if ((_conv->state () > Convproc::ST_STOP) || (_ninp == Convproc::MAXINP)) return -1;
    P = jack_port_register (_jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (P)
    {
        _jack_inppp [_ninp] = P;
        if (conn)
        {
	    sprintf (s, "%s:%s", _jack_name, name);
	    jack_connect (_jack_client, conn, s);
        }
        return _ninp++;
    }
    else
    {
	fprintf (stderr, "Can't create input port '%s'\n", name);
	exit (1);
    }
    return -1;
}


int Jclient::add_output_port (const char *name, const char *conn)
{
    char         s [512];
    jack_port_t  *P;

    if ((_conv->state () > Convproc::ST_STOP) || (_nout == Convproc::MAXOUT)) return -1;
    P = jack_port_register (_jack_client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    if (P)
    {
        _jack_outpp [_nout] = P;
        if (conn)
        {
	    sprintf (s, "%s:%s", _jack_name, name);
	    jack_connect (_jack_client, s, conn);
        }
        return _nout++;
    }
    else
    {
	fprintf (stderr, "Can't create output port '%s'\n", name);
	exit (1);
    }
    return -1;
}

