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

/*! \file dlsim.c
 \brief Top-level DL simulator
 \author R. Knopp
 \date 2011 - 2014
 \version 0.1
 \company Eurecom
 \email: knopp@eurecom.fr
 \note
 \warning
*/

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>

#include "SIMULATION/TOOLS/defs.h"
#include "PHY/types.h"
#include "PHY/defs.h"
#include "PHY/vars.h"

#include "SCHED/defs.h"
#include "SCHED/vars.h"
#include "LAYER2/MAC/vars.h"

#include "OCG_vars.h"
#include "UTIL/LOG/log.h"
#include "UTIL/LISTS/list.h"

#include "unitary_defs.h"


#include "PHY/TOOLS/lte_phy_scope.h"

PHY_VARS_eNB *eNB;
PHY_VARS_UE *UE;

int otg_enabled=0;
/*the following parameters are used to control the processing times calculations*/
double t_tx_max = -1000000000; /*!< \brief initial max process time for tx */
double t_rx_max = -1000000000; /*!< \brief initial max process time for rx */
double t_tx_min = 1000000000; /*!< \brief initial min process time for tx */
double t_rx_min = 1000000000; /*!< \brief initial min process time for rx */
int n_tx_dropped = 0; /*!< \brief initial max process time for tx */
int n_rx_dropped = 0; /*!< \brief initial max process time for rx */

void handler(int sig)
{
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, 2);
  exit(1);
}



//DCI2_5MHz_2A_M10PRB_TDD_t DLSCH_alloc_pdu2_2A[2];

DCI1E_5MHz_2A_M10PRB_TDD_t  DLSCH_alloc_pdu2_1E[2];
uint64_t DLSCH_alloc_pdu_1[2];

#define UL_RB_ALLOC 0x1ff;
#define CCCH_RB_ALLOC computeRIV(eNB->frame_parms.N_RB_UL,0,2)
//#define DLSCH_RB_ALLOC 0x1fbf // igore DC component,RB13
//#define DLSCH_RB_ALLOC 0x0001
void do_OFDM_mod_l(int32_t **txdataF, int32_t **txdata, uint16_t next_slot, LTE_DL_FRAME_PARMS *frame_parms)
{

  int aa, slot_offset, slot_offset_F;

  slot_offset_F = (next_slot)*(frame_parms->ofdm_symbol_size)*((frame_parms->Ncp==1) ? 6 : 7);
  slot_offset = (next_slot)*(frame_parms->samples_per_tti>>1);

  for (aa=0; aa<frame_parms->nb_antennas_tx; aa++) {
    //    printf("Thread %d starting ... aa %d (%llu)\n",omp_get_thread_num(),aa,rdtsc());

    if (frame_parms->Ncp == 1)
      PHY_ofdm_mod(&txdataF[aa][slot_offset_F],        // input
                   &txdata[aa][slot_offset],         // output
                   frame_parms->ofdm_symbol_size,
                   6,                 // number of symbols
                   frame_parms->nb_prefix_samples,               // number of prefix samples
                   CYCLIC_PREFIX);
    else {
      normal_prefix_mod(&txdataF[aa][slot_offset_F],
                        &txdata[aa][slot_offset],
                        7,
                        frame_parms);
    }


  }
  
}

void DL_channel(PHY_VARS_eNB *eNB,PHY_VARS_UE *UE,int subframe,int awgn_flag,double SNR, int tx_lev,int hold_channel,int abstx, int num_rounds, int trials, int round, channel_desc_t *eNB2UE[4], 
		double *s_re[2],double *s_im[2],double *r_re[2],double *r_im[2],FILE *csv_fd) {

  int i,u;
  int aa,aarx,aatx;
  double channelx,channely;
  double sigma2_dB,sigma2;
  double iqim=0.0;

  //    printf("Copying tx ..., nsymb %d (n_tx %d), awgn %d\n",nsymb,eNB->frame_parms.nb_antennas_tx,awgn_flag);
  for (i=0; i<2*UE->frame_parms.samples_per_tti; i++) {
    for (aa=0; aa<eNB->frame_parms.nb_antennas_tx; aa++) {
      if (awgn_flag == 0) {
	s_re[aa][i] = ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) + (i<<1)]);
	s_im[aa][i] = ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) +(i<<1)+1]);
      } else {
	for (aarx=0; aarx<UE->frame_parms.nb_antennas_rx; aarx++) {
	  if (aa==0) {
	    r_re[aarx][i] = ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) +(i<<1)]);
	    r_im[aarx][i] = ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) +(i<<1)+1]);
	  } else {
	    r_re[aarx][i] += ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) +(i<<1)]);
	    r_im[aarx][i] += ((double)(((short *)eNB->common_vars.txdata[0][aa]))[(2*subframe*UE->frame_parms.samples_per_tti) +(i<<1)+1]);
	  }
	  
	}
      }
    }
  }
  
  // Multipath channel
  if (awgn_flag == 0) {
    multipath_channel(eNB2UE[round],s_re,s_im,r_re,r_im,
		      2*UE->frame_parms.samples_per_tti,hold_channel);
    
    //      printf("amc: ****************** eNB2UE[%d]->n_rx = %d,dd %d\n",round,eNB2UE[round]->nb_rx,eNB2UE[round]->channel_offset);
    if(abstx==1 && num_rounds>1)
      if(round==0 && hold_channel==0) {
	random_channel(eNB2UE[1],0);
	random_channel(eNB2UE[2],0);
	random_channel(eNB2UE[3],0);
      }
    
    if (UE->perfect_ce==1) {
      // fill in perfect channel estimates
      freq_channel(eNB2UE[round],UE->frame_parms.N_RB_DL,12*UE->frame_parms.N_RB_DL + 1);
      /*
	write_output("channel.m","ch",eNB2UE[round]->ch[0],eNB2UE[round]->channel_length,1,8);
	write_output("channelF.m","chF",eNB2UE[round]->chF[0],12*UE->frame_parms.N_RB_DL + 1,1,8);
      */
    }
  }
  
  
  if(abstx) {
    if (trials==0 && round==0) {
      // calculate freq domain representation to compute SINR
      freq_channel(eNB2UE[0], eNB->frame_parms.N_RB_DL,2*eNB->frame_parms.N_RB_DL + 1);
      // snr=pow(10.0,.1*SNR);
      fprintf(csv_fd,"%f,",SNR);
      
      for (u=0; u<2*eNB->frame_parms.N_RB_DL; u++) {
	for (aarx=0; aarx<eNB2UE[0]->nb_rx; aarx++) {
	  for (aatx=0; aatx<eNB2UE[0]->nb_tx; aatx++) {
	    channelx = eNB2UE[0]->chF[aarx+(aatx*eNB2UE[0]->nb_rx)][u].x;
	    channely = eNB2UE[0]->chF[aarx+(aatx*eNB2UE[0]->nb_rx)][u].y;
	    fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
	  }
	}
      }	
      
      if(num_rounds>1) {
	freq_channel(eNB2UE[1], eNB->frame_parms.N_RB_DL,2*eNB->frame_parms.N_RB_DL + 1);
	
	for (u=0; u<2*eNB->frame_parms.N_RB_DL; u++) {
	  for (aarx=0; aarx<eNB2UE[1]->nb_rx; aarx++) {
	    for (aatx=0; aatx<eNB2UE[1]->nb_tx; aatx++) {
	      channelx = eNB2UE[1]->chF[aarx+(aatx*eNB2UE[1]->nb_rx)][u].x;
	      channely = eNB2UE[1]->chF[aarx+(aatx*eNB2UE[1]->nb_rx)][u].y;
	      fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
	    }
	  }
	}
	
	freq_channel(eNB2UE[2], eNB->frame_parms.N_RB_DL,2*eNB->frame_parms.N_RB_DL + 1);
	
	for (u=0; u<2*eNB->frame_parms.N_RB_DL; u++) {
	  for (aarx=0; aarx<eNB2UE[2]->nb_rx; aarx++) {
	    for (aatx=0; aatx<eNB2UE[2]->nb_tx; aatx++) {
	      channelx = eNB2UE[2]->chF[aarx+(aatx*eNB2UE[2]->nb_rx)][u].x;
	      channely = eNB2UE[2]->chF[aarx+(aatx*eNB2UE[2]->nb_rx)][u].y;
	      fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
	    }
	  }
	}
	
	freq_channel(eNB2UE[3], eNB->frame_parms.N_RB_DL,2*eNB->frame_parms.N_RB_DL + 1);
	
	for (u=0; u<2*eNB->frame_parms.N_RB_DL; u++) {
	  for (aarx=0; aarx<eNB2UE[3]->nb_rx; aarx++) {
	    for (aatx=0; aatx<eNB2UE[3]->nb_tx; aatx++) {
	      channelx = eNB2UE[3]->chF[aarx+(aatx*eNB2UE[3]->nb_rx)][u].x;
	      channely = eNB2UE[3]->chF[aarx+(aatx*eNB2UE[3]->nb_rx)][u].y;
	      fprintf(csv_fd,"%e+i*(%e),",channelx,channely);
	    }
	  }
	}
      }
    }
  }
  
  //AWGN
  // This is the SNR on the PDSCH for OFDM symbols without pilots -> rho_A
  sigma2_dB = 10*log10((double)tx_lev) +10*log10((double)eNB->frame_parms.ofdm_symbol_size/(double)(eNB->frame_parms.N_RB_DL*12)) - SNR - get_pa_dB(eNB->pdsch_config_dedicated);
  sigma2 = pow(10,sigma2_dB/10);

  for (i=0; i<2*UE->frame_parms.samples_per_tti; i++) {
    for (aa=0; aa<eNB->frame_parms.nb_antennas_rx; aa++) {
      //printf("s_re[0][%d]=> %f , r_re[0][%d]=> %f\n",i,s_re[aa][i],i,r_re[aa][i]);
      ((short*) UE->common_vars.rxdata[aa])[(2*subframe*UE->frame_parms.samples_per_tti)+2*i] =
	(short) (r_re[aa][i] + sqrt(sigma2/2)*gaussdouble(0.0,1.0));
      ((short*) UE->common_vars.rxdata[aa])[(2*subframe*UE->frame_parms.samples_per_tti)+2*i+1] =
	(short) (r_im[aa][i] + (iqim*r_re[aa][i]) + sqrt(sigma2/2)*gaussdouble(0.0,1.0));
    }
  }
}


void fill_DCI(PHY_VARS_eNB *eNB,
	      DCI_ALLOC_t *dci_alloc,
	      int subframe, 
	      int n_rnti,
	      int n_users,
	      int transmission_mode,
	      int common_flag,
	      int DLSCH_RB_ALLOC,
	      int TPC,
	      int mcs1,
	      int mcs2,
	      int ndi,
	      int rv,
	      int *num_common_dci,
	      int *num_ue_spec_dci,
	      int *num_dci) {

  int k;
  int dci_length,dci_length_bytes;

  //  printf("Generating DCIs for %d users, TM %d, mcs1 %d\n",n_users,transmission_mode,mcs1);
  for(k=0; k<n_users; k++) {
    switch(transmission_mode) {
    case 1:

    case 2:
      if (common_flag == 0) {
	
	if (eNB->frame_parms.frame_type == TDD) {
	  
	  switch (eNB->frame_parms.N_RB_DL) {
	  case 6:
	    dci_length = sizeof_DCI1_1_5MHz_TDD_t;
	    dci_length_bytes = sizeof(DCI1_1_5MHz_TDD_t);
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
	    ((DCI1_1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
	    break;
	    
	  case 25:
	    dci_length = sizeof_DCI1_5MHz_TDD_t;
	    dci_length_bytes = sizeof(DCI1_5MHz_TDD_t);
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
	    ((DCI1_5MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
	    break;
	    
	  case 50:
	    dci_length = sizeof_DCI1_10MHz_TDD_t;
	    dci_length_bytes = sizeof(DCI1_10MHz_TDD_t);
	    ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_10MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              break;

            case 100:
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_20MHz_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              dci_length = sizeof_DCI1_20MHz_TDD_t;
              dci_length_bytes = sizeof(DCI1_20MHz_TDD_t);
              break;
            }
          } else {
            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_1_5MHz_FDD_t);
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              break;

            case 25:
              dci_length = sizeof_DCI1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_5MHz_FDD_t);
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              break;

            case 50:
              dci_length = sizeof_DCI1_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_10MHz_FDD_t);
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              break;

            case 100:
              dci_length = sizeof_DCI1_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1_20MHz_FDD_t);
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = ndi;
              ((DCI1_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = rv;
              break;
            }
          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = n_rnti+k;
          dci_alloc[*num_dci].format     = format1;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

	  //          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             n_rnti+k,
                                             format1,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_dci = *num_dci+1;
          *num_ue_spec_dci = *num_ue_spec_dci+1;
        } else {
          if (eNB->frame_parms.frame_type == TDD) {

            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          } else {
            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;
            }
          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = SI_RNTI;
          dci_alloc[*num_dci].format     = format1A;
          dci_alloc[*num_dci].firstCCE       = 0;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             SI_RNTI,
                                             format1A,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_common_dci=*num_common_dci+1;
          *num_dci = *num_dci + 1;

        }

        break;

      case 3:
        if (common_flag == 0) {

          if (eNB->frame_parms.nb_antennas_tx == 2) {

            if (eNB->frame_parms.frame_type == TDD) {

              switch (eNB->frame_parms.N_RB_DL) {
              case 6:
                dci_length = sizeof_DCI2A_1_5MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2A_1_5MHz_2A_TDD_t);
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 25:
                dci_length = sizeof_DCI2A_5MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2A_5MHz_2A_TDD_t);
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 50:
                dci_length = sizeof_DCI2A_10MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2A_10MHz_2A_TDD_t);
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 100:
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                dci_length = sizeof_DCI2A_20MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2A_20MHz_2A_TDD_t);
                break;
              }
            }

            else {
              switch (eNB->frame_parms.N_RB_DL) {
              case 6:
                dci_length = sizeof_DCI2A_1_5MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2A_1_5MHz_2A_FDD_t);
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 25:
                dci_length = sizeof_DCI2A_5MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2A_5MHz_2A_FDD_t);
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 50:
                dci_length = sizeof_DCI2A_10MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2A_10MHz_2A_FDD_t);
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 100:
                dci_length = sizeof_DCI2A_20MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2A_20MHz_2A_FDD_t);
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2A_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;
              }
            }
          } else if (eNB->frame_parms.nb_antennas_tx == 4) {

          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = n_rnti+k;
          dci_alloc[*num_dci].format     = format2A;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

          printf("Generating dlsch params for user %d / format 2A (%d)\n",k,format2A);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             n_rnti+k,
                                             format2A,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_dci = *num_dci + 1;
          *num_ue_spec_dci = *num_ue_spec_dci + 1;
        } else {
          if (eNB->frame_parms.frame_type == TDD) {

            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          } else {
            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;
            }
          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = SI_RNTI;
          dci_alloc[*num_dci].format     = format1A;
          dci_alloc[*num_dci].firstCCE       = 0;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             SI_RNTI,
                                             format1A,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_common_dci = *num_common_dci + 1;
          *num_dci = *num_dci + 1;

        }

        printf("Generated DCI format 2A (Transmission Mode 3)\n");
        break;

      case 4:
        if (common_flag == 0) {

          if (eNB->frame_parms.nb_antennas_tx == 2) {

            if (eNB->frame_parms.frame_type == TDD) {

              switch (eNB->frame_parms.N_RB_DL) {
              case 6:
                dci_length = sizeof_DCI2_1_5MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2_1_5MHz_2A_TDD_t);
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_1_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 25:
                dci_length = sizeof_DCI2_5MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2_5MHz_2A_TDD_t);
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_5MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 50:
                dci_length = sizeof_DCI2_10MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2_10MHz_2A_TDD_t);
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_10MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 100:
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_20MHz_2A_TDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                dci_length = sizeof_DCI2_20MHz_2A_TDD_t;
                dci_length_bytes = sizeof(DCI2_20MHz_2A_TDD_t);
                break;
              }
            }

            else {
              switch (eNB->frame_parms.N_RB_DL) {
              case 6:
                dci_length = sizeof_DCI2_1_5MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2_1_5MHz_2A_FDD_t);
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_1_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 25:
                dci_length = sizeof_DCI2_5MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2_5MHz_2A_FDD_t);
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_5MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 50:
                dci_length = sizeof_DCI2_10MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2_10MHz_2A_FDD_t);
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_10MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;

              case 100:
                dci_length = sizeof_DCI2_20MHz_2A_FDD_t;
                dci_length_bytes = sizeof(DCI2_20MHz_2A_FDD_t);
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rah              = 0;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = DLSCH_RB_ALLOC;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = 0;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs1             = mcs1;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi1             = ndi;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv1              = rv;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs2             = mcs2;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi2             = ndi;
                ((DCI2_20MHz_2A_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv2              = rv;
                break;
              }
            }
          } else if (eNB->frame_parms.nb_antennas_tx == 4) {

          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = n_rnti+k;
          dci_alloc[*num_dci].format     = format2;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             n_rnti+k,
                                             format2,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_dci = *num_dci + 1;
          *num_ue_spec_dci = *num_ue_spec_dci + 1;
        } else {
          if (eNB->frame_parms.frame_type == TDD) {

            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_TDD_1_6_t);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_TDD_1_6_t);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_TDD_1_6_t);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->dai              = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_TDD_1_6_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              dci_length = sizeof_DCI1A_20MHz_TDD_1_6_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_TDD_1_6_t);
              break;
            }
          } else {
            switch (eNB->frame_parms.N_RB_DL) {
            case 6:
              dci_length = sizeof_DCI1A_1_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_1_5MHz_FDD_t);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_1_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 25:
              dci_length = sizeof_DCI1A_5MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_5MHz_FDD_t);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_5MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 50:
              dci_length = sizeof_DCI1A_10MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_10MHz_FDD_t);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_10MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;

            case 100:
              dci_length = sizeof_DCI1A_20MHz_FDD_t;
              dci_length_bytes = sizeof(DCI1A_20MHz_FDD_t);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->type             = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->vrb_type         = 1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rballoc          = computeRIV(eNB->frame_parms.N_RB_DL,0,9);
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->TPC              = TPC;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->harq_pid         = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->mcs             = mcs1;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->ndi             = 0;
              ((DCI1A_20MHz_FDD_t *)&DLSCH_alloc_pdu_1[k])->rv              = 0;
              break;
            }
          }

          memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu_1[k],dci_length_bytes);
          dci_alloc[*num_dci].dci_length = dci_length;
          dci_alloc[*num_dci].L          = 1;
          dci_alloc[*num_dci].rnti       = SI_RNTI;
          dci_alloc[*num_dci].format     = format1A;
          dci_alloc[*num_dci].firstCCE       = 0;
          dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);

          printf("Generating dlsch params for user %d\n",k);
          generate_eNB_dlsch_params_from_dci(0,
					     subframe,
                                             &DLSCH_alloc_pdu_1[0],
                                             SI_RNTI,
                                             format1A,
                                             eNB->dlsch[0],
                                             &eNB->frame_parms,
                                             eNB->pdsch_config_dedicated,
                                             SI_RNTI,
                                             0,
                                             P_RNTI,
                                             eNB->UE_stats[0].DL_pmi_single);

          *num_common_dci = *num_common_dci + 1;
          *num_dci = *num_dci + 1;

        }

        break;

      case 5:
      case 6:
        memcpy(&dci_alloc[*num_dci].dci_pdu[0],&DLSCH_alloc_pdu2_1E[k],sizeof(DCI1E_5MHz_2A_M10PRB_TDD_t));
        dci_alloc[*num_dci].dci_length = sizeof_DCI1E_5MHz_2A_M10PRB_TDD_t;
        dci_alloc[*num_dci].L          = 1;
        dci_alloc[*num_dci].rnti       = n_rnti+k;
        dci_alloc[*num_dci].format     = format1E_2A_M10PRB;
        dci_alloc[*num_dci].firstCCE       = 4*k;
        printf("Generating dlsch params for user %d\n",k);
        generate_eNB_dlsch_params_from_dci(0,
					   subframe,
                                           &DLSCH_alloc_pdu2_1E[k],
                                           n_rnti+k,
                                           format1E_2A_M10PRB,
                                           eNB->dlsch[k],
                                           &eNB->frame_parms,
                                           eNB->pdsch_config_dedicated,
                                           SI_RNTI,
                                           0,
                                           P_RNTI,
                                           eNB->UE_stats[k].DL_pmi_single);

        dump_dci(&eNB->frame_parms,&dci_alloc[*num_dci]);
        *num_ue_spec_dci = *num_ue_spec_dci + 1;
        *num_dci = *num_dci + 1;

        break;

      default:
        printf("Unsupported Transmission Mode!!!");
        exit(-1);
        break;
      }
    }
}

int n_users = 1;
sub_frame_t subframe=7;
DCI_PDU DCI_pdu;
int num_common_dci=0,num_ue_spec_dci=0,num_dci=0,num_pdcch_symbols=1;


DCI_PDU *get_dci_sdu(module_id_t module_idP,int CC_id,frame_t frameP,sub_frame_t subframeP) {

  if (subframeP == subframe) {
    DCI_pdu.Num_ue_spec_dci   = num_ue_spec_dci;
    DCI_pdu.Num_common_dci    = num_common_dci;
    DCI_pdu.num_pdcch_symbols = num_pdcch_symbols; 
    return(&DCI_pdu);
  } else {
    DCI_pdu.Num_ue_spec_dci   = 0;
    DCI_pdu.Num_common_dci    = 0;
    DCI_pdu.num_pdcch_symbols = num_pdcch_symbols; 
  }
}

void eNB_dlsch_ulsch_scheduler(module_id_t module_idP, uint8_t cooperation_flag, frame_t frameP, sub_frame_t subframeP) {
  
  return;
}

uint16_t n_rnti=0x1234;
unsigned char *input_buffer0[2],*input_buffer1[2];
unsigned short input_buffer_length0,input_buffer_length1;

uint8_t *get_dlsch_sdu(module_id_t module_idP,int CC_id,frame_t frameP,rnti_t rnti,uint8_t TBindex) {

  int k;

  for (k=0;k<n_users;k++)
    if (rnti == n_rnti+k)
      break;
  if (k<n_users)
    return(TBindex==0 ? input_buffer0[k] : input_buffer1[k]);
  else {
    printf("RNTI not found,exiting\n");
    exit(-1);
  }
}

int main(int argc, char **argv)
{

  int c;
  int k,i,aa;

  int s,Kr,Kr_bytes;

  double SNR,snr0=-2.0,snr1,rate;
  double snr_step=1,input_snr_step=1, snr_int=30;

  LTE_DL_FRAME_PARMS *frame_parms;
  double s_re0[30720*2],s_im0[30720*2],r_re0[30720*2],r_im0[30720*2];
  double s_re1[30720*2],s_im1[30720*2],r_re1[30720*2],r_im1[30720*2];
  double *s_re[2]={s_re0,s_re1};
  double *s_im[2]={s_im0,s_im1};
  double *r_re[2]={r_re0,r_re1};
  double *r_im[2]={r_im0,r_im1};
  double forgetting_factor=0.0; //in [0,1] 0 means a new channel every time, 1 means keep the same channel


  uint8_t extended_prefix_flag=0,transmission_mode=1,n_tx=1,n_rx=2;
  uint16_t Nid_cell=0;

  int eNB_id = 0;
  unsigned char mcs1=0,mcs2=0,mcs_i=0,dual_stream_UE = 0,awgn_flag=0,round;
  unsigned char i_mod = 2;
  unsigned short NB_RB;
  uint16_t tdd_config=3;



  SCM_t channel_model=Rayleigh1;
  //  unsigned char *input_data,*decoded_output;

  DCI_ALLOC_t *dci_alloc = &DCI_pdu.dci_alloc[0];

  unsigned int ret;
  unsigned int coded_bits_per_codeword=0,nsymb,tbs=0;

  unsigned int tx_lev=0,tx_lev_dB=0,trials,errs[4]= {0,0,0,0},errs2[4]= {0,0,0,0},round_trials[4]= {0,0,0,0},dci_errors=0;//,num_layers;
  int re_allocated;
  char fname[32],vname[32];
  FILE *bler_fd;
  char bler_fname[256];
  FILE *time_meas_fd;
  char time_meas_fname[256];
  //  FILE *tikz_fd;
  //  char tikz_fname[256];

  FILE *input_trch_fd=NULL;
  unsigned char input_trch_file=0;
  FILE *input_fd=NULL;
  unsigned char input_file=0;
  //  char input_val_str[50],input_val_str2[50];

  char input_trch_val[16];

  //  unsigned char pbch_pdu[6];




  //  FILE *rx_frame_file;

  int n_frames;
  int n_ch_rlz = 1;
  channel_desc_t *eNB2UE[4];
  uint8_t num_pdcch_symbols_2=0;
  uint8_t rx_sample_offset = 0;
  //char stats_buffer[4096];
  //int len;
  uint8_t num_rounds = 4;//,fix_rounds=0;

  int u;
  int n=0;
  int abstx=0;
  int iii;

  int ch_realization;
  int pmi_feedback=0;
  int hold_channel=0;

  // void *data;
  // int ii;
  //  int bler;
  double blerr[4],uncoded_ber,avg_ber;
  short *uncoded_ber_bit=NULL;
  uint8_t N_RB_DL=25,osf=1;
  frame_t frame_type = FDD;
  int xforms=0;
  FD_lte_phy_scope_ue *form_ue = NULL;
  char title[255];

  int numCCE=0;
  int dci_length_bytes=0,dci_length=0;
  //double channel_bandwidth = 5.0, sampling_rate=7.68;
  int common_flag=0,TPC=0;

  double cpu_freq_GHz;
  //  time_stats_t ts;//,sts,usts;
  int avg_iter,iter_trials;
  int rballocset=0;
  int print_perf=0;
  int test_perf=0;
  int dump_table=0;

  double effective_rate=0.0;
  char channel_model_input[10]="I";

  int TB0_active = 1;
  uint32_t perfect_ce = 0;

  //  LTE_DL_UE_HARQ_t *dlsch0_ue_harq;
  //  LTE_DL_eNB_HARQ_t *dlsch0_eNB_harq;
  uint8_t Kmimo;
  uint8_t ue_category=4;
  uint32_t Nsoft;



  int CCE_table[800];

  int threequarter_fs=0;

  opp_enabled=1; // to enable the time meas

  FILE *csv_fd=NULL;
  char csv_fname[32];
  int dci_flag=1;
  int two_thread_flag=0;
  int DLSCH_RB_ALLOC;

#if defined(__arm__)
  FILE    *proc_fd = NULL;
  char buf[64];

  proc_fd = fopen("/sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq", "r");
  if(!proc_fd)
     printf("cannot open /sys/devices/system/cpu/cpu4/cpufreq/cpuinfo_cur_freq");
  else {
     while(fgets(buf, 63, proc_fd))
        printf("%s", buf);
  }
  fclose(proc_fd);
  cpu_freq_GHz = ((double)atof(buf))/1e6;
#else
  cpu_freq_GHz = get_cpu_freq_GHz();
#endif
  printf("Detected cpu_freq %f GHz\n",cpu_freq_GHz);

  //signal(SIGSEGV, handler);
  //signal(SIGABRT, handler);

  logInit();

  // default parameters
  n_frames = 1000;
  snr0 = 0;
  //  num_layers = 1;
  perfect_ce = 0;

  while ((c = getopt (argc, argv, "ahdpZDe:Em:n:o:s:f:t:c:g:r:F:x:y:z:AM:N:I:i:O:R:S:C:T:b:u:v:w:B:Pl:WXY")) != -1) {
    switch (c) {
    case 'a':
      awgn_flag = 1;
      channel_model = AWGN;
      break;

    case 'A':
      abstx = 1;
      break;

    case 'b':
      tdd_config=atoi(optarg);
      break;

    case 'B':
      N_RB_DL=atoi(optarg);
      break;

    case 'c':
      num_pdcch_symbols=atoi(optarg);
      break;

    case 'C':
      Nid_cell = atoi(optarg);
      break;

    case 'd':
      dci_flag = 1;
      break;

    case 'D':
      frame_type=TDD;
      break;

    case 'e':
      num_rounds=1;
      common_flag = 1;
      TPC = atoi(optarg);
      break;
      
    case 'E':
      threequarter_fs=1;
      break;

    case 'f':
      input_snr_step= atof(optarg);
      break;

    case 'F':
      forgetting_factor = atof(optarg);
      break;

    case 'i':
      input_fd = fopen(optarg,"r");
      input_file=1;
      dci_flag = 1;
      break;

    case 'I':
      input_trch_fd = fopen(optarg,"r");
      input_trch_file=1;
      break;

    case 'W':
      two_thread_flag = 1;
      break;
    case 'l':
      offset_mumimo_llr_drange_fix=atoi(optarg);
      break;

    case 'm':
      mcs1 = atoi(optarg);
      break;

    case 'M':
      mcs2 = atoi(optarg);
      break;

    case 'O':
      test_perf=atoi(optarg);
      //print_perf =1;
      break;

    case 't':
      mcs_i = atoi(optarg);
      i_mod = get_Qm(mcs_i);
      break;

    case 'n':
      n_frames = atoi(optarg);
      break;


    case 'o':
      rx_sample_offset = atoi(optarg);
      break;

    case 'r':
      DLSCH_RB_ALLOC = atoi(optarg);
      rballocset = 1;
      break;

    case 's':
      snr0 = atof(optarg);
      break;

    case 'w':
      snr_int = atof(optarg);
      break;


    case 'N':
      n_ch_rlz= atof(optarg);
      break;

    case 'p':
      extended_prefix_flag=1;
      break;

    case 'g':
      memcpy(channel_model_input,optarg,10);

      switch((char)*optarg) {
      case 'A':
        channel_model=SCM_A;
        break;

      case 'B':
        channel_model=SCM_B;
        break;

      case 'C':
        channel_model=SCM_C;
        break;

      case 'D':
        channel_model=SCM_D;
        break;

      case 'E':
        channel_model=EPA;
        break;

      case 'F':
        channel_model=EVA;
        break;

      case 'G':
        channel_model=ETU;
        break;

      case 'H':
        channel_model=Rayleigh8;
        break;

      case 'I':
        channel_model=Rayleigh1;
        break;

      case 'J':
        channel_model=Rayleigh1_corr;
        break;

      case 'K':
        channel_model=Rayleigh1_anticorr;
        break;

      case 'L':
        channel_model=Rice8;
        break;

      case 'M':
        channel_model=Rice1;
        break;

      case 'N':
        channel_model=AWGN;
        break;
      default:
        msg("Unsupported channel model!\n");
        exit(-1);
      }

      break;
    case 'R':
      num_rounds=atoi(optarg);
      break;

    case 'S':
      subframe=atoi(optarg);
      break;

    case 'T':
      n_rnti=atoi(optarg);
      break;

    case 'u':
      dual_stream_UE=1;
      UE->use_ia_receiver = 1;

      if ((n_tx!=2) || (transmission_mode!=5)) {
        msg("IA receiver only supported for TM5!");
        exit(-1);
      }

      break;

    case 'v':
      i_mod = atoi(optarg);

      if (i_mod!=2 && i_mod!=4 && i_mod!=6) {
        msg("Wrong i_mod %d, should be 2,4 or 6\n",i_mod);
        exit(-1);
      }

      break;

    case 'P':
      print_perf=1;
      break;

    case 'x':
      transmission_mode=atoi(optarg);

      if ((transmission_mode!=1) &&
          (transmission_mode!=2) &&
          (transmission_mode!=3) &&
          (transmission_mode!=4) &&
          (transmission_mode!=5) &&
          (transmission_mode!=6)) {
        msg("Unsupported transmission mode %d\n",transmission_mode);
        exit(-1);
      }

      if (transmission_mode>1) {
        n_tx = 2;
      }

      break;

    case 'y':
      n_tx=atoi(optarg);

      if ((n_tx==0) || (n_tx>2)) {
        msg("Unsupported number of tx antennas %d\n",n_tx);
        exit(-1);
      }

      break;

    case 'X':
      xforms=1;
      break;

    case 'Y':
      perfect_ce=1;
      break;

    case 'z':
      n_rx=atoi(optarg);

      if ((n_rx==0) || (n_rx>2)) {
        msg("Unsupported number of rx antennas %d\n",n_rx);
        exit(-1);
      }

      break;

    case 'Z':
      dump_table=1;
      break;



    case 'h':
    default:
      printf("%s -h(elp) -a(wgn on) -d(ci decoding on) -p(extended prefix on) -m mcs1 -M mcs2 -n n_frames -s snr0 -x transmission mode (1,2,5,6) -y TXant -z RXant -I trch_file\n",argv[0]);
      printf("-h This message\n");
      printf("-a Use AWGN channel and not multipath\n");
      printf("-c Number of PDCCH symbols\n");
      printf("-m MCS1 for TB 1\n");
      printf("-M MCS2 for TB 2\n");
      printf("-d Transmit the DCI and compute its error statistics and the overall throughput\n");
      printf("-p Use extended prefix mode\n");
      printf("-n Number of frames to simulate\n");
      printf("-o Sample offset for receiver\n");
      printf("-s Starting SNR, runs from SNR to SNR+%.1fdB in steps of %.1fdB. If n_frames is 1 then just SNR is simulated and MATLAB/OCTAVE output is generated\n", snr_int, snr_step);
      printf("-f step size of SNR, default value is 1.\n");
      printf("-r ressource block allocation (see  section 7.1.6.3 in 36.213\n");
      printf("-g [A:M] Use 3GPP 25.814 SCM-A/B/C/D('A','B','C','D') or 36-101 EPA('E'), EVA ('F'),ETU('G') models (ignores delay spread and Ricean factor), Rayghleigh8 ('H'), Rayleigh1('I'), Rayleigh1_corr('J'), Rayleigh1_anticorr ('K'), Rice8('L'), Rice1('M')\n");
      printf("-F forgetting factor (0 new channel every trial, 1 channel constant\n");
      printf("-x Transmission mode (1,2,6 for the moment)\n");
      printf("-y Number of TX antennas used in eNB\n");
      printf("-z Number of RX antennas used in UE\n");
      printf("-t MCS of interfering UE\n");
      printf("-R Number of HARQ rounds (fixed)\n");
      printf("-A Turns on calibration mode for abstraction.\n");
      printf("-N Determines the number of Channel Realizations in Abstraction mode. Default value is 1. \n");
      printf("-O Set the percenatge of effective rate to testbench the modem performance (typically 30 and 70, range 1-100) \n");
      printf("-I Input filename for TrCH data (binary)\n");
      printf("-u Enables the Interference Aware Receiver for TM5 (default is normal receiver)\n");
      exit(1);
      break;
    }
  }

  if (common_flag == 0) {
    switch (N_RB_DL) {
    case 6:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x3f;
      num_pdcch_symbols = 3;
      break;

    case 25:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1fff;
      break;

    case 50:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1ffff;
      break;

    case 100:
      if (rballocset==0) DLSCH_RB_ALLOC = 0x1ffffff;
      break;
    }

    NB_RB=conv_nprb(0,DLSCH_RB_ALLOC,N_RB_DL);
  } else
    NB_RB = 4;

  if ((transmission_mode > 1) && (n_tx != 2))
    printf("n_tx must be >1 for transmission_mode %d\n",transmission_mode);

  if (xforms==1) {
    fl_initialize (&argc, argv, NULL, 0, 0);
    form_ue = create_lte_phy_scope_ue();
    sprintf (title, "LTE PHY SCOPE eNB");
    fl_show_form (form_ue->lte_phy_scope_ue, FL_PLACE_HOTSPOT, FL_FULLBORDER, title);
    
    if (!dual_stream_UE==0) {
      UE->use_ia_receiver = 1;
      fl_set_button(form_ue->button_0,1);
      fl_set_object_label(form_ue->button_0, "IA Receiver ON");
      fl_set_object_color(form_ue->button_0, FL_GREEN, FL_GREEN);
    }
  }

  if (transmission_mode==5) {
    n_users = 2;
    printf("dual_stream_UE=%d\n", dual_stream_UE);
  }

  lte_param_init(n_tx,
		 n_rx,
		 transmission_mode,
		 extended_prefix_flag,
		 frame_type,
		 Nid_cell,
		 tdd_config,
		 N_RB_DL,
		 threequarter_fs,
		 osf,
		 perfect_ce);

  eNB->mac_enabled=1;
  if (two_thread_flag == 0) {
    eNB->te = dlsch_encoding;
  }
  else {
    eNB->te = dlsch_encoding_2threads;
    init_td_thread(eNB,NULL);
    init_te_thread(eNB,NULL);
  }

  // callback functions required for phy_procedures_tx 
  mac_xface->get_dci_sdu = get_dci_sdu;
  mac_xface->get_dlsch_sdu = get_dlsch_sdu;
  mac_xface->eNB_dlsch_ulsch_scheduler = eNB_dlsch_ulsch_scheduler;
   
  //  eNB_id_i = UE->n_connected_eNB;

  printf("Setting mcs1 = %d\n",mcs1);
  printf("Setting mcs2 = %d\n",mcs2);
  printf("NPRB = %d\n",NB_RB);
  printf("n_frames = %d\n",n_frames);
  printf("Transmission mode %d with %dx%d antenna configuration, Extended Prefix %d\n",transmission_mode,n_tx,n_rx,extended_prefix_flag);

  snr1 = snr0+snr_int;
  printf("SNR0 %f, SNR1 %f\n",snr0,snr1);



  frame_parms = &eNB->frame_parms;

  nsymb = (eNB->frame_parms.Ncp == 0) ? 14 : 12;

  printf("Channel Model= (%s,%d)\n",channel_model_input, channel_model);
  printf("SCM-A=%d, SCM-B=%d, SCM-C=%d, SCM-D=%d, EPA=%d, EVA=%d, ETU=%d, Rayleigh8=%d, Rayleigh1=%d, Rayleigh1_corr=%d, Rayleigh1_anticorr=%d, Rice1=%d, Rice8=%d\n",
         SCM_A, SCM_B, SCM_C, SCM_D, EPA, EVA, ETU, Rayleigh8, Rayleigh1, Rayleigh1_corr, Rayleigh1_anticorr, Rice1, Rice8);

  if(transmission_mode==5)
    sprintf(bler_fname,"bler_tx%d_chan%d_nrx%d_mcs%d_mcsi%d_u%d_imod%d.csv",transmission_mode,channel_model,n_rx,mcs1,mcs_i,dual_stream_UE,i_mod);
  else
    sprintf(bler_fname,"bler_tx%d_chan%d_nrx%d_mcs%d.csv",transmission_mode,channel_model,n_rx,mcs1);

  bler_fd = fopen(bler_fname,"w");
  if (bler_fd==NULL) {
    fprintf(stderr,"Cannot create file %s!\n",bler_fname);
    exit(-1);
  }
  fprintf(bler_fd,"SNR; MCS; TBS; rate; err0; trials0; err1; trials1; err2; trials2; err3; trials3; dci_err\n");

  if (test_perf != 0) {
    char hostname[1024];
    hostname[1023] = '\0';
    gethostname(hostname, 1023);
    printf("Hostname: %s\n", hostname);
    //char dirname[FILENAME_MAX];
    //sprintf(dirname, "%s/SIMU/USER/pre-ci-logs-%s", getenv("OPENAIR_TARGETS"),hostname );
    sprintf(time_meas_fname,"time_meas_prb%d_mcs%d_anttx%d_antrx%d_pdcch%d_channel%s_tx%d.csv",
            N_RB_DL,mcs1,n_tx,n_rx,num_pdcch_symbols,channel_model_input,transmission_mode);
    //mkdir(dirname,0777);
    time_meas_fd = fopen(time_meas_fname,"w");
    if (time_meas_fd==NULL) {
      fprintf(stderr,"Cannot create file %s!\n",time_meas_fname);
      exit(-1);
    }
  }

  if(abstx) {
    // CSV file
    sprintf(csv_fname,"dataout_tx%d_u2%d_mcs%d_chan%d_nsimus%d_R%d.m",transmission_mode,dual_stream_UE,mcs1,channel_model,n_frames,num_rounds);
    csv_fd = fopen(csv_fname,"w");
    fprintf(csv_fd,"data_all%d=[",mcs1);
    if (csv_fd==NULL) {
      fprintf(stderr,"Cannot create file %s!\n",csv_fname);
      exit(-1);
    }
  }

  /*
  //sprintf(tikz_fname, "second_bler_tx%d_u2=%d_mcs%d_chan%d_nsimus%d.tex",transmission_mode,dual_stream_UE,mcs,channel_model,n_frames);
  sprintf(tikz_fname, "second_bler_tx%d_u2%d_mcs%d_chan%d_nsimus%d",transmission_mode,dual_stream_UE,mcs,channel_model,n_frames);
  tikz_fd = fopen(tikz_fname,"w");
  //fprintf(tikz_fd,"\\addplot[color=red, mark=o] plot coordinates {");
  switch (mcs)
    {
    case 0:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=star] plot coordinates {");
      break;
    case 1:
      fprintf(tikz_fd,"\\addplot[color=red, mark=star] plot coordinates {");
      break;
    case 2:
      fprintf(tikz_fd,"\\addplot[color=green, mark=star] plot coordinates {");
      break;
    case 3:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=star] plot coordinates {");
      break;
    case 4:
      fprintf(tikz_fd,"\\addplot[color=black, mark=star] plot coordinates {");
      break;
    case 5:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=o] plot coordinates {");
      break;
    case 6:
      fprintf(tikz_fd,"\\addplot[color=red, mark=o] plot coordinates {");
      break;
    case 7:
      fprintf(tikz_fd,"\\addplot[color=green, mark=o] plot coordinates {");
      break;
    case 8:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=o] plot coordinates {");
      break;
    case 9:
      fprintf(tikz_fd,"\\addplot[color=black, mark=o] plot coordinates {");
      break;
    case 10:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=square] plot coordinates {");
      break;
    case 11:
      fprintf(tikz_fd,"\\addplot[color=red, mark=square] plot coordinates {");
      break;
    case 12:
      fprintf(tikz_fd,"\\addplot[color=green, mark=square] plot coordinates {");
      break;
    case 13:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=square] plot coordinates {");
      break;
    case 14:
      fprintf(tikz_fd,"\\addplot[color=black, mark=square] plot coordinates {");
      break;
    case 15:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=diamond] plot coordinates {");
      break;
    case 16:
      fprintf(tikz_fd,"\\addplot[color=red, mark=diamond] plot coordinates {");
      break;
    case 17:
      fprintf(tikz_fd,"\\addplot[color=green, mark=diamond] plot coordinates {");
      break;
    case 18:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=diamond] plot coordinates {");
      break;
    case 19:
      fprintf(tikz_fd,"\\addplot[color=black, mark=diamond] plot coordinates {");
      break;
    case 20:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=x] plot coordinates {");
      break;
    case 21:
      fprintf(tikz_fd,"\\addplot[color=red, mark=x] plot coordinates {");
      break;
    case 22:
      fprintf(tikz_fd,"\\addplot[color=green, mark=x] plot coordinates {");
      break;
    case 23:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=x] plot coordinates {");
      break;
    case 24:
      fprintf(tikz_fd,"\\addplot[color=black, mark=x] plot coordinates {");
      break;
    case 25:
      fprintf(tikz_fd,"\\addplot[color=blue, mark=x] plot coordinates {");
      break;
    case 26:
      fprintf(tikz_fd,"\\addplot[color=red, mark=+] plot coordinates {");
      break;
    case 27:
      fprintf(tikz_fd,"\\addplot[color=green, mark=+] plot coordinates {");
      break;
    case 28:
      fprintf(tikz_fd,"\\addplot[color=yellow, mark=+] plot coordinates {");
      break;
    }
  */

  UE->pdcch_vars[0]->crnti = n_rnti;

  // Fill in UL_alloc
  UL_alloc_pdu.type    = 0;
  UL_alloc_pdu.hopping = 0;
  UL_alloc_pdu.rballoc = UL_RB_ALLOC;
  UL_alloc_pdu.mcs     = 1;
  UL_alloc_pdu.ndi     = 1;
  UL_alloc_pdu.TPC     = 0;
  UL_alloc_pdu.cqi_req = 1;

  CCCH_alloc_pdu.type               = 0;
  CCCH_alloc_pdu.vrb_type           = 0;
  CCCH_alloc_pdu.rballoc            = CCCH_RB_ALLOC;
  CCCH_alloc_pdu.ndi      = 1;
  CCCH_alloc_pdu.mcs      = 1;
  CCCH_alloc_pdu.harq_pid = 0;

  DLSCH_alloc_pdu2_1E[0].rah              = 0;
  DLSCH_alloc_pdu2_1E[0].rballoc          = DLSCH_RB_ALLOC;
  DLSCH_alloc_pdu2_1E[0].TPC              = 0;
  DLSCH_alloc_pdu2_1E[0].dai              = 0;
  DLSCH_alloc_pdu2_1E[0].harq_pid         = 0;
  //DLSCH_alloc_pdu2_1E[0].tb_swap          = 0;
  DLSCH_alloc_pdu2_1E[0].mcs             = mcs1;
  DLSCH_alloc_pdu2_1E[0].ndi             = 1;
  DLSCH_alloc_pdu2_1E[0].rv              = 0;
  // Forget second codeword
  DLSCH_alloc_pdu2_1E[0].tpmi             = (transmission_mode>=5 ? 5 : 0);  // precoding
  DLSCH_alloc_pdu2_1E[0].dl_power_off     = (transmission_mode==5 ? 0 : 1);

  DLSCH_alloc_pdu2_1E[1].rah              = 0;
  DLSCH_alloc_pdu2_1E[1].rballoc          = DLSCH_RB_ALLOC;
  DLSCH_alloc_pdu2_1E[1].TPC              = 0;
  DLSCH_alloc_pdu2_1E[1].dai              = 0;
  DLSCH_alloc_pdu2_1E[1].harq_pid         = 0;
  //DLSCH_alloc_pdu2_1E[1].tb_swap          = 0;
  DLSCH_alloc_pdu2_1E[1].mcs             = mcs_i;
  DLSCH_alloc_pdu2_1E[1].ndi             = 1;
  DLSCH_alloc_pdu2_1E[1].rv              = 0;
  // Forget second codeword
  DLSCH_alloc_pdu2_1E[1].tpmi             = (transmission_mode>=5 ? 5 : 0) ;  // precoding
  DLSCH_alloc_pdu2_1E[1].dl_power_off     = (transmission_mode==5 ? 0 : 1);

  eNB2UE[0] = new_channel_desc_scm(eNB->frame_parms.nb_antennas_tx,
                                   UE->frame_parms.nb_antennas_rx,
                                   channel_model,
                                   N_RB2sampling_rate(eNB->frame_parms.N_RB_DL),
				   N_RB2channel_bandwidth(eNB->frame_parms.N_RB_DL),
                                   forgetting_factor,
                                   rx_sample_offset,
                                   0);

  if(num_rounds>1) {
    for(n=1; n<4; n++)
      eNB2UE[n] = new_channel_desc_scm(eNB->frame_parms.nb_antennas_tx,
                                       UE->frame_parms.nb_antennas_rx,
                                       channel_model,
				       N_RB2sampling_rate(eNB->frame_parms.N_RB_DL),
				       N_RB2channel_bandwidth(eNB->frame_parms.N_RB_DL),
				       forgetting_factor,
                                       rx_sample_offset,
                                       0);
  }

  if (eNB2UE[0]==NULL) {
    msg("Problem generating channel model. Exiting.\n");
    exit(-1);
  }

  if ((transmission_mode == 3) || (transmission_mode==4))
    Kmimo=2;
  else
    Kmimo=1;

  switch (ue_category) {
  case 1:
    Nsoft = 250368;
    break;
  case 2:
  case 3:
    Nsoft = 1237248;
    break;
  case 4:
    Nsoft = 1827072;
    break;
  default:
    printf("Unsupported UE category %d\n",ue_category);
    exit(-1);
    break;
  }

  for (k=0; k<n_users; k++) {
    // Create transport channel structures for 2 transport blocks (MIMO)
    for (i=0; i<2; i++) {
      eNB->dlsch[k][i] = new_eNB_dlsch(Kmimo,8,Nsoft,N_RB_DL,0);

      if (!eNB->dlsch[k][i]) {
        printf("Can't get eNB dlsch structures\n");
        exit(-1);
      }

      eNB->dlsch[k][i]->rnti = n_rnti+k;
    }
  }

  for (i=0; i<2; i++) {
    UE->dlsch[0][i]  = new_ue_dlsch(Kmimo,8,Nsoft,MAX_TURBO_ITERATIONS,N_RB_DL,0);

    if (!UE->dlsch[0][i]) {
      printf("Can't get ue dlsch structures\n");
      exit(-1);
    }

    UE->dlsch[0][i]->rnti   = n_rnti;
  }

  // structure for SIC at UE
  UE->dlsch_eNB[0] = new_eNB_dlsch(Kmimo,8,Nsoft,N_RB_DL,0);

  if (DLSCH_alloc_pdu2_1E[0].tpmi == 5) {

    eNB->UE_stats[0].DL_pmi_single = (unsigned short)(taus()&0xffff);

    if (n_users>1)
      eNB->UE_stats[1].DL_pmi_single = (eNB->UE_stats[0].DL_pmi_single ^ 0x1555); //opposite PMI
  } else {
    eNB->UE_stats[0].DL_pmi_single = 0;

    if (n_users>1)
      eNB->UE_stats[1].DL_pmi_single = 0;
  }


  if (input_fd==NULL) {


    // UE specific DCI
    fill_DCI(eNB,
	     &dci_alloc[0],
	     subframe,
	     n_rnti,
	     n_users,
	     transmission_mode,
	     common_flag,
	     DLSCH_RB_ALLOC,
	     TPC,
	     mcs1,
	     mcs2,
	     0,
	     0,
	     &num_common_dci,
	     &num_ue_spec_dci,
	     &num_dci);

    numCCE = get_nCCE(num_pdcch_symbols,&eNB->frame_parms,get_mi(&eNB->frame_parms,subframe));

    if (n_frames==1) printf("num_pdcch_symbols %d, numCCE %d, num_dci %d/%d/%d\n",num_pdcch_symbols,numCCE, num_dci,num_ue_spec_dci,num_common_dci);




    for (k=0; k<n_users; k++) {

      input_buffer_length0 = eNB->dlsch[k][0]->harq_processes[0]->TBS/8;
      input_buffer0[k] = (unsigned char *)malloc(input_buffer_length0+4);
      memset(input_buffer0[k],0,input_buffer_length0+4);
      input_buffer_length1 = eNB->dlsch[k][1]->harq_processes[0]->TBS/8;
      input_buffer1[k] = (unsigned char *)malloc(input_buffer_length1+4);
      memset(input_buffer1[k],0,input_buffer_length1+4);

      if (input_trch_file==0) {
        for (i=0; i<input_buffer_length0; i++) {
          //input_buffer0[k][i] = (unsigned char)(i&0xff);
          input_buffer0[k][i] = (unsigned char)(taus()&0xff);
        }

        for (i=0; i<input_buffer_length1; i++) {
          input_buffer1[k][i]= (unsigned char)(taus()&0xff);
        }
      }

      else {
        i=0;

        while ((!feof(input_trch_fd)) && (i<input_buffer_length0<<3)) {
          ret=fscanf(input_trch_fd,"%s",input_trch_val);

          if (input_trch_val[0] == '1')
            input_buffer0[k][i>>3]+=(1<<(7-(i&7)));

          if (i<16)
            printf("input_trch_val %d : %c\n",i,input_trch_val[0]);

          i++;

          if (((i%8) == 0) && (i<17))
            printf("%x\n",input_buffer0[k][(i-1)>>3]);
        }

        printf("Read in %d bits\n",i);
      }
    }
  }

  // this is for user 0 only
  coded_bits_per_codeword = get_G(&eNB->frame_parms,
                                  eNB->dlsch[0][0]->harq_processes[0]->nb_rb,
                                  eNB->dlsch[0][0]->harq_processes[0]->rb_alloc,
                                  get_Qm(eNB->dlsch[0][0]->harq_processes[0]->mcs),
                                  eNB->dlsch[0][0]->harq_processes[0]->Nl,
                                  num_pdcch_symbols,
                                  0,subframe);

  uncoded_ber_bit = (short*) malloc(sizeof(short)*coded_bits_per_codeword);
  printf("uncoded_ber_bit=%p\n",uncoded_ber_bit);

  snr_step = input_snr_step;
  UE->high_speed_flag = 1;
  UE->ch_est_alpha=0;

  for (ch_realization=0; ch_realization<n_ch_rlz; ch_realization++) {
    if(abstx) {
      printf("**********************Channel Realization Index = %d **************************\n", ch_realization);
    }

    for (SNR=snr0; SNR<snr1; SNR+=snr_step) {
      UE->proc.proc_rxtx[subframe&1].frame_rx=0;
      errs[0]=0;
      errs[1]=0;
      errs[2]=0;
      errs[3]=0;
      errs2[0]=0;
      errs2[1]=0;
      errs2[2]=0;
      errs2[3]=0;
      round_trials[0] = 0;
      round_trials[1] = 0;
      round_trials[2] = 0;
      round_trials[3] = 0;

      dci_errors=0;
      //      avg_ber = 0;

      round=0;
      avg_iter = 0;
      iter_trials=0;
      reset_meas(&eNB->phy_proc_tx); // total eNB tx
      reset_meas(&eNB->dlsch_scrambling_stats);
      reset_meas(&UE->dlsch_unscrambling_stats);
      reset_meas(&eNB->ofdm_mod_stats);
      reset_meas(&eNB->dlsch_modulation_stats);
      reset_meas(&eNB->dlsch_encoding_stats);
      reset_meas(&eNB->dlsch_interleaving_stats);
      reset_meas(&eNB->dlsch_rate_matching_stats);
      reset_meas(&eNB->dlsch_turbo_encoding_stats);

      reset_meas(&UE->phy_proc_rx); // total UE rx
      reset_meas(&UE->ofdm_demod_stats);
      reset_meas(&UE->dlsch_channel_estimation_stats);
      reset_meas(&UE->dlsch_freq_offset_estimation_stats);
      reset_meas(&UE->rx_dft_stats);
      reset_meas(&UE->dlsch_decoding_stats);
      reset_meas(&UE->dlsch_turbo_decoding_stats);
      reset_meas(&UE->dlsch_deinterleaving_stats);
      reset_meas(&UE->dlsch_rate_unmatching_stats);
      reset_meas(&UE->dlsch_tc_init_stats);
      reset_meas(&UE->dlsch_tc_alpha_stats);
      reset_meas(&UE->dlsch_tc_beta_stats);
      reset_meas(&UE->dlsch_tc_gamma_stats);
      reset_meas(&UE->dlsch_tc_ext_stats);
      reset_meas(&UE->dlsch_tc_intl1_stats);
      reset_meas(&UE->dlsch_tc_intl2_stats);
      // initialization
      struct list time_vector_tx;
      initialize(&time_vector_tx);
      struct list time_vector_tx_ifft;
      initialize(&time_vector_tx_ifft);
      struct list time_vector_tx_mod;
      initialize(&time_vector_tx_mod);
      struct list time_vector_tx_enc;
      initialize(&time_vector_tx_enc);

      struct list time_vector_rx;
      initialize(&time_vector_rx);
      struct list time_vector_rx_fft;
      initialize(&time_vector_rx_fft);
      struct list time_vector_rx_demod;
      initialize(&time_vector_rx_demod);
      struct list time_vector_rx_dec;
      initialize(&time_vector_rx_dec);

      eNB_rxtx_proc_t *proc_eNB = &eNB->proc.proc_rxtx[subframe&1];

      for (trials = 0; trials<n_frames; trials++) {
	//printf("Trial %d\n",trials);
        fflush(stdout);
        round=0;

        //if (trials%100==0)
        eNB2UE[0]->first_run = 1;

        UE->dlsch_errors[0] = 1;

        while ((round < num_rounds) && (UE->dlsch_errors[0] > 0)) {
	  //	  printf("Trial %d, round %d\n",trials,round);
          round_trials[round]++;

          if(transmission_mode>=5)
            pmi_feedback=1;
          else
            pmi_feedback=0;

          if (abstx) {
            if (trials==0 && round==0 && SNR==snr0)  //generate a new channel
              hold_channel = 0;
            else
              hold_channel = 1;
          } else
            hold_channel = 0;//(round==0) ? 0 : 1;

	  //PMI_FEEDBACK:

          //  printf("Trial %d : Round %d, pmi_feedback %d \n",trials,round,pmi_feedback);
          for (aa=0; aa<eNB->frame_parms.nb_antennas_tx; aa++) {
            memset(&eNB->common_vars.txdataF[eNB_id][aa][0],0,FRAME_LENGTH_COMPLEX_SAMPLES_NO_PREFIX*sizeof(int32_t));
          }

          if (input_fd==NULL) {


            // Simulate HARQ procedures!!!
	    memset(CCE_table,0,800*sizeof(int));
            if (common_flag == 0) {

	      num_dci=0;
	      num_common_dci=0;
	      num_ue_spec_dci=0;

              if (round == 0) {   // First round
                TB0_active = 1;

                eNB->dlsch[0][0]->harq_processes[0]->rvidx = round&3;
	       
		fill_DCI(eNB,&dci_alloc[0],subframe,n_rnti,n_users,transmission_mode,common_flag,DLSCH_RB_ALLOC,TPC,mcs1,mcs2,trials&1,round&3,&num_common_dci,&num_ue_spec_dci,&num_dci);
	      }
	      else {
		fill_DCI(eNB,&dci_alloc[0],subframe,n_rnti,n_users,transmission_mode,common_flag,DLSCH_RB_ALLOC,TPC,
			 (TB0_active==1)?mcs1:0,mcs2,trials&1,(TB0_active==1)?round&3:0,&num_common_dci,&num_ue_spec_dci,&num_dci);
	      }
	      for (i=num_common_dci; i<num_dci; i++) {
		
		dci_alloc[i].firstCCE = get_nCCE_offset_l1(CCE_table,
							   1<<dci_alloc[i].L,
							   numCCE,
							   (dci_alloc[i].rnti==SI_RNTI)? 1 : 0,
							   dci_alloc[i].rnti,
							   subframe);
		
		if (dci_alloc[i].firstCCE < 0) {
		  printf("firstCCE <0 !! dci %d: rnti %x, format %d : nCCE %d/%d\n",i,dci_alloc[i].rnti, dci_alloc[i].format,
			 dci_alloc[i].firstCCE,numCCE);
		  exit(-1);
		}
		if (n_frames==1)
		  printf("dci %d: rnti %x, format %d : nCCE %d/%d\n",i,dci_alloc[i].rnti, dci_alloc[i].format,
			 dci_alloc[i].firstCCE,numCCE);
	      }

	    } // common_flag == 0
	    


          /*
            else {  // Read signal from file
            i=0;
            while (!feof(input_fd)) {
            fscanf(input_fd,"%s %s",input_val_str,input_val_str2);

            if ((i%4)==0) {
            ((short*)txdata[0])[i/2] = (short)((1<<15)*strtod(input_val_str,NULL));
            ((short*)txdata[0])[(i/2)+1] = (short)((1<<15)*strtod(input_val_str2,NULL));
            if ((i/4)<100)
            printf("sample %d => %e + j%e (%d +j%d)\n",i/4,strtod(input_val_str,NULL),strtod(input_val_str2,NULL),((short*)txdata[0])[i/4],((short*)txdata[0])[(i/4)+1]);//1,input_val2,);
            }
            i++;
            if (i>(FRAME_LENGTH_SAMPLES))
            break;
            }
            printf("Read in %d samples\n",i/4);
            write_output("txsig0.m","txs0", txdata[0],2*frame_parms->samples_per_tti,1,1);
            //    write_output("txsig1.m","txs1", txdata[1],FRAME_LENGTH_COMPLEX_SAMPLES,1,1);
            tx_lev = signal_energy(&txdata[0][0],
            OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES);
            tx_lev_dB = (unsigned int) dB_fixed(tx_lev);
            }
          */
		

	    proc_eNB->subframe_tx = subframe;
	    eNB->abstraction_flag=0;
 
	    phy_procedures_eNB_TX(eNB,proc_eNB,no_relay,NULL,1);
	    
	    
	    start_meas(&eNB->ofdm_mod_stats);
	    
	    do_OFDM_mod_l(eNB->common_vars.txdataF[eNB_id],
			  eNB->common_vars.txdata[eNB_id],
			  (subframe*2),
			  &eNB->frame_parms);
	    
	    do_OFDM_mod_l(eNB->common_vars.txdataF[eNB_id],
			  eNB->common_vars.txdata[eNB_id],
			  (subframe*2)+1,
			  &eNB->frame_parms);
	    
	    stop_meas(&eNB->ofdm_mod_stats);
	    
	    // generate next subframe for channel estimation
	    
	    proc_eNB->subframe_tx = subframe+1;
	    
	    phy_procedures_eNB_TX(eNB,proc_eNB,no_relay,NULL,0);
	    
	    do_OFDM_mod_l(eNB->common_vars.txdataF[eNB_id],
			  eNB->common_vars.txdata[eNB_id],
			  (subframe*2)+2,
			  &eNB->frame_parms);

	    
	    proc_eNB->frame_tx++;

            tx_lev = 0;

            for (aa=0; aa<eNB->frame_parms.nb_antennas_tx; aa++) {
              tx_lev += signal_energy(&eNB->common_vars.txdata[eNB_id][aa]
                                      [subframe*eNB->frame_parms.samples_per_tti],
                                      eNB->frame_parms.samples_per_tti);
            }

            tx_lev_dB = (unsigned int) dB_fixed(tx_lev);

	      
            if (n_frames==1) {
              printf("tx_lev = %d (%d dB)\n",tx_lev,tx_lev_dB);
              write_output("txsig0.m","txs0", &eNB->common_vars.txdata[eNB_id][0][subframe* eNB->frame_parms.samples_per_tti],

                           eNB->frame_parms.samples_per_tti,1,1);
	      write_output("txsigF0.m","txsF0", &eNB->common_vars.txdataF[eNB_id][0][subframe*nsymb*eNB->frame_parms.ofdm_symbol_size],
			   nsymb*eNB->frame_parms.ofdm_symbol_size,1,1);
            }
	  }

	  DL_channel(eNB,UE,subframe,awgn_flag,SNR,tx_lev,hold_channel,abstx,num_rounds,trials,round,eNB2UE,s_re,s_im,r_re,r_im,csv_fd);
	  

	  UE_rxtx_proc_t *proc = &UE->proc.proc_rxtx[subframe&1];
	  proc->subframe_rx = subframe;
	  UE->UE_mode[0] = PUSCH;
	  UE->dlsch_errors[0] = 0;
	  // first symbol has to be done separately in one-shot mode
	  slot_fep(UE,
		   0,
		   (proc->subframe_rx<<1),
		   UE->rx_offset,
		   0,
		   0);
	  
	  if (n_frames==1) printf("Running phy_procedures_UE_RX\n");
	  phy_procedures_UE_RX(UE,proc,0,0,normal_txrx,no_relay,NULL);

	  if (UE->dlsch[0][0]->active == 0) {
	    printf("DCI not received\n");
	    /*
	    write_output("pdcchF0_ext.m","pdcchF_ext", UE->pdcch_vars[eNB_id]->rxdataF_ext[0],2*3*UE->frame_parms.ofdm_symbol_size,1,1);
	    write_output("pdcch00_ch0_ext.m","pdcch00_ch0_ext",UE->pdcch_vars[eNB_id]->dl_ch_estimates_ext[0],12*UE->frame_parms.N_RB_DL*3,1,1);
	    
	    write_output("pdcch_rxF_comp0.m","pdcch0_rxF_comp0",UE->pdcch_vars[eNB_id]->rxdataF_comp[0],4*12*UE->frame_parms.N_RB_DL,1,1);
	    write_output("pdcch_rxF_llr.m","pdcch_llr",UE->pdcch_vars[eNB_id]->llr,12*UE->frame_parms.N_RB_DL*4*2,1,4);
	    write_output("txsigF0.m","txsF0", &eNB->common_vars.txdataF[eNB_id][0][subframe*nsymb*eNB->frame_parms.ofdm_symbol_size],
			 nsymb*eNB->frame_parms.ofdm_symbol_size,1,1);

	    write_output("rxsig0.m","rxs0", &UE->common_vars.rxdata[0][0],10*UE->frame_parms.samples_per_tti,1,1);
	    write_output("rxsigF0.m","rxsF0", &UE->common_vars.rxdataF[0][0],UE->frame_parms.ofdm_symbol_size*nsymb,1,1);

	    exit(-1);
	    */
	  }

	  if ((test_perf ==0 ) && (n_frames==1)) {
	    write_output("ch0.m","ch0",eNB2UE[0]->ch[0],eNB2UE[0]->channel_length,1,8);
	    
	    if (eNB->frame_parms.nb_antennas_tx>1)
	      write_output("ch1.m","ch1",eNB2UE[0]->ch[eNB->frame_parms.nb_antennas_rx],eNB2UE[0]->channel_length,1,8);
	    
	    //common vars
	    write_output("rxsig0.m","rxs0", &UE->common_vars.rxdata[0][0],10*UE->frame_parms.samples_per_tti,1,1);
	    write_output("rxsigF0.m","rxsF0", &UE->common_vars.rxdataF[0][0],UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	    
	    if (UE->frame_parms.nb_antennas_rx>1) {
	      write_output("rxsig1.m","rxs1", UE->common_vars.rxdata[1],UE->frame_parms.samples_per_tti,1,1);
	      write_output("rxsigF1.m","rxsF1", UE->common_vars.rxdataF[1],UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	    }
	    
	    write_output("dlsch00_r0.m","dl00_r0",
			 &(UE->common_vars.dl_ch_estimates[eNB_id][0][0]),
			 UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	    
	    if (UE->frame_parms.nb_antennas_rx>1)
	      write_output("dlsch01_r0.m","dl01_r0",
			   &(UE->common_vars.dl_ch_estimates[eNB_id][1][0]),
			   UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	    
	    if (eNB->frame_parms.nb_antennas_tx>1)
	      write_output("dlsch10_r0.m","dl10_r0",
			   &(UE->common_vars.dl_ch_estimates[eNB_id][2][0]),
			   UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	    
	    if ((UE->frame_parms.nb_antennas_rx>1) && (eNB->frame_parms.nb_antennas_tx>1))
	      write_output("dlsch11_r0.m","dl11_r0",
			   &(UE->common_vars.dl_ch_estimates[eNB_id][3][0]),
			   UE->frame_parms.ofdm_symbol_size*nsymb/2,1,1);
	    
	    //pdsch_vars
	    dump_dlsch2(UE,eNB_id,coded_bits_per_codeword,round);
	    //dump_dlsch2(UE,eNB_id_i,coded_bits_per_codeword);
	    write_output("dlsch_e.m","e",eNB->dlsch[0][0]->harq_processes[0]->e,coded_bits_per_codeword,1,4);
	    
	    //pdcch_vars
	    write_output("pdcchF0_ext.m","pdcchF_ext", UE->pdcch_vars[eNB_id]->rxdataF_ext[0],2*3*UE->frame_parms.ofdm_symbol_size,1,1);
	    write_output("pdcch00_ch0_ext.m","pdcch00_ch0_ext",UE->pdcch_vars[eNB_id]->dl_ch_estimates_ext[0],300*3,1,1);
	    
	    write_output("pdcch_rxF_comp0.m","pdcch0_rxF_comp0",UE->pdcch_vars[eNB_id]->rxdataF_comp[0],4*300,1,1);
	    write_output("pdcch_rxF_llr.m","pdcch_llr",UE->pdcch_vars[eNB_id]->llr,2400,1,4);
	    	    
	  }
	  
	  


          if (UE->dlsch_errors[0] == 0) {

            avg_iter += UE->dlsch[eNB_id][0]->last_iteration_cnt;
            iter_trials++;

            if (n_frames==1)
              printf("No DLSCH errors found (round %d),uncoded ber %f\n",round,uncoded_ber);

            UE->total_TBS[eNB_id] =  UE->total_TBS[eNB_id] + UE->dlsch[eNB_id][0]->harq_processes[UE->dlsch[eNB_id][0]->current_harq_pid]->TBS;
            TB0_active = 0;

               
	  } // DLSCH received ok
	  else {
            errs[round]++;

            avg_iter += UE->dlsch[eNB_id][0]->last_iteration_cnt-1;
            iter_trials++;

            if (n_frames==1) {
              //if ((n_frames==1) || (SNR>=30)) {
              printf("DLSCH errors found (round %d), uncoded ber %f\n",round,uncoded_ber);

              for (s=0; s<UE->dlsch[0][0]->harq_processes[0]->C; s++) {
                if (s<UE->dlsch[0][0]->harq_processes[0]->Cminus)
                  Kr = UE->dlsch[0][0]->harq_processes[0]->Kminus;
                else
                  Kr = UE->dlsch[0][0]->harq_processes[0]->Kplus;

                Kr_bytes = Kr>>3;

                printf("Decoded_output (Segment %d):\n",s);

                for (i=0; i<Kr_bytes; i++)
                  printf("%d : %x (%x)\n",i,UE->dlsch[0][0]->harq_processes[0]->c[s][i],UE->dlsch[0][0]->harq_processes[0]->c[s][i]^eNB->dlsch[0][0]->harq_processes[0]->c[s][i]);
              }

              sprintf(fname,"rxsig0_r%d.m",round);
              sprintf(vname,"rxs0_r%d",round);
              write_output(fname,vname, &UE->common_vars.rxdata[0][0],10*UE->frame_parms.samples_per_tti,1,1);
              sprintf(fname,"rxsigF0_r%d.m",round);
              sprintf(vname,"rxs0F_r%d",round);
              write_output(fname,vname, &UE->common_vars.rxdataF[0][0],UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
	     
              if (UE->frame_parms.nb_antennas_rx>1) {
                sprintf(fname,"rxsig1_r%d.m",round);
                sprintf(vname,"rxs1_r%d.m",round);
                write_output(fname,vname, UE->common_vars.rxdata[1],UE->frame_parms.samples_per_tti,1,1);
                sprintf(fname,"rxsigF1_r%d.m",round);
                sprintf(vname,"rxs1F_r%d.m",round);
                write_output(fname,vname, UE->common_vars.rxdataF[1],UE->frame_parms.ofdm_symbol_size*nsymb,1,1);
              }

              sprintf(fname,"dlsch00_r%d.m",round);
              sprintf(vname,"dl00_r%d",round);
              write_output(fname,vname,
                           &(UE->common_vars.dl_ch_estimates[eNB_id][0][0]),
                           UE->frame_parms.ofdm_symbol_size*nsymb,1,1);

              if (UE->frame_parms.nb_antennas_rx>1) {
                sprintf(fname,"dlsch01_r%d.m",round);
                sprintf(vname,"dl01_r%d",round);
                write_output(fname,vname,
                             &(UE->common_vars.dl_ch_estimates[eNB_id][1][0]),
                             UE->frame_parms.ofdm_symbol_size*nsymb/2,1,1);
              }

              if (eNB->frame_parms.nb_antennas_tx>1) {
                sprintf(fname,"dlsch10_r%d.m",round);
                sprintf(vname,"dl10_r%d",round);
                write_output(fname,vname,
                             &(UE->common_vars.dl_ch_estimates[eNB_id][2][0]),
                             UE->frame_parms.ofdm_symbol_size*nsymb/2,1,1);
              }

              if ((UE->frame_parms.nb_antennas_rx>1) && (eNB->frame_parms.nb_antennas_tx>1)) {
                sprintf(fname,"dlsch11_r%d.m",round);
                sprintf(vname,"dl11_r%d",round);
                write_output(fname,vname,
                             &(UE->common_vars.dl_ch_estimates[eNB_id][3][0]),
                             UE->frame_parms.ofdm_symbol_size*nsymb/2,1,1);
              }

              //pdsch_vars
              dump_dlsch2(UE,eNB_id,coded_bits_per_codeword,round);
              
              //write_output("dlsch_e.m","e",eNB->dlsch[0][0]->harq_processes[0]->e,coded_bits_per_codeword,1,4);
              //write_output("dlsch_ber_bit.m","ber_bit",uncoded_ber_bit,coded_bits_per_codeword,1,0);
              //write_output("dlsch_w.m","w",eNB->dlsch[0][0]->harq_processes[0]->w[0],3*(tbs+64),1,4);
              //write_output("dlsch_w.m","w",UE->dlsch[0][0]->harq_processes[0]->w[0],3*(tbs+64),1,0);
	      //pdcch_vars
	      write_output("pdcchF0_ext.m","pdcchF_ext", UE->pdcch_vars[eNB_id]->rxdataF_ext[0],2*3*UE->frame_parms.ofdm_symbol_size,1,1);
	      write_output("pdcch00_ch0_ext.m","pdcch00_ch0_ext",UE->pdcch_vars[eNB_id]->dl_ch_estimates_ext[0],300*3,1,1);
	      
	      write_output("pdcch_rxF_comp0.m","pdcch0_rxF_comp0",UE->pdcch_vars[eNB_id]->rxdataF_comp[0],4*300,1,1);
	      write_output("pdcch_rxF_llr.m","pdcch_llr",UE->pdcch_vars[eNB_id]->llr,2400,1,4);              
	      
              if (round == 3) exit(-1);
            }

            //      printf("round %d errors %d/%d\n",round,errs[round],trials);

            round++;
            //      UE->dlsch[0][0]->harq_processes[0]->round++;
          }

	  if (xforms==1) {
	    phy_scope_UE(form_ue,
			 UE,
			 eNB_id,
			 0,// UE_id
			 subframe);
	  }

	  UE->proc.proc_rxtx[subframe&1].frame_rx++;
	}  //round

        //      printf("\n");

        if ((errs[0]>=n_frames/10) && (trials>(n_frames/2)))
          break;

        //len = chbch_stats_read(stats_buffer,NULL,0,4096);
        //printf("%s\n\n",stats_buffer);

        if (UE->proc.proc_rxtx[subframe&1].frame_rx % 10 == 0) {
          UE->bitrate[eNB_id] = (UE->total_TBS[eNB_id] - UE->total_TBS_last[eNB_id])*10;
          LOG_D(PHY,"[UE %d] Calculating bitrate: total_TBS = %d, total_TBS_last = %d, bitrate = %d kbits/s\n",UE->Mod_id,UE->total_TBS[eNB_id],UE->total_TBS_last[eNB_id],
                UE->bitrate[eNB_id]/1000);
          UE->total_TBS_last[eNB_id] = UE->total_TBS[eNB_id];
        }




        /* calculate the total processing time for each packet,
         * get the max, min, and number of packets that exceed t>2000us
         */
        double t_tx = (double)eNB->phy_proc_tx.p_time/cpu_freq_GHz/1000.0;
        double t_tx_ifft = (double)eNB->ofdm_mod_stats.p_time/cpu_freq_GHz/1000.0;
        double t_tx_mod = (double)eNB->dlsch_modulation_stats.p_time/cpu_freq_GHz/1000.0;
        double t_tx_enc = (double)eNB->dlsch_encoding_stats.p_time/cpu_freq_GHz/1000.0;


        double t_rx = (double)UE->phy_proc_rx.p_time/cpu_freq_GHz/1000.0;
        double t_rx_fft = (double)UE->ofdm_demod_stats.p_time/cpu_freq_GHz/1000.0;
        double t_rx_demod = (double)UE->dlsch_rx_pdcch_stats.p_time/cpu_freq_GHz/1000.0;
        double t_rx_dec = (double)UE->dlsch_decoding_stats.p_time/cpu_freq_GHz/1000.0;

        if (t_tx > t_tx_max)
          t_tx_max = t_tx;

        if (t_tx < t_tx_min)
          t_tx_min = t_tx;

        if (t_rx > t_rx_max)
          t_rx_max = t_rx;

        if (t_rx < t_rx_min)
          t_rx_min = t_rx;

        if (t_tx > 2000)
          n_tx_dropped++;

        if (t_rx > 2000)
          n_rx_dropped++;

        push_front(&time_vector_tx, t_tx);
        push_front(&time_vector_tx_ifft, t_tx_ifft);
        push_front(&time_vector_tx_mod, t_tx_mod);
        push_front(&time_vector_tx_enc, t_tx_enc);

        push_front(&time_vector_rx, t_rx);
        push_front(&time_vector_rx_fft, t_rx_fft);
        push_front(&time_vector_rx_demod, t_rx_demod);
        push_front(&time_vector_rx_dec, t_rx_dec);


      }   //trials

      // round_trials[0]: number of code word : goodput the protocol
      double table_tx[time_vector_tx.size];
      totable(table_tx, &time_vector_tx);
      double table_tx_ifft[time_vector_tx_ifft.size];
      totable(table_tx_ifft, &time_vector_tx_ifft);
      double table_tx_mod[time_vector_tx_mod.size];
      totable(table_tx_mod, &time_vector_tx_mod);
      double table_tx_enc[time_vector_tx_enc.size];
      totable(table_tx_enc, &time_vector_tx_enc);

      double table_rx[time_vector_rx.size];
      totable(table_rx, &time_vector_rx);
      double table_rx_fft[time_vector_rx_fft.size];
      totable(table_rx_fft, &time_vector_rx_fft);
      double table_rx_demod[time_vector_rx_demod.size];
      totable(table_rx_demod, &time_vector_rx_demod);
      double table_rx_dec[time_vector_rx_dec.size];
      totable(table_rx_dec, &time_vector_rx_dec);


      // sort table
      qsort (table_tx, time_vector_tx.size, sizeof(double), &compare);
      qsort (table_rx, time_vector_rx.size, sizeof(double), &compare);

      if (dump_table == 1 ) {
        set_component_filelog(USIM);  // file located in /tmp/usim.txt
        int n;
        LOG_F(USIM,"The transmitter raw data: \n");

        for (n=0; n< time_vector_tx.size; n++) {
          printf("%f ", table_tx[n]);
          LOG_F(USIM,"%f ", table_tx[n]);
        }

        LOG_F(USIM,"\n");
        LOG_F(USIM,"The receiver raw data: \n");

        for (n=0; n< time_vector_rx.size; n++) {
          // printf("%f ", table_rx[n]);
          LOG_F(USIM,"%f ", table_rx[n]);
        }

        LOG_F(USIM,"\n");
      }

      double tx_median = table_tx[time_vector_tx.size/2];
      double tx_q1 = table_tx[time_vector_tx.size/4];
      double tx_q3 = table_tx[3*time_vector_tx.size/4];

      double tx_ifft_median = table_tx_ifft[time_vector_tx_ifft.size/2];
      double tx_ifft_q1 = table_tx_ifft[time_vector_tx_ifft.size/4];
      double tx_ifft_q3 = table_tx_ifft[3*time_vector_tx_ifft.size/4];

      double tx_mod_median = table_tx_mod[time_vector_tx_mod.size/2];
      double tx_mod_q1 = table_tx_mod[time_vector_tx_mod.size/4];
      double tx_mod_q3 = table_tx_mod[3*time_vector_tx_mod.size/4];

      double tx_enc_median = table_tx_enc[time_vector_tx_enc.size/2];
      double tx_enc_q1 = table_tx_enc[time_vector_tx_enc.size/4];
      double tx_enc_q3 = table_tx_enc[3*time_vector_tx_enc.size/4];

      double rx_median = table_rx[time_vector_rx.size/2];
      double rx_q1 = table_rx[time_vector_rx.size/4];
      double rx_q3 = table_rx[3*time_vector_rx.size/4];

      double rx_fft_median = table_rx_fft[time_vector_rx_fft.size/2];
      double rx_fft_q1 = table_rx_fft[time_vector_rx_fft.size/4];
      double rx_fft_q3 = table_rx_fft[3*time_vector_rx_fft.size/4];

      double rx_demod_median = table_rx_demod[time_vector_rx_demod.size/2];
      double rx_demod_q1 = table_rx_demod[time_vector_rx_demod.size/4];
      double rx_demod_q3 = table_rx_demod[3*time_vector_rx_demod.size/4];

      double rx_dec_median = table_rx_dec[time_vector_rx_dec.size/2];
      double rx_dec_q1 = table_rx_dec[time_vector_rx_dec.size/4];
      double rx_dec_q3 = table_rx_dec[3*time_vector_rx_dec.size/4];

      double std_phy_proc_tx=0;
      double std_phy_proc_tx_ifft=0;
      double std_phy_proc_tx_mod=0;
      double std_phy_proc_tx_enc=0;

      double std_phy_proc_rx=0;
      double std_phy_proc_rx_fft=0;
      double std_phy_proc_rx_demod=0;
      double std_phy_proc_rx_dec=0;

      effective_rate = ((double)(round_trials[0]-dci_errors)/((double)round_trials[0] + round_trials[1] + round_trials[2] + round_trials[3]));

      printf("\n**********************SNR = %f dB (tx_lev %f)**************************\n",
             SNR,
             (double)tx_lev_dB+10*log10(UE->frame_parms.ofdm_symbol_size/(NB_RB*12)));

      printf("Errors (%d(%d)/%d %d/%d %d/%d %d/%d), Pe = (%e,%e,%e,%e), dci_errors %d/%d, Pe = %e => effective rate %f  (%2.1f%%,%f, %f), normalized delay %f (%f)\n",
             errs[0],
             errs2[0],
             round_trials[0],
             errs[1],
             round_trials[0],
             errs[2],
             round_trials[0],
             errs[3],
             round_trials[0],
             (double)errs[0]/(round_trials[0]),
             (double)errs[1]/(round_trials[0]),
             (double)errs[2]/(round_trials[0]),
             (double)errs[3]/(round_trials[0]),
             dci_errors,
             round_trials[0],
             (double)dci_errors/(round_trials[0]),
             rate*effective_rate,
             100*effective_rate,
             rate,
             rate*get_Qm(UE->dlsch[0][0]->harq_processes[UE->dlsch[0][0]->current_harq_pid]->mcs),
             (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0])/
             (double)eNB->dlsch[0][0]->harq_processes[0]->TBS,
             (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0]));

      if (print_perf==1) {
        printf("eNB TX function statistics (per 1ms subframe)\n\n");
        std_phy_proc_tx = sqrt((double)eNB->phy_proc_tx.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                               2)/eNB->phy_proc_tx.trials - pow((double)eNB->phy_proc_tx.diff/eNB->phy_proc_tx.trials/cpu_freq_GHz/1000,2));
        std_phy_proc_tx_ifft = sqrt((double)eNB->ofdm_mod_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                    2)/eNB->ofdm_mod_stats.trials - pow((double)eNB->ofdm_mod_stats.diff/eNB->ofdm_mod_stats.trials/cpu_freq_GHz/1000,2));
        printf("OFDM_mod time                     :%f us (%d trials)\n",(double)eNB->ofdm_mod_stats.diff/eNB->ofdm_mod_stats.trials/cpu_freq_GHz/1000.0,eNB->ofdm_mod_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_ifft, tx_ifft_median, tx_ifft_q1, tx_ifft_q3);
        printf("Total PHY proc tx                 :%f us (%d trials)\n",(double)eNB->phy_proc_tx.diff/eNB->phy_proc_tx.trials/cpu_freq_GHz/1000.0,eNB->phy_proc_tx.trials);
        printf("|__ Statistcs                           std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus n_dropped: %d packet \n",std_phy_proc_tx, t_tx_max, t_tx_min, tx_median, tx_q1, tx_q3,
               n_tx_dropped);

        std_phy_proc_tx_mod = sqrt((double)eNB->dlsch_modulation_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/eNB->dlsch_modulation_stats.trials - pow((double)eNB->dlsch_modulation_stats.diff/eNB->dlsch_modulation_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH modulation time             :%f us (%d trials)\n",(double)eNB->dlsch_modulation_stats.diff/eNB->dlsch_modulation_stats.trials/cpu_freq_GHz/1000.0,
               eNB->dlsch_modulation_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_mod, tx_mod_median, tx_mod_q1, tx_mod_q3);
        printf("DLSCH scrambling time             :%f us (%d trials)\n",(double)eNB->dlsch_scrambling_stats.diff/eNB->dlsch_scrambling_stats.trials/cpu_freq_GHz/1000.0,
               eNB->dlsch_scrambling_stats.trials);
        std_phy_proc_tx_enc = sqrt((double)eNB->dlsch_encoding_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/eNB->dlsch_encoding_stats.trials - pow((double)eNB->dlsch_encoding_stats.diff/eNB->dlsch_encoding_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH encoding time               :%f us (%d trials)\n",(double)eNB->dlsch_encoding_stats.diff/eNB->dlsch_encoding_stats.trials/cpu_freq_GHz/1000.0,
               eNB->dlsch_modulation_stats.trials);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_tx_enc, tx_enc_median, tx_enc_q1, tx_enc_q3);
        printf("|__ DLSCH turbo encoding time         :%f us (%d trials)\n",
               ((double)eNB->dlsch_turbo_encoding_stats.trials/eNB->dlsch_encoding_stats.trials)*(double)
               eNB->dlsch_turbo_encoding_stats.diff/eNB->dlsch_turbo_encoding_stats.trials/cpu_freq_GHz/1000.0,eNB->dlsch_turbo_encoding_stats.trials);
        printf("|__ DLSCH rate-matching time          :%f us (%d trials)\n",
               ((double)eNB->dlsch_rate_matching_stats.trials/eNB->dlsch_encoding_stats.trials)*(double)
               eNB->dlsch_rate_matching_stats.diff/eNB->dlsch_rate_matching_stats.trials/cpu_freq_GHz/1000.0,eNB->dlsch_rate_matching_stats.trials);
        printf("|__ DLSCH sub-block interleaving time :%f us (%d trials)\n",
               ((double)eNB->dlsch_interleaving_stats.trials/eNB->dlsch_encoding_stats.trials)*(double)
               eNB->dlsch_interleaving_stats.diff/eNB->dlsch_interleaving_stats.trials/cpu_freq_GHz/1000.0,eNB->dlsch_interleaving_stats.trials);

        printf("\n\nUE RX function statistics (per 1ms subframe)\n\n");
        std_phy_proc_rx = sqrt((double)UE->phy_proc_rx.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                               2)/UE->phy_proc_rx.trials - pow((double)UE->phy_proc_rx.diff/UE->phy_proc_rx.trials/cpu_freq_GHz/1000,2));
        printf("Total PHY proc rx                                   :%f us (%d trials)\n",(double)UE->phy_proc_rx.diff/UE->phy_proc_rx.trials/cpu_freq_GHz/1000.0,
               UE->phy_proc_rx.trials*2/3);
        printf("|__Statistcs                                            std: %fus max: %fus min: %fus median %fus q1 %fus q3 %fus n_dropped: %d packet \n", std_phy_proc_rx, t_rx_max, t_rx_min, rx_median,
               rx_q1, rx_q3, n_rx_dropped);
        std_phy_proc_rx_fft = sqrt((double)UE->ofdm_demod_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/UE->ofdm_demod_stats.trials - pow((double)UE->ofdm_demod_stats.diff/UE->ofdm_demod_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH OFDM demodulation and channel_estimation time :%f us (%d trials)\n",(nsymb)*(double)UE->ofdm_demod_stats.diff/UE->ofdm_demod_stats.trials/cpu_freq_GHz/1000.0,
               UE->ofdm_demod_stats.trials*2/3);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_fft, rx_fft_median, rx_fft_q1, rx_fft_q3);
        printf("|__ DLSCH rx dft                                        :%f us (%d trials)\n",
               (nsymb*UE->frame_parms.nb_antennas_rx)*(double)UE->rx_dft_stats.diff/UE->rx_dft_stats.trials/cpu_freq_GHz/1000.0,UE->rx_dft_stats.trials*2/3);
        printf("|__ DLSCH channel estimation time                       :%f us (%d trials)\n",
               (4.0)*(double)UE->dlsch_channel_estimation_stats.diff/UE->dlsch_channel_estimation_stats.trials/cpu_freq_GHz/1000.0,UE->dlsch_channel_estimation_stats.trials*2/3);
        printf("|__ DLSCH frequency offset estimation time              :%f us (%d trials)\n",
               (4.0)*(double)UE->dlsch_freq_offset_estimation_stats.diff/UE->dlsch_freq_offset_estimation_stats.trials/cpu_freq_GHz/1000.0,
               UE->dlsch_freq_offset_estimation_stats.trials*2/3);
        printf("DLSCH rx pdcch                                       :%f us (%d trials)\n",(double)UE->dlsch_rx_pdcch_stats.diff/UE->dlsch_rx_pdcch_stats.trials/cpu_freq_GHz/1000.0,
               UE->dlsch_rx_pdcch_stats.trials);
        std_phy_proc_rx_demod = sqrt((double)UE->dlsch_llr_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                     2)/UE->dlsch_llr_stats.trials - pow((double)UE->dlsch_llr_stats.diff/UE->dlsch_llr_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH Channel Compensation and LLR generation time  :%f us (%d trials)\n",(14-num_pdcch_symbols)*(double)UE->dlsch_llr_stats.diff/UE->dlsch_llr_stats.trials/cpu_freq_GHz/1000.0,
               UE->dlsch_llr_stats.trials/3);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_demod, rx_demod_median, rx_demod_q1, rx_demod_q3);
        printf("DLSCH unscrambling time                             :%f us (%d trials)\n",(double)UE->dlsch_unscrambling_stats.diff/UE->dlsch_unscrambling_stats.trials/cpu_freq_GHz/1000.0,
               UE->dlsch_unscrambling_stats.trials);
        std_phy_proc_rx_dec = sqrt((double)UE->dlsch_decoding_stats.diff_square/pow(cpu_freq_GHz,2)/pow(1000,
                                   2)/UE->dlsch_decoding_stats.trials - pow((double)UE->dlsch_decoding_stats.diff/UE->dlsch_decoding_stats.trials/cpu_freq_GHz/1000,2));
        printf("DLSCH Decoding time (%02.2f Mbit/s, avg iter %1.2f)    :%f us (%d trials, max %f)\n",
               eNB->dlsch[0][0]->harq_processes[0]->TBS/1000.0,(double)avg_iter/iter_trials,
               (double)UE->dlsch_decoding_stats.diff/UE->dlsch_decoding_stats.trials/cpu_freq_GHz/1000.0,UE->dlsch_decoding_stats.trials,
               (double)UE->dlsch_decoding_stats.max/cpu_freq_GHz/1000.0);
        printf("|__ Statistcs                           std: %fus median %fus q1 %fus q3 %fus \n",std_phy_proc_rx_dec, rx_dec_median, rx_dec_q1, rx_dec_q3);
        printf("|__ DLSCH Rate Unmatching                               :%f us (%d trials)\n",
               (double)UE->dlsch_rate_unmatching_stats.diff/UE->dlsch_rate_unmatching_stats.trials/cpu_freq_GHz/1000.0,UE->dlsch_rate_unmatching_stats.trials);
        printf("|__ DLSCH Turbo Decoding(%d bits)                       :%f us (%d trials)\n",
               UE->dlsch[0][0]->harq_processes[0]->Cminus ? UE->dlsch[0][0]->harq_processes[0]->Kminus : UE->dlsch[0][0]->harq_processes[0]->Kplus,
               (double)UE->dlsch_turbo_decoding_stats.diff/UE->dlsch_turbo_decoding_stats.trials/cpu_freq_GHz/1000.0,UE->dlsch_turbo_decoding_stats.trials);
        printf("    |__ init                                            %f us (cycles/iter %f, %d trials)\n",
               (double)UE->dlsch_tc_init_stats.diff/UE->dlsch_tc_init_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_init_stats.diff/UE->dlsch_tc_init_stats.trials/((double)avg_iter/iter_trials),
               UE->dlsch_tc_init_stats.trials);
        printf("    |__ alpha                                           %f us (cycles/iter %f, %d trials)\n",
               (double)UE->dlsch_tc_alpha_stats.diff/UE->dlsch_tc_alpha_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_alpha_stats.diff/UE->dlsch_tc_alpha_stats.trials*2,
               UE->dlsch_tc_alpha_stats.trials);
        printf("    |__ beta                                            %f us (cycles/iter %f,%d trials)\n",
               (double)UE->dlsch_tc_beta_stats.diff/UE->dlsch_tc_beta_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_beta_stats.diff/UE->dlsch_tc_beta_stats.trials*2,
               UE->dlsch_tc_beta_stats.trials);
        printf("    |__ gamma                                           %f us (cycles/iter %f,%d trials)\n",
               (double)UE->dlsch_tc_gamma_stats.diff/UE->dlsch_tc_gamma_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_gamma_stats.diff/UE->dlsch_tc_gamma_stats.trials*2,
               UE->dlsch_tc_gamma_stats.trials);
        printf("    |__ ext                                             %f us (cycles/iter %f,%d trials)\n",
               (double)UE->dlsch_tc_ext_stats.diff/UE->dlsch_tc_ext_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_ext_stats.diff/UE->dlsch_tc_ext_stats.trials*2,
               UE->dlsch_tc_ext_stats.trials);
        printf("    |__ intl1                                           %f us (cycles/iter %f,%d trials)\n",
               (double)UE->dlsch_tc_intl1_stats.diff/UE->dlsch_tc_intl1_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_intl1_stats.diff/UE->dlsch_tc_intl1_stats.trials,
               UE->dlsch_tc_intl1_stats.trials);
        printf("    |__ intl2+HD+CRC                                    %f us (cycles/iter %f,%d trials)\n",
               (double)UE->dlsch_tc_intl2_stats.diff/UE->dlsch_tc_intl2_stats.trials/cpu_freq_GHz/1000.0,
               (double)UE->dlsch_tc_intl2_stats.diff/UE->dlsch_tc_intl2_stats.trials,
               UE->dlsch_tc_intl2_stats.trials);
      }

      if ((transmission_mode != 3) && (transmission_mode != 4)) {
        fprintf(bler_fd,"%f;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d\n",
                SNR,
                mcs1,
                eNB->dlsch[0][0]->harq_processes[0]->TBS,
                rate,
                errs[0],
                round_trials[0],
                errs[1],
                round_trials[1],
                errs[2],
                round_trials[2],
                errs[3],
                round_trials[3],
                dci_errors);
      } else {
        fprintf(bler_fd,"%f;%d;%d;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d\n",
                SNR,
                mcs1,mcs2,
                eNB->dlsch[0][0]->harq_processes[0]->TBS,
                eNB->dlsch[0][1]->harq_processes[0]->TBS,
                rate,
                errs[0],
                round_trials[0],
                errs[1],
                round_trials[1],
                errs[2],
                round_trials[2],
                errs[3],
                round_trials[3],
                dci_errors);
      }


      if(abstx) { //ABSTRACTION
        blerr[0] = (double)errs[0]/(round_trials[0]);

        if(num_rounds>1) {
          blerr[1] = (double)errs[1]/(round_trials[1]);
          blerr[2] = (double)errs[2]/(round_trials[2]);
          blerr[3] = (double)errs[3]/(round_trials[3]);
          fprintf(csv_fd,"%e,%e,%e,%e;\n",blerr[0],blerr[1],blerr[2],blerr[3]);
        } else {
          fprintf(csv_fd,"%e;\n",blerr[0]);
        }
      } //ABStraction

      if ( (test_perf != 0) && (100 * effective_rate > test_perf )) {
        //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; err0; trials0; err1; trials1; err2; trials2; err3; trials3; dci_err\n");
        if ((transmission_mode != 3) && (transmission_mode != 4)) {
          fprintf(time_meas_fd,"%f;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
                  SNR,
                  mcs1,
                  eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  rate,
                  errs[0],
                  round_trials[0],
                  errs[1],
                  round_trials[1],
                  errs[2],
                  round_trials[2],
                  errs[3],
                  round_trials[3],
                  dci_errors);

          //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; DL_DECOD_ITER; err0; trials0; err1; trials1; err2; trials2; err3; trials3; PE; dci_err;PE;ND;\n");
          fprintf(time_meas_fd,"%f;%d;%d;%f; %2.1f%%;%f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%e;%e;%e;%e;%d;%d;%e;%f;%f;",
                  SNR,
                  mcs1,
                  eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  rate*effective_rate,
                  100*effective_rate,
                  rate,
                  (double)avg_iter/iter_trials,
                  errs[0],
                  round_trials[0],
                  errs[1],
                  round_trials[1],
                  errs[2],
                  round_trials[2],
                  errs[3],
                  round_trials[3],
                  (double)errs[0]/(round_trials[0]),
                  (double)errs[1]/(round_trials[0]),
                  (double)errs[2]/(round_trials[0]),
                  (double)errs[3]/(round_trials[0]),
                  dci_errors,
                  round_trials[0],
                  (double)dci_errors/(round_trials[0]),
                  (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0])/
                  (double)eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0]));
        } else {
          fprintf(time_meas_fd,"%f;%d;%d;%d;%d;%f;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
                  SNR,
                  mcs1,mcs2,
                  eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  eNB->dlsch[0][1]->harq_processes[0]->TBS,
                  rate,
                  errs[0],
                  round_trials[0],
                  errs[1],
                  round_trials[1],
                  errs[2],
                  round_trials[2],
                  errs[3],
                  round_trials[3],
                  dci_errors);

          //fprintf(time_meas_fd,"SNR; MCS; TBS; rate; DL_DECOD_ITER; err0; trials0; err1; trials1; err2; trials2; err3; trials3; PE; dci_err;PE;ND;\n");
          fprintf(time_meas_fd,"%f;%d;%d;%d;%d;%f;%2.1f;%f;%f;%d;%d;%d;%d;%d;%d;%d;%d;%e;%e;%e;%e;%d;%d;%e;%f;%f;",
                  SNR,
                  mcs1,mcs2,
                  eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  eNB->dlsch[0][1]->harq_processes[0]->TBS,
                  rate*effective_rate,
                  100*effective_rate,
                  rate,
                  (double)avg_iter/iter_trials,
                  errs[0],
                  round_trials[0],
                  errs[1],
                  round_trials[1],
                  errs[2],
                  round_trials[2],
                  errs[3],
                  round_trials[3],
                  (double)errs[0]/(round_trials[0]),
                  (double)errs[1]/(round_trials[0]),
                  (double)errs[2]/(round_trials[0]),
                  (double)errs[3]/(round_trials[0]),
                  dci_errors,
                  round_trials[0],
                  (double)dci_errors/(round_trials[0]),
                  (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0])/
                  (double)eNB->dlsch[0][0]->harq_processes[0]->TBS,
                  (1.0*(round_trials[0]-errs[0])+2.0*(round_trials[1]-errs[1])+3.0*(round_trials[2]-errs[2])+4.0*(round_trials[3]-errs[3]))/((double)round_trials[0]));
        }

        //fprintf(time_meas_fd,"eNB_PROC_TX(%d); OFDM_MOD(%d); DL_MOD(%d); DL_SCR(%d); DL_ENC(%d); UE_PROC_RX(%d); OFDM_DEMOD_CH_EST(%d); RX_PDCCH(%d); CH_COMP_LLR(%d); DL_USCR(%d); DL_DECOD(%d);\n",
        fprintf(time_meas_fd,"%d; %d; %d; %d; %d; %d; %d; %d; %d; %d; %d;",
                eNB->phy_proc_tx.trials,
                eNB->ofdm_mod_stats.trials,
                eNB->dlsch_modulation_stats.trials,
                eNB->dlsch_scrambling_stats.trials,
                eNB->dlsch_encoding_stats.trials,
                UE->phy_proc_rx.trials,
                UE->ofdm_demod_stats.trials,
                UE->dlsch_rx_pdcch_stats.trials,
                UE->dlsch_llr_stats.trials,
                UE->dlsch_unscrambling_stats.trials,
                UE->dlsch_decoding_stats.trials
               );
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;%f;",
                get_time_meas_us(&eNB->phy_proc_tx),
                get_time_meas_us(&eNB->ofdm_mod_stats),
                get_time_meas_us(&eNB->dlsch_modulation_stats),
                get_time_meas_us(&eNB->dlsch_scrambling_stats),
                get_time_meas_us(&eNB->dlsch_encoding_stats),
                get_time_meas_us(&UE->phy_proc_rx),
                nsymb*get_time_meas_us(&UE->ofdm_demod_stats),
                get_time_meas_us(&UE->dlsch_rx_pdcch_stats),
                3*get_time_meas_us(&UE->dlsch_llr_stats),
                get_time_meas_us(&UE->dlsch_unscrambling_stats),
                get_time_meas_us(&UE->dlsch_decoding_stats)
               );
        //fprintf(time_meas_fd,"eNB_PROC_TX_STD;eNB_PROC_TX_MAX;eNB_PROC_TX_MIN;eNB_PROC_TX_MED;eNB_PROC_TX_Q1;eNB_PROC_TX_Q3;eNB_PROC_TX_DROPPED;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%d;", std_phy_proc_tx, t_tx_max, t_tx_min, tx_median, tx_q1, tx_q3, n_tx_dropped);

        //fprintf(time_meas_fd,"IFFT;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_ifft, tx_ifft_median, tx_ifft_q1, tx_ifft_q3);

        //fprintf(time_meas_fd,"MOD;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_mod, tx_mod_median, tx_mod_q1, tx_mod_q3);

        //fprintf(time_meas_fd,"ENC;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_tx_enc, tx_enc_median, tx_enc_q1, tx_enc_q3);


        //fprintf(time_meas_fd,"UE_PROC_RX_STD;UE_PROC_RX_MAX;UE_PROC_RX_MIN;UE_PROC_RX_MED;UE_PROC_RX_Q1;UE_PROC_RX_Q3;UE_PROC_RX_DROPPED;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;%f;%f;%d;", std_phy_proc_rx, t_rx_max, t_rx_min, rx_median, rx_q1, rx_q3, n_rx_dropped);

        //fprintf(time_meas_fd,"FFT;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_rx_fft, rx_fft_median, rx_fft_q1, rx_fft_q3);

        //fprintf(time_meas_fd,"DEMOD;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f;", std_phy_proc_rx_demod,rx_demod_median, rx_demod_q1, rx_demod_q3);

        //fprintf(time_meas_fd,"DEC;\n");
        fprintf(time_meas_fd,"%f;%f;%f;%f\n", std_phy_proc_rx_dec, rx_dec_median, rx_dec_q1, rx_dec_q3);


        /*
        fprintf(time_meas_fd,"%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;",
        eNB->phy_proc_tx.trials,
        eNB->ofdm_mod_stats.trials,
        eNB->dlsch_modulation_stats.trials,
        eNB->dlsch_scrambling_stats.trials,
        eNB->dlsch_encoding_stats.trials,
        UE->phy_proc_rx.trials,
        UE->ofdm_demod_stats.trials,
        UE->dlsch_rx_pdcch_stats.trials,
        UE->dlsch_llr_stats.trials,
        UE->dlsch_unscrambling_stats.trials,
        UE->dlsch_decoding_stats.trials);
        */
        printf("[passed] effective rate : %f  (%2.1f%%,%f)): log and break \n",rate*effective_rate, 100*effective_rate, rate );
        break;
      } else if (test_perf !=0 ) {
        printf("[continue] effective rate : %f  (%2.1f%%,%f)): increase snr \n",rate*effective_rate, 100*effective_rate, rate);
      }

      if (((double)errs[0]/(round_trials[0]))<(10.0/n_frames))
        break;
    }// SNR


  } //ch_realization


  fclose(bler_fd);

  if (test_perf !=0)
    fclose (time_meas_fd);

  //fprintf(tikz_fd,"};\n");
  //fclose(tikz_fd);

  if (input_trch_file==1)
    fclose(input_trch_fd);

  if (input_file==1)
    fclose(input_fd);

  if(abstx) { // ABSTRACTION
    fprintf(csv_fd,"];");
    fclose(csv_fd);
  }

  if (uncoded_ber_bit)
    free(uncoded_ber_bit);

  uncoded_ber_bit = NULL;

  for (k=0; k<n_users; k++) {
    free(input_buffer0[k]);
    free(input_buffer1[k]);
    input_buffer0[k]=NULL;
    input_buffer1[k]=NULL;
  }

  printf("Freeing dlsch structures\n");

  for (i=0; i<2; i++) {
    printf("eNB %d\n",i);
    free_eNB_dlsch(eNB->dlsch[0][i]);
    printf("UE %d\n",i);
    free_ue_dlsch(UE->dlsch[0][i]);
  }


  return(0);
}


