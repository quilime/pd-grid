#include "whitewhale.h"
#include <stdlib.h>

//// network inputs:
static void op_ww_in_bang(op_ww_t* grid);
static void op_ww_in_param(op_ww_t* grid, float f);

// polled-operator handler
static void op_ww_poll_handler(void* op);

/// monome event handler
static void op_ww_handler(t_monome* op_monome, u8 x, u8 y, u8 z);

// pickles
void op_ww_pickle(op_ww_t* ww, FILE *f);
void op_ww_unpickle(op_ww_t* ww, FILE *f);

static void op_ww_redraw(t_monome *op_monome);

static u32 rnd(void);

static u32 a1 = 0x19660d;
static u32 c1 = 0x3c6ef35f;
static u32 x1 = 1234567;  // seed
static u32 a2 = 0x19660d;
static u32 c2 = 0x3c6ef35f;
static u32 x2 = 7654321;  // seed




//////////////////////////////////////////////////

static const u16 SCALES[24][16] = {

  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },                        // ZERO
  { 0, 68, 136, 170, 238, 306, 375, 409, 477, 545, 579, 647, 715, 784, 818, 886, },         // ionian [2, 2, 1, 2, 2, 2, 1]
  { 0, 68, 102, 170, 238, 306, 340, 409, 477, 511, 579, 647, 715, 750, 818, 886, },         // dorian [2, 1, 2, 2, 2, 1, 2]
  { 0, 34, 102, 170, 238, 272, 340, 409, 443, 511, 579, 647, 681, 750, 818, 852, },         // phrygian [1, 2, 2, 2, 1, 2, 2]
  { 0, 68, 136, 204, 238, 306, 375, 409, 477, 545, 613, 647, 715, 784, 818, 886, },         // lydian [2, 2, 2, 1, 2, 2, 1]
  { 0, 68, 136, 170, 238, 306, 340, 409, 477, 545, 579, 647, 715, 750, 818, 886, },         // mixolydian [2, 2, 1, 2, 2, 1, 2]
  { 0, 68, 136, 170, 238, 306, 340, 409, 477, 545, 579, 647, 715, 750, 818, 886, },         // aeolian [2, 1, 2, 2, 1, 2, 2]
  { 0, 34, 102, 170, 204, 272, 340, 409, 443, 511, 579, 613, 681, 750, 818, 852, },         // locrian [1, 2, 2, 1, 2, 2, 2]
  {
    0, 34, 68, 102, 136, 170, 204, 238, 272, 306, 341, 375, 409, 443, 477, 511, },          // chromatic
  { 0, 68, 136, 204, 272, 341, 409, 477, 545, 613, 682, 750, 818, 886, 954, 1022,	},        // whole
  { 0, 170, 340, 511, 681, 852, 1022, 1193, 1363, 1534, 1704, 1875, 2045, 2215, 2386, 2556, },    // fourths
  { 0, 238, 477, 715, 954, 1193, 1431, 1670, 1909, 2147, 2386, 2625, 2863, 3102, 3340, 3579,	},   // fifths
  { 0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255, },           // quarter
  { 0, 8, 17, 25, 34, 42, 51, 59, 68, 76, 85, 93, 102, 110, 119, 127,	},              // eighth
  { 0, 61, 122, 163, 245, 327, 429, 491, 552, 613, 654, 736, 818, 920, 982, 1105, },          // just
  { 0, 61, 130, 163, 245, 337, 441, 491, 552, 621, 654, 736, 828, 932, 982, 1105,	},        // pythagorean
  {
    0, 272, 545, 818, 1090, 1363, 1636, 1909, 2181, 2454, 2727, 3000, 3272, 3545, 3818, 4091, },  // equal 10v
  { 0, 136, 272, 409, 545, 682, 818, 955, 1091, 1228, 1364, 1501, 1637, 1774, 1910, 2047,	},    // equal 5v
  { 0, 68, 136, 204, 272, 341, 409, 477, 545, 613, 682, 750, 818, 886, 954, 1023, },        // equal 2.5v
  { 0, 34, 68, 102, 136, 170, 204, 238, 272, 306, 340, 374, 408, 442, 476, 511,		},          // equal 1.25v
  { 0, 53, 118, 196, 291, 405, 542, 708, 908, 1149, 1441, 1792, 2216, 2728, 3345, 4091, },      // log-ish 10v
  { 0, 136, 272, 409, 545, 682, 818, 955, 1091, 1228, 1364, 1501, 1637, 1774, 1910, 2047,	},    // log-ish 5v
  { 0, 745, 1362, 1874, 2298, 2649, 2941, 3182, 3382, 3548, 3685, 3799, 3894, 3972, 4037, 4091, },  // exp-ish 10v
  { 0, 372, 681, 937, 1150, 1325, 1471, 1592, 1692, 1775, 1844, 1901, 1948, 1987, 2020, 2047 }    // exp-ish 5v

};

////////////////////////////////////////////////////////////////////////////////
// prototypes

static void ww_refresh(t_monome *op_monome);

//-------------------------------------------------
//----- extern function definition
static t_class *op_ww_class;
void *ww_new(t_symbol *s, int argc, t_atom *argv);
void ww_free(op_ww_t* op);
void whitewhale_setup (void) {
  net_monome_setup();
  op_ww_class = class_new(gensym("whitewhale"),
			  (t_newmethod)ww_new,
			  (t_method)ww_free, sizeof(op_ww_t),
			  CLASS_DEFAULT,
			  A_GIMME, 0);
  net_monome_add_focus_methods(op_ww_class);
  /* class_addmethod(op_ww_class, */
  /*		  (t_method)op_ww_in_size, gensym("size"), A_DEFFLOAT, 0); */
  // input func pointer array
  class_addbang(op_ww_class, (t_method)op_ww_in_bang);
  class_addmethod(op_ww_class,
		  (t_method)op_ww_in_param, gensym("param"), A_DEFFLOAT, 0);
}

void *ww_new(t_symbol *s, int argc, t_atom *argv) {
  (void) s;
  (void) argc;
  (void) argv;
  u8 i1,i2;
  op_ww_t *op = (op_ww_t *)pd_new(op_ww_class);

  op->tr = outlet_new((t_object *) op, &s_float);
  op->cv = outlet_new((t_object *) op, &s_list);
  op->pos = outlet_new((t_object *) op, &s_float);

  net_monome_init(&op->monome,
		  (monome_handler_t)&op_ww_handler,
		  (monome_pickle_t)&op_ww_pickle,
		  (monome_pickle_t)&op_ww_unpickle);

  op->clk = 0;
  op->param = 0;

  op->x.re = &ww_refresh;

      // clear out some reasonable defaults
    for(i1=0;i1<16;i1++) {
      for(i2=0;i2<16;i2++) {
	op->w.wp[i1].steps[i2] = 0;
	op->w.wp[i1].step_probs[i2] = 255;
	op->w.wp[i1].cv_probs[0][i2] = 255;
	op->w.wp[i1].cv_probs[1][i2] = 255;
	op->w.wp[i1].cv_curves[0][i2] = 0;
	op->w.wp[i1].cv_curves[1][i2] = 0;
	op->w.wp[i1].cv_values[i2] = SCALES[2][i2];
	op->w.wp[i1].cv_steps[0][i2] = 1<<i2;
	op->w.wp[i1].cv_steps[1][i2] = 1<<i2;
      }
      op->w.wp[i1].step_choice = 0;
      op->w.wp[i1].loop_end = 15;
      op->w.wp[i1].loop_len = 15;
      op->w.wp[i1].loop_start = 0;
      op->w.wp[i1].loop_dir = 0;
      op->w.wp[i1].step_mode = mForward;
      op->w.wp[i1].cv_mode[0] = 0;
      op->w.wp[i1].cv_mode[1] = 0;
      op->w.wp[i1].tr_mode = 0;
    }

    op->w.series_start = 0;
    op->w.series_end = 3;

    op->w.tr_mute[0] = 1;
    op->w.tr_mute[1] = 1;
    op->w.tr_mute[2] = 1;
    op->w.tr_mute[3] = 1;
    op->w.cv_mute[0] = 1;
    op->w.cv_mute[1] = 1;

    for(i1=0;i1<64;i1++)
      op->w.series_list[i1] = 1;

    op->x.LENGTH = 15;
    op->x.SIZE = 16;
    op->x.VARI = 1;

  // init monome drawing
  op_ww_redraw(&op->monome);

  op->clock = clock_new(op, (t_method) op_ww_poll_handler);
  clock_delay(op->clock, OP_WW_POLL_TIME);
  return (void *) op;
}

// de-init
void ww_free(op_ww_t* op) {
  // release focus
  net_monome_deinit((t_monome *) op);
  clock_free(op->clock);
}

//-------------------------------------------------
//----- static function definition


static void op_ww_in_bang(op_ww_t* op ) {
  static u8 i1, count;
  static u16 found[16];

  if(op->x.pattern_jump) {
    op->x.pattern = op->x.next_pattern;
    op->x.next_pos = op->w.wp[op->x.pattern].loop_start;
    op->x.pattern_jump = 0;
  }
  // for series mode and delayed pattern change
  if(op->x.series_jump) {
    op->x.series_pos = op->x.series_next;
    if(op->x.series_pos == op->w.series_end)
      op->x.series_next = op->w.series_start;
    else
      op->x.series_next++;

    // print_dbg("\r\nSERIES next ");
    // print_dbg_ulong(op->x.series_next);
    // print_dbg(" pos ");
    // print_dbg_ulong(op->x.series_pos);

    count = 0;
    for(i1=0;i1<16;i1++) {
      if((op->w.series_list[op->x.series_pos] >> i1) & 1) {
	found[count] = i1;
	count++;
      }
    }

    if(count == 1)
      op->x.next_pattern = found[0];
    else {

      op->x.next_pattern = found[rnd()%count];
    }

    op->x.pattern = op->x.next_pattern;
    op->x.series_playing = op->x.pattern;
    if(op->w.wp[op->x.pattern].step_mode == mReverse)
      op->x.next_pos = op->w.wp[op->x.pattern].loop_end;
    else
      op->x.next_pos = op->w.wp[op->x.pattern].loop_start;

    op->x.series_jump = 0;
    op->x.series_step = 0;
  }

  op->x.pos = op->x.next_pos;

  // live param record
  if(op->x.param_accept && op->x.live_in) {
    op->x.param_dest = &op->w.wp[op->x.pattern].cv_curves[op->x.edit_cv_ch][op->x.pos];
    op->w.wp[op->x.pattern].cv_curves[op->x.edit_cv_ch][op->x.pos] = op->param;
  }

  // calc next step
  if(op->w.wp[op->x.pattern].step_mode == mForward) {     // FORWARD
    if(op->x.pos == op->w.wp[op->x.pattern].loop_end) op->x.next_pos = op->w.wp[op->x.pattern].loop_start;
    else if(op->x.pos >= op->x.LENGTH) op->x.next_pos = 0;
    else op->x.next_pos++;
    op->x.cut_pos = 0;
  }
  else if(op->w.wp[op->x.pattern].step_mode == mReverse) {  // REVERSE
    if(op->x.pos == op->w.wp[op->x.pattern].loop_start)
      op->x.next_pos = op->w.wp[op->x.pattern].loop_end;
    else if(op->x.pos <= 0)
      op->x.next_pos = op->x.LENGTH;
    else op->x.next_pos--;
    op->x.cut_pos = 0;
  }
  else if(op->w.wp[op->x.pattern].step_mode == mDrunk) {  // DRUNK
    op->x.drunk_step += (rnd() % 3) - 1; // -1 to 1
    if(op->x.drunk_step < -1) op->x.drunk_step = -1;
    else if(op->x.drunk_step > 1) op->x.drunk_step = 1;

    op->x.next_pos += op->x.drunk_step;
    if(op->x.next_pos < 0)
      op->x.next_pos = op->x.LENGTH;
    else if(op->x.next_pos > op->x.LENGTH)
      op->x.next_pos = 0;
    else if(op->w.wp[op->x.pattern].loop_dir == 1 && op->x.next_pos < op->w.wp[op->x.pattern].loop_start)
      op->x.next_pos = op->w.wp[op->x.pattern].loop_end;
    else if(op->w.wp[op->x.pattern].loop_dir == 1 && op->x.next_pos > op->w.wp[op->x.pattern].loop_end)
      op->x.next_pos = op->w.wp[op->x.pattern].loop_start;
    else if(op->w.wp[op->x.pattern].loop_dir == 2 && op->x.next_pos < op->w.wp[op->x.pattern].loop_start && op->x.next_pos > op->w.wp[op->x.pattern].loop_end) {
      if(op->x.drunk_step == 1)
	op->x.next_pos = op->w.wp[op->x.pattern].loop_start;
      else
	op->x.next_pos = op->w.wp[op->x.pattern].loop_end;
    }

    op->x.cut_pos = 1;
  }
  else if(op->w.wp[op->x.pattern].step_mode == mRandom) { // RANDOM
    op->x.next_pos = (rnd() % (op->w.wp[op->x.pattern].loop_len + 1)) + op->w.wp[op->x.pattern].loop_start;
    // print_dbg("\r\nnext pos:");
    // print_dbg_ulong(op->x.next_pos);
    if(op->x.next_pos > op->x.LENGTH) op->x.next_pos -= op->x.LENGTH + 1;
    op->x.cut_pos = 1;
  }

  // next pattern?
  if(op->x.pos == op->w.wp[op->x.pattern].loop_end && op->w.wp[op->x.pattern].step_mode == mForward) {
    if(op->x.edit_mode == mSeries)
      op->x.series_jump++;
    else if(op->x.next_pattern != op->x.pattern)
      op->x.pattern_jump++;
  }
  else if(op->x.pos == op->w.wp[op->x.pattern].loop_start && op->w.wp[op->x.pattern].step_mode == mReverse) {
    if(op->x.edit_mode == mSeries)
      op->x.series_jump++;
    else if(op->x.next_pattern != op->x.pattern)
      op->x.pattern_jump++;
  }
  else if(op->x.series_step == op->w.wp[op->x.pattern].loop_len) {
    op->x.series_jump++;
  }

  if(op->x.edit_mode == mSeries)
    op->x.series_step++;


  // TRIGGER
  op->x.triggered = 0;
  if((rnd() % 255) < op->w.wp[op->x.pattern].step_probs[op->x.pos]) {

    if(op->w.wp[op->x.pattern].step_choice & 1<<op->x.pos) {
      count = 0;
      for(i1=0;i1<4;i1++)
	if(op->w.wp[op->x.pattern].steps[op->x.pos] >> i1 & 1) {
	  found[count] = i1;
	  count++;
	}

      if(count == 0)
	op->x.triggered = 0;
      else if(count == 1)
	op->x.triggered = 1<<found[0];
      else
	op->x.triggered = 1<<found[rnd()%count];
    }
    else {
      op->x.triggered = op->w.wp[op->x.pattern].steps[op->x.pos];
    }

    if(op->w.wp[op->x.pattern].tr_mode == 0) {
      if(op->x.triggered & 0x1 && op->w.tr_mute[0]) op->x.tr[0] = 1;
      if(op->x.triggered & 0x2 && op->w.tr_mute[1]) op->x.tr[1] = 1;
      if(op->x.triggered & 0x4 && op->w.tr_mute[2]) op->x.tr[2] = 1;
      if(op->x.triggered & 0x8 && op->w.tr_mute[3]) op->x.tr[3] = 1;
    } else {
      if(op->w.tr_mute[0]) {
	if(op->x.triggered & 0x1) op->x.tr[0] = 1;
	else op->x.tr[0] = 0;
      }
      if(op->w.tr_mute[1]) {
	if(op->x.triggered & 0x2) op->x.tr[1] = 1;
	else op->x.tr[1] = 0;
      }
      if(op->w.tr_mute[2]) {
	if(op->x.triggered & 0x4) op->x.tr[2] = 1;
	else op->x.tr[2] = 0;
      }
      if(op->w.tr_mute[3]) {
	if(op->x.triggered & 0x8) op->x.tr[3] = 1;
	else op->x.tr[3] = 0;
      }

    }
  }

  op->x.dirty++;


  // PARAM 0
  if((rnd() % 255) < op->w.wp[op->x.pattern].cv_probs[0][op->x.pos] && op->w.cv_mute[0]) {
    if(op->w.wp[op->x.pattern].cv_mode[0] == 0) {
      op->x.cv0 = op->w.wp[op->x.pattern].cv_curves[0][op->x.pos];
    }
    else {
      count = 0;
      for(i1=0;i1<16;i1++)
	if(op->w.wp[op->x.pattern].cv_steps[0][op->x.pos] & (1<<i1)) {
	  found[count] = i1;
	  count++;
	}
      if(count == 1)
	op->x.cv_chosen[0] = found[0];
      else
	op->x.cv_chosen[0] = found[rnd() % count];
      op->x.cv0 = op->w.wp[op->x.pattern].cv_values[op->x.cv_chosen[0]];
    }
  }

  // PARAM 1
  if((rnd() % 255) < op->w.wp[op->x.pattern].cv_probs[1][op->x.pos] && op->w.cv_mute[1]) {
    if(op->w.wp[op->x.pattern].cv_mode[1] == 0) {
      op->x.cv1 = op->w.wp[op->x.pattern].cv_curves[1][op->x.pos];
    }
    else {
      count = 0;
      for(i1=0;i1<16;i1++)
	if(op->w.wp[op->x.pattern].cv_steps[1][op->x.pos] & (1<<i1)) {
	  found[count] = i1;
	  count++;
	}
      if(count == 1)
	op->x.cv_chosen[1] = found[0];
      else
	op->x.cv_chosen[1] = found[rnd() % count];

      op->x.cv1 = op->w.wp[op->x.pattern].cv_values[op->x.cv_chosen[1]];
    }
  }



  // cv is a 'continuous quantity' - so output CV *every* bang, also
  // *before* outputting any triggers
  t_atom cvOut[2];
  cvOut[0].a_type = A_FLOAT;
  cvOut[1].a_type = A_FLOAT;
  cvOut[0].a_w.w_float = (float)op->x.cv0;
  cvOut[1].a_w.w_float = (float)op->x.cv1;
  outlet_list(op->cv, &s_list, 2, cvOut);

  int i;
  for(i=0; i <  4; i++) {
    if(op->x.tr[i]) {
      outlet_float(op->tr, i);
    }
  }
  op->x.tr[0] = 0;
  op->x.tr[1] = 0;
  op->x.tr[2] = 0;
  op->x.tr[3] = 0;


  // print_dbg("\r\n pos: ");
  // print_dbg_ulong(pos);
  outlet_float(op->pos, (float) op->x.pos);
}

static void op_ww_in_param(op_ww_t* op, float f) {
  if(f < 0.0) op->param = 0;
  else if(f > 4095.0) op->param = 4095;
  else op->param = (s16) f;

  op->x.param = op->param;

  u8 i;

  // PARAM POT INPUT
  if(op->x.param_accept && op->x.edit_prob) {
    *op->x.param_dest8 = op->param >> 4; // scale to 0-255;
    // print_dbg("\r\nnew prob: ");
    // print_dbg_ulong(*op->x.param_dest8);
    // print_dbg("\t" );
    // print_dbg_ulong(adc[1]);
  }
  else if(op->x.param_accept) {
    *op->x.param_dest = op->param;
    op->x.dirty++;
  }
  else if(op->x.screll) {
    i = op->param>>6;
    if(i > 58) i = 58;
    if(i != op->x.screll_pos) {
      op->x.screll_pos = i;
      op->x.dirty++;
      // print_dbg("\r op->x.screll pos: ");
      // print_dbg_ulong(op->x.screll_pos);
    }
  }



// net_activate(op, i, 1);
}

// poll event handler
static void op_ww_poll_handler(void* op) {
  static u16 i1,x,n1;
  op_ww_t* ww = (op_ww_t*)op;
  for(i1=0;i1<ww->x.key_count;i1++) {
    if(ww->x.key_times[ww->x.held_keys[i1]])
      if(--ww->x.key_times[ww->x.held_keys[i1]]==0) {
	if(ww->x.edit_mode != mSeries) {
	  // preset copy
	  if(ww->x.held_keys[i1] / 16 == 2) {
	    x = ww->x.held_keys[i1] % 16;
	    for(n1=0;n1<16;n1++) {
	      ww->w.wp[x].steps[n1] = ww->w.wp[ww->x.pattern].steps[n1];
	      ww->w.wp[x].step_probs[n1] = ww->w.wp[ww->x.pattern].step_probs[n1];
	      ww->w.wp[x].cv_values[n1] = ww->w.wp[ww->x.pattern].cv_values[n1];
	      ww->w.wp[x].cv_steps[0][n1] = ww->w.wp[ww->x.pattern].cv_steps[0][n1];
	      ww->w.wp[x].cv_curves[0][n1] = ww->w.wp[ww->x.pattern].cv_curves[0][n1];
	      ww->w.wp[x].cv_probs[0][n1] = ww->w.wp[ww->x.pattern].cv_probs[0][n1];
	      ww->w.wp[x].cv_steps[1][n1] = ww->w.wp[ww->x.pattern].cv_steps[1][n1];
	      ww->w.wp[x].cv_curves[1][n1] = ww->w.wp[ww->x.pattern].cv_curves[1][n1];
	      ww->w.wp[x].cv_probs[1][n1] = ww->w.wp[ww->x.pattern].cv_probs[1][n1];
	    }

	    ww->w.wp[x].cv_mode[0] = ww->w.wp[ww->x.pattern].cv_mode[0];
	    ww->w.wp[x].cv_mode[1] = ww->w.wp[ww->x.pattern].cv_mode[1];

	    ww->w.wp[x].loop_start = ww->w.wp[ww->x.pattern].loop_start;
	    ww->w.wp[x].loop_end = ww->w.wp[ww->x.pattern].loop_end;
	    ww->w.wp[x].loop_len = ww->w.wp[ww->x.pattern].loop_len;
	    ww->w.wp[x].loop_dir = ww->w.wp[ww->x.pattern].loop_dir;

	    ww->x.pattern = x;
	    ww->x.next_pattern = x;
	    ww->x.dirty++;

	    // print_dbg("\r\n saved pattern: ");
	    // print_dbg_ulong(x);
	  }
	}

	// print_dbg("\rlong press: ");
	// print_dbg_ulong(held_keys[i1]);
      }
  }

  if(ww->x.dirty) op_ww_redraw(&ww->monome);

  ww->x.dirty = 0;
  clock_delay(ww->clock, OP_WW_POLL_TIME);
}


// process monome key input
static void op_ww_handler(t_monome* op_monome, u8 x, u8 y, u8 z) {
  op_ww_t *ww = (op_ww_t *)op_monome;
  u8 index, i1, found, count;
  s16 delta;
  // print_dbg("\r\n monome event; x: ");
  // print_dbg_hex(x);
  // print_dbg("; y: 0x");
  // print_dbg_hex(y);
  // print_dbg("; z: 0x");
  // print_dbg_hex(z);

  //// TRACK LONG PRESSES
  index = y*16 + x;
  if(z) {
    ww->x.held_keys[ww->x.key_count] = index;
    ww->x.key_count++;
    ww->x.key_times[index] = 10;    //// THRESHOLD key hold time
  } else {
    found = 0; // "found"
    for(i1 = 0; i1<ww->x.key_count; i1++) {
      if(ww->x.held_keys[i1] == index)
	found++;
      if(found)
	ww->x.held_keys[i1] = ww->x.held_keys[i1+1];
    }
    ww->x.key_count--;

    // FAST PRESS
    if(ww->x.key_times[index] > 0) {
      if(ww->x.edit_mode != mSeries) {
	if(index/16 == 2) {
	  i1 = index % 16;
	  if(ww->x.key_alt)
	    ww->x.next_pattern = i1;
	  else {
	    ww->x.pattern = i1;
	    ww->x.next_pattern = i1;
	  }
	}
      }
      // print_dbg("\r\nfast press: ");
      // print_dbg_ulong(index);
      // print_dbg(": ");
      // print_dbg_ulong(ww->x.key_times[index]);
    }
  }


    // OPTIMIZE: order this if-branch by common priority/use
    //// SORT

  // cut position
  if(y == 1) {
    ww->x.keycount_pos += z * 2 - 1;
    if(ww->x.keycount_pos < 0) ww->x.keycount_pos = 0;
    // print_dbg("\r\nww->x.keycount: ");
    // print_dbg_ulong(ww->x.keycount_pos);

    if(ww->x.keycount_pos == 1 && z) {
      if(ww->x.key_alt == 0) {
	ww->x.next_pos = x;
	ww->x.cut_pos++;
	ww->x.dirty++;
	ww->x.keyfirst_pos = x;
      }
      else if(ww->x.key_alt == 1) {
	if(x == ww->x.LENGTH)
	  ww->w.wp[ww->x.pattern].step_mode = mForward;
	else if(x == ww->x.LENGTH-1)
	  ww->w.wp[ww->x.pattern].step_mode = mReverse;
	else if(x == ww->x.LENGTH-2)
	  ww->w.wp[ww->x.pattern].step_mode = mDrunk;
	else if(x == ww->x.LENGTH-3)
	  ww->w.wp[ww->x.pattern].step_mode = mRandom;
	// FIXME
	else if(x == 0) {
	  if(ww->x.pos == ww->w.wp[ww->x.pattern].loop_start)
	    ww->x.next_pos = ww->w.wp[ww->x.pattern].loop_end;
	  else if(ww->x.pos == 0)
	    ww->x.next_pos = ww->x.LENGTH;
	  else ww->x.next_pos--;
	  ww->x.cut_pos = 1;
	  ww->x.dirty++;
	}
	// FIXME
	else if(x == 1) {
	  if(ww->x.pos == ww->w.wp[ww->x.pattern].loop_end) ww->x.next_pos = ww->w.wp[ww->x.pattern].loop_start;
	  else if(ww->x.pos == ww->x.LENGTH) ww->x.next_pos = 0;
	  else ww->x.next_pos++;
	  ww->x.cut_pos = 1;
	  ww->x.dirty++;
	}
	else if(x == 2 ) {
	  ww->x.next_pos = (rnd() % (ww->w.wp[ww->x.pattern].loop_len + 1)) + ww->w.wp[ww->x.pattern].loop_start;
	  ww->x.cut_pos = 1;
	  ww->x.dirty++;
	}
      }
    }
    else if(ww->x.keycount_pos == 2 && z) {
      ww->w.wp[ww->x.pattern].loop_start = ww->x.keyfirst_pos;
      ww->w.wp[ww->x.pattern].loop_end = x;
      ww->x.dirty++;
      if(ww->w.wp[ww->x.pattern].loop_start > ww->w.wp[ww->x.pattern].loop_end) ww->w.wp[ww->x.pattern].loop_dir = 2;
      else if(ww->w.wp[ww->x.pattern].loop_start == 0 && ww->w.wp[ww->x.pattern].loop_end == ww->x.LENGTH) ww->w.wp[ww->x.pattern].loop_dir = 0;
      else ww->w.wp[ww->x.pattern].loop_dir = 1;

      ww->w.wp[ww->x.pattern].loop_len = ww->w.wp[ww->x.pattern].loop_end - ww->w.wp[ww->x.pattern].loop_start;

      if(ww->w.wp[ww->x.pattern].loop_dir == 2)
	ww->w.wp[ww->x.pattern].loop_len = (ww->x.LENGTH - ww->w.wp[ww->x.pattern].loop_start) + ww->w.wp[ww->x.pattern].loop_end + 1;

      // print_dbg("\r\nloop_len: ");
      // print_dbg_ulong(ww->w.wp[ww->x.pattern].loop_len);
    }
  }

  // top row
  else if(y == 0) {
    if(x == ww->x.LENGTH) {
      ww->x.key_alt = z;
      if(z == 0) {
	ww->x.param_accept = 0;
	ww->x.live_in = 0;
      }
      ww->x.dirty++;
    }
    else if(x < 4 && z) {
      if(ww->x.key_alt)
	ww->w.wp[ww->x.pattern].tr_mode ^= 1;
      else if(ww->x.screll)
	ww->w.tr_mute[x] ^= 1;
      else ww->x.edit_mode = mTrig;
      ww->x.edit_prob = 0;
      ww->x.param_accept = 0;
      ww->x.dirty++;
    }
    else if(ww->x.SIZE==16 && x > 3 && x < 12 && z) {
      ww->x.param_accept = 0;
      ww->x.edit_cv_ch = (x-4)/4;
      ww->x.edit_mode = mMap;
      ww->x.edit_prob = 0;

      if(ww->x.key_alt)
	ww->w.wp[ww->x.pattern].cv_mode[ww->x.edit_cv_ch] ^= 1;
      else if(ww->x.screll)
	ww->w.cv_mute[ww->x.edit_cv_ch] ^= 1;

      ww->x.dirty++;
    }
    else if(ww->x.SIZE==8 && (x == 4 || x == 5) && z) {
      ww->x.param_accept = 0;
      ww->x.edit_cv_ch = x-4;
      ww->x.edit_mode = mMap;
      ww->x.edit_prob = 0;

      if(ww->x.key_alt)
	ww->w.wp[ww->x.pattern].cv_mode[ww->x.edit_cv_ch] ^= 1;
      else if(ww->x.screll)
	ww->w.cv_mute[ww->x.edit_cv_ch] ^= 1;

      ww->x.dirty++;
    }
    else if(x == ww->x.LENGTH-1 && z && ww->x.key_alt) {
      ww->x.edit_mode = mSeries;
      ww->x.dirty++;
    }
    else if(x == ww->x.LENGTH-1)
      ww->x.screll = z;
  }


  // toggle steps and prob control
  else if(ww->x.edit_mode == mTrig) {
    if(z && y>3 && ww->x.edit_prob == 0) {
      if(ww->x.key_alt)
	ww->w.wp[ww->x.pattern].steps[ww->x.pos] |=  1 << (y-4);
      else if(ww->x.screll) {
	ww->w.wp[ww->x.pattern].step_choice ^= (1<<x);
      }
      else
	ww->w.wp[ww->x.pattern].steps[x] ^= (1<<(y-4));
      ww->x.dirty++;
    }
    // step probs
    else if(z && y==3) {
      if(ww->x.key_alt)
	ww->x.edit_prob = 1;
      else {
	if(ww->w.wp[ww->x.pattern].step_probs[x] == 255) ww->w.wp[ww->x.pattern].step_probs[x] = 0;
	else ww->w.wp[ww->x.pattern].step_probs[x] = 255;
      }
      ww->x.dirty++;
    }
    else if(ww->x.edit_prob == 1) {
      if(z) {
	if(y == 4) ww->w.wp[ww->x.pattern].step_probs[x] = 192;
	else if(y == 5) ww->w.wp[ww->x.pattern].step_probs[x] = 128;
	else if(y == 6) ww->w.wp[ww->x.pattern].step_probs[x] = 64;
	else ww->w.wp[ww->x.pattern].step_probs[x] = 0;
      }
    }
  }

  // edit map and probs
  else if(ww->x.edit_mode == mMap) {
    // step probs
    if(z && y==3) {
      if(ww->x.key_alt)
	ww->x.edit_prob = 1;
      else  {
	if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] == 255) ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 0;
	else ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 255;
      }

      ww->x.dirty++;
    }
    // edit data
    else if(ww->x.edit_prob == 0) {
      // CURVES
      if(ww->w.wp[ww->x.pattern].cv_mode[ww->x.edit_cv_ch] == 0) {
	if(y == 4 && z) {
	  if(ww->x.center)
	    delta = 3;
	  else if(ww->x.key_alt)
	    delta = 409;
	  else
	    delta = 34;

	  // saturate
	  if(ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] + delta < 4092)
	    ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] += delta;
	  else
	    ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] = 4092;
	}
	else if(y == 6 && z) {
	  if(ww->x.center)
	    delta = 3;
	  else if(ww->x.key_alt)
	    delta = 409;
	  else
	    delta = 34;

	  // saturate
	  if(ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] > delta)
	    ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] -= delta;
	  else
	    ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] = 0;

	}
	else if(y == 5) {
	  if(z == 1) {
	    ww->x.center = 1;
	    if(ww->x.key_alt)
	      ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] = ww->x.clip;
	    else
	      ww->x.clip = ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x];
	  }
	  else
	    ww->x.center = 0;
	}
	else if(y == 7) {
	  if(ww->x.key_alt && z) {
	    ww->x.param_dest = &ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][ww->x.pos];
	    ww->x.param = ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][ww->x.pos];
	    ww->x.param_accept = 1;
	    ww->x.live_in = 1;
	  }
	  else if(ww->x.center && z) {
	    ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] = rand() % 4092;
	  }
	  else {
	    ww->x.param_accept = z;
	    ww->x.param_dest = &ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x];
	    if(z) ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][x] = ww->x.param;
	  }
	  ww->x.dirty++;
	}
      }
      // MAP
      else {
	if(ww->x.scale_select && z) {
	  // index -= 64;
	  index = (y-4) * 8 + x;
	  if(index < 24 && y<8) {
	    for(i1=0;i1<16;i1++)
	      ww->w.wp[ww->x.pattern].cv_values[i1] = SCALES[index][i1];
	  }

	  ww->x.scale_select = 0;
	  ww->x.dirty++;
	}
	else {
	  if(z && y==4) {
	    ww->x.edit_cv_step = x;
	    count = 0;
	    for(i1=0;i1<16;i1++)
		if((ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] >> i1) & 1) {
		  count++;
		  ww->x.edit_cv_value = i1;
		}
	    if(count>1)
	      ww->x.edit_cv_value = -1;

	    ww->x.keycount_cv = 0;

	    ww->x.dirty++;
	  }
	  // load scale
	  else if(ww->x.key_alt && y==7 && x == 0 && z) {
	    ww->x.scale_select++;
	    ww->x.dirty++;
	  }
	  // read pot
	  else if(y==7 && ww->x.key_alt && ww->x.edit_cv_value != -1 && x==ww->x.LENGTH) {
	    ww->x.param_accept = z;
	    ww->x.param_dest = &(ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value]);
	  }
	  else if((y == 5 || y == 6) && z && x<4 && ww->x.edit_cv_step != -1) {
	    delta = 0;
	    if(x == 0)
	      delta = 409;
	    else if(x == 1)
	      delta = 239;
	    else if(x == 2)
	      delta = 34;
	    else if(x == 3)
	      delta = 3;

	    if(y == 6)
	      delta *= -1;

	    if(ww->x.key_alt) {
	      for(i1=0;i1<16;i1++) {
		if(ww->w.wp[ww->x.pattern].cv_values[i1] + delta > 4092)
		  ww->w.wp[ww->x.pattern].cv_values[i1] = 4092;
		else if(delta < 0 && ww->w.wp[ww->x.pattern].cv_values[i1] < -1*delta)
		  ww->w.wp[ww->x.pattern].cv_values[i1] = 0;
		else
		  ww->w.wp[ww->x.pattern].cv_values[i1] += delta;
	      }
	    }
	    else {
	      if(ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] + delta > 4092)
		ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] = 4092;
	      else if(delta < 0 && ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] < -1*delta)
		ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] = 0;
	      else
		ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] += delta;
	    }

	    ww->x.dirty++;
	  }
	  // choose values
	  else if(y==7) {
	    ww->x.keycount_cv += z*2-1;
	    if(ww->x.keycount_cv < 0)
	      ww->x.keycount_cv = 0;

	    if(z) {
	      count = 0;
	      for(i1=0;i1<16;i1++)
		if((ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] >> i1) & 1)
		  count++;

	      // single press toggle
	      if(ww->x.keycount_cv == 1 && count < 2) {
		ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] = (1<<x);
		ww->x.edit_cv_value = x;
	      }
	      // multiselect
	      else if(ww->x.keycount_cv > 1 || count > 1) {
		ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] ^= (1<<x);

		if(!ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step])
		  ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] = (1<<x);

		count = 0;
		for(i1=0;i1<16;i1++)
		  if((ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] >> i1) & 1) {
		    count++;
		    ww->x.edit_cv_value = i1;
		  }

		if(count > 1)
		  ww->x.edit_cv_value = -1;
	      }

	      ww->x.dirty++;
	    }
	  }
	}
      }
    }
    else if(ww->x.edit_prob == 1) {
      if(z) {
	if(y == 4) ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 192;
	else if(y == 5) ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 128;
	else if(y == 6) ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 64;
	else ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][x] = 0;
      }
    }
  }

  // series mode
  else if(ww->x.edit_mode == mSeries) {
    if(z && ww->x.key_alt) {
      if(x == 0)
	ww->x.series_next = y-2+ww->x.screll_pos;
      else if(x == ww->x.LENGTH-1)
	ww->w.series_start = y-2+ww->x.screll_pos;
      else if(x == ww->x.LENGTH)
	ww->w.series_end = y-2+ww->x.screll_pos;

      if(ww->w.series_end < ww->w.series_start)
	ww->w.series_end = ww->w.series_start;
    }
    else {
      ww->x.keycount_series += z*2-1;
      if(ww->x.keycount_series < 0)
	ww->x.keycount_series = 0;

      if(z) {
	count = 0;
	for(i1=0;i1<16;i1++)
	  count += (ww->w.series_list[y-2+ww->x.screll_pos] >> i1) & 1;

	// single press toggle
	if(ww->x.keycount_series == 1 && count < 2) {
	  ww->w.series_list[y-2+ww->x.screll_pos] = (1<<x);
	}
	// multi-select
	else if(ww->x.keycount_series > 1 || count > 1) {
	  ww->w.series_list[y-2+ww->x.screll_pos] ^= (1<<x);

	  // ensure not fully clear
	  if(!ww->w.series_list[y-2+ww->x.screll_pos])
	    ww->w.series_list[y-2+ww->x.screll_pos] = (1<<x);
	}
      }
    }

    ww->x.dirty++;
  }
}



// redraw monome grid
static void op_ww_redraw(t_monome *op_monome) {
  op_ww_t *ww = (op_ww_t *) op_monome;
  (*ww->x.re)(op_monome);
}



static void ww_refresh(t_monome *op_monome) {
   op_ww_t *ww = (op_ww_t *)op_monome;
   u8 i1,i2;


  // clear top, cut, pattern, prob
  for(i1=0;i1<16;i1++) {
    op_monome->opLedBuffer[i1] = 0;
    op_monome->opLedBuffer[16+i1] = 0;
    op_monome->opLedBuffer[32+i1] = 4;
    op_monome->opLedBuffer[48+i1] = 0;
  }

  // dim mode
  if(ww->x.edit_mode == mTrig) {
    op_monome->opLedBuffer[0] = 4;
    op_monome->opLedBuffer[1] = 4;
    op_monome->opLedBuffer[2] = 4;
    op_monome->opLedBuffer[3] = 4;
  }
  else if(ww->x.edit_mode == mMap) {
    if(ww->x.SIZE==16) {
      op_monome->opLedBuffer[4+(ww->x.edit_cv_ch*4)] = 4;
      op_monome->opLedBuffer[5+(ww->x.edit_cv_ch*4)] = 4;
      op_monome->opLedBuffer[6+(ww->x.edit_cv_ch*4)] = 4;
      op_monome->opLedBuffer[7+(ww->x.edit_cv_ch*4)] = 4;
    }
    else
      op_monome->opLedBuffer[4+ww->x.edit_cv_ch] = 4;
  }
  else if(ww->x.edit_mode == mSeries) {
    op_monome->opLedBuffer[ww->x.LENGTH-1] = 7;
  }

  // alt
  op_monome->opLedBuffer[ww->x.LENGTH] = 4;
  if(ww->x.key_alt) op_monome->opLedBuffer[ww->x.LENGTH] = 11;

  // show mutes or on steps
  if(ww->x.screll) {
    if(ww->w.tr_mute[0]) op_monome->opLedBuffer[0] = 11;
    if(ww->w.tr_mute[1]) op_monome->opLedBuffer[1] = 11;
    if(ww->w.tr_mute[2]) op_monome->opLedBuffer[2] = 11;
    if(ww->w.tr_mute[3]) op_monome->opLedBuffer[3] = 11;
  }
  else if(ww->x.triggered) {
    if(ww->x.triggered & 0x1 && ww->w.tr_mute[0]) op_monome->opLedBuffer[0] = 11 - 4 * ww->w.wp[ww->x.pattern].tr_mode;
    if(ww->x.triggered & 0x2 && ww->w.tr_mute[1]) op_monome->opLedBuffer[1] = 11 - 4 * ww->w.wp[ww->x.pattern].tr_mode;
    if(ww->x.triggered & 0x4 && ww->w.tr_mute[2]) op_monome->opLedBuffer[2] = 11 - 4 * ww->w.wp[ww->x.pattern].tr_mode;
    if(ww->x.triggered & 0x8 && ww->w.tr_mute[3]) op_monome->opLedBuffer[3] = 11 - 4 * ww->w.wp[ww->x.pattern].tr_mode;
  }

  // cv indication
  if(ww->x.screll) {
    if(ww->x.SIZE==16) {
      if(ww->w.cv_mute[0]) {
	op_monome->opLedBuffer[4] = 11;
	op_monome->opLedBuffer[5] = 11;
	op_monome->opLedBuffer[6] = 11;
	op_monome->opLedBuffer[7] = 11;
      }
      if(ww->w.cv_mute[1]) {
	op_monome->opLedBuffer[8] = 11;
	op_monome->opLedBuffer[9] = 11;
	op_monome->opLedBuffer[10] = 11;
	op_monome->opLedBuffer[11] = 11;
      }
    }
    else {
      if(ww->w.cv_mute[0])
	op_monome->opLedBuffer[4] = 11;
      if(ww->w.cv_mute[1])
	op_monome->opLedBuffer[5] = 11;
    }

  }
  else if(ww->x.SIZE==16) {
    op_monome->opLedBuffer[ww->x.cv0 / 1024 + 4] = 11;
    op_monome->opLedBuffer[ww->x.cv1 / 1024 + 8] = 11;
  }

  // show pos loop dim
  if(ww->w.wp[ww->x.pattern].loop_dir) {
    for(i1=0;i1<ww->x.SIZE;i1++) {
      if(ww->w.wp[ww->x.pattern].loop_dir == 1 && i1 >= ww->w.wp[ww->x.pattern].loop_start && i1 <= ww->w.wp[ww->x.pattern].loop_end)
	op_monome->opLedBuffer[16+i1] = 4;
      else if(ww->w.wp[ww->x.pattern].loop_dir == 2 && (i1 <= ww->w.wp[ww->x.pattern].loop_end || i1 >= ww->w.wp[ww->x.pattern].loop_start))
	op_monome->opLedBuffer[16+i1] = 4;
    }
  }

  // show position and next cut
  if(ww->x.cut_pos) op_monome->opLedBuffer[16+ww->x.next_pos] = 7;
  op_monome->opLedBuffer[16+ww->x.pos] = 15;

  // show pattern
  op_monome->opLedBuffer[32+ww->x.pattern] = 11;
  if(ww->x.pattern != ww->x.next_pattern) op_monome->opLedBuffer[32+ww->x.next_pattern] = 7;

  // show step data
  if(ww->x.edit_mode == mTrig) {
    if(ww->x.edit_prob == 0) {
      for(i1=0;i1<ww->x.SIZE;i1++) {
	for(i2=0;i2<4;i2++) {
	  if((ww->w.wp[ww->x.pattern].steps[i1] & (1<<i2)) && i1 == ww->x.pos && (ww->x.triggered & 1<<i2) && ww->w.tr_mute[i2]) op_monome->opLedBuffer[(i2+4)*16+i1] = 11;
	  else if(ww->w.wp[ww->x.pattern].steps[i1] & (1<<i2) && (ww->w.wp[ww->x.pattern].step_choice & 1<<i1)) op_monome->opLedBuffer[(i2+4)*16+i1] = 4;
	  else if(ww->w.wp[ww->x.pattern].steps[i1] & (1<<i2)) op_monome->opLedBuffer[(i2+4)*16+i1] = 7;
	  else if(i1 == ww->x.pos) op_monome->opLedBuffer[(i2+4)*16+i1] = 4;
	  else op_monome->opLedBuffer[(i2+4)*16+i1] = 0;
	}

	// probs
	if(ww->w.wp[ww->x.pattern].step_probs[i1] == 255) op_monome->opLedBuffer[48+i1] = 11;
	else if(ww->w.wp[ww->x.pattern].step_probs[i1] > 0) op_monome->opLedBuffer[48+i1] = 4;
      }
    }
    else if(ww->x.edit_prob == 1) {
      for(i1=0;i1<ww->x.SIZE;i1++) {
	op_monome->opLedBuffer[64+i1] = 4;
	op_monome->opLedBuffer[80+i1] = 4;
	op_monome->opLedBuffer[96+i1] = 4;
	op_monome->opLedBuffer[112+i1] = 4;

	if(ww->w.wp[ww->x.pattern].step_probs[i1] == 255)
	  op_monome->opLedBuffer[48+i1] = 11;
	else if(ww->w.wp[ww->x.pattern].step_probs[i1] == 0) {
	  op_monome->opLedBuffer[48+i1] = 0;
	  op_monome->opLedBuffer[112+i1] = 7;
	}
	else if(ww->w.wp[ww->x.pattern].step_probs[i1]) {
	  op_monome->opLedBuffer[48+i1] = 4;
	  op_monome->opLedBuffer[64+16*(3-(ww->w.wp[ww->x.pattern].step_probs[i1]>>6))+i1] = 7;
	}
      }
    }
  }

  // show map
  else if(ww->x.edit_mode == mMap) {
    if(ww->x.edit_prob == 0) {
      // CURVES
      if(ww->w.wp[ww->x.pattern].cv_mode[ww->x.edit_cv_ch] == 0) {
	for(i1=0;i1<ww->x.SIZE;i1++) {
	  // probs
	  if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] == 255) op_monome->opLedBuffer[48+i1] = 11;
	  else if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] > 0) op_monome->opLedBuffer[48+i1] = 7;

	  op_monome->opLedBuffer[112+i1] = (ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][i1] > 1023) * 7;
	  op_monome->opLedBuffer[96+i1] = (ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][i1] > 2047) * 7;
	  op_monome->opLedBuffer[80+i1] = (ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][i1] > 3071) * 7;
	  op_monome->opLedBuffer[64+i1] = 0;
	  op_monome->opLedBuffer[64+16*(3-(ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][i1]>>10))+i1] = (ww->w.wp[ww->x.pattern].cv_curves[ww->x.edit_cv_ch][i1]>>7) & 0x7;
	}

	// play step highlight
	op_monome->opLedBuffer[64+ww->x.pos] += 4;
	op_monome->opLedBuffer[80+ww->x.pos] += 4;
	op_monome->opLedBuffer[96+ww->x.pos] += 4;
	op_monome->opLedBuffer[112+ww->x.pos] += 4;
      }
      // MAP
      else {
	if(!ww->x.scale_select) {
	  for(i1=0;i1<ww->x.SIZE;i1++) {
	    // probs
	    if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] == 255) op_monome->opLedBuffer[48+i1] = 11;
	    else if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] > 0) op_monome->opLedBuffer[48+i1] = 7;

	    // clear edit select line
	    op_monome->opLedBuffer[64+i1] = 4;

	    // show current edit value, selected
	    if(ww->x.edit_cv_value != -1) {
	      if((ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] >> 8) >= i1)
		op_monome->opLedBuffer[80+i1] = 7;
	      else
		op_monome->opLedBuffer[80+i1] = 0;

	      if(((ww->w.wp[ww->x.pattern].cv_values[ww->x.edit_cv_value] >> 4) & 0xf) >= i1)
		op_monome->opLedBuffer[96+i1] = 4;
	      else
		op_monome->opLedBuffer[96+i1] = 0;
	    }
	    else {
	      op_monome->opLedBuffer[80+i1] = 0;
	      op_monome->opLedBuffer[96+i1] = 0;
	    }

	    // show steps
	    if(ww->w.wp[ww->x.pattern].cv_steps[ww->x.edit_cv_ch][ww->x.edit_cv_step] & (1<<i1)) op_monome->opLedBuffer[112+i1] = 7;
	    else op_monome->opLedBuffer[112+i1] = 0;
	  }

	  // show play position
	  op_monome->opLedBuffer[64+ww->x.pos] = 7;
	  // show edit position
	  op_monome->opLedBuffer[64+ww->x.edit_cv_step] = 11;
	  // show playing note
	  op_monome->opLedBuffer[112+ww->x.cv_chosen[ww->x.edit_cv_ch]] = 11;
	}
	else {
	  for(i1=0;i1<ww->x.SIZE;i1++) {
	    // probs
	    if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] == 255) op_monome->opLedBuffer[48+i1] = 11;
	    else if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] > 0) op_monome->opLedBuffer[48+i1] = 7;

	    op_monome->opLedBuffer[64+i1] = (i1<8) * 4;
	    op_monome->opLedBuffer[80+i1] = (i1<8) * 4;
	    op_monome->opLedBuffer[96+i1] = (i1<8) * 4;
	    op_monome->opLedBuffer[112+i1] = 0;
	  }

	  op_monome->opLedBuffer[112] = 7;
	}

      }
    }
    else if(ww->x.edit_prob == 1) {
      for(i1=0;i1<ww->x.SIZE;i1++) {
	op_monome->opLedBuffer[64+i1] = 4;
	op_monome->opLedBuffer[80+i1] = 4;
	op_monome->opLedBuffer[96+i1] = 4;
	op_monome->opLedBuffer[112+i1] = 4;

	if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] == 255)
	  op_monome->opLedBuffer[48+i1] = 11;
	else if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1] == 0) {
	  op_monome->opLedBuffer[48+i1] = 0;
	  op_monome->opLedBuffer[112+i1] = 7;
	}
	else if(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1]) {
	  op_monome->opLedBuffer[48+i1] = 4;
	  op_monome->opLedBuffer[64+16*(3-(ww->w.wp[ww->x.pattern].cv_probs[ww->x.edit_cv_ch][i1]>>6))+i1] = 7;
	}
      }
    }

  }

  // series
  else if(ww->x.edit_mode == mSeries) {
    for(i1 = 0;i1<6;i1++) {
      for(i2=0;i2<ww->x.SIZE;i2++) {
	// start/end bars, clear
	if(i1+ww->x.screll_pos == ww->w.series_start || i1+ww->x.screll_pos == ww->w.series_end) op_monome->opLedBuffer[32+i1*16+i2] = 4;
	else op_monome->opLedBuffer[32+i1*16+i2] = 0;
      }

      // ww->x.screll position helper
      op_monome->opLedBuffer[32+i1*16+((ww->x.screll_pos+i1)/(64/ww->x.SIZE))] = 4;

      // sidebar selection indicators
      if(i1+ww->x.screll_pos > ww->w.series_start && i1+ww->x.screll_pos < ww->w.series_end) {
	op_monome->opLedBuffer[32+i1*16] = 4;
	op_monome->opLedBuffer[32+i1*16+ww->x.LENGTH] = 4;
      }

      for(i2=0;i2<ww->x.SIZE;i2++) {
	// show possible states
	if((ww->w.series_list[i1+ww->x.screll_pos] >> i2) & 1)
	  op_monome->opLedBuffer[32+(i1*16)+i2] = 7;
      }

    }

    // highlight playhead
    if(ww->x.series_pos >= ww->x.screll_pos && ww->x.series_pos < ww->x.screll_pos+6) {
      op_monome->opLedBuffer[32+(ww->x.series_pos-ww->x.screll_pos)*16+ww->x.series_playing] = 11;
    }
  }
}


static u32 rnd() {
  x1 = x1 * c1 + a1;
  x2 = x2 * c2 + a2;
  return (x1>>16) | (x2>>16);
}


// pickle / unpickle
void op_ww_pickle(op_ww_t* ww, FILE *f) {
  fwrite(&ww->w, sizeof(ww->x), 1, f);
  fwrite(&ww->x, sizeof(ww->x), 1, f);
}

void op_ww_unpickle(op_ww_t* ww, FILE *f) {
  fread(&ww->w, sizeof(ww->x), 1, f);
  fread(&ww->x, sizeof(ww->x), 1, f);
  op_ww_redraw(&ww->monome);
}
