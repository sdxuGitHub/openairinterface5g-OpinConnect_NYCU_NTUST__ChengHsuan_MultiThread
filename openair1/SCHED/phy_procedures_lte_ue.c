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

/*! \file phy_procedures_lte_ue.c
 * \brief Implementation of UE procedures from 36.213 LTE specifications
 * \author R. Knopp, F. Kaltenberger, N. Nikaein
 * \date 2011
 * \version 0.1
 * \company Eurecom
 * \email: knopp@eurecom.fr,florian.kaltenberger@eurecom.fr, navid.nikaein@eurecom.fr
 * \note
 * \warning
 */

#include "assertions.h"
#include "defs.h"
#include "PHY/defs.h"
#include "PHY/extern.h"
#include "SCHED/defs.h"
#include "SCHED/extern.h"

#ifdef EMOS
#include "SCHED/phy_procedures_emos.h"
#endif

#define DEBUG_PHY_PROC

#ifndef PUCCH
#define PUCCH
#endif

#include "LAYER2/MAC/extern.h"
#include "LAYER2/MAC/defs.h"
#include "UTIL/LOG/log.h"

#ifdef EMOS
fifo_dump_emos_UE emos_dump_UE;
#endif

#include "UTIL/LOG/vcd_signal_dumper.h"

#if defined(ENABLE_ITTI)
# include "intertask_interface.h"
#endif


#define DLSCH_RB_ALLOC 0x1fbf  // skip DC RB (total 23/25 RBs)
#define DLSCH_RB_ALLOC_12 0x0aaa  // skip DC RB (total 23/25 RBs)

#define NS_PER_SLOT 500000

extern int oai_exit;




#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
extern uint32_t downlink_frequency[MAX_NUM_CCs][4];
#endif



void dump_dlsch(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t subframe,uint8_t harq_pid)
{
  unsigned int coded_bits_per_codeword;
  uint8_t nsymb = (ue->frame_parms.Ncp == 0) ? 14 : 12;

  coded_bits_per_codeword = get_G(&ue->frame_parms,
                                  ue->dlsch[eNB_id][0]->harq_processes[harq_pid]->nb_rb,
                                  ue->dlsch[eNB_id][0]->harq_processes[harq_pid]->rb_alloc_even,
                                  ue->dlsch[eNB_id][0]->harq_processes[harq_pid]->Qm,
                                  ue->dlsch[eNB_id][0]->harq_processes[harq_pid]->Nl,
                                  ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
                                  proc->frame_rx,subframe);

  write_output("rxsigF0.m","rxsF0", ue->common_vars.rxdataF[0],2*nsymb*ue->frame_parms.ofdm_symbol_size,2,1);
  write_output("rxsigF0_ext.m","rxsF0_ext", ue->pdsch_vars[0]->rxdataF_ext[0],2*nsymb*ue->frame_parms.ofdm_symbol_size,1,1);
  write_output("dlsch00_ch0_ext.m","dl00_ch0_ext", ue->pdsch_vars[0]->dl_ch_estimates_ext[0],300*nsymb,1,1);
  /*
    write_output("dlsch01_ch0_ext.m","dl01_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[1],300*12,1,1);
    write_output("dlsch10_ch0_ext.m","dl10_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[2],300*12,1,1);
    write_output("dlsch11_ch0_ext.m","dl11_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[3],300*12,1,1);
    write_output("dlsch_rho.m","dl_rho",pdsch_vars[0]->rho[0],300*12,1,1);
  */
  write_output("dlsch_rxF_comp0.m","dlsch0_rxF_comp0", ue->pdsch_vars[0]->rxdataF_comp0[0],300*12,1,1);
  write_output("dlsch_rxF_llr.m","dlsch_llr", ue->pdsch_vars[0]->llr[0],coded_bits_per_codeword,1,0);

  write_output("dlsch_mag1.m","dlschmag1",ue->pdsch_vars[0]->dl_ch_mag0,300*12,1,1);
  write_output("dlsch_mag2.m","dlschmag2",ue->pdsch_vars[0]->dl_ch_magb0,300*12,1,1);
}

void dump_dlsch_SI(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t subframe)
{
  unsigned int coded_bits_per_codeword;
  uint8_t nsymb = ((ue->frame_parms.Ncp == 0) ? 14 : 12);

  coded_bits_per_codeword = get_G(&ue->frame_parms,
                                  ue->dlsch_SI[eNB_id]->harq_processes[0]->nb_rb,
                                  ue->dlsch_SI[eNB_id]->harq_processes[0]->rb_alloc_even,
                                  2,
                                  1,
                                  ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
                                  proc->frame_rx,subframe);
  LOG_D(PHY,"[UE %d] Dumping dlsch_SI : ofdm_symbol_size %d, nsymb %d, nb_rb %d, mcs %d, nb_rb %d, num_pdcch_symbols %d,G %d\n",
        ue->Mod_id,
	ue->frame_parms.ofdm_symbol_size,
	nsymb,
        ue->dlsch_SI[eNB_id]->harq_processes[0]->nb_rb,
        ue->dlsch_SI[eNB_id]->harq_processes[0]->mcs,
        ue->dlsch_SI[eNB_id]->harq_processes[0]->nb_rb,
        ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
        coded_bits_per_codeword);

  write_output("rxsig0.m","rxs0", &ue->common_vars.rxdata[0][subframe*ue->frame_parms.samples_per_tti],ue->frame_parms.samples_per_tti,1,1);

  write_output("rxsigF0.m","rxsF0", ue->common_vars.rxdataF[0],nsymb*ue->frame_parms.ofdm_symbol_size,1,1);
  write_output("rxsigF0_ext.m","rxsF0_ext", ue->pdsch_vars_SI[0]->rxdataF_ext[0],2*nsymb*ue->frame_parms.ofdm_symbol_size,1,1);
  write_output("dlsch00_ch0_ext.m","dl00_ch0_ext", ue->pdsch_vars_SI[0]->dl_ch_estimates_ext[0],ue->frame_parms.N_RB_DL*12*nsymb,1,1);
  /*
    write_output("dlsch01_ch0_ext.m","dl01_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[1],300*12,1,1);
    write_output("dlsch10_ch0_ext.m","dl10_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[2],300*12,1,1);
    write_output("dlsch11_ch0_ext.m","dl11_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[3],300*12,1,1);
    write_output("dlsch_rho.m","dl_rho",pdsch_vars[0]->rho[0],300*12,1,1);
  */
  write_output("dlsch_rxF_comp0.m","dlsch0_rxF_comp0", ue->pdsch_vars_SI[0]->rxdataF_comp0[0],ue->frame_parms.N_RB_DL*12*nsymb,1,1);
  write_output("dlsch_rxF_llr.m","dlsch_llr", ue->pdsch_vars_SI[0]->llr[0],coded_bits_per_codeword,1,0);

  write_output("dlsch_mag1.m","dlschmag1",ue->pdsch_vars_SI[0]->dl_ch_mag0,300*nsymb,1,1);
  write_output("dlsch_mag2.m","dlschmag2",ue->pdsch_vars_SI[0]->dl_ch_magb0,300*nsymb,1,1);
  sleep(1);
  exit(-1);
}

#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
//unsigned int gain_table[31] = {100,112,126,141,158,178,200,224,251,282,316,359,398,447,501,562,631,708,794,891,1000,1122,1258,1412,1585,1778,1995,2239,2512,2818,3162};
/*
  unsigned int get_tx_amp_prach(int power_dBm, int power_max_dBm, int N_RB_UL)
  {

  int gain_dB = power_dBm - power_max_dBm;
  int amp_x_100;

  switch (N_RB_UL) {
  case 6:
  amp_x_100 = AMP;      // PRACH is 6 PRBS so no scale
  break;
  case 15:
  amp_x_100 = 158*AMP;  // 158 = 100*sqrt(15/6)
  break;
  case 25:
  amp_x_100 = 204*AMP;  // 204 = 100*sqrt(25/6)
  break;
  case 50:
  amp_x_100 = 286*AMP;  // 286 = 100*sqrt(50/6)
  break;
  case 75:
  amp_x_100 = 354*AMP;  // 354 = 100*sqrt(75/6)
  break;
  case 100:
  amp_x_100 = 408*AMP;  // 408 = 100*sqrt(100/6)
  break;
  default:
  LOG_E(PHY,"Unknown PRB size %d\n",N_RB_UL);
  mac_xface->macphy_exit("");
  break;
  }
  if (gain_dB < -30) {
  return(amp_x_100/3162);
  } else if (gain_dB>0)
  return(amp_x_100);
  else
  return(amp_x_100/gain_table[-gain_dB]);  // 245 corresponds to the factor sqrt(25/6)
  }
*/

unsigned int get_tx_amp(int power_dBm, int power_max_dBm, int N_RB_UL, int nb_rb)
{

  int gain_dB = power_dBm - power_max_dBm;
  double gain_lin;

  if (gain_dB < -20)
    return(AMP/10);

  gain_lin = pow(10,.1*gain_dB);
  if ((nb_rb >0) && (nb_rb <= N_RB_UL)) {
    return((int)(AMP*sqrt(gain_lin*N_RB_UL/(double)nb_rb)));
  }
  else {
    LOG_E(PHY,"Illegal nb_rb/N_RB_UL combination (%d/%d)\n",nb_rb,N_RB_UL);
    mac_xface->macphy_exit("");
  }
  return(0);
}

#endif

void dump_dlsch_ra(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t subframe)
{
  unsigned int coded_bits_per_codeword;
  uint8_t nsymb = ((ue->frame_parms.Ncp == 0) ? 14 : 12);

  coded_bits_per_codeword = get_G(&ue->frame_parms,
                                  ue->dlsch_ra[eNB_id]->harq_processes[0]->nb_rb,
                                  ue->dlsch_ra[eNB_id]->harq_processes[0]->rb_alloc_even,
                                  2,
                                  1,
                                  ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
                                  proc->frame_rx,subframe);
  LOG_D(PHY,"[UE %d] Dumping dlsch_ra : nb_rb %d, mcs %d, nb_rb %d, num_pdcch_symbols %d,G %d\n",
        ue->Mod_id,
        ue->dlsch_ra[eNB_id]->harq_processes[0]->nb_rb,
        ue->dlsch_ra[eNB_id]->harq_processes[0]->mcs,
        ue->dlsch_ra[eNB_id]->harq_processes[0]->nb_rb,
        ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
        coded_bits_per_codeword);

  write_output("rxsigF0.m","rxsF0", ue->common_vars.rxdataF[0],2*12*ue->frame_parms.ofdm_symbol_size,2,1);
  write_output("rxsigF0_ext.m","rxsF0_ext", ue->pdsch_vars_ra[0]->rxdataF_ext[0],2*12*ue->frame_parms.ofdm_symbol_size,1,1);
  write_output("dlsch00_ch0_ext.m","dl00_ch0_ext", ue->pdsch_vars_ra[0]->dl_ch_estimates_ext[0],300*nsymb,1,1);
  /*
    write_output("dlsch01_ch0_ext.m","dl01_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[1],300*12,1,1);
    write_output("dlsch10_ch0_ext.m","dl10_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[2],300*12,1,1);
    write_output("dlsch11_ch0_ext.m","dl11_ch0_ext",pdsch_vars[0]->dl_ch_estimates_ext[3],300*12,1,1);
    write_output("dlsch_rho.m","dl_rho",pdsch_vars[0]->rho[0],300*12,1,1);
  */
  write_output("dlsch_rxF_comp0.m","dlsch0_rxF_comp0", ue->pdsch_vars_ra[0]->rxdataF_comp0[0],300*nsymb,1,1);
  write_output("dlsch_rxF_llr.m","dlsch_llr", ue->pdsch_vars_ra[0]->llr[0],coded_bits_per_codeword,1,0);

  write_output("dlsch_mag1.m","dlschmag1",ue->pdsch_vars_ra[0]->dl_ch_mag0,300*nsymb,1,1);
  write_output("dlsch_mag2.m","dlschmag2",ue->pdsch_vars_ra[0]->dl_ch_magb0,300*nsymb,1,1);
}


void phy_reset_ue(uint8_t Mod_id,uint8_t CC_id,uint8_t eNB_index)
{

  // This flushes ALL DLSCH and ULSCH harq buffers of ALL connected eNBs...add the eNB_index later
  // for more flexibility

  uint8_t i,j,k;
  PHY_VARS_UE *ue = PHY_vars_UE_g[Mod_id][CC_id];

  //[NUMBER_OF_CONNECTED_eNB_MAX][2];
  for(i=0; i<NUMBER_OF_CONNECTED_eNB_MAX; i++) {
    for(j=0; j<2; j++) {
      //DL HARQ
      if(ue->dlsch[i][j]) {
        for(k=0; k<NUMBER_OF_HARQ_PID_MAX && ue->dlsch[i][j]->harq_processes[k]; k++) {
          ue->dlsch[i][j]->harq_processes[k]->status = SCH_IDLE;
        }
      }
    }

    //UL HARQ
    if(ue->ulsch[i]) {
      for(k=0; k<NUMBER_OF_HARQ_PID_MAX && ue->ulsch[i]->harq_processes[k]; k++) {
        ue->ulsch[i]->harq_processes[k]->status = SCH_IDLE;
        //Set NDIs for all UL HARQs to 0
        //  ue->ulsch[i]->harq_processes[k]->Ndi = 0;

      }
    }

    // flush Msg3 buffer
    ue->ulsch_Msg3_active[i] = 0;

  }
}

void ra_failed(uint8_t Mod_id,uint8_t CC_id,uint8_t eNB_index)
{

  // if contention resolution fails, go back to PRACH
  PHY_vars_UE_g[Mod_id][CC_id]->UE_mode[eNB_index] = PRACH;
  LOG_E(PHY,"[UE %d] Random-access procedure fails, going back to PRACH, setting SIStatus = 0 and State RRC_IDLE\n",Mod_id);
  //mac_xface->macphy_exit("");
}

void ra_succeeded(uint8_t Mod_id,uint8_t CC_id,uint8_t eNB_index)
{

  int i;

  LOG_I(PHY,"[UE %d][RAPROC] Random-access procedure succeeded\n",Mod_id);

  PHY_vars_UE_g[Mod_id][CC_id]->ulsch_Msg3_active[eNB_index] = 0;
  PHY_vars_UE_g[Mod_id][CC_id]->UE_mode[eNB_index] = PUSCH;

  for (i=0; i<8; i++) {
    if (PHY_vars_UE_g[Mod_id][CC_id]->ulsch[eNB_index]->harq_processes[i]) {
      PHY_vars_UE_g[Mod_id][CC_id]->ulsch[eNB_index]->harq_processes[i]->status=IDLE;
      PHY_vars_UE_g[Mod_id][CC_id]->dlsch[eNB_index][0]->harq_processes[i]->round=0;
    }
  }


}

UE_MODE_t get_ue_mode(uint8_t Mod_id,uint8_t CC_id,uint8_t eNB_index)
{

  return(PHY_vars_UE_g[Mod_id][CC_id]->UE_mode[eNB_index]);

}
void process_timing_advance_rar(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint16_t timing_advance) {

  ue->timing_advance = timing_advance*4;


#ifdef DEBUG_PHY_PROC
  LOG_I(PHY,"[UE %d] AbsoluteSubFrame %d.%d, received (rar) timing_advance %d, HW timing advance %d\n",ue->Mod_id,proc->frame_rx, proc->subframe_rx, ue->timing_advance);
#endif

}

void process_timing_advance(uint8_t Mod_id,uint8_t CC_id,int16_t timing_advance)
{

  //  uint32_t frame = PHY_vars_UE_g[Mod_id]->frame;

  // timing advance has Q1.5 format
  timing_advance = timing_advance - 31;

  PHY_vars_UE_g[Mod_id][CC_id]->timing_advance = PHY_vars_UE_g[Mod_id][CC_id]->timing_advance+timing_advance*4; //this is for 25RB only!!!


  LOG_I(PHY,"[UE %d] Got timing advance %d from MAC, new value %d\n",Mod_id, timing_advance, PHY_vars_UE_g[Mod_id][CC_id]->timing_advance);


}

uint8_t is_SR_TXOp(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id)
{

  int subframe=proc->subframe_tx;

  LOG_D(PHY,"[UE %d][SR %x] Frame %d subframe %d Checking for SR TXOp (sr_ConfigIndex %d)\n",
        ue->Mod_id,ue->pdcch_vars[eNB_id]->crnti,proc->frame_tx,subframe,
        ue->scheduling_request_config[eNB_id].sr_ConfigIndex);

  if (ue->scheduling_request_config[eNB_id].sr_ConfigIndex <= 4) {        // 5 ms SR period
    if ((subframe%5) == ue->scheduling_request_config[eNB_id].sr_ConfigIndex)
      return(1);
  } else if (ue->scheduling_request_config[eNB_id].sr_ConfigIndex <= 14) { // 10 ms SR period
    if (subframe==(ue->scheduling_request_config[eNB_id].sr_ConfigIndex-5))
      return(1);
  } else if (ue->scheduling_request_config[eNB_id].sr_ConfigIndex <= 34) { // 20 ms SR period
    if ((10*(proc->frame_tx&1)+subframe) == (ue->scheduling_request_config[eNB_id].sr_ConfigIndex-15))
      return(1);
  } else if (ue->scheduling_request_config[eNB_id].sr_ConfigIndex <= 74) { // 40 ms SR period
    if ((10*(proc->frame_tx&3)+subframe) == (ue->scheduling_request_config[eNB_id].sr_ConfigIndex-35))
      return(1);
  } else if (ue->scheduling_request_config[eNB_id].sr_ConfigIndex <= 154) { // 80 ms SR period
    if ((10*(proc->frame_tx&7)+subframe) == (ue->scheduling_request_config[eNB_id].sr_ConfigIndex-75))
      return(1);
  }

  return(0);
}

uint16_t get_n1_pucch(PHY_VARS_UE *ue,
		      UE_rxtx_proc_t *proc,
                      uint8_t eNB_id,
                      uint8_t *b,
                      uint8_t SR)
{

  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;
  uint8_t nCCE0,nCCE1,harq_ack1,harq_ack0;
  ANFBmode_t bundling_flag;
  uint16_t n1_pucch0=0,n1_pucch1=0;
  int subframe_offset;
  int sf;
  int M;
  // clear this, important for case where n1_pucch selection is not used
  int subframe=proc->subframe_tx;

  ue->pucch_sel[subframe] = 0;

  if (frame_parms->frame_type == FDD ) { // FDD
    sf = (subframe<4)? subframe+6 : subframe-4;
    LOG_D(PHY,"n1_pucch_UE: subframe %d, nCCE %d\n",sf,ue->pdcch_vars[eNB_id]->nCCE[sf]);

    if (SR == 0)
      return(frame_parms->pucch_config_common.n1PUCCH_AN + ue->pdcch_vars[eNB_id]->nCCE[sf]);
    else
      return(ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
  } else {

    bundling_flag = ue->pucch_config_dedicated[eNB_id].tdd_AckNackFeedbackMode;
#ifdef DEBUG_PHY_PROC

    if (bundling_flag==bundling) {
      LOG_D(PHY,"[UE%d] Frame %d subframe %d : get_n1_pucch, bundling, SR %d/%d\n",ue->Mod_id,proc->frame_tx,subframe,SR,
            ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
    } else {
      LOG_D(PHY,"[UE%d] Frame %d subframe %d : get_n1_pucch, multiplexing, SR %d/%d\n",ue->Mod_id,proc->frame_tx,subframe,SR,
            ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
    }

#endif

    switch (frame_parms->tdd_config) {
    case 1:  // DL:S:UL:UL:DL:DL:S:UL:UL:DL

      harq_ack0 = 2; // DTX
      M=1;

      // This is the offset for a particular subframe (2,3,4) => (0,2,4)
      if (subframe == 2) {  // ACK subframes 5 (forget 6)
        subframe_offset = 5;
        M=2;
      } else if (subframe == 3) { // ACK subframe 9
        subframe_offset = 9;
      } else if (subframe == 7) { // ACK subframes 0 (forget 1)
        subframe_offset = 0;
        M=2;
      } else if (subframe == 8) { // ACK subframes 4
        subframe_offset = 4;
      } else {
        LOG_E(PHY,"[UE%d] : Frame %d phy_procedures_lte.c: get_n1pucch, illegal subframe %d for tdd_config %d\n",
              ue->Mod_id,proc->frame_tx,subframe,frame_parms->tdd_config);
        return(0);
      }


      // i=0
      nCCE0 = ue->pdcch_vars[eNB_id]->nCCE[subframe_offset];
      n1_pucch0 = get_Np(frame_parms->N_RB_DL,nCCE0,0) + nCCE0+ frame_parms->pucch_config_common.n1PUCCH_AN;

      // set ACK/NAK to values if not DTX
      if (ue->dlsch[eNB_id][0]->harq_ack[subframe_offset].send_harq_status>0)  // n-6 // subframe 5 is to be ACK/NAKed
        harq_ack0 = ue->dlsch[eNB_id][0]->harq_ack[subframe_offset].ack;


      if (harq_ack0!=2) {  // DTX
        if (SR == 0) {  // last paragraph pg 68 from 36.213 (v8.6), m=0
          b[0]=(M==2) ? 1-harq_ack0 : harq_ack0;
          b[1]=harq_ack0;   // in case we use pucch format 1b (subframes 2,7)
          ue->pucch_sel[subframe] = 0;
          return(n1_pucch0);
        } else { // SR and only 0 or 1 ACKs (first 2 entries in Table 7.3-1 of 36.213)
          b[0]=harq_ack0;
          return(ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
        }
      }


      break;

    case 3:  // DL:S:UL:UL:UL:DL:DL:DL:DL:DL
      // in this configuration we have M=2 from pg 68 of 36.213 (v8.6)
      // Note: this doesn't allow using subframe 1 for PDSCH transmission!!! (i.e. SF 1 cannot be acked in SF 2)
      // set ACK/NAKs to DTX
      harq_ack1 = 2; // DTX
      harq_ack0 = 2; // DTX
      // This is the offset for a particular subframe (2,3,4) => (0,2,4)
      subframe_offset = (subframe-2)<<1;
      // i=0
      nCCE0 = ue->pdcch_vars[eNB_id]->nCCE[5+subframe_offset];
      n1_pucch0 = get_Np(frame_parms->N_RB_DL,nCCE0,0) + nCCE0+ frame_parms->pucch_config_common.n1PUCCH_AN;
      // i=1
      nCCE1 = ue->pdcch_vars[eNB_id]->nCCE[(6+subframe_offset)%10];
      n1_pucch1 = get_Np(frame_parms->N_RB_DL,nCCE1,1) + nCCE1 + frame_parms->pucch_config_common.n1PUCCH_AN;

      // set ACK/NAK to values if not DTX
      if (ue->dlsch[eNB_id][0]->harq_ack[(6+subframe_offset)%10].send_harq_status>0)  // n-6 // subframe 6 is to be ACK/NAKed
        harq_ack1 = ue->dlsch[eNB_id][0]->harq_ack[(6+subframe_offset)%10].ack;

      if (ue->dlsch[eNB_id][0]->harq_ack[5+subframe_offset].send_harq_status>0)  // n-6 // subframe 5 is to be ACK/NAKed
        harq_ack0 = ue->dlsch[eNB_id][0]->harq_ack[5+subframe_offset].ack;


      if (harq_ack1!=2) { // n-6 // subframe 6,8,0 and maybe 5,7,9 is to be ACK/NAKed

        if ((bundling_flag==bundling)&&(SR == 0)) {  // This is for bundling without SR,
          // n1_pucch index takes value of smallest element in set {0,1}
          // i.e. 0 if harq_ack0 is not DTX, otherwise 1
          b[0] = harq_ack1;

          if (harq_ack0!=2)
            b[0]=b[0]&harq_ack0;

          ue->pucch_sel[subframe] = 1;
          return(n1_pucch1);

        } else if ((bundling_flag==multiplexing)&&(SR==0)) { // Table 10.1
          if (harq_ack0 == 2)
            harq_ack0 = 0;

          b[1] = harq_ack0;
          b[0] = (harq_ack0!=harq_ack1)?0:1;

          if ((harq_ack0 == 1) && (harq_ack1 == 0)) {
            ue->pucch_sel[subframe] = 0;
            return(n1_pucch0);
          } else {
            ue->pucch_sel[subframe] = 1;
            return(n1_pucch1);
          }
        } else if (SR==1) { // SR and 0,1,or 2 ACKS, (first 3 entries in Table 7.3-1 of 36.213)
          // this should be number of ACKs (including
          if (harq_ack0 == 2)
            harq_ack0 = 0;

          b[0]= harq_ack1 | harq_ack0;
          b[1]= harq_ack1 ^ harq_ack0;
          return(ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
        }
      } else if (harq_ack0!=2) { // n-7  // subframe 5,7,9 only is to be ACK/NAKed
        if ((bundling_flag==bundling)&&(SR == 0)) {  // last paragraph pg 68 from 36.213 (v8.6), m=0
          b[0]=harq_ack0;
          ue->pucch_sel[subframe] = 0;
          return(n1_pucch0);
        } else if ((bundling_flag==multiplexing)&&(SR==0)) { // Table 10.1 with i=1 set to DTX
          b[0] = harq_ack0;
          b[1] = 1-b[0];
          ue->pucch_sel[subframe] = 0;
          return(n1_pucch0);
        } else if (SR==1) { // SR and only 0 or 1 ACKs (first 2 entries in Table 7.3-1 of 36.213)
          b[0]=harq_ack0;
          b[1]=b[0];
          return(ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex);
        }
      }

      break;

    }  // switch tdd_config
  }

  LOG_E(PHY,"[UE%d] : Frame %d phy_procedures_lte.c: get_n1pucch, exit without proper return\n",proc->frame_tx);
  return(-1);
}


#ifdef EMOS
/*
  void phy_procedures_emos_UE_TX(uint8_t next_slot,uint8_t eNB_id) {
  uint8_t harq_pid;


  if (next_slot%2==0) {
  // get harq_pid from subframe relationship
  harq_pid = subframe2harq_pid(&ue->frame_parms,ue->frame,(next_slot>>1));
  if (harq_pid==255) {
  LOG_E(PHY,"[UE%d] Frame %d : FATAL ERROR: illegal harq_pid, returning\n",
  0,ue->frame);
  return;
  }

  if (ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag == 1) {
  emos_dump_UE.uci_cnt[next_slot>>1] = 1;
  memcpy(emos_dump_UE.UCI_data[0][next_slot>>1].o,ulsch[eNB_id]->o,MAX_CQI_BITS*sizeof(char));
  emos_dump_UE.UCI_data[0][next_slot>>1].O = ulsch[eNB_id]->O;
  memcpy(emos_dump_UE.UCI_data[0][next_slot>>1].o_RI,ulsch[eNB_id]->o_RI,2*sizeof(char));
  emos_dump_UE.UCI_data[0][next_slot>>1].O_RI = ulsch[eNB_id]->O_RI;
  memcpy(emos_dump_UE.UCI_data[0][next_slot>>1].o_ACK,ulsch[eNB_id]->o_ACK,4*sizeof(char));
  emos_dump_UE.UCI_data[0][next_slot>>1].O_ACK = ulsch[eNB_id]->harq_processes[harq_pid]->O_ACK;
  }
  else {
  emos_dump_UE.uci_cnt[next_slot>>1] = 0;
  }
  }
  }
*/
#endif

void ulsch_common_procedures(PHY_VARS_UE *ue, UE_rxtx_proc_t *proc) {

  int aa;
  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;

  int nsymb;
  int subframe_tx = proc->subframe_tx;
  int frame_tx = proc->frame_tx;
  int ulsch_start;
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
  int overflow=0;
  int k,l;
  int dummy_tx_buffer[3840*4] __attribute__((aligned(16)));
#endif

  start_meas(&ue->ofdm_mod_stats);
  nsymb = (frame_parms->Ncp == 0) ? 14 : 12;
  
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)//this is the EXPRESS MIMO case
  ulsch_start = (ue->rx_offset+subframe_tx*frame_parms->samples_per_tti-
		 ue->hw_timing_advance-
		 ue->timing_advance-
		 ue->N_TA_offset+5)%(LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti);
#else //this is the normal case
  ulsch_start = (frame_parms->samples_per_tti*subframe_tx)-ue->N_TA_offset; //-ue->timing_advance;
#endif //else EXMIMO
  if ((frame_tx%100) == 0)
    LOG_D(PHY,"[UE %d] Frame %d, subframe %d: ulsch_start = %d (rxoff %d, HW TA %d, timing advance %d, TA_offset %d\n",
	  ue->Mod_id,frame_tx,subframe_tx,
	  ulsch_start,
	  ue->rx_offset,
	  ue->hw_timing_advance,
	  ue->timing_advance,
	  ue->N_TA_offset);
  
  
  for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
    if (frame_parms->Ncp == 1)
      PHY_ofdm_mod(&ue->common_vars.txdataF[aa][subframe_tx*nsymb*frame_parms->ofdm_symbol_size],
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
		   dummy_tx_buffer,
#else
		   &ue->common_vars.txdata[aa][ulsch_start],
#endif
		   frame_parms->ofdm_symbol_size,
		   nsymb,
		   frame_parms->nb_prefix_samples,
		   CYCLIC_PREFIX);
    else
      normal_prefix_mod(&ue->common_vars.txdataF[aa][subframe_tx*nsymb*frame_parms->ofdm_symbol_size],
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
			dummy_tx_buffer,
#else
			&ue->common_vars.txdata[aa][ulsch_start],
#endif
			nsymb,
			&ue->frame_parms);
    
    
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
    apply_7_5_kHz(ue,dummy_tx_buffer,0);
    apply_7_5_kHz(ue,dummy_tx_buffer,1);
#else
    apply_7_5_kHz(ue,&ue->common_vars.txdata[aa][ulsch_start],0);
    apply_7_5_kHz(ue,&ue->common_vars.txdata[aa][ulsch_start],1);
#endif
    
    
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
    overflow = ulsch_start - 9*frame_parms->samples_per_tti;
    
    
    for (k=ulsch_start,l=0; k<cmin(frame_parms->samples_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME,ulsch_start+frame_parms->samples_per_tti); k++,l++) {
      ((short*)ue->common_vars.txdata[aa])[2*k] = ((short*)dummy_tx_buffer)[2*l]<<4;
      ((short*)ue->common_vars.txdata[aa])[2*k+1] = ((short*)dummy_tx_buffer)[2*l+1]<<4;
    }
    
    for (k=0; k<overflow; k++,l++) {
      ((short*)ue->common_vars.txdata[aa])[2*k] = ((short*)dummy_tx_buffer)[2*l]<<4;
      ((short*)ue->common_vars.txdata[aa])[2*k+1] = ((short*)dummy_tx_buffer)[2*l+1]<<4;
    }
#if defined(EXMIMO)
    // handle switch before 1st TX subframe, guarantee that the slot prior to transmission is switch on
    for (k=ulsch_start - (frame_parms->samples_per_tti>>1) ; k<ulsch_start ; k++) {
      if (k<0)
	ue->common_vars.txdata[aa][k+frame_parms->samples_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME] &= 0xFFFEFFFE;
      else if (k>(frame_parms->samples_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME))
	ue->common_vars.txdata[aa][k-frame_parms->samples_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME] &= 0xFFFEFFFE;
      else
	ue->common_vars.txdata[aa][k] &= 0xFFFEFFFE;
    }
#endif
#endif
    
  } //nb_antennas_tx
  
  stop_meas(&ue->ofdm_mod_stats);



}

void ue_prach_procedures(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag,runmode_t mode) {

  int frame_tx = proc->frame_tx;
  int subframe_tx = proc->subframe_tx;
  int prach_power;
  PRACH_RESOURCES_t prach_resources_local;

  ue->generate_prach=0;

  if (ue->mac_enabled==0){
    ue->prach_resources[eNB_id] = &prach_resources_local;
    prach_resources_local.ra_RNTI = 0xbeef;
    prach_resources_local.ra_PreambleIndex = 0;
  }

  if (ue->mac_enabled==1){
    // ask L2 for RACH transport
    if ((mode != rx_calib_ue) && (mode != rx_calib_ue_med) && (mode != rx_calib_ue_byp) && (mode != no_L2_connect) ) {
      LOG_D(PHY,"Getting PRACH resources\n");
      ue->prach_resources[eNB_id] = mac_xface->ue_get_rach(ue->Mod_id,
							   ue->CC_id,
							   frame_tx,
							   eNB_id,
							   subframe_tx);
      LOG_D(PHY,"Got prach_resources for eNB %d address %d, RRCCommon %d\n",eNB_id,ue->prach_resources[eNB_id],UE_mac_inst[ue->Mod_id].radioResourceConfigCommon);
      LOG_D(PHY,"Prach resources %p\n",ue->prach_resources[eNB_id]);
    }
  }
  
  if (ue->prach_resources[eNB_id]!=NULL) {
    
    ue->generate_prach=1;
    ue->prach_cnt=0;
#ifdef SMBV
    ue->prach_resources[eNB_id]->ra_PreambleIndex = 19;
#endif
#ifdef OAI_EMU
    ue->prach_PreambleIndex=ue->prach_resources[eNB_id]->ra_PreambleIndex;
#endif
    
    if (abstraction_flag == 0) {

      LOG_I(PHY,"mode %d\n",mode);
      
      if ((ue->mac_enabled==1) && (mode != calib_prach_tx)) {
	ue->tx_power_dBm[subframe_tx] = ue->prach_resources[eNB_id]->ra_PREAMBLE_RECEIVED_TARGET_POWER+get_PL(ue->Mod_id,ue->CC_id,eNB_id);
      }
      else {
	ue->tx_power_dBm[subframe_tx] = ue->tx_power_max_dBm;
	ue->prach_resources[eNB_id]->ra_PreambleIndex = 19;	      
      }
      
      LOG_I(PHY,"[UE  %d][RAPROC] Frame %d, Subframe %d : Generating PRACH, preamble %d, P0_PRACH %d, TARGET_RECEIVED_POWER %d dBm, PRACH TDD Resource index %d, RA-RNTI %d\n",
	    ue->Mod_id,
	    frame_tx,
	    subframe_tx,
	    ue->prach_resources[eNB_id]->ra_PreambleIndex,
		ue->tx_power_dBm[subframe_tx],
	    ue->prach_resources[eNB_id]->ra_PREAMBLE_RECEIVED_TARGET_POWER,
	    ue->prach_resources[eNB_id]->ra_TDD_map_index,
	    ue->prach_resources[eNB_id]->ra_RNTI);

      ue->tx_total_RE[subframe_tx] = 96;
      
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
      ue->prach_vars[eNB_id]->amp = get_tx_amp(ue->tx_power_dBm[subframe_tx],
					       ue->tx_power_max_dBm,
					       ue->frame_parms.N_RB_UL,
					       6);
#else
      ue->prach_vars[eNB_id]->amp = AMP;
#endif
      if ((mode == calib_prach_tx) && (((proc->frame_tx&0xfffe)%100)==0))
	LOG_D(PHY,"[UE  %d][RAPROC] Frame %d, Subframe %d : PRACH TX power %d dBm, amp %d\n",
	      ue->Mod_id,
	      proc->frame_rx,
	      proc->subframe_tx,
	      ue->tx_power_dBm[subframe_tx],
	      ue->prach_vars[eNB_id]->amp);
      
      
      //      start_meas(&ue->tx_prach);
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GENERATE_PRACH, VCD_FUNCTION_IN);
      prach_power = generate_prach(ue,eNB_id,subframe_tx,frame_tx);
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GENERATE_PRACH, VCD_FUNCTION_OUT);
      //      stop_meas(&ue->tx_prach);
      LOG_D(PHY,"[UE  %d][RAPROC] PRACH PL %d dB, power %d dBm, digital power %d dB (amp %d)\n",
	    ue->Mod_id,
	    get_PL(ue->Mod_id,ue->CC_id,eNB_id),
	    ue->tx_power_dBm[subframe_tx],
	    dB_fixed(prach_power),
	    ue->prach_vars[eNB_id]->amp);
    } else {
      UE_transport_info[ue->Mod_id][ue->CC_id].cntl.prach_flag=1;
      UE_transport_info[ue->Mod_id][ue->CC_id].cntl.prach_id=ue->prach_resources[eNB_id]->ra_PreambleIndex;
      if (ue->mac_enabled==1){
	mac_xface->Msg1_transmitted(ue->Mod_id,
				    ue->CC_id,
				    frame_tx,
				    eNB_id);
      }
    }
    
    LOG_D(PHY,"[UE  %d][RAPROC] Frame %d, subframe %d: Generating PRACH (eNB %d) preamble index %d for UL, TX power %d dBm (PL %d dB), l3msg \n",
	  ue->Mod_id,frame_tx,subframe_tx,eNB_id,
	  ue->prach_resources[eNB_id]->ra_PreambleIndex,
	  ue->prach_resources[eNB_id]->ra_PREAMBLE_RECEIVED_TARGET_POWER+get_PL(ue->Mod_id,ue->CC_id,eNB_id),
	  get_PL(ue->Mod_id,ue->CC_id,eNB_id));
    
  }	  
  

  // if we're calibrating the PRACH kill the pointer to its resources so that the RA protocol doesn't continue
  if (mode == calib_prach_tx)
    ue->prach_resources[eNB_id]=NULL;
  
  LOG_D(PHY,"[UE %d] frame %d subframe %d : generate_prach %d, prach_cnt %d\n",
	ue->Mod_id,frame_tx,subframe_tx,ue->generate_prach,ue->prach_cnt);
  
  ue->prach_cnt++;
  
  if (ue->prach_cnt==3)
    ue->generate_prach=0;
}

void ue_ulsch_uespec_procedures(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag) {

  int harq_pid;
  int frame_tx=proc->frame_tx;
  int subframe_tx=proc->subframe_tx;
  int Mod_id = ue->Mod_id;
  int CC_id = ue->CC_id;
  uint8_t Msg3_flag=0;
  uint8_t ack_status=0;
  uint16_t first_rb, nb_rb;
  unsigned int input_buffer_length;
  int i;
  int aa;
  int tx_amp;
  uint8_t ulsch_input_buffer[5477] __attribute__ ((aligned(32)));
  uint8_t access_mode;

  // get harq_pid from subframe relationship
  harq_pid = subframe2harq_pid(&ue->frame_parms,
			       frame_tx,
			       subframe_tx);
  
  
  if (ue->mac_enabled == 1) {
    if ((ue->ulsch_Msg3_active[eNB_id] == 1) &&
	(ue->ulsch_Msg3_frame[eNB_id] == frame_tx) &&
	(ue->ulsch_Msg3_subframe[eNB_id] == subframe_tx)) { // Initial Transmission of Msg3
      
      ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag = 1;
      
      if (ue->ulsch[eNB_id]->harq_processes[harq_pid]->round==0)
	generate_ue_ulsch_params_from_rar(ue,
					  proc,
					  eNB_id);
      
      ue->ulsch[eNB_id]->power_offset = 14;
      LOG_D(PHY,"[UE  %d][RAPROC] Frame %d: Setting Msg3_flag in subframe %d, for harq_pid %d\n",
	    Mod_id,
	    frame_tx,
	    subframe_tx,
	    harq_pid);
      Msg3_flag = 1;
    } else {
      
      if (harq_pid==255) {
	LOG_E(PHY,"[UE%d] Frame %d ulsch_decoding.c: FATAL ERROR: illegal harq_pid, returning\n",
	      Mod_id,frame_tx);
	mac_xface->macphy_exit("Error in ulsch_decoding");
	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX, VCD_FUNCTION_OUT);
	stop_meas(&ue->phy_proc_tx);
	return;
      }
      
      Msg3_flag=0;
    }
  }
  
  if (ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag == 1) {
    
    ue->generate_ul_signal[eNB_id] = 1;
    
    // deactivate service request
    ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag = 0;
    
    ack_status = get_ack(&ue->frame_parms,
			 ue->dlsch[eNB_id][0]->harq_ack,
			 subframe_tx,
			 ue->ulsch[eNB_id]->o_ACK);
    
    first_rb = ue->ulsch[eNB_id]->harq_processes[harq_pid]->first_rb;
    nb_rb = ue->ulsch[eNB_id]->harq_processes[harq_pid]->nb_rb;
    
    
    
    
#ifdef DEBUG_PHY_PROC
    LOG_D(PHY,
	  "[UE  %d][PUSCH %d] Frame %d subframe %d Generating PUSCH : first_rb %d, nb_rb %d, round %d, mcs %d, rv %d, cyclic_shift %d (cyclic_shift_common %d,n_DMRS2 %d,n_PRS %d), ACK (%d,%d), O_ACK %d\n",
	  Mod_id,harq_pid,frame_tx,subframe_tx,
	  first_rb,nb_rb,
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->round,
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->mcs,
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->rvidx,
	  (ue->frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift+
	   ue->ulsch[eNB_id]->harq_processes[harq_pid]->n_DMRS2+
	   ue->frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[subframe_tx<<1])%12,
	  ue->frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.cyclicShift,
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->n_DMRS2,
	  ue->frame_parms.pusch_config_common.ul_ReferenceSignalsPUSCH.nPRS[subframe_tx<<1],
	  ue->ulsch[eNB_id]->o_ACK[0],ue->ulsch[eNB_id]->o_ACK[1],
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->O_ACK);
#endif
    
    if (ack_status > 0) {
      LOG_D(PHY,"[UE  %d][PDSCH %x] Frame %d subframe %d Generating ACK (%d,%d) for %d bits on PUSCH\n",
	    Mod_id,
	    ue->ulsch[eNB_id]->rnti,
	    frame_tx,subframe_tx,
	    ue->ulsch[eNB_id]->o_ACK[0],ue->ulsch[eNB_id]->o_ACK[1],
	    ue->ulsch[eNB_id]->harq_processes[harq_pid]->O_ACK);
    }
    
    
    
    
    
    if (Msg3_flag == 1) {
      LOG_I(PHY,"[UE  %d][RAPROC] Frame %d, Subframe %d Generating (RRCConnectionRequest) Msg3 (nb_rb %d, first_rb %d, round %d, rvidx %d) Msg3: %x.%x.%x|%x.%x.%x.%x.%x.%x\n",Mod_id,frame_tx,
	    subframe_tx,
	    ue->ulsch[eNB_id]->harq_processes[harq_pid]->nb_rb,
	    ue->ulsch[eNB_id]->harq_processes[harq_pid]->first_rb,
	    ue->ulsch[eNB_id]->harq_processes[harq_pid]->round,
	    ue->ulsch[eNB_id]->harq_processes[harq_pid]->rvidx,
	    ue->prach_resources[eNB_id]->Msg3[0],
	    ue->prach_resources[eNB_id]->Msg3[1],
	    ue->prach_resources[eNB_id]->Msg3[2],
	    ue->prach_resources[eNB_id]->Msg3[3],
	    ue->prach_resources[eNB_id]->Msg3[4],
	    ue->prach_resources[eNB_id]->Msg3[5],
	    ue->prach_resources[eNB_id]->Msg3[6],
	    ue->prach_resources[eNB_id]->Msg3[7],
	    ue->prach_resources[eNB_id]->Msg3[8]);
      
      start_meas(&ue->ulsch_encoding_stats);
      
      if (abstraction_flag==0) {
	if (ulsch_encoding(ue->prach_resources[eNB_id]->Msg3,
			   ue,
			   harq_pid,
			   eNB_id,
			   ue->transmission_mode[eNB_id],0,0)!=0) {
	  LOG_E(PHY,"ulsch_coding.c: FATAL ERROR: returning\n");
	  mac_xface->macphy_exit("Error in ulsch_coding");
	  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX, VCD_FUNCTION_OUT);
	  stop_meas(&ue->phy_proc_tx);
	  return;
	}
      }
      
#ifdef PHY_ABSTRACTION
      else {
	ulsch_encoding_emul(ue->prach_resources[eNB_id]->Msg3,ue,eNB_id,harq_pid,0);
      }
      
#endif
      
      stop_meas(&ue->ulsch_encoding_stats);
      
      if (ue->mac_enabled == 1) {
	// signal MAC that Msg3 was sent
	mac_xface->Msg3_transmitted(Mod_id,
				    CC_id,
				    frame_tx,
				    eNB_id);
      }
    } // Msg3_flag==1
    else {
      input_buffer_length = ue->ulsch[eNB_id]->harq_processes[harq_pid]->TBS/8;
      
      if (ue->mac_enabled==1) {
	//  LOG_D(PHY,"[UE  %d] ULSCH : Searching for MAC SDUs\n",Mod_id);
	if (ue->ulsch[eNB_id]->harq_processes[harq_pid]->round==0) {
	  //if (ue->ulsch[eNB_id]->harq_processes[harq_pid]->calibration_flag == 0) {
	  access_mode=SCHEDULED_ACCESS;
	  mac_xface->ue_get_sdu(Mod_id,
				CC_id,
				frame_tx,
				subframe_tx,
				eNB_id,
				ulsch_input_buffer,
				input_buffer_length,
				&access_mode);
	}
	
#ifdef DEBUG_PHY_PROC
#ifdef DEBUG_ULSCH
	LOG_D(PHY,"[UE] Frame %d, subframe %d : ULSCH SDU (TX harq_pid %d)  (%d bytes) : \n",frame_tx,subframe_tx,harq_pid, ue->ulsch[eNB_id]->harq_processes[harq_pid]->TBS>>3);
	
	for (i=0; i<ue->ulsch[eNB_id]->harq_processes[harq_pid]->TBS>>3; i++)
	  LOG_T(PHY,"%x.",ulsch_input_buffer[i]);
	
	LOG_T(PHY,"\n");
#endif
#endif
      }
      else {
	unsigned int taus(void);
	
	for (i=0; i<input_buffer_length; i++)
	  ulsch_input_buffer[i]= (uint8_t)(taus()&0xff);
	
      }
      
      start_meas(&ue->ulsch_encoding_stats);
      
      if (abstraction_flag==0) {
	
	if (ulsch_encoding(ulsch_input_buffer,
			   ue,
			   harq_pid,
			   eNB_id,
			   ue->transmission_mode[eNB_id],0,
			   0)!=0) {  //  Nbundled, to be updated!!!!
	  LOG_E(PHY,"ulsch_coding.c: FATAL ERROR: returning\n");
	  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX, VCD_FUNCTION_OUT);
	  stop_meas(&ue->phy_proc_tx);
	  return;
	}
      }
      
#ifdef PHY_ABSTRACTION
      else {
	ulsch_encoding_emul(ulsch_input_buffer,ue,eNB_id,harq_pid,0);
      }
      
#endif
      stop_meas(&ue->ulsch_encoding_stats);
    }
    
    if (abstraction_flag == 0) {
      if (ue->mac_enabled==1) {
	pusch_power_cntl(ue,proc,eNB_id,1, abstraction_flag);
	ue->tx_power_dBm[subframe_tx] = ue->ulsch[eNB_id]->Po_PUSCH;
      }
      else {
	ue->tx_power_dBm[subframe_tx] = ue->tx_power_max_dBm;
      }
      ue->tx_total_RE[subframe_tx] = nb_rb*12;
      
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
      tx_amp = get_tx_amp(ue->tx_power_dBm[subframe_tx],
			  ue->tx_power_max_dBm,
			  ue->frame_parms.N_RB_UL,
			  nb_rb);
#else
      tx_amp = AMP;
#endif
      LOG_D(PHY,"[UE  %d][PUSCH %d] Frame %d subframe %d, generating PUSCH, Po_PUSCH: %d dBm (max %d dBm), amp %d\n",
	    Mod_id,harq_pid,frame_tx,subframe_tx,ue->tx_power_dBm[subframe_tx],ue->tx_power_max_dBm, tx_amp);
      start_meas(&ue->ulsch_modulation_stats);
      ulsch_modulation(ue->common_vars.txdataF,
		       tx_amp,
		       frame_tx,
		       subframe_tx,
		       &ue->frame_parms,
		       ue->ulsch[eNB_id]);
      for (aa=0; aa<1/*frame_parms->nb_antennas_tx*/; aa++)
	generate_drs_pusch(ue,
			   proc,
			   eNB_id,
			   tx_amp,
			   subframe_tx,
			   first_rb,
			   nb_rb,
			   aa);
      
      stop_meas(&ue->ulsch_modulation_stats);
    }
    
    if (abstraction_flag==1) {
      // clear SR
      ue->sr[subframe_tx]=0;
    }
  } // subframe_scheduling_flag==1
}


void ue_pucch_procedures(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag) {


  uint8_t pucch_ack_payload[2];
  uint8_t n1_pucch;
  ANFBmode_t bundling_flag;
  PUCCH_FMT_t format;
  uint8_t SR_payload;
  LTE_DL_FRAME_PARMS *frame_parms = &ue->frame_parms;
  int frame_tx=proc->frame_tx;
  int subframe_tx=proc->subframe_tx;
  int Mod_id = ue->Mod_id;
  int CC_id = ue->CC_id;
  int tx_amp;
  int8_t Po_PUCCH;

  bundling_flag = ue->pucch_config_dedicated[eNB_id].tdd_AckNackFeedbackMode;
  
  if ((frame_parms->frame_type==FDD) ||
      (bundling_flag==bundling)    ||
      ((frame_parms->frame_type==TDD)&&(frame_parms->tdd_config==1)&&((subframe_tx!=2)||(subframe_tx!=7)))) {
    format = pucch_format1a;
    LOG_D(PHY,"[UE] PUCCH 1a\n");
  } else {
    format = pucch_format1b;
    LOG_D(PHY,"[UE] PUCCH 1b\n");
  }
  
  // Check for SR and do ACK/NACK accordingly
  if (is_SR_TXOp(ue,proc,eNB_id)==1) {
    LOG_D(PHY,"[UE %d][SR %x] Frame %d subframe %d: got SR_TXOp, Checking for SR for PUSCH from MAC\n",
	  Mod_id,ue->pdcch_vars[eNB_id]->crnti,frame_tx,subframe_tx);
    
    if (ue->mac_enabled==1) {
      SR_payload = mac_xface->ue_get_SR(Mod_id,
					CC_id,
					frame_tx,
					eNB_id,
					ue->pdcch_vars[eNB_id]->crnti,
					subframe_tx); // subframe used for meas gap
    }
    else {
      SR_payload = 1;
    }
	    
    if (SR_payload>0) {
      ue->generate_ul_signal[eNB_id] = 1;
      LOG_D(PHY,"[UE %d][SR %x] Frame %d subframe %d got the SR for PUSCH is %d\n",
	    Mod_id,ue->pdcch_vars[eNB_id]->crnti,frame_tx,subframe_tx,SR_payload);
    } else {
      ue->sr[subframe_tx]=0;
    }
  } else {
    SR_payload=0;
  }
        	  
  if (get_ack(&ue->frame_parms,
	      ue->dlsch[eNB_id][0]->harq_ack,
	      subframe_tx,pucch_ack_payload) > 0) {
    // we need to transmit ACK/NAK in this subframe
	    
    ue->generate_ul_signal[eNB_id] = 1;
	    
    n1_pucch = get_n1_pucch(ue,
			    proc,
			    eNB_id,
			    pucch_ack_payload,
			    SR_payload);
	    
    if (ue->mac_enabled == 1) {
      Po_PUCCH = pucch_power_cntl(ue,proc,subframe_tx,eNB_id,format);
    } 
    else {
      Po_PUCCH = ue->tx_power_max_dBm;
    }
    ue->tx_power_dBm[subframe_tx] = Po_PUCCH;
    ue->tx_total_RE[subframe_tx] = 12;
	    
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
    tx_amp = get_tx_amp(Po_PUCCH,
			ue->tx_power_max_dBm,
			ue->frame_parms.N_RB_UL,
			1);
#else
    tx_amp = AMP;
#endif
	    
    if (SR_payload>0) {
      LOG_D(PHY,"[UE  %d][SR %x] Frame %d subframe %d Generating PUCCH 1a/1b payload %d,%d (with SR for PUSCH), n1_pucch %d, Po_PUCCH, amp %d\n",
	    Mod_id,
	    ue->dlsch[eNB_id][0]->rnti,
	    frame_tx, subframe_tx,
	    pucch_ack_payload[0],pucch_ack_payload[1],
	    ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex,
	    Po_PUCCH,
	    tx_amp);
    } else {
      LOG_D(PHY,"[UE  %d][PDSCH %x] Frame %d subframe %d Generating PUCCH 1a/1b, n1_pucch %d, b[0]=%d,b[1]=%d (SR_Payload %d), Po_PUCCH %d, amp %d\n",
	    Mod_id,
	    ue->dlsch[eNB_id][0]->rnti,
	    frame_tx, subframe_tx,
	    n1_pucch,pucch_ack_payload[0],pucch_ack_payload[1],SR_payload,
	    Po_PUCCH,
	    tx_amp);
    }
	    
    if (abstraction_flag == 0) {
	      
      generate_pucch1x(ue->common_vars.txdataF,
		       &ue->frame_parms,
		       ue->ncs_cell,
		       format,
		       &ue->pucch_config_dedicated[eNB_id],
		       n1_pucch,
		       1,  // shortened format
		       pucch_ack_payload,
		       tx_amp,
		       subframe_tx);
	      
    } else {
#ifdef PHY_ABSTRACTION
      LOG_D(PHY,"Calling generate_pucch_emul ... (ACK %d %d, SR %d)\n",pucch_ack_payload[0],pucch_ack_payload[1],SR_payload);
      generate_pucch_emul(ue,
			  proc,
			  format,
			  ue->frame_parms.pucch_config_common.nCS_AN,
			  pucch_ack_payload,
			  SR_payload);
#endif
    }
  } else if (SR_payload==1) { // no ACK/NAK but SR is triggered by MAC
	    
    if (ue->mac_enabled == 1) {
      Po_PUCCH = pucch_power_cntl(ue,proc,subframe_tx,eNB_id,pucch_format1);
    }
    else {
      Po_PUCCH = ue->tx_power_max_dBm;
    }
    ue->tx_power_dBm[subframe_tx] = Po_PUCCH;
    ue->tx_total_RE[subframe_tx] = 12;
	    
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)
    tx_amp =  get_tx_amp(Po_PUCCH,
			 ue->tx_power_max_dBm,
			 ue->frame_parms.N_RB_UL,
			 1);
#else
    tx_amp = AMP;
#endif
    LOG_D(PHY,"[UE  %d][SR %x] Frame %d subframe %d Generating PUCCH 1 (SR for PUSCH), n1_pucch %d, Po_PUCCH %d\n",
	  Mod_id,
	  ue->dlsch[eNB_id][0]->rnti,
	  frame_tx, subframe_tx,
	  ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex,
	  Po_PUCCH);
	    
    if (abstraction_flag == 0) {
	      
      generate_pucch1x(ue->common_vars.txdataF,
		       &ue->frame_parms,
		       ue->ncs_cell,
		       pucch_format1,
		       &ue->pucch_config_dedicated[eNB_id],
		       ue->scheduling_request_config[eNB_id].sr_PUCCH_ResourceIndex,
		       1,  // shortened format
		       pucch_ack_payload,  // this is ignored anyway, we just need a pointer
		       tx_amp,
		       subframe_tx);
    } else {
      LOG_D(PHY,"Calling generate_pucch_emul ...\n");
      generate_pucch_emul(ue,
			  proc,
			  pucch_format1,
			  ue->frame_parms.pucch_config_common.nCS_AN,
			  pucch_ack_payload,
			  SR_payload);

    }
  } // SR_Payload==1
}

void phy_procedures_UE_TX(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag,runmode_t mode,relaying_type_t r_type) {
  

  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;
  int32_t ulsch_start=0;
  int subframe_tx = proc->subframe_tx;
  int frame_tx = proc->frame_tx;
  unsigned int aa;




  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX,VCD_FUNCTION_IN);

  ue->generate_ul_signal[eNB_id] = 0;

  start_meas(&ue->phy_proc_tx);

#ifdef EMOS
  //phy_procedures_emos_UE_TX(next_slot);
#endif

  ue->tx_power_dBm[subframe_tx]=-127;
      
  if (abstraction_flag==0) {
    for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
      memset(&ue->common_vars.txdataF[aa][subframe_tx*frame_parms->ofdm_symbol_size*frame_parms->symbols_per_tti],
	     0,
	     frame_parms->ofdm_symbol_size*frame_parms->symbols_per_tti*sizeof(int32_t));
    }
  }
      
  if (ue->UE_mode[eNB_id] != PRACH) {

    ue_ulsch_uespec_procedures(ue,proc,eNB_id,abstraction_flag);

  }
  	  
  if (ue->UE_mode[eNB_id] == PUSCH) { // check if we need to use PUCCH 1a/1b
	  ue_pucch_procedures(ue,proc,eNB_id,abstraction_flag);
  } // UE_mode==PUSCH
	
  	
#ifdef CBA
	
  if ((ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_cba_scheduling_flag >= 1) &&
      (ue->ulsch[eNB_id]->harq_processes[harq_pid]->status == CBA_ACTIVE)) {
    ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag=0; //-=1
    //  ue->ulsch[eNB_id]->harq_processes[harq_pid]->status= IDLE;
    first_rb = ue->ulsch[eNB_id]->harq_processes[harq_pid]->first_rb;
    nb_rb = ue->ulsch[eNB_id]->harq_processes[harq_pid]->nb_rb;
    //cba_mcs=ue->ulsch[eNB_id]->harq_processes[harq_pid]->mcs;
    input_buffer_length = ue->ulsch[eNB_id]->harq_processes[harq_pid]->TBS/8;
    access_mode=CBA_ACCESS;
	  
    LOG_D(PHY,"[UE %d] Frame %d, subframe %d: CBA num dci %d\n",
	  Mod_id,frame_tx,subframe_tx,
	  ue->ulsch[eNB_id]->num_cba_dci[subframe_tx]);
	  
    mac_xface->ue_get_sdu(Mod_id,
			  CC_id,
			  frame_tx,
			  subframe_tx,
			  eNB_id,
			  ulsch_input_buffer,
			  input_buffer_length,
			  &access_mode);
	  
    ue->ulsch[eNB_id]->num_cba_dci[subframe_tx]=0;
	  
    if (access_mode > UNKNOWN_ACCESS) {
	    
      if (abstraction_flag==0) {
	if (ulsch_encoding(ulsch_input_buffer,
			   ue,
			   harq_pid,
			   eNB_id,
			   ue->transmission_mode[eNB_id],0,
			   0)!=0) {  //  Nbundled, to be updated!!!!
	  LOG_E(PHY,"ulsch_coding.c: FATAL ERROR: returning\n");
	  return;
	}
      }
	    
#ifdef PHY_ABSTRACTION
      else {
	ulsch_encoding_emul(ulsch_input_buffer,ue,eNB_id,harq_pid,0);
      }
	    
#endif
    } else {
      ue->ulsch[eNB_id]->harq_processes[harq_pid]->status= IDLE;
      //reset_cba_uci(ue->ulsch[eNB_id]->o);
      LOG_N(PHY,"[UE %d] Frame %d, subframe %d: CBA transmission cancelled or postponed\n",
	    Mod_id, frame_tx,subframe_tx);
    }
  }
	
#endif // end CBA

  	
  if (abstraction_flag == 0) {
	  
    if (ue->generate_ul_signal[eNB_id] == 1 )
    {
      ulsch_common_procedures(ue,proc);
    }
    else {  // no uplink so clear signal buffer instead
#if defined(EXMIMO) || defined(OAI_USRP) || defined(OAI_BLADERF) || defined(OAI_LMSSDR)//this is the EXPRESS MIMO case
      ulsch_start = (ue->rx_offset+subframe_tx*frame_parms->samples_per_tti-
		     ue->hw_timing_advance-
		     ue->timing_advance-
		     ue->N_TA_offset+5)%(LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti);
#else //this is the normal case
      ulsch_start = (frame_parms->samples_per_tti*subframe_tx)-ue->N_TA_offset; //-ue->timing_advance;
#endif //else EXMIMO
      for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
	memset(&ue->common_vars.txdata[aa][ulsch_start],0,frame_parms->samples_per_tti<<2);
      }
    }

  } // mode != PRACH
    
      
  if ((ue->UE_mode[eNB_id] == PRACH) && 
      (ue->frame_parms.prach_config_common.prach_Config_enabled==1)) {
	
    // check if we have PRACH opportunity

    if (is_prach_subframe(&ue->frame_parms,frame_tx,subframe_tx)) {

      ue_prach_procedures(ue,proc,eNB_id,abstraction_flag,mode);
    }
  } // mode is PRACH
  else {
    ue->generate_prach=0;
  }
    
      
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_TX, VCD_FUNCTION_OUT);
  stop_meas(&ue->phy_proc_tx);
}

void phy_procedures_UE_S_TX(PHY_VARS_UE *ue,uint8_t eNB_id,uint8_t abstraction_flag,relaying_type_t r_type)
{
  int aa;//i,aa;
  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;
  
  if (abstraction_flag==0) {
    
    for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
#if defined(EXMIMO) //this is the EXPRESS MIMO case
      int i;
      // set the whole tx buffer to RX
      for (i=0; i<LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti; i++)
	ue->common_vars.txdata[aa][i] = 0x00010001;
      
#else //this is the normal case
      memset(&ue->common_vars.txdata[aa][0],0,
	     (LTE_NUMBER_OF_SUBFRAMES_PER_FRAME*frame_parms->samples_per_tti)*sizeof(int32_t));
#endif //else EXMIMO
      
    }
  }
}

void ue_measurement_procedures(uint16_t l, PHY_VARS_UE *ue,UE_rxtx_proc_t *proc, uint8_t eNB_id,uint8_t abstraction_flag,runmode_t mode)
{
  
  LTE_DL_FRAME_PARMS *frame_parms=&ue->frame_parms;

  int subframe_rx = proc->subframe_rx;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_MEASUREMENT_PROCEDURES, VCD_FUNCTION_IN);

  if (l==0) {
    // UE measurements on symbol 0
    if (abstraction_flag==0) {
      LOG_D(PHY,"Calling measurements subframe %d, rxdata %p\n",subframe_rx,ue->common_vars.rxdata);

      lte_ue_measurements(ue,
			  (subframe_rx*frame_parms->samples_per_tti+ue->rx_offset)%(frame_parms->samples_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME),
			  (subframe_rx == 1) ? 1 : 0,
			  0);
    } else {
      lte_ue_measurements(ue,
			  0,
			  0,
			  1);
    }
  }

  if (l==(6-ue->frame_parms.Ncp)) {
	
    // make sure we have signal from PSS/SSS for N0 measurement

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_RRC_MEASUREMENTS, VCD_FUNCTION_IN);
    ue_rrc_measurements(ue,
			subframe_rx,
			abstraction_flag);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_RRC_MEASUREMENTS, VCD_FUNCTION_OUT);

    if (abstraction_flag==1)
      ue->sinr_eff =  sinr_eff_cqi_calc(ue, 0);

  }

  if ((subframe_rx==0) && (l==(4-frame_parms->Ncp))) {

    // AGC

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GAIN_CONTROL, VCD_FUNCTION_IN);

#ifndef OAI_USRP
#ifndef OAI_BLADERF
#ifndef OAI_LMSSDR
    phy_adjust_gain (ue,dB_fixed(ue->measurements.rssi),0);
#endif
#endif
#endif

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_GAIN_CONTROL, VCD_FUNCTION_OUT);

    eNB_id = 0;
    
    if (abstraction_flag == 0) {
      if (ue->no_timing_correction==0)
	lte_adjust_synch(&ue->frame_parms,
			 ue,
			 eNB_id,
			 0,
			 16384);
    }      

  }

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_MEASUREMENT_PROCEDURES, VCD_FUNCTION_OUT);
}

#ifdef EMOS
void phy_procedures_emos_UE_RX(PHY_VARS_UE *ue,uint8_t last_slot,uint8_t eNB_id)
{

  uint8_t i,j;
  //uint16_t last_slot_emos;
  uint32_t bytes;
  int Mod_id = ue->Mod_id;

  /*
    if (last_slot<2)
    last_slot_emos = last_slot;
    else if (last_slot>9)
    last_slot_emos = last_slot - 8;
    else {
    LOG_E(PHY,"emos rx last_slot_emos %d, last_slot %d\n", last_slot_emos,last_slot);
    mac_xface->macphy_exit("should never happen");
    }
  */

#ifdef EMOS_CHANNEL

  if ((last_slot==10) || (last_slot==11)) {
    for (i=0; i<ue->frame_parms.nb_antennas_rx; i++)
      for (j=0; j<ue->frame_parms.nb_antennas_tx; j++) {
	// first OFDM symbol with pilots
	memcpy(&emos_dump_UE.channel[i][j][(last_slot%2)*2*ue->frame_parms.ofdm_symbol_size],
	       &ue->common_vars.dl_ch_estimates[eNB_id][(j<<1) + i][0],
	       ue->frame_parms.ofdm_symbol_size*sizeof(int));
	// second OFDM symbol with pilots
	memcpy(&emos_dump_UE.channel[i][j][((last_slot%2)*2+1)*ue->frame_parms.ofdm_symbol_size],
	       &ue->common_vars.dl_ch_estimates[eNB_id][(j<<1) + i][(ue->frame_parms.Ncp == 0 ? 4 : 3)*ue->frame_parms.ofdm_symbol_size],
	       ue->frame_parms.ofdm_symbol_size*sizeof(int));
      }
  }

#endif

  if (last_slot==0) {
    emos_dump_UE.timestamp = rt_get_time_ns();
    emos_dump_UE.frame_rx = proc->frame_rx;
    emos_dump_UE.UE_mode = ue->UE_mode[eNB_id];
    emos_dump_UE.mimo_mode = ue->transmission_mode[eNB_id];
    emos_dump_UE.freq_offset = ue->common_vars.freq_offset;
    emos_dump_UE.timing_advance = ue->timing_advance;
    emos_dump_UE.timing_offset  = ue->rx_offset;
    emos_dump_UE.rx_total_gain_dB = ue->rx_total_gain_dB;
    emos_dump_UE.eNb_id = eNB_id;
    memcpy(&emos_dump_UE.PHY_measurements,&measurements,sizeof(PHY_MEASUREMENTS));
  }

  if (last_slot==1) {
    emos_dump_UE.pbch_errors = ue->pbch_vars[eNB_id]->pdu_errors;
    emos_dump_UE.pbch_errors_last = ue->pbch_vars[eNB_id]->pdu_errors_last;
    emos_dump_UE.pbch_errors_conseq = ue->pbch_vars[eNB_id]->pdu_errors_conseq;
    emos_dump_UE.pbch_fer = ue->pbch_vars[eNB_id]->pdu_fer;
  }

  if (last_slot==19) {
    emos_dump_UE.dlsch_errors = ue->dlsch_errors[eNB_id];
    emos_dump_UE.dlsch_errors_last = ue->dlsch_errors_last[eNB_id];
    emos_dump_UE.dlsch_received = ue->dlsch_received[eNB_id];
    emos_dump_UE.dlsch_received_last = ue->dlsch_received_last[eNB_id];
    emos_dump_UE.dlsch_fer = ue->dlsch_fer[eNB_id];
    emos_dump_UE.dlsch_cntl_errors = ue->dlsch_SI_errors[eNB_id];
    emos_dump_UE.dlsch_ra_errors = ue->dlsch_ra_errors[eNB_id];
    emos_dump_UE.total_TBS = ue->total_TBS[eNB_id];
    emos_dump_UE.total_TBS_last = ue->total_TBS_last[eNB_id];
    emos_dump_UE.bitrate = ue->bitrate[eNB_id];
    emos_dump_UE.total_received_bits = ue->total_received_bits[eNB_id];
    emos_dump_UE.pmi_saved = ue->dlsch[eNB_id][0]->pmi_alloc;
    emos_dump_UE.mcs = ue->dlsch[eNB_id][0]->harq_processes[ue->dlsch[eNB_id][0]->current_harq_pid]->mcs;
    emos_dump_UE.use_ia_receiver = openair_daq_vars.use_ia_receiver;

    bytes = rtf_put(CHANSOUNDER_FIFO_MINOR, &emos_dump_UE, sizeof(fifo_dump_emos_UE));

    if (bytes!=sizeof(fifo_dump_emos_UE)) {
      LOG_W(PHY,"[UE  %d] frame %d, slot %d, Problem writing EMOS data to FIFO\n",Mod_id,proc->frame_rx, last_slot);
    } else {
      if (proc->frame_rx%100==0) {
	LOG_I(PHY,"[UE  %d] frame %d, slot %d, Writing %d bytes EMOS data to FIFO\n",Mod_id,proc->frame_rx, last_slot, bytes);
      }
    }
  }

}
#endif


void restart_phy(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc, uint8_t eNB_id,uint8_t abstraction_flag)
{

  //  uint8_t last_slot;
  uint8_t i;
  LOG_I(PHY,"[UE  %d] frame %d, slot %d, restarting PHY!\n",ue->Mod_id,proc->frame_rx,proc->subframe_rx);
  mac_xface->macphy_exit("restart_phy called");
  //   first_run = 1;

  if (abstraction_flag ==0 ) {
    ue->UE_mode[eNB_id] = NOT_SYNCHED;
  } else {
    ue->UE_mode[eNB_id] = PRACH;
    ue->prach_resources[eNB_id]=NULL;
  }

  proc->frame_rx = -1;
  proc->frame_tx = -1;
  //  ue->synch_wait_cnt=0;
  //  ue->sched_cnt=-1;

  ue->pbch_vars[eNB_id]->pdu_errors_conseq=0;
  ue->pbch_vars[eNB_id]->pdu_errors=0;

  ue->pdcch_vars[eNB_id]->dci_errors = 0;
  ue->pdcch_vars[eNB_id]->dci_missed = 0;
  ue->pdcch_vars[eNB_id]->dci_false  = 0;
  ue->pdcch_vars[eNB_id]->dci_received = 0;

  ue->dlsch_errors[eNB_id] = 0;
  ue->dlsch_errors_last[eNB_id] = 0;
  ue->dlsch_received[eNB_id] = 0;
  ue->dlsch_received_last[eNB_id] = 0;
  ue->dlsch_fer[eNB_id] = 0;
  ue->dlsch_SI_received[eNB_id] = 0;
  ue->dlsch_ra_received[eNB_id] = 0;
  ue->dlsch_p_received[eNB_id] = 0;
  ue->dlsch_SI_errors[eNB_id] = 0;
  ue->dlsch_ra_errors[eNB_id] = 0;
  ue->dlsch_p_errors[eNB_id] = 0;

  ue->dlsch_mch_received[eNB_id] = 0;

  for (i=0; i < MAX_MBSFN_AREA ; i ++) {
    ue->dlsch_mch_received_sf[i][eNB_id] = 0;
    ue->dlsch_mcch_received[i][eNB_id] = 0;
    ue->dlsch_mtch_received[i][eNB_id] = 0;
    ue->dlsch_mcch_errors[i][eNB_id] = 0;
    ue->dlsch_mtch_errors[i][eNB_id] = 0;
    ue->dlsch_mcch_trials[i][eNB_id] = 0;
    ue->dlsch_mtch_trials[i][eNB_id] = 0;
  }

  //ue->total_TBS[eNB_id] = 0;
  //ue->total_TBS_last[eNB_id] = 0;
  //ue->bitrate[eNB_id] = 0;
  //ue->total_received_bits[eNB_id] = 0;
}


void ue_pbch_procedures(uint8_t eNB_id,PHY_VARS_UE *ue,UE_rxtx_proc_t *proc, uint8_t abstraction_flag)
{

  //  int i;
  int pbch_tx_ant=0;
  uint8_t pbch_phase;
  uint16_t frame_tx;
  static uint8_t first_run = 1;
  uint8_t pbch_trials = 0;

  DevAssert(ue);

  int frame_rx = proc->frame_rx;
  int subframe_rx = proc->subframe_rx;

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PBCH_PROCEDURES, VCD_FUNCTION_IN);

  pbch_phase=(frame_rx%4);

  if (pbch_phase>=4)
    pbch_phase=0;

  for (pbch_trials=0; pbch_trials<4; pbch_trials++) {
    //for (pbch_phase=0;pbch_phase<4;pbch_phase++) {
    //LOG_I(PHY,"[UE  %d] Frame %d, Trying PBCH %d (NidCell %d, eNB_id %d)\n",ue->Mod_id,frame_rx,pbch_phase,ue->frame_parms.Nid_cell,eNB_id);
    if (abstraction_flag == 0) {
      pbch_tx_ant = rx_pbch(&ue->common_vars,
			    ue->pbch_vars[eNB_id],
			    &ue->frame_parms,
			    eNB_id,
			    ue->frame_parms.mode1_flag==1?SISO:ALAMOUTI,
			    ue->high_speed_flag,
			    pbch_phase);



    }

#ifdef PHY_ABSTRACTION
    else {
      pbch_tx_ant = rx_pbch_emul(ue,
				 eNB_id,
				 pbch_phase);
    }

#endif

    if ((pbch_tx_ant>0) && (pbch_tx_ant<=4)) {
      break;
    }

    pbch_phase++;

    if (pbch_phase>=4)
      pbch_phase=0;
  }



  if ((pbch_tx_ant>0) && (pbch_tx_ant<=4)) {

    if (pbch_tx_ant>2) {
      LOG_W(PHY,"[openair][SCHED][SYNCH] PBCH decoding: pbch_tx_ant>2 not supported\n");
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PBCH_PROCEDURES, VCD_FUNCTION_OUT);
      return;
    }


    ue->pbch_vars[eNB_id]->pdu_errors_conseq = 0;
    frame_tx = (((int)(ue->pbch_vars[eNB_id]->decoded_output[2]&0x03))<<8);
    frame_tx += ((int)(ue->pbch_vars[eNB_id]->decoded_output[1]&0xfc));
    frame_tx += pbch_phase;

    if (ue->mac_enabled==1) {
      mac_xface->dl_phy_sync_success(ue->Mod_id,frame_rx,eNB_id,
				     ue->UE_mode[eNB_id]==NOT_SYNCHED ? 1 : 0);
    }
    
#ifdef EMOS
    //emos_dump_UE.frame_tx = frame_tx;
    //emos_dump_UE.mimo_mode = ue->pbch_vars[eNB_id]->decoded_output[1];
#endif

    if (first_run) {
      first_run = 0;

      proc->frame_rx = (proc->frame_rx & 0xFFFFFC00) | (frame_tx & 0x000003FF);
      proc->frame_tx = proc->frame_rx;
      ue->proc.proc_rxtx[1].frame_rx = proc->frame_rx;
      ue->proc.proc_rxtx[1].frame_tx = proc->frame_tx;
      LOG_I(PHY,"[UE %d] frame %d, subframe %d: Adjusting frame counter (PBCH ant_tx=%d, frame_tx=%d, phase %d, rx_offset %d) => new frame %d\n",
	    ue->Mod_id,
	    frame_rx,
	    subframe_rx,
	    pbch_tx_ant,
	    frame_tx,
	    pbch_phase,
	    ue->rx_offset,
	    proc->frame_rx);
      frame_rx = proc->frame_rx;
      
    } else if (((frame_tx & 0x03FF) != (proc->frame_rx & 0x03FF))) {
      //(pbch_tx_ant != ue->frame_parms.nb_antennas_tx)) {
      LOG_D(PHY,"[UE %d] frame %d, subframe %d: Re-adjusting frame counter (PBCH ant_tx=%d, frame_rx=%d, frame%1024=%d, phase %d).\n",
	    ue->Mod_id,
	    proc->frame_rx,
	    subframe_rx,
	    pbch_tx_ant,
	    frame_tx,
	    frame_rx & 0x03FF,
	    pbch_phase);

      proc->frame_rx = (proc->frame_rx & 0xFFFFFC00) | (frame_tx & 0x000003FF);
      ue->proc.proc_rxtx[1].frame_rx = (proc->frame_rx & 0xFFFFFC00) | (frame_tx & 0x000003FF);
      proc->frame_tx = proc->frame_rx;
      ue->proc.proc_rxtx[1].frame_tx = proc->frame_rx;
      frame_rx = proc->frame_rx;

    }

#ifdef DEBUG_PHY_PROC
    LOG_D(PHY,"[UE %d] frame %d, subframe %d, Received PBCH (MIB): mode1_flag %d, tx_ant %d, frame_tx %d. N_RB_DL %d, phich_duration %d, phich_resource %d/6!\n",
	  ue->Mod_id,
	  frame_rx,
	  subframe_rx,
	  ue->frame_parms.mode1_flag,
	  pbch_tx_ant,
	  frame_tx,
	  ue->frame_parms.N_RB_DL,
	  ue->frame_parms.phich_config_common.phich_duration,
	  ue->frame_parms.phich_config_common.phich_resource);
#endif

  } else { 
    /*
    LOG_E(PHY,"[UE %d] frame %d, subframe %d, Error decoding PBCH!\n",
	  ue->Mod_id,frame_rx, subframe_rx);

    LOG_I(PHY,"[UE %d] rx_offset %d\n",ue->Mod_id,ue->rx_offset);


    write_output("rxsig0.m","rxs0", ue->common_vars.rxdata[0],ue->frame_parms.samples_per_tti,1,1);

    write_output("H00.m","h00",&(ue->common_vars.dl_ch_estimates[0][0][0]),((ue->frame_parms.Ncp==0)?7:6)*(ue->frame_parms.ofdm_symbol_size),1,1);
    write_output("H10.m","h10",&(ue->common_vars.dl_ch_estimates[0][2][0]),((ue->frame_parms.Ncp==0)?7:6)*(ue->frame_parms.ofdm_symbol_size),1,1);

    write_output("rxsigF0.m","rxsF0", ue->common_vars.rxdataF[0],8*ue->frame_parms.ofdm_symbol_size,1,1);
    write_output("PBCH_rxF0_ext.m","pbch0_ext",ue->pbch_vars[0]->rxdataF_ext[0],12*4*6,1,1);
    write_output("PBCH_rxF0_comp.m","pbch0_comp",ue->pbch_vars[0]->rxdataF_comp[0],12*4*6,1,1);
    write_output("PBCH_rxF_llr.m","pbch_llr",ue->pbch_vars[0]->llr,(ue->frame_parms.Ncp==0) ? 1920 : 1728,1,4);
    exit(-1);
    */

    ue->pbch_vars[eNB_id]->pdu_errors_conseq++;
    ue->pbch_vars[eNB_id]->pdu_errors++;
    if (ue->mac_enabled == 1) {
      mac_xface->out_of_sync_ind(ue->Mod_id,frame_rx,eNB_id);
    }
    else{
      if (ue->pbch_vars[eNB_id]->pdu_errors_conseq>=100) {
	LOG_E(PHY,"More that 100 consecutive PBCH errors! Exiting!\n");
	mac_xface->macphy_exit("More that 100 consecutive PBCH errors!");
      }
    }
  }

  if (frame_rx % 100 == 0) {
    ue->pbch_vars[eNB_id]->pdu_fer = ue->pbch_vars[eNB_id]->pdu_errors - ue->pbch_vars[eNB_id]->pdu_errors_last;
    ue->pbch_vars[eNB_id]->pdu_errors_last = ue->pbch_vars[eNB_id]->pdu_errors;
  }

#ifdef DEBUG_PHY_PROC
  LOG_D(PHY,"[UE %d] frame %d, slot %d, PBCH errors = %d, consecutive errors = %d!\n",
	ue->Mod_id,frame_rx, subframe_rx,
	ue->pbch_vars[eNB_id]->pdu_errors,
	ue->pbch_vars[eNB_id]->pdu_errors_conseq);
#endif
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PBCH_PROCEDURES, VCD_FUNCTION_OUT);
}

int ue_pdcch_procedures(uint8_t eNB_id,PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t abstraction_flag)
{

  unsigned int dci_cnt=0, i;

  int frame_rx = proc->frame_rx;
  int subframe_rx = proc->subframe_rx;
  DCI_ALLOC_t dci_alloc_rx[8];


#ifdef PHY_ABSTRACTION
  int CC_id;
  int UE_id;
  uint8_t harq_pid;
#endif

  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PDCCH_PROCEDURES, VCD_FUNCTION_IN);
  start_meas(&ue->dlsch_rx_pdcch_stats);

  //  if (subframe_rx != 5)
  //    return 0;
  if (abstraction_flag == 0)  {

    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_RX_PDCCH, VCD_FUNCTION_IN);
    rx_pdcch(&ue->common_vars,
	     ue->pdcch_vars,
	     &ue->frame_parms,
	     subframe_rx,
	     eNB_id,
	     (ue->frame_parms.mode1_flag == 1) ? SISO : ALAMOUTI,
	     ue->high_speed_flag,
	     ue->is_secondary_ue);


    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_RX_PDCCH, VCD_FUNCTION_OUT);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DCI_DECODING, VCD_FUNCTION_IN);
    dci_cnt = dci_decoding_procedure(ue,
				     dci_alloc_rx,
				     (ue->UE_mode[eNB_id] < PUSCH)? 1 : 0,  // if we're in PUSCH don't listen to common search space,
				     // later when we need paging or RA during connection, update this ...
				     eNB_id,subframe_rx);
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_DCI_DECODING, VCD_FUNCTION_OUT);
    //LOG_D(PHY,"[UE  %d][PUSCH] Frame %d subframe %d PHICH RX\n",ue->Mod_id,frame_rx,subframe_rx);

    if (is_phich_subframe(&ue->frame_parms,subframe_rx)) {
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_RX_PHICH, VCD_FUNCTION_IN);
      rx_phich(ue,proc,
	       subframe_rx,eNB_id);
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_RX_PHICH, VCD_FUNCTION_OUT);
    }
  }

#ifdef PHY_ABSTRACTION
  else {
    for (i=0; i<NB_eNB_INST; i++) {
      for (CC_id=0; CC_id<MAX_NUM_CCs; CC_id++)
	if (PHY_vars_eNB_g[i][CC_id]->frame_parms.Nid_cell == ue->frame_parms.Nid_cell)
	  break;

      if (CC_id < MAX_NUM_CCs)
	break;
    }

    if (i==NB_eNB_INST) {
      LOG_E(PHY,"[UE  %d] phy_procedures_lte_ue.c: FATAL : Could not find attached eNB for DCI emulation (Nid_cell %d)!!!!\n",ue->Mod_id,ue->frame_parms.Nid_cell);
      mac_xface->macphy_exit("Could not find attached eNB for DCI emulation");
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PDCCH_PROCEDURES, VCD_FUNCTION_OUT);
      return(-1);
    }

    LOG_D(PHY,"Calling dci_decoding_proc_emul ...\n");
    dci_cnt = dci_decoding_procedure_emul(ue->pdcch_vars,
					  PHY_vars_eNB_g[i][CC_id]->num_ue_spec_dci[subframe_rx&1],
					  PHY_vars_eNB_g[i][CC_id]->num_common_dci[subframe_rx&1],
					  PHY_vars_eNB_g[i][CC_id]->dci_alloc[subframe_rx&1],
					  dci_alloc_rx,
					  eNB_id);
    //    printf("DCI: dci_cnt %d\n",dci_cnt);
    UE_id = (uint32_t)find_ue((int16_t)ue->pdcch_vars[eNB_id]->crnti,PHY_vars_eNB_g[i][CC_id]);

    if (UE_id>=0) {
      //      printf("Checking PHICH for UE  %d (eNB %d)\n",UE_id,i);
      if (is_phich_subframe(&ue->frame_parms,subframe_rx)) {
	harq_pid = phich_subframe_to_harq_pid(&ue->frame_parms,frame_rx,subframe_rx);

	if (ue->ulsch[eNB_id]->harq_processes[harq_pid]->status == ACTIVE) {
	  // ue->ulsch[eNB_id]->harq_processes[harq_pid]->phich_ACK=1;
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->subframe_scheduling_flag =0;
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->status = IDLE;
	  ue->ulsch_Msg3_active[eNB_id] = 0;
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->round = 0;
	  LOG_D(PHY,"Msg3 inactive\n");

	} // harq_pid is ACTIVE
      } // This is a PHICH subframe
    } // UE_id exists
  }

#endif


  LOG_D(PHY,"[UE  %d] Frame %d, subframe %d, Mode %s: DCI found %i\n",ue->Mod_id,frame_rx,subframe_rx,mode_string[ue->UE_mode[eNB_id]],dci_cnt);

 

  ue->pdcch_vars[eNB_id]->dci_received += dci_cnt;

#ifdef EMOS
  //emos_dump_UE.dci_cnt[subframe_rx] = dci_cnt;
#endif

  for (i=0; i<dci_cnt; i++) {



    if ((ue->UE_mode[eNB_id]>PRACH) &&
	(dci_alloc_rx[i].rnti == ue->pdcch_vars[eNB_id]->crnti) &&
	(dci_alloc_rx[i].format != format0)) {
      

      LOG_D(PHY,"[UE  %d][DCI][PDSCH %x] frame %d, subframe %d: format %d, num_pdcch_symbols %d, nCCE %d, total CCEs %d\n",
	    ue->Mod_id,dci_alloc_rx[i].rnti,
	    frame_rx,subframe_rx,
	    dci_alloc_rx[i].format,
	    ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
	    ue->pdcch_vars[eNB_id]->nCCE[subframe_rx],
	    get_nCCE(3,&ue->frame_parms,get_mi(&ue->frame_parms,0)));




      
      //      dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
      if ((ue->UE_mode[eNB_id] > PRACH) &&
	  (generate_ue_dlsch_params_from_dci(frame_rx,
					     subframe_rx,
					     (void *)&dci_alloc_rx[i].dci_pdu,
					     ue->pdcch_vars[eNB_id]->crnti,
					     dci_alloc_rx[i].format,
					     ue->dlsch[eNB_id],
					     &ue->frame_parms,
					     ue->pdsch_config_dedicated,
					     SI_RNTI,
					     0,
					     P_RNTI)==0)) {

	ue->dlsch_received[eNB_id]++;
	
#ifdef DEBUG_PHY_PROC
	LOG_D(PHY,"[UE  %d] Generated UE DLSCH C_RNTI format %d\n",ue->Mod_id,dci_alloc_rx[i].format);
	dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
	LOG_D(PHY,"[UE %d] *********** dlsch->active in subframe %d=> %d\n",ue->Mod_id,subframe_rx,ue->dlsch[eNB_id][0]->active);
#endif
	
	// we received a CRNTI, so we're in PUSCH
	if (ue->UE_mode[eNB_id] != PUSCH) {
#ifdef DEBUG_PHY_PROC
	  LOG_D(PHY,"[UE  %d] Frame %d, subframe %d: Received DCI with CRNTI %x => Mode PUSCH\n",ue->Mod_id,frame_rx,subframe_rx,ue->pdcch_vars[eNB_id]->crnti);
#endif
	  //dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
	  ue->UE_mode[eNB_id] = PUSCH;
	  //mac_xface->macphy_exit("Connected. Exiting\n");
	}
      } else {
	LOG_E(PHY,"[UE  %d] Frame %d, subframe %d: Problem in DCI!\n",ue->Mod_id,frame_rx,subframe_rx);
	dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
      }
    }

    else if ((dci_alloc_rx[i].rnti == SI_RNTI) &&
	     ((dci_alloc_rx[i].format == format1A) || (dci_alloc_rx[i].format == format1C))) {

#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d] subframe %d: Found rnti %x, format 1%s, dci_cnt %d\n",ue->Mod_id,subframe_rx,dci_alloc_rx[i].rnti,dci_alloc_rx[i].format==format1A?"A":"C",i);
#endif


      if (generate_ue_dlsch_params_from_dci(frame_rx,
					    subframe_rx,
					    (void *)&dci_alloc_rx[i].dci_pdu,
					    SI_RNTI,
					    dci_alloc_rx[i].format,
					    &ue->dlsch_SI[eNB_id],
					    &ue->frame_parms,
					    ue->pdsch_config_dedicated,
					    SI_RNTI,
					    0,
					    P_RNTI)==0) {

	ue->dlsch_SI_received[eNB_id]++;
 

	LOG_D(PHY,"[UE  %d] Frame %d, subframe %d : Generate UE DLSCH SI_RNTI format 1%s\n",ue->Mod_id,frame_rx,subframe_rx,dci_alloc_rx[i].format==format1A?"A":"C");
	//dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);

      }
    }

    else if ((dci_alloc_rx[i].rnti == P_RNTI) &&
	     ((dci_alloc_rx[i].format == format1A) || (dci_alloc_rx[i].format == format1C))) {

#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d] subframe %d: Found rnti %x, format 1%s, dci_cnt %d\n",ue->Mod_id,subframe_rx,dci_alloc_rx[i].rnti,dci_alloc_rx[i].format==format1A?"A":"C",i);
#endif


      if (generate_ue_dlsch_params_from_dci(frame_rx,
					    subframe_rx,
					    (void *)&dci_alloc_rx[i].dci_pdu,
					    SI_RNTI,
					    dci_alloc_rx[i].format,
					    &ue->dlsch_SI[eNB_id],
					    &ue->frame_parms,
					    ue->pdsch_config_dedicated,
					    SI_RNTI,
					    0,
					    P_RNTI)==0) {

	ue->dlsch_p_received[eNB_id]++;
 

	LOG_D(PHY,"[UE  %d] Frame %d, subframe %d : Generate UE DLSCH P_RNTI format 1%s\n",ue->Mod_id,frame_rx,subframe_rx,dci_alloc_rx[i].format==format1A?"A":"C");
	//dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);

      }
    }

    else if ((ue->prach_resources[eNB_id]) &&
	     (dci_alloc_rx[i].rnti == ue->prach_resources[eNB_id]->ra_RNTI) &&
	     (dci_alloc_rx[i].format == format1A)) {

#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d][RAPROC] subframe %d: Found RA rnti %x, format 1A, dci_cnt %d\n",ue->Mod_id,subframe_rx,dci_alloc_rx[i].rnti,i);

      //if (((frame_rx%100) == 0) || (frame_rx < 20))
      //dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
      //mac_xface->macphy_exit("so far so good...\n");
#endif


      if (generate_ue_dlsch_params_from_dci(frame_rx,
					    subframe_rx,
					    (DCI1A_5MHz_TDD_1_6_t *)&dci_alloc_rx[i].dci_pdu,
					    ue->prach_resources[eNB_id]->ra_RNTI,
					    format1A,
					    &ue->dlsch_ra[eNB_id],
					    &ue->frame_parms,
					    ue->pdsch_config_dedicated,
					    SI_RNTI,
					    ue->prach_resources[eNB_id]->ra_RNTI,
					    P_RNTI)==0) {

	ue->dlsch_ra_received[eNB_id]++;

#ifdef DEBUG_PHY_PROC
	LOG_D(PHY,"[UE  %d] Generate UE DLSCH RA_RNTI format 1A, rb_alloc %x, dlsch_ra[eNB_id] %p\n",
	      ue->Mod_id,ue->dlsch_ra[eNB_id]->harq_processes[0]->rb_alloc_even[0],ue->dlsch_ra[eNB_id]);
#endif
      }
    } else if( (dci_alloc_rx[i].rnti == ue->pdcch_vars[eNB_id]->crnti) &&
	       (dci_alloc_rx[i].format == format0)) {
#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d][PUSCH] Frame %d subframe %d: Found rnti %x, format 0, dci_cnt %d\n",
	    ue->Mod_id,frame_rx,subframe_rx,dci_alloc_rx[i].rnti,i);
#endif

      ue->ulsch_no_allocation_counter[eNB_id] = 0;
      //dump_dci(&ue->frame_parms,&dci_alloc_rx[i]);

      if ((ue->UE_mode[eNB_id] > PRACH) &&
	  (generate_ue_ulsch_params_from_dci((void *)&dci_alloc_rx[i].dci_pdu,
					     ue->pdcch_vars[eNB_id]->crnti,
					     subframe_rx,
					     format0,
					     ue,
					     proc,
					     SI_RNTI,
					     0,
					     P_RNTI,
					     CBA_RNTI,
					     eNB_id,
					     0)==0)) {

#ifdef DEBUG_PHY_PROC
	LOG_D(PHY,"[UE  %d] Generate UE ULSCH C_RNTI format 0 (subframe %d)\n",ue->Mod_id,subframe_rx);
#endif

      }
    } else if( (dci_alloc_rx[i].rnti == ue->ulsch[eNB_id]->cba_rnti[0]) &&
	       (dci_alloc_rx[i].format == format0)) {
      // UE could belong to more than one CBA group
      // ue->Mod_id%ue->ulsch[eNB_id]->num_active_cba_groups]
#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d][PUSCH] Frame %d subframe %d: Found cba rnti %x, format 0, dci_cnt %d\n",
	    ue->Mod_id,frame_rx,subframe_rx,dci_alloc_rx[i].rnti,i);
      /*
	if (((frame_rx%100) == 0) || (frame_rx < 20))
	dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
      */
#endif

      ue->ulsch_no_allocation_counter[eNB_id] = 0;
      //dump_dci(&ue->frame_parms,&dci_alloc_rx[i]);

      if ((ue->UE_mode[eNB_id] > PRACH) &&
	  (generate_ue_ulsch_params_from_dci((void *)&dci_alloc_rx[i].dci_pdu,
					     ue->ulsch[eNB_id]->cba_rnti[0],
					     subframe_rx,
					     format0,
					     ue,
					     proc,
					     SI_RNTI,
					     0,
					     P_RNTI,
					     CBA_RNTI,
					     eNB_id,
					     0)==0)) {

#ifdef DEBUG_PHY_PROC
	LOG_D(PHY,"[UE  %d] Generate UE ULSCH CBA_RNTI format 0 (subframe %d)\n",ue->Mod_id,subframe_rx);
#endif
	ue->ulsch[eNB_id]->num_cba_dci[(subframe_rx+4)%10]++;
      }
    }

    else {
#ifdef DEBUG_PHY_PROC
      LOG_D(PHY,"[UE  %d] frame %d, subframe %d: received DCI %d with RNTI=%x (C-RNTI:%x, CBA_RNTI %x) and format %d!\n",ue->Mod_id,frame_rx,subframe_rx,i,dci_alloc_rx[i].rnti,
	    ue->pdcch_vars[eNB_id]->crnti,
	    ue->ulsch[eNB_id]->cba_rnti[0],
	    dci_alloc_rx[i].format);
      //      dump_dci(&ue->frame_parms, &dci_alloc_rx[i]);
#endif
    }

  }

  stop_meas(&ue->dlsch_rx_pdcch_stats);
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_PDCCH_PROCEDURES, VCD_FUNCTION_OUT);
  return(0);
}


void ue_pmch_procedures(PHY_VARS_UE *ue, UE_rxtx_proc_t *proc,int eNB_id,int abstraction_flag) {

  int subframe_rx = proc->subframe_rx;
  int frame_rx = proc->frame_rx;
  int pmch_mcs=-1;
  int CC_id = ue->CC_id;
  uint8_t sync_area=255;
  uint8_t mcch_active;
  int l;
  int ret=0;

  if (is_pmch_subframe(frame_rx,subframe_rx,&ue->frame_parms)) {
    LOG_D(PHY,"ue calling pmch subframe ..\n ");
    
    LOG_D(PHY,"[UE %d] Frame %d, subframe %d: Querying for PMCH demodulation\n",
	  ue->Mod_id,(subframe_rx==9?-1:0)+frame_rx,subframe_rx);
#ifdef Rel10
    pmch_mcs = mac_xface->ue_query_mch(ue->Mod_id,
				       CC_id,
				       frame_rx,
				       subframe_rx,
				       eNB_id,
				       &sync_area,
				       &mcch_active);
    
#else
    pmch_mcs=-1;
#endif
    
    if (pmch_mcs>=0) {
      LOG_D(PHY,"[UE %d] Frame %d, subframe %d: Programming PMCH demodulation for mcs %d\n",ue->Mod_id,frame_rx,subframe_rx,pmch_mcs);
      fill_UE_dlsch_MCH(ue,pmch_mcs,1,0,0);
      
      if (abstraction_flag == 0 ) {
	for (l=2; l<12; l++) {
	  
	  slot_fep_mbsfn(ue,
			 l,
			 subframe_rx,
			 0,0);//ue->rx_offset,0);
	}

	for (l=2; l<12; l++) {
	  rx_pmch(ue,
		  0,
		  subframe_rx,
		  l);
	}


	ue->dlsch_MCH[0]->harq_processes[0]->G = get_G(&ue->frame_parms,
						       ue->dlsch_MCH[0]->harq_processes[0]->nb_rb,
						       ue->dlsch_MCH[0]->harq_processes[0]->rb_alloc_even,
						       ue->dlsch_MCH[0]->harq_processes[0]->Qm,
						       1,
						       2,
						       frame_rx,subframe_rx);
	
	dlsch_unscrambling(&ue->frame_parms,1,ue->dlsch_MCH[0],
			   ue->dlsch_MCH[0]->harq_processes[0]->G,
			   ue->pdsch_vars_MCH[0]->llr[0],0,subframe_rx<<1);
	
	ret = dlsch_decoding(ue,
			     ue->pdsch_vars_MCH[0]->llr[0],
			     &ue->frame_parms,
			     ue->dlsch_MCH[0],
			     ue->dlsch_MCH[0]->harq_processes[0],
			     subframe_rx,
			     0,
			     0,1);
      } else { // abstraction
#ifdef PHY_ABSTRACTION
	ret = dlsch_decoding_emul(ue,
				  subframe_rx,
				  5, // PMCH
				  eNB_id);
#endif
      }
      
      if (mcch_active == 1)
	ue->dlsch_mcch_trials[sync_area][0]++;
      else
	ue->dlsch_mtch_trials[sync_area][0]++;
      
      if (ret == (1+ue->dlsch_MCH[0]->max_turbo_iterations)) {
	if (mcch_active == 1)
	  ue->dlsch_mcch_errors[sync_area][0]++;
	else
	  ue->dlsch_mtch_errors[sync_area][0]++;
	
	LOG_D(PHY,"[UE %d] Frame %d, subframe %d: PMCH in error (%d,%d), not passing to L2 (TBS %d, iter %d,G %d)\n",
	      frame_rx,subframe_rx,
	      ue->dlsch_mcch_errors[sync_area][0],
	      ue->dlsch_mtch_errors[sync_area][0],
	      ue->dlsch_MCH[0]->harq_processes[0]->TBS>>3,
	      ue->dlsch_MCH[0]->max_turbo_iterations,
	      ue->dlsch_MCH[0]->harq_processes[0]->G);
	dump_mch(ue,0,ue->dlsch_MCH[0]->harq_processes[0]->G,subframe_rx);
#ifdef DEBUG_DLSCH
	
	for (int i=0; i<ue->dlsch_MCH[0]->harq_processes[0]->TBS>>3; i++) {
	  LOG_T(PHY,"%02x.",ue->dlsch_MCH[0]->harq_processes[0]->c[0][i]);
	}
	
	LOG_T(PHY,"\n");
#endif
	
	if (subframe_rx==9)
	  mac_xface->macphy_exit("Why are we exiting here?");
      } else { // decoding successful
#ifdef Rel10
	
	if (mcch_active == 1) {
	  mac_xface->ue_send_mch_sdu(ue->Mod_id,
				     CC_id,
				     frame_rx,
				     ue->dlsch_MCH[0]->harq_processes[0]->b,
				     ue->dlsch_MCH[0]->harq_processes[0]->TBS>>3,
				     eNB_id,// not relevant in eMBMS context
				     sync_area);
	  ue->dlsch_mcch_received[sync_area][0]++;
	  
	  
	  if (ue->dlsch_mch_received_sf[subframe_rx%5][0] == 1 ) {
	    ue->dlsch_mch_received_sf[subframe_rx%5][0]=0;
	  } else {
	    ue->dlsch_mch_received[0]+=1;
	    ue->dlsch_mch_received_sf[subframe_rx][0]=1;
	  }
	  
	  
	}
#endif // Rel10
      } // decoding sucessful
    } // pmch_mcs>=0
  } // is_pmch_subframe=true
}

void ue_pdsch_procedures(PHY_VARS_UE *ue, UE_rxtx_proc_t *proc, int eNB_id, PDSCH_t pdsch, LTE_UE_DLSCH_t *dlsch0, LTE_UE_DLSCH_t *dlsch1, int s0, int s1, int abstraction_flag) {

  int subframe_rx = proc->subframe_rx;
  int m;
  int harq_pid;
  int i_mod,eNB_id_i,dual_stream_UE;
  int first_symbol_flag=0;

  if (dlsch0->active == 0)
    return;

  for (m=s0;m<=s1;m++) {

    if (dlsch0 && (!dlsch1))  {
      harq_pid = dlsch0->current_harq_pid;
      LOG_D(PHY,"[UE %d] PDSCH active in subframe %d (%d), harq_pid %d\n",ue->Mod_id,subframe_rx,harq_pid);
	    
      if ((pdsch==PDSCH) && 
	  (ue->transmission_mode[eNB_id] == 5) &&
	  (dlsch0->harq_processes[harq_pid]->dl_power_off==0) &&
	  (ue->use_ia_receiver ==1)) {
	dual_stream_UE = 1;
	eNB_id_i = ue->n_connected_eNB;
	i_mod =  dlsch0->harq_processes[harq_pid]->Qm;
      } else {
	dual_stream_UE = 0;
	eNB_id_i = eNB_id+1;
	i_mod = 0;
      }
      
      if ((m==s0) && (m<4))
	first_symbol_flag = 1;
      else
	first_symbol_flag = 0;

      start_meas(&ue->dlsch_llr_stats);
      // process DLSCH received in first slot
      rx_pdsch(ue,
	       pdsch,
	       eNB_id,
	       eNB_id_i,
	       subframe_rx,  // subframe,
	       m,
	       first_symbol_flag,
	       dual_stream_UE,
	       i_mod,
	       dlsch0->current_harq_pid);
      stop_meas(&ue->dlsch_llr_stats);
    } // CRNTI active
  }
} 

void process_rar(PHY_VARS_UE *ue, UE_rxtx_proc_t *proc, int eNB_id, runmode_t mode, int abstraction_flag) {

  int frame_rx = proc->frame_rx;
  int subframe_rx = proc->subframe_rx;
  int timing_advance;
  LTE_UE_DLSCH_t *dlsch0 = ue->dlsch_ra[eNB_id];
  int harq_pid = 0;
  uint8_t *rar;

  LOG_D(PHY,"[UE  %d][RAPROC] Frame %d subframe %d Received RAR  mode %d\n",
	ue->Mod_id,
	frame_rx,
	subframe_rx, ue->UE_mode[eNB_id]);
  
	
  if (ue->mac_enabled == 1) {
    if ((ue->UE_mode[eNB_id] != PUSCH) && 
	(ue->prach_resources[eNB_id]->Msg3!=NULL)) {
      LOG_D(PHY,"[UE  %d][RAPROC] Frame %d subframe %d Invoking MAC for RAR (current preamble %d)\n",
	    ue->Mod_id,frame_rx,
	    subframe_rx,
	    ue->prach_resources[eNB_id]->ra_PreambleIndex);
      
      timing_advance = mac_xface->ue_process_rar(ue->Mod_id,
						 ue->CC_id,
						 frame_rx,
						 dlsch0->harq_processes[0]->b,
						 &ue->pdcch_vars[eNB_id]->crnti,
						 ue->prach_resources[eNB_id]->ra_PreambleIndex);
      
	    
      if (timing_advance!=0xffff) {
	      
	LOG_D(PHY,"[UE  %d][RAPROC] Frame %d subframe %d Got rnti %x and timing advance %d from RAR\n",
	      ue->Mod_id,
	      frame_rx,
	      subframe_rx,
	      ue->pdcch_vars[eNB_id]->crnti,
	      timing_advance);
	      
	//timing_advance = 0;
	process_timing_advance_rar(ue,proc,timing_advance);
	      
	if (mode!=debug_prach) {
	  ue->ulsch_Msg3_active[eNB_id]=1;
	  get_Msg3_alloc(&ue->frame_parms,
			 subframe_rx,
			 frame_rx,
			 &ue->ulsch_Msg3_frame[eNB_id],
			 &ue->ulsch_Msg3_subframe[eNB_id]);
	  
	  LOG_D(PHY,"[UE  %d][RAPROC] Got Msg3_alloc Frame %d subframe %d: Msg3_frame %d, Msg3_subframe %d\n",
		ue->Mod_id,
		frame_rx,
		subframe_rx,
		ue->ulsch_Msg3_frame[eNB_id],
		ue->ulsch_Msg3_subframe[eNB_id]);
	  harq_pid = subframe2harq_pid(&ue->frame_parms,
				       ue->ulsch_Msg3_frame[eNB_id],
				       ue->ulsch_Msg3_subframe[eNB_id]);
	  ue->ulsch[eNB_id]->harq_processes[harq_pid]->round = 0;
	  
	  ue->UE_mode[eNB_id] = RA_RESPONSE;
	  //      ue->Msg3_timer[eNB_id] = 10;
	  ue->ulsch[eNB_id]->power_offset = 6;
	  ue->ulsch_no_allocation_counter[eNB_id] = 0;
	}
      } else { // PRACH preamble doesn't match RAR
	LOG_W(PHY,"[UE  %d][RAPROC] Received RAR preamble (%d) doesn't match !!!\n",
	      ue->Mod_id,
	      ue->prach_resources[eNB_id]->ra_PreambleIndex);
      }
    } // mode != PUSCH
  }
  else {
    rar = dlsch0->harq_processes[0]->b+1;
    timing_advance = ((((uint16_t)(rar[0]&0x7f))<<4) + (rar[1]>>4));
    process_timing_advance_rar(ue,proc,timing_advance);
  }
  
}

void ue_dlsch_procedures(PHY_VARS_UE *ue, 
			 UE_rxtx_proc_t *proc, 
			 int eNB_id,
			 PDSCH_t pdsch, 
			 LTE_UE_DLSCH_t *dlsch0, 
			 LTE_UE_DLSCH_t *dlsch1, 
			 int *dlsch_errors, 
			 runmode_t mode, 
			 int abstraction_flag) {

  int harq_pid;
  int frame_rx = proc->frame_rx;
  int subframe_rx = proc->subframe_rx;
  int ret=0;
  int CC_id = ue->CC_id;
  LTE_UE_PDSCH *pdsch_vars;

  if (dlsch0 && (!dlsch1)) {
    switch (pdsch) {
    case SI_PDSCH:
      pdsch_vars = ue->pdsch_vars_SI[eNB_id];
      break;
    case RA_PDSCH:
      pdsch_vars = ue->pdsch_vars_ra[eNB_id];
      break;
    case P_PDSCH:
      pdsch_vars = ue->pdsch_vars_p[eNB_id];
      break;
    case PDSCH:
      pdsch_vars = ue->pdsch_vars[eNB_id];
      break;
    case PMCH:
    case PDSCH1:
      LOG_E(PHY,"Illegal PDSCH %d for ue_pdsch_procedures\n",pdsch);
      pdsch_vars = NULL;
      return;
      break;
    default:
      pdsch_vars = NULL;
      return;
      break;

    }
  
    harq_pid = dlsch0->current_harq_pid;

    if (frame_rx < *dlsch_errors)
      *dlsch_errors=0;

    if (pdsch==RA_PDSCH) {
      if (ue->prach_resources[eNB_id]!=NULL)
	dlsch0->rnti = ue->prach_resources[eNB_id]->ra_RNTI;
      else {
	LOG_E(PHY,"[UE %d] Frame %d, subframe %d: FATAL, prach_resources is NULL\n",ue->Mod_id,frame_rx,subframe_rx);
	mac_xface->macphy_exit("prach_resources is NULL");
	return;
      }
    }

    if (abstraction_flag == 0) {

      dlsch0->harq_processes[harq_pid]->G = get_G(&ue->frame_parms,
						  dlsch0->harq_processes[harq_pid]->nb_rb,
						  dlsch0->harq_processes[harq_pid]->rb_alloc_even,
						  dlsch0->harq_processes[harq_pid]->Qm,
						  dlsch0->harq_processes[harq_pid]->Nl,
						  ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
						  frame_rx,subframe_rx);
      start_meas(&ue->dlsch_unscrambling_stats);
      dlsch_unscrambling(&ue->frame_parms,
			 0,
			 dlsch0,
			 dlsch0->harq_processes[harq_pid]->G,
			 pdsch_vars->llr[0],
			 0,
			 subframe_rx<<1);
      stop_meas(&ue->dlsch_unscrambling_stats);
      
      start_meas(&ue->dlsch_decoding_stats);
      ret = dlsch_decoding(ue,
			   pdsch_vars->llr[0],
			   &ue->frame_parms,
			   dlsch0,
			   dlsch0->harq_processes[harq_pid],
			   subframe_rx,
			   harq_pid,
			   pdsch==PDSCH?1:0,
			   dlsch0->harq_processes[harq_pid]->nb_rb>10?1:0);
      stop_meas(&ue->dlsch_decoding_stats);
    }
	
    else {
      LOG_D(PHY,"Calling dlsch_decoding_emul ...\n");
#ifdef PHY_ABSTRACTION
      ret = dlsch_decoding_emul(ue,
				subframe_rx,
				pdsch,
				eNB_id);
#endif
    }
	
    if (ret == (1+dlsch0->max_turbo_iterations)) {
      *dlsch_errors=*dlsch_errors+1;
      

      LOG_D(PHY,"[UE  %d][PDSCH %x/%d] Frame %d subframe %d DLSCH in error (rv %d,mcs %d,TBS %d)\n",
	    ue->Mod_id,dlsch0->rnti,
	    harq_pid,frame_rx,subframe_rx,
	    dlsch0->harq_processes[harq_pid]->rvidx,
	    dlsch0->harq_processes[harq_pid]->mcs,
	    dlsch0->harq_processes[harq_pid]->TBS);
      

    } else {
      LOG_D(PHY,"[UE  %d][PDSCH %x/%d] Frame %d subframe %d: Received DLSCH (rv %d,mcs %d,TBS %d)\n",
	    ue->Mod_id,dlsch0->rnti,
	    harq_pid,frame_rx,subframe_rx,
	    dlsch0->harq_processes[harq_pid]->rvidx,
	    dlsch0->harq_processes[harq_pid]->mcs,
	    dlsch0->harq_processes[harq_pid]->TBS);

#ifdef DEBUG_DLSCH
      int j;
      LOG_D(PHY,"dlsch harq_pid %d (rx): \n",dlsch0->current_harq_pid);
      
      for (j=0; j<dlsch0->harq_processes[dlsch0->current_harq_pid]->TBS>>3; j++)
	LOG_T(PHY,"%x.",dlsch0->harq_processes[dlsch0->current_harq_pid]->b[j]);
      
      LOG_T(PHY,"\n");
#endif

      
      if (ue->mac_enabled == 1) {
	switch (pdsch) {
	case PDSCH:
	  mac_xface->ue_send_sdu(ue->Mod_id,
				 CC_id,
				 frame_rx,
				 dlsch0->harq_processes[dlsch0->current_harq_pid]->b,
				 dlsch0->harq_processes[dlsch0->current_harq_pid]->TBS>>3,
				 eNB_id);
	  break;
	case SI_PDSCH:
	  mac_xface->ue_decode_si(ue->Mod_id,
				  CC_id,
				  frame_rx,
				  eNB_id,
				  ue->dlsch_SI[eNB_id]->harq_processes[0]->b,
				  ue->dlsch_SI[eNB_id]->harq_processes[0]->TBS>>3);
	  break;
	case P_PDSCH:
	  mac_xface->ue_decode_p(ue->Mod_id,
				 CC_id,
				 frame_rx,
				 eNB_id,
				 ue->dlsch_SI[eNB_id]->harq_processes[0]->b,
				 ue->dlsch_SI[eNB_id]->harq_processes[0]->TBS>>3);
	  break;
	case RA_PDSCH:
	  process_rar(ue,proc,eNB_id,mode,abstraction_flag);
	  break;
	case PDSCH1:
	  LOG_E(PHY,"Shouldn't have PDSCH1 yet, come back later\n");
	  AssertFatal(1==0,"exiting");
	  break;
	case PMCH:
	  LOG_E(PHY,"Shouldn't have PMCH here\n");
	  AssertFatal(1==0,"exiting");
	  break;
	}
      }
      ue->total_TBS[eNB_id] =  ue->total_TBS[eNB_id] +
	dlsch0->harq_processes[dlsch0->current_harq_pid]->TBS;
      ue->total_received_bits[eNB_id] = ue->total_TBS[eNB_id] +
	dlsch0->harq_processes[dlsch0->current_harq_pid]->TBS;
    }
  
  
      
#ifdef DEBUG_PHY_PROC
    LOG_D(PHY,"[UE  %d][PDSCH %x/%d] Frame %d subframe %d: PDSCH/DLSCH decoding iter %d (mcs %d, rv %d, TBS %d)\n",
	  ue->Mod_id,
	  dlsch0->rnti,harq_pid,
	  frame_rx,subframe_rx,ret,
	  dlsch0->harq_processes[harq_pid]->mcs,
	  dlsch0->harq_processes[harq_pid]->rvidx,
	  dlsch0->harq_processes[harq_pid]->TBS);
    
    if (frame_rx%100==0) {
      LOG_D(PHY,"[UE  %d][PDSCH %x] Frame %d subframe %d dlsch_errors %d, dlsch_received %d, dlsch_fer %d, current_dlsch_cqi %d\n",
	    ue->Mod_id,dlsch0->rnti,
	    frame_rx,subframe_rx,
	    ue->dlsch_errors[eNB_id],
	    ue->dlsch_received[eNB_id],
	    ue->dlsch_fer[eNB_id],
	    ue->measurements.wideband_cqi_tot[eNB_id]);
    }
    
#endif

  }


}
int phy_procedures_UE_RX(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag,runmode_t mode,
			 relaying_type_t r_type,PHY_VARS_RN *phy_vars_rn) {
 
  int l,l2;
  int pilot1;
  int pmch_flag=0;
  int frame_rx = proc->frame_rx;
  int subframe_rx = proc->subframe_rx;



  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_RX, VCD_FUNCTION_IN);


  start_meas(&ue->phy_proc_rx);

  pmch_flag = is_pmch_subframe(frame_rx,subframe_rx,&ue->frame_parms) ? 1 : 0;


  // deactivate reception until we scan pdcch
  if (ue->dlsch[eNB_id][0])
    ue->dlsch[eNB_id][0]->active = 0;
  if (ue->dlsch[eNB_id][1])
    ue->dlsch[eNB_id][1]->active = 0;

  if (ue->dlsch_SI[eNB_id])
    ue->dlsch_SI[eNB_id]->active = 0;
  if (ue->dlsch_p[eNB_id])
    ue->dlsch_p[eNB_id]->active = 0;
  if (ue->dlsch_ra[eNB_id])
    ue->dlsch_ra[eNB_id]->active = 0;

  
#ifdef DEBUG_PHY_PROC
  LOG_D(PHY,"[%s %d] Frame %d subframe %d: Doing phy_procedures_UE_RX (%d)\n",
	(r_type == multicast_relay) ? "RN/UE" : "UE",
	ue->Mod_id,frame_rx, subframe_rx);
#endif

  if (ue->frame_parms.Ncp == 0) {  // normal prefix
    pilot1 = 4;
  } else { // extended prefix
    pilot1 = 3;
  }
  
  
  if (subframe_select(&ue->frame_parms,subframe_rx) == SF_S) { // S-subframe, do first 5 symbols only
    l2 = 5;
  } else if (pmch_flag == 1) { // do first 2 symbols only
    l2 = 1;
  } else { // normal subframe, last symbol to be processed is the first of the second slot
    l2 = (ue->frame_parms.symbols_per_tti/2)-1;
  }
  
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  // RX processing of symbols l=1...l2 (l=0 is done in last scheduling epoch)
  //++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  
  for (l=1; l<=l2; l++) {
    if (abstraction_flag == 0) {
      start_meas(&ue->ofdm_demod_stats);
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP, VCD_FUNCTION_IN);
      slot_fep(ue,
	       l,
	       (subframe_rx<<1),
	       ue->rx_offset,
	       0,
	       0);
      VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP, VCD_FUNCTION_OUT);
      stop_meas(&ue->ofdm_demod_stats);
    }
    
    ue_measurement_procedures(l-1,ue,proc,eNB_id,abstraction_flag,mode);
    if ((l==pilot1) ||
	((pmch_flag==1)&(l==l2)))  {
      LOG_D(PHY,"[UE  %d] Frame %d: Calling pdcch procedures (eNB %d)\n",ue->Mod_id,frame_rx,eNB_id);
      
      if (ue_pdcch_procedures(eNB_id,ue,proc,abstraction_flag) == -1) {
	LOG_E(PHY,"[UE  %d] Frame %d, subframe %d: Error in pdcch procedures\n",ue->Mod_id,frame_rx,subframe_rx);
	return(-1);
      }
      LOG_D(PHY,"num_pdcch_symbols %d\n",ue->pdcch_vars[eNB_id]->num_pdcch_symbols);
    }
    
  } // for l=1..l2
  ue_measurement_procedures(l-1,ue,proc,eNB_id,abstraction_flag,mode);  
  
    // If this is PMCH, call procedures and return
  if (pmch_flag == 1) {
    ue_pmch_procedures(ue,proc,eNB_id,abstraction_flag);
    return 0;
  }

  slot_fep(ue,
	   0,
	   1+(subframe_rx<<1),
	   ue->rx_offset,
	   0,
	   0);

  // first slot has been processed (FFTs + Channel Estimation, PCFICH/PHICH/PDCCH)
 
  // do procedures for C-RNTI
  if (ue->dlsch[eNB_id][0]->active == 1) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			PDSCH,
			ue->dlsch[eNB_id][0],
			NULL,
			ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
			ue->frame_parms.symbols_per_tti>>1,
			abstraction_flag);
  }
  // do procedures for SI-RNTI
  if ((ue->dlsch_SI[eNB_id]) && (ue->dlsch_SI[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			SI_PDSCH,
			ue->dlsch_SI[eNB_id],
			NULL,
			ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
			ue->frame_parms.symbols_per_tti>>1,
			abstraction_flag);
  }

  // do procedures for SI-RNTI
  if ((ue->dlsch_p[eNB_id]) && (ue->dlsch_p[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			P_PDSCH,
			ue->dlsch_p[eNB_id],
			NULL,
			ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
			ue->frame_parms.symbols_per_tti>>1,
			abstraction_flag);
  }

  // do procedures for RA-RNTI
  if ((ue->dlsch_ra[eNB_id]) && (ue->dlsch_ra[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			RA_PDSCH,
			ue->dlsch_ra[eNB_id],
			NULL,
			ue->pdcch_vars[eNB_id]->num_pdcch_symbols,
			ue->frame_parms.symbols_per_tti>>1,
			abstraction_flag);
  }    
  
  if (subframe_select(&ue->frame_parms,subframe_rx) != SF_S) {  // do front-end processing for second slot, and first symbol of next subframe
    for (l=1; l<ue->frame_parms.symbols_per_tti>>1; l++) {
      if (abstraction_flag == 0) {
	start_meas(&ue->ofdm_demod_stats);
	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP, VCD_FUNCTION_IN);
	slot_fep(ue,
		 l,
		 1+(subframe_rx<<1),
		 ue->rx_offset,
		 0,
		 0);
	VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_UE_SLOT_FEP, VCD_FUNCTION_OUT);
	stop_meas(&ue->ofdm_demod_stats);
      }
      
      ue_measurement_procedures(l-1,ue,proc,eNB_id,abstraction_flag,mode);
      
    } // for l=1..l2
      // do first symbol of next subframe for channel estimation
    slot_fep(ue,
	     0,
	     (2+(subframe_rx<<1))%20,
	     ue->rx_offset,
	     0,
	     0);
  } // not an S-subframe

  // run pbch procedures if subframe is 0
  if (subframe_rx == 0)
    ue_pbch_procedures(eNB_id,ue,proc,abstraction_flag);
   
  // do procedures for C-RNTI
  if (ue->dlsch[eNB_id][0]->active == 1) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			PDSCH,
			ue->dlsch[eNB_id][0],
			NULL,
			1+(ue->frame_parms.symbols_per_tti>>1),
			ue->frame_parms.symbols_per_tti-1,
			abstraction_flag);
    ue_dlsch_procedures(ue,
			proc,
			eNB_id,
			PDSCH,
			ue->dlsch[eNB_id][0],
			NULL,
			&ue->dlsch_errors[eNB_id],
			mode,
			abstraction_flag);
      

  }
  else {
    //  printf("PDSCH inactive in subframe %d\n",subframe_rx-1);
    ue->dlsch[eNB_id][0]->harq_ack[subframe_rx].send_harq_status = 0;
  }

  // do procedures for SI-RNTI
  if ((ue->dlsch_SI[eNB_id]) && (ue->dlsch_SI[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			SI_PDSCH,
			ue->dlsch_SI[eNB_id],
			NULL,
			1+(ue->frame_parms.symbols_per_tti>>1),
			ue->frame_parms.symbols_per_tti-1,
			abstraction_flag);

    ue_dlsch_procedures(ue,
			proc,
			eNB_id,
			SI_PDSCH,
			ue->dlsch_SI[eNB_id],
			NULL,
			&ue->dlsch_SI_errors[eNB_id],
			mode,
			abstraction_flag);
    ue->dlsch_SI[eNB_id]->active = 0;
  }

  // do procedures for P-RNTI
  if ((ue->dlsch_p[eNB_id]) && (ue->dlsch_p[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			P_PDSCH,
			ue->dlsch_p[eNB_id],
			NULL,
			1+(ue->frame_parms.symbols_per_tti>>1),
			ue->frame_parms.symbols_per_tti-1,
			abstraction_flag);

    ue_dlsch_procedures(ue,
			proc,
			eNB_id,
			P_PDSCH,
			ue->dlsch_p[eNB_id],
			NULL,
			&ue->dlsch_p_errors[eNB_id],
			mode,
			abstraction_flag);
    ue->dlsch_p[eNB_id]->active = 0;
  }
  // do procedures for RA-RNTI
  if ((ue->dlsch_ra[eNB_id]) && (ue->dlsch_ra[eNB_id]->active == 1)) {
    ue_pdsch_procedures(ue,
			proc,
			eNB_id,
			RA_PDSCH,
			ue->dlsch_ra[eNB_id],
			NULL,
			1+(ue->frame_parms.symbols_per_tti>>1),
			ue->frame_parms.symbols_per_tti-1,
			abstraction_flag);
    ue_dlsch_procedures(ue,
			proc,
			eNB_id,
			RA_PDSCH,
			ue->dlsch_ra[eNB_id],
			NULL,
			&ue->dlsch_ra_errors[eNB_id],
			mode,
			abstraction_flag);
    ue->dlsch_ra[eNB_id]->active = 0;
  }

  if (subframe_rx==9) {
    if (frame_rx % 10 == 0) {
      if ((ue->dlsch_received[eNB_id] - ue->dlsch_received_last[eNB_id]) != 0)
	ue->dlsch_fer[eNB_id] = (100*(ue->dlsch_errors[eNB_id] - ue->dlsch_errors_last[eNB_id]))/(ue->dlsch_received[eNB_id] - ue->dlsch_received_last[eNB_id]);

      ue->dlsch_errors_last[eNB_id] = ue->dlsch_errors[eNB_id];
      ue->dlsch_received_last[eNB_id] = ue->dlsch_received[eNB_id];
    }


    ue->bitrate[eNB_id] = (ue->total_TBS[eNB_id] - ue->total_TBS_last[eNB_id])*100;
    ue->total_TBS_last[eNB_id] = ue->total_TBS[eNB_id];
    LOG_D(PHY,"[UE %d] Calculating bitrate Frame %d: total_TBS = %d, total_TBS_last = %d, bitrate %f kbits\n",
	  ue->Mod_id,frame_rx,ue->total_TBS[eNB_id],
	  ue->total_TBS_last[eNB_id],(float) ue->bitrate[eNB_id]/1000.0);
  }



#ifdef EMOS
  phy_procedures_emos_UE_RX(ue,slot,eNB_id);
#endif

     
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_RX, VCD_FUNCTION_OUT);
  stop_meas(&ue->phy_proc_rx);
  return (0);
}
   
#ifdef Rel10
int phy_procedures_RN_UE_RX(uint8_t slot_rx, uint8_t next_slot, relaying_type_t r_type)
{
   
  int do_proc =0; // do nothing by default
   
  switch(r_type) {
  case no_relay:
    do_proc=no_relay; // perform the normal UE operation
    break;
     
  case multicast_relay:
    if (slot_rx > 12)
      do_proc = 0; // do nothing
    else // SF#1, SF#2, SF3, SF#3, SF#4, SF#5, SF#6(do rx slot 12)
      do_proc = multicast_relay ; // do PHY procedures UE RX
     
    break;
     
  default: // should'not be here
    LOG_W(PHY,"Not supported relay type %d, do nothing \n", r_type);
    do_proc= 0;
    break;
  }
   
  return do_proc;
}
#endif
 
 
void phy_procedures_UE_lte(PHY_VARS_UE *ue,UE_rxtx_proc_t *proc,uint8_t eNB_id,uint8_t abstraction_flag,runmode_t mode,
			   relaying_type_t r_type, PHY_VARS_RN *phy_vars_rn)
{
#if defined(ENABLE_ITTI)
  MessageDef   *msg_p;
  const char   *msg_name;
  instance_t    instance;
  unsigned int  Mod_id;
  int           result;
#endif
   
  int           frame_rx = proc->frame_rx;
  int           frame_tx = proc->frame_tx;
  int           subframe_rx = proc->subframe_rx;
  int           subframe_tx = proc->subframe_tx;
#undef DEBUG_PHY_PROC
   
  UE_L2_STATE_t ret;
  int slot;

  if (ue->mac_enabled == 0) {
    ue->UE_mode[eNB_id]=PUSCH;
  }
   
   
  VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_LTE,1);
  start_meas(&ue->phy_proc);
#if defined(ENABLE_ITTI)

  do {
    // Checks if a message has been sent to PHY sub-task
    itti_poll_msg (TASK_PHY_UE, &msg_p);
     
    if (msg_p != NULL) {
      msg_name = ITTI_MSG_NAME (msg_p);
      instance = ITTI_MSG_INSTANCE (msg_p);
      Mod_id = instance - NB_eNB_INST;
       
      switch (ITTI_MSG_ID(msg_p)) {
      case PHY_FIND_CELL_REQ:
	LOG_I(PHY, "[UE %d] Received %s\n", Mod_id, msg_name);
	 
	/* TODO process the message */
	break;
	 
      default:
	LOG_E(PHY, "[UE %d] Received unexpected message %s\n", Mod_id, msg_name);
	break;
      }
       
      result = itti_free (ITTI_MSG_ORIGIN_ID(msg_p), msg_p);
      AssertFatal (result == EXIT_SUCCESS, "Failed to free memory (%d)!\n", result);
    }
  } while(msg_p != NULL);
   
#endif
   
  for (slot=0;slot<2;slot++) {
     
    if ((subframe_select(&ue->frame_parms,subframe_tx)==SF_UL)||
	(ue->frame_parms.frame_type == FDD)) {
      phy_procedures_UE_TX(ue,proc,eNB_id,abstraction_flag,mode,r_type);
    }
     
    if ((subframe_select(&ue->frame_parms,subframe_rx)==SF_DL) ||
	(ue->frame_parms.frame_type == FDD)) {
#ifdef Rel10
       
      if (phy_procedures_RN_UE_RX(subframe_rx, subframe_tx, r_type) != 0 )
#endif
	phy_procedures_UE_RX(ue,proc,eNB_id,abstraction_flag,mode,r_type,phy_vars_rn);
    }
     
    if ((subframe_select(&ue->frame_parms,subframe_tx)==SF_S) &&
	(slot==1)) {
      phy_procedures_UE_S_TX(ue,eNB_id,abstraction_flag,r_type);
    }
       
    if ((subframe_select(&ue->frame_parms,subframe_rx)==SF_S) &&
	(slot==0)) {
#ifdef Rel10
	 
      if (phy_procedures_RN_UE_RX(subframe_rx, subframe_tx, r_type) != 0 )
#endif
	phy_procedures_UE_RX(ue,proc,eNB_id,abstraction_flag,mode,r_type,phy_vars_rn);
    }
       
    if (ue->mac_enabled==1) {
      if (slot==0) {
	ret = mac_xface->ue_scheduler(ue->Mod_id,
				      frame_tx,
				      subframe_rx,
				      subframe_select(&ue->frame_parms,subframe_tx),
				      eNB_id,
				      0/*FIXME CC_id*/);
	   
	if (ret == CONNECTION_LOST) {
	  LOG_E(PHY,"[UE %d] Frame %d, subframe %d RRC Connection lost, returning to PRACH\n",ue->Mod_id,
		frame_rx,subframe_tx);
	  ue->UE_mode[eNB_id] = PRACH;
	  //      mac_xface->macphy_exit("Connection lost");
	} else if (ret == PHY_RESYNCH) {
	  LOG_E(PHY,"[UE %d] Frame %d, subframe %d RRC Connection lost, trying to resynch\n",
		ue->Mod_id,
		frame_rx,subframe_tx);
	  ue->UE_mode[eNB_id] = RESYNCH;
	  //     mac_xface->macphy_exit("Connection lost");
	} else if (ret == PHY_HO_PRACH) {
	  LOG_I(PHY,"[UE %d] Frame %d, subframe %d, return to PRACH and perform a contention-free access\n",
		ue->Mod_id,frame_rx,subframe_tx);
	  ue->UE_mode[eNB_id] = PRACH;
	}
      }
    }
       
    VCD_SIGNAL_DUMPER_DUMP_FUNCTION_BY_NAME(VCD_SIGNAL_DUMPER_FUNCTIONS_PHY_PROCEDURES_UE_LTE,0);
    stop_meas(&ue->phy_proc);
  } // slot
}
 
 
