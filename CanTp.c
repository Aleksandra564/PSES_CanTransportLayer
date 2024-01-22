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
#include "CanTp_Types.h"
#include "Det.h"
#include "PduR_CanTp.h"

/*====================================================================================================================*\
    Makra lokalne
\*====================================================================================================================*/
#define CANTP_IS_ON() (CanTp_State.activation == CANTP_ON)
#define PARAM_UNUSED(param) (void)param
#define ARR_SIZE(arr) (sizeof(arr) / (sizeof(*arr)))
#define CANTP_SEQUENCE_NUMBER_START_VALUE 1U
#define CANTP_SEQUENCE_NUMBER_INCREMENT_VALUE 1U

#define CAN_2_0_MAX_LEN (uint8)8U
#define CAN_FD_MAX_LEN (uint8)64U

#define CANTP_SF_PCI_SIZE 0x01
#define CANTP_FF_PCI_SIZE 0x02
#define CANTP_CF_PCI_SIZE 0x01

/*====================================================================================================================*\
    Typy lokalne
\*====================================================================================================================*/
// ENUM:
typedef enum{
    CANTP_N_PCI_TYPE_SF = 0x00,
    CANTP_N_PCI_TYPE_FF = 0x01,
    CANTP_N_PCI_TYPE_CF = 0x02,
    CANTP_N_PCI_TYPE_FC = 0x03,
} CanTp_PciType;

typedef enum{
    CANTP_FS_TYPE_CTS = 0x00,
    CANTP_FS_TYPE_WT = 0x01,
    CANTP_FS_TYPE_OVF = 0x02
} CanTp_FsType;

typedef enum{
    /**
     * No pending operations on a connection.
     */
    CANTP_TX_STATE_FREE,
    /**
     * Getting the data from PduR and composing the frame for lower layer.
     */
    CANTP_TX_STATE_SF_SEND_REQ,
    /**
     * Sending the data to CanIf.
     */
    CANTP_TX_STATE_SF_SEND_PROCESS,
    CANTP_TX_STATE_FF_SEND_REQ,
    CANTP_TX_STATE_FF_SEND_PROCESS,
    CANTP_TX_STATE_WAIT_FC,
    CANTP_TX_STATE_CF_SEND_REQ,
    CANTP_TX_STATE_CF_SEND_PROCESS,
    CANTP_TX_STATE_WAIT_CANIF_CONFIRM,
    CANTP_TX_STATE_CANCEL
} CanTp_TxConnectionState;

typedef enum{
    CANTP_RX_STATE_FREE,
    CANTP_RX_STATE_WAIT_CF,
    CANTP_RX_STATE_FC_TX_REQ,
    CANTP_RX_STATE_PROCESSED,
    CANTP_RX_STATE_ABORT,
    CANTP_RX_STATE_INVALID
} CanTp_RxConnectionState;

// STRUCT
typedef struct{
    uint8 payloadOffset;
    uint8 payloadLength;
#if defined(CONFIG_CAN_2_0_OR_CAN_FD)
    uint8 data[CAN_2_0_MAX_LEN];
#elif defined(CONFIG_CAN_FD_ONLY)
    uint8 data[CAN_FD_MAX_LEN];
#endif
} CanTp_ConnectionBuffer;

typedef struct{
    CanTp_RxNSduState activation;
    // Points to nsdu in CanTp_Config.channels
    CanTp_RxNSduType *nsdu;
    CanTp_RxConnectionState state;
    struct{
        uint32 ar;
        uint32 br;
        uint32 cr;
    } timer;
    PduInfoType pduInfo;
    PduLengthType buffSize;
    PduLengthType aquiredBuffSize;
    uint8 sn;
    uint8 bs;
    CanTp_FsType fs;
    CanTp_ConnectionBuffer fcBuf;
} CanTp_RxConnection;

typedef struct{
    CanTp_TxNSduState activation;
    // Points to nsdu in CanTp_Config.channels
    CanTp_TxNSduType *nsdu;
    CanTp_TxConnectionState state;
    PduInfoType pduInfo;
    CanTp_ConnectionBuffer buf;
    struct{
        uint32 as;
        uint32 bs;
        uint32 cs;
    } timer;

    uint8 sequenceNumber;
} CanTp_TxConnection;

typedef struct{
    CanTp_PaddingActivationType activation;
    uint32 currentTime;
    CanTp_RxConnection
        rxConnections[CONFIG_CAN_TP_MAX_CHANNELS_COUNT * CONFIG_CANTP_MAX_RX_NSDU_PER_CHANNEL];
    CanTp_TxConnection
        txConnections[CONFIG_CAN_TP_MAX_CHANNELS_COUNT * CONFIG_CANTP_MAX_TX_NSDU_PER_CHANNEL];
} CanTp_State_t;

typedef enum{
    CANTP_NSDU_DIRECTION_TX,
    CANTP_NSDU_DIRECTION_RX
} CanTp_NSduDirection_t;

/*====================================================================================================================*\
    Zmienne globalne
\*====================================================================================================================*/
static CanTp_State_t CanTp_State;

static CanTp_ConfigType config = {
    .channels = {
        {// Channel 0
         .rxNSdu = {{.id = 101}, {.id = 102}, {.id = 103}, {.id = 104}, {.id = 105}},
         .rxNSduCount = 5,
         .txNSdu = {{.id = 201}},
         .txNSduCount = 1},
        {// Channel 1
         .rxNSdu = {{.id = 106}, {.id = 107}, {.id = 108}},
         .rxNSduCount = 3,
         .txNSdu = {{.id = 206}, {.id = 207}, {.id = 208}, {.id = 209}, {.id = 210}},
         .txNSduCount = 5},
        {// Channel 2
         .rxNSduCount = 0,
         .txNSdu = {{.id = 211}, {.id = 212}},
         .txNSduCount = 2},
        {// Channel 3
         .rxNSdu = {{.id = 113}, {.id = 114}, {.id = 115}},
         .rxNSduCount = 3,
         .txNSduCount = 0},
    }
};

static CanTp_State_t CanTp_State = {
    .activation = CANTP_OFF,
    .currentTime = 0,
    .rxConnections = {
        {.nsdu = &config.channels[0].rxNSdu[0], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[0].rxNSdu[1], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[0].rxNSdu[2], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[0].rxNSdu[3], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[0].rxNSdu[4], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[1].rxNSdu[0], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[1].rxNSdu[1], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[1].rxNSdu[2], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[3].rxNSdu[0], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[3].rxNSdu[1], .activation = CANTP_RX_WAIT},
        {.nsdu = &config.channels[3].rxNSdu[2], .activation = CANTP_RX_WAIT},
    },
    .txConnections = {
        {.nsdu = &config.channels[0].txNSdu[0], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[1].txNSdu[0], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[1].txNSdu[1], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[1].txNSdu[2], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[1].txNSdu[3], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[1].txNSdu[4], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[2].txNSdu[0], .activation = CANTP_TX_WAIT},
        {.nsdu = &config.channels[2].txNSdu[1], .activation = CANTP_TX_WAIT},
    },
};

/*====================================================================================================================*\
    Kod globalnych funkcji inline i makr funkcyjnych
\*====================================================================================================================*/
static inline void memzero(uint8_t *ptr, uint32_t size){
    for (uint32_t i = 0; i < size; i++) {
        *(ptr + i) = 0;
    }
}

static CanTp_TxConnection *getTxConnection(PduIdType PduId){
    CanTp_TxConnection *txConnection = NULL;
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.txConnections) && !txConnection;
         connItr++) {
        if (CanTp_State.txConnections[connItr].nsdu != NULL) {
            if (CanTp_State.txConnections[connItr].nsdu->id == PduId) {
                txConnection = &CanTp_State.txConnections[connItr];
            }
        }
    }
    return txConnection;
}

static CanTp_RxConnection *getRxConnection(PduIdType PduId){
    CanTp_RxConnection *rxConnection = NULL;
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections) && !rxConnection;
         connItr++){
        if (CanTp_State.rxConnections[connItr].nsdu != NULL){
            if (CanTp_State.rxConnections[connItr].nsdu->id == PduId){
                rxConnection = &CanTp_State.rxConnections[connItr];
            }
        }
    }
    return rxConnection;
}

static inline CanTp_PciType CanTp_DecodeFrameType(const uint8 *sdu){
    return (((sdu[0]) >> 4) & 0xF);
}

static inline uint8 CanTp_GetAddrFieldLen(const CanTp_AddressingFormatType af){
    uint8 extAddrFieldLen = 0;
    switch (af){
        case CANTP_EXTENDED:
        case CANTP_MIXED:
        case CANTP_MIXED29BIT:
            extAddrFieldLen = 1;
            break;
        case CANTP_STANDARD:
        case CANTP_NORMALFIXED:
            extAddrFieldLen = 0;
            break;
        default:
            break;
    }
    return extAddrFieldLen;
}

static inline PduLengthType CanTp_DecodeFrameDL(const CanTp_PciType frameType, const CanTp_PaddingActivationType padding, const uint8 *sdu){
    PduLengthType dl = 0;
    if (frameType == CANTP_N_PCI_TYPE_SF) {
        dl = sdu[0] & 0x0F;
#if defined(CONFIG_CAN_FD_ONLY)
        if (dl == 0) {
            dl = sdu[1];
        }
#endif
    } 
    else if (frameType == CANTP_N_PCI_TYPE_FF) {
        dl = ((PduLengthType)(sdu[0] & 0x0f) << 8) || (PduLengthType)(sdu[1]);

        if (dl == 0){
            dl = ((PduLengthType)(sdu[2]) << 24) || ((PduLengthType)(sdu[3]) << 16) || ((PduLengthType)(sdu[4]) << 8) || (PduLengthType)(sdu[5]);
        }
    } 
    else{
        // CF & FC doesn't have DL
    }
    return dl;
}

static uint32 determineMaxTxNsduLength(CanTp_TxNSduType *nsdu){
    uint32 maxLen = 0;
#if defined(CONFIG_CAN_2_0_OR_CAN_FD)
    maxLen = CAN_2_0_MAX_LEN - CANTP_SF_PCI_SIZE - CanTp_GetAddrFieldLen(nsdu->addressingFormat);
#elif defined(CONFIG_CAN_FD_ONLY)
    maxLen = CAN_FD_MAX_LEN - 2 - CanTp_GetAddrFieldLen(nsdu->addressingFormat);
#endif
    return maxLen;
}

static PduLengthType CanTp_GetRxBS(const CanTp_RxConnection *conn){
    PduLengthType result;
    const PduLengthType payloadSize =
        CANTP_CAN_FRAME_SIZE -
        (CANTP_N_PCI_TYPE_CF + CanTp_GetAddrFieldLen(conn->nsdu->addressingFormat));
    const PduLengthType fullBs = conn->nsdu->bs * payloadSize;
    const PduLengthType lastBs = conn->buffSize;

    if ((lastBs < fullBs) || (fullBs == 0x00u)){
        result = lastBs;
    } 
    else{
        result = fullBs;
    }
    return result;
}

static BufReq_ReturnType CanTp_CopyRxData(CanTp_RxConnection *rxConn){
    BufReq_ReturnType result;

    result = PduR_CanTpCopyRxData(rxConn->nsdu->id, &rxConn->pduInfo, &rxConn->aquiredBuffSize);

    if (result == BUFREQ_OK) {
        rxConn->buffSize -= rxConn->pduInfo.SduLength;
    }

    return result;
}

static CanTp_RxConnectionState CanTp_RxIndSF(CanTp_RxConnection *conn, const PduInfoType *PduInfoPtr, uint8 nAeSize){
    uint8 headerSize;
    BufReq_ReturnType status;
    CanTp_RxConnectionState result = CANTP_RX_STATE_INVALID;

    // if reception is in progres report it and start new reception
    if (conn->activation == CANTP_RX_PROCESSING){
        PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
    } 
    else{
        conn->activation = CANTP_RX_PROCESSING;
    }

    headerSize = CANTP_SF_PCI_SIZE + nAeSize;
    conn->buffSize = CanTp_DecodeFrameDL(CANTP_N_PCI_TYPE_SF, conn->nsdu->paddingActivation, &(PduInfoPtr->SduDataPtr[nAeSize]));

    conn->pduInfo.SduDataPtr = &(PduInfoPtr->SduDataPtr[headerSize]);
    conn->pduInfo.MetaDataPtr = NULL;
    conn->pduInfo.SduLength = conn->buffSize;

    status = PduR_CanTpStartOfReception(conn->nsdu->id, &conn->pduInfo, conn->buffSize, &conn->aquiredBuffSize);
    switch (status){
        case BUFREQ_OK:
            if ((conn->aquiredBuffSize >= conn->buffSize) &&
                (CanTp_CopyRxData(conn) == BUFREQ_OK)) {
                PduR_CanTpRxIndication(conn->nsdu->id, E_OK);
                result = CANTP_RX_STATE_PROCESSED;
            } else {
                PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
                result = CANTP_RX_STATE_ABORT;
            }
            break;

        case BUFREQ_OVFL:
        case BUFREQ_E_NOT_OK:
            PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
            result = CANTP_RX_STATE_ABORT;
            break;

        case BUFREQ_BUSY:
            PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
            result = CANTP_RX_STATE_ABORT;
            break;
        default:
            break;
    }
    return result;
}

static CanTp_RxConnectionState CanTp_RxIndFF(CanTp_RxConnection *conn, const PduInfoType *PduInfoPtr, uint8 nAeSize){
    uint8 headerSize;
    uint8 payloadSize;
    BufReq_ReturnType status;
    CanTp_RxConnectionState result = CANTP_RX_STATE_INVALID;

    // if reception is in progres report it and start new reception
    if (conn->activation == CANTP_RX_PROCESSING){
        PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
    } 
    else{
        conn->activation = CANTP_RX_PROCESSING;
    }

    headerSize = CANTP_FF_PCI_SIZE + nAeSize;
    payloadSize = CANTP_CAN_FRAME_SIZE - headerSize;

    conn->buffSize = CanTp_DecodeFrameDL(CANTP_N_PCI_TYPE_FF, conn->nsdu->paddingActivation, &(PduInfoPtr->SduDataPtr[nAeSize]));
    conn->sn = 0;
    conn->bs = conn->nsdu->bs;
    conn->pduInfo.SduDataPtr = &(PduInfoPtr->SduDataPtr[headerSize]);
    conn->pduInfo.MetaDataPtr = NULL;
    conn->pduInfo.SduLength = PduInfoPtr->SduLength - headerSize;

    status = PduR_CanTpStartOfReception(conn->nsdu->id, &conn->pduInfo, conn->buffSize, &conn->aquiredBuffSize);
    switch (status) {
        case BUFREQ_OK:
            if (conn->aquiredBuffSize < conn->buffSize) {
                PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
                result = CANTP_RX_STATE_ABORT;
            } 
            else{
                result = CANTP_RX_STATE_FC_TX_REQ;

                if (conn->aquiredBuffSize < CanTp_GetRxBS(conn)) {
                    conn->timer.br = 0;
                    conn->fs = CANTP_FS_TYPE_WT;
                } 
                else{
                    conn->fs = CANTP_FS_TYPE_CTS;
                }
            }

            if ((CanTp_CopyRxData(conn) != BUFREQ_OK)){
                PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
                result = CANTP_RX_STATE_ABORT;
            } 
            else{
            }
            break;
        case BUFREQ_OVFL:
            result = CANTP_RX_STATE_FC_TX_REQ;
            conn->fs = CANTP_FS_TYPE_OVF;
        case BUFREQ_E_NOT_OK:
        case BUFREQ_BUSY:
            result = CANTP_RX_STATE_ABORT;
            break;
        default:
            break;
    }
    return result;
}

static CanTp_RxConnectionState CanTp_RxIndCF(CanTp_RxConnection *conn, const PduInfoType *PduInfoPtr, uint8 nAeSize){
    uint8 headerSize;
    uint8 payloadSize;
    BufReq_ReturnType status;
    CanTp_RxConnectionState result = CANTP_RX_STATE_INVALID;

    // if reception is in progres report it and start new reception
    if (conn->activation != CANTP_RX_PROCESSING){
        result = CANTP_RX_STATE_ABORT;
        return result;
    }

    if (conn->state != CANTP_RX_STATE_WAIT_CF){
        result = CANTP_RX_STATE_ABORT;
        return result;
    }

    if ((PduInfoPtr->SduDataPtr[nAeSize] & 0x0Fu) != ((conn->sn + 0x01u) & 0x0Fu)){
        PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
        result = CANTP_RX_STATE_ABORT;
        return result;
    }

    headerSize = CANTP_CF_PCI_SIZE + nAeSize;

    conn->sn++;
    conn->bs--;

    conn->pduInfo.SduDataPtr = &(PduInfoPtr->SduDataPtr[headerSize]);
    conn->pduInfo.MetaDataPtr = NULL;
    conn->pduInfo.SduLength = PduInfoPtr->SduLength - headerSize;

    if (CanTp_CopyRxData(conn) == BUFREQ_OK){
        if (conn->buffSize != 0){
            if (conn->bs == 0){
                conn->bs = conn->nsdu->bs;
                conn->timer.br = 0;
                result = CANTP_RX_STATE_WAIT_CF;
            } 
            else{
                conn->timer.cr = 0;
                result = CANTP_RX_STATE_FC_TX_REQ;
            }
        } 
        else{
            PduR_CanTpRxIndication(conn->nsdu->id, E_OK);
            result = CANTP_RX_STATE_PROCESSED;
        }
    } 
    else{
        PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
        result = CANTP_RX_STATE_ABORT;
    }
    return result;
}

static CanTp_TxConnectionState CanTp_RxStateTXFC(CanTp_RxConnection *conn){
    PduInfoType pduInfo;
    PduLengthType remainingLength;
    uint8 payloadOffset;
    BufReq_ReturnType copyTxRet;
    CanTp_RxConnectionState nextState;

    conn->fcBuf.data[0] = (((uint8)CANTP_N_PCI_TYPE_FC) << 4) || conn->fs;
    conn->fcBuf.data[0] = conn->bs;
    conn->fcBuf.data[0] = conn->nsdu->STmin;

    pduInfo.MetaDataPtr = NULL;
    pduInfo.SduDataPtr = &conn->fcBuf.data[CanTp_GetAddrFieldLen(conn->nsdu->addressingFormat)];
    pduInfo.SduLength = 3;

    if (CanIf_Transmit(conn->nsdu->id, &pduInfo) == E_OK){
        nextState = CANTP_RX_STATE_WAIT_CF;
    } 
    else{
        nextState = CANTP_RX_STATE_ABORT;
    }
    return nextState;
}

static CanTp_RxConnectionState CanTp_RxIndFC(CanTp_TxConnection *conn, const PduInfoType *PduInfoPtr, uint8 nAeSize){
    conn->state = CANTP_TX_STATE_CF_SEND_REQ;
}

static inline void CanTp_FillTpHeader(CanTp_TxConnection *conn, CanTp_PciType pciType){
    uint8 addressingInfoOffset = CanTp_GetAddrFieldLen(conn->nsdu->addressingFormat);
    uint8 *buf = &conn->buf.data[addressingInfoOffset];

    buf[0] = ((uint8)pciType << 4);

    switch (pciType){
        case CANTP_N_PCI_TYPE_SF:
#if defined(CONFIG_CAN_2_0_OR_CAN_FD)
            buf[0] &= (uint8)0xF0;
            buf[0] |= (uint8)(conn->pduInfo.SduLength & 0x0F);
            conn->buf.payloadOffset = CANTP_SF_PCI_SIZE + addressingInfoOffset;
#elif defined(CONFIG_CAN_FD_ONLY)
#error "Implement CanTp_FillTpHeader for CAN_FD"
#endif
            break;

        case CANTP_N_PCI_TYPE_FF:
#if defined(CONFIG_CAN_2_0_OR_CAN_FD)
            *((uint16 *)&buf[0]) = (CANTP_N_PCI_TYPE_FF << 12) | (conn->pduInfo.SduLength & 0x0FFF);
            conn->buf.payloadOffset = CANTP_FF_PCI_SIZE + addressingInfoOffset;
#elif defined(CONFIG_CAN_FD_ONLY)
#error "Implement CanTp_FillTpHeader for CAN_FD"
#endif
            break;

        case CANTP_N_PCI_TYPE_CF:
#if defined(CONFIG_CAN_2_0_OR_CAN_FD)
            buf[0] &= (uint8)0xF0;
            buf[0] |= (uint8)(conn->sequenceNumber & 0x0F);
            conn->buf.payloadOffset = CANTP_CF_PCI_SIZE + addressingInfoOffset;
#elif defined(CONFIG_CAN_FD_ONLY)
#error "Implement CanTp_FillTpHeader for CAN_FD"
#endif
        default:
            break;
    }
}

static CanTp_TxConnectionState CanTp_TxStateSFSendReq(CanTp_TxConnection *conn){
    PduInfoType pduInfo;
    PduLengthType remainingLength;
    BufReq_ReturnType copyTxRet;
    CanTp_TxConnectionState nextState;

    CanTp_FillTpHeader(conn, CANTP_N_PCI_TYPE_SF);

    pduInfo.MetaDataPtr = NULL;
    pduInfo.SduDataPtr = &conn->buf.data[conn->buf.payloadOffset];
    pduInfo.SduLength = conn->pduInfo.SduLength;

    // Lock the data within PduR
    copyTxRet = PduR_CanTpCopyTxData(conn->nsdu->id, &pduInfo, NULL, &remainingLength);

    if (copyTxRet == BUFREQ_OK){
        conn->buf.payloadLength = conn->pduInfo.SduLength + conn->buf.payloadOffset;
        nextState = CANTP_TX_STATE_SF_SEND_PROCESS;
    } 
    else{
        nextState = CANTP_TX_STATE_SF_SEND_REQ;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateSFProcess(CanTp_TxConnection *conn){
    CanTp_TxConnectionState nextState;
    Std_ReturnType transmitResult;
    const PduInfoType pduInfo = {.MetaDataPtr = NULL, .SduDataPtr = conn->buf.data, .SduLength = conn->buf.payloadLength};

    transmitResult = CanIf_Transmit(conn->nsdu->id, &pduInfo);
    if (transmitResult == E_OK){
        // Inform higher layer about successful transmission
        PduR_CanTpTxConfirmation(conn->nsdu->id, E_OK);
        nextState = CANTP_TX_STATE_FREE;
        conn->activation = CANTP_TX_WAIT;
    } 
    else{
        nextState = CANTP_TX_STATE_SF_SEND_PROCESS;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateFFSendReq(CanTp_TxConnection *conn){
    PduInfoType pduInfo;
    PduLengthType remainingLength;
    BufReq_ReturnType copyTxRet;
    CanTp_TxConnectionState nextState;

    CanTp_FillTpHeader(conn, CANTP_N_PCI_TYPE_FF);

    pduInfo.MetaDataPtr = NULL;
    pduInfo.SduDataPtr = &conn->buf.data[conn->buf.payloadOffset];
    pduInfo.SduLength = CAN_2_0_MAX_LEN - conn->buf.payloadOffset;

    copyTxRet = PduR_CanTpCopyTxData(conn->nsdu->id, &pduInfo, NULL, &remainingLength);

    if (copyTxRet == BUFREQ_OK){
        conn->buf.payloadLength = pduInfo.SduLength + conn->buf.payloadOffset;
        conn->pduInfo.SduLength = remainingLength;
        nextState = CANTP_TX_STATE_FF_SEND_PROCESS;
    } 
    else{
        nextState = CANTP_TX_STATE_FF_SEND_REQ;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateFFSendProcess(CanTp_TxConnection *conn){
    CanTp_TxConnectionState nextState;
    Std_ReturnType transmitResult;
    const PduInfoType pduInfo = {.MetaDataPtr = NULL, .SduDataPtr = conn->buf.data, .SduLength = conn->buf.payloadLength};

    transmitResult = CanIf_Transmit(conn->nsdu->id, &pduInfo);
    if (transmitResult == E_OK){
        // Start waiting for FC
        nextState = CANTP_TX_STATE_WAIT_FC;
        conn->sequenceNumber = CANTP_SEQUENCE_NUMBER_START_VALUE;
    } 
    else{
        nextState = CANTP_TX_STATE_FF_SEND_PROCESS;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateFFWaitFC(CanTp_TxConnection *conn){
    // Do nothing
    return conn->state;
}

static CanTp_TxConnectionState CanTp_TxStateCFSendReq(CanTp_TxConnection *conn){
    PduInfoType pduInfo;
    PduLengthType remainingLength;
    BufReq_ReturnType copyTxRet;
    CanTp_TxConnectionState nextState;
    uint8 maxCFSize;

    CanTp_FillTpHeader(conn, CANTP_N_PCI_TYPE_CF);
    maxCFSize = CAN_2_0_MAX_LEN - conn->buf.payloadOffset;

    pduInfo.MetaDataPtr = NULL;
    pduInfo.SduDataPtr = &conn->buf.data[conn->buf.payloadOffset];

    if (conn->pduInfo.SduLength <= maxCFSize){
        pduInfo.SduLength = conn->pduInfo.SduLength;
    } 
    else{
        pduInfo.SduLength = maxCFSize;
    }

    // Lock the data within PduR
    copyTxRet = PduR_CanTpCopyTxData(conn->nsdu->id, &pduInfo, NULL, &remainingLength);

    if (copyTxRet == BUFREQ_OK){
        conn->buf.payloadLength = pduInfo.SduLength + conn->buf.payloadOffset;
        conn->pduInfo.SduLength = remainingLength;
        conn->sequenceNumber += CANTP_SEQUENCE_NUMBER_INCREMENT_VALUE;
        nextState = CANTP_TX_STATE_CF_SEND_PROCESS;
    } 
    else{
        nextState = CANTP_TX_STATE_CF_SEND_REQ;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateCFSendProcess(CanTp_TxConnection *conn){
    CanTp_TxConnectionState nextState;
    Std_ReturnType transmitResult;
    const PduInfoType pduInfo = {.MetaDataPtr = NULL, .SduDataPtr = conn->buf.data, .SduLength = conn->buf.payloadLength};

    transmitResult = CanIf_Transmit(conn->nsdu->id, &pduInfo);
    if (transmitResult == E_OK){
        // Determine if further fragmentation is needed
        if (conn->pduInfo.SduLength > 0){
            nextState = CANTP_TX_STATE_CF_SEND_REQ;
        } 
        else{
            // No fragmentation needed, inform higher layer and free nsdu
            PduR_CanTpTxConfirmation(conn->nsdu->id, E_OK);
            nextState = CANTP_TX_STATE_FREE;
            conn->activation = CANTP_TX_WAIT;
        }
    } else{
        nextState = CANTP_TX_STATE_CF_SEND_PROCESS;
    }
    return nextState;
}

static CanTp_TxConnectionState CanTp_TxStateCancel(CanTp_TxConnection *conn){
    CanTp_TxConnectionState nextState = CANTP_TX_STATE_FREE;
    PduR_CanTpTxConfirmation(conn->nsdu->id, E_NOT_OK);
    conn->activation = CANTP_TX_WAIT;
    return nextState;
}

static void CanTp_TxIteration(void){
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.txConnections); connItr++){
        CanTp_TxConnection *conn = &CanTp_State.txConnections[connItr];
        CanTp_TxConnectionState nextState = conn->state;

        // TX state machine
        switch (conn->state) {
            case CANTP_TX_STATE_SF_SEND_REQ:
                nextState = CanTp_TxStateSFSendReq(conn);
                break;
            case CANTP_TX_STATE_SF_SEND_PROCESS:
                nextState = CanTp_TxStateSFProcess(conn);
                break;
            case CANTP_TX_STATE_FF_SEND_REQ:
                nextState = CanTp_TxStateFFSendReq(conn);
                break;
            case CANTP_TX_STATE_FF_SEND_PROCESS:
                nextState = CanTp_TxStateFFSendProcess(conn);
                break;
            case CANTP_TX_STATE_WAIT_FC:
                nextState = CanTp_TxStateFFWaitFC(conn);
                break;
            case CANTP_TX_STATE_CF_SEND_REQ:
                nextState = CanTp_TxStateCFSendReq(conn);
                break;
            case CANTP_TX_STATE_CF_SEND_PROCESS:
                nextState = CanTp_TxStateCFSendProcess(conn);
                break;
            case CANTP_TX_STATE_FREE:
                break;
            case CANTP_TX_STATE_CANCEL:
                nextState = CanTp_TxStateCancel(conn);
                break;
            default:
                break;
        }
        conn->state = nextState;
    }
}

static void CanTp_RxIteration(void){
    CanTp_RxConnectionState nextState;
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections); connItr++){
        CanTp_RxConnection *conn = &CanTp_State.rxConnections[connItr];
        nextState = conn->state;
        // RX state machine
        switch (conn->state) {
            case CANTP_RX_STATE_FREE:
                break;
            case CANTP_RX_STATE_WAIT_CF:
                break;
            case CANTP_RX_STATE_FC_TX_REQ:
                CanTp_RxStateTXFC(conn);
                break;
            case CANTP_RX_STATE_PROCESSED:
            case CANTP_RX_STATE_ABORT:
                nextState = CANTP_RX_STATE_FREE;
                break;
        }
        conn->state = nextState;
    }
}

static void CanTp_ConnectionsTimersInc(void){
    // Increments timers for each tx and rx connection
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.txConnections); connItr++){
        CanTp_TxConnection *conn = &CanTp_State.txConnections[connItr];
        conn->timer.as += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
        conn->timer.bs += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
        conn->timer.cs += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
    }

    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections); connItr++){
        CanTp_RxConnection *conn = &CanTp_State.rxConnections[connItr];
        conn->timer.ar += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
        conn->timer.br += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
        conn->timer.cr += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
    }
}

/*====================================================================================================================*\
    Kod funkcji
\*====================================================================================================================*/

/**
  @brief CanTp_Init

  This function initializes the CanTp module.

*/
void CanTp_Init(const CanTp_ConfigType *CfgPtr){
    PARAM_UNUSED(CfgPtr);
    // rxConnections and txConnections have the same size
    for(uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections); connItr++){
        CanTp_State.rxConnections[connItr].activation = CANTP_RX_WAIT;
        CanTp_State.txConnections[connItr].activation = CANTP_TX_WAIT;
    }
    CanTp_State.activation = CANTP_ON;
    CanTp_State.currentTime = 0;
}


/**
  @brief CanTp_Shutdown

  This function is called to shutdown the CanTp module.

*/
void CanTp_Shutdown(void){
    CanTp_State.activation = CANTP_OFF;
    // rxConnections and txConnections have the same size
    for (uint32 connItr = 0; connItr < ARR_SIZE(CanTp_State.rxConnections); connItr++){
        CanTp_State.rxConnections[connItr].activation = CANTP_RX_WAIT;
        CanTp_State.txConnections[connItr].activation = CANTP_TX_WAIT;
    }
}


/**
  @brief CanTp_Transmit

  Requests transmission of a PDU.

*/
Std_ReturnType CanTp_Transmit(PduIdType TxPduId, const PduInfoType *PduInfoPtr){
    Std_ReturnType result = E_NOT_OK;
    CanTp_TxNSduType *nsdu = NULL;
    uint32 maxNsduLength = 0;
    CanTp_TxConnection *connection = getTxConnection(TxPduId);

    if (!CANTP_IS_ON()){
        return result;
    }
    if (connection == NULL){
        return result;
    }
    if (connection->activation == CANTP_TX_PROCESSING){
        return result;
    }
    if (PduInfoPtr == NULL){
        return result;
    }
    if ((PduInfoPtr->SduLength > 0) && (PduInfoPtr->SduDataPtr == NULL)){
        return result;
    }

    connection->activation = CANTP_TX_PROCESSING;
    nsdu = connection->nsdu;
    maxNsduLength = determineMaxTxNsduLength(nsdu);
    result = E_OK;

    // Only SF and FF transmission is triggered here. CF frames and FC are triggered from RX/TX state machines.
    if (PduInfoPtr->SduLength <= maxNsduLength){
        connection->state = CANTP_TX_STATE_SF_SEND_REQ;
    } 
    else{
        connection->state = CANTP_TX_STATE_FF_SEND_REQ;
    }
    connection->pduInfo.SduLength = PduInfoPtr->SduLength;
    return result;
}


/**
  @brief CanTp_CancelTransmit

  Requests cancellation of an ongoing transmission of a PDU in a lower layer communication module.

*/
Std_ReturnType CanTp_CancelTransmit(PduIdType TxPduId){
    Std_ReturnType status = E_NOT_OK;

    CanTp_TxConnection *conn = getTxConnection(TxPduId);

    if (!conn){
        return status;
    }
    if (conn->activation == CANTP_TX_PROCESSING){
        conn->state = CANTP_TX_STATE_CANCEL;
        status = E_OK;
    }
    return status;
}


/**
  @brief CanTp_CancelReceive

  Requests cancellation of an ongoing reception of a PDU in a lower layer transport protocol module.

*/
Std_ReturnType CanTp_CancelReceive(PduIdType RxPduId){
    CanTp_RxConnection *conn = getRxConnection(RxPduId);
    if (conn != NULL){
        if ((conn->activation == CANTP_RX_PROCESSING)){
            conn->activation = CANTP_RX_WAIT;
            conn->state = CANTP_RX_STATE_ABORT;
            PduR_CanTpRxIndication(conn->nsdu->id, E_NOT_OK);
            return E_OK;
        } 
        else{
            return E_NOT_OK;
        }
    } 
    else{
        return E_NOT_OK;
    }
}


/**
  @brief CanTp_ChangeParameter

  Request to change a specific transport protocol parameter (e.g. block size).

*/
Std_ReturnType CanTp_ChangeParameter(PduIdType id, TPParameterType parameter, uint16 value){
    Std_ReturnType result = E_NOT_OK;
    CanTp_RxConnection *conn = getRxConnection(id);
    if ((conn != NULL) && (conn->activation == CANTP_RX_WAIT) && (conn->state == CANTP_RX_STATE_FREE) && (value <= 0xFF)){
        switch (parameter){
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
void CanTp_MainFunction(void){
    if (CANTP_IS_ON()){
        CanTp_TxIteration();
        CanTp_RxIteration();
        CanTp_ConnectionsTimersInc();
        CanTp_State.currentTime += CONFIG_CANTP_MAIN_FUNCTION_PERIOD;
    }
}



// Call-back notifications  -  8.4. in documentation
/**
  @brief CanTp_RxIndication

  Indication of a received PDU from a lower layer communication interface module.

*/
void CanTp_RxIndication(PduIdType RxPduId, const PduInfoType *PduInfoPtr){
    CanTp_TxConnection *txConn = NULL;
    CanTp_RxConnection *rxConn = getRxConnection(RxPduId);
    uint8 nAeSize = 0;
    CanTp_NSduDirection_t nsduDir = CANTP_NSDU_DIRECTION_RX;
    CanTp_TxConnectionState nextState = CANTP_RX_STATE_FREE;

    if (rxConn == NULL){
        txConn = getTxConnection(RxPduId);
        if (txConn == NULL){
            return;
        }
        nsduDir = CANTP_NSDU_DIRECTION_TX;
    }

    if (nsduDir == CANTP_NSDU_DIRECTION_RX) {
        if ((rxConn->nsdu->paddingActivation == CANTP_ON) && (PduInfoPtr->SduLength < 8)){
            PduR_CanTpRxIndication(rxConn->nsdu->id, E_NOT_OK);
            return;
        }
        nAeSize = CanTp_GetAddrFieldLen(rxConn->nsdu->addressingFormat);

        switch (CanTp_DecodeFrameType(&(PduInfoPtr->SduDataPtr[nAeSize]))){
            case CANTP_N_PCI_TYPE_SF:
                nextState = CanTp_RxIndSF(rxConn, PduInfoPtr, nAeSize);
                break;
            case CANTP_N_PCI_TYPE_FF:
                nextState = CanTp_RxIndFF(rxConn, PduInfoPtr, nAeSize);
                break;
            case CANTP_N_PCI_TYPE_CF:
                nextState = CanTp_RxIndCF(rxConn, PduInfoPtr, nAeSize);
                break;
            case CANTP_N_PCI_TYPE_FC:
            default:
                return;
                break;
        }
    } 
    else if (nsduDir == CANTP_NSDU_DIRECTION_TX){
        if (txConn->state != CANTP_TX_STATE_WAIT_FC){
            return;
        }
        nAeSize = CanTp_GetAddrFieldLen(rxConn->nsdu->addressingFormat);

        if (CanTp_DecodeFrameType(&(PduInfoPtr->SduDataPtr[nAeSize])) == CANTP_N_PCI_TYPE_FC){
            nextState = CanTp_RxIndFC(txConn, PduInfoPtr, nAeSize);
        } 
        else{
            return;
        }
        txConn->state = nextState;
    }
    return;
}


/**
  @brief CanTp_TxConfirmation

  The lower layer communication interface module confirms the transmission of a PDU, or the failure to transmit a PDU.

*/
void CanTp_TxConfirmation(PduIdType TxPduId, Std_ReturnType result){
    CanTp_TxConnection *conn = getTxConnection(TxPduId);

    if (conn == NULL){
        return;
    }

    if (result == E_OK){
        if (conn->activation == CANTP_TX_PROCESSING && conn->state != CANTP_TX_STATE_FREE){
        }
    } 
    else{
        if (conn->activation == CANTP_TX_PROCESSING && conn->state != CANTP_TX_STATE_FREE){
            conn->state = CANTP_TX_STATE_CANCEL;
        }
    }
}
