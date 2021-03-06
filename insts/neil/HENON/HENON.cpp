/* HENON - Henon map noise generator

   p0 = output start time
   p1 = duration
   p2 = amplitude multiplier
   p3 = a [optional, default is 1.4]
   p4 = b [optional, default is 0.3]
   p5 = x [optional, default is 1]
   p6 = y [optional, default is 1]
   p7 = pan (in percent-to-left format) [optional, default is .5]

   p2 (amp), p3-p6 (function parameters), and p7 (pan) can
   receive updates from a table or real-time control source.

   p3-p6: Try values within a few tenths of the defaults given
   here.

   Neil Thornock <neilthornock at gmail>, 11/2016

   Function from the Henon map by Michel Henon.
*/
#include <stdio.h>
#include <stdlib.h>
#include <ugens.h>
#include <math.h>
#include "HENON.h"
#include <rt.h>
#include <rtdefs.h>

#include <iostream>

HENON::HENON()
	: branch(0)
{
}

HENON::~HENON()
{
}

int HENON::init(double p[], int n_args)
{
	nargs = n_args;

	const float outskip = p[0];
	const float dur = p[1];

	if (rtsetoutput(outskip, dur, this) == -1)
		return DONT_SCHEDULE;

	if (outputChannels() > 2)
		return die("HENON", "Use mono or stereo output only.");

	return nSamps();
}

int HENON::configure()
{
	return 0;
}

void HENON::doupdate()
{
	double p[nargs];
	update(p, nargs);

	amp = p[2];
	a = (nargs > 3) ? p[3] : 1.4;
	b = (nargs > 4) ? p[4] : 0.3;
	x = (nargs > 5) ? p[5] : 1;
	y = (nargs > 6) ? p[6] : 1;

	pan = (nargs > 7) ? p[7] : 0.5;
}

int HENON::run()
{
	for (int i = 0; i < framesToRun(); i++) {
		if (--branch <= 0) {
			doupdate();
			branch = getSkip();
		}

		x = y + 1 - (a * x * x);
		y = b * z;
		z = x;

		float samp;
		if (x > 1 || x < -1)
			samp = sin(x);
		else
			samp = x;

		float out[2];
		out[0] = samp * amp;

		if (outputChannels() == 2) {
			out[1] = out[0] * (1.0 - pan);
			out[0] *= pan;
		}
		rtaddout(out);
		increment();
	}

	return framesToRun();
}

Instrument *makeHENON()
{
	HENON *inst = new HENON();
	inst->set_bus_config("HENON");

	return inst;
}

#ifndef EMBEDDED
void rtprofile()
{
	RT_INTRO("HENON", makeHENON);
}
#endif
