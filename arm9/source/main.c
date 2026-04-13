/*
*   main.c
*/

#include <string.h>

#include "types.h"
#include "crypto.h"
#include "i2c.h"
#include "fs.h"
#include "firm.h"
#include "utils.h"
#include "buttons.h"
#include "fatfs/sdmmc/unprotboot9_sdmmc.h"
#include "ndma.h"
#include "cache.h"

#include "chainloader.h"

typedef enum FirmLoadStatus {
    FIRM_LOAD_OK = 0,
    FIRM_LOAD_CANT_READ, // can't mount, file missing or empty
    FIRM_LOAD_CORRUPT,
} FirmLoadStatus;

static volatile Arm11Operation *operation = (volatile Arm11Operation *)0x1FF80204;
extern u8 __itcm_start__[], __itcm_lma__[], __itcm_bss_start__[], __itcm_end__[];

static void invokeArm11Function(Arm11Operation op)
{
    while(*operation != ARM11_READY);
    *operation = op;
    while(*operation != ARM11_READY); 
}

static FirmLoadStatus loadFirm(Firm **outFirm)
{
    static const char *firmName = "boot.firm";
    Firm *firmHeader = (Firm *)0x080A0000;
    u32 rd = fileRead(firmHeader, firmName, 0x200, 0);
    if (rd != 0x200)
        return rd == 0 ? FIRM_LOAD_CANT_READ : FIRM_LOAD_CORRUPT;

    bool isPreLockout = ((firmHeader->reserved2[0] & 2) != 0);
    if ((CFG9_SYSPROT9 & 1) != 0 || (CFG9_SYSPROT11 & 1) != 0)
        isPreLockout = false;
    Firm *firm;
    u32 maxFirmSize;

    if(!isPreLockout)
    {
        //Lockout
        while(!(CFG9_SYSPROT9  & 1)) CFG9_SYSPROT9  |= 1;
        while(!(CFG9_SYSPROT11 & 1)) CFG9_SYSPROT11 |= 1;
        invokeArm11Function(WAIT_BOOTROM11_LOCKED);

        firm = (Firm *)0x20001000;
        maxFirmSize = 0x07FFF000; //around 127MB (although we don't enable ext FCRAM on N3DS, beware!)
    }
    else
    {
        //Uncached area, shouldn't affect performance too much, though
        firm = (Firm *)0x18000000;
        maxFirmSize = 0x300000; //3MB
    }

    *outFirm = firm;

    u32 calculatedFirmSize = checkFirmHeader(firmHeader, (u32)firm, isPreLockout);

    if(!calculatedFirmSize || fileRead(firm, firmName, 0, maxFirmSize) < calculatedFirmSize || !checkSectionHashes(firm))
        return FIRM_LOAD_CORRUPT;
    else
        return FIRM_LOAD_OK;
}

static void bootFirm(Firm *firm, bool isNand)
{
    bool isScreenInit = (firm->reserved2[0] & 1) != 0;
    if(isScreenInit)
    {
        invokeArm11Function(INIT_SCREENS);
        I2C_writeReg(I2C_DEV_MCU, 0x22, 0x2A); //Turn on backlight
    }

    memcpy(__itcm_start__, __itcm_lma__, __itcm_bss_start__ - __itcm_start__);
    memset(__itcm_bss_start__, 0, __itcm_end__ - __itcm_bss_start__);

    //Launch firm
    invokeArm11Function(PREPARE_ARM11_FOR_FIRMLAUNCH);
    __dsb();

    flushEntireDCache();
    chainload(firm, isNand);
    __builtin_unreachable();
}

void arm9Main(void)
{
    Firm *firm = NULL;

    setupKeyslots();
    ndmaInit();
    unprotboot9_sdmmc_initialize();

    unmountSd();
    unmountCtrNand();

    if (mountSd() && loadFirm(&firm) == FIRM_LOAD_OK){
          bootFirm(firm, sdStatus != FIRM_LOAD_OK);
    }else{
        mcuPowerOff();
    }

    __builtin_unreachable();
}
