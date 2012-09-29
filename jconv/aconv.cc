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
#include <alsa/asoundlib.h>
#include <time.h>

Convproc      *conv = 0;
unsigned int   rate = 0;
unsigned int   ninp = 0;
unsigned int   nout = 0;
unsigned int   size = 0;

unsigned int frag = 0;
unsigned int part = 0;

static const char *options = "hvM";
static bool v_opt = false;
static bool M_opt = false;
static bool stop = false;


static void help (void)
{
    fprintf (stderr, "\nrealtime %s\n", VERSION);
    fprintf (stderr, "(C) 2006-2007 Fons Adriaensen  <fons@kokkinizita.net>\n\n");
    fprintf (stderr, "Usage: realtime <configuration file> <input asla device> <output alsa device>\n");
    //FIXME rate, buffer, etc options
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

#define PERIODSIZE 512  //(512 / 4)  //1024  //512  //256  //(1024 * 2)  //512  //256
#define PERIODS 16

#define BUF_SIZE (PERIODSIZE * PERIODS)

#define SAMPLE_RATE 48000  //44100

#define BUFDATA_SIZE (1024 * 8)

static float ldata[BUFDATA_SIZE];
static float rdata[BUFDATA_SIZE];
static float ldata_out[BUFDATA_SIZE];
static float rdata_out[BUFDATA_SIZE];

enum { PM_CONVOL, PM_REVERB, PM_SIMPLE };
enum { FL_EXIT = 0x80000000, FL_BUFF = 0x40000000 };

static unsigned int _mode = PM_CONVOL;
static float _gain1 = 3.0f;
static float _gain2 = 3.0f;
//static Convproc *_conv;

static void process_simple(unsigned int nframes)
{
  int i;

  for (i = 0;  i < (int)nframes;  i++) {
    ldata_out[i] = ldata[i];
    rdata_out[i] = rdata[i];
  }
}

static void process(unsigned int nframes)
{
    unsigned int i, k;
    float        *p1, *p2, *p3;
    float        *inpp [Convproc::MAXINP];
    float        *outp [Convproc::MAXOUT];
    unsigned int _frag;

    _frag = nframes;  // * sizeof(float);

    outp[0] = ldata_out;
    outp[1] = rdata_out;
    inpp[0] = ldata;
    inpp[1] = rdata;

    if (conv->state () == Convproc::ST_WAIT) conv->check ();

    if (conv->state () != Convproc::ST_PROC)
    {
	for (i = 0; i < nout; i++)
	{
            memset (outp [i], 0, _frag * sizeof (float));
	}
	return;
    }

    if (_mode == PM_SIMPLE)
    {
        p1 = inpp [0];
	p2 = inpp [1];
	p3 = conv->inpdata (0);
	if (ninp == 2) for (k = 0; k < _frag; k++) p3 [k] = _gain2 * (p1 [k] + p2 [k]);
	else            for (k = 0; k < _frag; k++) p3 [k] = _gain2 * p1 [k];
    }
    else if (_mode == PM_REVERB)
    {
	p2 = conv->inpdata (0);
	memcpy (p2, inpp [0], _frag * sizeof (float));
	for (i = 1; i < ninp; i++)
	{
	    p1 = conv->inpdata (i);
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
      //printf("convolv\n");
	for (i = 0; i < ninp; i++)
	{
            memcpy (conv->inpdata (i), inpp [i], _frag * sizeof (float));
	}
    }

    conv->process ();

    for (i = 0; i < nout; i++)
    {
	memcpy (outp [i], conv->outdata (i), _frag * sizeof (float));
    }
    if (_mode == PM_SIMPLE)
    {
        p1 = inpp [0];
	p2 = outp [0];
        for (k = 0; k < _frag; k++) p2 [k] = _gain1 * p1 [k];
	if (ninp == 2)
	{
            p1 = inpp [1];
	    p2 = outp [1];
            for (k = 0; k < _frag; k++) p2 [k] = _gain1 * p1 [k];
	}
    }
}

int main (int ac, char *av [])
{
  //SNDFILE        *Finp, *Fout;
  //SF_INFO        Sinp, Sout;
  //unsigned int   i, j, k, n;
  int i, j;
  //unsigned int k, n;
  //unsigned int n;

  //unsigned long  nf;
    //float          *buff, *p, *q;
    //float *buff;
    snd_pcm_t *playback_handle;
    snd_pcm_t *capture_handle;
    //unsigned int alsaRate;
    snd_pcm_hw_params_t *hw_params_capture;
    snd_pcm_hw_params_t *hw_params_playback;
    snd_pcm_uframes_t capBufferSize, playBufferSize;
    int periods = PERIODS;
    snd_pcm_uframes_t periodsize = PERIODSIZE;  // / 2;
    signed short buf[PERIODSIZE * PERIODS];
    signed short stereoData[PERIODSIZE * PERIODS];
    int err;
    int dataSize;
    int captureChannels = 2;
    int playbackChannels = 2;
    //time_t sec;
    unsigned int loops;

    procoptions (ac, av, "On command line:");
    if (ac != optind + 3) help ();
    av += optind;

    frag = periodsize >> 2;  //(periodsize * periods) >> 2;
    printf("alsa frag size %d\n", frag);

    //// capture ////
    if ((err = snd_pcm_open(&capture_handle, av[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
      fprintf(stderr, "cannot open audio capture device %s (%s)\n",
              av[1],
              snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params_capture)) < 0) {
      fprintf(stderr, "cannot allocate capture hardware parameter structure (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, hw_params_capture)) < 0) {
      fprintf(stderr, "cannot initialize capture hardware parameter structure (%s)\n",
              snd_strerror(err));
      exit(1);
    }

#if 0
    resample = 1;
    err = snd_pcm_hw_params_set_rate_resample(capture_handle, hw_params_capture, resample);
    if (err < 0) {
      fprintf(stderr, "resampling setup failed for capture device (%s)\n",
              snd_strerror(err));
      exit(1);
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params_capture, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      fprintf(stderr, "cannot set capture access type (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params_capture, SND_PCM_FORMAT_S16_LE)) < 0) {
      fprintf(stderr, "cannot set capture sample format (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    rate = SAMPLE_RATE;
    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params_capture, &rate, 0)) < 0) {
      fprintf(stderr, "cannot set capture sample rate (%s)\n",
              snd_strerror(err));
      exit(1);
    }
    printf("capture rate set to %d\n", rate);

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params_capture, captureChannels)) < 0) {
      fprintf(stderr, "cannot set capture channel count (%s)\n",
              snd_strerror(err));
      //FIXME double check
      captureChannels = 1;
    }
    printf("capture channels %d\n", captureChannels);

    /* Set number of periods. Periods used to be called fragments. */
    if ((err = snd_pcm_hw_params_set_periods(capture_handle, hw_params_capture, periods, 0)) < 0) {
      fprintf(stderr, "Error setting capture periods (%s)\n", snd_strerror(err));
      exit(1);
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if ((err = snd_pcm_hw_params_set_buffer_size(capture_handle, hw_params_capture, (periodsize * periods)>>2)) < 0) {
      fprintf(stderr, "Error setting capture buffersize (%s)  %ld\n", snd_strerror(err), (periodsize * periods)>>2);
      exit(1);
    }

    if ((err = snd_pcm_hw_params(capture_handle, hw_params_capture)) < 0) {
      fprintf(stderr, "cannot set capture parameters (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    snd_pcm_hw_params_get_buffer_size(hw_params_capture, &capBufferSize);
    printf("capture buffer size %d\n", (int)capBufferSize);
    printf("capture bits %d\n", snd_pcm_hw_params_get_sbits(hw_params_capture));
    if (capBufferSize > BUF_SIZE) {
      fprintf(stderr, "shit capture buffer size > BUF_SIZE  (%d > %d)\n", (int)capBufferSize, BUF_SIZE);
      exit(1);
    }


    if ((err = snd_pcm_prepare(capture_handle)) < 0) {
      fprintf(stderr, "cannot prepare audio capture interface for use (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    ////////////////   playback    ///////////////

    if ((err = snd_pcm_open(&playback_handle, av[2], SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
      fprintf(stderr, "cannot open audio playback device %s (%s)\n",
              av[2],
              snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params_malloc(&hw_params_playback)) < 0) {
      fprintf(stderr, "cannot allocate playback hardware parameter structure (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params_any(playback_handle, hw_params_playback)) < 0) {
      fprintf(stderr, "cannot initialize playback hardware parameter structure (%s)\n",
              snd_strerror(err));
      exit(1);
    }

#if 0
    resample = 1;
    err = snd_pcm_hw_params_set_rate_resample(playback_handle, hw_params_playback, resample);
    if (err < 0) {
      fprintf(stderr, "resampling setup failed for playback device (%s)\n",
              snd_strerror(err));
      exit(1);
    }
#endif

    if ((err = snd_pcm_hw_params_set_access(playback_handle, hw_params_playback, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
      fprintf(stderr, "cannot set playback access type (%s)\n",
              snd_strerror(err));
      exit (1);
    }

    if ((err = snd_pcm_hw_params_set_format(playback_handle, hw_params_playback, SND_PCM_FORMAT_S16_LE)) < 0) {
      fprintf (stderr, "cannot set playback sample format (%s)\n",
               snd_strerror(err));
      exit(1);
    }

    //rate = SampleRate;
    if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, hw_params_playback, &rate, 0)) < 0) {
      fprintf(stderr, "cannot set playback sample rate (%s)\n",
              snd_strerror(err));
      exit(1);
    }
    printf("playback rate set to %d\n", rate);

    if ((err = snd_pcm_hw_params_set_channels(playback_handle, hw_params_playback, playbackChannels)) < 0) {
      fprintf(stderr, "cannot set playback channel count (%s)\n",
              snd_strerror(err));
      exit(1);
    }
    printf("playback channels %d\n", playbackChannels);

    /* Set number of periods. Periods used to be called fragments. */
    if ((err = snd_pcm_hw_params_set_periods(playback_handle, hw_params_playback, periods, 0)) < 0) {
      fprintf(stderr, "Error setting playback periods (%s)\n", snd_strerror(err));
      exit(1);
    }

    /* Set buffer size (in frames). The resulting latency is given by */
    /* latency = periodsize * periods / (rate * bytes_per_frame)     */
    if ((err = snd_pcm_hw_params_set_buffer_size(playback_handle, hw_params_playback, (periodsize * periods)>>2)) < 0) {
      fprintf(stderr, "Error setting playback buffersize (%s)\n", snd_strerror(err));
      exit(1);
    }

    if ((err = snd_pcm_hw_params(playback_handle, hw_params_playback)) < 0) {
      fprintf (stderr, "cannot set playback parameters (%s)\n",
               snd_strerror(err));
      exit(1);
    }

    snd_pcm_hw_params_get_buffer_size(hw_params_playback, &playBufferSize);
    printf("playback buffer size %d\n", (int)playBufferSize);
    printf("playback bits %d\n", snd_pcm_hw_params_get_sbits(hw_params_playback));
    if (playBufferSize > BUF_SIZE) {
      fprintf(stderr, "shit playback buffer size > BUF_SIZE  (%d > %d)\n", (int)playBufferSize, BUF_SIZE);
      exit(1);
    }

    if ((err = snd_pcm_prepare(playback_handle)) < 0) {
      fprintf(stderr, "cannot prepare playback audio interface for use (%s)\n",
              snd_strerror(err));
      exit(1);
    }

    //////////////////////////////

    if (playBufferSize != capBufferSize) {
      fprintf(stderr, "playBufferSize != capBufferSize\n");
      exit(1);
    }

    //rate = Sinp.samplerate;
    rate = SAMPLE_RATE;


    conv = new Convproc;
    //_conv = conv;
    if (M_opt) conv->set_fftwopt (FFTW_MEASURE);
    if (config (av [0], 0))
    {
	conv->cleanup ();
	//sf_close (Finp);
        //FIXME clean up open devices
        return 1;
    }
    if (v_opt) conv->print ();

    //if (Sinp.channels != (int) ninp)
    //if (captureChannels != (int) ninp)
    if (captureChannels > (int) ninp)
    {
	fprintf (stderr, "Number of inputs (%d) in config file is less than capture inputs (%d)'\n",
                 ninp, captureChannels);
	conv->cleanup ();
	//sf_close (Finp);
        //FIXME clean up sound devices
        return 1;
    }

#if 0
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
#endif

    //FIXME not sure
    //nf = Sinp.frames + size;
    //n = Convproc::MAXQUANT;
    //buff = new float [n * ((ninp > nout) ? ninp : nout)];
    //signal (SIGINT, sigint_handler); 

    conv->start_process (0, 0);
    usleep(10000);

    // do it   jack_process()
    //FIXME check Convproc error conditions like jconv.cc main()
    //FIXME assuming stereo out
    loops = 0;
    while (!stop) {
      loops++;
      err = snd_pcm_readi(capture_handle, buf, periodsize >> 2);
      dataSize = err;
      if (dataSize < 0) {
        snd_pcm_prepare(capture_handle);
        {
          static time_t print_time;
          time_t currTime;

          currTime = time(NULL);
          if (print_time != currTime) {
            fprintf(stderr, "%d capture buffer overrun <<<<<<  (%s)\n", loops, snd_strerror(err));
            print_time = currTime;
          }
        }
        continue;
      }
      if (dataSize / captureChannels < 32) {
        fprintf(stderr, "fragment size is too small %d\n", dataSize);
        exit(0);
      }
      if (dataSize / captureChannels > 4096) {
        fprintf(stderr, "fragment size is too large %d\n", dataSize);
        exit(0);
      }

      // capture ok, continue
      if (captureChannels == 2) {
        //FIXME broken, check
        exit(1);
        // convert to float and separate channels
        for (i = 0, j = 0;  i < err * 2;  i += 2, j++) {
          ldata[j] = ((float)(buf[i]) * (1.0f / 32767.5f));
          rdata[j] = ((float)(buf[i + 1]) * (1.0f / 32767.5f));
        }
      } else if (captureChannels == 1) {

        for (i = 0;  i < err;  i++) {
          ldata[i] = rdata[i] = ((float)(buf[i]) * (1.0f / 32767.5f));
        }
      } else {  // output channels not 1 or 2
        fprintf(stderr,"more than 2 capture channels\n");
        exit(1);
      }

      // apply_effects(err);
      process(dataSize / captureChannels);
      //process_simple(dataSize / captureChannels);

      //FIXME check that output channels == 2
      // convert to interleaved and short int for playback
#define GAINX 1
      for (i = 0;  i < err;  i++) {
        stereoData[2 * i] = (signed short int)(ldata_out[i] * 32767.5f) * GAINX;
        stereoData[2 * i + 1] = (signed short int)(rdata_out[i] * 32767.5f) * GAINX;
      }

    
      while ((err = snd_pcm_writei(playback_handle, stereoData, periodsize >>2)) < 0) {
        snd_pcm_prepare(playback_handle);
        {
          static time_t print_time;
          time_t currTime;

          currTime = time(NULL);
          if (print_time != currTime) {
            fprintf(stderr, "%d playback xrun (%s)\n", loops, snd_strerror(err));
            print_time = currTime;
          }
        }
        break;
      }
      //FIXME
      //exit(0);
    }
    printf("all done\n");


    conv->stop_process ();
    loops = 0;
    while (conv->state () != Convproc::ST_STOP) {
      loops++;
      printf("waiting for conv to stop\n");
      if (loops > 1) {
        break;
      }
      usleep (100000);
    }

    //conv->cleanup ();

    //sf_close (Finp);
    //sf_close (Fout);
    //if (stop) unlink (av [2]);
    if (hw_params_capture)
      snd_pcm_hw_params_free(hw_params_capture);
    if (hw_params_playback)
      snd_pcm_hw_params_free(hw_params_playback);
    if (capture_handle)
      snd_pcm_close(capture_handle);
    if (playback_handle)
      snd_pcm_close(playback_handle);

    //delete[] buff;
    delete[] conv;

    //return stop ? 1 : 0;
    return 0;
}

