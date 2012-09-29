#ifndef tuneit_h_included
#define tuneit_h_included

#ifdef __cplusplus
extern "C" {
#endif

void tuneit_init (unsigned int sampleRate);
void tuneit_runf (int nframes, float *indata);
void tuneit_runi (int nframes, signed short int *indata);

#ifdef __cplusplus
}
#endif
#endif  // tuneit_h_included
