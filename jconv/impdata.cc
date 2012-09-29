// -------------------------------------------------------------------------
//
//    Copyright (C) 2006-2009 Fons Adriaensen <fons@kokkinizita.net>
//    
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
// -------------------------------------------------------------------------


#include <stdlib.h>
#include <string.h>
#include "impdata.h"


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
