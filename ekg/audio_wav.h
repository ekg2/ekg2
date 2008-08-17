/* *.wav I/O stolen from xawtv (recode program) which was stolen from cdda2wav */
/* Copyright (C) by Heiko Eissfeldt */

typedef uint8_t   BYTE;
typedef uint16_t  WORD;
typedef uint32_t  DWORD;
typedef uint32_t  FOURCC;	/* a four character code */

/* flags for 'wFormatTag' field of WAVEFORMAT */
#define WAVE_FORMAT_PCM 1

/* MMIO macros */
#define mmioFOURCC(ch0, ch1, ch2, ch3) \
  ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) | \
  ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24))

#define FOURCC_RIFF	mmioFOURCC ('R', 'I', 'F', 'F')
#define FOURCC_LIST	mmioFOURCC ('L', 'I', 'S', 'T')
#define FOURCC_WAVE	mmioFOURCC ('W', 'A', 'V', 'E')
#define FOURCC_FMT	mmioFOURCC ('f', 'm', 't', ' ')
#define FOURCC_DATA	mmioFOURCC ('d', 'a', 't', 'a')

typedef struct CHUNKHDR {
    FOURCC ckid;		/* chunk ID */
    DWORD dwSize;		/* chunk size */
} CHUNKHDR;

/* simplified Header for standard WAV files */
typedef struct WAVEHDR {
    CHUNKHDR chkRiff;
    FOURCC fccWave;
    CHUNKHDR chkFmt;
    WORD wFormatTag;	   /* format type */
    WORD nChannels;	   /* number of channels (i.e. mono, stereo, etc.) */
    DWORD nSamplesPerSec;  /* sample rate */
    DWORD nAvgBytesPerSec; /* for buffer estimation */
    WORD nBlockAlign;	   /* block size of data */
    WORD wBitsPerSample;
    CHUNKHDR chkData;
} WAVEHDR;

#define cpu_to_le32(x) (x)
#define cpu_to_le16(x) (x)
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)

static void *audio_wav_set_header(const char *freq, const char *sample, const char *channels) {
	WAVEHDR *fileheader;
	int rate, nchannels, nBitsPerSample;

	if (!freq || !sample || !channels) 
		return NULL;

	rate		= atoi(freq);
	nchannels	= atoi(channels);
	nBitsPerSample	= atoi(sample);

	fileheader = xmalloc(sizeof(WAVEHDR));

	/* stolen from xawtv && cdda2wav */
	unsigned long nBlockAlign = nchannels * ((nBitsPerSample + 7) / 8);
	unsigned long nAvgBytesPerSec = nBlockAlign * rate;
	unsigned long temp = /* data length */ 0 + sizeof(WAVEHDR) - sizeof(CHUNKHDR);

	fileheader->chkRiff.ckid    = cpu_to_le32(FOURCC_RIFF);
	fileheader->fccWave	    = cpu_to_le32(FOURCC_WAVE);
	fileheader->chkFmt.ckid     = cpu_to_le32(FOURCC_FMT);
	fileheader->chkFmt.dwSize   = cpu_to_le32(16);
	fileheader->wFormatTag	    = cpu_to_le16(WAVE_FORMAT_PCM);
	fileheader->nChannels	    = cpu_to_le16(nchannels);
	fileheader->nSamplesPerSec  = cpu_to_le32(rate);
	fileheader->nAvgBytesPerSec = cpu_to_le32(nAvgBytesPerSec);
	fileheader->nBlockAlign     = cpu_to_le16(nBlockAlign);
	fileheader->wBitsPerSample  = cpu_to_le16(nBitsPerSample);
	fileheader->chkData.ckid    = cpu_to_le32(FOURCC_DATA);
	fileheader->chkRiff.dwSize  = cpu_to_le32(temp);
	fileheader->chkData.dwSize  = cpu_to_le32(0 /* data length */);

	return fileheader;

}

