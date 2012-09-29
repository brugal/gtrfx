#ifndef sound_h_included
#define sound_h_included

#ifdef __cplusplus
extern "C" {
#endif

//FIXME this needs to change -- support other than alsa
// 512 for a long time
#define PERIODSIZE 256  //(1024 * 2) //512 //256  //(1024 * 2)  //512  //256
#define PERIODS 16

#define BUF_SIZE (PERIODSIZE * PERIODS)

unsigned int SampleRate;


void *gtrfx_main_thread (void *data);

#ifdef __cplusplus
}
#endif
#endif  // sound_h_included
