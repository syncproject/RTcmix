#ifdef USE_SNDLIB

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../H/sndlibsupport.h"

/* #define DEBUG */

#ifndef TRUE
 #define TRUE (1)
#endif
#ifndef FALSE
 #define FALSE (0)
#endif

enum {
   UNSPECIFIED_ENDIAN = 0,
   CREATE_BIG_ENDIAN,
   CREATE_LITTLE_ENDIAN
};

#define DEFAULT_SRATE          44100
#define DEFAULT_NCHANS         2
#define DEFAULT_ENDIAN         CREATE_BIG_ENDIAN
#define DEFAULT_IS_SHORT       TRUE
#define DEFAULT_FORMAT_NAME    "aiff"

#define MINSRATE               11025
#define MAXSRATE               96000

#define USAGE_MSG  "\
  option     description                    default                     \n\
  --------------------------------------------------------------------- \n\
  -r NUM     sampling rate                  [44100]                     \n\
  -c NUM     number of channels             [2]                         \n\
  -i or -f   16-bit integer or 32-bit float [16-bit integer]            \n\
  -b or -l   big-endian or little-endian    [LE for wav, BE for others] \n\
  -t NAME    file format name; one of...    [aiff, aifc for float]      \n\
             aiff, aifc, wav, next, sun, ircam                          \n\
             (sun is a synonym for next)                                \n\
                                                                        \n\
  Defaults take effect unless overridden by supplied values.            \n\
                                                                        \n\
  If filename exists and seems to be a sound file, its header will      \n\
  be overwritten with the one specified. If that might result in some   \n\
  loss or corruption of sound data, the program will warn about this    \n\
  and require you to use the \"--force\" flag on the command line.      \n\
                                                                        \n\
  NOTE: The following combinations are not available:                   \n\
        aiff and -l                                                     \n\
        aiff and -f (will substitute aifc here)                         \n\
        aifc and -f and -l together (aifc floats are BE only)           \n\
        ircam and -l                                                    \n\
        next and -l                                                     \n\
        wav and -b                                                      \n\
"


#define OVERWRITE_WARNING                                                 \
"You're asking to change the header type or sound data format of an     \n\
existing file. Changing the header type could result in good samples    \n\
deleted from the beginning of the file (or garbage samples added there).\n\
Also, channels could be swapped; sample words could even be corrupted.  \n\
Changing the data format specified in the header would cause a program  \n\
to interpret the existing sound data in a new way, probably as loud,    \n\
painful noise. If you still want to write over the header, run the      \n\
program with the \"--force\" option."

#define WORD_INTEGRITY_WARNING                                            \
"WARNING: Sample words have been corrupted!"

#define CHANNEL_SWAP_WARNING                                              \
"WARNING: Channels have been swapped!"


char *progname, *sfname = NULL;
int srate = DEFAULT_SRATE;
int nchans = DEFAULT_NCHANS;
int is_short = DEFAULT_IS_SHORT;
int endian = UNSPECIFIED_ENDIAN;
char comment[DEFAULT_COMMENT_LENGTH] = "";

#define FORMAT_NAME_LENGTH 32
char format_name[FORMAT_NAME_LENGTH] = DEFAULT_FORMAT_NAME;

/* these are assigned in check_params */
static int header_type;
static int data_format;
static int is_aifc = FALSE;

static void usage(void);
static int check_params(void);


/* ---------------------------------------------------------------- usage --- */
static void
usage()
{
   printf("\nusage: \"%s [options] filename\"\n\n", progname);
   printf("%s", USAGE_MSG);
   exit(1);
}


/* --------------------------------------------------------- check_params --- */
/* Convert user specifications into a form we can hand off to sndlib.
   Validate input, and make sure the combination of parameters specifies
   a valid sound file format type (i.e., one whose header sndlib can write).
   See the usage message for the types we allow.
*/
static int
check_params()
{
   if (nchans < 1 || nchans > MAXCHANS) {
      fprintf(stderr, "Number of channels must be between 1 and %d.\n\n",
              MAXCHANS);
      exit(1);
   }
   if (srate < MINSRATE || srate > MAXSRATE) {
      fprintf(stderr, "Sampling rate must be between %d and %d.\n\n",
              MINSRATE, MAXSRATE);
      exit(1);
   }

   if (strcasecmp(format_name, "aiff") == 0)
      header_type = AIFF_sound_file;
   else if (strcasecmp(format_name, "aifc") == 0) {
      header_type = AIFF_sound_file;
      is_aifc = TRUE;
   }
   else if (strcasecmp(format_name, "wav") == 0)
      header_type = RIFF_sound_file;
   else if (strcasecmp(format_name, "next") == 0)
      header_type = NeXT_sound_file;
   else if (strcasecmp(format_name, "sun") == 0)
      header_type = NeXT_sound_file;
   else if (strcasecmp(format_name, "ircam") == 0)
      header_type = IRCAM_sound_file;
   else {
      fprintf(stderr, "Invalid file format name: %s\n", format_name);
      fprintf(stderr, "Valid names: aiff, aifc, wav, next, sun, ircam\n");
      exit(1);
   }

   if (endian == UNSPECIFIED_ENDIAN) {
      if (header_type == RIFF_sound_file)
         endian = CREATE_LITTLE_ENDIAN;
      else
         endian = DEFAULT_ENDIAN;
   }

   if (endian == CREATE_BIG_ENDIAN)
      data_format = is_short? snd_16_linear : snd_32_float;
   else
      data_format = is_short? snd_16_linear_little_endian :
                                                   snd_32_float_little_endian;

   /* Check for combinations of parameters we don't allow.
      See the NOTE at bottom of usage message.
   */
   if (header_type == AIFF_sound_file) {
      if (!is_aifc && endian == CREATE_LITTLE_ENDIAN) {
         fprintf(stderr, "AIFF little-endian header not allowed.\n");
         exit(1);
      }
      if (!is_aifc && !is_short) {
         is_aifc = TRUE;
         fprintf(stderr, "WARNING: using AIFC header for floats.\n");
      }
      if (is_aifc && data_format == snd_32_float_little_endian) {
         fprintf(stderr, "AIFC little-endian float header not allowed.\n");
         exit(1);
      }
   }
   if (header_type == IRCAM_sound_file && endian == CREATE_LITTLE_ENDIAN) {
      fprintf(stderr, "IRCAM little-endian header not allowed.\n");
      exit(1);
   }
   if (header_type == NeXT_sound_file && endian == CREATE_LITTLE_ENDIAN) {
      fprintf(stderr, "NeXT little-endian header not allowed.\n");
      exit(1);
   }
   if (header_type == RIFF_sound_file && endian == CREATE_BIG_ENDIAN) {
      fprintf(stderr, "RIFF big-endian header not allowed.\n");
      exit(1);
   }

   /* Tell sndlib which AIF flavor to write. */
   if (header_type == AIFF_sound_file)
      set_aifc_header(is_aifc);

#ifdef DEBUG
   printf("%s: nchans=%d, srate=%d, fmtname=%s, %s, %s\n",
          sfname, nchans, srate, format_name,
          is_short? "short" : "float",
          endian? "bigendian" : "littleendian");
#endif

   return 0;
}


/* ----------------------------------------------------------------- main --- */
/* Create a soundfile header of the specified type. The usage msg above
   describes the kind of types you can create. (These are ones that
   sndlib can write and that can be useful to the average cmix user.
   There are other kinds of header sndlib can write -- for example,
   for 24-bit files -- but we want to keep the sfcreate syntax as
   simple as possible.

   We allocate a generous comment area, because expanding it after a
   sound has already been written would mean copying the entire file.
   (We use comments to store peak stats -- see sys/sndlibsupport.c.)
*/
int
main(int argc, char *argv[])
{
   int         i, fd, result, overwrite_file, old_format;
   int         data_location, old_data_location, old_nsamps, old_datum_size;
   int         force = FALSE;
   struct stat statbuf;

   /* get name of this program */
   progname = strrchr(argv[0], '/');
   if (progname == NULL)
      progname = argv[0];
   else
      progname++;

   if (argc < 2)
      usage();

   for (i = 1; i < argc; i++) {
      char *arg = argv[i];

      if (arg[0] == '-') {
         switch (arg[1]) {
            case 'c':
               if (++i >= argc)
                  usage();
               nchans = atoi(argv[i]);
               break;
            case 'r':
               if (++i >= argc)
                  usage();
               srate = atoi(argv[i]);
               break;
            case 'i':
               is_short = TRUE;
               break;
            case 'f':
               is_short = FALSE;
               break;
            case 'b':
               endian = CREATE_BIG_ENDIAN;
               break;
            case 'l':
               endian = CREATE_LITTLE_ENDIAN;
               break;
            case 't':
               if (++i >= argc)
                  usage();
               strncpy(format_name, argv[i], FORMAT_NAME_LENGTH - 1);
               format_name[FORMAT_NAME_LENGTH - 1] = 0; /* ensure termination */
               break;
            case '-':
               if (strcmp(arg, "--force") == 0)
                  force = TRUE;
               else
                  usage();
               break;
            default:  
               usage();
         }
      }
      else
         sfname = arg;
   }
   if (sfname == NULL)
      usage();

   if (check_params())
      usage();

   old_data_location = old_nsamps = 0;

   /* Test for existing file. If there is one, and we can read and write it,
      and it seems to be a sound file, and the force flag is set ...
      then we'll overwrite its header.
      If there isn't an existing file, we'll create a new one.
   */
   overwrite_file = FALSE;
   result = stat(sfname, &statbuf);
   if (result == -1) {
      if (errno == ENOENT) {       /* file doesn't exist, we'll create one */
         fd = open(sfname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
         if (fd == -1) {
            fprintf(stderr, "Error creating file \"%s\" (%s)\n",
                                                      sfname, strerror(errno));
            exit(1);
         }
      }
      else {
         fprintf(stderr,
              "File \"%s\" exists, but there was an error accessing it (%s)\n",
                                                      sfname, strerror(errno));
         exit(1);
      }
   }
   else {
      int type, drastic_change;

      overwrite_file = TRUE;

      /* File exists and we could stat it. If it's a regular file, open
         it and see if it looks like a sound file.
      */
      if (!S_ISREG(statbuf.st_mode)) {
         fprintf(stderr, "\"%s\" exists, but it's not a regular file.\n",
                                                                      sfname);
         exit(1);
      }
      fd = open(sfname, O_RDWR);
      if (fd == -1) {
         fprintf(stderr, "Error opening file (%s)\n", strerror(errno));
         exit(1);
      }
      if (sndlib_read_header(fd) == -1) {
         fprintf(stderr, "Error reading header (%s)\n", strerror(errno));
         exit(1);
      }
      type = c_snd_header_type();
      old_format = c_snd_header_format();
      if (NOT_A_SOUND_FILE(type) || INVALID_DATA_FORMAT(old_format)) {
         fprintf(stderr,
               "\"%s\" exists, but doesn't look like a sound file.\n", sfname);
         exit(1);
      }

      if (type == AIFF_sound_file
                               && is_aifc != sndlib_current_header_is_aifc())
         drastic_change = TRUE;
      else if (type != header_type || old_format != data_format)
         drastic_change = TRUE;
      else
         drastic_change = FALSE;

      if (drastic_change && !force) {
         fprintf(stderr, "%s\n", OVERWRITE_WARNING);
         exit(1);
      }

      old_data_location = c_snd_header_data_location();
      old_nsamps = c_snd_header_data_size();         /* samples, not frames */
      old_datum_size = c_snd_header_datum_size();
   }

   result = sndlib_write_header(fd, 0, header_type, data_format, srate, nchans,
                                                        NULL, &data_location);
   if (result == -1) {
      fprintf(stderr, "Error writing header (%s)\n", strerror(errno));
      exit(1);
   }

   /* If we're overwriting a header, we have to fiddle around a bit
      to get the correct data_size into the header.
      (These will not likely be the same if we're changing header types.)
   */
   if (overwrite_file) {
      int loc_byte_diff, datum_size, sound_bytes;

      /* need to do this again */
      if (sndlib_read_header(fd) == -1) {
         fprintf(stderr, "Error re-reading header (%s)\n", strerror(errno));
         exit(1);
      }

      if (data_format != old_format)
        if (! FORMATS_SAME_BYTE_ORDER(data_format, old_format))
            printf("WARNING: Byte order changed!\n");

      datum_size = c_snd_header_datum_size();

      loc_byte_diff = data_location - old_data_location;

      /* If the data locations have changed, we're effectively adding or
         subtracting sound data bytes. Depending on the number of bytes
         added, this could result in swapped channels or worse. We can't
         do anything about this, because our scheme for encoding peak stats
         in the header comment requires a fixed comment allocation in the
         header. (Otherwise, we could shrink or expand this to make things
         right.) The best we can do is warn the user.
      */
      if (loc_byte_diff) {
         if (loc_byte_diff > 0)
            printf("Losing %d bytes of sound data\n", loc_byte_diff);
         else if (loc_byte_diff < 0)
            printf("Gaining %d bytes of sound data\n", -loc_byte_diff);

         if (loc_byte_diff % datum_size)
            printf("%s\n", WORD_INTEGRITY_WARNING);
         else if (loc_byte_diff % (nchans * datum_size))
            printf("%s\n", CHANNEL_SWAP_WARNING);
         /* else got lucky: no shifted words or swapping */

         sound_bytes = (old_nsamps * old_datum_size) - loc_byte_diff;  
      }
      else                               /* same number of bytes as before */
         sound_bytes = old_nsamps * old_datum_size;  

      result = sndlib_set_header_data_size(fd, header_type, sound_bytes);
      if (result == -1) {
         fprintf(stderr, "Error updating header\n");
         exit(1);
      }

      close(fd);
   }
   else
      close(fd);

   return 0;
}



/*****************************************************************************/
#else /* !USE_SNDLIB */
/*****************************************************************************/

#include "../H/byte_routines.h"
#include "../H/sfheader.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>

int swap;

static SFCODE	ampcode = {
	SF_MAXAMP,
	sizeof(SFMAXAMP) + sizeof(SFCODE)
}; 

static SFCODE	commentcode = {
	SF_COMMENT,
	MINCOMM + sizeof(SFCODE)
	};

#define NBYTES 16384


int check_endian() {
  int i = 1;
  
  if(*(char *)&i) 
    return 0; /* Little endian */
  else
    return 1; /* Big endian */
}


main(argc,argv)

int argc;
char *argv[];

{
	int i,sf,nchars=MINCOMM;
	char *buffer;
#ifndef sgi
	char *malloc();
#endif
	int nbytes,todo;
	int write_big_endian;  /* Defaults to a big endian header */
	int big_endian;
	int tswap;
	int isnative = 0;
	long atol();
	float dur;
	int comment = 0;
	double atof();
	SFHEADER sfh;
	SFMAXAMP sfm;
	SFCOMMENT sfcm;
	FILE *fcom;
	char *sfname,*cp;
	struct stat st;

	write_big_endian = 1;  /* The default */
	swap = 0;  /* This gets set below */
	big_endian = check_endian();



usage:	if(argc < 7) {
		printf("usage: \"sfcreate -r [s. rate] -c [# chans] -[i=int; f=float; m=mu-law] -l[little_endian] -b[big_endian] <-w<x commentsize>> <-d [dur]> filename\"\n");
		exit(1);
		}
	
	dur = 0;

	while((*++argv)[0] == '-') {
		argc -= 2; /* Take away two args */
		for(cp = argv[0]+1; *cp; cp++) {
			switch(*cp) { /* Grap options */
			case 'r': 
				sfsrate(&sfh) = atof(*++argv);
				if(sfsrate(&sfh) < 0 || sfsrate(&sfh) > 60000) {
					printf("Illegal srate\n");
					exit(1);
				}
				printf("Sampling rate set to %f\n",sfsrate(&sfh));
				break;
			case 'd': 
				dur = atof(*++argv);
				printf("Play duration is %f\n",dur);
				break;
			case 'i': 
				sfclass(&sfh) = SF_SHORT;
				break;
			case 'f':
				sfclass(&sfh) = SF_FLOAT;
				break;
			case 'l':
				write_big_endian = 0;
				break;
			case 'b':
			        write_big_endian = 1;  /* Default */
				break;
			case 'c': 
				sfchans(&sfh) = atoi(*++argv);
				if(sfchans(&sfh) != 1 && sfchans(&sfh) != 2 && sfchans(&sfh) != 4) {
					printf("Illegal channel specification\n");
					exit(1);
				}
				printf("Number of channels set to %d\n",sfchans(&sfh));
				break;
			case 'w':
				if(*(argv[0]+2) == 'x') {
					nchars = atoi(*++argv);
					++cp;
					}
				comment = 1;
				break;
			default:  
				printf("Don't know about option: %c\n",*cp);
			}
		}

	}

	/* This is the condition defining when we need to do byte swapping */
	if (((!big_endian) && write_big_endian) || (big_endian && (!write_big_endian))) {
	  swap = 1;
	}

	if((sfsrate(&sfh) == 0.) || (sfclass(&sfh) == 0) 
			|| (sfchans(&sfh) == 0))  {
		printf("********You are missing specifications!\n");
			goto usage;
	}

	/* This looks wierd, no? */
	/* May want to change it just to set the magic # type */
	/*	sfmagic(&sfh) = isnative ? 0 : SF_MAGIC; */
	sfmagic(&sfh) = SF_MAGIC;
        sfname = argv[0];

	if((sf = open(sfname,O_CREAT|O_RDWR,0644)) < 0 ) {
		printf("Can't open file %s\n",sfname);
		exit(2);
		}
	if(comment && isnative)
		printf("You cannot write a comment into a native file with this program.\n");
	if(isnative) goto writeit;		/* goto  == laziness */

	/* put in peak amps of 0 */
	for(i=0; i<sfchans(&sfh); i++)
		sfmaxamp(&sfm,i)=sfmaxamploc(&sfm,i)=sfmaxamploc(&sfm,i)=0;
	sfmaxamptime(&sfm) = 0;

	/* I wonder if this makes a difference, since it's all 0 right now */
	/* Looks like it doesn't */
	if (swap) {
	  for (i=0;i<sfchans(&sfh);i++) {
	    byte_reverse4(&sfm.value[i]);
	    byte_reverse4(&sfm.samploc[i]);
	  }
	  byte_reverse4(&sfm.timetag);
	}

	/* Note that putsfcode does swapping and unswapping internally for the sfm vector */
	putsfcode(&sfh,&sfm,&ampcode);

	if(!comment) {
		strcpy(&sfcomm(&sfcm,0),sfname);
		sfcomm(&sfcm,strlen(sfname)) = '\n';
		commentcode.bsize = MAXCOMM + sizeof(SFCODE); 

		if (putsfcode(&sfh,&sfcm,&commentcode) < 0) {
			printf("comment didn't get written, sorry!\n");
			exit(1);
			}
	}
	else {
		system("vi /tmp/comment");
		fcom = fopen("/tmp/comment","r");
		i=0;
		while ( (sfcomm(&sfcm,i) = getc(fcom)) != EOF ) {
			if (++i > MAXCOMM) {
		        	printf("Gimme a break! I can only take %d characters\n",MAXCOMM);
				printf("comment truncated to %d characters\n",MAXCOMM);
				commentcode.bsize = MAXCOMM + sizeof(SFCODE);
				break;
				}
		}
		sfcomm(&sfcm,i) = '\0';
		system("rm /tmp/comment");
		if (nchars > MINCOMM)
				commentcode.bsize = nchars + sizeof(SFCODE);
		if (i > nchars)
				commentcode.bsize = i + sizeof(SFCODE);

		if (putsfcode(&sfh,&sfcm,&commentcode) < 0) {
				printf("comment didn't get written, sorry!\n");
				exit(1);
				}
	}

writeit:

	if(stat((char *)sfname,&st))  {
	  fprintf(stderr, "Couldn't stat file %s\n",sfname);
	  return(1);
	}
	
	NSchans(&sfh) = sfchans(&sfh);
	NSmagic(&sfh) = SND_MAGIC;
	NSsrate(&sfh) = sfsrate(&sfh);
	NSdsize(&sfh) = (st.st_size - 1024);
	NSdloc(&sfh) = 1024;
	
	switch(sfclass(&sfh)) {
	case SF_SHORT:
	  NSclass(&sfh) = SND_FORMAT_LINEAR_16;
	  break;
	case SF_FLOAT:
	  NSclass(&sfh) = SND_FORMAT_FLOAT;
	  break;
	default:
	  NSclass(&sfh) = 0;
	  break;
	}

	if (swap) {
	  /* Swap the main header info */
	  byte_reverse4(&(sfh.sfinfo.sf_magic));
	  byte_reverse4(&(sfh.sfinfo.sf_srate));
	  byte_reverse4(&(sfh.sfinfo.sf_chans));
	  byte_reverse4(&(sfh.sfinfo.sf_packmode)); 
	  byte_reverse4(&NSchans(&sfh));
	  byte_reverse4(&NSmagic(&sfh));
	  byte_reverse4(&NSsrate(&sfh));
	  byte_reverse4(&NSdsize(&sfh));
	  byte_reverse4(&NSdloc(&sfh));
	  byte_reverse4(&NSclass(&sfh));
	}


	if(wheader(sf,(char *)&sfh)) {
	       printf("Can't seem to write header on file %s\n",sfname);
		perror("main");
		exit(1);
	}
	
	if(dur) {
		nbytes = todo = dur * sfsrate(&sfh) * (float)sfchans(&sfh)
			 * (float)sfclass(&sfh) + .5;
		fprintf(stderr,"Blocking out file, %d bytes...",nbytes);
		buffer = (char *)malloc(NBYTES);
		/*bzero(buffer,NBYTES);*/
		while(nbytes>0) {
			todo = (nbytes > NBYTES) ? NBYTES : nbytes;
			if(write(sf,buffer,todo) <= 0) {
				printf("Bad write on file\n");
				exit(1);
			}
			nbytes -= todo;
		}
		printf("\ndone\n");
	}
	putlength(sfname,sf,&sfh);
}

#endif /* !USE_SNDLIB */

