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
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <sndfile.h>
//#include "config.h"  // jconv
#include <time.h>
#include <math.h>
#include "common.h"
#include "common-string.h"
#include "builtin-effect.h"
#include "zita-convolver.h"


/**************
greathall.conf

/cd /audio/reverbs

#                in  out   partition    maxsize
# -----------------------------------------------
/convolver/new    1    2         512     120000

#              num   port name     connect to
# -----------------------------------------------
/input/name     1     Input

/output/name    1     Output.L
/output/name    2     Output.R

#               in out  gain  delay  offset  length  chan      file
# -------------------------------------------------------------------------
/impulse/read    1  1   0.2     0       0       0     1    greathall.wav
/impulse/read    1  2   0.2     0       0       0     2    greathall.wav

********************/

#define MAXSIZE 0x00100000
#define BSIZE 0x4000

static const char *label = "conv";
static const char *description = "impulse response based on Fons Adriaensen's work";

static const char *inputNames[] = { "impulse file" };
static int inputType[] = { INPUT_FILE };
static int iHasMin[] = { FALSE };
static int iHasMax[] = { FALSE };
static float iDefault[] = { NULL };
static int iIsLog[] = { FALSE };
static int iUsesSampleRate[] = { FALSE };
static int nInputs = 1;

static int nOutputs = 0;

static int nInputBuffers = 1;
static int nOutputBuffers = 1;

typedef struct data_s data_t;

struct data_s {
  float *input_buffer_left;
  float *output_buffer_left;

  int initialized;
  unsigned long sampleCount;  // used to init zita lib
  unsigned long sampleRate;
  unsigned long rate;
  unsigned int ninp;
  unsigned int nout;
  unsigned int size;
  unsigned int part;
  unsigned int frag;
  unsigned int latency;

  unsigned int err;

  char *impulse_file_name;
  char currentFilename[MAX_CHARS];

  Convproc *conv;
};

class Impdata
{
public:

    enum
    {
        TYPE_RAW     = 0x00010000,
	TYPE_SWEEP   = 0x00020002,
	TYPE_LR      = 0x00030002,
	TYPE_MS      = 0x00040002,
        TYPE_AMB_A   = 0x00050004,
        TYPE_AMB_B3  = 0x00060003,
        TYPE_AMB_B4  = 0x00070004,
	TYPE_LA_OCT  = 0x0008000C,
	TYPE__CHAN   = 0x0000ffff,
	TYPE__TYPE   = 0xffff0000
    };

    enum
    {
        ERR_NONE    = 0,
	ERR_MODE    = -1,
	ERR_DATA    = -2,
	ERR_OPEN    = -3,
	ERR_FORM    = -4,
	ERR_SEEK    = -5,
	ERR_READ    = -6,
	ERR_WRITE   = -7
    };

    enum
    {
        MODE_NONE   = 0,
	MODE_READ   = 1,
	MODE_WRITE  = 2
    };

    Impdata (void);
    ~Impdata (void);

    uint32_t mode (void) const { return _mode; }
    uint32_t vers (void) const { return _vers; }
    uint32_t type (void) const { return _type; }
    uint32_t rate_n (void) const { return _rate_n; }
    uint32_t rate_d (void) const { return _rate_d; }
    uint32_t n_chan (void) const { return _type & TYPE__CHAN; }
    uint32_t n_fram (void) const { return _n_fram; }
    uint32_t n_sect (void) const { return _n_sect; }
    uint32_t i_fram (void) const { return _i_fram; }
    uint32_t i_sect (void) const { return _i_sect; }
    int32_t  tref_i (void) const { return _tref_i; }
    uint32_t tref_n (void) const { return _tref_n; }
    uint32_t tref_d (void) const { return _tref_d; }

    int  set_type (uint32_t type);
    int  set_n_fram (uint32_t n_fram);
    int  set_n_sect (uint32_t n_sect);
    void set_rate (uint32_t rate_n, uint32_t rate_d = 1) { _rate_n = rate_n; _rate_d = rate_d; }
    void set_tref (int32_t i, uint32_t n = 0, uint32_t d = 1) { _tref_i = i; _tref_n = n; _tref_d = d; }
    void set_bits (uint32_t bits) { _bits = bits; }

    double drate (void) const { return (double) _rate_n / (double) _rate_d; }
    double dtref (void) const { return (double) _tref_n / (double) _tref_d + _tref_i; }

    float *data (int c) const { return _data  ? (_data + c) : 0; }

    int alloc (void);
    int deall (void);
    int clear (void);

    int open_read (const char *name);
    int open_write (const char *name);
    int sf_open_read (const char *name);
    int sf_open_write (const char *name);
    int close (void);

    int read_sect  (uint32_t i_sect);
    int write_sect (uint32_t i_sect);
    int seek_sect (uint32_t i_sect);

    int read_ext  (uint32_t k, float *p);
    int write_ext (uint32_t k, float *p);
    int seek_ext (uint32_t k);

    int locate (uint32_t i_sect) { return seek_sect (i_sect); }

    static void checkext (char *name);
    static const char *channame (uint32_t type, uint32_t chan);

private:

    uint32_t       _vers;
    uint32_t       _type;
    uint32_t       _rate_n;
    uint32_t       _rate_d;
    uint32_t       _n_fram;
    uint32_t       _n_sect;
    int32_t        _tref_i;
    uint32_t       _tref_n;
    uint32_t       _tref_d;
    uint32_t       _bits;

    uint32_t       _i_fram;
    uint32_t       _i_sect;
    uint32_t       _mode;
    uint32_t       _size;
    float         *_data;
    FILE          *_aldfile;
    SNDFILE       *_sndfile;

    static uint32_t mapvers1 (uint32_t type);

    static char _suffix [12];
    static const char *_suffix2 [2];
    static const char *_suffix3 [2];
    static const char *_suffix4 [2];
    static const char *_suffix5 [4];
    static const char *_suffix7 [4];
    static const char *_suffix8 [12];
};

#define HDRSIZE 256


//------------------------------------------------------------------------------------------------------------


Impdata::Impdata (void) :
    _vers (0),
    _type (0),
    _rate_n (0),
    _rate_d (1),
    _n_fram (0),
    _n_sect (0),
    _tref_i (0),
    _tref_n (0),
    _tref_d (1),
    _bits (0),
    _i_fram (0),
    _i_sect (0),
    _mode (0),
    _size (0),
    _data (0),
    _aldfile (0),
    _sndfile (0)
{
}


Impdata::~Impdata (void)
{
    close ();
    delete[] _data;
}


int Impdata::set_type (uint32_t type)
{
    if (_mode) return ERR_MODE;
    _type = type;
    deall ();
    return 0;
}


int Impdata::set_n_fram (uint32_t n_fram)
{
    if (_aldfile) return ERR_MODE;
    _n_fram = n_fram;
    deall ();
    return 0;
}


int Impdata::set_n_sect (uint32_t n_sect)
{
    if (_aldfile) return ERR_MODE;
    _n_sect = n_sect;
    return 0;
}


int Impdata::alloc (void)
{
    uint32_t k;

    k = (_type & TYPE__CHAN) * _n_fram;
    if (k != _size)
    {
        delete[] _data;
	if (k)
	{
	    _size = k;
            _data = new float [k];
            memset (_data, 0, _size * sizeof (float));
	}
        else
	{
            _size = 0;
            _data = 0;
	}
    }
    return 0;
}


int Impdata::deall (void)
{
    delete[] _data;
    _data = 0;
    _size = 0;
    return 0;
}


int Impdata::clear (void)
{
    if (_data) memset (_data, 0, _size * sizeof (float));
    return 0;
}


//------------------------------------------------------------------------------------------------------------


int Impdata::open_read (const char *name)
{
    char p [HDRSIZE];

    if (_mode) return ERR_MODE;
    deall ();
    _type = 0;
    _n_fram = 0;
    _n_sect = 0;

    if ((_aldfile = fopen (name, "r")) == 0) return ERR_OPEN;

    if (fread (p, 1, HDRSIZE, _aldfile) != HDRSIZE)
    {
	fclose (_aldfile);
	_aldfile = 0;
	return ERR_READ;
    }

    if (strcmp (p, "aliki") || p [6] || p [7])
    {
	fclose (_aldfile);
	_aldfile = 0;
	return ERR_FORM;
    }

    _vers = *(uint32_t *)(p + 8);
    if (_vers == 1)
    {
        _type   = mapvers1 (*(uint32_t *)(p + 12));
        _rate_n = *(uint32_t *)(p + 16);
        _rate_d = 1;
        _n_sect = *(uint32_t *)(p + 20);
        _n_fram = *(uint32_t *)(p + 24);
        _tref_i = *(uint32_t *)(p + 28);
        _tref_n = 0;
        _tref_d = 1;
	_bits   = 0;
    }
    else if (_vers == 2)
    {
        _type   = *(uint32_t *)(p + 12);
        _rate_n = *(uint32_t *)(p + 16);
        _rate_d = *(uint32_t *)(p + 20);
        _n_fram = *(uint32_t *)(p + 24);
        _n_sect = *(uint32_t *)(p + 28);
        _tref_i = *(uint32_t *)(p + 32);
        _tref_n = *(uint32_t *)(p + 36);
        _tref_d = *(uint32_t *)(p + 40);
        _bits   = *(uint32_t *)(p + 44);
    }
    else
    {
	fclose (_aldfile);
	_aldfile = 0;
	return ERR_FORM;
    }

    if (!_type || !_n_fram || !_n_sect)
    {
	fclose (_aldfile);
	_aldfile = 0;
	return ERR_FORM;
    }

    _mode = MODE_READ;
    _i_sect = 0;
    _i_fram = 0;

    return 0;
}


int Impdata::open_write (const char *name)
{
    char p [HDRSIZE];

    if (_mode || !_type || !_n_fram || !_n_sect) return ERR_MODE;

    if ((_aldfile = fopen (name, "w")) == 0) return ERR_OPEN;

    strcpy (p, "aliki");
    p [6] = p [7] = 0;
    *(uint32_t *)(p +  8) = _vers = 2;
    *(uint32_t *)(p + 12) = _type;
    *(uint32_t *)(p + 16) = _rate_n;
    *(uint32_t *)(p + 20) = _rate_d;
    *(uint32_t *)(p + 24) = _n_fram;
    *(uint32_t *)(p + 28) = _n_sect;
    *(uint32_t *)(p + 32) = _tref_i;
    *(uint32_t *)(p + 36) = _tref_n;
    *(uint32_t *)(p + 40) = _tref_d;
    *(uint32_t *)(p + 44) = _bits;

    memset (p + 48, 0, HDRSIZE - 48);
    if (fwrite (p, 1, HDRSIZE, _aldfile) != HDRSIZE)
    {
	fclose (_aldfile);
	_aldfile = 0;
	return ERR_WRITE;
    }

    _mode = MODE_WRITE;
    _i_sect = 0;
    _i_fram = 0;

    return 0;
}


int Impdata::sf_open_read (const char *name)
{
    SF_INFO   I;

    if (_mode) return ERR_MODE;
    deall ();
    _type = 0;
    _n_fram = 0;
    _n_sect = 0;

    if ((_sndfile = sf_open (name, SFM_READ, &I)) == 0) return ERR_OPEN;

    _type   = TYPE_RAW + I.channels;
    _rate_n = I.samplerate;
    _rate_d = 1;
    _n_sect = 1;
    _n_fram = I.frames;
    _tref_i = 1;
    _tref_n = 0;
    _tref_d = 1;
    _bits   = 0;

#ifndef NOAMBIS
    if (sf_command (_sndfile, SFC_WAVEX_GET_AMBISONIC, 0, 0) == SF_AMBISONIC_B_FORMAT)
    {
	switch (I.channels)
	{
	case 3: _type = TYPE_AMB_B3; break;
	case 4: _type = TYPE_AMB_B4; break;
	}
    }
#endif

    if (!_n_fram)
    {
	sf_close (_sndfile);
	_sndfile = 0;
	return ERR_FORM;
    }

    _mode = MODE_READ;

    return 0;
}


int Impdata::sf_open_write (const char *name)
{
    SF_INFO   I;

    if (_mode || !_type || !_n_fram || !_n_sect) return ERR_MODE;

    I.channels = _type & TYPE__CHAN;
    I.format = (I.channels > 2) ? SF_FORMAT_WAVEX : SF_FORMAT_WAV;
    I.format |= SF_FORMAT_FLOAT;
    I.samplerate = _rate_n / _rate_d;
    I.sections = 1;

    if ((_sndfile = sf_open (name, SFM_WRITE, &I)) == 0) return ERR_OPEN;

#ifndef NOAMBIS
    switch (_type)
    {
    case TYPE_AMB_B3:
    case TYPE_AMB_B4:
        sf_command (_sndfile, SFC_WAVEX_SET_AMBISONIC, 0, SF_AMBISONIC_B_FORMAT);
	break;
    }
#endif

    _mode = MODE_WRITE;
    _i_sect = 0;
    _i_fram = 0;

    return 0;
}


int Impdata::close (void)
{
    if (_aldfile)
    {
	fclose (_aldfile);
        _aldfile = 0;
    }
    else if (_sndfile)
    {
	sf_close (_sndfile);
	_sndfile = 0;
    }
    _mode = 0;
    return 0;
}


//------------------------------------------------------------------------------------------------------------


int Impdata::read_sect (uint32_t i_sect)
{
    int r;

    r = locate (i_sect);
    if (r) return r;
    return _data ? read_ext (_n_fram, _data) : ERR_DATA;
}


int Impdata::write_sect (uint32_t i_sect)
{
    int r;

    r = locate (i_sect);
    if (r) return r;
    return _data ? write_ext (_n_fram, _data) : ERR_DATA;
}


int Impdata::seek_sect (uint32_t i_sect)
{
    if (_aldfile)
    {
        if (fseek (_aldfile, HDRSIZE + (_type & TYPE__CHAN) * _n_fram * i_sect * sizeof (float), SEEK_SET)) return ERR_SEEK;
        _i_sect = i_sect;
        _i_fram = 0;
        return 0;
    }
    else if (_sndfile)
    {
	if (sf_seek (_sndfile, _n_fram * i_sect, SEEK_SET) != _n_fram * i_sect) return ERR_SEEK;
        _i_sect = i_sect;
        _i_fram = 0;
	return 0;
    }
    else return ERR_MODE;
}


int Impdata::read_ext (uint32_t k, float *p)
{
    if (_mode != MODE_READ) return ERR_MODE;
    if (k > _n_fram - _i_fram) k = _n_fram - _i_fram;
    if (_aldfile)
    {
        k = fread (p, (_type & TYPE__CHAN) * sizeof (float), k, _aldfile);
    }
    else if (_sndfile)
    {
	k = sf_readf_float (_sndfile, p, k);
    }
    else return ERR_MODE;
    _i_fram += k;
    return k;
}


int Impdata::write_ext (uint32_t k, float *p)
{
    if (_mode != MODE_WRITE) return ERR_MODE;
    if (k > _n_fram - _i_fram) k = _n_fram - _i_fram;
    if (_aldfile)
    {
        k = fwrite (p, (_type & TYPE__CHAN) * sizeof (float), k, _aldfile);
    }
    else if (_sndfile)
    {
	k = sf_writef_float (_sndfile, p, k);
    }
    else return ERR_MODE;
    _i_fram += k;
    return k;
}


int Impdata::seek_ext (uint32_t k)
{
    if (_aldfile)
    {
        if (fseek (_aldfile, HDRSIZE + (_type & TYPE__CHAN) * k * sizeof (float), SEEK_SET)) return ERR_SEEK;
        _i_fram = k;
        return 0;
    }
    else if (_sndfile)
    {
        if (sf_seek (_sndfile, k, SEEK_SET) != k) return ERR_SEEK;
        _i_fram = k;
        return 0;
    }
    else return ERR_MODE;
}


//------------------------------------------------------------------------------------------------------------


uint32_t Impdata::mapvers1 (uint32_t type)
{
    if (type < 256) return TYPE_RAW | type;
    switch (type)
    {
    case 0x0102: return TYPE_MS;
    case 0x0202: return TYPE_SWEEP;
    case 0x0103: return TYPE_AMB_B3;
    case 0x0104: return TYPE_AMB_B4;
    }
    return TYPE_RAW | type;
}


void Impdata::checkext (char *name)
{
    char  *p;

    p = strrchr (name, '.');
    if (p && ! strcmp (p, ".ald")) return;
    strcat (name, ".ald");
}


char Impdata::_suffix [12];
const char *Impdata::_suffix2 [] = { "F", "I" };
const char *Impdata::_suffix3 [] = { "L", "R" };
const char *Impdata::_suffix4 [] = { "M", "S" };
const char *Impdata::_suffix5 [] = { "LF", "RF", "LB", "RB" };
const char *Impdata::_suffix7 [] = { "W", "X", "Y", "Z" };
const char *Impdata::_suffix8 [] = { "Lin", "A-W", "32", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };


const char *Impdata::channame (uint32_t type, uint32_t chan)
{
    const char **p;

    if (chan >= (type & TYPE__CHAN)) return 0;

    switch (type & TYPE__TYPE)
    {
    case 0x00020000: p = _suffix2; break;
    case 0x00030000: p = _suffix3; break;
    case 0x00040000: p = _suffix4; break;
    case 0x00050000: p = _suffix5; break;
    case 0x00060000:
    case 0x00070000: p = _suffix7; break;
    case 0x00080000: p = _suffix8; break;
    default: p = 0;
    }

    if (p) return p [chan];
    sprintf (_suffix, "%d", chan + 1);
    return _suffix;
}


//------------------------------------------------------------------------------------------------------------


static void *instantiate (unsigned long sampleRate)
{
  void *r;

  r = calloc(1, sizeof(data_t));
  if (!r) {
    xerror();
  }
  ((data_t *)r)->sampleRate = sampleRate;
  ((data_t *)r)->rate = sampleRate;

  return r;
}

static void cleanup (void *instance)
{
  data_t *ins = (data_t *)instance;

  if (!instance) {
    xerror();
  }

  ins->conv->cleanup();

  free(instance);
}

static void activate (void *instance)
{
  data_t *ins = (data_t *)instance;

  if (!ins) {
    xerror();
  }
}

static void deactivate (void *instance)
{
  data_t *ins = (data_t *)instance;

  if (!ins)
    return;
  if (!ins->conv)
    return;

  //ins->conv->cleanup();
  ins->conv->stop_process();

}

static void connect_input_buffer (void *instance, int num, float *buffer)
{
  data_t *ins = (data_t *)instance;

  if (!ins) {
    xerror();
  }
  if (!buffer) {
    xerror();
  }
  if (num >= nInputBuffers) {
    xerror();
  }

  if (num == 0) {
    ins->input_buffer_left = buffer;
  } else {
    //ins->input_buffer_right = buffer;
    xerror();
  }
}

static void connect_output_buffer (void *instance, int num, float *buffer)
{
  data_t *ins = (data_t *)instance;

  if (!ins) {
    xerror();
  }
  if (!buffer) {
    xerror();
  }
  if (num >= nOutputBuffers) {
    xerror();
  }

  if (num == 0) {
    ins->output_buffer_left = buffer;
  } else {
    //ins->output_buffer_right = buffer;
    xerror();
  }
}

static void connect_input (void *instance, int num, void *f)
{
  data_t *ins = (data_t *)instance;

  if (!ins) {
    xerror();
  }
  if (!f) {
    xerror();
  }
  if (num >= nInputs) {
    xerror();
  }

  if (num == 0) {
    ins->impulse_file_name = (char *)f;
  } else {
    xerror();
  }
}

static void init_instance (data_t *ins)
{
  unsigned int  ip1, op1;
  float         gain;
  unsigned int  delay;
  unsigned int  offset;
  unsigned int  length;
  unsigned int  ichan, nchan;
  int           ifram, nfram;  //, err;
  char          file [1024];
  char          path [1024];
  Impdata       impd;
  float         *buff, *p;
  int lnum = 0;  // just to compile ok

  ins->initialized = TRUE;
  //FIXME
  ins->err = TRUE;

  // frag = periodsize >> 2;
  ins->frag = ins->sampleCount;
  ins->latency = 0;  //FIXME pass in

  ins->conv = new Convproc;
  //if (M_opt) conv->set_fftwopt(FFTW_MEASURE);

  /*             config(av[0], 0)     */

  // convnew(q)
  /*
    #                in  out   partition    maxsize
    # -----------------------------------------------
    /convolver/new    1    2         512     120000
  */
  ins->ninp = 1;
  ins->nout = 1;
  ins->part = 512;
  ins->size = 120000;

  //FIXME checks

  if (ins->part > Convproc::MAXDIVIS * ins->frag) {
    ins->part = Convproc::MAXDIVIS * ins->frag;
    fprintf (stderr, "Warning: partition size adjusted to %d\n", ins->part);
  }
  if (ins->part < ins->frag) {
    ins->part = ins->frag;
    fprintf (stderr, "Warning: partition size adjusted to %d\n", ins->part);
  }

  if (ins->conv->configure(ins->ninp, ins->nout, ins->size, ins->frag, ins->part, Convproc::MAXPART)) {
    fprintf(stderr, "Couldn't initialize convolver engine.\n");
    //FIXME wat
    return;
  }
  // end convnew()


  // read_sndfile(q, lnum, cdir, latency)
  /*
    #               in out  gain  delay  offset  length  chan      file
    # ------------------------------------------------------------------
    /impulse/read    1  1   0.2     0       0       0     1    greathall.wav
    /impulse/read    1  2   0.2     0       0       0     2    greathall.wav
  */
  ip1 = 1;
  op1 = 1;
  gain = 0.1;  //0.2;
  delay = 0;
  offset = 0;
  length = 0;
  ichan = 1;
  //snprintf(file, 1024, "/home/acano/tmp5/greathall.wav");
  snprintf(file, 1024, ins->impulse_file_name);

  if (ins->latency)
    {
      if (delay >= ins->latency)
        {
          delay -= ins->latency;
        }
      else 
        {
          ins->latency -= delay;
          delay = 0;
          offset += ins->latency;
          fprintf (stderr, "Warning: first %d frames removed by latency compensation,\n", ins->latency);
        }
    }
  //err = check1 (ip1, op1);
  //if (err) return err;

  //if (*file == '/') strcpy (path, file);
  //else
  //  {
  //    strcpy (path, cdir);
  //    strcat (path, "/");
  //    strcat (path, file);
  //  }
  strcpy(path, file);

  if (impd.sf_open_read (path))
    {
      fprintf (stderr, "Unable to open '%s'\n", path);
      return;
    }
  if (impd.rate_n () != (unsigned int) ins->rate)
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
      return;
    } 
  if (offset && impd.seek_ext (offset))
    {
      fprintf (stderr, "Can't seek to offset in line %d\n", lnum);
      impd.close ();
      return;
    }
  if (! length) length = nfram - offset;
  if (length > ins->size - delay) 
    {
      length = ins->size - delay;
      fprintf (stderr, "Warning: data truncated in line %d\n", lnum);
    }

    try 
      {
        buff = new float [BSIZE * nchan];
      }
    catch (...)
      {
        fprintf(stderr, "covn couldn't allocate buffer\n");
        impd.close ();
        return;
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
            return;
          }
        if (nfram)
          {
            p = buff + ichan - 1;
            for (ifram = 0; ifram < nfram; ifram++) p [ifram * nchan] *= gain;
            if (ins->conv->impdata_create (ip1 - 1, op1 - 1, nchan, p, delay, delay + nfram))
              {
                fprintf(stderr, "Error creating impulse data\n");
                impd.close ();
                delete[] buff;
                return;
              }
            delay  += nfram;
            length -= nfram;
          }
      }

    impd.close ();
    delete[] buff;
    //return 0;
    // end read_sndfile()

  //FIXME /impulse/copy

  // done config()
    //////////////////////////////////////////////////

    ins->conv->print();

    ins->conv->start_process(0, 0);

    ins->err = FALSE;
    printf("conv initialized\n");
}

static void run (void *instance, unsigned long sampleCount)
{
  data_t *ins = (data_t *)instance;
  //int i;
  //float q, dist, fx;

  if (!ins) {
    xerror();
  }
  if (!ins->input_buffer_left) {
    xerror();
  }
  if (!ins->output_buffer_left) {
    xerror();
  }
  if (!ins->impulse_file_name) {
    return;
  }

  if (!str_matches(ins->currentFilename, ins->impulse_file_name)) {
    printf("new file: %s\n", ins->impulse_file_name);
    //FIXME not sure
    deactivate(ins);
    ins->err = 0;  //FIXME /
    ins->initialized = FALSE;
    strncpy(ins->currentFilename, ins->impulse_file_name, MAX_CHARS);
  }

  if (ins->err) {  //FIXME
    return;
  }

#if 1
  if (*(ins->impulse_file_name) == '\0') {
    //printf("null\n");
    return;
  }
#endif
  if (!ins->initialized) {
    ins->sampleCount = sampleCount;
    init_instance(ins);
  }

  if (ins->err) {
    fprintf(stderr, "ooooops\n");
    return;
  }

  // process()

  {
    unsigned int i;  //, k;
    //float        *p1, *p2, *p3;
    float        *inpp [Convproc::MAXINP];
    float        *outp [Convproc::MAXOUT];
    unsigned int _frag;

    _frag = sampleCount;  //nframes;  // * sizeof(float);
    outp[0] = ins->output_buffer_left;
    inpp[0] = ins->input_buffer_left;

    if (ins->conv->state () == Convproc::ST_WAIT) ins->conv->check ();

    if (ins->conv->state () != Convproc::ST_PROC)
      {
        for (i = 0; i < ins->nout; i++)
          {
            memset (outp [i], 0, _frag * sizeof (float));
          }
        return;
      }

#if 1
    for (i = 0;  i < ins->ninp;  i++) {
      memcpy(ins->conv->inpdata(i), inpp[i], _frag * sizeof(float));
    }
    ins->conv->process();
    for (i = 0;  i < ins->nout;  i++) {
      memcpy(outp[i], ins->conv->outdata(i), _frag * sizeof(float));
    }
#endif

  }
}

extern "C" void be_conv_init (builtin_effect_t *be);

void be_conv_init (builtin_effect_t *be)
{
  be->label = (char *)label;
  be->description = (char *)description;
  be->nInputs = nInputs;
  be->inputNames = (char **)inputNames;
  be->inputType = inputType;
  be->iHasMin = iHasMin;
  be->iHasMax = iHasMax;
  be->iDefault = iDefault;
  be->iIsLog = iIsLog;
  be->iUsesSampleRate = iUsesSampleRate;

  be->nOutputs = nOutputs;

  be->nInputBuffers = nInputBuffers;
  be->nOutputBuffers = nOutputBuffers;

  be->instantiate = instantiate;
  be->cleanup = cleanup;
  be->activate = activate;
  be->deactivate = deactivate;
  be->connect_input_buffer = connect_input_buffer;
  be->connect_output_buffer = connect_output_buffer;
  be->connect_input = connect_input;
  be->run = run;
}

