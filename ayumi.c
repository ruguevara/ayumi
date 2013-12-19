/* Author: Peter Sovietov */

#include <string.h>
#include <math.h>
#include "ayumi.h"

static const double AY_dac_table[] = {
  0, 0,
  0.012757, 0.012757,
  0.018494, 0.018494,
  0.027054, 0.027054,
  0.039963, 0.039963,
  0.059129, 0.059129,
  0.082353, 0.082353,
  0.13463, 0.13463,
  0.158572, 0.158572,
  0.254917, 0.254917,
  0.35613, 0.35613,
  0.446967, 0.446967,
  0.564111, 0.564111,
  0.708339, 0.708339,
  0.842222, 0.842222,
  1, 1
};

static const double YM_dac_table[] = {
  0, 0,
  0.003784, 0.006867,
  0.010224, 0.012604,
  0.015412, 0.018906,
  0.023682, 0.029282,
  0.035309, 0.04007,
  0.047776, 0.057649,
  0.067247, 0.076768,
  0.091066, 0.10927,
  0.128405, 0.146822,
  0.174273, 0.208881,
  0.243488, 0.278935,
  0.332021, 0.398993,
  0.465751, 0.532219,
  0.632242, 0.753857,
  0.877272, 1
};

static void reset_segment(struct ayumi* ay);

static int update_tone(struct ayumi* ay, int index) {
  struct sound_channel* ch = &ay->channels[index];
  ch->tone_counter += 1;
  if (ch->tone_counter >= ch->tone_period) {
    ch->tone_counter = 0;
    ch->tone ^= 1;
  }
  return ch->tone;
}

static int update_noise(struct ayumi* ay) {
  int bit0x3;
  ay->noise_counter += 1;
  if (ay->noise_counter >= (ay->noise_period << 1)) {
    ay->noise_counter = 0;
    bit0x3 = ((ay->noise ^ (ay->noise >> 3)) & 1);
    ay->noise = (ay->noise >> 1) | (bit0x3 << 16);
  }
  return ay->noise & 1;
}

static void slide_up(struct ayumi* ay) {
  ay->envelope += 1;
  if (ay->envelope > 31) {
    ay->envelope_segment ^= 1;
    reset_segment(ay);
  }
}

static void slide_down(struct ayumi* ay) {
  ay->envelope -= 1;
  if (ay->envelope < 0) {
    ay->envelope_segment ^= 1;
    reset_segment(ay);
  }
}

static void hold_top(struct ayumi* ay) {
  (void) ay;
}

static void hold_bottom(struct ayumi* ay) {
  (void) ay;
}

static void (* const Envelopes[][2])(struct ayumi*) = {
  {slide_down, hold_bottom},
  {slide_down, hold_bottom},
  {slide_down, hold_bottom},
  {slide_down, hold_bottom},
  {slide_up, hold_bottom},
  {slide_up, hold_bottom},
  {slide_up, hold_bottom},
  {slide_up, hold_bottom},
  {slide_down, slide_down},
  {slide_down, hold_bottom},
  {slide_down, slide_up},
  {slide_down, hold_top},
  {slide_up, slide_up},
  {slide_up, hold_top},
  {slide_up, slide_down},
  {slide_up, hold_bottom}
};

static void reset_segment(struct ayumi* ay) {
  if (Envelopes[ay->envelope_shape][ay->envelope_segment] == slide_down
    || Envelopes[ay->envelope_shape][ay->envelope_segment] == hold_top) {
    ay->envelope = 31;
    return;
  }
  ay->envelope = 0;
}

int update_envelope(struct ayumi* ay) {
  ay->envelope_counter += 1;
  if (ay->envelope_counter >= ay->envelope_period) {
    ay->envelope_counter = 0;
    Envelopes[ay->envelope_shape][ay->envelope_segment](ay);
  }
  return ay->envelope;
}

static void update_mixer(struct ayumi* ay) {
  int i;
  int out;
  int noise = update_noise(ay);
  int envelope = update_envelope(ay);
  ay->left = 0;
  ay->right = 0;
  for (i = 0; i < SOUND_CHANNELS; i += 1) {
    out = (update_tone(ay, i) | ay->channels[i].t_off) & (noise | ay->channels[i].n_off);
    out *= ay->channels[i].e_on ? envelope : ay->channels[i].volume * 2 + 1;
    ay->left += ay->dac_table[out] * ay->channels[i].pan_left;
    ay->right += ay->dac_table[out] * ay->channels[i].pan_right;
  }
}

void ayumi_configure(struct ayumi* ay, int is_ym, double clock_rate, int sr) {
  int i;
  memset(ay, 0, sizeof(struct ayumi));
  ay->step = clock_rate / (sr * 8 * DECIMATOR_FACTOR);
  ay->dac_table = is_ym ? YM_dac_table : AY_dac_table;
  ay->noise = 1;
  ayumi_set_envelope(ay, 1);
  for (i = 0; i < SOUND_CHANNELS; i += 1) {
    ayumi_set_tone(ay, i, 1);
  }
}

void ayumi_set_pan(struct ayumi* ay, int index, double pan, int is_eqp) {
  if (is_eqp) {
    ay->channels[index].pan_left = sqrt(1 - pan);
    ay->channels[index].pan_right = sqrt(pan);
  } else {
    ay->channels[index].pan_left = 1 - pan;
    ay->channels[index].pan_right = pan;
  }
}

void ayumi_set_tone(struct ayumi* ay, int index, int period) {
  period &= 0xfff;
  ay->channels[index].tone_period = (period == 0) | period;
}

void ayumi_set_noise(struct ayumi* ay, int period) {
  ay->noise_period = period & 0x1f;
}

void ayumi_set_mixer(struct ayumi* ay, int index, int t_off, int n_off, int e_on) {
  ay->channels[index].t_off = t_off & 1;
  ay->channels[index].n_off = n_off & 1;
  ay->channels[index].e_on = e_on;
}

void ayumi_set_volume(struct ayumi* ay, int index, int volume) {
  ay->channels[index].volume = volume & 0xf;
}

void ayumi_set_envelope(struct ayumi* ay, int period) {
  period &= 0xffff;
  ay->envelope_period = (period == 0) | period;
}

void ayumi_set_envelope_shape(struct ayumi* ay, int shape) {
  ay->envelope_shape = shape & 0xf;
  ay->envelope_counter = 0;
  ay->envelope_segment = 0;
  reset_segment(ay);
}

static double decimator(double* x, double* samples) {
  int i;
  double sum = 0;
  memmove(x + DECIMATOR_FACTOR, x, (DECIMATOR_SIZE - DECIMATOR_FACTOR) * sizeof(double));
  for (i = 0; i < DECIMATOR_FACTOR; i += 1) {
    x[DECIMATOR_FACTOR - 1 - i] = samples[i];
  }
  sum += -0.0000046183113992051936 * (x[1] + x[191]);
  sum += -0.00001117761640887225 * (x[2] + x[190]);
  sum += -0.000018610264502005432 * (x[3] + x[189]);
  sum += -0.000025134586135631012 * (x[4] + x[188]);
  sum += -0.000028494281690666197 * (x[5] + x[187]);
  sum += -0.000026396828793275159 * (x[6] + x[186]);
  sum += -0.000017094212558802156 * (x[7] + x[185]);
  sum += 0.000023798193576966866 * (x[9] + x[183]);
  sum += 0.000051281160242202183 * (x[10] + x[182]);
  sum += 0.00007762197826243427 * (x[11] + x[181]);
  sum += 0.000096759426664120416 * (x[12] + x[180]);
  sum += 0.00010240229300393402 * (x[13] + x[179]);
  sum += 0.000089344614218077106 * (x[14] + x[178]);
  sum += 0.000054875700118949183 * (x[15] + x[177]);
  sum += -0.000069839082210680165 * (x[17] + x[175]);
  sum += -0.0001447966132360757 * (x[18] + x[174]);
  sum += -0.00021158452917708308 * (x[19] + x[173]);
  sum += -0.00025535069106550544 * (x[20] + x[172]);
  sum += -0.00026228714374322104 * (x[21] + x[171]);
  sum += -0.00022258805927027799 * (x[22] + x[170]);
  sum += -0.00013323230495695704 * (x[23] + x[169]);
  sum += 0.00016182578767055206 * (x[25] + x[167]);
  sum += 0.00032846175385096581 * (x[26] + x[166]);
  sum += 0.00047045611576184863 * (x[27] + x[165]);
  sum += 0.00055713851457530944 * (x[28] + x[164]);
  sum += 0.00056212565121518726 * (x[29] + x[163]);
  sum += 0.00046901918553962478 * (x[30] + x[162]);
  sum += 0.00027624866838952986 * (x[31] + x[161]);
  sum += -0.00032564179486838622 * (x[33] + x[159]);
  sum += -0.00065182310286710388 * (x[34] + x[158]);
  sum += -0.00092127787309319298 * (x[35] + x[157]);
  sum += -0.0010772534348943575 * (x[36] + x[156]);
  sum += -0.0010737727700273478 * (x[37] + x[155]);
  sum += -0.00088556645390392634 * (x[38] + x[154]);
  sum += -0.00051581896090765534 * (x[39] + x[153]);
  sum += 0.00059548767193795277 * (x[41] + x[151]);
  sum += 0.0011803558710661009 * (x[42] + x[150]);
  sum += 0.0016527320270369871 * (x[43] + x[149]);
  sum += 0.0019152679330965555 * (x[44] + x[148]);
  sum += 0.0018927324805381538 * (x[45] + x[147]);
  sum += 0.0015481870327877937 * (x[46] + x[146]);
  sum += 0.00089470695834941306 * (x[47] + x[145]);
  sum += -0.0010178225878206125 * (x[49] + x[143]);
  sum += -0.0020037400552054292 * (x[50] + x[142]);
  sum += -0.0027874356824117317 * (x[51] + x[141]);
  sum += -0.003210329988021943 * (x[52] + x[140]);
  sum += -0.0031540624117984395 * (x[53] + x[139]);
  sum += -0.0025657163651900345 * (x[54] + x[138]);
  sum += -0.0014750752642111449 * (x[55] + x[137]);
  sum += 0.0016624165446378462 * (x[57] + x[135]);
  sum += 0.0032591192839069179 * (x[58] + x[134]);
  sum += 0.0045165685815867747 * (x[59] + x[133]);
  sum += 0.0051838984346123896 * (x[60] + x[132]);
  sum += 0.0050774264697459933 * (x[61] + x[131]);
  sum += 0.0041192521414141585 * (x[62] + x[130]);
  sum += 0.0023628575417966491 * (x[63] + x[129]);
  sum += -0.0026543507866759182 * (x[65] + x[127]);
  sum += -0.0051990251084333425 * (x[66] + x[126]);
  sum += -0.0072020238234656924 * (x[67] + x[125]);
  sum += -0.0082672928192007358 * (x[68] + x[124]);
  sum += -0.0081033739572956287 * (x[69] + x[123]);
  sum += -0.006583111539570221 * (x[70] + x[122]);
  sum += -0.0037839040415292386 * (x[71] + x[121]);
  sum += 0.0042781252851152507 * (x[73] + x[119]);
  sum += 0.0084176358598320178 * (x[74] + x[118]);
  sum += 0.01172566057463055 * (x[75] + x[117]);
  sum += 0.013550476647788672 * (x[76] + x[116]);
  sum += 0.013388189369997496 * (x[77] + x[115]);
  sum += 0.010979501242341259 * (x[78] + x[114]);
  sum += 0.006381274941685413 * (x[79] + x[113]);
  sum += -0.007421229604153888 * (x[81] + x[111]);
  sum += -0.01486456304340213 * (x[82] + x[110]);
  sum += -0.021143584622178104 * (x[83] + x[109]);
  sum += -0.02504275058758609 * (x[84] + x[108]);
  sum += -0.025473530942547201 * (x[85] + x[107]);
  sum += -0.021627310017882196 * (x[86] + x[106]);
  sum += -0.013104323383225543 * (x[87] + x[105]);
  sum += 0.017065133989980476 * (x[89] + x[103]);
  sum += 0.036978919264451952 * (x[90] + x[102]);
  sum += 0.05823318062093958 * (x[91] + x[101]);
  sum += 0.079072012081405949 * (x[92] + x[100]);
  sum += 0.097675998716952317 * (x[93] + x[99]);
  sum += 0.11236045936950932 * (x[94] + x[98]);
  sum += 0.12176343577287731 * (x[95] + x[97]);
  sum += 0.125 * x[96];
  return sum;
}

void ayumi_process(struct ayumi* ay) {
  int i;
  double c0;
  double c1;
  double c2;
  double y1;
  double left_samples[DECIMATOR_FACTOR];
  double right_samples[DECIMATOR_FACTOR];
  for (i = 0; i < DECIMATOR_FACTOR; i += 1) {
    ay->point += ay->step;
    if (ay->point >= 1) {
      ay->point -= 1;
      ay->y_left[0] = ay->y_left[1];
      ay->y_left[1] = ay->y_left[2];
      ay->y_left[2] = ay->y_left[3];
      ay->y_right[0] = ay->y_right[1];
      ay->y_right[1] = ay->y_right[2];
      ay->y_right[2] = ay->y_right[3];
      update_mixer(ay);
      ay->y_left[3] = ay->left;
      ay->y_right[3] = ay->right;
    }
    y1 = ay->y_left[2] - ay->y_left[0];
    c0 = 1 / 2. * ay->y_left[1] + 1 / 4. * (ay->y_left[0] + ay->y_left[2]);
    c1 = 1 / 2. * y1;
    c2 = 1 / 4. * (ay->y_left[3] - ay->y_left[1] - y1);
    left_samples[i] = (c2 * ay->point + c1) * ay->point + c0;
    y1 = ay->y_right[2] - ay->y_right[0];
    c0 = 1 / 2. * ay->y_right[1] + 1 / 4. * (ay->y_right[0] + ay->y_right[2]);
    c1 = 1 / 2. * y1;
    c2 = 1 / 4. * (ay->y_right[3] - ay->y_right[1] - y1);
    right_samples[i] = (c2 * ay->point + c1) * ay->point + c0;
  }
  ay->left = decimator(ay->decimator_left, left_samples);
  ay->right = decimator(ay->decimator_right, right_samples);
}

static double dc_filter(struct dc_filter* dc, int index, double x) {
  dc->sum += -dc->delay[index] + x;
  dc->delay[index] = x; 
  return x - dc->sum / DC_FILTER_SIZE;
}

void ayumi_remove_dc(struct ayumi* ay) {
  ay->left = dc_filter(&ay->dc_left, ay->dc_index, ay->left);
  ay->right = dc_filter(&ay->dc_right, ay->dc_index, ay->right);
  ay->dc_index = (ay->dc_index + 1) & (DC_FILTER_SIZE - 1);
}
