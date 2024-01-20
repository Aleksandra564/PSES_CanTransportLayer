#ifndef CAN_IF_H
#define CAN_IF_H

/*====================================================================================================================*\
 \@file Can Interface


\*====================================================================================================================*/

#include "ComStack_Types.h"

Std_ReturnType CanIf_Transmit(PduIdType txPduId, const PduInfoType *pPduInfo);

#endif