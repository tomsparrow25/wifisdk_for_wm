#ifndef TKIP_ROM_H__
#define TKIP_ROM_H__

#include "bufferMgmtLib.h"
#include <string.h> 
//#include "hal_encr.h"

// fixed algorithm "parameters"
#define PHASE1_LOOP_CNT         8    // this needs to be "big enough"
#define TA_SIZE                 6    //  48-bit transmitter address
#define TK_SIZE                 16    // 128-bit temporal key
#define P1K_SIZE                10    // 80-bit Phase1 key
#define RC4_KEY_SIZE            16    // 128-bit RC4KEY (104 bits unknown)
#define WPA_AES_KEY_SIZE        16

#define MICFAILTIMEOUTTIME      5000 //5000 x 12  = 60000 msec = 60 sec

// macros for extraction/creation of UINT8/UINT16 values
#define Lo8(v16)                ((UINT8)( (v16)       & 0x00FF))
#define Hi8(v16)                ((UINT8)(((v16) >> 8) & 0x00FF))
#define Lo16(v32)               ((UINT16)( (v32)       & 0xFFFF))
#define Hi16(v32)               ((UINT16)(((v32) >>16) & 0xFFFF))

#define RotR1(v16)              ((((v16) >> 1) & 0x7FFF) ^ (((v16) & 1) << 15))
#define Mk16(hi,lo)             ((lo) ^ (((UINT16)(hi)) << 8))

// select the Nth 16-bit word of the temporal key UINT8 array TK[]
#define TK16(N)                 Mk16(TK[2*(N)+1],TK[2*(N)])

// S-box lookup: 16 bits --> 16 bits
#define _S_(v16)                (Sbox[0][Lo8(v16)] ^ Sbox[1][Hi8(v16)])

#define ROL32(B,A,n)            B = (((A) << (n)) | ((A) >> (32-(n))))
#define ROR32(A,B,n)            ROL32((A),(B),(32-(n)))
#define XSWAP(A)                (((A & 0xff00ff00) >> 8)        \
                                 | ((A & 0x00ff00ff) << 8))

#define CheckMIC(calc_MIC, rx_MIC)                              \
        ((*calc_MIC == *(UINT32*)rx_MIC) &&                     \
        (*(calc_MIC + 1) == *(UINT32*)(rx_MIC + 4)))

#define InsertIVintoFrmBody(FramePtr, RC4Key, IV32, keyID)      \
{                                                               \
    FramePtr[0] = RC4Key[0];                                    \
    FramePtr[1] = RC4Key[1];                                    \
    FramePtr[2] = RC4Key[2];                                    \
    FramePtr[3] = keyID << 6;                                   \
    FramePtr[3] |= EXT_IV;                                      \
    *((UINT32 *)(FramePtr+4)) = IV32;                           \
}

#define appendMIC(data_ptr,MIC)                                 \
{                                                               \
    data_ptr[0] = *MIC;                                         \
    data_ptr[1] = *MIC >> 8;                                    \
    data_ptr[2] = *MIC >> 16;                                   \
    data_ptr[3] = *MIC >> 24;                                   \
    data_ptr[4] = *(MIC+1);                                     \
    data_ptr[5] = *(MIC+1) >> 8;                                \
    data_ptr[6] = *(MIC+1) >> 16;                               \
    data_ptr[7] = *(MIC+1) >> 24;                               \
}

#define block_function(L,R)                                     \
{                                                               \
    UINT32 temp;                                                \
    ROL32(temp,*L,17);                                          \
    *R ^= temp;                                                 \
    *L += *R;                                                   \
    *R ^= XSWAP(*L);                                            \
    *L += *R;                                                   \
    ROL32(temp,*L,3);                                           \
    *R ^= temp;                                                 \
    *L += *R;                                                   \
    ROR32(temp,*L,2);                                           \
    *R ^= temp;                                                 \
    *L += *R;                                                   \
}

#define S_SWAP(a,b)                                             \
    do { unsigned char  t = S[a]; S[a] = S[b]; S[b] = t; } while (0)

#if 0
typedef struct
{
    BufferDesc_t    *bufDesc;
    UINT8           *pCurrent;
    UINT32          time_out_time;
    UINT32          time_out_timeMSB;
    UINT16          SeqNum;
    UINT8           FragNum;
} DeFragBufInfo_t;


#ifdef WAR_ROM_WEP_ICV_ERROR_EVENT_PATCH
extern BOOLEAN (*ResetWepIcvEvent_hook)(IEEEtypes_Frame_t *frame, void *dummy);

extern BOOLEAN (*StoreWepIcvEvent_hook)(void *connPtr,
                                        IEEEtypes_GenHdr_t *hdr, 
                                        UINT8 keyIdx, UINT8 key_mode, UINT8 *key, 
                                        void *dummy);
#endif

extern void ResetDeFragBufInfo(DeFragBufInfo_t *pDeFragBufInfo);

extern BOOLEAN (*DoWPADeFrag_hook)(BufferDesc_t *bufDesc,
                                   IEEEtypes_Frame_t *pEncrData,
                                   BufferDesc_t **pWPADeFragBufDesc,
                                   DeFragBufInfo_t *DeFragBufInfo,
                                   UINT8 *WaitForFirstFragment,
                                   UINT32 * ptr_val);
extern BufferDesc_t* DoWPADeFrag(BufferDesc_t *bufDesc,
                                 IEEEtypes_Frame_t *pEncrData,
                                 BufferDesc_t **pWPADeFragBufDesc,
                                 DeFragBufInfo_t *DeFragBufInfo,
                                 UINT8 *WaitForFirstFragment);

extern BOOLEAN (*DoWEPDeFrag_hook)(BufferDesc_t *bufDesc, UINT8 *pKey,
                                   UINT8 wep_mode,
                                   UINT8 keyidx,
                                   BufferDesc_t **pWEPDeFragBufDesc,
                                   DeFragBufInfo_t *DeFragBufInfo, 
                                   void *dummy,     /* wepIcvEvtIndices[] obsolete */
                                   void *dummy1,    /* host_802_11GetLog_t* Log obsolete */
                                   UINT32 * ptr_val);
extern BufferDesc_t * DoWEPDeFrag(BufferDesc_t *bufDesc,
                                  UINT8 *pKey, UINT8 wep_mode,
                                  UINT8 keyidx,
                                  BufferDesc_t **pWEPDeFragBufDesc,
                                  DeFragBufInfo_t *DeFragBufInfo,
                                  void *dummy,     /* wepIcvEvtIndices[] obsolete */
                                  void *dummy1); /* host_802_11GetLog_t* Log obsolete */

extern Status_e (*ramHook_CBProc_SendEvent)(void *connPtr,
                                            uint32 Event);
extern void (*ramHook_SEND_HOST_EVENT_WITH_INFO)(void *connPtr,
                                                 UINT16 event, UINT8 *pAddr,
                                                 UINT32 len);
extern void (*ramHook_FreeMACBuffer)(UINT16 StartRdPtr, UINT16 EndRdPtr,
                                     UINT8 RingBufId);
extern void* (*ramHook_dma_memcpy)(void *dst, void *src, size_t size);
extern void (*ramHook_wlan_set_startpkt_offset)(BufferDesc_t * bufDesc);
extern void (*ramHook_DoWepDefrag_updateRsnStats)(void * connPtr);
extern void (*ramHook_DoWepDefrag_vistaResetIcvEvent)(UINT32 bufDesc);

extern UINT32 ramHook_timeout_val;
extern pool_Config_t* ramHook_rxPoolConfig;
extern UINT32 ramHook_RX_DATA_OFFSET;
extern void ResetDeFragBufInfo(DeFragBufInfo_t *pDeFragBufInfo);
extern void (*ramHook_ReleaseRXBuffer)(BufferDesc_t* bufDesc);        
#endif
#endif
