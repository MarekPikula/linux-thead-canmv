/******************************************************************************

   Copyright (C) 2011-2014 ZheJiang Dahua Technology CO.,LTD.

 ******************************************************************************
  File Name     : I2C_drv.c
  Version       : Initial Draft
  Author        : Zxj
  Created       : 2014.5.5
  Last Modified :
  Description   : I2C register arch driver
  Function List :
  DHstory       :
  1.Date        : 2014/5/5
    Author      : 11853
    Modification: Create
******************************************************************************/
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/reset.h>


#include "./include/dh_define.h"
#include "i2c_hal.h"
#include "i2c_reg.h"

#include "./include/dh_errno.h"
#include "i2c_drv.h"

#define DHC_I2C_TIMEOUT     (0xFFFF)


/* ����IIC�ٶ����� */
static DH_U32 gu32IicSpdMode;
static long gu32IicBaseAddr[DHC_MAX_I2C_DEV];


DHC_I2C_GROUP_STATUS_E gEgroupStatus[DHC_MAX_I2C_DEV];
struct mutex    lock[DHC_MAX_I2C_DEV];
DHC_I2C_GROUP_CMD_S gSgrouopCmd[DHC_MAX_I2C_DEV];


DH_S32 I2C_DRV_EnableI2c(DH_VOID *base,DH_U32 enabel)
{
    I2C_HAL_SetReg( enabel,base,I2C_REG_ENABLE);
    return 0;
}


DH_S32 I2C_DRV_InitInterrupts(DH_VOID *base)
{
    DH_U32 intr = 0;

    /*���ж�״̬*/
    I2C_HAL_GetReg(base,I2C_REG_CLR_INTR);
    intr |= (I2C_REG_BIT_INT_FREE | I2C_REG_BIT_INT_TX_ABRT);
    I2C_HAL_SetReg( intr,base,I2C_REG_INTR_MASK);
    return 0;
}

DH_S32 I2C_DRV_ConfigClk(DH_VOID *base, DH_U32 u32BusClk, DH_U32 u32Clk)
{
    DH_U16 lu16hcnt,lu16lcnt;
    DH_U32 lu32SpdMode,lu32Tmp;

	/* ȷ���ٶ����ڷ�Χ */
	if (u32Clk > DHC_I2C_SPD_FAST)
    {
        lu32SpdMode = DHC_I2C_SPDMODE_HIGH;
    }
    else if (u32Clk > DHC_I2C_SPD_STANDARD)
    {
        lu32SpdMode = DHC_I2C_SPDMODE_FAST;
    }
    else
    {
        lu32SpdMode = DHC_I2C_SPDMODE_STD;
    }

    if (gu32IicSpdMode != lu32SpdMode)
    {
        lu32Tmp = I2C_HAL_GetReg(base, I2C_REG_CON);
        lu32Tmp &= I2C_REG_BITMASK_SPDMODE;
        lu32Tmp |= (lu32SpdMode << I2C_REG_BITSHF_SPDMODE);
        I2C_HAL_SetReg(lu32Tmp, base, I2C_REG_CON);

        gu32IicSpdMode = lu32SpdMode;
    }

    /* ����APB����Ƶ�ʼ��� */
    lu16hcnt = (u32BusClk / u32Clk) >> 1;
    lu16lcnt = (u32BusClk / u32Clk) - lu16hcnt;

    if (lu32SpdMode == DHC_I2C_SPDMODE_HIGH)
    {
        I2C_HAL_SetReg( lu16hcnt - 8,base,I2C_REG_HS_SCL_HCNT);
        I2C_HAL_SetReg( lu16lcnt - 1,base,I2C_REG_HS_SCL_LCNT);
    }
    else if (lu32SpdMode == DHC_I2C_SPDMODE_FAST)
    {
        I2C_HAL_SetReg( lu16hcnt - 8,base,I2C_REG_FS_SCL_HCNT);
        I2C_HAL_SetReg( lu16lcnt - 1,base,I2C_REG_FS_SCL_LCNT);
    }
    else
    {
        I2C_HAL_SetReg( lu16hcnt - 8,base,I2C_REG_SS_SCL_HCNT);
        I2C_HAL_SetReg( lu16lcnt - 1,base,I2C_REG_SS_SCL_LCNT);
    }
    return 0;
}


DH_S32 I2C_DRV_SetConfig(DH_VOID *base)
{
    DH_U32 lu32Cfg;

    lu32Cfg = I2C_REG_BIT_MASTER_MODE | I2C_REG_BIT_SPDMD_FAST | (1 << 6) \
        | I2C_REG_BIT_RESTART_EN;
    I2C_HAL_SetReg(lu32Cfg, base, I2C_REG_CON);

    return DH_SUCCESS;
}


DH_S32 I2C_DRV_SetSlaveAddr(DH_VOID *base,DH_U32 addr)
{
    I2C_HAL_SetReg( addr,base,I2C_REG_TAR);
    return 0;
}

DH_S32 I2C_DRV_WaitTransmitOver(DH_VOID *base,DH_U32 fifoFlag)
{
	int i;
	DH_U32 lu32Tmp;

	for (i = 0; i < DHC_I2C_TIMEOUT; i++)
	{
	    lu32Tmp = I2C_HAL_GetReg(base,I2C_REG_STATUS);
	    if ( I2C_REG_BIT_ACTIVITY != (lu32Tmp & I2C_REG_BITMASK_ACTIVITY)
            && (lu32Tmp & fifoFlag ))
		{
			return DH_SUCCESS;
		}
	}

    return DH_FAILURE;

}

DH_S32  I2C_DRV_SetStart(DH_VOID *base,DH_U32 u32Enable)
{
    I2C_HAL_SetReg(u32Enable, base, I2C_REG_START);

    return DH_SUCCESS;
}


DH_U32 I2C_DRV_GetIntStatus(DH_VOID *base)
{
    return I2C_HAL_GetReg(base,I2C_REG_INTR_STAT);
}

DH_U32 I2C_DRV_GetRecFifoCnt(DH_VOID *base)
{
    return I2C_HAL_GetReg(base,I2C_REG_RXFLR);
}


DH_VOID I2C_DRV_IrqHandle(DH_VOID *dev_id)
{
	DH_U32 status;
    DHC_I2C_GROUP_STATUS_E groupStatus;
    struct dhc_i2c_dev *dev = dev_id;
    groupStatus = I2C_DRV_GetGroupSendStatus(dev->adapter.nr);
    status = I2C_DRV_GetIntStatus(dev->base);

    if(groupStatus == STATUS_HANDLING)
    {
        if(status & I2C_REG_BIT_INT_TX_ABRT)
        {
            I2C_DRV_SetGroupSendStatus(dev->adapter.nr,STATUS_ERROR);
            I2C_DRV_SetStart(dev->base,I2C_STOP);
            I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
            return;
        }

        /*��������û�з�����*/
        if(gSgrouopCmd[dev->adapter.nr].cmdCnt > 0)
        {
           // printk("int cmdcnt=%d\n",gSgrouopCmd[dev->adapter.nr].cmdCnt);
            I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
            /*���ж�״̬����ʹ���ж�*/
            I2C_DRV_InitInterrupts(dev->base);
            I2C_DRV_SetSlaveAddr(dev->base,gSgrouopCmd[dev->adapter.nr].devid >> 1);
            I2C_DRV_EnableI2c(dev->base,I2C_ENABLES);
            #ifndef I2C_HARD_GROUP_SEND
            I2C_DRV_WriteFifo( dev->adapter.nr ,0);
            #else
            I2C_DRV_WriteFifo( dev->adapter.nr ,1);
            #endif
            I2C_DRV_SetStart(dev->base,I2C_START);
        }
        else /*����ȫ���������*/
        {
            //printk("int comlete\n");
            I2C_DRV_SetGroupSendStatus(dev->adapter.nr,STATUS_DONE);
            I2C_DRV_SetStart(dev->base,I2C_STOP);
            I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
        }

    }
    else
    {
        if(status & I2C_REG_BIT_INT_TX_ABRT)
        {
            dev->abort = DH_TRUE;
        }
        dev->complete = DH_TRUE;
        I2C_DRV_InitInterrupts(dev->base);
        wake_up(&dev->wait);
    }

}
/*дi2cʱֻ��һ��msgs*/
DH_S32 I2C_DRV_SendMsgs(struct dhc_i2c_dev *dev, struct i2c_msg *msgs, DH_S32 num )
{
    DH_U32 lu32TimeOut,i;
    DH_S32 ls32Ret = num;
    DH_U16 *lpu16CmdDatPtr = dev->cmdBuf;

    if(num != 1 || msgs->len > I2C_BUF_DEP)
    {
        printk("i2c transmite err,must one msgs or msg length must less than %d!\n",I2C_BUF_DEP);
        return -1;
    }

    for(i = 0; i < msgs->len; i++)
    {
        lpu16CmdDatPtr[i] = msgs->buf[i];
    }
    dev->complete = DH_FALSE;
    dev->abort    = DH_FALSE;
    I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
    I2C_DRV_InitInterrupts(dev->base);
    //I2C_HAL_GetReg(dev->base,I2C_REG_CLR_INTR);
    I2C_DRV_SetSlaveAddr(dev->base,msgs->addr);
    I2C_DRV_EnableI2c(dev->base,I2C_ENABLES);

    for(i = 0; i < msgs->len; i++ )
    {
        I2C_HAL_SetReg( lpu16CmdDatPtr[i],dev->base,I2C_REG_DATA_CMD);
    }

    I2C_DRV_SetStart(dev->base,I2C_START);

    lu32TimeOut = wait_event_timeout(dev->wait, dev->complete == DH_TRUE, HZ * 3);

	if(0 == lu32TimeOut || I2C_DRV_WaitTransmitOver(dev->base,I2C_REG_BIT_TFE))
	{
		printk("i2c send timeout!\n");
        ls32Ret = 0;
	}
    else if(dev->abort == DH_TRUE)
    {
        printk("i2c send abort!\n");
        ls32Ret = 0;
    }
    else if(dev->complete != DH_TRUE)
    {
        printk("i2c send error!\n");
        ls32Ret = 0;
    }
    I2C_DRV_SetStart(dev->base,I2C_STOP);
    I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
    return ls32Ret;
}

/*��i2cʱֻ������msg*/
DH_S32 I2C_DRV_ReceiveMsgs(struct dhc_i2c_dev *dev, struct i2c_msg *msgs, DH_S32 num)
{
    DH_U32 lu32MaxRdCnt,lu32TimeOut,i,j;
    DH_S32 ls32Ret = num;
    DH_U16 *lpu16CmdDatPtr;
    struct i2c_msg *lpstRdMsg,*lpstWrMsg;
    lpstWrMsg = msgs;
    lpstRdMsg = msgs + 1;
    lpu16CmdDatPtr = dev->cmdBuf;
    lu32MaxRdCnt = lpstRdMsg->len + lpstWrMsg->len;

    if (lu32MaxRdCnt > DHC_I2C_CMD_BUF_DEP)
    {
        printk("i2c read cnt %d,and write cnt %d,bigger than fifo dep %d\n",
                lpstRdMsg->len, lpstWrMsg->len, DHC_I2C_CMD_BUF_DEP);
        return DH_FAILURE;
    }

     /* ����д���� */
    for (i = 0; i < lpstWrMsg->len; i++)
    {
        /* Bit8 Ϊ0��ʶд����*/
        lpu16CmdDatPtr[i] = lpstWrMsg->buf[i];
        lpu16CmdDatPtr[i] &= ~(I2C_REG_BIT_CDM_READ);
    }


      /* ���������� */
    for( j = 0; j < lpstRdMsg->len; j++, i++ )
    {
        lpu16CmdDatPtr[i] = 0;
        lpu16CmdDatPtr[i] |= I2C_REG_BIT_CDM_READ;
    }

    dev->complete = DH_FALSE;
    dev->abort = DH_FALSE;

    I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
    I2C_DRV_InitInterrupts(dev->base);
    //I2C_HAL_GetReg(dev->base,I2C_REG_CLR_INTR);
    I2C_DRV_SetSlaveAddr(dev->base,msgs->addr);
    I2C_DRV_EnableI2c(dev->base,I2C_ENABLES);

    for(j = 0; j < i; j++ )
    {
        I2C_HAL_SetReg( lpu16CmdDatPtr[j],dev->base,I2C_REG_DATA_CMD);
    }


    I2C_DRV_SetStart(dev->base,I2C_START);

    lu32TimeOut = wait_event_timeout(dev->wait, dev->complete == DH_TRUE, HZ * 3);

    if(0 == lu32TimeOut || I2C_DRV_WaitTransmitOver(dev->base,I2C_REG_BIT_RFNE))
	{
		printk("i2c receive timeout!\n");
        ls32Ret = 0;
	}
    else if(dev->abort == DH_TRUE)
    {
        printk("i2c receive abort!\n");
        ls32Ret = 0;
    }
    else if((dev->complete != DH_TRUE))
    {
        printk("i2c receive error!\n");
        ls32Ret = 0;
    }

    lu32MaxRdCnt = I2C_DRV_GetRecFifoCnt(dev->base);

    /* �����Ƿ�����ȷ���������FIFO�����ݣ������� */
    for(i = 0; i < lu32MaxRdCnt; i++)
    {
        lpstRdMsg->buf[i] = I2C_HAL_GetReg(dev->base,I2C_REG_DATA_CMD);
    }

    I2C_DRV_SetStart(dev->base,I2C_STOP);
    I2C_DRV_EnableI2c(dev->base,I2C_DISABLES);
    return ls32Ret;


}


DH_S32 I2C_DRV_Xfer(struct dhc_i2c_dev *dev, struct i2c_msg *msgs, int num)
{
    DH_S32 ls32Ret;
    if(num > 1 && !(msgs->flags & I2C_M_RD))
    {
        ls32Ret = I2C_DRV_ReceiveMsgs(dev, msgs,  num);
    }
    else
    {
        ls32Ret = I2C_DRV_SendMsgs(dev, msgs,  num);
    }
	return ls32Ret;
}

DH_S32 I2C_DRV_Write8bit(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
                          DH_U32 u32Value )
{
    DH_S32 ls32Ret = 0,i;
    DH_VOID *lpRegBase = NULL;
    DH_U16 lu16CmdDat[2];
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }

    lu16CmdDat[0] = u32RegAddr & 0xff;
    lu16CmdDat[1] = u32Value & 0xff;
    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }

    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
    /*���ж�״̬*/
    I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
    I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

    I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
    I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

    for(i = 0; i < 2; i++)
    {
        I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_START);
    if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_TFE))
	{
		printk("i2c xfer timeout!\n");
        ls32Ret = -1;
	}
    I2C_DRV_SetStart(lpRegBase,I2C_STOP);
    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

    return ls32Ret;
}


DH_S32 I2C_DRV_Read8bit(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
                       DH_U8 *buf)
{
    DH_S32 ls32Ret = 0,i;
    DH_VOID *lpRegBase = NULL;
    DH_U32 lu32RdCnt;
    DH_U16 lu16CmdDat[2];
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }

    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }

    lu16CmdDat[0] = u32RegAddr & 0xff;
    lu16CmdDat[1] = 1 << 8;

    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
    /*���ж�״̬*/
    I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
    I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

    I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
    I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

    for(i = 0; i < 2; i++)
    {
        I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_START);
    if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_RFNE))
	{
		printk("i2c xfer timeout!\n");
        ls32Ret = -1;
	}
    lu32RdCnt = I2C_DRV_GetRecFifoCnt(lpRegBase);
    if(lu32RdCnt != 1)
    {
        printk("i2c transmit error\n");
        ls32Ret = -1;
    }
    else
    {
        *buf = I2C_HAL_GetReg(lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_STOP);
    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

    return ls32Ret;

}


DH_S32 I2C_DRV_Write16bit(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
                          DH_U32 u32Value )
{
    DH_S32 ls32Ret = 0,i;
    DH_VOID *lpRegBase = NULL;
    DH_U16 lu16CmdDat[4];
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }


    lu16CmdDat[0] = (u32RegAddr >> 8) & 0xff;
    lu16CmdDat[1] = u32RegAddr & 0xff;
    lu16CmdDat[2] = (u32Value >> 8) & 0xff;
    lu16CmdDat[3] = u32Value & 0xff;

    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }


    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
    /*���ж�״̬*/
    I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
    I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

    I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
    I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

    for(i = 0; i < 4; i++)
    {
        I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_START);
    if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_TFE))
	{
		printk("i2c xfer timeout!\n");
        ls32Ret = -1;
	}
    I2C_DRV_SetStart(lpRegBase,I2C_STOP);
    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

    return ls32Ret;
}


DH_S32 I2C_DRV_Read16bit(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
                       DH_U16 *buf)
{
    DH_S32 ls32Ret = 0,i;
    DH_VOID *lpRegBase;
    DH_U32 lu32RdCnt;
    DH_U16 lu16CmdDat[4];
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }


    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }

    lu16CmdDat[0] = (u32RegAddr >> 8) & 0xff;
    lu16CmdDat[1] =  u32RegAddr & 0xff;
    lu16CmdDat[2] = 1 << 8;
    lu16CmdDat[3] = 1 << 8;

    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
    /*���ж�״̬*/
    I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
    I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

    I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
    I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

    for(i = 0; i < 4; i++)
    {
        I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_START);
    if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_RFNE))
	{
		printk("i2c xfer timeout!\n");
        ls32Ret = -1;
	}
    lu32RdCnt = I2C_DRV_GetRecFifoCnt(lpRegBase);
    if(lu32RdCnt != 2)
    {
        printk("i2c transmit error\n");
        ls32Ret = -1;
    }
    else
    {
        *buf = I2C_HAL_GetReg(lpRegBase,I2C_REG_DATA_CMD);
        *buf <<= 8;
        *buf |= I2C_HAL_GetReg(lpRegBase,I2C_REG_DATA_CMD);
    }
    I2C_DRV_SetStart(lpRegBase,I2C_STOP);
    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

    return ls32Ret;

}

DH_S32 I2C_DRV_Write(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
					    DH_U32 u32RegByte, DH_U32 u32Value,   DH_U32 u32ValByte)
{
	DH_S32 ls32Ret = 0,i,j,bytes;
	DH_VOID *lpRegBase = NULL;
	DH_U16 lu16CmdDat[8];
	if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
	{
	    printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
	    return -1;
	}
	bytes = u32RegByte + u32ValByte;
	if(bytes > 8)
	{
		printk("i2c write bytes %d big than max 8 bytes\n",bytes);
		return -1;
	}
	for(i = 0; i < u32RegByte; i++)
	{
		lu16CmdDat[i] = (u32RegAddr >> ((u32RegByte - 1 - i)*8)) & 0xff;
	}
	for(j = 0; j < u32ValByte; j++,i++)
	{
		lu16CmdDat[i] = (u32Value >> ((u32ValByte - 1 - j)*8)) & 0xff;
	}


    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }


	I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
	/*���ж�״̬*/
	I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
	I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

	I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
	I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

	for(i = 0; i < bytes; i++)
	{
	    I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
	}
	I2C_DRV_SetStart(lpRegBase,I2C_START);
	if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_TFE))
	{
		printk("i2c xfer timeout!\n");
	    ls32Ret = -1;
	}
	I2C_DRV_SetStart(lpRegBase,I2C_STOP);
	I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

	return ls32Ret;
}

DH_S32 I2C_DRV_Read(DH_U32 u32CtlIndex,DH_U32 u32DevAddr, DH_U32 u32RegAddr,
						   DH_U32 u32RegByte, DH_U32 *buf,  DH_U32 u32ValByte)
{
	DH_S32 ls32Ret = 0,i,j,bytes;
	DH_VOID *lpRegBase = NULL;
	DH_U32 lu32RdCnt;
	DH_U8 data;
	DH_U16 lu16CmdDat[8];
	if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
	{
	    printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
	    return -1;
	}

	bytes = u32RegByte + u32ValByte;
	if(bytes > 8)
	{
		printk("i2c write bytes %d big than max 8 bytes\n",bytes);
		return -1;
	}

    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }

	for(i = 0; i < u32RegByte; i++)
	{
		lu16CmdDat[i] = (u32RegAddr >> ((u32RegByte - 1 - i)*8)) & 0xff;
	}

	for(j = 0; j < u32ValByte; j++,i++)
	{
		lu16CmdDat[i] = 1 << 8;
	}

	I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
	/*���ж�״̬*/
	I2C_HAL_GetReg(lpRegBase,I2C_REG_CLR_INTR);
	I2C_HAL_SetReg( 0,lpRegBase,I2C_REG_INTR_MASK);

	I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
	I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

	for(i = 0; i < bytes; i++)
	{
	    I2C_HAL_SetReg( lu16CmdDat[i] ,lpRegBase,I2C_REG_DATA_CMD);
	}
	I2C_DRV_SetStart(lpRegBase,I2C_START);
	if(I2C_DRV_WaitTransmitOver(lpRegBase,I2C_REG_BIT_RFNE))
	{
		printk("i2c xfer timeout!\n");
	    ls32Ret = -1;
	}
	lu32RdCnt = I2C_DRV_GetRecFifoCnt(lpRegBase);
	if(lu32RdCnt != u32ValByte)
	{
	    printk("i2c transmit error,readCnt=%d,fifoCnt=%d\n",u32ValByte,lu32RdCnt);
	    ls32Ret = -1;
	}
	else
	{
		*buf = 0;
		for(i = 0;i < u32ValByte; i++)
		{
			data = I2C_HAL_GetReg(lpRegBase,I2C_REG_DATA_CMD);
			*buf |= data << (u32ValByte - 1 - i)*8;
		}
	}
	I2C_DRV_SetStart(lpRegBase,I2C_STOP);
	I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);

	return ls32Ret;

}


 /*����fifo�������ż�����������16λ��ַ����16λ���ݱ��ֳ�����д*/

DH_S32 I2C_DRV_GroupSend(DH_U32 u32CtlIndex,DH_U32 u32DevAddr,
                       DH_U16 *buf,DH_U32 cnt)
{
    DH_S32 i=0;
    DH_VOID *lpRegBase = NULL;
    DHC_I2C_GROUP_STATUS_E groupStatus;
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }

    if(cnt > I2C_GROUP_CMD_CNT)
    {
        printk("i2c send group %d data > iic cmd buf %d\n",cnt,I2C_GROUP_CMD_CNT);
        return -1;
    }


    /*�жϵ�ַ��ʽ�Ƿ����Ҫ��*/
    if((buf[0] & (1 << 9)) == 0)
    {
        printk("cmd buf first must be addr\n");
        return -1;
    }

    gSgrouopCmd[u32CtlIndex].devid = u32DevAddr;
    gSgrouopCmd[u32CtlIndex].cmdCnt = cnt;
    gSgrouopCmd[u32CtlIndex].cmdPos = 0;
    gSgrouopCmd[u32CtlIndex].nofifo = 0;
    //printk("data:\n");
    for(i = 0; i < cnt; i++)
    {
       // printk("%d,0x%x,\n",i,buf[i]);
        gSgrouopCmd[u32CtlIndex].cmdBuf[i] = buf[i];
    }
//printk("\n");
    groupStatus = I2C_DRV_GetGroupSendStatus(u32CtlIndex);
    if(groupStatus != STATUS_IDLE)
    {
        printk("i2c group send is handling,status = %d\n",groupStatus);
        return -1;
    }

    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }


    I2C_DRV_SetGroupSendStatus( u32CtlIndex, STATUS_HANDLING );

    I2C_DRV_EnableI2c(lpRegBase,I2C_DISABLES);
    /*���ж�״̬����ʹ���ж�*/
    I2C_DRV_InitInterrupts(lpRegBase);
    I2C_DRV_SetSlaveAddr(lpRegBase,u32DevAddr >> 1);
    I2C_DRV_EnableI2c(lpRegBase,I2C_ENABLES);

    #ifndef I2C_HARD_GROUP_SEND
    I2C_DRV_WriteFifo( u32CtlIndex ,0);
    #else
    I2C_DRV_WriteFifo( u32CtlIndex ,1);
    #endif

    I2C_DRV_SetStart(lpRegBase,I2C_START);

    return 0;
}

DH_S32 I2C_DRV_InitCmdBuf(DH_U32 u32CtlIndex)
{
    gSgrouopCmd[u32CtlIndex].devid = 0;
    gSgrouopCmd[u32CtlIndex].regAddr = 0;
    gSgrouopCmd[u32CtlIndex].cmdCnt = 0;
    gSgrouopCmd[u32CtlIndex].cmdPos = 0;
    gSgrouopCmd[u32CtlIndex].nofifo = 0;
    return 0;
}

DH_S32 I2C_DRV_WriteFifo(DH_U32 u32CtlIndex,DH_U32 hardgroup)
{
    DH_S32 fifoCnt;
    DH_U16 cmd;
    DH_U32 regAddr,start = 0;
    DH_VOID *lpRegBase;

    if (gu32IicBaseAddr[u32CtlIndex] != 0)
    {
        lpRegBase = (DH_VOID *)gu32IicBaseAddr[u32CtlIndex];
    }
    else
    {
        printk("i2c dev have not register %d\n", u32CtlIndex);

        return -1;
    }


    //printk("I2C_DRV_WriteFifo:\n");
    /*��Ҫ������ַ��Ϣ*/
    if(gSgrouopCmd[u32CtlIndex].nofifo == 1)
    {
        regAddr = gSgrouopCmd[u32CtlIndex].regAddr;
        if((regAddr & 0xff00) != 0)
        {
            //printk("cmd:0x%x\n",((regAddr & 0xff00) >> 8) | (1 << 9));
            I2C_HAL_SetReg( ((regAddr & 0xff00) >> 8) | (1 << 9) ,lpRegBase,I2C_REG_DATA_CMD);
            //printk("cmd:0x%x\n", regAddr & 0xff);
            I2C_HAL_SetReg( regAddr & 0xff ,lpRegBase,I2C_REG_DATA_CMD);
        }
        else
        {
            //printk("cmd:0x%x\n", (regAddr & 0xff) | (1 << 9));
            I2C_HAL_SetReg( (regAddr & 0xff) | (1 << 9) ,lpRegBase,I2C_REG_DATA_CMD);
        }

        gSgrouopCmd[u32CtlIndex].nofifo = 0;
    }

    while(1)
    {

        /*��������ַ*/
        cmd = gSgrouopCmd[u32CtlIndex].cmdBuf[gSgrouopCmd[u32CtlIndex].cmdPos];
        if((cmd & (1 << 9)) != 0)
        {
            /*���ģ��ʱ��������ַ��Ҫ���·��ͣ���������ֱ���˳�*/
            if(hardgroup == 0)
            {
                if(start != 0)
                    break;
                else
                    start = 1;
            }
            fifoCnt = I2C_HAL_GetReg(lpRegBase,I2C_REG_TXFLR);
            /*����ֻ�ѵ�ַ����ȥ֮���û��fifo�����*/
            /*���ʱ���ж����ִ��µĵ�ַ��ʼ���ͣ�����Ҫ�������͵�ַ��Ϣ*/

            if(I2C_BUF_DEP - fifoCnt < 4)
            {
                break;
            }
            gSgrouopCmd[u32CtlIndex].regAddr = cmd;
            //printk("cmd:0x%x\n", cmd);
            I2C_HAL_SetReg( cmd ,lpRegBase,I2C_REG_DATA_CMD);
            gSgrouopCmd[u32CtlIndex].cmdPos++;
            gSgrouopCmd[u32CtlIndex].cmdCnt--;
            cmd = gSgrouopCmd[u32CtlIndex].cmdBuf[gSgrouopCmd[u32CtlIndex].cmdPos];
            if((cmd & (1 << 9)) != 0)
            {
                gSgrouopCmd[u32CtlIndex].regAddr <<= 8;
                gSgrouopCmd[u32CtlIndex].regAddr += cmd & 0xff;
                 /*ȡ���ڶ����ֽڵĵ�9λ��ַ��־*/
                //printk("cmd:0x%x\n", cmd & 0xff);
                I2C_HAL_SetReg( cmd & 0xff ,lpRegBase,I2C_REG_DATA_CMD);
                gSgrouopCmd[u32CtlIndex].cmdPos++;
                gSgrouopCmd[u32CtlIndex].cmdCnt--;

            }
        }
        else
        {
            //printk("cmd:0x%x\n", cmd);
            I2C_HAL_SetReg( cmd ,lpRegBase,I2C_REG_DATA_CMD);
            gSgrouopCmd[u32CtlIndex].cmdPos++;
            gSgrouopCmd[u32CtlIndex].cmdCnt--;
            gSgrouopCmd[u32CtlIndex].regAddr++;
        }

        if(gSgrouopCmd[u32CtlIndex].cmdCnt <= 0)
        {
            break;
        }

        /*����fifo��*/
        //if((I2C_HAL_GetReg(lpRegBase,I2C_REG_STATUS) & (1 << 1)) == 0)
        fifoCnt = I2C_HAL_GetReg(lpRegBase,I2C_REG_TXFLR);
        if(fifoCnt >= I2C_BUF_DEP)
        {
            /*����Ļ������ݣ�����Ҫ������ַ��Ϣ*/
            cmd = gSgrouopCmd[u32CtlIndex].cmdBuf[gSgrouopCmd[u32CtlIndex].cmdPos];
            if((cmd & (1 << 9)) == 0)
            {
                gSgrouopCmd[u32CtlIndex].nofifo = 1;
            }
            break;
        }

    }
    return 0;
}


DH_S32 I2C_DRV_SetGroupSendStatus(DH_U32 u32CtlIndex,DHC_I2C_GROUP_STATUS_E status)
{
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }
    gEgroupStatus[u32CtlIndex] = status;
    return 0;
}

DHC_I2C_GROUP_STATUS_E I2C_DRV_GetGroupSendStatus(DH_U32 u32CtlIndex)
{
    if(u32CtlIndex > DHC_MAX_I2C_DEV - 1)
    {
        printk("sag i2c contrl from 0 to %d\n",DHC_MAX_I2C_DEV - 1);
        return -1;
    }
    return gEgroupStatus[u32CtlIndex];
}

DH_S32 I2C_DRV_InitLock(DH_U32 u32CtlIndex)
{
    mutex_init(&lock[u32CtlIndex]);
    return 0;
}


DH_S32 I2C_DRV_GetLock(DH_U32 u32CtlIndex)
{
    if(gEgroupStatus[u32CtlIndex] != STATUS_IDLE)
    {
        printk("i2c group send status %d not %d idle\n",gEgroupStatus[u32CtlIndex],STATUS_IDLE);
        return -1;
    }
    mutex_lock(&lock[u32CtlIndex]);
    return 0;
}

DH_S32 I2C_DRV_FreeLock(DH_U32 u32CtlIndex)
{
    mutex_unlock(&lock[u32CtlIndex]);
    return 0;
}


/* I2C������ARCH������ȥ��ʼ�� */
DH_S32 I2C_DRV_DeInit(DH_VOID *base, DH_U32 u32CtlIndex)
{
    gu32IicBaseAddr[u32CtlIndex] = 0;

    return DH_SUCCESS;
}


/* I2C������ARCH��������ʼ�� */
DH_S32 I2C_DRV_Init(DH_VOID *base, DH_U32 u32BusClk, DH_U32 u32Clk, DH_U32 u32CtlIndex)
{
    gu32IicSpdMode = DHC_I2C_SPDMODE_REV;
    I2C_DRV_EnableI2c(base, I2C_DISABLES);
    I2C_DRV_InitInterrupts(base);
    I2C_DRV_SetConfig(base);
    I2C_DRV_ConfigClk(base, u32BusClk, u32Clk);

    gu32IicBaseAddr[u32CtlIndex] = (long)base;

    return DH_SUCCESS;
}


