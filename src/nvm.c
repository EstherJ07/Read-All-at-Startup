#include "nvm.h"
static lpi2c_master_state_t masterState;
void NvM_Init(void){
    CLOCK_DRV_Init(&clockMan1_InitConfig0);
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS0,g_pin_mux_InitConfigArr0);
    LPUART_DRV_Init(INST_LPUART_1,&lpUartState0,&lpuart_0_InitConfig0);
    LPI2C_DRV_MasterInit(LPI2C_INSTANCE,&lpi2c0_MasterConfig0,&masterState);
}

static uint16_t Failed_Blocks[NVM_BLOCK_COUNT];
static uint8_t  Failed_BlockCount = 0U;
static uint8_t RAM_Mirror1[BLOCK_LENGTH];
static uint8_t RAM_Mirror2[BLOCK_LENGTH];
static uint8_t RAM_Mirror3[BLOCK_LENGTH];

typedef struct{
    uint16_t Block_ID;
    uint16_t EEPROM_Addr;
    uint8_t *RAM_Mirror;
    uint16_t length;
}NvM_BlockType;

static const NvM_BlockType NVM_Blocks[NVM_BLOCK_COUNT]=
{
    {1U,BLOCK1_ADDRESS,RAM_Mirror1,BLOCK_LENGTH},
    {2U,BLOCK2_ADDRESS,RAM_Mirror2,BLOCK_LENGTH},
    {3U,BLOCK3_ADDRESS,RAM_Mirror3,BLOCK_LENGTH}
};
static const uint8_t Initial_Data[NVM_BLOCK_COUNT][BLOCK_LENGTH]=
{
    {0x08U,0x05U,0x06U},
    {0x20U,0x06U,0x07U},
    {0x01U,0x01U,0x07U}
};
static uint8_t CalculateCRC(const uint8_t *data, uint16_t length){
    uint8_t crc=0xFFU;
    for (uint16_t i=0U;i<length;i++){
        crc^=data[i];
        for(uint8_t bit=0U;bit<8U;bit++){
            if((crc&0x80U)!=0U){
                crc=(uint8_t)((crc<<1U)^0x07U);
            }
            else{
                crc=(uint8_t)(crc<<1U);
            }
        }
    }
    return crc;
}

static void UART_Print(const char *message){
    uint32_t length=0U;
    while(message[length]!='\0'){
        length++;
    }
    if (length>0U){
        (void)LPUART_DRV_SendDataBlocking(INST_LPUART_1,(const uint8_t*)message,length,UART_TIMEOUT);
    }
}
static status_t EEPROM_Read(uint16_t address,uint8_t *data,uint16_t length){
    status_t status;
    uint8_t memoryAddress;
    if((data==NULL)||(length==0U)||(address>=EEPROM_CAPACITY)||((uint32_t)address+length>EEPROM_CAPACITY))
    {
        return STATUS_ERROR;
    }
    memoryAddress=(uint8_t)(address&0xFFU);
    LPI2C_DRV_MasterSetSlaveAddr(LPI2C_INSTANCE,EEPROM_I2C_ADDRESS,false);
    status=LPI2C_DRV_MasterSendDataBlocking(LPI2C_INSTANCE,&memoryAddress,1U,false,I2C_TIMEOUT);
    if(status!=STATUS_SUCCESS){
        return status;
    }
    return LPI2C_DRV_MasterReceiveDataBlocking(LPI2C_INSTANCE,data,length,true,I2C_TIMEOUT);
}
static status_t EEPROM_WriteRecord(uint16_t address,const uint8_t *data,uint16_t length){
    status_t status;
    uint8_t txBuffer[1U+RECORD_SIZE];
    uint8_t memoryAddress;
    if((data==NULL)||(length!=RECORD_SIZE)||(address>=EEPROM_CAPACITY)||((uint32_t)address+length>EEPROM_CAPACITY)){
        return STATUS_ERROR;
    }
    if(((address%EEPROM_PAGE_SIZE)+length)>EEPROM_PAGE_SIZE){
        return STATUS_ERROR;
    }
    memoryAddress=(uint8_t)(address & 0xFFU);
    txBuffer[0]=memoryAddress;
    for(uint16_t j=0U;j<length;j++)
    {
        txBuffer[1U+j]=data[j];
    }
    LPI2C_DRV_MasterSetSlaveAddr(LPI2C_INSTANCE,EEPROM_I2C_ADDRESS,false);
    status=LPI2C_DRV_MasterSendDataBlocking(LPI2C_INSTANCE,txBuffer,(uint32_t)(1U+length),true,I2C_TIMEOUT);
    if(status!=STATUS_SUCCESS){
        return status;
    }
    OSIF_TimeDelay(6U);
    return STATUS_SUCCESS;
}
static void NvM_WriteAll(void)
{
    uint8_t record[RECORD_SIZE];
    uint8_t crc;
    status_t status;
    char txBuff[148];
    int len;
    UART_Print("\r\nStarting EEPROM initialization\r\n");
    for(uint8_t i=0U;i<NVM_BLOCK_COUNT;i++)
    {
    	for(uint8_t j=0U;j<BLOCK_LENGTH;j++){
    	    record[j]=Initial_Data[i][j];
    	}
        crc=CalculateCRC(record, BLOCK_LENGTH);
        record[BLOCK_LENGTH] = crc;
        if (i == 1U){ record[0] ^= 0xFFU; } 
        len=sprintf(txBuff,"Writing block ID: %u\r\n",(unsigned int)NVM_Blocks[i].Block_ID);
        if(len>0){
            UART_Print(txBuff);
        }
        status=EEPROM_WriteRecord(NVM_Blocks[i].EEPROM_Addr,record,RECORD_SIZE);
        if(status!=STATUS_SUCCESS){
            UART_Print("EEPROM WRITE FAILED\r\n");
        }
        else
        {
            UART_Print("EEPROM WRITE COMMAND OK\r\n");
        }
    }
    UART_Print("EEPROM initialization finished\r\n");
}
static void NvM_ReadAll(void)
{
    uint8_t record[RECORD_SIZE];
    status_t status;
    uint8_t storedCRC;
    uint8_t calculatedCRC;
    char txBuff[48];
    int len;
    Failed_BlockCount=0U;
    UART_Print("\r\nStarting NvM ReadAll\r\n");
    for(uint8_t i=0U;i<NVM_BLOCK_COUNT;i++)
    {
    	for(uint16_t j=0U;j<NVM_Blocks[i].length;j++)
    	{
    	    NVM_Blocks[i].RAM_Mirror[j]=0U;
    	}
    	for(uint8_t j=0U;j<RECORD_SIZE;j++)
    	{
    	    record[j]=0U;
    	}
        len=sprintf(txBuff,"\r\nReading block ID: %u\r\n",(unsigned int)NVM_Blocks[i].Block_ID);
        if(len>0){
            UART_Print(txBuff);
        }
        status=EEPROM_Read(NVM_Blocks[i].EEPROM_Addr,record,RECORD_SIZE);
        if(status!=STATUS_SUCCESS)
        {
            Failed_Blocks[Failed_BlockCount++]=NVM_Blocks[i].Block_ID;
            UART_Print("I2C READ FAILED\r\n");
            continue;
        }
        len=sprintf(txBuff,"Record: %02X %02X %02X %02X\r\n",record[0],record[1],record[2],record[3]);
        if(len>0){
            UART_Print(txBuff);
        }
        storedCRC=record[BLOCK_LENGTH];
        calculatedCRC=CalculateCRC(record, BLOCK_LENGTH);
        len=sprintf(txBuff, "Stored CRC: %02X\r\n",(unsigned int)storedCRC);
        if(len>0){
            UART_Print(txBuff);
        }
        len=sprintf(txBuff,"Calculated CRC: %02X\r\n",(unsigned int)calculatedCRC);
        if(len>0){
            UART_Print(txBuff);
        }
        if(storedCRC==calculatedCRC){
        	for(uint8_t j=0U;j<BLOCK_LENGTH;j++)
        	{
        	    NVM_Blocks[i].RAM_Mirror[j]=record[j];
        	}
            UART_Print("CRC OK - BLOCK VALID\r\n");
        }
        else
        {
            Failed_Blocks[Failed_BlockCount++]=NVM_Blocks[i].Block_ID;
            UART_Print("CRC MISMATCH\r\n");
        }
    }
    UART_Print("\r\nNvM ReadAll finished\r\n");
}
static void NvM_PrintResults(void){
    char txBuff[48];
    int len;
    len=sprintf(txBuff,"\r\nFailed block count: %u\r\n",(unsigned int)Failed_BlockCount);
    if(len>0){
        UART_Print(txBuff);
    }
    for(uint8_t i=0U;i<Failed_BlockCount;i++){
    	len=sprintf(txBuff,"Failed block ID: %u\r\n",(unsigned int)Failed_Blocks[i]);
    	if((len > 0)&&((size_t)len<sizeof(txBuff))){
            UART_Print(txBuff);
        }
    }
    if(Failed_BlockCount==0U){
        UART_Print("ALL BLOCKS VALID\r\n");
    }
}
void NvM_Run(void){
    if(WRITE_INITIAL_DATA==1U){
        NvM_WriteAll();
    }
    else
    {
        UART_Print("Write disabled; preserving EEPROM data\r\n");
    }
    NvM_ReadAll();
    NvM_PrintResults();
}
