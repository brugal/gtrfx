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
#include "config.h"


int convnew (const char *buff)
{
    int part;

    if (sscanf (buff, "%u %u %u %u", &ninp, &nout, &part, &size) != 4) return ERR_PARAM;
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
    if (size > MAXSIZE)
    {
        fprintf (stderr, "Convolver size (%d) is out of range\n", size);
        return ERR_OTHER;
    }

    if (conv->configure (ninp, nout, size, Convproc::MAXQUANT, Convproc::MAXQUANT, Convproc::MAXQUANT))
    {   
        fprintf (stderr, "Can't initialise convolver engine\n");
        return ERR_OTHER;
    }

    return 0;
}


int inpname (const char *)
{
    return 0;
}


int outname (const char *)
{
    return 0;
}

