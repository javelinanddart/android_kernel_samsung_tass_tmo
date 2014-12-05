//--------------------------------------------------------
//
//
//	Melfas MCS8000 Series Download base v1.0 2010.04.05
//
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/preempt.h>
//
#include <asm/gpio.h>
#include <asm/io.h>

#include "melfas_download.h"

#include <mach/gpio.h>
#include <mach/vreg.h>

//============================================================
//
//	Include MELFAS Binary code File ( ex> MELFAS_FIRM_bin.c)
//
//	Warning!!!!
//		Please, don't add binary.c file into project
//		Just #include here !!
//
//============================================================
//#include "COOPERPLUS_R01_V01.c" - i2c addr = 72; 5 finger touch support

#include "Firmware_FLIPBOOK02_INNER_HW01_SW07.c"
#include "Firmware_FLIPBOOK02_OUTER_HW01_SW07.c"
#include "Firmware_FLIPBOOK02_INNER_HW02_SW07.c"
#include "Firmware_FLIPBOOK02_OUTER_HW02_SW07.c"

#define INNER 0

UINT8  ucVerifyBuffer[MELFAS_TRANSFER_LENGTH];		//	You may melloc *ucVerifyBuffer instead of this
extern int working_TSP_for_fwupdate;


//---------------------------------
//	Downloading functions
//---------------------------------
static int mcsdl_download(const UINT8 *pData, const UINT16 nLength, INT8 IdxNum);

static void mcsdl_set_ready(void);
static void mcsdl_reboot_mcs(void);

static int mcsdl_erase_flash(INT8 IdxNum);
static int mcsdl_program_flash(UINT8 *pDataOriginal, UINT16 unLength,
		INT8 IdxNum);
static void mcsdl_program_flash_part(UINT8 *pData);

static int mcsdl_verify_flash(UINT8 *pData, UINT16 nLength, INT8 IdxNum);

static void mcsdl_read_flash(UINT8 *pBuffer);
static int mcsdl_read_flash_from(UINT8 *pBuffer, UINT16 unStart_addr,
		UINT16 unLength, INT8 IdxNum);

static void mcsdl_select_isp_mode(UINT8 ucMode);
static void mcsdl_unselect_isp_mode(void);

static void mcsdl_read_32bits(UINT8 *pData);
static void mcsdl_write_bits(UINT32 wordData, int nBits);
static void mcsdl_scl_toggle_twice(void);

//---------------------------------
//	Delay functions
//---------------------------------
static void mcsdl_delay(UINT32 nCount);

//---------------------------------
//	For debugging display
//---------------------------------
#if MELFAS_ENABLE_DBG_PRINT
static void mcsdl_print_result(int nRet);
#endif
#define DEBUG_PRINT 1

//----------------------------------
// Download enable command
//----------------------------------
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD

void melfas_send_download_enable_command(void)
{
	// TO DO : Fill this up

}

#endif

#if MCSDL_USE_VDD_CONTROL
void mcsdl_vdd_on(void)
{ 

	struct vreg *vreg_touch;
	int ret = 0;

#if DEBUG_PRINT
//	printk(KERN_ERR "%s TSP TYPE %d\n",__func__,working_TSP_for_fwupdate);
#endif	

	if( working_TSP_for_fwupdate == INNER )
		vreg_touch = vreg_get(NULL, "ldo19"); // inner TSP
	else
		vreg_touch = vreg_get(NULL, "ldo6"); // inner TSP

	if (ret) {
		printk(KERN_ERR "%s: vreg set level failed (%d)\n",
				__func__, ret);
		return -EIO;
	}

    ret = vreg_enable(vreg_touch);
 
	if (ret) {
		printk(KERN_ERR "%s: vreg enable failed (%d)\n",
				__func__, ret);
		return -EIO;
	}


	return true;



}

void mcsdl_vdd_off(void)
{

	struct vreg *vreg_touch;
	int ret = 0;

#if DEBUG_PRINT
//	printk(KERN_ERR "%s TSP TYPE %d\n",__func__,working_TSP_for_fwupdate);
#endif	

	if( working_TSP_for_fwupdate == INNER )
		vreg_touch = vreg_get(NULL, "ldo19"); // inner TSP
	else
		vreg_touch = vreg_get(NULL, "ldo6"); // outer TSP

    ret = vreg_disable(vreg_touch);

	if (ret) {
		printk(KERN_ERR "%s: vreg disable failed (%d)\n",
				__func__, ret);
		return -EIO;
	}

	return true;


}
#endif

//============================================================
//
//	Main Download furnction
//
//   1. Run mcsdl_download( pBinary[IdxNum], nBinary_length[IdxNum], IdxNum);
//       IdxNum : 0 (Master Chip Download)
//       IdxNum : 1 (2Chip Download)
//
//
//============================================================

int mcsdl_download_binary_data(int HW_ver)
{
	int nRet;
	UINT8 * pBinary;
	UINT16  nLength;
	
	printk(KERN_INFO "(%s) start. \n",__func__);

	if (HW_ver <= 1) {
		if (working_TSP_for_fwupdate == INNER)
		{
			pBinary = MELFAS_INNER_binary;
			nLength = MELFAS_INNER_binary_nLength;
		}
		else
		{
			pBinary = MELFAS_OUTER_binary;
			nLength = MELFAS_OUTER_binary_nLength;
		}
	} 
	else if (HW_ver == 2) {
		if (working_TSP_for_fwupdate == INNER)
		{
			pBinary = MELFAS_INNER_HW02_binary;
			nLength = MELFAS_INNER_HW02_binary_nLength;
		}
		else
		{
			pBinary = MELFAS_OUTER_HW02_binary;
			nLength = MELFAS_OUTER_HW02_binary_nLength;
		}		
	}
	else 
	{
		printk(KERN_INFO "(%s) unknown TSP HW version\n",__func__);
		return MCSDL_RET_WRONG_MODULE_REVISION;
	}
			
#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR(); // Disable Baseband touch interrupt ISR.
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET(); // Disable Baseband watchdog timer

	//------------------------
	// Run Download
	//------------------------
	nRet = mcsdl_download((const UINT8*) pBinary,
			(const UINT16) nLength, 0);
#if defined (MELFAS_2CHIP_DOWNLOAD_ENABLE)
	nRet = mcsdl_download( (const UINT8*) MELFAS_binary_2, (const UINT16)MELFAS_binary_nLength_2, 1); // Slave Binary data download
#endif
	MELFAS_ROLLBACK_BASEBAND_ISR(); // Roll-back Baseband touch interrupt ISR.
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET(); // Roll-back Baseband watchdog timer
	
	printk(KERN_INFO "(%s) finish. \n",__func__);

	return (nRet == MCSDL_RET_SUCCESS);
}

int mcsdl_download_binary_file(void)
{
	int nRet;
	int i;

	UINT8 *pBinary[2] =	{ NULL, NULL };
	UINT16 nBinary_length[2] ={ 0, 0 };
	#if defined (MELFAS_2CHIP_DOWNLOAD_ENABLE)
	UINT8 IdxNum = 1;
	#else 
	UINT8 IdxNum = 0;
	#endif
	//==================================================
	//
	//	1. Read '.bin file'
	//   2. *pBinary[0]       : Binary data(Master)
	//       *pBinary[1]       : Binary data(Slave)
	//	   nBinary_length[0] : Firmware size(Master)
	//	   nBinary_length[1] : Firmware size(Slave)	
	//	3. Run mcsdl_download( pBinary[IdxNum], nBinary_length[IdxNum], IdxNum);
	//       IdxNum : 0 (Master Chip Download)
	//       IdxNum : 1 (2Chip Download)
	//
	//==================================================

#if 0

	// TO DO : File Process & Get file Size(== Binary size)
	//			This is just a simple sample

	FILE *fp;
	INT nRead;

	//------------------------------
	// Open a file
	//------------------------------

	if( fopen( fp, "MELFAS_FIRMWARE.bin", "rb" ) == NULL )
	{
		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	//------------------------------
	// Get Binary Size
	//------------------------------

	fseek( fp, 0, SEEK_END );

	nBinary_length = (UINT16)ftell(fp);

	//------------------------------
	// Memory allocation
	//------------------------------

	pBinary = (UINT8*)malloc( (INT)nBinary_length );

	if( pBinary == NULL )
	{

		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	//------------------------------
	// Read binary file
	//------------------------------

	fseek( fp, 0, SEEK_SET );

	nRead = fread( pBinary, 1, (INT)nBinary_length, fp ); // Read binary file

	if( nRead != (INT)nBinary_length )
	{

		fclose(fp); // Close file

		if( pBinary != NULL ) // free memory alloced.
		free(pBinary);

		return MCSDL_RET_FILE_ACCESS_FAILED;
	}

	//------------------------------
	// Close file
	//------------------------------

	fclose(fp);

#endif

#if MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD
	melfas_send_download_enable_command();
	mcsdl_delay(MCSDL_DELAY_100US);
#endif

	MELFAS_DISABLE_BASEBAND_ISR(); // Disable Baseband touch interrupt ISR.
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET(); // Disable Baseband watchdog timer

	for (i = 0; i <= IdxNum; i++)
	{
		if (pBinary[i] != NULL && nBinary_length[i] > 0 && nBinary_length[i]
				< 32 * 1024)
		{

			//------------------------
			// Run Download
			//------------------------
			nRet = mcsdl_download((const UINT8 *) pBinary[i],
					(const UINT16) nBinary_length[i], i);
		}
		else
		{

			nRet = MCSDL_RET_WRONG_BINARY;
		}
	}

	MELFAS_ROLLBACK_BASEBAND_ISR(); // Roll-back Baseband touch interrupt ISR.
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET(); // Roll-back Baseband watchdog timer

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result( nRet );
#endif

#if 0
	if( pData != NULL ) // free memory alloced.
	free(pData);
#endif

	return (nRet == MCSDL_RET_SUCCESS);

}

//------------------------------------------------------------------
//
//	Download function
//
//------------------------------------------------------------------

static int mcsdl_download(const UINT8 *pBianry, const UINT16 unLength,
		INT8 IdxNum)
{
	int nRet = 0;

	//---------------------------------
	// Check Binary Size
	//---------------------------------
	if (unLength >= MELFAS_FIRMWARE_MAX_SIZE)
	{

		nRet = MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
		goto MCSDL_DOWNLOAD_FINISH;
	}

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" - Starting download...\n");
#endif

	//---------------------------------
	// Make it ready
	//---------------------------------
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Ready\n");
#endif

	mcsdl_set_ready(); 

	//---------------------------------
	// Erase Flash
	//---------------------------------
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Erase\n");
#endif

	printk(" > Erase - Start \n");

	preempt_disable();
	nRet = mcsdl_erase_flash(IdxNum);
	preempt_enable();
	msleep(2);
	printk(" > Erase- End \n");

	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	//---------------------------------
	// Program Flash
	//---------------------------------
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Program   ");
#endif
	preempt_disable();
	nRet = mcsdl_program_flash((UINT8*) pBianry, (UINT16) unLength, IdxNum);	
	preempt_enable();
	msleep(2);
	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;
	//---------------------------------
	// Verify flash
	//---------------------------------

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Verify    ");
#endif

	preempt_disable();
	nRet = mcsdl_verify_flash((UINT8*) pBianry, (UINT16) unLength, IdxNum); 
	preempt_enable();

	if (nRet != MCSDL_RET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	nRet = MCSDL_RET_SUCCESS;

	MCSDL_DOWNLOAD_FINISH:

#if MELFAS_ENABLE_DBG_PRINT
	mcsdl_print_result( nRet ); // Show result
#endif

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" > Rebooting\n");
	printk(" - Fin.\n\n");
#endif

	mcsdl_reboot_mcs();

	return nRet;
}

//------------------------------------------------------------------
//
//	Sub functions
//
//------------------------------------------------------------------

static int mcsdl_erase_flash(INT8 IdxNum)
{
	int i;
	UINT8 readBuffer[32];

	//----------------------------------------
	//	Do erase
	//----------------------------------------
	if (IdxNum > 0)
		mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);

	mcsdl_select_isp_mode(ISP_MODE_ERASE_FLASH);
	mcsdl_unselect_isp_mode();

	//----------------------------------------
	//	Check 'erased well'
	//----------------------------------------
	//start ADD DELAY
	mcsdl_read_flash_from(readBuffer, 0x0000, 16, IdxNum);
	mcsdl_read_flash_from(&readBuffer[16], 0x7FF0, 16, IdxNum);
	//end ADD DELAY

	// Compare with '0xFF'
	for (i = 0; i < 32; i++)
	{
		if (readBuffer[i] != 0xFF)
			return MCSDL_RET_ERASE_FLASH_VERIFY_FAILED;
	}

	return MCSDL_RET_SUCCESS;
}

static int mcsdl_program_flash(UINT8 *pDataOriginal, UINT16 unLength,
		INT8 IdxNum)
{
	int i;

	UINT8 *pData;
	UINT8 ucLength;

	UINT16 addr;
	UINT32 header;

	addr = 0;
	pData = pDataOriginal;

	ucLength = MELFAS_TRANSFER_LENGTH;

	//kang

	while ((addr * 4) < (int) unLength)
	{

		if ((unLength - (addr * 4)) < MELFAS_TRANSFER_LENGTH)
		{
			ucLength = (UINT8)(unLength - (addr * 4));
		}

		//--------------------------------------
		//	Select ISP Mode
		//--------------------------------------

		// start ADD DELAY        
		mcsdl_delay(MCSDL_DELAY_20US);
		//end ADD DELAY        
		if (IdxNum > 0)
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_WRITE);

		//---------------------------------------------
		//	Header
		//	Address[13ibts] <<1
		//---------------------------------------------
		header = ((addr & 0x1FFF) << 1) | 0x0;
		header = header << 14;

		//Write 18bits
		mcsdl_write_bits(header, 18);
		//start ADD DELAY      
		//mcsdl_delay(MCSDL_DELAY_5MS);
		//end ADD DELAY        

		//---------------------------------
		//	Writing
		//---------------------------------
		//		addr += (UINT16)ucLength;
		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk("#");
#endif

		mcsdl_program_flash_part(pData);

		pData += ucLength;

		//---------------------------------------------
		//	Tail
		//---------------------------------------------
		MCSDL_GPIO_SDA_SET_HIGH();
		//kang
		mcsdl_delay(MCSDL_DELAY_20US);

		for (i = 0; i < 6; i++)
		{

			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_10US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_20US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_5US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_5US);
		}
		//kang
#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		//printk("\n");
#endif

		mcsdl_unselect_isp_mode();
		//start ADD DELAY
		mcsdl_delay(MCSDL_DELAY_100US);
		//end ADD DELAY


	}

	return MCSDL_RET_SUCCESS;
}

static void mcsdl_program_flash_part(UINT8 *pData)
{
	int i;
	UINT32 data;

	//---------------------------------
	//	Body
	//---------------------------------

	data = (UINT32) pData[0] << 0;
	data |= (UINT32) pData[1] << 8;
	data |= (UINT32) pData[2] << 16;
	data |= (UINT32) pData[3] << 24;
	mcsdl_write_bits(data, 32);

}

static int mcsdl_verify_flash(UINT8 *pDataOriginal, UINT16 unLength,
		INT8 IdxNum)
{
	int i, j;
	int nRet;

	UINT8 *pData;
	UINT8 ucLength;

	UINT16 addr;
	UINT32 wordData;

	addr = 0;
	pData = (UINT8 *) pDataOriginal;

	ucLength = MELFAS_TRANSFER_LENGTH;

	while ((addr * 4) < (int) unLength)
	{

		if ((unLength - (addr * 4)) < MELFAS_TRANSFER_LENGTH)
		{
			ucLength = (UINT8)(unLength - (addr * 4));
		}

		// start ADD DELAY        
		mcsdl_delay(MCSDL_DELAY_20US);

		//--------------------------------------
		//	Select ISP Mode
		//--------------------------------------
		if (IdxNum > 0)
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);
		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);

		//---------------------------------------------
		//	Header
		//	Address[13ibts] <<1
		//---------------------------------------------

		wordData = ((addr & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;

		mcsdl_write_bits(wordData, 18);

		addr += 1;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		printk("#");
#endif

		//--------------------
		// Read flash
		//--------------------
		mcsdl_read_flash(ucVerifyBuffer);

		//kang

		MCSDL_GPIO_SDA_SET_HIGH();

		//            mcsdl_delay(MCSDL_DELAY_1MS);
		for (i = 0; i < 6; i++)
		{

			if (i == 2)
				mcsdl_delay(MCSDL_DELAY_3US);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_20US);//(MCSDL_DELAY_1MS);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_5US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_5US);
		}
		//kang
		//--------------------
		// Comparing
		//--------------------

		for (j = 0; j < (int) ucLength; j++)
		{

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
			printk(" %02X", ucVerifyBuffer[j] );
#endif

			if (ucVerifyBuffer[j] != pData[j])
			{

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
				printk("\n [Error] Address : 0x%04X : 0x%02X - 0x%02X\n", addr, pData[j], ucVerifyBuffer[j] );
#endif

				nRet = MCSDL_RET_PROGRAM_VERIFY_FAILED;
				goto MCSDL_VERIFY_FLASH_FINISH;

			}
		}

		pData += ucLength;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		//printk("\n");
#endif

		mcsdl_unselect_isp_mode();
	}

	nRet = MCSDL_RET_SUCCESS;

	MCSDL_VERIFY_FLASH_FINISH:

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk("\n");
#endif

	mcsdl_unselect_isp_mode();

	return nRet;
}

static void mcsdl_read_flash(UINT8 *pBuffer)
{
	int i;

	MCSDL_GPIO_SDA_SET_LOW();

	mcsdl_delay(MCSDL_DELAY_20US);

	for (i = 0; i < 5; i++)
	{
		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_5US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_5US);
	}

	mcsdl_read_32bits(pBuffer);
}

static int mcsdl_read_flash_from(UINT8 *pBuffer, UINT16 unStart_addr,
		UINT16 unLength, INT8 IdxNum)
{
	int i;
	int j;

	UINT8 ucLength;

	UINT16 addr;
	UINT32 wordData;

	if (unLength >= MELFAS_FIRMWARE_MAX_SIZE)
	{
		return MCSDL_RET_PROGRAM_SIZE_IS_WRONG;
	}

	addr = 0;
	ucLength = MELFAS_TRANSFER_LENGTH;

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	printk(" %04X : ", unStart_addr );
#endif

	for (i = 0; i < (int) unLength; i += (int) ucLength)
	{

		addr = (UINT16) i;
		if (IdxNum > 0)
			mcsdl_select_isp_mode(ISP_MODE_NEXT_CHIP_BYPASS);

		mcsdl_select_isp_mode(ISP_MODE_SERIAL_READ);
		wordData = (((unStart_addr + addr) & 0x1FFF) << 1) | 0x0;
		wordData <<= 14;

		mcsdl_write_bits(wordData, 18);

		if ((unLength - addr) < MELFAS_TRANSFER_LENGTH)
		{

			ucLength = (UINT8)(unLength - addr);
		}

		//--------------------
		// Read flash
		//--------------------
		mcsdl_read_flash(&pBuffer[addr]);

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
		for(j=0; j<(int)ucLength; j++)
		{
			printk("%02X ", pBuffer[j] );
		}
#endif

		mcsdl_unselect_isp_mode();

	}

#if MELFAS_ENABLE_DBG_PROGRESS_PRINT
	//printk("\n");
#endif

	return MCSDL_RET_SUCCESS;

}

static void mcsdl_set_ready(void)
{
	//--------------------------------------------
	// Tkey module reset
	//--------------------------------------------

	MCSDL_VDD_SET_LOW(); // power 

	//MCSDL_CE_SET_LOW();
	//MCSDL_CE_SET_OUTPUT();

	//MCSDL_SET_GPIO_I2C();

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT();

	MCSDL_GPIO_SCL_SET_LOW();
	MCSDL_GPIO_SCL_SET_OUTPUT();

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT();

	mcsdl_delay(MCSDL_DELAY_25MS); // Delay for Stable VDD

	MCSDL_VDD_SET_HIGH();
	//MCSDL_CE_SET_HIGH();

	MCSDL_GPIO_SDA_SET_HIGH();

	mcsdl_delay(MCSDL_DELAY_10MS); //mcsdl_delay(MCSDL_DELAY_40MS); // Delay '30 msec'

}

static void mcsdl_reboot_mcs(void)
{
	//--------------------------------------------
	// Tkey module reset
	//--------------------------------------------

	MCSDL_VDD_SET_LOW();

	//MCSDL_CE_SET_LOW();
	//MCSDL_CE_SET_OUTPUT();

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT();

	MCSDL_GPIO_SCL_SET_HIGH();
	MCSDL_GPIO_SCL_SET_OUTPUT();

	//MCSDL_SET_HW_I2C();

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT();

	mcsdl_delay(MCSDL_DELAY_25MS); // Delay for Stable VDD
	

	MCSDL_RESETB_SET_INPUT();
	MCSDL_VDD_SET_HIGH();

	MCSDL_RESETB_SET_HIGH();
	//MCSDL_CE_SET_HIGH();

	mcsdl_delay(MCSDL_DELAY_30MS); // Delay '25 msec'

}

//--------------------------------------------
//
//   Write ISP Mode entering signal
//
//--------------------------------------------

static void mcsdl_select_isp_mode(UINT8 ucMode)
{
	int i;

	UINT8 enteringCodeMassErase[16] =
	{ 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1 };
	UINT8 enteringCodeSerialWrite[16] =
	{ 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1 };
	UINT8 enteringCodeSerialRead[16] =
	{ 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1 };
	UINT8 enteringCodeNextChipBypass[16] =
	{ 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1 };

	UINT8 *pCode;

	//------------------------------------
	// Entering ISP mode : Part 1
	//------------------------------------

	if (ucMode == ISP_MODE_ERASE_FLASH)
		pCode = enteringCodeMassErase;
	else if (ucMode == ISP_MODE_SERIAL_WRITE)
		pCode = enteringCodeSerialWrite;
	else if (ucMode == ISP_MODE_SERIAL_READ)
		pCode = enteringCodeSerialRead;
	else if (ucMode == ISP_MODE_NEXT_CHIP_BYPASS)
		pCode = enteringCodeNextChipBypass;

	for (i = 0; i < 16; i++)
	{

		if (pCode[i] == 1)
			MCSDL_RESETB_SET_HIGH();
		else
			MCSDL_RESETB_SET_LOW();

		//start add delay for INT
		mcsdl_delay(MCSDL_DELAY_3US);
		//end delay for INT

		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}

	MCSDL_RESETB_SET_HIGH(); // High

	//---------------------------------------------------
	// Entering ISP mode : Part 2	- Only Mass Erase
	//---------------------------------------------------

	if (ucMode == ISP_MODE_ERASE_FLASH)
	{
		mcsdl_delay(MCSDL_DELAY_7US);
		for (i = 0; i < 4; i++)
		{

			if (i == 2)
				mdelay(20); // mcsdl_delay(MCSDL_DELAY_25MS);
			else if (i == 3)
				mcsdl_delay(MCSDL_DELAY_150US);

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_1US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_1US);
		}
	}
}

static void mcsdl_unselect_isp_mode(void)
{
	int i;

	// MCSDL_GPIO_SDA_SET_HIGH();
	// MCSDL_GPIO_SDA_SET_OUTPUT();

	MCSDL_RESETB_SET_LOW();
	MCSDL_RESETB_SET_OUTPUT();

	mcsdl_delay(MCSDL_DELAY_3US);

	for (i = 0; i < 10; i++)
	{

		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_3US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_3US);
	}

}

static void mcsdl_read_32bits(UINT8 *pData)
{
	int i, j;

	MCSDL_GPIO_SDA_SET_INPUT();

	for (i = 3; i >= 0; i--)
	{

		pData[i] = 0;

		for (j = 0; j < 8; j++)
		{

			pData[i] <<= 1;

			MCSDL_GPIO_SCL_SET_HIGH();
			mcsdl_delay(MCSDL_DELAY_1US);
			MCSDL_GPIO_SCL_SET_LOW();
			mcsdl_delay(MCSDL_DELAY_1US);

			if (MCSDL_GPIO_SDA_IS_HIGH())
				pData[i] |= 0x01;

		}
	}

}

static void mcsdl_write_bits(UINT32 wordData, int nBits)
{
	int i;

	MCSDL_GPIO_SDA_SET_LOW();
	MCSDL_GPIO_SDA_SET_OUTPUT();

	for (i = 0; i < nBits; i++)
	{

		if (wordData & 0x80000000)
		{
			MCSDL_GPIO_SDA_SET_HIGH();
		}
		else
		{
			MCSDL_GPIO_SDA_SET_LOW();
		}

		mcsdl_delay(MCSDL_DELAY_2US); // lcs_test

		MCSDL_GPIO_SCL_SET_HIGH();
		mcsdl_delay(MCSDL_DELAY_2US);
		MCSDL_GPIO_SCL_SET_LOW();
		mcsdl_delay(MCSDL_DELAY_2US);

		wordData <<= 1;
	}
}

static void mcsdl_scl_toggle_twice(void)
{

	MCSDL_GPIO_SDA_SET_HIGH();
	MCSDL_GPIO_SDA_SET_OUTPUT();

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_20US);

	MCSDL_GPIO_SCL_SET_HIGH();
	mcsdl_delay(MCSDL_DELAY_20US);
	MCSDL_GPIO_SCL_SET_LOW();
	mcsdl_delay(MCSDL_DELAY_20US);
}

//============================================================
//
//	Delay Function
//
//============================================================
static void mcsdl_delay(UINT32 nCount)
{

	switch (nCount)
	{
	case MCSDL_DELAY_1US:
		udelay(1);
		break;
	case MCSDL_DELAY_2US:
		udelay(2);
		break;
	case MCSDL_DELAY_3US:
		udelay(3);
		break;
	case MCSDL_DELAY_5US:
		udelay(5);
		break;
	case MCSDL_DELAY_7US:
		udelay(7);
		break;
	case MCSDL_DELAY_10US:
		udelay(10);
		break;
	case MCSDL_DELAY_15US:
		udelay(15);
		break;
	case MCSDL_DELAY_20US:
		udelay(20);
		break;
	case MCSDL_DELAY_100US:
		udelay(100);
		break;
	case MCSDL_DELAY_150US:
		udelay(150);
		break;
	case MCSDL_DELAY_500US:
		udelay(500);
		break;
	case MCSDL_DELAY_800US:
		udelay(800);
		break;
	case MCSDL_DELAY_1MS:
		msleep(1);
		break;
	case MCSDL_DELAY_5MS:
		msleep(5);
		break;
	case MCSDL_DELAY_10MS:
		msleep(10);
		break;
	case MCSDL_DELAY_25MS:
		msleep(25);
		break;
	case MCSDL_DELAY_30MS:
		msleep(30);
		break;
	case MCSDL_DELAY_40MS:
		msleep(40);
		break;
	case MCSDL_DELAY_45MS:
		msleep(45);
		break;
		//start ADD DELAY
	case MCSDL_DELAY_300US:
		udelay(300);
		break;
	case MCSDL_DELAY_60MS:
		msleep(60);
		break;
	case MCSDL_DELAY_40US:
		udelay(40);
		break;
		//end del
	default:
		printk(KERN_DEBUG "(%s) Undefined delay. \n", __func__);
		break;
	}// Please, Use your delay function


}

//============================================================
//
//	Debugging print functions.
//
//============================================================

#ifdef MELFAS_ENABLE_DBG_PRINT

static void mcsdl_print_result(int nRet)
{
	if( nRet == MCSDL_RET_SUCCESS )
	{

		printk(" > MELFAS Firmware downloading SUCCESS.\n");

	}
	else
	{

		printk(" > MELFAS Firmware downloading FAILED  :  ");

		switch( nRet )
		{

			case MCSDL_RET_SUCCESS : printk("MCSDL_RET_SUCCESS\n" ); break;
			case MCSDL_RET_ERASE_FLASH_VERIFY_FAILED : printk("MCSDL_RET_ERASE_FLASH_VERIFY_FAILED\n" ); break;
			case MCSDL_RET_PROGRAM_VERIFY_FAILED : printk("MCSDL_RET_PROGRAM_VERIFY_FAILED\n" ); break;

			case MCSDL_RET_PROGRAM_SIZE_IS_WRONG : printk("MCSDL_RET_PROGRAM_SIZE_IS_WRONG\n" ); break;
			case MCSDL_RET_VERIFY_SIZE_IS_WRONG : printk("MCSDL_RET_VERIFY_SIZE_IS_WRONG\n" ); break;
			case MCSDL_RET_WRONG_BINARY : printk("MCSDL_RET_WRONG_BINARY\n" ); break;

			case MCSDL_RET_READING_HEXFILE_FAILED : printk("MCSDL_RET_READING_HEXFILE_FAILED\n" ); break;
			case MCSDL_RET_FILE_ACCESS_FAILED : printk("MCSDL_RET_FILE_ACCESS_FAILED\n" ); break;
			case MCSDL_RET_MELLOC_FAILED : printk("MCSDL_RET_MELLOC_FAILED\n" ); break;

			case MCSDL_RET_WRONG_MODULE_REVISION : printk("MCSDL_RET_WRONG_MODULE_REVISION\n" ); break;

			default : printk("UNKNOWN ERROR. [0x%02X].\n", nRet ); break;
		}

		printk("\n");
	}

}

#endif

#if MELFAS_ENABLE_DELAY_TEST

//============================================================
//
//	For initial testing of delay and gpio control
//
//	You can confirm GPIO control and delay time by calling this function.
//
//============================================================

void mcsdl_delay_test(INT32 nCount)
{
	INT16 i;

	MELFAS_DISABLE_BASEBAND_ISR(); // Disable Baseband touch interrupt ISR.
	MELFAS_DISABLE_WATCHDOG_TIMER_RESET(); // Disable Baseband watchdog timer

	//--------------------------------
	//	Repeating 'nCount' times
	//--------------------------------

	MCSDL_SET_GPIO_I2C();
	MCSDL_GPIO_SCL_SET_OUTPUT();
	MCSDL_GPIO_SDA_SET_OUTPUT();
	MCSDL_RESETB_SET_OUTPUT();

	MCSDL_GPIO_SCL_SET_HIGH();

	for( i=0; i<nCount; i++ )
	{

#if 1

		MCSDL_GPIO_SCL_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_20US);

		MCSDL_GPIO_SCL_SET_HIGH();

		mcsdl_delay(MCSDL_DELAY_100US);

#elif 0

		MCSDL_GPIO_SCL_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_500US);

		MCSDL_GPIO_SCL_SET_HIGH();

		mcsdl_delay(MCSDL_DELAY_1MS);

#else

		MCSDL_GPIO_SCL_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_25MS);

		TKEY_INTR_SET_LOW();

		mcsdl_delay(MCSDL_DELAY_45MS);

		TKEY_INTR_SET_HIGH();

#endif
	}

	MCSDL_GPIO_SCL_SET_HIGH();

	MELFAS_ROLLBACK_BASEBAND_ISR(); // Roll-back Baseband touch interrupt ISR.
	MELFAS_ROLLBACK_WATCHDOG_TIMER_RESET(); // Roll-back Baseband watchdog timer
}

#endif

