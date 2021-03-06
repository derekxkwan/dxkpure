/* Copyright 2016 - Derek Kwan
 *  * Distributed under GPL v3 */

#include "dkwtab.h"
#include <stdlib.h>

#ifndef STUTLEN
#define STUTLEN 240000 //max length of delaybuf
#endif

#ifndef STUTXF
#define STUTXF 1 //xfade ramp in ms
#endif

static t_class *dkstut_tilde_class;

typedef struct _dkstut_tilde {
	t_object x_obj;
	int x_sr; //samplerate
	float x_stutms; //stuttering size in ms
	int x_stutsamp; //stuttering in samples
	t_float x_stut; //stuttering on/off  if on, no recording, if off, recording
	double delaybuf[STUTLEN]; //delay table
	int x_writehead; //write head for stutter buf
	int x_readhead; //reading head for stuttering relative to beginning of stutter segment
	int x_startpos; //starting position of readhead
	double window[WINLEN]; //stutter window
	double x_winstep; //steps in the window relative to position in stutter
	double x_xfade; //xfade between dry and wet (0 is dry, 1 is wet)
	double x_xfstep; //xfade step
	float x_xfms; //xfade time
	int x_xfading; //state of xfading  0=not, -1=to dry, 1 = to wet
	int x_prevstate; //store previous state
	t_float x_input; //dummy
	t_inlet *x_mslet;
	t_outlet *x_outlet;
} t_dkstut_tilde;


static void dkstut_tilde_dur(t_dkstut_tilde *x, t_float dur){
	int sr = x->x_sr;
	float xfms = x->x_xfms; 
	float srms = (float)sr * 0.001f;
	int dursamp = (int)(srms* dur);
	x->x_stutms = dur;
	x->x_stutsamp = dursamp;
	x->x_readhead = 0;
	x->x_startpos= (x->x_writehead + (STUTLEN-dursamp)) % STUTLEN;
	double winst = (double)WINLEN/dursamp;
	x->x_winstep = winst;
	x->x_xfstep = 1.f/((double)xfms*srms);
}

static void *dkstut_tilde_new(t_floatarg dur){
	int i;
	t_dkstut_tilde *x = (t_dkstut_tilde *)pd_new(dkstut_tilde_class);
	x->x_sr = sys_getsr();
	x->x_writehead = 0.f;
	x->x_readhead = 0.f;
	x->x_stut =0.f;
	x->x_prevstate = 0;
	x->x_xfade = 0.f;
	x->x_xfading = 0;
	x->x_xfms = STUTXF;
	dkstut_tilde_dur(x, dur);
	for(i=0;i<STUTLEN; i++){//zeroing out delay table
		x->delaybuf[i] = 0.f;
	};

	for(i=0;i<WINLEN; i++){//creating stutter window
		float xpos = OPI*(float)i/(float)WINLEN;
		x->window[i] = sin(xpos);
	};
	x->x_mslet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("dur"));
	floatinlet_new(&x->x_obj, &x->x_stut);
	x->x_outlet = outlet_new(&x->x_obj, gensym("signal"));
	return (x);
}


static double dkstut_tilde_readbuf(t_dkstut_tilde *x, int phs){
	//reading the buf with linear interpolation
	double output, winval;
	int x1, wx1, wx2;
	double wfrac, windx;
	double  wy1, wy2;
	int start = x->x_startpos;
	double winst = x->x_winstep;
	
	//reading from the delaybuf
	x1 = (phs + start) % STUTLEN;
	output = x->delaybuf[x1];
	//reading from the window
	windx = (double)phs*winst;
	wx1 = (int)(windx) % WINLEN;
	wy1 = x->window[wx1];
	wx2 = (wx1 + 1) % WINLEN;
	wfrac = windx - (double)wx1;
	wy2 = x->window[wx2];
	winval = wy1 + wfrac*(wy2-wy1);

	//windowing output
	output *=winval;
	return output;

};


static t_int *dkstut_tilde_perform(t_int *w){
	 t_dkstut_tilde *x = (t_dkstut_tilde *)(w[1]);
	 t_float *in = (t_float *)(w[2]);
	t_float *out = (t_float *)(w[3]);
	int n = (int)(w[4]);
	int stut;
	int dur = x->x_stutsamp;
	float stutstat = x->x_stut;
	int prev = x->x_prevstate;
	if(stutstat <= 0.f){
		stut = 0;
	}
	else{
		stut = 1;
	};
	while(n--){
		double input = *in++;
		double outputwet = 0.f, outputdry = 0.f;
		int readh = x->x_readhead;
		int writeh = x->x_writehead;
		double xfade = x->x_xfade;
		int xfading = x->x_xfading;

		if(abs(xfading)){
			double xfstep = x->x_xfstep;
			xfade += (double)xfading*xfstep;
			x->x_xfade = xfade;
			if(xfade < 0.f){
				xfade = 0.f;
				x->x_xfading = 0;//stop xfading
			} else if(xfade > 1.f){
				xfade = 1.f;
				x->x_xfading = 0;//stop xfading
			};
		};
			if(xfade < 1.f){//if xfading or not stuttering, get clean output 
				outputdry = input;
				outputdry *= (1.f-xfade);
			};
		if( xfade > 0.f){// if xfading or stuttering, get wet output and increment read header
			//read from delaybuf
			outputwet = dkstut_tilde_readbuf(x, readh);
			outputwet *= xfade;
		
			//incrementing read head
			readh = (readh + 1)% dur;
			x->x_readhead = readh;};
		if(stut){// if stuttering, keep track of changing state (since reading the buf is done above)
			if(!prev){
				x->x_prevstate = 1;
				x->x_xfading = 1; //set xfading towards the wet
			};
		}
		else {//not stuttering, recording
			x->delaybuf[writeh] = input;
			writeh = (writeh + 1) % STUTLEN;
			x->x_writehead = writeh;
			x->x_startpos= (writeh + (STUTLEN-dur)) % STUTLEN;
			if(prev){//if clean not previous statem set xfading towards the dry
				x->x_prevstate = 0;
				x->x_xfading = -1;
			};
		};
		*out++ = outputwet + outputdry;
	};
	return(w+5);
}

static void dkstut_tilde_dsp(t_dkstut_tilde *x, t_signal **sp){

	//(freq*tablelen)/(samplerate) = array values per sample to advance
	// divide by tablelen to map to 0 to 1 range,..freq/samplerate
   int sr = sp[0]->s_sr; //amount to change phase for freq 1
   if(sr != x->x_sr){
		float dur = x->x_stutms;
		dkstut_tilde_dur(x, dur);
   };
	    dsp_add(dkstut_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);

}


static void *dkstut_tilde_free(t_dkstut_tilde *x){
	inlet_free(x->x_mslet);
	outlet_free(x->x_outlet);
	
	return (void *)x;
}

void dkstut_tilde_setup(void){
	dkstut_tilde_class = class_new(gensym("dkstut~"), (t_newmethod)dkstut_tilde_new, 0,
			sizeof(t_dkstut_tilde), CLASS_DEFAULT, A_DEFFLOAT, 0);
	class_addmethod(dkstut_tilde_class, (t_method)dkstut_tilde_dsp, gensym("dsp"), A_CANT, 0);
   CLASS_MAINSIGNALIN(dkstut_tilde_class, t_dkstut_tilde, x_input);
   class_addmethod(dkstut_tilde_class, (t_method)dkstut_tilde_dur, gensym("dur"), A_FLOAT, 0);
}
