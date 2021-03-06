rtsetparams(44100, 2);
rtinput("../../../snd/huhh.wav");

load("TRANSBEND");

dur = DUR(0) * 0.8;
amp = 0.8;
pan = 0.5;

/* amplitude curve */
setline(0,0, 1, 1, 9, 1, 10, 0);

/* transpose from 4 semitones up to 8 down - stored in gen slot 2 */
makegen(-2, 18, 512, 1,.4, 512,-.8);

TRANSBEND(0, 0, dur, amp, 2, 0, pan);

