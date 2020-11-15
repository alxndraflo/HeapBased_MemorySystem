//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#include "Framework.h"

#include "Used.h"
#include "Free.h"
#include "Block.h"

// Add magic here

//Constructors

Free::Free(uint32_t size)
	: pFreeNext(nullptr),
	pFreePrev(nullptr),
	mBlockSize(size),
	mType(Block::Free),
	mAboveBlockFree(false),
	pad(0)
{
}

Free::Free(void* topAddr, void* bottomAddr)
	: pFreeNext(nullptr),
	pFreePrev(nullptr),
	mType(Block::Free),
	mAboveBlockFree(false),
	pad(0)
{
	this->mBlockSize = (uint32_t)bottomAddr - (uint32_t)topAddr - sizeof Free;
}


// ---  End of File ---------------
