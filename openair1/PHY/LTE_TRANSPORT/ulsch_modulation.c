/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.0  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file PHY/LTE_TRANSPORT/ulsch_modulation.c
* \brief Top-level routines for generating PUSCH physical channel from 36.211 V8.6 2009-03
* \author R. Knopp, F. Kaltenberger, A. Bhamri
* \date 2011
* \version 0.1
* \company Eurecom
* \email: knopp@eurecom.fr,florian.kaltenberger@eurecom.fr,ankit.bhamri@eurecom.fr
* \note
* \warning
*/
#include "PHY/defs.h"
#include "PHY/extern.h"
#include "PHY/CODING/defs.h"
#include "PHY/CODING/extern.h"
#include "PHY/LTE_TRANSPORT/defs.h"
#include "defs.h"
#include "UTIL/LOG/vcd_signal_dumper.h"



//#define DEBUG_ULSCH_MODULATION

#ifndef OFDMA_ULSCH
void dft_lte(int32_t *z,int32_t *d, int32_t Msc_PUSCH, uint8_t Nsymb)
{

#if defined(__x86_64__) || defined(__i386__)
  __m128i dft_in128[4][1200],dft_out128[4][1200];
#elif defined(__arm__)
  int16x8_t dft_in128[4][1200],dft_out128[4][1200];
#endif
  uint32_t *dft_in0=(uint32_t*)dft_in128[0],*dft_out0=(uint32_t*)dft_out128[0];
  uint32_t *dft_in1=(uint32_t*)dft_in128[1],*dft_out1=(uint32_t*)dft_out128[1];
  uint32_t *dft_in2=(uint32_t*)dft_in128[2],*dft_out2=(uint32_t*)dft_out128[2];
  //  uint32_t *dft_in3=(uint32_t*)dft_in128[3],*dft_out3=(uint32_t*)dft_out128[3];

  uint32_t *d0,*d1,*d2,*d3,*d4,*d5,*d6,*d7,*d8,*d9,*d10,*d11;

  uint32_t *z0,*z1,*z2,*z3,*z4,*z5,*z6,*z7,*z8,*z9,*z10,*z11;
  uint32_t i,ip;
#if defined(__x86_64__) || defined(__i386__)
  __m128i norm128;
#elif defined(__arm__)
  int16x8_t norm128;
#endif
  //  printf("Doing lte_dft for Msc_PUSCH %d\n",Msc_PUSCH);

  d0 = (uint32_t *)d;
  d1 = d0+Msc_PUSCH;
  d2 = d1+Msc_PUSCH;
  d3 = d2+Msc_PUSCH;
  d4 = d3+Msc_PUSCH;
  d5 = d4+Msc_PUSCH;
  d6 = d5+Msc_PUSCH;
  d7 = d6+Msc_PUSCH;
  d8 = d7+Msc_PUSCH;
  d9 = d8+Msc_PUSCH;
  d10 = d9+Msc_PUSCH;
  d11 = d10+Msc_PUSCH;

  //  printf("symbol 0 (d0 %p, d %p)\n",d0,d);
  for (i=0,ip=0; i<Msc_PUSCH; i++,ip+=4) {
    dft_in0[ip]   =  d0[i];
    dft_in0[ip+1] =  d1[i];
    dft_in0[ip+2] =  d2[i];
    dft_in0[ip+3] =  d3[i];
    dft_in1[ip]   =  d4[i];
    dft_in1[ip+1] =  d5[i];
    dft_in1[ip+2] =  d6[i];
    dft_in1[ip+3] =  d7[i];
    dft_in2[ip]   =  d8[i];
    dft_in2[ip+1] =  d9[i];
    dft_in2[ip+2] =  d10[i];
    dft_in2[ip+3] =  d11[i];
    //    printf("dft%d %d: %d,%d,%d,%d\n",Msc_PUSCH,ip,d0[i],d1[i],d2[i],d3[i]);

    //    dft_in_re2[ip+1] =  d9[i];
    //    dft_in_re2[ip+2] =  d10[i];
  }

  //  printf("\n");

  switch (Msc_PUSCH) {
  case 12:
    dft12((int16_t *)dft_in0,(int16_t *)dft_out0);
    dft12((int16_t *)dft_in1,(int16_t *)dft_out1);
    dft12((int16_t *)dft_in2,(int16_t *)dft_out2);

    /*
    dft12f(&((__m128i *)dft_in0)[0],&((__m128i *)dft_in0)[1],&((__m128i *)dft_in0)[2],&((__m128i *)dft_in0)[3],&((__m128i *)dft_in0)[4],&((__m128i *)dft_in0)[5],&((__m128i *)dft_in0)[6],&((__m128i *)dft_in0)[7],&((__m128i *)dft_in0)[8],&((__m128i *)dft_in0)[9],&((__m128i *)dft_in0)[10],&((__m128i *)dft_in0)[11],
    &((__m128i *)dft_out0)[0],&((__m128i *)dft_out0)[1],&((__m128i *)dft_out0)[2],&((__m128i *)dft_out0)[3],&((__m128i *)dft_out0)[4],&((__m128i *)dft_out0)[5],&((__m128i *)dft_out0)[6],&((__m128i *)dft_out0)[7],&((__m128i *)dft_out0)[8],&((__m128i *)dft_out0)[9],&((__m128i *)dft_out0)[10],&((__m128i *)dft_out0)[11]);

    dft12f(&((__m128i *)dft_in1)[0],&((__m128i *)dft_in1)[1],&((__m128i *)dft_in1)[2],&((__m128i *)dft_in1)[3],&((__m128i *)dft_in1)[4],&((__m128i *)dft_in1)[5],&((__m128i *)dft_in1)[6],&((__m128i *)dft_in1)[7],&((__m128i *)dft_in1)[8],&((__m128i *)dft_in1)[9],&((__m128i *)dft_in1)[10],&((__m128i *)dft_in1)[11],
    &((__m128i *)dft_out1)[0],&((__m128i *)dft_out1)[1],&((__m128i *)dft_out1)[2],&((__m128i *)dft_out1)[3],&((__m128i *)dft_out1)[4],&((__m128i *)dft_out1)[5],&((__m128i *)dft_out1)[6],&((__m128i *)dft_out1)[7],&((__m128i *)dft_out1)[8],&((__m128i *)dft_out1)[9],&((__m128i *)dft_out1)[10],&((__m128i *)dft_out1)[11]);

    dft12f(&((__m128i *)dft_in2)[0],&((__m128i *)dft_in2)[1],&((__m128i *)dft_in2)[2],&((__m128i *)dft_in2)[3],&((__m128i *)dft_in2)[4],&((__m128i *)dft_in2)[5],&((__m128i *)dft_in2)[6],&((__m128i *)dft_in2)[7],&((__m128i *)dft_in2)[8],&((__m128i *)dft_in2)[9],&((__m128i *)dft_in2)[10],&((__m128i *)dft_in2)[11],
    &((__m128i *)dft_out2)[0],&((__m128i *)dft_out2)[1],&((__m128i *)dft_out2)[2],&((__m128i *)dft_out2)[3],&((__m128i *)dft_out2)[4],&((__m128i *)dft_out2)[5],&((__m128i *)dft_out2)[6],&((__m128i *)dft_out2)[7],&((__m128i *)dft_out2)[8],&((__m128i *)dft_out2)[9],&((__m128i *)dft_out2)[10],&((__m128i *)dft_out2)[11]);
    */
#if defined(__x86_64__) || defined(__i386__)
    norm128 = _mm_set1_epi16(9459);
#elif defined(__arm__)
    norm128 = vdupq_n_s16(9459);
#endif
    for (i=0; i<12; i++) {
#if defined(__x86_64__) || defined(__i386__)
      ((__m128i*)dft_out0)[i] = _mm_slli_epi16(_mm_mulhi_epi16(((__m128i*)dft_out0)[i],norm128),1);
      ((__m128i*)dft_out1)[i] = _mm_slli_epi16(_mm_mulhi_epi16(((__m128i*)dft_out1)[i],norm128),1);
      ((__m128i*)dft_out2)[i] = _mm_slli_epi16(_mm_mulhi_epi16(((__m128i*)dft_out2)[i],norm128),1);
#elif defined(__arm__)
      ((int16x8_t*)dft_out0)[i] = vqdmulhq_s16(((int16x8_t*)dft_out0)[i],norm128);
      ((int16x8_t*)dft_out1)[i] = vqdmulhq_s16(((int16x8_t*)dft_out1)[i],norm128);
      ((int16x8_t*)dft_out2)[i] = vqdmulhq_s16(((int16x8_t*)dft_out2)[i],norm128);
#endif
    }

    break;

  case 24:
    dft24((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft24((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft24((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 36:
    dft36((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft36((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft36((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 48:
    dft48((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft48((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft48((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 60:
    dft60((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft60((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft60((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 72:
    dft72((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft72((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft72((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 96:
    dft96((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft96((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft96((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 108:
    dft108((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft108((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft108((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 120:
    dft120((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft120((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft120((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 144:
    dft144((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft144((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft144((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 180:
    dft180((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft180((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft180((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 192:
    dft192((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft192((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft192((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 216:
    dft216((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft216((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft216((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 240:
    dft240((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft240((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft240((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 288:
    dft288((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft288((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft288((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 300:
    dft300((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft300((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft300((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 324:
    dft324((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft324((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft324((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 360:
    dft360((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft360((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft360((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 384:
    dft384((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft384((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft384((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 432:
    dft432((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft432((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft432((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 480:
    dft480((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft480((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft480((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 540:
    dft540((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft540((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft540((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 576:
    dft576((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft576((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft576((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 600:
    dft600((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft600((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft600((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 648:
    dft648((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft648((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft648((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 720:
    dft720((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft720((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft720((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 864:
    dft864((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft864((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft864((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 900:
    dft900((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft900((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft900((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 960:
    dft960((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft960((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft960((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 972:
    dft972((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft972((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft972((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 1080:
    dft1080((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft1080((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft1080((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 1152:
    dft1152((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft1152((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft1152((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;

  case 1200:
    dft1200((int16_t*)dft_in0,(int16_t*)dft_out0,1);
    dft1200((int16_t*)dft_in1,(int16_t*)dft_out1,1);
    dft1200((int16_t*)dft_in2,(int16_t*)dft_out2,1);
    break;
  }

  z0 = (uint32_t *)z;
  z1 = z0+Msc_PUSCH;
  z2 = z1+Msc_PUSCH;
  z3 = z2+Msc_PUSCH;
  z4 = z3+Msc_PUSCH;
  z5 = z4+Msc_PUSCH;
  z6 = z5+Msc_PUSCH;
  z7 = z6+Msc_PUSCH;
  z8 = z7+Msc_PUSCH;
  z9 = z8+Msc_PUSCH;
  z10 = z9+Msc_PUSCH;
  z11 = z10+Msc_PUSCH;

  //  printf("symbol0 (dft)\n");
  for (i=0,ip=0; i<Msc_PUSCH; i++,ip+=4) {
    z0[i]     = dft_out0[ip];
    //    printf("%d,%d,",((short*)&z0[i])[0],((short*)&z0[i])[1]);
    z1[i]     = dft_out0[ip+1];
    z2[i]     = dft_out0[ip+2];
    z3[i]     = dft_out0[ip+3];
    z4[i]     = dft_out1[ip+0];
    z5[i]     = dft_out1[ip+1];
    z6[i]     = dft_out1[ip+2];
    z7[i]     = dft_out1[ip+3];
    z8[i]     = dft_out2[ip];
    z9[i]     = dft_out2[ip+1];
    z10[i]    = dft_out2[ip+2];
    z11[i]    = dft_out2[ip+3];
    //    printf("out dft%d %d: %d,%d,%d,%d,%d,%d,%d,%d\n",Msc_PUSCH,ip,z0[i],z1[i],z2[i],z3[i],z4[i],z5[i],z6[i],z7[i]);

  }

  //  printf("\n");
}

#endif
void ulsch_modulation(int32_t **txdataF,
                      short amp,
                      uint32_t frame,
                      uint32_t subframe,
                      LTE_DL_FRAME_PARMS *frame_parms,
                      LTE_UE_ULSCH_t *ulsch)
{

  uint8_t qam64_table_offset_re = 0;
  uint8_t qam64_table_offset_im = 0;
  uint8_t qam16_table_offset_re = 0;
  uint8_t qam16_table_offset_im = 0;
  short gain_lin_QPSK;

  DevAssert(frame_parms);

  int re_offset,re_offset0,i,Msymb,j,k,nsymb,Msc_PUSCH,l;
  //  uint8_t harq_pid = (rag_flag == 1) ? 0 : subframe2harq_pid_tdd(frame_parms->tdd_config,subframe);
  uint8_t harq_pid = subframe2harq_pid(frame_parms,frame,subframe);
  uint8_t Q_m;
  int32_t *txptr;
  uint32_t symbol_offset;
  uint16_t first_rb;
  uint16_t nb_rb;
  int G;

  uint32_t x1, x2, s=0;
  uint8_t c;

  if (!ulsch) {
    printf("ulsch_modulation.c: Null ulsch\n");
    return;
  }

  // x1 is set in lte_gold_generic
  x2 = (ulsch->rnti<<14) + (subframe<<9) + frame_parms->Nid_cell; //this is c_init in 36.211 Sec 6.3.1

  if (harq_pid > 7) {
    printf("ulsch_modulation.c: Illegal harq_pid %d\n",harq_pid);
    return;
  }

  first_rb = ulsch->harq_processes[harq_pid]->first_rb;
  nb_rb = ulsch->harq_processes[harq_pid]->nb_rb;

  if (nb_rb == 0) {
    printf("ulsch_modulation.c: Frame %d, Subframe %d Illegal nb_rb %d\n",frame,subframe,nb_rb);
    return;
  }

  if (first_rb >25 ) {
    printf("ulsch_modulation.c: Frame %d, Subframe %d Illegal first_rb %d\n",frame,subframe,first_rb);
    return;
  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_ULSCH_MODULATION, VCD_FUNCTION_IN);

  Q_m = get_Qm_ul(ulsch->harq_processes[harq_pid]->mcs);

  G = (int)ulsch->harq_processes[harq_pid]->nb_rb * (12 * Q_m) * (ulsch->Nsymb_pusch);

  // Mapping
  nsymb = (frame_parms->Ncp==0) ? 14:12;
  Msc_PUSCH = ulsch->harq_processes[harq_pid]->nb_rb*12;

#ifdef DEBUG_ULSCH_MODULATION
  printf("ulsch_modulation.c: Doing modulation (rnti %x,x2 %x) for G=%d bits, harq_pid %d , nb_rb %d, Q_m %d, Nsymb_pusch %d (nsymb %d), subframe %d\n",
      ulsch->rnti,x2,G,harq_pid,ulsch->harq_processes[harq_pid]->nb_rb,Q_m, ulsch->Nsymb_pusch,nsymb,subframe);
#endif

  // scrambling (Note the placeholding bits are handled in ulsch_coding.c directly!)
  //printf("ulsch bits: ");
  s = lte_gold_generic(&x1, &x2, 1);
  k=0;

  //printf("G %d\n",G);
  for (i=0; i<(1+(G>>5)); i++) {
    for (j=0; j<32; j++,k++) {
      c = (uint8_t)((s>>j)&1);

      if (ulsch->h[k] == PUSCH_x) {
        //  printf("i %d: PUSCH_x\n",i);
        ulsch->b_tilde[k] = 1;
      } else if (ulsch->h[k] == PUSCH_y) {
        //  printf("i %d: PUSCH_y\n",i);
        ulsch->b_tilde[k] = ulsch->b_tilde[k-1];
      } else {
        ulsch->b_tilde[k] = (ulsch->h[k]+c)&1;
        //  printf("i %d : %d (h %d c %d)\n", (i<<5)+j,ulsch->b_tilde[k],ulsch->h[k],c);
      }

    }

    s = lte_gold_generic(&x1, &x2, 0);
  }

  //printf("\n");


  gain_lin_QPSK = (short)((amp*ONE_OVER_SQRT2_Q15)>>15);


  // Modulation

  Msymb = G/Q_m;

  if(ulsch->cooperation_flag == 2)
    // For Distributed Alamouti Scheme in Collabrative Communication
  {
    for (i=0,j=Q_m; i<Msymb; i+=2,j+=2*Q_m) {

      switch (Q_m) {

      case 2:


        //UE1, -x1*
        ((int16_t*)&ulsch->d[i])[0] = (ulsch->b_tilde[j] == 1)  ? (gain_lin_QPSK) : -gain_lin_QPSK;
        ((int16_t*)&ulsch->d[i])[1] = (ulsch->b_tilde[j+1] == 1)? (-gain_lin_QPSK) : gain_lin_QPSK;
        //      if (i<Msc_PUSCH)
        //  printf("input %d (%p): %d,%d\n", i,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);

        // UE1, x0*
        ((int16_t*)&ulsch->d[i+1])[0] = (ulsch->b_tilde[j-2] == 1)  ? (-gain_lin_QPSK) : gain_lin_QPSK;
        ((int16_t*)&ulsch->d[i+1])[1] = (ulsch->b_tilde[j-1] == 1)? (gain_lin_QPSK) : -gain_lin_QPSK;

        break;

      case 4:


        //UE1,-x1*
        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j+1] == 1)
          qam16_table_offset_im+=2;



        if (ulsch->b_tilde[j+2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j+3] == 1)
          qam16_table_offset_im+=1;


        ((int16_t*)&ulsch->d[i])[0]=-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i])[1]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);

        //UE1,x0*
        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j-4] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j-3] == 1)
          qam16_table_offset_im+=2;


        if (ulsch->b_tilde[j-2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j-1] == 1)
          qam16_table_offset_im+=1;


        //    ((int16_t*)&ulsch->d[i+1])[0]=-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        //    ((int16_t*)&ulsch->d[i+1])[1]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);
        ((int16_t*)&ulsch->d[i+1])[0]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i+1])[1]=-(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);


        break;

      case 6:



        //UE1,-x1*FPGA_UE
        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j+1] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j+2] == 1)
          qam64_table_offset_re+=2;


        if (ulsch->b_tilde[j+3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j+4] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j+5] == 1)
          qam64_table_offset_im+=1;


        ((int16_t*)&ulsch->d[i])[0]=-(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i])[1]=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        //UE1,x0*
        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j-6] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j-5] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j-4] == 1)
          qam64_table_offset_re+=2;


        if (ulsch->b_tilde[j-3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j-2] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j-1] == 1)
          qam64_table_offset_im+=1;


        ((int16_t*)&ulsch->d[i+1])[0]=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i+1])[1]=-(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        break;

      }//switch
    }//for
  }//cooperation_flag == 2
  else {
    for (i=0,j=0; i<Msymb; i++,j+=Q_m) {

      switch (Q_m) {

      case 2:
        // TODO: this has to be updated!!!

        ((int16_t*)&ulsch->d[i])[0] = (ulsch->b_tilde[j] == 1)  ? (-gain_lin_QPSK) : gain_lin_QPSK;
        ((int16_t*)&ulsch->d[i])[1] = (ulsch->b_tilde[j+1] == 1)? (-gain_lin_QPSK) : gain_lin_QPSK;
        //        if (i<Msc_PUSCH)
        //    printf("input %d/%d Msc_PUSCH %d (%p): %d,%d\n", i,Msymb,Msc_PUSCH,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);

        break;

      case 4:

        qam16_table_offset_re = 0;
        qam16_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam16_table_offset_re+=2;

        if (ulsch->b_tilde[j+1] == 1)
          qam16_table_offset_im+=2;

        if (ulsch->b_tilde[j+2] == 1)
          qam16_table_offset_re+=1;

        if (ulsch->b_tilde[j+3] == 1)
          qam16_table_offset_im+=1;


        ((int16_t*)&ulsch->d[i])[0]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i])[1]=(int16_t)(((int32_t)amp*qam16_table[qam16_table_offset_im])>>15);
        //      printf("input(16qam) %d (%p): %d,%d\n", i,&ulsch->d[i],((int16_t*)&ulsch->d[i])[0],((int16_t*)&ulsch->d[i])[1]);
        break;

      case 6:


        qam64_table_offset_re = 0;
        qam64_table_offset_im = 0;

        if (ulsch->b_tilde[j] == 1)
          qam64_table_offset_re+=4;

        if (ulsch->b_tilde[j+1] == 1)
          qam64_table_offset_im+=4;

        if (ulsch->b_tilde[j+2] == 1)
          qam64_table_offset_re+=2;

        if (ulsch->b_tilde[j+3] == 1)
          qam64_table_offset_im+=2;

        if (ulsch->b_tilde[j+4] == 1)
          qam64_table_offset_re+=1;

        if (ulsch->b_tilde[j+5] == 1)
          qam64_table_offset_im+=1;


        ((int16_t*)&ulsch->d[i])[0]=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_re])>>15);
        ((int16_t*)&ulsch->d[i])[1]=(int16_t)(((int32_t)amp*qam64_table[qam64_table_offset_im])>>15);

        break;

      }
    }
  }// normal symbols


  // Transform Precoding

#ifdef OFDMA_ULSCH

  for (i=0; i<Msymb; i++) {
    ulsch->z[i] = ulsch->d[i];
  }

#else
  dft_lte(ulsch->z,ulsch->d,Msc_PUSCH,ulsch->Nsymb_pusch);
#endif

  DevAssert(txdataF);

#ifdef OFDMA_ULSCH
  re_offset0 = frame_parms->first_carrier_offset + (ulsch->harq_processes[harq_pid]->first_rb*12);

  if (re_offset0>frame_parms->ofdm_symbol_size) {
    re_offset0 -= frame_parms->ofdm_symbol_size;
    //    re_offset0++;
  }

  //  printf("re_offset0 %d\n",re_offset0);


  for (j=0,l=0; l<(nsymb-ulsch->srs_active); l++) {
    re_offset = re_offset0;
    symbol_offset = (int)frame_parms->ofdm_symbol_size*(l+(subframe*nsymb));
#ifdef DEBUG_ULSCH_MODULATION
    printf("symbol %d (subframe %d): symbol_offset %d\n",l,subframe,symbol_offset);
#endif
    txptr = &txdataF[0][symbol_offset];

    if (((frame_parms->Ncp == 0) && ((l==3) || (l==10)))||
        ((frame_parms->Ncp == 1) && ((l==2) || (l==8)))) {
    }
    // Skip reference symbols
    else {

      //      printf("copying %d REs\n",Msc_PUSCH);
      for (i=0; i<Msc_PUSCH; i++,j++) {
#ifdef DEBUG_ULSCH_MODULATION
        printf("re_offset %d (%p): %d,%d\n", re_offset,&ulsch->z[j],((int16_t*)&ulsch->z[j])[0],((int16_t*)&ulsch->z[j])[1]);
#endif
        txptr[re_offset++] = ulsch->z[j];

        if (re_offset==frame_parms->ofdm_symbol_size)
          re_offset = 0;
      }
    }
  }

# else  // OFDMA_ULSCH = 0
  re_offset0 = frame_parms->first_carrier_offset + (ulsch->harq_processes[harq_pid]->first_rb*12);

  if (re_offset0>frame_parms->ofdm_symbol_size) {
    re_offset0 -= frame_parms->ofdm_symbol_size;
    //    re_offset0++;
  }

  //    printf("re_offset0 %d\n",re_offset0);
  //  printf("txdataF %p\n",&txdataF[0][0]);
  for (j=0,l=0; l<(nsymb-ulsch->srs_active); l++) {
    re_offset = re_offset0;
    symbol_offset = (uint32_t)frame_parms->ofdm_symbol_size*(l+(subframe*nsymb));
#ifdef DEBUG_ULSCH_MODULATION
    printf("ulsch_mod (SC-FDMA) symbol %d (subframe %d): symbol_offset %d\n",l,subframe,symbol_offset);
#endif
    txptr = &txdataF[0][symbol_offset];

    if (((frame_parms->Ncp == 0) && ((l==3) || (l==10)))||
        ((frame_parms->Ncp == 1) && ((l==2) || (l==8)))) {
    }
    // Skip reference symbols
    else {
      //      printf("copying %d REs\n",Msc_PUSCH);
      for (i=0; i<Msc_PUSCH; i++,j++) {

#ifdef DEBUG_ULSCH_MODULATION
        printf("re_offset %d (%p): %d,%d => %p\n", re_offset,&ulsch->z[j],((int16_t*)&ulsch->z[j])[0],((int16_t*)&ulsch->z[j])[1],&txptr[re_offset]);
#endif //DEBUG_ULSCH_MODULATION
        txptr[re_offset++] = ulsch->z[j];

        if (re_offset==frame_parms->ofdm_symbol_size)
          re_offset = 0;
      }
    }
  }

#endif
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_ULSCH_MODULATION, VCD_FUNCTION_OUT);

}

