#ifndef PDUR_CAN_TP_H
#define PDUR_CAN_TP_H

/*====================================================================================================================*\
 \@file PDU Router CAN Tp

\*====================================================================================================================*/

#include "ComStack_Types.h"

BufReq_ReturnType PduR_CanTpCopyRxData(PduIdType rxPduId, const PduInfoType *pPduInfo, PduLengthType *pBuffer);
BufReq_ReturnType PduR_CanTpCopyTxData(PduIdType txPduId, const PduInfoType *pPduInfo, const RetryInfoType *pRetryInfo, PduLengthType *pAvailableData);
void PduR_CanTpRxIndication(PduIdType rxPduId, Std_ReturnType result);
BufReq_ReturnType PduR_CanTpStartOfReception(PduIdType pduId, const PduInfoType *pPduInfo, PduLengthType tpSduLength, PduLengthType *pBufferSize);
void PduR_CanTpTxConfirmation(PduIdType txPduId, Std_ReturnType result);

#endif