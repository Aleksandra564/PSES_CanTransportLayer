/** ==================================================================================================================*\
  @file CanTp.c

  @brief Testy jednostkowe do CanTp.c
\*====================================================================================================================*/

/*====================================================================================================================*\
    Includes
\*====================================================================================================================*/
#include "fff.h"

DEFINE_FFF_GLOBALS; 

#include "acutest.h"
#include "Std_Types.h"

#include "CanTp.c"  

#include <stdio.h>
#include <string.h>

/*====================================================================================================================*\
    Helpers
\*====================================================================================================================*/
#define PDU_INVALID (PduIdType)0xFFFFFFFF

#define PDU_PAYLOAD_LEN_1 6
#define PDU_ID_1 102

#define PDU_PAYLOAD_LEN_2 7
#define PDU_ID_2 103

// #define PDU_PAYLOAD_LEN_3 6
// #define PDU_ID_3 104

uint8 testBuffer[64];
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

    return pduId;
}

/*====================================================================================================================*\
    Fake functions and mocks
\*====================================================================================================================*/
/**
  @brief Fake functions do Det.h
*/
FAKE_VALUE_FUNC(Std_ReturnType, Det_ReportRuntimeError, uint16, uint8, uint8, uint8);
/**
  @brief Fake functions do CanIf.h
*/
FAKE_VALUE_FUNC(Std_ReturnType, CanIf_Transmit, PduIdType, const PduInfoType *);
/**
  @brief Fake functions do PduR_CanTp.h
*/
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyRxData, PduIdType, const PduInfoType *, PduLengthType *);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpCopyTxData, PduIdType, const PduInfoType *, const RetryInfoType *, PduLengthType *);
FAKE_VOID_FUNC(PduR_CanTpRxIndication, PduIdType, Std_ReturnType);
FAKE_VALUE_FUNC(BufReq_ReturnType, PduR_CanTpStartOfReception, PduIdType, const PduInfoType *, PduLengthType, PduLengthType *);
FAKE_VOID_FUNC(PduR_CanTpTxConfirmation, PduIdType, Std_ReturnType);

// MOCKS
/**
  @brief Mocks do PduR_CanTp.h
*/
static BufReq_ReturnType PduR_CanTpStartOfReception_MOCK(PduIdType pduId, const PduInfoType *pPduInfo, PduLengthType tpSduLength, PduLengthType *pBufferSize){
    *pBufferSize = tpSduLength;
    return BUFREQ_OK;
}
static BufReq_ReturnType PduR_CanTpCopyRxData_MOCK(PduIdType rxPduId, const PduInfoType *pPduInfo, PduLengthType *pBuffer){
    for (PduLengthType i = 0; i < pPduInfo->SduLength; i++) {
        testBuffer[i] = pPduInfo->SduDataPtr[i];
    }
    return BUFREQ_OK;
}

/*====================================================================================================================*\
    Unit Tests
\*====================================================================================================================*/
void Test_Of_CanTp_Init(void){
    CanTp_Init(NULL);
    TEST_CHECK(CanTp_State.activation == CANTP_ON);
}


void Test_Of_CanTp_Shutdown(void){
    uint8 data[] = {1,2,3,4,5,6,7,8,9,10};
    PduInfoType pduInfo = {.SduDataPtr = data, .SduLength = ARR_SIZE(data)};

    PduIdType pduId = findNextValidTxPduId();
    CanTp_State.activation = CANTP_ON;

    CanTp_Transmit(pduId, &pduInfo);
    CanTp_MainFunction();
    CanTp_Shutdown();
    CanTp_MainFunction();

    TEST_CHECK(CanTp_State.activation == CANTP_OFF);

    for (int i=0; i < ARR_SIZE(CanTp_State.rxConnections); i++){
        TEST_CHECK(CanTp_State.rxConnections[i].activation == CANTP_RX_WAIT);
    }

    for (int i=0; i < ARR_SIZE(CanTp_State.txConnections); i++){
        TEST_CHECK(CanTp_State.txConnections[i].activation == CANTP_TX_WAIT);
    }
}


void TestOf_CanTp_Transmit(void){
    // TEST 1 - valid frame
    uint8 sdu[] = {0xF, 0xA, 0xD, 0xE, 0xD};
    PduInfoType pduInfo = {.SduDataPtr = sdu, .SduLength = ARR_SIZE(sdu),};

    PduIdType pduId = findNextValidTxPduId();
    Std_ReturnType transmitResult;
    uint8 sduLengthPassedToCanIf = ARR_SIZE(sdu) + 1; // 5 payload bytes + 1 CanTp header byte

    CanTp_State.activation = CANTP_ON;
    transmitResult = CanTp_Transmit(pduId, &pduInfo);

    for (int i = 0; i < 2; i++){
        CanTp_MainFunction();
    }

    // Verification of CanIf_Transmit usage
    TEST_CHECK(CanIf_Transmit_fake.call_count == 1);
    TEST_CHECK(CanIf_Transmit_fake.arg0_val == pduId);

    TEST_CHECK(((PduInfoType *)CanIf_Transmit_fake.arg1_val)->SduLength == sduLengthPassedToCanIf);

    // Verification of PduR_CanTpCopyTxData usage
    TEST_CHECK(PduR_CanTpCopyTxData_fake.call_count == 1);
    TEST_CHECK(PduR_CanTpCopyTxData_fake.arg0_val == pduId);

    // Verification of PduR_CanTpTxConfirmation usage
    TEST_CHECK(PduR_CanTpTxConfirmation_fake.call_count == 1);
    TEST_CHECK(PduR_CanTpTxConfirmation_fake.arg0_val == pduId);
    TEST_CHECK(PduR_CanTpTxConfirmation_fake.arg1_val == E_OK);
}


void TestOf_CanTp_CancelTransmit(void){
    uint8 data[] = {1,2,3,4,5,6,7,8,9,10};
    PduInfoType pduInfo = {.SduDataPtr = data, .SduLength = ARR_SIZE(data)};
    PduIdType pduId = findNextValidTxPduId();

    CanTp_State.activation = CANTP_ON;
    CanTp_Transmit(pduId, &pduInfo);

    CanTp_MainFunction();
    TEST_CHECK(E_OK == CanTp_CancelTransmit(pduId));

    CanTp_MainFunction();
    TEST_CHECK(getTxConnection(pduId)->activation == CANTP_TX_WAIT);
}


void TestOf_CanTp_CancelReceive(void){
    uint8 pduPayload[PDU_PAYLOAD_LEN_1] = {CANTP_N_PCI_TYPE_SF << 4 | 5, 'T', 'E', 'S', 'T', 0,};
    PduInfoType pdu = {.SduDataPtr = pduPayload, .MetaDataPtr = NULL, .SduLength = PDU_PAYLOAD_LEN_1,};
    CanTp_RxNSduType test_nsdu = {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD, .STmin = 100};
    config.channels[0].rxNSdu[1] = test_nsdu;
    
    // TEST 1 - valid connection
    CanTp_State.rxConnections[1].activation = CANTP_RX_PROCESSING;
    CanTp_State.rxConnections[1].state = CANTP_RX_STATE_FC_TX_REQ;

    TEST_CHECK(CanTp_CancelReceive(PDU_ID_1) == E_OK);

    TEST_CHECK(PduR_CanTpRxIndication_fake.call_count == 1);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg0_val == PDU_ID_1);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg1_val == E_NOT_OK);

    TEST_CHECK(CanTp_State.rxConnections[1].activation == CANTP_RX_WAIT);
    TEST_CHECK(CanTp_State.rxConnections[1].state == CANTP_RX_STATE_ABORT);
    
    // TEST 2 - invalid connection state
    CanTp_State.rxConnections[1].activation = CANTP_RX_WAIT;
    CanTp_State.rxConnections[1].state = CANTP_RX_STATE_FC_TX_REQ;

    TEST_CHECK(CanTp_CancelReceive(PDU_ID_1) == E_NOT_OK);

    TEST_CHECK(PduR_CanTpRxIndication_fake.call_count == 1);        // 1 because call_count = 1 after test 1
    TEST_CHECK(CanTp_State.rxConnections[1].activation == CANTP_RX_WAIT);
    TEST_CHECK(CanTp_State.rxConnections[1].state == CANTP_RX_STATE_FC_TX_REQ);
    getRxConnection(300);

    // TEST 3 - invalid connection
    TEST_CHECK(CanTp_CancelReceive(300) == E_NOT_OK); // invalid pduid
    TEST_CHECK(PduR_CanTpRxIndication_fake.call_count == 1);        // 1 because call_count = 1 after test 1
}


void TestOf_CanTp_ChangeParameter(void){
    uint8 pduPayload[PDU_PAYLOAD_LEN_1] = {CANTP_N_PCI_TYPE_SF << 4 | 5, 'T', 'E', 'S', 'T', 0,};
    PduInfoType pdu = {.SduDataPtr = pduPayload, .MetaDataPtr = NULL, .SduLength = PDU_PAYLOAD_LEN_1,};
    CanTp_RxNSduType test_nsdu = {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD, .bs = 100};
    config.channels[0].rxNSdu[1] = test_nsdu;
    uint16 value = 123;
    
     // TEST 1 - invalid state
    CanTp_State.rxConnections[1].activation = CANTP_RX_PROCESSING;

    TEST_CHECK(CanTp_ChangeParameter(PDU_ID_1, TP_BS, value) == E_NOT_OK);
    TEST_CHECK(CanTp_State.rxConnections[1].nsdu->bs == 100);

    // TEST 2 - valid
    CanTp_State.rxConnections[1].activation = CANTP_RX_WAIT;

    TEST_CHECK(CanTp_ChangeParameter(PDU_ID_1, TP_BS, value) == E_OK);
    TEST_CHECK(CanTp_State.rxConnections[1].nsdu->bs == value);
    
    // TEST 3 - invalid value
    test_nsdu = (CanTp_RxNSduType) {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD, .STmin = 100};
    config.channels[0].rxNSdu[1] = test_nsdu;
    CanTp_State.rxConnections[1].activation = CANTP_RX_WAIT;
    value = 567;

    TEST_CHECK(CanTp_ChangeParameter(PDU_ID_1, TP_STMIN, value) == E_NOT_OK);
    TEST_CHECK(CanTp_State.rxConnections[1].nsdu->STmin == 100);
}


void TestOf_CanTp_ReadParameter(void){
    uint8 pduPayload[PDU_PAYLOAD_LEN_1] = {CANTP_N_PCI_TYPE_SF << 4 | 5, 'T', 'E', 'S', 'T', 0,};
    PduInfoType pdu = {.SduDataPtr = pduPayload, .MetaDataPtr = NULL, .SduLength = PDU_PAYLOAD_LEN_1,};
    CanTp_RxNSduType test_nsdu = {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD, .bs = 100};
    config.channels[0].rxNSdu[1] = test_nsdu;
    uint16 readVal = 0;
    
    // TEST 1 - valid
    CanTp_State.rxConnections[1].aquiredBuffSize = 0;

    TEST_CHECK(CanTp_ReadParameter(PDU_ID_1, TP_BS, &readVal) == E_OK);
    TEST_CHECK(readVal == 100);

    // TEST 2 - invalid parameter
    CanTp_State.rxConnections[1].aquiredBuffSize = 0;
    readVal = 0;

    TEST_CHECK(CanTp_ReadParameter(PDU_ID_1, TP_BC, &readVal) == E_NOT_OK);
    TEST_CHECK(readVal == 0);

    // TEST 3 - invalid value
    test_nsdu = (CanTp_RxNSduType) {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD, .STmin = 300};
    config.channels[0].rxNSdu[1] = test_nsdu;
    CanTp_State.rxConnections[1].aquiredBuffSize = 0;
    readVal = 0;

    TEST_CHECK(CanTp_ReadParameter(PDU_ID_1, TP_STMIN, &readVal) == E_NOT_OK);
    TEST_CHECK(readVal == 0);
}


void TestOf_CanTp_RxIndication(void){
    RESET_FAKE(PduR_CanTpStartOfReception);
    RESET_FAKE(PduR_CanTpCopyRxData);
    RESET_FAKE(PduR_CanTpRxIndication);
    FFF_RESET_HISTORY();

    BufReq_ReturnType (*fakeStartRFunc[])(PduIdType, const PduInfoType, PduLengthType, PduLengthType) = {PduR_CanTpStartOfReception_MOCK};
    BufReq_ReturnType (*fakeCopyRData[])(PduIdType, const PduInfoType, PduLengthType) = {PduR_CanTpCopyRxData_MOCK};
    SET_CUSTOM_FAKE_SEQ(PduR_CanTpStartOfReception, fakeStartRFunc, 1);
    SET_CUSTOM_FAKE_SEQ(PduR_CanTpCopyRxData, fakeCopyRData, 1);

    // TEST 1 - normal addressing
    uint8 pduPayload_1[PDU_PAYLOAD_LEN_1] = { CANTP_N_PCI_TYPE_SF << 4 | 5, 'T', 'E', 'S', 'T', 0,};
    PduInfoType pdu = {.SduDataPtr = pduPayload_1, .MetaDataPtr = NULL, .SduLength = PDU_PAYLOAD_LEN_1,};
    CanTp_RxNSduType test_nsdu = {.id = PDU_ID_1, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_STANDARD};
    config.channels[0].rxNSdu[1] = test_nsdu;

    CanTp_RxIndication(PDU_ID_1, &pdu);

    TEST_CHECK(PduR_CanTpStartOfReception_fake.call_count == 1);
    TEST_CHECK(PduR_CanTpStartOfReception_fake.arg0_val == PDU_ID_1);
    TEST_CHECK((PduR_CanTpStartOfReception_fake.arg2_val) == PDU_PAYLOAD_LEN_1 - 1);
    TEST_CHECK(*PduR_CanTpStartOfReception_fake.arg3_val == PDU_PAYLOAD_LEN_1 - 1);

    TEST_CHECK(PduR_CanTpCopyRxData_fake.call_count == 1);
    TEST_CHECK(strcmp(testBuffer, "TEST") == 0);
    TEST_CHECK(PduR_CanTpCopyRxData_fake.arg0_val == PDU_ID_1);

    TEST_CHECK(PduR_CanTpRxIndication_fake.call_count == 1);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg0_val == PDU_ID_1);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg1_val == E_OK);

    // TEST 2 - extended addressing
    uint8 pduPayload_2[PDU_PAYLOAD_LEN_2] = {NULL, CANTP_N_PCI_TYPE_SF << 4 | 5, 'P', 'S', 'E', 'S', 0,};
    pdu = (PduInfoType) {.SduDataPtr = pduPayload_2, .MetaDataPtr = NULL, .SduLength = PDU_PAYLOAD_LEN_2,};
    test_nsdu = (CanTp_RxNSduType) {.id = PDU_ID_2, .paddingActivation = CANTP_OFF, .addressingFormat = CANTP_EXTENDED};
    config.channels[0].rxNSdu[1] = test_nsdu;

    CanTp_RxIndication(PDU_ID_2, &pdu);

    TEST_CHECK(PduR_CanTpStartOfReception_fake.call_count == 2);
    TEST_CHECK(PduR_CanTpStartOfReception_fake.arg0_val == PDU_ID_2);
    TEST_CHECK((PduR_CanTpStartOfReception_fake.arg2_val) == PDU_PAYLOAD_LEN_2 - 2);
    TEST_CHECK(*(PduLengthType *)(PduR_CanTpStartOfReception_fake.arg3_val) == PDU_PAYLOAD_LEN_2 - 2);

    TEST_CHECK(PduR_CanTpCopyRxData_fake.call_count == 2);
    TEST_CHECK(strcmp(testBuffer, "PSES") == 0);
    TEST_CHECK(PduR_CanTpCopyRxData_fake.arg0_val == PDU_ID_2);

    TEST_CHECK(PduR_CanTpRxIndication_fake.call_count == 3);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg0_val == PDU_ID_2);
    TEST_CHECK(PduR_CanTpRxIndication_fake.arg1_val == E_OK);
}


/*
  Lista testów
*/
TEST_LIST = {
    {"Test_Of_CanTp_Init",Test_Of_CanTp_Init},    // Format to {"nazwa testu", nazwa_funkcji}
    {"Test_Of_CanTp_Shutdown", Test_Of_CanTp_Shutdown},
    {"TestOf_CanTp_ChangeParameter", TestOf_CanTp_ChangeParameter},
    {"TestOf_CanTp_ReadParameter", TestOf_CanTp_ReadParameter},
    {"TestOf_CanTp_Transmit", TestOf_CanTp_Transmit},
    {"TestOf_CanTp_CancelTransmit", TestOf_CanTp_CancelTransmit},
    {"TestOf_CanTp_CancelReceive", TestOf_CanTp_CancelReceive},
    {"TestOf_CanTp_RxIndication", TestOf_CanTp_RxIndication},
    {NULL, NULL}  // To musi być na końcu
};
