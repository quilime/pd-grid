#include "kria.h"

//// network inputs:
static void op_kria_in_clock(op_kria_t* grid);
static void op_kria_in_octave(op_kria_t* grid, float f);
static void op_kria_in_tuning(op_kria_t* grid, float f);

// polled-operator handler
static void op_kria_poll_handler(void* op);

/// monome event handler
static void op_kria_handler(t_monome* op_monome, u8 x, u8 y, u8 z);

// pickles
static void op_kria_pickle(op_kria_t* kria, FILE* f);
static void op_kria_unpickle(op_kria_t* kria, FILE* f);

#define L2 12
#define L1 8
#define L0 4

//////////////////////////////////////////////////
static const u8 SCALE[49] = {
  2, 2, 1, 2, 2, 2, 1,	// ionian
  2, 1, 2, 2, 2, 1, 2,	// dorian
  1, 2, 2, 2, 1, 2, 2,	// phyrgian
  2, 2, 2, 1, 2, 2, 1,	// lydian
  2, 2, 1, 2, 2, 1, 2,	// mixolydian
  2, 1, 2, 2, 1, 2, 2,	// aeolian
  1, 2, 2, 1, 2, 2, 2	// locrian
};


u8 preset_mode, preset_select, front_timer;

modes mode = mTr;
mod_modes mod_mode = modNone;
u8 p, p_next;
u8 ch;
u8 pos[2][NUM_PARAMS];
u8 trans_edit;
u8 pscale_edit;

u8 key_alt, mod1, mod2;
s8 loop_count;
u8 loop_edit;

u8 cur_scale[2][7];

u8 tr[2];

u8 ac[2];
u8 oct[2];
u16 dur[2];
u8 note[2];
u8 sc[2];
u16 trans[2];
u16 cv[2];

u8 pos_mul[2][NUM_PARAMS];

static void adjust_loop_start(op_kria_t *kria, u8 x, u8 m);
static void adjust_loop_end(op_kria_t *kria, u8 x, u8 m);

static void calc_scale(op_kria_t *kria, u8 c);

static void phase_reset0(op_kria_t *kria);
static void phase_reset1(op_kria_t *kria);



bool kria_next_step(op_kria_t *kria, uint8_t t, uint8_t p);

static void kria_refresh(t_monome *op_monome);
/* static void kria_refresh_mono(t_monome *op_monome); */

// static void handler_KeyTimer(s32 data);



static void phase_reset0(op_kria_t *kria) {
  for(u8 i1=0;i1<NUM_PARAMS;i1++)
    pos[0][i1] = kria->k.kp[0][p].lend[i1];
}

static void phase_reset1(op_kria_t *kria) {
  for(u8 i1=0;i1<NUM_PARAMS;i1++)
    pos[1][i1] = kria->k.kp[1][p].lend[1];
}


static t_class *op_kria_class;
void *kria_new(t_symbol *s, int argc, t_atom *argv);
void kria_free(op_kria_t* op);
void kria_setup (void) {
  net_monome_setup();
  op_kria_class = class_new(gensym("kria"),
			    (t_newmethod)kria_new,
			    (t_method)kria_free,
			    sizeof(op_kria_t),
			    CLASS_DEFAULT,
			    A_GIMME, 0);
  net_monome_add_focus_methods(op_kria_class);
  class_addbang(op_kria_class, (t_method)op_kria_in_clock);
  class_addmethod(op_kria_class,
		  (t_method)op_kria_in_octave, gensym("octave"), A_DEFFLOAT, 0);
  class_addmethod(op_kria_class,
		  (t_method)op_kria_in_tuning, gensym("tuning"), A_DEFFLOAT, 0);
}

//-------------------------------------------------
//----- extern function definition
void *kria_new(t_symbol *s, int argc, t_atom *argv) {
  (void) s;
  (void) argc;
  (void) argv;
  u8 i1,i2;
  op_kria_t *op = (op_kria_t *)pd_new(op_kria_class);

  op->voice0 = outlet_new((t_object *) op, &s_list);
  op->voice1 = outlet_new((t_object *) op, &s_list);

  net_monome_init(&op->monome, (monome_handler_t)&op_kria_handler,
		  (monome_pickle_t)op_kria_pickle,
		  (monome_pickle_t)op_kria_unpickle);

  op->clk = 0;
  op->octave = 12;
  op->tuning = 5;

  u8 c;
  // clear out some reasonable defaults
  for(c=0;c<2;c++) {
    for(i1=0;i1<16;i1++) {
      for(i2=0;i2<16;i2++) {
	op->k.kp[c][i1].tr[i2] = 0;
	op->k.kp[c][i1].ac[i2] = 0;
	op->k.kp[c][i1].oct[i2] = 0;
	op->k.kp[c][i1].dur[i2] = 0;
	op->k.kp[c][i1].note[i2] = 0;
	op->k.kp[c][i1].sc[i2] = 0;
	op->k.kp[c][i1].trans[i2] = 0;
      }

      op->k.kp[c][i1].dur_mul = 4;

      for(i2=0;i2<NUM_PARAMS;i2++) {
	op->k.kp[c][i1].lstart[i2] = 0;
	op->k.kp[c][i1].lend[i2] = 5;
	op->k.kp[c][i1].llen[i2] = 6;
	op->k.kp[c][i1].lswap[i2] = 0;
	op->k.kp[c][i1].tmul[i2] = 1;
      }
    }
  }

  for(i1=0;i1<7;i1++) {
    op->k.pscale[i1] = i1;
  }

  for(i1=0;i1<42;i1++) {
    op->k.scales[i1][0] = 0;
    for(i2=0;i2<6;i2++)
      op->k.scales[i1][i2+1] = 1;
  }



  // init monome drawing
  kria_refresh(&op->monome);

  op->clock = clock_new(op, (t_method) op_kria_poll_handler);
  clock_delay(op->clock,  OP_KRIA_POLL_TIME);
  return (void *) op;
}

// de-init
void kria_free(op_kria_t* op) {
  net_monome_deinit((t_monome *)op);
  clock_free(op->clock);
}

// t = track
// param = mode_idx (enum modes)
bool kria_next_step(op_kria_t *kria, uint8_t t, uint8_t param) {
#ifndef DISABLE_KRIA
  pos_mul[t][param]++; // pos_mul is current sub-clock position?

  if(pos_mul[t][param] >= kria->k.kp[t][p].tmul[param]) { // tmul is mode's clock divisor
    if(pos[t][param] == kria->k.kp[t][p].lend[param]) // lend means "loop end", pos means current super-clock position
      pos[t][param] = kria->k.kp[t][p].lstart[param]; // lstart means "loop start"
    else {
      pos[t][param]++;
      if(pos[t][param] > 15)
	pos[t][param] = 0;
    }
    pos_mul[t][param] = 0;
    return 1;
  }
  else
#endif
    return 0;
}

// t = track
static void op_kria_track_tick (op_kria_t *kria, u8 t) {
#ifndef DISABLE_KRIA

  if(p_next != p) {
    p = p_next;
    phase_reset0(kria);
    phase_reset1(kria);
    return;
  }

  if(kria_next_step(kria, t,tTrans)) {
    trans[t] = kria->k.kp[t][p].trans[pos[t][tTrans]];
  }

  if(kria_next_step(kria,t, tScale)) {
    if(sc[t] != kria->k.kp[t][p].sc[pos[t][tScale]]) {
      sc[t] = kria->k.kp[t][p].sc[pos[t][tScale]];
      calc_scale(kria, t);
    }
  }

  if(kria_next_step(kria,t, tDur)) {
    dur[t] = (kria->k.kp[t][p].dur[pos[t][tDur]]+1) * (kria->k.kp[t][p].dur_mul<<2);
  }
    
  if(kria_next_step(kria,t, tOct)) {
    oct[t] = kria->k.kp[t][p].oct[pos[t][tOct]];
  }

  if(kria_next_step(kria,t, tNote)) {
    note[t] = kria->k.kp[t][p].note[pos[t][tNote]];
  }

  if(kria_next_step(kria,t, tAc)) {
    ac[t] = kria->k.kp[t][p].ac[pos[t][tAc]];
  }

  if(kria_next_step(kria,t,tTr)) {
    if(kria->k.kp[t][p].tr[pos[t][tTr]]) {
      tr[t] = 1 + ac[t];
      cv[t] = cur_scale[t][note[t]] + (oct[t] * kria->octave) + (trans[t] & 0xf) + ((trans[t] >> 4)*kria->tuning);
    }
    else {
      tr[t] = 0;
    }
  }
#endif
}

static void op_kria_in_octave(op_kria_t* op, float f) {
  op->octave = f;
}

static void op_kria_in_tuning(op_kria_t* op, float f) {
  op->tuning = f;
}

static void op_kria_in_clock(op_kria_t* op) {
  op_kria_track_tick(op, 0);
  op_kria_track_tick(op, 1);
  if(tr[0]) {
    t_atom outNote[3];
    outNote[0].a_type = A_FLOAT;
    outNote[0].a_w.w_float = (float) cv[0];
    outNote[1].a_type = A_FLOAT;
    outNote[1].a_w.w_float = (float) tr[0];
    outNote[2].a_type = A_FLOAT;
    outNote[2].a_w.w_float = (float) dur[0];
    outlet_list(op->voice0, &s_list, 3, outNote);
  }
  if(tr[1]) {
    t_atom outNote[3];
    outNote[0].a_type = A_FLOAT;
    outNote[0].a_w.w_float = (float) cv[1];
    outNote[1].a_type = A_FLOAT;
    outNote[1].a_w.w_float = (float) tr[1];
    outNote[2].a_type = A_FLOAT;
    outNote[2].a_w.w_float = (float) dur[1];
    outlet_list(op->voice1, &s_list, 3, outNote);
  }
  tr[0] = 0;
  tr[1] = 0;

  // FIXME not optimal - but for now total redraw on every clock
}


// poll event handler
static void op_kria_poll_handler(void* op) {
  op_kria_t *kria = (op_kria_t *) op;
  kria_refresh(&kria->monome);
  clock_delay(kria->clock, OP_KRIA_POLL_TIME);
}

static void handle_bottom_row_key(op_kria_t *kria, u8 x, u8 z) {
#ifndef DISABLE_KRIA
  if(z) {
    if(x == 3) {
      mode = mTr;
    }
    else if(x == 4) {
      mode = mDur;
    }
    else if(x == 5) {
      mode = mNote;
    }
    else if(x == 6) {
      mode = mScale;
    }
    else if(x == 7) {
      mode = mTrans;
    }
    else if(x == 9) {
      mod_mode = modLoop;
      loop_count = 0;
    }
    else if (x == 10) {
      mod_mode = modTime;
    }
    else if(x == 12) {
      mode = mScaleEdit;
    }
    else if(x == 13) {
      mode = mPattern;
    }
    if(x == 15) {
      key_alt = 1;
    }
    else if(x == 0) {
      ch = 0;
      if(key_alt)
	phase_reset0(kria);
    }
    else if(x == 1) {
      ch = 1;
      if(key_alt)
	phase_reset1(kria);
    }
  }
  else {
    if(x == 9) {
      mod_mode = modNone;
    }
    else if (x == 10) {
      mod_mode = modNone;
    }
    if(x == 15) {
      key_alt = 0;
    }
  }
#endif
}

static void mode_mTr_handle_key(op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  if(mod_mode == modNone) {
    if(z) {
      if(y==0)
	kria->k.kp[ch][p].tr[x] ^= 1;
      else if(y==1)
	kria->k.kp[ch][p].ac[x] ^= 1;
      else if(y>1)
	kria->k.kp[ch][p].oct[x] = 6-y;
    }
  }
  else if(mod_mode == modLoop) {
    if(z) {
      loop_count++;

      if(key_alt) {
	if(y==0) {
	  adjust_loop_start(kria, x, tTr);
	  adjust_loop_end(kria, x, tTr);
	}
	else if(y==1) {
	  adjust_loop_start(kria, x, tAc);
	  adjust_loop_end(kria, x, tAc);
	}
	else if(y>1) {
	  adjust_loop_start(kria, x, tOct);
	  adjust_loop_end(kria, x, tOct);
	}
      }
      else if(loop_count == 1) {
	loop_edit = y;
	if(y==0)
	  adjust_loop_start(kria, x, tTr);
	else if(y==1)
	  adjust_loop_start(kria, x, tAc);
	else if(y>1)
	  adjust_loop_start(kria, x, tOct);
      }
      else if(y == loop_edit) {
	if(y==0)
	  adjust_loop_end(kria, x, tTr);
	else if(y==1)
	  adjust_loop_end(kria, x, tAc);
	else if(y>1)
	  adjust_loop_end(kria, x, tOct);
      }
    }
    else {
      loop_count--;
    }
  }
  else if(mod_mode == modTime) {
    if(z) {
      if(y == 0) {
	kria->k.kp[ch][p].tmul[tTr] = x+1;
      }
      else if(y == 2) {
	kria->k.kp[ch][p].tmul[tAc] = x+1;
      }
      else if(y == 4) {
	kria->k.kp[ch][p].tmul[tOct] = x+1;
      }
    }
  }
#endif
}

static void mode_mDur_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  if(z) {
    if(mod_mode != modTime) {
      if(y==0)
	kria->k.kp[ch][p].dur_mul = x+1;
      else {
	if(mod_mode == modNone)
	  kria->k.kp[ch][p].dur[x] = y-1;
	else if(mod_mode == modLoop) {
	  loop_count++;
	  if(key_alt) {
	    adjust_loop_start(kria, x, tDur);
	    adjust_loop_end(kria, x, tDur);
	  }
	  else if(loop_count == 1)
	    adjust_loop_start(kria, x, tDur);
	  else adjust_loop_end(kria, x, tDur);
	}
      }
    }
    else {
      if(y == 0) {
	kria->k.kp[ch][p].tmul[tDur] = x+1;
      }
    }
  }
  else if(mod_mode == modLoop) loop_count--;
#endif
}

static void mode_mNote_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  if(z) {
    if(mod_mode != modTime) {
      if(mod_mode == modNone)
	kria->k.kp[ch][p].note[x] = 6-y;
      else if(mod_mode == modLoop) {
	loop_count++;
	if(key_alt) {
	  adjust_loop_start(kria, x, tNote);
	  adjust_loop_end(kria, x, tNote);
	}
	else if(loop_count == 1)
	  adjust_loop_start(kria, x, tNote);
	else adjust_loop_end(kria, x, tNote);
      }
    }
    else {
      if(y == 0) {
	kria->k.kp[ch][p].tmul[tNote] = x+1;
      }
    }
  }
  else if(mod_mode == modLoop) loop_count--;
#endif
}

static void mode_mScale_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  if(z) {
    if(mod_mode != modTime) {
      if(mod_mode == modNone)
	kria->k.kp[ch][p].sc[x] = y;
      else if(mod_mode == modLoop) {
	loop_count++;
	if(key_alt) {
	  adjust_loop_start(kria, x, tScale);
	  adjust_loop_end(kria, x, tScale);
	}
	else if(loop_count == 1)
	  adjust_loop_start(kria, x, tScale);
	else adjust_loop_end(kria, x, tScale);
      }
    }
    else {
      if(y == 0) {
	kria->k.kp[ch][p].tmul[tScale] = x+1;
	// calctimes[ch][tScale] = (basetime * kria->k.kp[ch][p].tmul[tScale]) / kria->k.kp[ch][p].tdiv[tScale];
      }
      // else if(y == 1) {
      //	kria->k.kp[ch][p].tdiv[tScale] = x+1;
      //	calctimes[ch][tScale] = (basetime * kria->k.kp[ch][p].tmul[tScale]) / kria->k.kp[ch][p].tdiv[tScale];
      // }
    }

  }
  else if(mod_mode == modLoop) loop_count--;
#endif
}
static void mode_mTrans_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  if(z) {
    if(mod_mode != modTime) {
      if(y == 0) {
	if(mod_mode == modNone)
	  trans_edit = x;
	else if(mod_mode == modLoop) {
	  loop_count++;
	  if(key_alt) {
	    adjust_loop_start(kria, x, tTrans);
	    adjust_loop_end(kria, x, tTrans);
	  }
	  else if(loop_count == 1)
	    adjust_loop_start(kria, x, tTrans);
	  else adjust_loop_end(kria, x, tTrans);
	}
      }
      else
	kria->k.kp[ch][p].trans[trans_edit] = x + ((6-y)<<4);
    }
    else {
      if(y == 0) {
	kria->k.kp[ch][p].tmul[tTrans] = x+1;
      }
    }
  }
  else if(mod_mode == modLoop) loop_count--;
#endif
}

static void mode_mScaleEdit_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  u8 i1;
  if(z) {
    if(x==0) {
      pscale_edit = y;
    }
    else if(y == 6 && x < 8) {
      for(i1=0;i1<6;i1++)
	kria->k.scales[kria->k.pscale[pscale_edit]][i1+1] = SCALE[(x-1)*7+i1];
      kria->k.scales[kria->k.pscale[pscale_edit]][0] = 0;

      if(sc[0] == pscale_edit)
	calc_scale(kria, 0);
      if(sc[1] == pscale_edit)
	calc_scale(kria, 1);
    }
    else if(x > 0 && x < 8) {
      if(key_alt) {
	for(i1=0;i1<7;i1++)
	  kria->k.scales[kria->k.pscale[x-1 + y*7]][i1] = kria->k.scales[kria->k.pscale[pscale_edit]][i1];
      }

      kria->k.pscale[pscale_edit] = x-1 + y*7;

      if(sc[0] == pscale_edit)
	calc_scale(kria, 0);
      if(sc[1] == pscale_edit)
	calc_scale(kria, 1);

    }
    else if(x>7) {
      if(key_alt) {
	if(y!=0) {
	  s8 diff, change;
	  diff = (x-8) - kria->k.scales[kria->k.pscale[pscale_edit]][6-y];
	  change = kria->k.scales[kria->k.pscale[pscale_edit]][6-y+1] - diff;
	  if(change<0) change = 0;
	  if(change>7) change = 7;
	  kria->k.scales[kria->k.pscale[pscale_edit]][6-y+1] = change;
	}

	kria->k.scales[kria->k.pscale[pscale_edit]][6-y] = x-8;
      }
      else kria->k.scales[kria->k.pscale[pscale_edit]][6-y] = x-8;

      if(sc[0] == pscale_edit)
	calc_scale(kria, 0);
      if(sc[1] == pscale_edit)
	calc_scale(kria, 1);
    }
  }
  else if(mod_mode == modLoop) loop_count--;
#endif
}

static void mode_mPattern_handle_key (op_kria_t *kria, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  u8 i1;
  if(z && y==0) {
    if(key_alt) {
      p_next = x;

      for(i1=0;i1<2;i1++) {
	for(u8 i2=0;i2<16;i2++) {
	  kria->k.kp[i1][p_next].tr[i2] = kria->k.kp[i1][p].tr[i2];
	  kria->k.kp[i1][p_next].ac[i2] = kria->k.kp[i1][p].ac[i2];
	  kria->k.kp[i1][p_next].oct[i2] = kria->k.kp[i1][p].oct[i2];
	  kria->k.kp[i1][p_next].dur[i2] = kria->k.kp[i1][p].dur[i2];
	  kria->k.kp[i1][p_next].note[i2] = kria->k.kp[i1][p].note[i2];
	  kria->k.kp[i1][p_next].trans[i2] = kria->k.kp[i1][p].trans[i2];
	  kria->k.kp[i1][p_next].sc[i2] = kria->k.kp[i1][p].sc[i2];
	}

	kria->k.kp[i1][p_next].dur_mul = kria->k.kp[i1][p].dur_mul;

	for(u8 i2=0;i2<NUM_PARAMS;i2++) {
	  kria->k.kp[i1][p_next].lstart[i2] = kria->k.kp[i1][p].lstart[i2];
	  kria->k.kp[i1][p_next].lend[i2] = kria->k.kp[i1][p].lend[i2];
	  kria->k.kp[i1][p_next].llen[i2] = kria->k.kp[i1][p].llen[i2];
	  kria->k.kp[i1][p_next].lswap[i2] = kria->k.kp[i1][p].lswap[i2];
	  kria->k.kp[i1][p_next].tmul[i2] = kria->k.kp[i1][p].tmul[i2];
	}
      }
      p = p_next;
    }
    else {
      p_next = x;
    }
  }
#endif
}

// process monome key input
static void op_kria_handler(t_monome* op_monome, u8 x, u8 y, u8 z) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;

  // bottom row
  if(y == 7) {
    handle_bottom_row_key(kria, x, z);
  }
  // toggle steps
  else if(mode == mTr) {
    mode_mTr_handle_key(kria, x, y, z);
  }
  else if(mode == mDur) {
    mode_mDur_handle_key(kria, x, y, z);
  }
  else if(mode == mNote) {
    mode_mNote_handle_key(kria, x, y, z);
  }
  else if(mode == mScale) {
    mode_mScale_handle_key (kria, x, y, z);
  }
  else if(mode == mTrans) {
    mode_mTrans_handle_key(kria, x, y, z);
  }
  else if(mode == mScaleEdit) {
    mode_mScaleEdit_handle_key(kria, x, y, z);
  }
  else if(mode==mPattern) {
    mode_mPattern_handle_key(kria, x, y, z);
  }
  // FIXME not optimal - but for now simply total redraw on every press
#endif
}

static void adjust_loop_start(op_kria_t *kria, u8 x, u8 m) {
#ifndef DISABLE_KRIA
  s8 temp;

  temp = pos[ch][m] - kria->k.kp[ch][p].lstart[m] + x;
  if(temp < 0) temp += 16;
  else if(temp > 15) temp -= 16;
  pos[ch][m] = temp;

  kria->k.kp[ch][p].lstart[m] = x;
  temp = x + kria->k.kp[ch][p].llen[m]-1;
  if(temp > 15) {
    kria->k.kp[ch][p].lend[m] = temp - 16;
    kria->k.kp[ch][p].lswap[m] = 1;
  }
  else {
    kria->k.kp[ch][p].lend[m] = temp;
    kria->k.kp[ch][p].lswap[m] = 0;
  }
#endif
}

static void adjust_loop_end(op_kria_t *kria, u8 x, u8 m) {
#ifndef DISABLE_KRIA
  s8 temp;

  kria->k.kp[ch][p].lend[m] = x;
  temp = kria->k.kp[ch][p].lend[m] - kria->k.kp[ch][p].lstart[m];
  if(temp < 0) {
    kria->k.kp[ch][p].llen[m] = temp + 17;
    kria->k.kp[ch][p].lswap[m] = 1;
  }
  else {
    kria->k.kp[ch][p].llen[m] = temp+1;
    kria->k.kp[ch][p].lswap[m] = 0;
  }

  temp = pos[ch][m];
  if(kria->k.kp[ch][p].lswap[m]) {
    if(temp < kria->k.kp[ch][p].lstart[m] && temp > kria->k.kp[ch][p].lend[m])
      pos[ch][m] = kria->k.kp[ch][p].lstart[m];
  }
  else {
    if(temp < kria->k.kp[ch][p].lstart[m] || temp > kria->k.kp[ch][p].lend[m])
      pos[ch][m] = kria->k.kp[ch][p].lstart[m];
  }
#endif
}

static void calc_scale(op_kria_t *kria, u8 c) {
#ifndef DISABLE_KRIA
  u8 i1;
  cur_scale[c][0] = kria->k.scales[kria->k.pscale[sc[c]]][0];

  for(i1=1;i1<7;i1++) {
    cur_scale[c][i1] = cur_scale[c][i1-1] + kria->k.scales[kria->k.pscale[sc[c]]][i1];
  }
#endif
}

static void bottom_strip_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  u8 i1;
  op_monome->opLedBuffer[112+(ch==0)] = L0;
  op_monome->opLedBuffer[112+ch] = L2;

  for(i1=0;i1<5;i1++)
    op_monome->opLedBuffer[115+i1] = L0;
  if(mode <= mTrans)
    op_monome->opLedBuffer[115 + mode] = L2;

  op_monome->opLedBuffer[121] = L0 + ((mod_mode == modLoop) << 2);
  op_monome->opLedBuffer[122] = L0 + ((mod_mode == modTime) << 2);

  op_monome->opLedBuffer[124] = L0 + ((mode == mScaleEdit) << 2);
  op_monome->opLedBuffer[125] = L0 + ((mode == mPattern) << 2);

  if(key_alt) op_monome->opLedBuffer[127] = L2;
  else op_monome->opLedBuffer[127] = 0;
#endif
}

static void mode_mTr_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1, i2;
  if(mod_mode != modTime) {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    if(mod_mode == modLoop) {
      if(kria->k.kp[ch][p].lswap[tTr]) {
	for(i1=0;i1<kria->k.kp[ch][p].llen[tTr];i1++)
	  op_monome->opLedBuffer[(i1+kria->k.kp[ch][p].lstart[tTr])%16] = L0;
      }
      else {
	for(i1=kria->k.kp[ch][p].lstart[tTr];i1<=kria->k.kp[ch][p].lend[tTr];i1++)
	  op_monome->opLedBuffer[i1] = L0;
      }
      if(kria->k.kp[ch][p].lswap[tAc]) {
	for(i1=0;i1<kria->k.kp[ch][p].llen[tAc];i1++)
	  op_monome->opLedBuffer[16 + (i1 + kria->k.kp[ch][p].lstart[tAc]) % 16] = L0;
      }
      else {
	for(i1=kria->k.kp[ch][p].lstart[tAc];i1<=kria->k.kp[ch][p].lend[tAc];i1++)
	  op_monome->opLedBuffer[16 + i1] = L0;
      }
    }

    for(i1=0;i1<16;i1++) {
      if(kria->k.kp[ch][p].tr[i1])
	op_monome->opLedBuffer[i1] = L1;

      if(kria->k.kp[ch][p].ac[i1])
	op_monome->opLedBuffer[16+i1] = L1;

      for(i2=0;i2<=kria->k.kp[ch][p].oct[i1];i2++)
	op_monome->opLedBuffer[96-16*i2+i1] = L0;

      if(i1 == pos[ch][tTr])
	op_monome->opLedBuffer[i1] += 4;
      if(i1 == pos[ch][tAc])
	op_monome->opLedBuffer[16+i1] += 4;
      if(i1 == pos[ch][tOct])
	op_monome->opLedBuffer[96 - kria->k.kp[ch][p].oct[i1]*16 + i1] += 4;
    }

    if(kria->k.kp[ch][p].lswap[tTr]) {
      for(i1=0;i1<16;i1++)
	if(op_monome->opLedBuffer[i1])
	  if((i1 < kria->k.kp[ch][p].lstart[tTr]) && (i1 > kria->k.kp[ch][p].lend[tTr]))
	    op_monome->opLedBuffer[i1] -= 6;
    }
    else {
      for(i1=0;i1<16;i1++)
	if(op_monome->opLedBuffer[i1])
	  if((i1 < kria->k.kp[ch][p].lstart[tTr]) || (i1 > kria->k.kp[ch][p].lend[tTr]))
	    op_monome->opLedBuffer[i1] -= 6;
    }

    if(kria->k.kp[ch][p].lswap[tAc]) {
      for(i1=0;i1<16;i1++)
	if(op_monome->opLedBuffer[16+i1])
	  if((i1 < kria->k.kp[ch][p].lstart[tAc]) && (i1 > kria->k.kp[ch][p].lend[tAc]))
	    op_monome->opLedBuffer[16+i1] -= 6;
    }
    else {
      for(i1=0;i1<16;i1++)
	if(op_monome->opLedBuffer[16+i1])
	  if((i1 < kria->k.kp[ch][p].lstart[tAc]) || (i1 > kria->k.kp[ch][p].lend[tAc]))
	    op_monome->opLedBuffer[16+i1] -= 6;
    }

    if(kria->k.kp[ch][p].lswap[tOct]) {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tOct]) && (i1 > kria->k.kp[ch][p].lend[tOct]))
	  for(i2=0;i2<=kria->k.kp[ch][p].oct[i1];i2++)
	    op_monome->opLedBuffer[96-16*i2+i1] -= 2;
    }
    else {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tOct]) || (i1 > kria->k.kp[ch][p].lend[tOct]))
	  for(i2=0;i2<=kria->k.kp[ch][p].oct[i1];i2++)
	    op_monome->opLedBuffer[96-16*i2+i1] -= 2;
    }
  }
  else {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[pos[ch][tTr]] = L0;
    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tTr] - 1] = L2;

    op_monome->opLedBuffer[pos[ch][tAc]+32] = L0;
    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tAc] - 1 + 32] = L2;

    op_monome->opLedBuffer[pos[ch][tOct]+64] = L0;
    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tOct] - 1 + 64] = L2;
  }
#endif
}

static void mode_mDur_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1, i2;
  if(mod_mode != modTime) {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[kria->k.kp[ch][p].dur_mul - 1] = L1;

    for(i1=0;i1<16;i1++) {
      for(i2=0;i2<=kria->k.kp[ch][p].dur[i1];i2++)
	op_monome->opLedBuffer[16+16*i2+i1] = L0;

      if(i1 == pos[ch][tDur])
	op_monome->opLedBuffer[16+i1+16*kria->k.kp[ch][p].dur[i1]] += 4;
    }

    if(kria->k.kp[ch][p].lswap[tDur]) {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tDur]) && (i1 > kria->k.kp[ch][p].lend[tDur]))
	  for(i2=0;i2<=kria->k.kp[ch][p].dur[i1];i2++)
	    op_monome->opLedBuffer[16+16*i2+i1] -= 2;
    }
    else {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tDur]) || (i1 > kria->k.kp[ch][p].lend[tDur]))
	  for(i2=0;i2<=kria->k.kp[ch][p].dur[i1];i2++)
	    op_monome->opLedBuffer[16+16*i2+i1] -= 2;
    }
  }
  else {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[pos[ch][tDur]] = L0;

    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tDur] - 1] = L2;
  }
#endif
}

static void mode_mNote_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1;
  if(mod_mode != modTime) {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;
    for(i1=0;i1<16;i1++)
      op_monome->opLedBuffer[i1+(6-kria->k.kp[ch][p].note[i1])*16] = L1;
    op_monome->opLedBuffer[pos[ch][tNote] + (6-kria->k.kp[ch][p].note[pos[ch][tNote]])*16] += 4;

    if(kria->k.kp[ch][p].lswap[tNote]) {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tNote]) && (i1 > kria->k.kp[ch][p].lend[tNote]))
	  op_monome->opLedBuffer[i1+(6-kria->k.kp[ch][p].note[i1])*16] -= 4;
    }
    else {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tNote]) || (i1 > kria->k.kp[ch][p].lend[tNote]))
	  op_monome->opLedBuffer[i1+(6-kria->k.kp[ch][p].note[i1])*16] -= 4;
    }
  }
  else {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[pos[ch][tNote]] = L0;

    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tNote] - 1] = L2;
  }
#endif
}

static void mode_mScale_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1, i2;
  if(mod_mode != modTime) {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    for(i1=0;i1<16;i1++) {
      for(i2=0;i2<=kria->k.kp[ch][p].sc[i1];i2++)
	op_monome->opLedBuffer[16*i2+i1] = L0;

      op_monome->opLedBuffer[i1+16*kria->k.kp[ch][p].sc[i1]] = L1;

      if(i1 == pos[ch][tScale])
	op_monome->opLedBuffer[i1+16*kria->k.kp[ch][p].sc[i1]] += 4;
    }

    if(kria->k.kp[ch][p].lswap[tScale]) {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tScale]) && (i1 > kria->k.kp[ch][p].lend[tScale]))
	  for(i2=0;i2<=kria->k.kp[ch][p].sc[i1];i2++)
	    op_monome->opLedBuffer[16*i2+i1] -= 4;
    }
    else {
      for(i1=0;i1<16;i1++)
	if((i1 < kria->k.kp[ch][p].lstart[tScale]) || (i1 > kria->k.kp[ch][p].lend[tScale]))
	  for(i2=0;i2<=kria->k.kp[ch][p].sc[i1];i2++)
	    op_monome->opLedBuffer[16*i2+i1] -= 4;;
    }
  }
  else {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[pos[ch][tScale]] = L0;

    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tScale] - 1] = L2;
  }
#endif
}

static void mode_mTrans_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1;
  if(mod_mode != modTime) {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    if(mod_mode == modLoop) {
      if(kria->k.kp[ch][p].lswap[tTrans]) {
	for(i1=0;i1<kria->k.kp[ch][p].llen[tTrans];i1++)
	  op_monome->opLedBuffer[(i1+kria->k.kp[ch][p].lstart[tTrans])%16] = L0;
      }
      else {
	for(i1=kria->k.kp[ch][p].lstart[tTrans];i1<=kria->k.kp[ch][p].lend[tTrans];i1++)
	  op_monome->opLedBuffer[i1] = L0;
      }
    }

    op_monome->opLedBuffer[trans_edit] = L1;
    op_monome->opLedBuffer[pos[ch][tTrans]] += 4;

    for(i1=0;i1<16;i1++) {
      op_monome->opLedBuffer[(kria->k.kp[ch][p].trans[i1] & 0xf) + 16*(6-(kria->k.kp[ch][p].trans[i1] >> 4))] = L0;
    }

    op_monome->opLedBuffer[(kria->k.kp[ch][p].trans[pos[ch][tTrans]] & 0xf) + 16*(6-(kria->k.kp[ch][p].trans[pos[ch][tTrans]] >> 4))] += 4;
    op_monome->opLedBuffer[(kria->k.kp[ch][p].trans[trans_edit] & 0xf) + 16*(6-(kria->k.kp[ch][p].trans[trans_edit] >> 4))] = L2;
  }
  else {
    for(i1=0;i1<112;i1++)
      op_monome->opLedBuffer[i1] = 0;

    op_monome->opLedBuffer[pos[ch][tTrans]] = L0;

    op_monome->opLedBuffer[kria->k.kp[ch][p].tmul[tTrans] - 1] = L2;
  }
#endif
}

static void mode_mScaleEdit_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  op_kria_t *kria = (op_kria_t *) op_monome;
  u8 i1;
  op_monome->opLedBuffer[112] = L1;
  op_monome->opLedBuffer[113] = L1;

  for(i1=0;i1<112;i1++)
    op_monome->opLedBuffer[i1] = 0;
  for(i1=0;i1<7;i1++) {
    op_monome->opLedBuffer[i1*16] = 4;
    op_monome->opLedBuffer[97+i1] = L0;
    op_monome->opLedBuffer[8+16*i1] = L0;
  }
  op_monome->opLedBuffer[kria->k.kp[0][p].sc[pos[0][tScale]] * 16] = L1;
  op_monome->opLedBuffer[kria->k.kp[1][p].sc[pos[1][tScale]] * 16] = L1;
  op_monome->opLedBuffer[pscale_edit * 16] = L2;

  op_monome->opLedBuffer[(kria->k.pscale[pscale_edit] / 7) * 16 + 1 + (kria->k.pscale[pscale_edit] % 7)] = L2;

  for(i1=0;i1<7;i1++)
    op_monome->opLedBuffer[kria->k.scales[kria->k.pscale[pscale_edit]][i1] + 8 + (6-i1)*16] = L1;

  if(sc[0] == pscale_edit && tr[0]) {
    i1 = kria->k.kp[0][p].note[pos[0][tNote]];
    op_monome->opLedBuffer[kria->k.scales[kria->k.pscale[pscale_edit]][i1] + 8 + (6-i1)*16] = L2;
  }

  if(sc[1] == pscale_edit && tr[1]) {
    i1 = kria->k.kp[1][p].note[pos[1][tNote]];
    op_monome->opLedBuffer[kria->k.scales[kria->k.pscale[pscale_edit]][i1] + 8 + (6-i1)*16] = L2;
  }
#endif
}

static void mode_mPattern_redraw (t_monome *op_monome) {
#ifndef DISABLE_KRIA
  u8 i1;
  op_monome->opLedBuffer[112] = L1;
  op_monome->opLedBuffer[113] = L1;

  for(i1=0;i1<112;i1++)
    op_monome->opLedBuffer[i1] = 0;
  for(i1=0;i1<16;i1++)
    op_monome->opLedBuffer[i1] = L0;
  if(p_next != p)
    op_monome->opLedBuffer[p_next] = L1;
  op_monome->opLedBuffer[p] = L2;
#endif
}

static void kria_refresh(t_monome *op_monome) {
#ifndef DISABLE_KRIA
  bottom_strip_redraw(op_monome);

  if(mode == mTr) {
    mode_mTr_redraw(op_monome);
  }
  else if(mode == mDur) {
    mode_mDur_redraw(op_monome);
  }
  else if(mode == mNote) {
    mode_mNote_redraw(op_monome);
  }
  else if(mode == mScale) {
    mode_mScale_redraw(op_monome);
  }
  else if(mode == mTrans) {
    mode_mTrans_redraw(op_monome);
  }
  else if(mode==mScaleEdit) {
    mode_mScaleEdit_redraw(op_monome);
  }
  else if(mode==mPattern) {
    mode_mPattern_redraw(op_monome);
  }

#endif
}

/* // pickle / unpickle */
void op_kria_pickle(op_kria_t* kria, FILE *f) {
  fwrite(&kria->octave, sizeof(s16), 1, f);
  fwrite(&kria->tuning, sizeof(s16), 1, f);
  fwrite(&kria->k, sizeof(kria_set), 1, f);
}

void op_kria_unpickle(op_kria_t* kria, FILE* f) {
  fread(&kria->octave, sizeof(s16), 1, f);
  fread(&kria->tuning, sizeof(s16), 1, f);
  fread(&kria->k, sizeof(kria_set), 1, f);
}

