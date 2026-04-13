#include "types.h"
#include "PXI.h"
#include "arm11.h"
#include "fatfs/ff.h"

#define LCD_FILL_ENABLE(n)      ((1U << 24) | n)
vu32 *errchk_color = (vu32 *)0x10202204;

#define CFG11_SHAREDWRAM_32K_DATA(i)    (*(vu8 *)(0x10140000 + i))
#define CFG11_SHAREDWRAM_32K_CODE(i)    (*(vu8 *)(0x10140008 + i))
#define CFG11_DSP_CNT                   (*(vu8 *)0x10141230)

struct fb {
     u8 *top_left;
     u8 *top_right;
     u8 *bottom;
} ;

static const struct fb fbs[2] =
{
    {
        .top_left  = (u8 *)0x18000000,
        .top_right = (u8 *)0x18000000,
        .bottom    = (u8 *)0x18046500,
    },
    {
        .top_left  = (u8 *)0x18000000,
        .top_right = (u8 *)0x18000000,
        .bottom    = (u8 *)0x18046500,
    },
};

static FATFS sdFs;

static void resetDSPAndSharedWRAMConfig(void)
{
    CFG11_DSP_CNT = 2; // PDN_DSP_CNT
    for(volatile int i = 0; i < 10; i++);

    CFG11_DSP_CNT = 3;
    for(volatile int i = 0; i < 10; i++);

    for(int i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = i << 2; // disabled, master = arm9

    for(int i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = i << 2; // disabled, master = arm9

    for(int i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_DATA(i) = 0x80 | (i << 2); // enabled, master = arm9

    for(int i = 0; i < 8; i++)
        CFG11_SHAREDWRAM_32K_CODE(i) = 0x80 | (i << 2); // enabled, master = arm9
}

static void doFirmlaunch(void)
{

    while(PXIReceiveWord() != 0x44836);
    PXISendWord(0x964536);
    while(PXIReceiveWord() != 0x44837);
    PXIReceiveWord(); // High FIRM titleID
    PXIReceiveWord(); // Low FIRM titleID
    resetDSPAndSharedWRAMConfig();

     while(PXIReceiveWord() != 0x44846);

     *(vu32 *)0x1FFFFFF8 = 0;
     memcpy((void *)0x1FFFF400, arm11FirmlaunchStub, arm11FirmlaunchStubSize);


     
     if( f_mount(&sdFs, "0:", 1) == FR_OK ){
         FIL f;
         if(f_open(&f,"/SafeB9S.bin",1) == FR_OK){
             u32 ret = 0;
             if((FRESULT)f_read(&f,(void *)0x23F00000, (u32)0x100000, (unsigned int *)&ret) == FR_OK && ret != 0 ){
                  
                  *(vu32 *)0x1FFFFFFC = 0x1FFFF400;
                  return;
                  
             }else{
                   *errchk_color = LCD_FILL_ENABLE(0xFFFF00);
             }
         }else{
              *errchk_color = LCD_FILL_ENABLE(0xFF00FF);
         }
    }else{
         *errchk_color = LCD_FILL_ENABLE(0x00FFFF);
    }
     *(vu32 *)0x1FFFFFFC = 0x1FFFF404;
     while((~(*(volatile u32*)0x10146000) & BUTTON_ANY) & 0x00000FFF);
     i2cWriteRegister(I2C_DEV_MCU, 0x20, 1 << 0);
     while(true);
}

static void patchSvcReplyAndReceive11(void)
{
    /*
       Basically, we're patching svc 0x4F's contents to svcKernelSetState(0, (u64)<dontcare>).
       Assumption: kernel .text is in the same 64KB as the first SVCs.
    */
    u32 *off, *svcTable;

    for(off = (u32 *)0x1FF80000; off[0] != 0xF96D0513 || off[1] != 0xE94D6F00; off++);
    for(; *off != 0; off++);
    svcTable = off;

    u32 baseAddr = svcTable[1] & ~0xFFFF;
    u32 *patch = (u32 *)(0x1FF80000 + svcTable[0x4F] - baseAddr);
    patch[0] = 0xE3A00000;
    patch[1] = 0xE51FF004;
    patch[2] = svcTable[0x7C];
}

void main(void)
{
             
    memcpy((void *)0x23FFFE00, fbs, 2 * sizeof(struct fb));
    patchSvcReplyAndReceive11();
    doFirmlaunch();
}
