/**===================================================================================================================*\
  @file CanTp.c

  @brief Can Transport Layer
  
  Implementation of Can Transport Layer

  @see CanTp.pdf
\*====================================================================================================================*/

/*====================================================================================================================*\
    Załączenie nagłówków
\*====================================================================================================================*/
#include "CanTp.h"
#include "CanIf.h"      // 8.5.1 in specification
#include "Pdur_CanTp.h"

/*====================================================================================================================*\
    Makra lokalne
\*====================================================================================================================*/
#define NE_NULL_PTR( ptr ) (ptr != NULL)

#define SF_ID 0x0 // single frame
#define FF_ID 0x1 // first frame
#define CF_ID 0x2 // consecutive frame
#define FC_ID 0x3 // flow control frame

#define DEFAULT_ST 0

#define FC_WAIT_FRAME_CTR_MAX 10

/*====================================================================================================================*\
    Typy lokalne
\*====================================================================================================================*/
// ENUM:
typedef enum {
    CAN_TP_ON, 
    CAN_TP_OFF
}CanTpState_Type;

typedef enum {
    CANTP_TX_WAIT,                      // wait for first frame or single frame 
    CANTP_TX_PROCESSING,                // wait for consecutive frame 
    CANTP_TX_PROCESSING_SUSPENDED       // wait for buffer - UWAGA STAN MOZE OKAZAC SIE NIEPOTRZEBNY
} CanTp_TxState_Type;

typedef enum {
    CANTP_RX_WAIT,
    CANTP_RX_PROCESSING,
    CANTP_RX_PROCESSING_SUSPENDED
} CanTp_RxState_Type;

// STRUCT:
typedef struct{
    CanTp_TxState_Type Cantp_TxState;   // stan transmitera
    PduIdType CanTp_Current_TxId;       // id procesowanej wiadomosci 
    uint16 sent_bytes;                  // nuber of sent bytes 
    uint16 message_legth;               // length of message 
    uint8_t next_SN;
    uint16 blocks_to_fc;                // number of block between flow control
} CanTp_Tx_StateVariables_type;

typedef struct{
    uint16 message_length;              //length of message
    uint16 expected_CF_SN;              //expected number of CF
    uint16 sended_bytes;                // number of sended bytes
    CanTp_RxState_Type CanTp_RxState;   // state
    PduIdType CanTp_Current_RxId;       // id of currently processed message
    uint8 blocks_to_next_cts; 
} CanTp_StateVariables_type;

/*====================================================================================================================*\
    Zmienne globalne
\*====================================================================================================================*/
CanTpState_Type CanTp_State; 
CanTp_Tx_StateVariables_type CanTp_Tx_StateVariables;
CanTp_StateVariables_type CanTp_StateVariables;

/*====================================================================================================================*\
    Zmienne lokalne (statyczne)
\*====================================================================================================================*/

/*====================================================================================================================*\
    Deklaracje funkcji lokalnych
\*====================================================================================================================*/


/*====================================================================================================================*\
    Kod globalnych funkcji inline i makr funkcyjnych
\*====================================================================================================================*/

/*====================================================================================================================*\
    Kod funkcji
\*====================================================================================================================*/

/**
  @brief CanTp_Init

  This function initializes the CanTp module.

*/
void CanTp_Init (void){
/* Reset all state variables and change state to can tp on */
    CanTp_Reset_Rx_State_Variables();
    CanTp_ResetTxStateVariables();
    CanTp_State = CAN_TP_ON;
}


/**
  @brief CanTp_GetVersionInfo

  This function returns the version information of the CanTp module.

*/
void CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo);


/**
  @brief CanTp_Shutdown

  This function is called to shutdown the CanTp module.

*/
void CanTp_Shutdown (void){
    /* Reset all state variables and change state to can tp on */
    CanTp_Reset_Rx_State_Variables();
    CanTp_ResetTxStateVariables();
    CanTp_State = CAN_TP_OFF;

}


/**
  @brief CanTp_Transmit

  Requests transmission of a PDU.

*/
Std_ReturnType CanTp_Transmit (PduIdType TxPduId, const PduInfoType* PduInfoPtr){
    BufReq_ReturnType BufReq_State;
    PduLengthType Pdu_Len;
    Std_ReturnType ret = E_OK;
    
    PduInfoType Temp_Pdu;
    uint8_t payload[8];
    Temp_Pdu.SduDataPtr = payload;
    Temp_Pdu.MetaDataPtr = NULL;
    
    if(CanTp_State == CAN_TP_ON){
        if(CanTp_Tx_StateVariables.Cantp_TxState == CANTP_TX_WAIT){
            if(PduInfoPtr->SduLength < 8){
                //Send signle frame
                Temp_Pdu.SduLength = PduInfoPtr->SduLength;
                BufReq_State = PduR_CanTpCopyTxData(TxPduId, &Temp_Pdu, NULL, &Pdu_Len);
                if(BufReq_State == BUFREQ_OK){
                    ret = CanTp_SendSingleFrame(TxPduId, Temp_Pdu.SduDataPtr, PduInfoPtr->SduLength );
                }
                else if(BufReq_State == BUFREQ_E_NOT_OK){
                    CanTp_ResetTxStateVariables();  
                    PduR_CanTpTxConfirmation(TxPduId, E_NOT_OK);
                    ret = E_NOT_OK;
                }
                else {
                    //Start N_Cs timer
                    CanTp_TimerStart(&N_Cs_timer);
                    ret = E_OK;
                }
            }
            else{
                //Send First Frame
                if(CanTp_SendFirstFrame(TxPduId, PduInfoPtr->SduLength) == E_OK){
                    //Transmiter przechodzi w stan Processing_suspended
                    CanTp_Tx_StateVariables.Cantp_TxState = TxPduId;
                    CanTp_Tx_StateVariables.Cantp_TxState = CANTP_TX_PROCESSING_SUSPENDED;
                    CanTp_Tx_StateVariables.message_legth = PduInfoPtr->SduLength;
                    CanTp_Tx_StateVariables.sent_bytes = 0;
                    ret = E_OK;
                }
                else{
                    ret = E_NOT_OK;
                }
            }
        }
        else{
            ret = E_NOT_OK;
        }
    
    }   
    else{ 
        ret = E_NOT_OK;
    }
    return ret;
}


/**
  @brief CanTp_CancelTransmit

  Requests cancellation of an ongoing transmission of a PDU in a lower layer communication module.

*/
Std_ReturnType CanTp_CancelTransmit (PduIdType TxPduId){

    Std_ReturnType ret;             
    if(CanTp_Tx_StateVariables.CanTp_Current_TxId == TxPduId){
        PduR_CanTpTxConfirmation(CanTp_Tx_StateVariables.CanTp_Current_TxId, E_NOT_OK);
        CanTp_ResetTxStateVariables();
        ret = E_OK;
    }
    else{
        ret = E_NOT_OK;
    }
    return ret;
}


/**
  @brief CanTp_CancelReceive

  Requests cancellation of an ongoing reception of a PDU in a lower layer transport protocol module.

*/
Std_ReturnType CanTp_CancelReceive ( PduIdType RxPduId ){
    Std_ReturnType ret;             
    if( CanTp_StateVariables.CanTp_Current_RxId == RxPduId ){
        PduR_CanTpRxIndication (CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
        CanTp_Reset_Rx_State_Variables();
        ret = E_OK;
    }
    else{
        ret = E_NOT_OK;
    }
    return ret;
}


/**
  @brief CanTp_ChangeParameter

  Request to change a specific transport protocol parameter (e.g. block size).

*/
Std_ReturnType CanTp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value){
    Std_ReturnType result = E_NOT_OK;
    CanTp_RxConnection *conn = getRxConnection(id);
    if ((conn != NULL) && (conn->activation == CANTP_RX_WAIT) && (conn->state == CANTP_RX_STATE_FREE) && (value <= 0xFF)){
        switch(parameter){
            case TP_STMIN:
                conn->nsdu->STmin = value;
                result = E_OK;
                break;
            case TP_BS:
                conn->nsdu->bs = value;
                result = E_OK;
                break;
            case TP_BC:
            default:
                break;
        }
    }
    return result;
}


/**
  @brief CanTp_ReadParameter

  This service is used to read the current value of reception parameters BS and STmin for a specified N-SDU.

*/
Std_ReturnType CanTp_ReadParameter(PduIdType id, TPParameterType parameter, uint16 *value){
    Std_ReturnType result = E_NOT_OK;
    CanTp_RxConnection *conn = getRxConnection(id);

    if (conn != NULL){
        uint16 readVal;
`
        switch (parameter){
            case TP_STMIN:
                readVal = conn->nsdu->STmin;
                result = E_OK;
                break;
            case TP_BS:
                readVal = conn->nsdu->bs;
                result = E_OK;
                break;
            case TP_BC:
            default:
                break;
        }
        if ((result == E_OK) && (readVal <= 0xff)){
            *value = readVal;
        } 
        else{
            result = E_NOT_OK;
        }
    }
    return result;
}


/**
  @brief CanTp_MainFunction 

  The main function for scheduling the CAN TP.

*/
// void CanTp_MainFunction (void){      // TO DO!!!!!!!!!!

// }



// Call-back notifications
// void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
// void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
