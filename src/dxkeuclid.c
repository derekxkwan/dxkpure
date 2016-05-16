/* Copyright 2016 - Derek Kwan
 *  * Distributed under GPL v3 */

#include "m_pd.h"
#include <stdlib.h>

//taken from puredata's src/x_list.c

#ifndef HAVE_ALLOCA     /* can work without alloca() but we never need it */
#define HAVE_ALLOCA 1
#endif
#define TEXT_NGETBYTE 100 /* bigger that this we use alloc, not alloca */
#if HAVE_ALLOCA
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)((n) < TEXT_NGETBYTE ?  \
        alloca((n) * sizeof(t_atom)) : getbytes((n) * sizeof(t_atom))))
#define ATOMS_FREEA(x, n) ( \
    ((n) < TEXT_NGETBYTE || (freebytes((x), (n) * sizeof(t_atom)), 0)))
#else
#define ATOMS_ALLOCA(x, n) ((x) = (t_atom *)getbytes((n) * sizeof(t_atom)))
#define ATOMS_FREEA(x, n) (freebytes((x), (n) * sizeof(t_atom)))
#endif

#define EUCMAX(a,b) \
	({__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	 _a > _b ? _a : _b;})
#define EUCMIN(a,b) \
	({__typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	 _a < _b ? _a : _b;})



static t_class *dxkeuclid_class;

typedef struct _dxkeuclid {
	t_object x_obj;
	t_float n;
	t_float k;
	t_float m;
} t_dxkeuclid;

static void *dxkeuclid_new(t_floatarg f1, t_floatarg f2){
	int n, k, m;
	t_dxkeuclid *x = (t_dxkeuclid *)pd_new(dxkeuclid_class);
	n = f1;
	k = f2;
	if(k > n){n=k;};
	m = n-k;
	x->n = (t_float)n;
	x->k = (t_float)k;
	x->m = (t_float)m;
	floatinlet_new(&x->x_obj, &x->n);
	floatinlet_new(&x->x_obj, &x->k);
	outlet_new(&x->x_obj, &s_list);
	return (x);
}


static void dxkeuclid_bang(t_dxkeuclid *x){
	int i, r, n, k, m, ksize, msize;
	int * eucstr, *temp;
	m = (int)x->m;
	n = (int)x->n;
	k = (int)x->k;
	r = EUCMIN(m,k);
	eucstr = malloc(n*sizeof(int));
	temp = malloc(n*sizeof(int));
	for(i=0; i<n;i++){
		if(i<k){
			eucstr[i] = 1;
			}
		else{
			eucstr[i] = 0;
			};
		};
	ksize = 1;
	msize = 1;
	while(r > 1){
		int kidx = 0;
		int midx = k*ksize;
		int tidx = 0;
		int j=0;
		while(tidx<n){
			if(kidx<(k*ksize)){
			for(j=0;j<ksize;j++){
				temp[tidx] = eucstr[kidx];
				tidx++;
				kidx++;
				};
			};
			if(midx<n){
				for(j=0;j<msize;j++){
					temp[tidx] = eucstr[midx];
					tidx++;
					midx++;
				};
			}
			

		};
		ksize+= msize;
		if(m>=k){
			m = m-k;

			}
		else{
			int tint;
			tint = k;
			k = m;
			m = tint-m;
			msize++;
			};
		r = EUCMIN(m,k);
		for(i=0;i<n; i++){
			eucstr[i] = temp[i];
		};
		};


	t_atom * estr;
	ATOMS_ALLOCA(estr, n);
	for(i=0; i<n; i++){
		SETFLOAT((estr+i), (t_float)eucstr[i]);

	};
	free(eucstr);
	free(temp);
	outlet_list(x->x_obj.ob_outlet, &s_list, n, estr);
	ATOMS_FREEA(estr, n);
}

void dxkeuclid_setup(void){
	dxkeuclid_class = class_new(gensym("dxkeuclid"), (t_newmethod)dxkeuclid_new, 0,
			sizeof(t_dxkeuclid), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
	class_addbang(dxkeuclid_class, dxkeuclid_bang);
}
