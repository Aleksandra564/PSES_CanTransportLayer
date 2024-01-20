#ifndef COMSTACK_TYPES_H
#define COMSTACK_TYPES_H

#include "Std_Types.h"
#include "Platform_Types.h"

typedef uint16 PduIdType;
typedef uint32 PduLengthType;

typedef enum{
    BUFREQ_OK,       // buffer request accomplished successful
    BUFREQ_E_NOT_OK, // buffer request not successful
    BUFREQ_BUSY,     // temporarily no buffer available
    BUFREQ_OVFL      // no buffer of the required length can be provided
}BufReq_ReturnType;

typedef enum{
    TP_DATA_CONF,   // all data have been copied, and can be removed from TP buffer
    TP_DATARETRY,   // API call shall copy already copied data in rder to recover from an error
    TP_CONFPENDING  // previously copied data must remain in the TP
}TpDataStateType;

typedef enum{
    TP_STMIN,   // Separation Time
    TP_BS,      // Block Size
    TP_BC       // bandwidth control parameter
}TPParameterType;

typedef struct{
    uint8*        SduDataPtr;   // pointer to the payload data of PDU
    uint8*        MetaDataPtr;  // pointer to metadata (ie CAN ID) // dodatkowe informacje, można zignorować
    PduLengthType SduLength;    // lengthof the SDU in bytes // tego nie ruszać, obsluguje to warstwa niżej
}PduInfoType;

typedef struct{
    TpDataStateType TpDataStateType;    // store state of Tp buffer
    PduLengthType   TxTpDataCnt;        // number of bytes to be retransmitted 

}RetryInfoType;

#endif /* COMSTACK_TYPES_H */