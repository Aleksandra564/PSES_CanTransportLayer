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
#include "CanTp_Timer.h"

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
typedef enum{
    CAN_TP_ON, 
    CAN_TP_OFF
} CanTpState_Type;

typedef enum{
    CANTP_TX_WAIT,                      // wait for first frame or single frame 
    CANTP_TX_PROCESSING,                // wait for consecutive frame 
    CANTP_TX_PROCESSING_SUSPENDED       // wait for buffer - UWAGA STAN MOZE OKAZAC SIE NIEPOTRZEBNY
} CanTp_TxState_Type;

typedef enum{
    CANTP_RX_WAIT,
    CANTP_RX_PROCESSING,
    CANTP_RX_PROCESSING_SUSPENDED
} CanTp_RxState_Type;

typedef enum{     // used in call-back functions
    SF = 0, // signel frame
    FF = 1, // first frame
    CF = 2, // consecutive frame
    FC = 3, // flow control frame
    UNKNOWN = 4
} frame_type_t;

typedef enum{
    CANTP_RX_STATE_FREE,
    CANTP_RX_STATE_WAIT_CF,
    CANTP_RX_STATE_FC_TX_REQ,
    CANTP_RX_STATE_PROCESSED,
    CANTP_RX_STATE_ABORT,
    CANTP_RX_STATE_INVALID
} CanTp_RxConnectionState;


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

typedef struct{     // used in call-back functions
    frame_type_t frame_type;
    uint32 frame_lenght; 
    uint8 SN; // Sequence Nubmer - only consecutive frame
    uint8 BS; // Block Size - only flow control frame 
    uint8 FS; // Flow Status -only flow control frame 
    uint8 ST; // Sepsration Time - only flow control frame
} CanPCI_Type;

// typedef struct{
//     CanTp_RxNSduState activation;
//     /** Points to nsdu in CanTp_Config.channels */
//     CanTp_RxNSduType *nsdu;
//     CanTp_RxConnectionState state;

//     struct{
//         uint32 ar;
//         uint32 br;
//         uint32 cr;
//     } timer;

//     PduInfoType pduInfo;
//     PduLengthType buffSize;
//     PduLengthType aquiredBuffSize;
//     uint8 sn;
//     uint8 bs;
//     CanTp_FsType fs;
//     CanTp_ConnectionBuffer fcBuf;
// } CanTp_RxConnection;

// static CanTp_RxConnection *getRxConnection(PduIdType PduId){
//     CanTp_RxConnection *rxConnection = NULL;
//     for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections) && !rxConnection;
//          connItr++) {
//         if (CanTp_State.rxConnections[connItr].nsdu != NULL) {
//             if (CanTp_State.rxConnections[connItr].nsdu->id == PduId) {
//                 rxConnection = &CanTp_State.rxConnections[connItr];
//             }
//         }
//     }
//     return rxConnection;
// }

/*====================================================================================================================*\
    Zmienne globalne
\*====================================================================================================================*/
CanTpState_Type CanTp_State; 
CanTp_Tx_StateVariables_type CanTp_Tx_StateVariables;
CanTp_StateVariables_type CanTp_StateVariables;

CanTp_Timer_type N_Ar_timer = {TIMER_NOT_ACTIVE, 0, N_AR_TIMEOUT_VAL};
CanTp_Timer_type N_Br_timer = {TIMER_NOT_ACTIVE, 0, N_BR_TIMEOUT_VAL};
CanTp_Timer_type N_Cr_timer = {TIMER_NOT_ACTIVE, 0, N_CR_TIMEOUT_VAL};

CanTp_Timer_type N_As_timer = {TIMER_NOT_ACTIVE, 0, N_AS_TIMEOUT_VAL};
CanTp_Timer_type N_Bs_timer = {TIMER_NOT_ACTIVE, 0, N_BS_TIMEOUT_VAL};
CanTp_Timer_type N_Cs_timer = {TIMER_NOT_ACTIVE, 0, N_CS_TIMEOUT_VAL};

uint32 FC_Wait_frame_ctr;

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
void CanTp_GetVersionInfo(Std_VersionInfoType* versioninfo){
    if (pVersionInfo != NULL_PTR){
        pVersionInfo->vendorID = 0x00u;
        pVersionInfo->moduleID = (uint16)CANTP_MODULE_ID;
        pVersionInfo->sw_major_version = CANTP_SW_MAJOR_VERSION;
        pVersionInfo->sw_minor_version = CANTP_SW_MINOR_VERSION;
        pVersionInfo->sw_patch_version = CANTP_SW_PATCH_VERSION;
    }
    else{
        CanTp_ReportError(0x00u, CANTP_GET_VERSION_INFO_API_ID, CANTP_E_PARAM_POINTER);
    }
}


/**
  @brief CanTp_Shutdown

  This function is called to shutdown the CanTp module.

*/
void CanTp_Shutdown(void){
    /* Reset all state variables and change state to can tp on */
    CanTp_Reset_Rx_State_Variables();
    CanTp_ResetTxStateVariables();
    CanTp_State = CAN_TP_OFF;
}


/**
  @brief CanTp_Transmit

  Requests transmission of a PDU.

*/
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr){
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
                //Send single frame
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
                else{
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


// /**
//   @brief CanTp_ChangeParameter

//   Request to change a specific transport protocol parameter (e.g. block size).

// */
// Std_ReturnType CanTp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value){
//     Std_ReturnType result = E_NOT_OK;
//     CanTp_RxConnection *conn = getRxConnection(id);
//     if ((conn != NULL) && (conn->activation == CANTP_RX_WAIT) && (conn->state == CANTP_RX_STATE_FREE) && (value <= 0xFF)){
//         switch(parameter){
//             case TP_STMIN:
//                 conn->nsdu->STmin = value;
//                 result = E_OK;
//                 break;
//             case TP_BS:
//                 conn->nsdu->bs = value;
//                 result = E_OK;
//                 break;
//             case TP_BC:
//             default:
//                 break;
//         }
//     }
//     return result;
// }


// /**
//   @brief CanTp_ReadParameter

//   This service is used to read the current value of reception parameters BS and STmin for a specified N-SDU.

// */
// Std_ReturnType CanTp_ReadParameter(PduIdType id, TPParameterType parameter, uint16 *value){
//     Std_ReturnType result = E_NOT_OK;
//     CanTp_RxConnection *conn = getRxConnection(id);

//     if (conn != NULL){
//         uint16 readVal;
// `
//         switch (parameter){
//             case TP_STMIN:
//                 readVal = conn->nsdu->STmin;
//                 result = E_OK;
//                 break;
//             case TP_BS:
//                 readVal = conn->nsdu->bs;
//                 result = E_OK;
//                 break;
//             case TP_BC:
//             default:
//                 break;
//         }
//         if ((result == E_OK) && (readVal <= 0xff)){
//             *value = readVal;
//         } 
//         else{
//             result = E_NOT_OK;
//         }
//     }
//     return result;
// }



/**
  @brief CanTp_MainFunction 

  The main function for scheduling the CAN TP.

*/
void CanTp_MainFunction (void){
    static boolean N_Ar_timeout, N_Br_timeout, N_Cr_timeout;
    static PduLengthType PduLenght;
    static const PduInfoType PduInfoConst = {NULL, NULL, 0};
  //  static PduIdType RxPduId;
    uint16 block_size;
    uint8 separation_time;
    BufReq_ReturnType BufReq_State; 

    /*
    funkcja ma odpowiadać za timeouty oraz za obslugę eventów od timera 

    mamy łącznie 3 timery w przypadku odbierania:

    N_Br - odmierza czas między ramkami FlowControl 
    N_Cr - odmierza czas między ramkami CF
    N_Ar - odmierza czas od wyslania ramki do potwierdzenia wyslania przez Driver

    timery N_Br i N_Cr mają dwa stany:
    - <timer>_active 
    - <timer>_not_active

    jeżeli timer N_Br jest w stanie N_Br_ACTIVE to wartość licznika ma być inkrementowana z każdym wywołaniem funkcji CanTp_MainFunction 
    jeżeli timer N_Br jest w stanie N_Br_NOT_ACVITE to wartość licznika ma się nie zmieniać 

    dokładnie tak samo ma być w przypadku timera N_Cr 

    Generowane Eventy:

    Jeżeli N_Br jest aktywny to:

    -z każdym wywołaniem funkcji CanTp_MainFunction nalezy wywołać funkcję PduR_CanTpCopyRxData() 
    oraz sprawdzać jej parametry, oraz wysyłać funkcję CanTp_Calculate_Available_Blocks( uint16 buffer_size ), która obliczy ilość bloków wymaganie: [SWS_CanTp_00222] 

    -jeżeli funkcja CanTp_Calculate_Available_Blocks() zwroci wartość większą od 0 
     to należy wysłać ramkę FlowControl z parametrem CTS [SWS_CanTp_00224], w tym wypadku 
     nalezy takze zatrzymac timer (przejsc do stanu not_active) oraz wyzerowac jego licznik, a także aktywować 
     timer N_Cr

    -jeżeli timer doliczy do wartości N_Br timeout, należy inkrementować to należy wyslać ramkę FlowControl 
    z argumentem WAIT oraz inkrementować licznik wysłanych ramek WAIT [SWS_CanTp_00341]

    -jeżeli licznik wysłanych ramek FlowControl WAIT się przepełni to mamy problem, musimy zgłosić problem
    wywołujemy PduR_CanTpRxIndication ( RxPduId, E_NOT_OK) i na tym koniec transmisji [SWS_CanTp_00223], nalezy
    zatrzymac timer oraz zresetowac jego licznik 
    */

    // Inkrementacja wszystkich aktywnych timerów
    CanTp_TimerTick(&N_Ar_timer);
    CanTp_TimerTick(&N_Br_timer);
    CanTp_TimerTick(&N_Cr_timer);

    CanTp_TimerTick(&N_As_timer);
    CanTp_TimerTick(&N_Bs_timer);
    CanTp_TimerTick(&N_Cs_timer);

   if(N_Br_timer.state == TIMER_ACTIVE){
// 1   - Sprawdzenie i obsługa wartości zwronej PduR_CanTpCopyRxData
        //[SWS_CanTp_00222] 
       BufReq_State = PduR_CanTpCopyRxData(CanTp_StateVariables.CanTp_Current_RxId, &PduInfoConst, &PduLenght);

       if(BufReq_State == BUFREQ_E_NOT_OK){
           // [SWS_CanTp_00271]
           PduR_CanTpRxIndication(CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
       }
       else{
            block_size = CanTp_Calculate_Available_Blocks(PduLenght);
            if(block_size > 0){
// 3            Wywolanie funkcji CanTp_Set_blocks... i CanTp_Resume jeżeli block_size > 0
                CanTp_set_blocks_to_next_cts(block_size);
                CanTp_Resume();

                //[SWS_CanTp_00224]
                if(CanTp_SendFlowControl(CanTp_StateVariables.CanTp_Current_RxId, block_size, FC_CTS, separation_time) == E_NOT_OK){
// 4   - resetowanie recievera jeżeli SendFlowControl zwróci błąd                   
                    CanTp_Reset_Rx_State_Variables();
                }
                else{
                    //Zresetowanie Timera N_Br:
                    CanTp_TimerReset(&N_Br_timer); 
                }
// 5            - Usuniecie startowania timera
                //Start timera N_Cr:
                //CanTp_TimerStart(&N_Cr_timer);
            }
            //Obsługa timeouta
            if(CanTp_TimerTimeout(&N_Br_timer)){
                FC_Wait_frame_ctr++;  //Inkrementacja licznika ramek WAIT 
                N_Br_timer.counter = 0;
                if(FC_Wait_frame_ctr >= FC_WAIT_FRAME_CTR_MAX){
                    // [SWS_CanTp_00223]
// 2- reset recievera  --Zbyt duza ilość ramek FC_WAIT,
                    PduR_CanTpRxIndication (CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                    CanTp_Reset_Rx_State_Variables();
                    //Zresetowanie Timera N_Br:
                    FC_Wait_frame_ctr = 0;
                }
                else{
                    // [SWS_CanTp_00341]
                    if(CanTp_SendFlowControl(CanTp_StateVariables.CanTp_Current_RxId, block_size, FC_WAIT, separation_time) == E_NOT_OK){
// 4   - resetowanie recievera jeżeli SendFlowControl zwróci błąd
                        CanTp_Reset_Rx_State_Variables();
                    }
                }
            }
        }
   }

   //N_Cr jest w stanie aktywnym
   if(N_Cr_timer.state == TIMER_ACTIVE){
       //N_Cr zgłasza timeout 
       if(CanTp_TimerTimeout(&N_Cr_timer) == E_NOT_OK){
// 2- reset recievera  --Zbyt duza ilość ramek FC_WAIT, 
            // [SWS_CanTp_00223]
            PduR_CanTpRxIndication(CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
            CanTp_Reset_Rx_State_Variables();
       }
   }
   //N_Ar jest w stanie aktywnym
   if(N_Ar_timer.state == TIMER_ACTIVE){
       //N_Ar zgłasza timeout 
       if(CanTp_TimerTimeout(&N_Ar_timer) == E_NOT_OK){
// 2- reset recievera  --Zbyt duza ilość ramek FC_WAIT,             
            // [SWS_CanTp_00223]
            PduR_CanTpRxIndication(CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
            CanTp_Reset_Rx_State_Variables();
       }
   }
// [SWS_CanTp_00167]
    if(N_Cs_timer.state == TIMER_ACTIVE){
       //N_Ar zgłasza timeout
       if(CanTp_TimerTimeout(&N_Cs_timer) == E_NOT_OK){
// 2- timeout - brak odpowiedzi od drivera
            // [SWS_CanTp_00223]
            PduR_CanTpTxConfirmation(CanTp_Tx_StateVariables.CanTp_Current_TxId, E_NOT_OK);
            CanTp_ResetTxStateVariables();
       }
   }
// [SWS_CanTp_00310]
    if(N_As_timer.state == TIMER_ACTIVE){
       //N_Ar zgłasza timeout
       if(CanTp_TimerTimeout(&N_As_timer) == E_NOT_OK){
// 2- timeout - brak odpowiedzi od drivera
            // [SWS_CanTp_00223]
            PduR_CanTpTxConfirmation(CanTp_Tx_StateVariables.CanTp_Current_TxId, E_NOT_OK);
            CanTp_ResetTxStateVariables();
       }
   }
// [SWS_CanTp_00316]
    if(N_Bs_timer.state == TIMER_ACTIVE){
       //N_Ar zgłasza timeout
       if(CanTp_TimerTimeout(&N_Bs_timer) == E_NOT_OK){
// 2 - timeout - brak odpowiedzi od drivera 
            // [SWS_CanTp_00223]
            PduR_CanTpTxConfirmation(CanTp_Tx_StateVariables.CanTp_Current_TxId, E_NOT_OK);
            CanTp_ResetTxStateVariables();
       }
   }
} 



// Call-back notifications
/**
  @brief CanTp_RxIndication

  Indication of a received PDU from a lower layer communication interface module.

*/
void CanTp_RxIndication ( PduIdType RxPduId, const PduInfoType* PduInfoPtr ){
    CanPCI_Type Can_PCI;            // PCI extracted from frame
    PduInfoType Extracted_Data;     // Payload extracted from PDU
    uint8 temp_data[8];             // array for temp payload

    if( CanTp_State == CAN_TP_ON ){
        if( CanTp_StateVariables.CanTp_RxState == CANTP_RX_WAIT){
        //  CanTp_StateVariables.CanTp_Current_RxId = RxPduId; // setting of segmented frame id possible only in waiting state
            CanTp_GetPCI(PduInfoPtr, &Can_PCI);

        /* transmisja danych jeszcze nie wystartowała 
           pierwsza ramka musi być SF albo FF, w przeciwnym wypadku
            wiadomość zostanie zignorowana
        */
            if(Can_PCI.frame_type == FF){
                CanTp_FirstFrameReception(RxPduId, PduInfoPtr, &Can_PCI);
            }
            else if(Can_PCI.frame_type == SF){
                CanTp_SingleFrameReception(RxPduId, &Can_PCI, PduInfoPtr);           
            } 
            else if(Can_PCI.frame_type == FC){
            // accept flow control if Tranmitter is waiting for it
                CanTp_FlowControlReception(RxPduId, &Can_PCI);
            }
            else
            {
            // in this state, CanTP expect only SF or FF, FC other frames shuld vbe ignored
                CanTp_StateVariables.CanTp_RxState = CANTP_RX_WAIT; 
            } 
        }

        else if(CanTp_StateVariables.CanTp_RxState == CANTP_RX_PROCESSING){
        /*
            in this state process only CF and FC, 
            FF and CF cause abort communication and start a new one
        */
             CanTp_GetPCI(PduInfoPtr, &Can_PCI);
            // EXPECTED
             if(Can_PCI.frame_type == CF){
                CanTp_ConsecutiveFrameReception(RxPduId, &Can_PCI, PduInfoPtr);
            }
            else if(Can_PCI.frame_type == FC){
                CanTp_FlowControlReception(RxPduId, &Can_PCI);
            }
       // UNEXPECTED 
        /*
            [SWS_CanTp_00057] ⌈If unexpected frames are received, the CanTp module shall
            behave according to the table below. This table specifies the N-PDU handling
            considering the current CanTp internal status (CanTp status).
        */

            else if(Can_PCI.frame_type == FF){            
            /*
                Table 1: Handling of N-PDU arrivals
                Terminate the current reception, report an indication, with parameter 
                Result set to E_NOT_OK, to the upper layer, 
                and process the FF N-PDU as the start of a new reception
            */
                PduR_CanTpRxIndication ( CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
                CanTp_FirstFrameReception(RxPduId, PduInfoPtr, &Can_PCI);
            }
            else if(Can_PCI.frame_type == SF) {            
            /*
                Terminate the current reception, report an indication, 
                with parameter Result set to E_NOT_OK, to the upper layer, 
                and process the SF N-PDU as the start of a new reception
            */
                PduR_CanTpRxIndication (CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
                CanTp_SingleFrameReception(RxPduId, &Can_PCI, PduInfoPtr);

            }
            else{
                // UNKNOWN FRAME SHOULD BE IGNORED
            }
        }

            // STAN PROCESSING SUSPENDED
        else {    

            CanTp_GetPCI(PduInfoPtr, &Can_PCI);

            if(Can_PCI.frame_type == FC){
                CanTp_FlowControlReception(RxPduId, &Can_PCI);
            }

            else if(Can_PCI.frame_type == FF) {            
            /*
                Table 1: Handling of N-PDU arrivals
                Terminate the current reception, report an indication, with parameter 
                Result set to E_NOT_OK, to the upper layer, 
                and process the FF N-PDU as the start of a new reception
            */
                PduR_CanTpRxIndication ( CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
                CanTp_FirstFrameReception(RxPduId, PduInfoPtr, &Can_PCI);

            }
            else if(Can_PCI.frame_type == SF) {            
            /*
                Terminate the current reception, report an indication, 
                with parameter Result set to E_NOT_OK, to the upper layer, 
                and process the SF N-PDU as the start of a new reception
            */
                PduR_CanTpRxIndication (CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
                CanTp_SingleFrameReception(RxPduId, &Can_PCI, PduInfoPtr);

            }

            else {
                PduR_CanTpRxIndication (CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
            }
        }
    }
}


/**
  @brief CanTp_TxConfirmation

  The lower layer communication interface module confirms the transmission of a PDU, or the failure to transmit a PDU.

*/
void CanTp_TxConfirmation (PduIdType TxPduId, Std_ReturnType result){
    /*
        funkcja jest wywoływana przez niższą warstwę i informuje nas o statusie operacji wysyłania
        w wypadku błędu przerwać wysyłanie lub odbierania (w zależności od tego co jest aktywne)
        w przypadku receivera funkcja powinna zatrzymać timer N_Ar, o ile nie będzie błędu
   */
if( CanTp_State == CAN_TP_ON ){
   // RECEIVER
    if( CanTp_StateVariables.CanTp_Current_RxId == TxPduId ){
        if((CanTp_StateVariables.CanTp_RxState == CANTP_RX_PROCESSING) || (CanTp_StateVariables.CanTp_RxState == CANTP_RX_PROCESSING_SUSPENDED)){

            if(result == E_OK){
                CanTp_TimerReset(&N_Ar_timer);   
            }    
            else{
                // w przypadku receivera tylko wysłanie FlowControl aktywuje ten timer, oznacza to, że nie powinien się on wysypać w stanie innym niż PROCESSING
                PduR_CanTpRxIndication ( CanTp_StateVariables.CanTp_Current_RxId, E_NOT_OK);
                CanTp_Reset_Rx_State_Variables();
            }
        }
        else{
            // IGNORE
        } 
    }

    // TRASMITER 
    // W PRZYPADKU TRANSMITERA MAMY PEWNOSC ŻE RAMKA DOSZŁA (O ILE ZWRÓCIŁ OK ) I MOŻNA ŁADOWAĆ KOLEJNĄ
    if( CanTp_Tx_StateVariables.CanTp_Current_TxId == TxPduId ){
        if(result == E_OK){
            // jeżeli poprzednia ramka przeszła 
            if(CanTp_Tx_StateVariables.Cantp_TxState == CANTP_TX_PROCESSING){
               CanTp_SendNextCF();               
            }
            else{
                // only reset timer
            }
        }
        else{
            PduR_CanTpTxConfirmation(CanTp_Tx_StateVariables.CanTp_Current_TxId, E_NOT_OK);
            CanTp_ResetTxStateVariables();
        }
    }
    else{
        // ignore unknown ID
    }
}
    // UNKNOWN ID WILL BE IGNORED
}

