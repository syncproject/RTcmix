#include "../H/ugens.h"
#include "../H/sfheader.h"
#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "../rtstuff/rtdefs.h"


extern int	     isopen[NFILES];        /* open status */
extern SFHEADER      sfdesc[NFILES];
extern SFMAXAMP      sfm[NFILES];
extern struct stat   sfst[NFILES];
extern int headersize[NFILES];

extern int rtInputIndex;
extern InputDesc inputFileTable[];


double m_sr(p,n_args)
float *p;
{
  if(!isopen[(int)p[0]]) {
    fprintf(stderr, "You haven't opened file %d yet!\n", (int)p[0]);
    closesf();
  }
  return(sfsrate(&sfdesc[(int)p[0]]));
}

double m_chans(p,n_args)
float *p;
{	
  if(!isopen[(int)p[0]]) {
    fprintf(stderr, "You haven't opened file %d yet!\n", (int)p[0]);
    closesf();
  }
  
  return(sfchans(&sfdesc[(int)p[0]]));
}

double m_class(p,n_args)
float *p;
{
  if(!isopen[(int)p[0]]) {
    fprintf(stderr, "You haven't opened file %d yet!\n", (int)p[0]);
    closesf();
  }
  return(sfclass(&sfdesc[(int)p[0]]));
}

// Still uses old style soundfile IO arrays, which are now updated with sndlib
// We need to kill that old beast completely!

double m_dur(p,n_args)
float *p;
{
	int i;
	float dur;
	i = p[0];
	if(!isopen[i]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", i);
		closesf();
	}
	dur = (float)(sfst[i].st_size - headersize[i])
		 /(float)sfclass(&sfdesc[i])/(float)sfchans(&sfdesc[i])
		 /sfsrate(&sfdesc[i]);
	return(dur);
}

double m_DUR(float *p, int n_args)   /* returns duration for rtinput() files */
{
  if (rtInputIndex < 0) {
    fprintf(stderr, "There are no currently opened input files!\n");
    return 0.0;
  }
  return(inputFileTable[rtInputIndex].dur);
}

#ifdef USE_SNDLIB

/* Note: the old versions of the peak info functions copy peak stats from
   the file header in memory into the sfm[fno] array maintained in sound.c.
   This seems unnecessary, since both are initialized on opening any file
   and updated on endnote. When would they ever be different from the
   perspective of these info functions, which can't be called from Minc
   in the *middle* of an instrument run? Are they also called internally,
   like m_dur?  -JGG
*/ 
double
m_peak(float p[], int n_args)
{
	int      n, fno;
	float    peak, chanpeak;

	fno = p[0];
	if (!isopen[fno]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", fno);
		closesf();
	}

	peak = 0.0;

	if (sfmaxamptime(&sfm[fno]) > 0L) {
		for (n = 0; n < sfchans(&sfdesc[fno]); n++) {
			chanpeak = sfmaxamp(&sfm[fno], n);
			if (chanpeak > peak)
				peak = chanpeak;
		}
	}
	else
		fprintf(stderr, "File %d has no peak stats!\n", fno);

	return peak;
}


double
m_left(float p[], int n_args)
{
	int      fno;

	fno = p[0];
	if (!isopen[fno]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", fno);
		closesf();
	}

	if (sfmaxamptime(&sfm[fno]) > 0L)
		return (sfmaxamp(&sfm[fno], 0));    /* for channel 0 */
	else
		fprintf(stderr, "File %d has no peak stats!\n", fno);

	return 0.0;
}


double
m_right(float p[], int n_args)
{
	int      fno;

	fno = p[0];
	if (!isopen[fno]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", fno);
		closesf();
	}

	if (sfmaxamptime(&sfm[fno]) > 0L)
		return (sfmaxamp(&sfm[fno], 1));    /* for channel 1 */
	else
		fprintf(stderr, "File %d has no peak stats!\n", fno);

	return 0.0;
}


#else /* !USE_SNDLIB */


double m_peak(p,n_args)
float *p;
{
	char *cp,*getsfcode();
	int i,j;
	float peak;
	j = p[0];
	if(!isopen[j]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", j);
		closesf();
	}
	cp = getsfcode(&sfdesc[j],SF_MAXAMP);
	bcopy(cp + sizeof(SFCODE), (char *) &sfm[j], sizeof(SFMAXAMP));
	if(cp != NULL) {
		for(i=0,peak=0; i<sfchans(&sfdesc[j]); i++)
			if(sfmaxamp(&sfm[j],i) > peak) peak=sfmaxamp(&sfm[j],i);
		return(peak);
		}
	return(0.);
}

double m_left(p,n_args)
float *p;
{
	char *cp,*getsfcode();
	int i,j;
	j = p[0];
	if(!isopen[j]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", j);
		closesf();
	}
	cp = getsfcode(&sfdesc[j],SF_MAXAMP);
	bcopy(cp + sizeof(SFCODE), (char *) &sfm[j], sizeof(SFMAXAMP));
	if(cp != NULL) 
		return(sfmaxamp(&sfm[j],0));
	return(0.);
}

double m_right(p,n_args)
float *p;
{
	char *cp,*getsfcode();
	int i,j;
	j = p[0];
	if(!isopen[j]) {
		fprintf(stderr, "You haven't opened file %d yet!\n", j);
		closesf();
	}
	if(sfchans(&sfdesc[j]) == 1) return(0);
	cp = getsfcode(&sfdesc[j],SF_MAXAMP);
	bcopy(cp + sizeof(SFCODE), (char *) &sfm[j], sizeof(SFMAXAMP));
	if(cp != NULL) 
		return(sfmaxamp(&sfm[j],1));
	return(0.);
}


#endif /* !USE_SNDLIB */


#ifdef USE_SNDLIB

#include <sndlibsupport.h>

#define ALL_CHANS -1

extern double inSR;           /* defined in rtinput.c */
extern int    inNCHANS;

/* Returns peak for <chan> of current RT input file, between <start> and
   <end> times (in seconds). If <chan> is -1, returns the highest peak
   of all the channels. If <end> is 0, sets <end> to duration of file.
*/
static float
get_peak(float start, float end, int chan)
{
   int       n, fd, result, nchans, srate, dataloc, format, file_stats_valid;
   long      startframe, endframe, nframes, loc;
   long      peakloc[MAXCHANS];
   float     peak[MAXCHANS];
   SFComment sfc;

   if (rtInputIndex < 0) {
      fprintf(stderr, "There are no currently opened input files!\n");
      return -1.0;
   }

   if (end == 0.0)
      end = inputFileTable[rtInputIndex].dur;       /* use end time of file */

   fd = inputFileTable[rtInputIndex].fd;

// *** If end - start is, say, 60 seconds, less trouble to just analyze file
// than to dig through file header?

   /* Try to use peak stats in file header. */
   if (sndlib_get_header_comment(fd, &sfc) == -1)
      return -1.0;         /* this call prints an err msg */

   if (SFCOMMENT_PEAKSTATS_VALID(&sfc)) {
      struct stat statbuf;
      fstat(fd, &statbuf);
      if (statbuf.st_mtime <= sfc.timetag + 2)
         file_stats_valid = 1;
   }
   else
      file_stats_valid = 0;    /* file written since peak stats were updated */

   format = inputFileTable[rtInputIndex].data_format;
   dataloc = inputFileTable[rtInputIndex].data_location;
   srate = (float)inSR;
   nchans = inNCHANS;

   startframe = (long)(start * srate + 0.5);
   endframe = (long)(end * srate + 0.5);
   nframes = (endframe - startframe) + 1;

   if (file_stats_valid) {
      int c = 0;
      if (chan == ALL_CHANS) {
         float max = 0.0;
         for (n = 0; n < nchans; n++) {
            if (sfc.peak[n] > max) {
               max = sfc.peak[n];
               c = n;
            }
         }
         loc = sfc.peakloc[c];
      }
      else {
         loc = sfc.peakloc[chan];
         c = chan;
      }
      if (loc >= startframe && loc <= endframe)
         return sfc.peak[c];
   }

   /* NOTE: Might get here even if file_stats_valid. */
   result = sndlib_findpeak(fd, -1, dataloc, -1, format, nchans,
                            startframe, nframes, peak, peakloc);

   if (chan == ALL_CHANS) {
      float max = 0.0;
      for (n = 0; n < nchans; n++) {
         if (peak[n] > max)
            max = peak[n];
      }
      return max;
   }
   else
      return peak[chan];
}


double
m_PEAK(float p[], int n_args)
{
   return get_peak(p[0], p[1], ALL_CHANS);
}


double
m_LEFT_PEAK(float p[], int n_args)
{
   return get_peak(p[0], p[1], 0);
}


double
m_RIGHT_PEAK(float p[], int n_args)
{
   return get_peak(p[0], p[1], 1);
}


#endif /* !USE_SNDLIB */


extern int sfd[NFILES];

double
m_info(p,n_args)
float *p;
{
  sfstats(sfd[(int) p[0]]);
    return 0;
}
