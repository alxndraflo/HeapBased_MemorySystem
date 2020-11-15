//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#ifndef FREE_H
#define FREE_H

#include "Types.h"
#include "Block.h"

class Used;

class Free
{
public:

	//Constructors
	Free() = default;
	Free(const Free& rhs) = default;
	Free& operator=(const Free& rhs) = default;
	Free(uint32_t size);
	Free(void* topAddr, void* bottomAddr);
	~Free() = default;

	//Data
	Free        *pFreeNext;       // next free block
	Free        *pFreePrev;       // prev free block
	uint32_t    mBlockSize;       // size of block
	Block       mType;            // block type 
	bool        mAboveBlockFree;  // AboveBlock flag
										  //    if(AboveBlock is type free) -> true 
										  //    if(AboveBlock is type used) -> false 
	uint16_t    pad;              // future use


};

#endif 

// ---  End of File ---------------
