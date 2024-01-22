/** ==================================================================================================================*\
  @file CanTp.c

  @brief Testy jednostkowe do CanTp.c
\*====================================================================================================================*/

#include "fff.h"

DEFINE_FFF_GLOBALS; 

#include "acutest.h"
#include "Std_Types.h"

#include "CanTp.c"  

#include <stdio.h>
#include <string.h>

// HELPER FUNCTIONS:
#define PDU_INVALID (PduIdType)0xFFFFFFFF

static PduIdType findNextValidTxPduId(void){
    static uint32 connItr = 0;

    PduIdType pduId = PDU_INVALID;

    for (connItr = 0; pduId == PDU_INVALID; connItr++){
        if (CanTp_State.txConnections[connItr].nsdu){
            pduId = CanTp_State.txConnections[connItr].nsdu->id;
            break;
        }
        connItr = connItr % ARR_SIZE(CanTp_State.txConnections);
    }
    TEST_ASSERT(pduId != PDU_INVALID);
    TEST_MSG("Could not find any PduId -> Modify CanTp config so it have at least one tx nsdu\n");

    return pduId;
}



// FAKE FUNCTIONS
// FAKE_STATIC_VALUE_FUNC2(Std_ReturnType, CanIf_Transmit, PduIdType, const PduInfoType *);
// FAKE_STATIC_VALUE_FUNC4(BufReq_ReturnType, PduR_CanTpCopyTxData, PduIdType, const PduInfoType *, const RetryInfoType *, PduLengthType *);
// FAKE_VOID_FUNC2(PduR_CanTpTxConfirmation, PduIdType, Std_ReturnType);

/*====================================================================================================================*\
    Fake functions
\*====================================================================================================================*/
/**
  @brief Mocks do Det.h
*/
FAKE_VALUE_FUNC(Std_ReturnType, Det_ReportRuntimeError, uint16, uint8, uint8, uint8);
/**
  @brief Mocks do CanIf.h
*/
FAKE_VALUE_FUNC(Std_ReturnType, CanIf_Transmit, PduIdType, const PduInfoType *);
/**
  @brief Mocks do PduR_CanTp.h
*/
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyRxData, PduIdType, const PduInfoType *, PduLengthType *);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyTxData, PduIdType, const PduInfoType *, const RetryInfoType *, PduLengthType *);
FAKE_VOID_FUNC(PduR_CanTpRxIndication, PduIdType, Std_ReturnType);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpStartOfReception, PduIdType, const PduInfoType *, PduLengthType, PduLengthType *);
FAKE_VOID_FUNC(PduR_CanTpTxConfirmation, PduIdType, Std_ReturnType);
//

// MOCKS
/**
  @brief Mocks do Det.h
*/
// Std_ReturnType Det_ReportRuntimeError_MOCK(uint16 moduleId, uint8 instanceId, uint8 apiId, uint8 errorId){
//     (void)moduleId;
//     (void)instanceId;
//     (void)apiId;
//     (void)errorId;
//     return E_OK;
// }

/**
  @brief Mocks do CanIf.h
*/
// Std_ReturnType CanIf_Transmit_MOCK(PduIdType txPduId, const PduInfoType *pPduInfo){
//     (void)txPduId;
//     (void)pPduInfo;
//     return E_OK;
// }

/**
  @brief Mocks do PduR_CanTp.h
*/
// BufReq_ReturnType PduR_CanTpCopyRxData_MOCK(PduIdType rxPduId, const PduInfoType *pPduInfo, PduLengthType *pBuffer){
//     (void)rxPduId;
//     (void)pPduInfo;
//     (void)pBuffer;
//     return BUFREQ_OK;
// }

// BufReq_ReturnType PduR_CanTpCopyTxData_MOCK(PduIdType txPduId, const PduInfoType *pPduInfo, const RetryInfoType *pRetryInfo, PduLengthType *pAvailableData){
//     (void)txPduId;
//     (void)pPduInfo;
//     (void)pRetryInfo;
//     (void)pAvailableData;
//     return BUFREQ_OK;
// }

// void PduR_CanTpRxIndication_MOCK(PduIdType rxPduId, Std_ReturnType result){
//     (void)rxPduId;
//     (void)result;
// }

// BufReq_ReturnType PduR_CanTpStartOfReception_MOCK(PduIdType pduId, const PduInfoType *pPduInfo, PduLengthType tpSduLength, PduLengthType *pBufferSize){
//     (void)pduId;
//     (void)pPduInfo;
//     (void)tpSduLength;
//     (void)pBufferSize;
//     return BUFREQ_OK;
// }

// void PduR_CanTpTxConfirmation_MOCK(PduIdType txPduId, Std_ReturnType result){
//     (void)txPduId;
//     (void)result;
// }



/*====================================================================================================================*\
    Unit Tests
\*====================================================================================================================*/
void Test_Of_CanTp_Init(void){
    CanTp_Init(NULL);
    TEST_CHECK(CanTp_State.activation == CANTP_ON);
}


// void Test_Of_CanTp_Shutdown(void){
//     uint8 data[] = {1,2,3,4,5,6,7,8,9,10};
//     PduInfoType pduInfo = {
//         .SduDataPtr = data,
//         .SduLength = ARR_SIZE(data)
//     };

//     PduIdType pduId = findNextValidTxPduId();
//     CanTp_State.activation = CANTP_ON;

//     CanTp_Transmit(pduId, &pduInfo);
//     CanTp_MainFunction();
//     CanTp_Shutdown();
//     CanTp_MainFunction();

//     TEST_CHECK(CanTp_State.activation == CANTP_OFF);

//     for (int i=0; i < ARR_SIZE(CanTp_State.rxConnections); i++){
//         TEST_CHECK(CanTp_State.rxConnections[i].activation == CANTP_RX_WAIT);
//     }

//     for (int i=0; i < ARR_SIZE(CanTp_State.txConnections); i++){
//         TEST_CHECK(CanTp_State.txConnections[i].activation == CANTP_TX_WAIT);
//     }
// }


// void TestOf_CanTp_Transmit(void){

// }


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


/*
  Lista testów - wpisz tutaj wszystkie funkcje które mają być wykonane jako testy.
*/
TEST_LIST = {
    {"Test_Of_CanTp_Init",Test_Of_CanTp_Init},    // Format to {"nazwa testu", nazwa_funkcji}
    // {"Test_Of_CanTp_Shutdown", Test_Of_CanTp_Shutdown},
    // {"TestOf_CanTp_Transmit", TestOf_CanTp_Transmit},
    // {"TestOf_CanTp_CancelTransmit", TestOf_CanTp_CancelTransmit},
    // {"TestOf_CanTp_CancelReceive", TestOf_CanTp_CancelReceive},
    // {"TestOf_CanTp_ChangeParameter", TestOf_CanTp_ChangeParameter},
    // {"TestOf_CanTp_ReadParameter", TestOf_CanTp_ReadParameter},
    // {"TestOf_CanTp_RxIndication", TestOf_CanTp_RxIndication},
    // {"TestOf_CanTp_TxConfirmation", TestOf_CanTp_TxConfirmation},
    {NULL, NULL}  // To musi być na końcu
};
