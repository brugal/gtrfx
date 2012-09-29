#ifndef wav_h_included
#define wav_h_included

#ifdef __cplusplus
extern "C" {
#endif

// http://svn.xiph.org/trunk/ao/src/ao_wav.c

#define WAVE_FORMAT_PCM  0x0001
#define FORMAT_MULAW     0x0101
#define IBM_FORMAT_ALAW  0x0102
#define IBM_FORMAT_ADPCM 0x0103

#define WAV_HEADER_LEN 44

#define WRITE_U32(buf, x) *(buf)     = (unsigned char)(x&0xff);\
    *((buf)+1) = (unsigned char)((x>>8)&0xff);\
    *((buf)+2) = (unsigned char)((x>>16)&0xff);\
    *((buf)+3) = (unsigned char)((x>>24)&0xff);

#define WRITE_U16(buf, x) *(buf)     = (unsigned char)(x&0xff);\
    *((buf)+1) = (unsigned char)((x>>8)&0xff);

#define DEFAULT_SWAP_BUFFER_SIZE 2048

struct riff_struct {
    unsigned char id[4];   /* RIFF */
    unsigned int len;
    unsigned char wave_id[4]; /* WAVE */
};


struct chunk_struct
{
    unsigned char id[4];
    unsigned int len;
};

struct common_struct
{
    unsigned short wFormatTag;
    unsigned short wChannels;
    unsigned int dwSamplesPerSec;
    unsigned int dwAvgBytesPerSec;
    unsigned short wBlockAlign;
    unsigned short wBitsPerSample;  /* Only for PCM */
};

struct wave_header
{
    struct riff_struct   riff;
    struct chunk_struct  format;
    struct common_struct common;
    struct chunk_struct  data;
};

#ifdef __cplusplus
}
#endif
#endif  // wav_h_included
