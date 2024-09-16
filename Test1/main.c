#include "support/gcc8_c_support.h"
#include <stddef.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <exec/execbase.h>
#include <graphics/gfxmacros.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>

struct ExecBase *SysBase;
volatile struct Custom *custom;

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef signed int s32;
typedef signed short s16;
typedef signed char s8;


#define GBA_E_OK					0
#define GBA_E_ERROR					(-1)
#define GBA_E_INVALID_ARGUMENT		(-2)
#define GBA_E_OUT_OF_MEMORY			(-3)


static void DecompressionU_decompress(const unsigned char* pInputData, unsigned char* pOutData);

INCBIN(compressedData, "C:/work/DebuggingDetails/AmigaDH0/Temp/bin20000.cmp")
//INCBIN_CHIP(compressedDataChip, "foo.bin")

#if 0
int runCodeAt(const void* startAddress) {
	register volatile const void* _a0 ASM("a0") = startAddress;
	register                int   _d0 ASM("d0");
	__asm volatile (
		"movem.l %%d1-%%d7/%%a1-%%a6,-(%%sp)\n"
		"jsr 0(%%a0)\n"
		"movem.l (%%sp)+,%%d1-%%d7/%%a1-%%a6"
	: "=r" (_d0), "+rf"(_a0)
	:
	: "cc", "memory");
	return _d0;
}
#endif

int main() {
	SysBase = *((struct ExecBase**)4UL);
	custom = (struct Custom*)0xdff000;

	Forbid();
	Disable();

	DecompressionU_decompress((unsigned char*) &incbin_compressedData_start , (unsigned char*) 0x1ffe0);

//	runCodeAt((void*)0x20000);

	__asm volatile (
		"jmp 0x20000\n"
	: 
	:
	: );


	return 0;
}



#define COMPU_EOD 512

static int DecompressionU_readBit(const unsigned char** ppInBlock, int* currentBitMask, unsigned char* bits)
{
	int nBit;
	const unsigned char* pInBlock = *ppInBlock;

	if ((*currentBitMask) == 0)
	{
		(*bits) = *pInBlock++;
		(*currentBitMask) = 128;
	}

	nBit = ((*bits) & 128) ? 1 : 0;

	(*bits) <<= 1;
	(*currentBitMask) >>= 1;

	*ppInBlock = pInBlock;
	return nBit;
}

static int DecompressionU_readPackedValue(const unsigned char** ppInBlock, const int initialValue, int* currentBitMask, unsigned char* bits)
{
	int nValue = initialValue;

	while (!DecompressionU_readBit(ppInBlock, currentBitMask, bits))
	{
		nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, currentBitMask, bits);
	}

	return nValue;
}

static int DecompressionU_readPackedValuePrefix(const unsigned char** ppInBlock, const int initialValue, int* currentBitMask, unsigned char* bits, unsigned int firstBit)
{
	int nValue = initialValue;

	if (!firstBit)
	{
		nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, currentBitMask, bits);
		while (!DecompressionU_readBit(ppInBlock, currentBitMask, bits))
		{
			nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, currentBitMask, bits);
		}
	}

	return nValue;
}

static void DecompressionU_decompress(const unsigned char* pInputData, unsigned char* pOutData)
{
	unsigned char* pCurOutData = pOutData;
	int currentBitMask = 0;
	unsigned char bits = 0;
	int nMatchOffset = 1;
	int nIsFirstCommand = 1;

	while (1)
	{
		unsigned int nIsMatchWithOffset;

		if (nIsFirstCommand)
		{
			// The first command is always literals
			nIsFirstCommand = 0;
			nIsMatchWithOffset = 0;
		}
		else
		{
			// Read match with offset / literals bit
			nIsMatchWithOffset = DecompressionU_readBit(&pInputData, &currentBitMask, &bits);
		}

		if (nIsMatchWithOffset == 0)
		{
			unsigned int numLiterals = DecompressionU_readPackedValue(&pInputData, 1, &currentBitMask, &bits);

			custom->color[0] = (UWORD) numLiterals;

			// Copy literals
			memcpy(pCurOutData, pInputData, numLiterals);
			pInputData += numLiterals;
			pCurOutData += numLiterals;

			// Read match with offset / rep match bit
			nIsMatchWithOffset = DecompressionU_readBit(&pInputData, &currentBitMask, &bits);
		}

		unsigned int nMatchLen;

		if (nIsMatchWithOffset)
		{
			// Match with offset
			unsigned int nMatchOffsetHighByte = DecompressionU_readPackedValue(&pInputData, 1, &currentBitMask, &bits);

			if (nMatchOffsetHighByte == COMPU_EOD)
			{
				break;
			}

			nMatchOffsetHighByte--;

			unsigned int nMatchOffsetLowByte = (unsigned int)(*pInputData++);
			nMatchOffset = (nMatchOffsetHighByte << 7) | (127 - (nMatchOffsetLowByte >> 1));
			nMatchOffset++;

			nMatchLen = DecompressionU_readPackedValuePrefix(&pInputData, 1, &currentBitMask, &bits, nMatchOffsetLowByte & 1);

			nMatchLen += (2 - 1);
		}
		else
		{
			// Rep-match
			nMatchLen = DecompressionU_readPackedValue(&pInputData, 1, &currentBitMask, &bits);
		}

		// Copy matched bytes
		const unsigned char* pSrc = pCurOutData - nMatchOffset;
		custom->color[0] = (UWORD) nMatchLen;

//					// Note: Debug
//					printf("nMatchOffset $%x nMatchLen $%x\n" , nMatchOffset , nMatchLen);
		while (nMatchLen)
		{
//						// Note: Debug
//						if ( (pCurOutData - pOutData) == 0xac45 - 0x400 )
//						{
//							int i = 0;
//						}
			*pCurOutData++ = *pSrc++;
			nMatchLen--;
		}
	}
}
