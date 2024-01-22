#ifndef CAN_TP_H
#define CAN_TP_H

/*====================================================================================================================*\
 \@file CAN Transport Layer
\*====================================================================================================================*/

#include "CanTp_Types.h"
#include "ComStack_Types.h"
#include "Std_Types.h"

#define CANTP_MODULE_ID (0x0Eu)

/**
 * @brief Runtime Errors
 */
#define CANTP_E_PADDING 0x70
#define CANTP_E_INVALID_TATYPE 0x90
#define CANTP_E_OPER_NOT_SUPPORTED 0xA0
#define CANTP_E_COM 0xB0
#define CANTP_E_RX_COM 0xC0
#define CANTP_E_TX_COM 0xD0

/**
 * @brief API ID's for Det_ReportRuntimeError
 */
#define CANTP_INIT_API_ID (0x01u)
#define CANTP_GET_VERSION_INFO_API_ID (0x07u)
#define CANTP_SHUTDOWN_API_ID (0x02u)
#define CANTP_TRANSMIT_API_ID (0x49u)
#define CANTP_CANCEL_TRANSMIT_API_ID (0x4Au)
#define CANTP_CANCEL_RECEIVE_API_ID (0x4Cu)
#define CANTP_CHANGE_PARAMETER_API_ID (0x4Bu)
#define CANTP_READ_PARAMETER_API_ID (0x0Bu)
#define CANTP_RX_INDICATION_API_ID (0x42u)
#define CANTP_MAIN_FUNCTION_API_ID (0x06u)

void CanTp_Init(const CanTp_ConfigType *CfgPtr);
void CanTp_GetVersionInfo(Std_VersionInfoType *versioninfo);
void CanTp_Shutdown(void);
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr);
Std_ReturnType CanTp_CancelTransmit(PduIdType TxPduId);
Std_ReturnType CanTp_CancelReceive(PduIdType RxPduId);
Std_ReturnType CanTp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value);
Std_ReturnType CanTp_ReadParameter(PduIdType id, TPParameterType parameter, uint16 *value);
void CanTp_MainFunction(void);
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr);
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);

#endif /* CAN_TP_H */