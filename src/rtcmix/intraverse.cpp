/* RTcmix  - Copyright (C) 2000  The RTcmix Development Team
   See ``AUTHORS'' for a list of contributors. See ``LICENSE'' for
   the license to this software and for a DISCLAIMER OF ALL WARRANTIES.
*/
#include <globals.h>
#include <prototypes.h>
#include <pthread.h>
#include <iostream.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>           // DT: 3/97 needed for time function
#include "../rtstuff/heap/heap.h"
#include "../rtstuff/rtdefs.h"
#include "../H/dbug.h"

// #define TBUG
// #define ALLBUG

double baseTime;
long elapsed;

IBusClass checkClass(BusSlot *slot) {
  if (slot == NULL)
	return UNKNOWN;
  if ((slot->auxin_count > 0) && (slot->auxout_count > 0))
	return AUX_TO_AUX;
  if ((slot->auxout_count > 0) && (slot->out_count > 0))
	return TO_AUX_AND_OUT;
  if (slot->auxout_count > 0)
	return TO_AUX;
  if (slot->out_count > 0)
	return TO_OUT;
  return UNKNOWN;
}

extern "C" {
  void *inTraverse(void *arg)
  {
    short rtInst;
    short playEm;
    int i,j,chunksamps;
    int heapSize,rtQSize,allQSize;
    int offset;
    int keepGoing;
    int dummy;
	short bus_q_offset;

    Instrument *Iptr;

    unsigned long bufEndSamp,endsamp;
    unsigned long chunkStart;
    unsigned long heapChunkStart;

    struct timeval tv;
    struct timezone tz;
    double sec,usec;

	Bool aux_pb_done,frame_done;
	Bool audio_configured = NO;
	short bus,bus_count,play_bus,busq,endbus,t_bus,t_count;
	IBusClass bus_class,qStatus,t_class;
	BusType bus_type;
	
#ifdef ALLBUG	
    cout << "ENTERING inTraverse() FUNCTION *****\n";
#endif

    // Wait for the ok to go ahead
	pthread_mutex_lock(&audio_config_lock);
    if (!audio_config) {
      cout << "inTraverse():  waiting for audio_config . . . ";
    }
	pthread_mutex_unlock(&audio_config_lock);

    while (!audio_configured) {
	  pthread_mutex_lock(&audio_config_lock);
	  if (audio_config) {
		audio_configured = YES;
	  }
	  pthread_mutex_unlock(&audio_config_lock);
    }

    if (audio_configured && rtInteractive) {
      cout << "audio set.\n\n";
    }

    // Initialize everything
    bufStartSamp = 0;  // current end sample for buffer
    bufEndSamp = RTBUFSAMPS;
    chunkStart = 0;
    heapSize = 0;
    chunkStart = 0;
    elapsed = 0;
    rtInst = 0;
    playEm = 0;

    // NOTE: audioin, aux and output buffers are zero'd during allocation

    // read in an input buffer (if audio input is active)
    if (audio_on) {
      rtgetsamps();
      rtsendzeros(0);  // send a buffer of zeros to audio device
    }
  
    if (rtsetparams_called)         // otherwise, disk-based only
      playEm = 1;

	// Try and be tight with time
	gettimeofday(&tv, &tz);
	sec = (double)tv.tv_sec;
	usec = (double)tv.tv_usec;
	baseTime = (sec * 1e6) + usec;

    while(playEm) { // the big loop ++++++++++++++++++++++++++++++++++++++++++

#ifdef TBUG
	  printf("Entering big loop .....................\n");
#endif

      pthread_mutex_lock(&heapLock);
      heapSize = rtHeap.getSize();
      if (heapSize > 0) {
		heapChunkStart = rtHeap.getTop();
      }
      pthread_mutex_unlock(&heapLock);

#ifdef TBUG
	  cout << "heapSize = " << heapSize << endl;
	  cout << "heapChunkStart = " << heapChunkStart << endl;
	  cout << "Bus_Configed = " << Bus_Configed << endl;
#endif

      // Pop elements off rtHeap and insert into rtQueue +++++++++++++++++++++
      while ((heapChunkStart < bufEndSamp) && (heapSize > 0)) {
        rtInst = 1;
        pthread_mutex_lock(&heapLock);
		Iptr = rtHeap.deleteMin();  // get next instrument off heap
		pthread_mutex_unlock(&heapLock);
		if (!Iptr)
		  break;

		// DJT Now we push things onto different queues
		bus_class = checkClass(Iptr->bus_config);
		switch (bus_class) {
		case TO_AUX:
		  bus_count = Iptr->bus_config->auxout_count;
		  bus_q_offset = 0;
		  for(i=0;i<bus_count;i++) {
			bus = Iptr->bus_config->auxout[i];
			busq = bus+bus_q_offset;
#ifdef ALLBUG
			cout << "Pushing on TO_AUX[" << busq << "] rtQueue\n";
#endif
			rtQueue[busq].push(Iptr,heapChunkStart);
		  }
		  break;
		case AUX_TO_AUX:
		  bus_count = Iptr->bus_config->auxout_count;
		  bus_q_offset = MAXBUS;
		  for(i=0;i<bus_count;i++) {
			bus = Iptr->bus_config->auxout[i];
			busq = bus+bus_q_offset;
#ifdef ALLBUG
			cout << "Pushing on AUX_TO_AUX[" << busq << "] rtQueue\n";
#endif
			rtQueue[busq].push(Iptr,heapChunkStart);
		  }
		  break;
		case TO_OUT:
		  bus_count = Iptr->bus_config->out_count;
		  bus_q_offset = MAXBUS*2;
		  for(i=0;i<bus_count;i++) {
			bus = Iptr->bus_config->out[i];
			busq = bus+bus_q_offset;
#ifdef ALLBUG
			cout << "Pushing on TO_OUT[" << busq << "] rtQueue\n";
#endif
			rtQueue[busq].push(Iptr,heapChunkStart);
		  }
		  break;
		case TO_AUX_AND_OUT:
		  bus_count = Iptr->bus_config->out_count;
		  bus_q_offset = MAXBUS;
		  for(i=0;i<bus_count;i++) {
			bus = Iptr->bus_config->out[i];
			busq = bus+bus_q_offset;
#ifdef ALLBUG
			cout << "Pushing on TO_OUT2[" << busq << "] rtQueue\n";
#endif
			rtQueue[busq].push(Iptr,heapChunkStart);
		  }
		  bus_count = Iptr->bus_config->auxout_count;
		  bus_q_offset = 2*MAXBUS;
		  for(i=0;i<bus_count;i++) {
			bus = Iptr->bus_config->auxout[i];
			busq = bus+bus_q_offset;
#ifdef ALLBUG
			cout << "Pushing on TO_AUX2[" << busq << "] rtQueue\n";
#endif
			rtQueue[busq].push(Iptr,heapChunkStart);
		  }
		  break;
		default:
		  cout << "ERROR (intraverse): unknown bus_class\n";
		  break;
		}

		pthread_mutex_lock(&heapLock);
		heapSize = rtHeap.getSize();
		if (heapSize > 0)
		  heapChunkStart = rtHeap.getTop();
        pthread_mutex_unlock(&heapLock);
	  }
	  // End rtHeap popping and rtQueue insertion ----------------------------

	  qStatus = TO_AUX;
	  play_bus = 0;
	  aux_pb_done = NO;
	  allQSize = 0;
	  
	  // rtQueue[] playback shuffling ++++++++++++++++++++++++++++++++++++++++
	  while (!aux_pb_done) {

		switch (qStatus) {
		case TO_AUX:
		  bus_q_offset = 0;
		  bus_type = BUS_AUX_OUT;
		  bus = ToAuxPlayList[play_bus++];
		  break;
		case AUX_TO_AUX:
		  bus_q_offset = MAXBUS;
		  bus = AuxToAuxPlayList[play_bus++];
		  bus_type = BUS_AUX_OUT;
		  break;
		case TO_OUT:
		  bus_q_offset = MAXBUS*2;
		  bus = ToOutPlayList[play_bus++];
		  bus_type = BUS_OUT;
		  break;
		default:
		  cout << "ERROR (intraverse): unknown bus_class\n";
		  break;
		}

		if (bus != -1) {
		  busq = bus+bus_q_offset;
		  rtQSize = rtQueue[busq].getSize();
		  if (rtQSize > 0) {
			chunkStart = rtQueue[busq].nextChunk();
		  }
		}
		else {
		  switch (qStatus) {
		  case TO_AUX:
			qStatus = AUX_TO_AUX;
			play_bus = 0;
			break;
		  case AUX_TO_AUX:
			qStatus = TO_OUT;
			play_bus = 0;
			break;
		  case TO_OUT:
#ifdef ALLBUG
			cout << "aux_pb_done\n";
#endif
			aux_pb_done = YES;
			break;
		  default:
			cout << "ERROR (intraverse): unknown bus_class\n";
			break;
		  }
		}
#ifdef ALLBUG
		cout << "bus: " << bus << endl;
		cout << "busq:  " << busq << endl;
#endif

		// Play elements on queue (insert back in if needed) ++++++++++++++++++
		while ((rtQSize > 0) && (chunkStart < bufEndSamp) && (bus != -1)) {
		  
		  Iptr = rtQueue[busq].pop();  // get next instrument off queue
		  
		  endsamp = Iptr->getendsamp();
		  
		  // difference in sample start (countdown)
		  offset = chunkStart - bufStartSamp;  
		  
		  // DJT:  may have to expand here.  IE., conditional above
		  // (chunkStart >= bufStartSamp)
		  // unlcear what that will do just now
		  if (offset < 0) { // BGG: added this trap for robustness
			cout << "WARNING: the scheduler is behind the queue!" << endl;
			offset = 0;
		  }
		  
		  Iptr->set_output_offset(offset);
		  
		  if (endsamp < bufEndSamp) {  // compute # of samples to write
			chunksamps = endsamp-chunkStart;
		  }
		  else {
			chunksamps = bufEndSamp-chunkStart;
		  }

#ifdef TBUG
		  cout << "Begin playback iteration==========\n";
		  cout << "Q-chunkStart:  " << chunkStart << endl;
		  cout << "bufEndSamp:  " << bufEndSamp << endl;
		  cout << "RTBUFSAMPS:  " << RTBUFSAMPS << endl;
		  cout << "endsamp:  " << endsamp << endl;
		  cout << "offset:  " << offset << endl;
		  cout << "chunksamps:  " << chunksamps << endl;
#endif      

		  Iptr->setchunk(chunksamps);  // set "chunksamps"		 
#ifdef TBUG
		  cout << "Iptr->exec(" << bus_type << "," << bus << ")\n";
#endif		  
		  Iptr->exec(bus_type, bus);    // write the samples * * * * * * * * * 
		  
		  // ReQueue or delete ++++++++++++++++++++++++++++++++++++++++++++++
		  if (endsamp > bufEndSamp) {
#ifdef TBUG
			cout << "re queueing\n";
#endif
			rtQueue[busq].push(Iptr,chunkStart+chunksamps);   // put back onto queue
		  }
		  else {
			t_class = checkClass(Iptr->bus_config);
			switch (t_class) {
			case TO_AUX:
			  t_count = Iptr->bus_config->auxout_count;
			  endbus = Iptr->bus_config->auxout[t_count-1];
			  break;
			case AUX_TO_AUX:
			  t_count = Iptr->bus_config->auxout_count;
			  endbus = Iptr->bus_config->auxout[t_count-1];
			  break;
			case TO_AUX_AND_OUT:
			  if (qStatus == TO_OUT) {
				t_count = Iptr->bus_config->out_count;
				endbus = Iptr->bus_config->out[t_count-1];			
			  }
			  else
				endbus = 1000;  /* can never equal this */
			  break;
			case TO_OUT:
			  t_count = Iptr->bus_config->out_count;
			  endbus = Iptr->bus_config->out[t_count-1];
			  break;
			default:
			  cout << "ERROR (intraverse): unknown bus_class\n";
			  break;
			}
			if ((qStatus == t_class) && (bus == endbus)) {
			  delete Iptr;
			}
 		  }  // end rtQueue or delete ----------------------------------------
		  
		  // DJT:  not sure this check before new chunkStart is necessary
		  rtQSize = rtQueue[busq].getSize();
		  if (rtQSize) {
			chunkStart = rtQueue[busq].nextChunk(); /* FIXME:  crapping out */
			allQSize += tQSize;                /* in RT situation sometimes */
		  }
#ifdef TBUG
		  cout << "rtQSize: " << rtQSize << endl;
		  cout << "chunkStart:  " << chunkStart << endl;
		  cout << "chunksamps:  " << chunksamps << endl;
		  cout << "Iteration done==========\n";
#endif
		} // end Play elements on queue (insert back in if needed) -----------
	  }  // end aux_pb_done --------------------------------------------------
	  
	  // Write buf to audio device - - - - - - - - - - - - - - - - - - - - -
#ifdef TBUG
	  cout << "Writing samples----------\n";
	  cout << "Q-chunkStart:  " << chunkStart << endl;
	  cout << "bufEndSamp:  " << bufEndSamp << endl;
#endif
	  
	  rtsendsamps();
	  
	  gettimeofday(&tv, &tz);
	  sec = (double)tv.tv_sec;
	  usec = (double)tv.tv_usec;
	  baseTime = (sec * 1e6) + usec;
	  elapsed += RTBUFSAMPS;	
	  bufStartSamp += RTBUFSAMPS;
	  bufEndSamp += RTBUFSAMPS;

	  // zero the buffers
	  clear_aux_buffers();
	  clear_output_buffers();
	  
	  // read in an input buffer (if audio input is active)
	  if (audio_on) { 
		// cout << "Reading data from audio port\n";
		rtgetsamps();
	  }
      
      if (!rtInteractive) {  // Ending condition
		if ((heapSize == 0) && (allQSize == 0)) {
#ifdef ALLBUG
		  cout << "heapSize:  " << heapSize << endl;
		  cout << "rtQSize:  " << rtQSize << endl;
		  cout << "PLAYEM = 0\n";
		  cout << "The end\n\n";
#endif
		  playEm = 0;
		}
      }
    } // end playEm ----------------------------------------------------------
  
	if (rtsetparams_called) {
	  if (play_audio) {             // Play zero'd buffers to avoid clicks
		int count = NCHANS * 4;  // DJT:  clicks on my box with 2
		for (j = 0; j < count; j++) 
		  rtsendzeros(0);
      }  
	  close_audio_ports();
	  rtreportstats();
	  rtcloseout();
	}

	cout << "\n";
#ifdef ALLBUG
	cout << "EXITING inTraverse() FUNCTION *****\n";
	exit(1);
#endif
  }

} /* extern "C" */
