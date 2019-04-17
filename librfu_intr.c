#include "global.h"
#include "main.h"
#include "librfu.h"

extern void sub_82E3EB0(u32);                //bx r0
//extern void sub_82E3EAC(/*args*/);         //bx r1
extern void sub_82E3EA8(u8, u16, void(*)()); //bx r2

//This file's functions
void IntrSIO32(void);
void sio32intr_clock_master(void);
extern void sio32intr_clock_slave(void);
u16 handshake_wait(u16);
void STWI_set_timer_in_RAM(u8);
void STWI_stop_timer_in_RAM(void);
void STWI_init_slave(void);


void IntrSIO32(void)
{
    if (gRfuState->unk_0 == 10)
    {
        if (gRfuState->callbackID)
            sub_82E3EB0(gRfuState->callbackID);
    }
    else if (gRfuState->msMode == 1)
        sio32intr_clock_master();
    else
        sio32intr_clock_slave();
}

//register swaps in two places
void sio32intr_clock_master(void)
{
    u32 regSIODATA32;
    STWI_set_timer_in_RAM(80);
    regSIODATA32 = REG_SIODATA32;
    
    if (gRfuState->unk_0 == 0)
    {
        if (regSIODATA32 == 0x80000000)
        {
            if (gRfuState->unk_5 <= gRfuState->txParams)
            {
                REG_SIODATA32 = ((u32*)gRfuState->txPacket->rfuPacket8.data)[gRfuState->unk_5];
                gRfuState->unk_5++;
            }   
            else
            {
                gRfuState->unk_0 = 1;
                REG_SIODATA32 = 0x80000000;
            }
        }
        else
        {
            STWI_stop_timer_in_RAM();
            STWI_set_timer_in_RAM(130);
            return;
        }
    }
    else if (gRfuState->unk_0 == 1)//_082E3638
    {
        if ((regSIODATA32 & 0xFFFF0000) == 0x99660000)
        {
            gRfuState->unk_8 = 0;
            ((u32*)gRfuState->txPacket->rfuPacket8.data)[gRfuState->unk_8] = regSIODATA32;
            gRfuState->unk_8++;
            gRfuState->unk_9 = regSIODATA32;
            gRfuState->unk_7 = regSIODATA32 >> 8; //register swap
            if (gRfuState->unk_7 >= gRfuState->unk_8)
            {
                gRfuState->unk_0 = 2;
                REG_SIODATA32 = 0x80000000;
            }
            else
            {
                gRfuState->unk_0 = 3;
            }
        }
        else //_082E36B8
        {
            STWI_stop_timer_in_RAM();
            STWI_set_timer_in_RAM(130);
            return;
        }
    }
    else if (gRfuState->unk_0 == 2)//_082E36C8
    {
        ((u32*)gRfuState->txPacket->rfuPacket8.data)[gRfuState->unk_8] = regSIODATA32;
        gRfuState->unk_8++;
        if (gRfuState->unk_7 < gRfuState->unk_8)
            gRfuState->unk_0 = 3;
        else
            REG_SIODATA32 = 0x80000000;
    }
    //_082E3714
    if (handshake_wait(1) == 1)
        return;
    
    REG_SIOCNT = 0x500B;
    
    if (handshake_wait(0) == 1)
        return;
    
    STWI_stop_timer_in_RAM();
    
    if (gRfuState->unk_0 == 3) //register swap
    {
        if (gRfuState->unk_9 == 167 || gRfuState->unk_9 == 165 || gRfuState->unk_9 == 181 || gRfuState->unk_9 == 183)
        {
            //_082E37D0
            gRfuState->msMode = 0;
            REG_SIODATA32 = 0x80000000;
            REG_SIOCNT = 0x5002;
            REG_SIOCNT = 0x5082;
            gRfuState->unk_0 = 5;
        }
        else
        {
            //_082E3788
            if (gRfuState->unk_9 == 238)
            {
                REG_SIOCNT = 0x5003;
                gRfuState->unk_0 = 4;
                gRfuState->unk_12 = 4;
            }
            else
            {
                REG_SIOCNT = 0x5003;
                gRfuState->unk_0 = 4;
            }
        }
        gRfuState->unk_2c = 0;
        if (gRfuState->callbackM)
            sub_82E3EA8(gRfuState->activeCommand, gRfuState->unk_12, gRfuState->callbackM);
    }
    else
    {
        REG_SIOCNT = 0x5003;
        REG_SIOCNT = 0x5083;
    }
}

/*                                     ***WIP***
void sio32intr_clock_slave(void)
{
    u32 regSIODATA32;
    
    gRfuState->timerSelect = 0;
    STWI_set_timer_in_RAM(100);
    if (handshake_wait(0) == 1)
        return;
    REG_SIOCNT = 0x500A;
    
    regSIODATA32 = REG_SIODATA32;
    if (gRfuState->unk_0 == 5)
    {
        gRfuState->rxPacket->rfuPacket32.command = regSIODATA32;
        gRfuState->unk_5 = 1;
         
            
    }
    else //_082E3978
    {
        
    }
}*/ 

u16 handshake_wait(u16 arg0)
{
    while (1)
    {
        if (gRfuState->timerActive == 1)
        {
            gRfuState->timerActive = 0;
            return 1;
        }
        if ((REG_SIOCNT & 4) == (arg0 << 2))
            break;
    } 
    return 0;
}

void STWI_set_timer_in_RAM(u8 r0)
{
    vu16* regTMCNTL = (vu16*)(REG_ADDR_TMCNT_L + gRfuState->timerSelect * 4);
    vu16* regTMCNTH = (vu16*)(REG_ADDR_TMCNT_H + gRfuState->timerSelect * 4);
    
    REG_IME = 0;
    
    switch (r0)
    {
    case 50:
        *regTMCNTL = 0xFCCB;
        gRfuState->timerState = 1;
        break;
    case 80:
        *regTMCNTL = 0xFAE0;
        gRfuState->timerState = 2;
        break;
    case 100:
        *regTMCNTL = 0xF996;
        gRfuState->timerState = 3;
        break;
    case 130:
        *regTMCNTL = 0xF7AD;
        gRfuState->timerState = 4;
        break;
    }
    *regTMCNTH = 0x00C3;
    
    REG_IF = 8 << gRfuState->timerSelect;
    REG_IME = 1;
}

void STWI_stop_timer_in_RAM(void)
{
    gRfuState->timerState = 0;
    REG_TMCNT_L(gRfuState->timerSelect) = 0;
    REG_TMCNT_H(gRfuState->timerSelect) = 0;
}

void STWI_init_slave(void)
{
    gRfuState->unk_0 = 5;
    gRfuState->msMode = 0;
    gRfuState->txParams = 0;
    gRfuState->unk_5 = 0;
    gRfuState->activeCommand = 0;
    gRfuState->unk_7 = 0;
    gRfuState->unk_8 = 0;
    gRfuState->unk_9 = 0;
    gRfuState->timerState = 0;
    gRfuState->timerActive = 0;
    gRfuState->unk_12 = 0;
    gRfuState->unk_15 = 0;
    REG_SIOCNT = 0x5082;
}
