
#ifndef _HOSTMSGHDR_H
#define _HOSTMSGHDR_H

typedef PACK_START  struct host_MsgHdr_t
{
    UINT16 Msg;          /*Command number - host_Msg_e*/
    UINT16 Size;         /*Size of the data structure*/
    UINT8 SeqNum;       /*Command sequence number*/
    UINT8 BssNum:4;
    UINT8 BssType:4;
    UINT16 Result;       /*Result code - host_Result_e*/
}
PACK_END host_MsgHdr_t;


#endif

