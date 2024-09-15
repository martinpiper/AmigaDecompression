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
struct DosLibrary *DOSBase;
struct GfxBase *GfxBase;

typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef signed int s32;
typedef signed short s16;
typedef signed char s8;

extern u32 LONG_OFFSET_THRESHOLD;

#define GBA_E_OK					0
#define GBA_E_ERROR					(-1)
#define GBA_E_INVALID_ARGUMENT		(-2)
#define GBA_E_OUT_OF_MEMORY			(-3)


int DecompressU( const u8 * source, u32 sourceLen ,u8 * dest, u32 * destLen );

INCBIN(compressedData, "C:/work/DebuggingDetails/AmigaDH0/Temp/bin20000.cmp")
//INCBIN_CHIP(compressedDataChip, "foo.bin")

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

int main() {
	SysBase = *((struct ExecBase**)4UL);
	custom = (struct Custom*)0xdff000;

	Disable();
	Forbid();

	u32 destLen = 0;
	DecompressU((u8*) compressedData , (u8*)&incbin_compressedData_end - (u8*)&incbin_compressedData_start, (u8*) 0x1ffe0, &destLen);

	runCodeAt((void*)0x20000);

	return 0;
}



#define MIN_OFFSET 1
#define MAX_OFFSET 0xffff
#define COMPU_EOD 512


#define MAX_VARLEN 0xffff

#define BLOCK_SIZE 0x10000

// Anything above this, let the optimiser handle them
#define MAX_BLOCK_MATCH_LEN 0x2000

#define LCP_BITS 18
#define TAG_BITS 4
#define LCP_MAX ((1U<<(LCP_BITS - TAG_BITS)) - 1)

#define NINITIAL_ARRIVALS_PER_POSITION 140
#define NMAX_ARRIVALS_PER_POSITION 209
#define NMATCHES_PER_INDEX 150

#define LEAVE_ALONE_MATCH_SIZE 340


#define MIN_ENCODED_MATCH_SIZE   2
#define TOKEN_SIZE               1
#define OFFSET_COST(__offset)    (((__offset) <= 128) ? 8 : (7 + CompressionU_getPackedValueSize((((__offset) - 1) >> 7) + 1)))


static inline int DecompressionU_readBit(const unsigned char** ppInBlock, const unsigned char* pDataEnd, int* currentBitMask, unsigned char* bits)
{
	int nBit;
	const unsigned char* pInBlock = *ppInBlock;

	if ((*currentBitMask) == 0)
	{
		if (pInBlock >= pDataEnd) return -1;
		(*bits) = *pInBlock++;
		(*currentBitMask) = 128;
	}

	nBit = ((*bits) & 128) ? 1 : 0;

	(*bits) <<= 1;
	(*currentBitMask) >>= 1;

	*ppInBlock = pInBlock;
	return nBit;
}

static inline int DecompressionU_readPackedValue(const unsigned char** ppInBlock, const unsigned char* pDataEnd, const int initialValue, int* currentBitMask, unsigned char* bits)
{
	int nValue = initialValue;

	while (!DecompressionU_readBit(ppInBlock, pDataEnd, currentBitMask, bits))
	{
		nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, pDataEnd, currentBitMask, bits);
	}

	return nValue;
}

static inline int DecompressionU_readPackedValuePrefix(const unsigned char** ppInBlock, const unsigned char* pDataEnd, const int initialValue, int* currentBitMask, unsigned char* bits, unsigned int firstBit)
{
	int nValue = initialValue;

	if (!firstBit)
	{
		nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, pDataEnd, currentBitMask, bits);
		while (!DecompressionU_readBit(ppInBlock, pDataEnd, currentBitMask, bits))
		{
			nValue = (nValue << 1) | DecompressionU_readBit(ppInBlock, pDataEnd, currentBitMask, bits);
		}
	}

	return nValue;
}

static size_t DecompressionU_decompress(const unsigned char* pInputData, unsigned char* pOutData, size_t inputSize, size_t maxOutBufferSize)
{
	const unsigned char* pInputDataEnd = pInputData + inputSize;
	unsigned char* pCurOutData = pOutData;
	const unsigned char* pOutDataEnd = pCurOutData + maxOutBufferSize;
	int currentBitMask = 0;
	unsigned char bits = 0;
	int nMatchOffset = 1;
	int nIsFirstCommand = 1;

	if (pInputData >= pInputDataEnd && pCurOutData < pOutDataEnd)
		return -1;

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
			nIsMatchWithOffset = DecompressionU_readBit(&pInputData, pInputDataEnd, &currentBitMask, &bits);
			if (nIsMatchWithOffset == -1)
				return -1;
		}

		if (nIsMatchWithOffset == 0)
		{
			unsigned int numLiterals = DecompressionU_readPackedValue(&pInputData, pInputDataEnd, 1, &currentBitMask, &bits);

			custom->color[0] = (UWORD) numLiterals;

			// Copy literals
			if ((pInputData + numLiterals) <= pInputDataEnd && (pCurOutData + numLiterals) <= pOutDataEnd)
			{
				memcpy(pCurOutData, pInputData, numLiterals);
				pInputData += numLiterals;
				pCurOutData += numLiterals;
			}
			else
			{
				return -1;
			}

			// Read match with offset / rep match bit
			nIsMatchWithOffset = DecompressionU_readBit(&pInputData, pInputDataEnd, &currentBitMask, &bits);
			if (nIsMatchWithOffset == -1)
			{
				return -1;
			}
		}

		unsigned int nMatchLen;

		if (nIsMatchWithOffset)
		{
			// Match with offset
			unsigned int nMatchOffsetHighByte = DecompressionU_readPackedValue(&pInputData, pInputDataEnd, 1, &currentBitMask, &bits);

			if (nMatchOffsetHighByte == COMPU_EOD)
			{
				break;
			}
			if (nMatchOffsetHighByte > COMPU_EOD)
			{
				return -1;
			}

			nMatchOffsetHighByte--;

			if (pInputData >= pInputDataEnd)
			{
				return -1;
			}

			unsigned int nMatchOffsetLowByte = (unsigned int)(*pInputData++);
			nMatchOffset = (nMatchOffsetHighByte << 7) | (127 - (nMatchOffsetLowByte >> 1));
			nMatchOffset++;

			nMatchLen = DecompressionU_readPackedValuePrefix(&pInputData, pInputDataEnd, 1, &currentBitMask, &bits, nMatchOffsetLowByte & 1);

			nMatchLen += (2 - 1);
		}
		else
		{
			// Rep-match
			nMatchLen = DecompressionU_readPackedValue(&pInputData, pInputDataEnd, 1, &currentBitMask, &bits);
		}

		// Copy matched bytes
		const unsigned char* pSrc = pCurOutData - nMatchOffset;
		if (pSrc >= pOutData)
		{
			if ((pSrc + nMatchLen) <= pOutDataEnd)
			{
				if ((pCurOutData + nMatchLen) <= pOutDataEnd)
				{
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
				else
				{
					return -1;
				}
			}
			else
			{
				return -1;
			}
		}
		else
		{
			return -1;
		}
	}

	return (size_t)(pCurOutData - pOutData);
}


int DecompressU( const u8 * source, u32 sourceLen ,u8 * dest, u32 * destLen )
{
	size_t ret = DecompressionU_decompress(source , dest , sourceLen , 0x10000);

	if (ret == (size_t) -1)
	{
		return GBA_E_ERROR;
	}

	*destLen = (u32) ret;

	return GBA_E_OK;
}
