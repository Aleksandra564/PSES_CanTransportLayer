/** ==================================================================================================================*\
  @file CanTp.c

  @brief Testy jednostkowe do CanTp.c
\*====================================================================================================================*/

#include "acutest.h"
#include "Std_Types.h"
#include "fff.h"

#include "CanTp.c"  

#include <stdio.h>
#include <string.h>

DEFINE_FFF_GLOBALS; 


// FAKE FUNCTIONS:
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyRxData, PduIdType, const PduInfoType*, PduLengthType*);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyTxData, PduIdType, const PduInfoType*, const RetryInfoType*, PduLengthType*);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpStartOfReception, PduIdType, const PduInfoType*, PduLengthType, PduLengthType*);
FAKE_VOID_FUNC(PduR_CanTpRxIndication, PduIdType, Std_ReturnType);
FAKE_VOID_FUNC(PduR_CanTpTxConfirmation, PduIdType, Std_ReturnType);

FAKE_VALUE_FUNC(Std_ReturnType, CanIf_Transmit, PduIdType, const PduInfoType*);


// MOCKS
PduLengthType* PduR_CanTpCopyRxData_buffSize_array;

BufReq_ReturnType PduR_CanTpCopyRxData_mock(PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr){
    static int i = 0;
    i = PduR_CanTpCopyRxData_fake.call_count - 1;
    *bufferSizePtr = PduR_CanTpCopyRxData_buffSize_array[i];
    return PduR_CanTpCopyRxData_fake.return_val_seq[i];
}

uint8 PduR_CanTpCopyTxData_sdu_data[20][7];
PduLengthType *PduR_CanTpCopyTxData_availableDataPtr; 

BufReq_ReturnType PduR_CanTpCopyTxData_mock(PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr){
    static int i = 0;
    int loop_ctr;
    i = PduR_CanTpCopyTxData_fake.call_count - 1;
    for(loop_ctr = 0; loop_ctr < info->SduLength; loop_ctr++){
      info->SduDataPtr[loop_ctr] = PduR_CanTpCopyTxData_sdu_data[i][loop_ctr];
    }
    *availableDataPtr = PduR_CanTpCopyTxData_availableDataPtr[i];
    return PduR_CanTpCopyTxData_fake.return_val_seq[i];
}

PduLengthType *PduR_CanTpStartOfReception_buffSize_array;

BufReq_ReturnType PduR_CanTpStartOfReception_mock(PduIdType id, const PduInfoType* info, PduLengthType TpSduLength, PduLengthType* bufferSizePtr){
    static int i = 0;
    i = PduR_CanTpStartOfReception_fake.call_count - 1;
    *bufferSizePtr = PduR_CanTpStartOfReception_buffSize_array[i];
   return PduR_CanTpStartOfReception_fake.return_val_seq[i];
}



// UNIT TESTS:
void Test_Of_CanTp_Init(void){
    CanTp_Tx_StateVariables.Cantp_TxState = CANTP_TX_WAIT;
    CanTp_Tx_StateVariables.blocks_to_fc = 1;
    CanTp_Tx_StateVariables.CanTp_Current_TxId = 0x1;
    CanTp_Tx_StateVariables.message_legth = 100;
    CanTp_Tx_StateVariables.sent_bytes = 95;
    CanTp_Tx_StateVariables.next_SN = 0;
    CanTp_Tx_StateVariables.CanTp_Current_TxId = 2;

    CanTp_StateVariables.CanTp_RxState = CANTP_RX_PROCESSING;
    CanTp_StateVariables.blocks_to_next_cts = 1;
    CanTp_StateVariables.CanTp_Current_RxId = 1;
    CanTp_StateVariables.expected_CF_SN = 1;
    CanTp_StateVariables.sended_bytes = 1;

    CanTp_State = CAN_TP_ON;
    // should reset all variables and set state to ON
    CanTp_Shutdown();
    
    TEST_CHECK(CanTp_Tx_StateVariables.Cantp_TxState == CANTP_TX_WAIT);
    TEST_CHECK(CanTp_Tx_StateVariables.CanTp_Current_TxId == 0); 
    TEST_CHECK(CanTp_Tx_StateVariables.blocks_to_fc == 0);
    TEST_CHECK(CanTp_Tx_StateVariables.message_legth == 0);
    TEST_CHECK(CanTp_Tx_StateVariables.sent_bytes == 0);

    TEST_CHECK(CanTp_StateVariables.CanTp_RxState == CANTP_RX_WAIT);
    TEST_CHECK(CanTp_StateVariables.blocks_to_next_cts == 0);
    TEST_CHECK(CanTp_StateVariables.CanTp_Current_RxId == 0);
    TEST_CHECK(CanTp_StateVariables.expected_CF_SN == 0);
    TEST_CHECK(CanTp_StateVariables.sended_bytes == 0);

    TEST_CHECK(CanTp_State == CAN_TP_ON);
}


void Test_Of_CanTp_Shutdown(void){
    CanTp_Tx_StateVariables.Cantp_TxState = CANTP_TX_WAIT;
    CanTp_Tx_StateVariables.blocks_to_fc = 1;
    CanTp_Tx_StateVariables.CanTp_Current_TxId = 0x1;
    CanTp_Tx_StateVariables.message_legth = 100;
    CanTp_Tx_StateVariables.sent_bytes = 95;
    CanTp_Tx_StateVariables.next_SN = 0;
    CanTp_Tx_StateVariables.CanTp_Current_TxId = 2;

    CanTp_StateVariables.CanTp_RxState = CANTP_RX_PROCESSING;
    CanTp_StateVariables.blocks_to_next_cts = 1;
    CanTp_StateVariables.CanTp_Current_RxId = 1;
    CanTp_StateVariables.expected_CF_SN = 1;
    CanTp_StateVariables.sended_bytes = 1;

    CanTp_State = CAN_TP_ON;
    // should reset all variables and set state to OFF
    CanTp_Shutdown();
    
    TEST_CHECK(CanTp_Tx_StateVariables.Cantp_TxState == CANTP_TX_WAIT);
    TEST_CHECK(CanTp_Tx_StateVariables.CanTp_Current_TxId == 0); 
    TEST_CHECK(CanTp_Tx_StateVariables.blocks_to_fc == 0);
    TEST_CHECK(CanTp_Tx_StateVariables.message_legth == 0);
    TEST_CHECK(CanTp_Tx_StateVariables.sent_bytes == 0);

    TEST_CHECK(CanTp_StateVariables.CanTp_RxState == CANTP_RX_WAIT);
    TEST_CHECK(CanTp_StateVariables.blocks_to_next_cts == 0);
    TEST_CHECK(CanTp_StateVariables.CanTp_Current_RxId == 0);
    TEST_CHECK(CanTp_StateVariables.expected_CF_SN == 0);
    TEST_CHECK(CanTp_StateVariables.sended_bytes == 0);

    TEST_CHECK(CanTp_State == CAN_TP_OFF);
}


void TestOf_CanTp_Transmit(void){
    PduIdType PduId = 0x01;
    PduInfoType PduInfo;
    uint8 SduDataPtr[8];
    uint8 *MetaDataPtr;
    PduInfo.MetaDataPtr = MetaDataPtr;
    PduInfo.SduDataPtr = SduDataPtr;
    Std_ReturnType ret; 

    

}


// void TestOf_CanTp_CancelTransmit(void){

// }


// void TestOf_CanTp_CancelReceive(void){

// }


// void TestOf_CanTp_ChangeParameter(void){

// }


// void TestOf_CanTp_ReadParameter(void){

// }


// void TestOf_CanTp_RxIndication(void){

// }


// void TestOf_CanTp_TxConfirmation(void){

// }

,,,,,,

/*
  Lista testów - wpisz tutaj wszystkie funkcje które mają być wykonane jako testy.
*/
TEST_LIST = {
    {"Test_Of_CanTp_Init",Test_Of_CanTp_Init},    // Format to {"nazwa testu", nazwa_funkcji}
    {"Test_Of_CanTp_Shutdown", Test_Of_CanTp_Shutdown},
    // {"TestOf_CanTp_Transmit", TestOf_CanTp_Transmit},
    // {"TestOf_CanTp_CancelTransmit", TestOf_CanTp_CancelTransmit},
    // {"TestOf_CanTp_CancelReceive", TestOf_CanTp_CancelReceive},
    // {"TestOf_CanTp_ChangeParameter", TestOf_CanTp_ChangeParameter},
    // {"TestOf_CanTp_ReadParameter", TestOf_CanTp_ReadParameter},
    // {"TestOf_CanTp_RxIndication", TestOf_CanTp_RxIndication},
    // {"TestOf_CanTp_TxConfirmation", TestOf_CanTp_TxConfirmation},
    {NULL, NULL}  // To musi być na końcu
};
