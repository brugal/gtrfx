#include "sound.h"
#include "main.h"
#include "tuneit.h"
#include "common.h"
#include <alsa/asoundlib.h>

static snd_pcm_t *playback_handle;
static snd_pcm_t *capture_handle;

static int dataSize;

void *gtrfx_main_thread (void *data)
{
    int err;
    int i, j;
    unsigned int rate;
    signed short buf[PERIODSIZE * PERIODS];
    signed short stereoData[PERIODSIZE * PERIODS];
    //unsigned int resample;
    snd_pcm_hw_params_t *hw_params_capture;
    snd_pcm_hw_params_t *hw_params_playback;
    snd_pcm_uframes_t capBufferSize, playBufferSize;
    int periods = PERIODS;
    snd_pcm_uframes_t periodsize = PERIODSIZE;  // / 2;
    int captureChannels = 2;
    int playbackChannels = 2;
    //long int totalx = 0;
    gtkthread_t *args = data;
    //FIXME
    //time_t sec;
    unsigned int loops;
    snd_pcm_sframes_t framesLeft;

    //FIXME exit(1) thread
    if (args->argc < 3) {
        fprintf(stderr, "usage: %s <capture device> <playback device>\n", args->argv[0]);
        exit(1);
    }

    //sec = time(NULL);

    //FIXME parse_config_file() uses SampleRate
    if (args->argc < 4)
        parse_config_file(NULL);
    else
        parse_config_file(args->argv[3]);

    printf("\n");

    //////////   capture    /////////////

    if ((err = snd_pcm_open(&capture_handle, args->argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "cannot open audio capture device %s (%s)\n",
                args->argv[1],
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

    rate = SampleRate;
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
        fprintf(stderr, "Error setting capture buffersize (%s)\n", snd_strerror(err));
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

    if ((err = snd_pcm_open(&playback_handle, args->argv[2], SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "cannot open audio playback device %s (%s)\n",
                args->argv[2],
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

    rate = SampleRate;
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

    tuneit_init(SampleRate);

    loops = 0;
    while (1) {
        loops++;

        while (!programExit) {
            err = snd_pcm_readi(capture_handle, buf, periodsize >> 2);
            dataSize = err;
            //printf("dataSize: %d\n", dataSize);

            if (err >= 0) {
                if (captureChannels == 2) {
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
                    xerror();
                }

                for (i = 0;  i < dataSize;  i++) {
                    if (bufDataPosIn >= BUFDATA_SIZE) {
                        bufDataPosIn = 0;
                    }
                    bufDataIn[bufDataPosIn] = ldata[i];
                    bufDataPosIn++;
                }


                if (optionTuner) {
                    tuneit_runf(err, ldata);
                    tuner(err);
                }

                ////////////////////////////
                /////  add effects  ////////
                ////////////////////////////

                apply_effects(err);

                for (i = 0;  i < dataSize;  i++) {
                    if (bufDataPosOut >= BUFDATA_SIZE) {
                        bufDataPosOut = 0;
                    }
                    bufDataOut[bufDataPosOut] = ldata[i];
                    bufDataPosOut++;
                }

                //FIXME check that output channels == 2
                // convert to interleaved and short int for playback
                for (i = 0;  i < err;  i++) {
                    stereoData[2 * i] = (signed short int)(ldata[i] * 32767.5f);
                    stereoData[2 * i + 1] = (signed short int)(rdata[i] * 32767.5f);
                }

                break;
            } else {
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
        }

#if 0
        while ((err = snd_pcm_writei(playback_handle, stereoData, periodsize >> 2)) < 0) {
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
#endif

        framesLeft = periodsize >> 2;
        while (1) {
            err = snd_pcm_writei(playback_handle, stereoData, framesLeft);
            if (err < 0) {
                err = snd_pcm_recover(playback_handle, err, 1);
            }
            if (err < 0) {
                static time_t print_time;
                time_t currTime;

                currTime = time(NULL);
                if (print_time != currTime) {
                    fprintf(stderr, "%d playback xrun %d (%s)\n", loops, err, snd_strerror(err));
                    print_time = currTime;
                }
                snd_pcm_prepare(playback_handle);
                break;
            }
            framesLeft -= err;
            if (framesLeft < 0) {
                fprintf(stderr, "%d playback overwrote ?? %d\n", loops, (int)framesLeft);
            }
            if (framesLeft == 0) {
                break;
            }

            //printf("%d frames left\n", (int)framesLeft);
        }
    }

    printf("all done\n");

    if (hw_params_capture)
        snd_pcm_hw_params_free(hw_params_capture);
    if (hw_params_playback)
        snd_pcm_hw_params_free(hw_params_playback);
    if (capture_handle)
        snd_pcm_close(capture_handle);
    if (playback_handle)
        snd_pcm_close(playback_handle);

    return 0;
}
