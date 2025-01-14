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

/*! \file rlc_am_retransmit.h
* \brief This file defines the prototypes of the functions dealing with the retransmission.
* \author GAUTHIER Lionel
* \date 2010-2011
* \version
* \note
* \bug
* \warning
*/
/** @defgroup _rlc_am_internal_retransmit_impl_ RLC AM Retransmitter Internal Reference Implementation
* @ingroup _rlc_am_internal_impl_
* @{
*/
#    ifndef __RLC_AM_RETRANSMIT_H__
#        define __RLC_AM_RETRANSMIT_H__
//-----------------------------------------------------------------------------
#        ifdef RLC_AM_RETRANSMIT_C
#            define private_rlc_am_retransmit(x)    x
#            define protected_rlc_am_retransmit(x)  x
#            define public_rlc_am_retransmit(x)     x
#        else
#            ifdef RLC_AM_MODULE
#                define private_rlc_am_retransmit(x)
#                define protected_rlc_am_retransmit(x)  extern x
#                define public_rlc_am_retransmit(x)     extern x
#            else
#                define private_rlc_am_retransmit(x)
#                define protected_rlc_am_retransmit(x)
#                define public_rlc_am_retransmit(x)     extern x
#            endif
#        endif
/*! \fn void  rlc_am_nack_pdu (const protocol_ctxt_t* const  ctxt_pP, rlc_am_entity_t *rlcP, uint16_t snP, sdu_size_t so_startP, sdu_size_t so_endP)
* \brief      The RLC AM PDU which have the sequence number snP is marked NACKed with segment offset fields.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
* \param[in]  snP          Sequence number of the PDU that is negative acknowledged.
* \param[in]  so_startP    Start of the segment offset of the PDU that .
* \param[in]  so_endP      Transport blocks received from MAC layer.
* \note It may appear a new hole in the retransmission buffer depending on the segment offset informations. Depending on the state of the retransmission buffer, negative confirmation can be sent to higher layers about the drop by the RLC AM instance of a particular SDU.
*/
protected_rlc_am_retransmit(void         rlc_am_nack_pdu (
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t *const rlcP,
                              const rlc_sn_t snP,
                              const sdu_size_t so_startP,
                              const sdu_size_t so_endP);)

/*! \fn void rlc_am_ack_pdu (const protocol_ctxt_t* const  ctxt_pP,rlc_am_entity_t *rlcP, rlc_sn_t snP)
* \brief      The RLC AM PDU which have the sequence number snP is marked ACKed.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
* \param[in]  snP          Sequence number of the PDU that is acknowledged.
* \note                    Depending on the state of the retransmission buffer, positive confirmation can be sent to higher layers about the receiving by the peer RLC AM instance of a particular SDU.
*/
protected_rlc_am_retransmit(void         rlc_am_ack_pdu (
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t *const rlcP,
                              const rlc_sn_t snP);)

/*! \fn mem_block_t* rlc_am_retransmit_get_copy (const protocol_ctxt_t* const  ctxt_pP, rlc_am_entity_t *rlcP, rlc_sn_t snP)
* \brief      The RLC AM PDU which have the sequence number snP is marked ACKed.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
* \param[in]  snP          Sequence number of the PDU to be copied.
* \return                  A copy of the PDU having sequence number equal to parameter snP.
*/
protected_rlc_am_retransmit(mem_block_t* rlc_am_retransmit_get_copy (
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t *const rlcP,
                              const rlc_sn_t snP));

/*! \fn mem_block_t* rlc_am_retransmit_get_subsegment (const protocol_ctxt_t* const  ctxt_pP,rlc_am_entity_t *rlcP,rlc_sn_t snP, sdu_size_t *sizeP)
* \brief      The RLC AM PDU which have the sequence number snP is marked ACKed.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
* \param[in]  snP          Sequence number of the PDU to be copied.
* \param[in,out]  sizeP    Maximum size allowed for the subsegment, it is updated with the amount of bytes not used (sizeP[out] = sizeP[in] - size of segment).
* \return                  A copy of a segment of the PDU having sequence number equal to parameter snP.
*/
protected_rlc_am_retransmit(mem_block_t* rlc_am_retransmit_get_subsegment (
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t *const rlcP,
                              const rlc_sn_t snP,
                              sdu_size_t *const sizeP));

/*! \fn void rlc_am_retransmit_any_pdu(const protocol_ctxt_t* const  ctxt_pP, rlc_am_entity_t* rlcP)
* \brief      Retransmit any PDU in order to unblock peer entity, if no suitable PDU is found (depending on requested MAC size) to be retransmitted, then try to retransmit a subsegment of any PDU.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
*/
protected_rlc_am_retransmit(void rlc_am_retransmit_any_pdu(
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t* const rlcP);)

/*! \fn void rlc_am_tx_buffer_display (const protocol_ctxt_t* const  ctxt_pP,rlc_am_entity_t* rlcP,  char* message_pP)
* \brief      Display the dump of the retransmission buffer.
* \param[in]  ctxtP        Running context.
* \param[in]  rlcP         RLC AM protocol instance pointer.
* \param[in]  message_pP     Message to be displayed along with the display of the dump of the retransmission buffer.
*/
protected_rlc_am_retransmit(void rlc_am_tx_buffer_display (
                              const protocol_ctxt_t* const  ctxt_pP,
                              rlc_am_entity_t* const rlcP,
                              char* const message_pP);)
/** @} */
#    endif
