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


#ifndef __IMPDATA_H
#define __IMPDATA_H


#include <stdio.h>
#include <stdint.h>
#include <sndfile.h>


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


#endif

