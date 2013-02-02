#include <windows.h>
#include <pehdr.h>
#include <romldr.h>
#include <bsp_cfg.h>
#include "image_cfg.h"
#include "utils.h"
//#define BUZZE_ON              (0x0<<15)
//#define BUZZE_OFF             (0x1<<15)
//void Buzze_init(int);

#define NAND_SB_PAGE_SIZE_BYTES    (0x200)        // 1 Page = 0x200 (512 Bytes)
#define NAND_SB_BLOCK_SIZE_BYTES    (0x4000)        // 1 Block = 16 KB
#define NAND_SB_PAGES_PER_BLOCK    (NAND_SB_BLOCK_SIZE_BYTES / NAND_SB_PAGE_SIZE_BYTES)    // 32-pages

#define NAND_LB_PAGE_SIZE_BYTES    (0x800)        // 1 Page = 0x800 (2048 Bytes)
#define NAND_LB_BLOCK_SIZE_BYTES    (0x20000)    // 1 Block = 128 KB
#define NAND_LB_PAGES_PER_BLOCK    (NAND_LB_BLOCK_SIZE_BYTES / NAND_LB_PAGE_SIZE_BYTES)    // 64-pages

#define NAND_BYTES_PER_PAGE        ((g_bLargeBlock==TRUE)?NAND_LB_PAGE_SIZE_BYTES:NAND_SB_PAGE_SIZE_BYTES)
#define NAND_PAGES_PER_BLOCK        ((g_bLargeBlock==TRUE)?NAND_LB_PAGES_PER_BLOCK:NAND_SB_PAGES_PER_BLOCK)

    // NOTE: we assume that this Steppingstone loader occupies *part* the first (good) NAND flash block.  More
    // specifically, the loader takes up 4096 bytes (or 8 NAND pages) of the first block.  We'll start our image
    // copy on the very next page.

#define LOAD_ADDRESS_PHYSICAL        (IMAGE_EBOOT_PA_START)
#define LOAD_IMAGE_BYTE_COUNT        (IMAGE_EBOOT_SIZE)
#define LOAD_IMAGE_PAGE_COUNT    (LOAD_IMAGE_BYTE_COUNT / NAND_BYTES_PER_PAGE)
#define LOAD_IMAGE_PAGE_OFFSET    (NAND_PAGES_PER_BLOCK * 2)    // StpeLoader uses 2 Block

// Globals variables.
ROMHDR * volatile const pTOC = (ROMHDR *)-1;
// For Large Block Check
static BOOL g_bLargeBlock;

// Function in "Nand.c"
extern BOOL NAND_Init(void);
extern BOOL NAND_ReadPage(UINT32 block, UINT32 sector, volatile BYTE *buffer);

typedef void (*PFN_IMAGE_LAUNCH)();

static BOOLEAN SetupCopySection(ROMHDR *const pTOC)
{
    // This code doesn't make use of global variables so there are no copy sections.
    // To reduce code size, this is a stub function...
    return(TRUE);
}

#define UART_DEBUG  (FALSE)     //< This will increase code size incredibly, if not in evitable, do not use this.
                                //< And, sources file also do not compile and link utils.c

void main(void)
{
    register nBlock;
    register nPage;
    register nBadBlocks;
    volatile unsigned char *pBuf;

    // Set up copy section (initialized globals).
    //
    // NOTE: after this call, globals become valid.
    //
    SetupCopySection(pTOC);

    // Enable the ICache.
    //System_EnableICache();        // I-Cache was already enabled in startup.s

    // Set up all GPIO ports for LED.
    Port_Init();
    Led_Display(0xf);
  //  Buzze_init(BUZZE_ON);

    // UART Initialize
#if UART_DEBUG
    Uart_Init();
    Uart_SendString("\r\nWinCE 6.0 Steploader for SMDK6410\r\n");
    // Initialize the NAND flash interface.
    Uart_SendString("NAND Initialize\n\r");
#endif    
    g_bLargeBlock = NAND_Init();
  

    // Copy image from NAND flash to RAM.
    pBuf = (unsigned char *)LOAD_ADDRESS_PHYSICAL;
    nBadBlocks = 0;
    //Led_Display(0x4);    
    for (nPage = LOAD_IMAGE_PAGE_OFFSET; nPage < (LOAD_IMAGE_PAGE_OFFSET + LOAD_IMAGE_PAGE_COUNT) ; nPage++)
    {
        //Led_Display(0x1);      
        nBlock = ((nPage / NAND_PAGES_PER_BLOCK) + nBadBlocks);

        if (!NAND_ReadPage(nBlock, (nPage % NAND_PAGES_PER_BLOCK), pBuf))
        {
            if ((nPage % NAND_PAGES_PER_BLOCK) != 0)
            {
                //Led_Display(0x9);    // real ECC Error.
#if UART_DEBUG
                Uart_SendString("ECC Error.\r\n");
#endif

                while(1)
                {
                    // Spin forever...
                }
            }

            // ECC error on a block boundary is (likely) a bad block - retry the page 0 read on the next block.
            nBadBlocks++;
            nPage--;

            continue;
        }

        pBuf += NAND_BYTES_PER_PAGE;
    }
   //Buzze_init(BUZZE_OFF);	
    //Led_Display(0x6);
    Led_Display(0x0);
#if UART_DEBUG
    Uart_SendString("Launch Eboot...\n\r");
#endif

    ((PFN_IMAGE_LAUNCH)(LOAD_ADDRESS_PHYSICAL))();
}

