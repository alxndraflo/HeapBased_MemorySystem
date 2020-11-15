//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#include <malloc.h>
#include <new>

#include "Framework.h"

#include "Mem.h"
#include "Heap.h"
#include "Block.h"

#define STUB_PLEASE_REPLACE(x) (x)

//Struct to define secret pointer. 
struct SecretPtr
{
	SecretPtr() = default;

	//specialized constructor to set free header address
	SecretPtr(Free* pAddr)
	{
		this->free = pAddr;
	}

	//Method to access secret pointer
	Free* GetSecretPtr() const;

	Free *free;
};

//Method to get free member of secret ptr. Points to free header
Free* SecretPtr::GetSecretPtr() const
{
	return this->free;
}

//Function to create initial free block in heap from raw memory
void Mem::initialize() const
{
	// calculate address to place initial free block
	void* initAddr = this->pHeap + 1;

	//Place initial free header at address
	Free* initFree = placement_new(initAddr, Free, initAddr, this->pHeap->mStats.heapBottomAddr); //use heapBottomAddr in this case only because creating INITIAL free block of memory from our "heap"

	//Set pointers for HEAP
	this->pHeap->pFreeHead = initFree;
	this->pHeap->pNextFit = initFree;

	//Set pointers for free block
	initFree->pFreePrev = nullptr;
	initFree->pFreeNext = nullptr;

	//Update stats
	this->pHeap->mStats.currFreeMem = initFree->mBlockSize;
	this->pHeap->mStats.currNumFreeBlocks = 1;
}

//Function to allocate memory on heap according to a given size
void* Mem::malloc(const uint32_t size) const
{
	//Get nextFit ptr
	Free* pFree = this->GetNextFit(size);
	Used* usedBlock = nullptr;

	if (pFree != nullptr)
	{
		if (size == pFree->mBlockSize) //if perfect fit
		{
			//Remove old free block from list & update stats
			RemoveFromFreeList(pFree);
			RemoveFreeStatsUpdate(pFree);

			//Create used header/block
			usedBlock = CreateUsedBlock(pFree, size);

			//Add newly created used block to list & update stats
			AddToUsedList(usedBlock);
			AddUsedStatsUpdate(usedBlock);
		}
		else //if size is less than block size (need to subdivide)
		{
			//Save original free (next fit) address, ptrs, and size
			Free* origFree = pFree;
			uint32_t oldBlockSize = origFree->mBlockSize; //save old block size

			//Remove old free from free list 
			RemoveFromFreeList(pFree);
			//Update stats
			RemoveFreeStatsUpdate(pFree);

			//Create new used header from subdivided free block
			usedBlock = CreateUsedBlock(pFree, size);

			//Subdivide - Get new block size, calculate extra free block address, create new free block.
			void* newFreeAddr = (uint32_t)usedBlock + sizeof Used + (uint8_t*)size;
			uint32_t newBlockSize = oldBlockSize - (size + sizeof Free);
			Free* extraSpace = CreateFreeBlock(newFreeAddr, newBlockSize);

			//Reset nextFit to new free block (subdivided)
			this->pHeap->pNextFit = extraSpace;

			//Add used block to list
			AddToUsedList(usedBlock);
			//Update stats
			AddUsedStatsUpdate(usedBlock);

			//Add extraFree to free list
			AddToFreeList(extraSpace);
			//Update stats
			AddFreeStatsUpdate(extraSpace);
		}
	}

	return usedBlock + 1; //return used + 1 to point to block ptr, NOT hdr ptr
}

//Function to free memory from heap given a pointer to specific block
void Mem::free(void * const data)
{
	//Get ptr to used header from block ptr
	Used* pUsed = (Used*)GetHeaderPtr(data);

	//Save original block size
	const uint32_t origBlockSize = pUsed->mBlockSize;

	//Find address for block below 
	Free* blockBelow = (Free*)GetBlockBelow(pUsed, origBlockSize);

	//Check if block below is free
	const bool belowFree = IsBelowFree(blockBelow);

	//Find address of block above (get secret ptr)
	Free* blockAbove = nullptr;
	bool aboveFree = false;

	if (pUsed->mAboveBlockFree && pUsed > this->pHeap->mStats.heapTopAddr)
	{
		blockAbove = GetBlockAboveFromSecretPtr(pUsed);
		aboveFree = IsAboveFree(blockAbove);
	}

	//Remove used block from list
	RemoveFromUsedList(pUsed);
	//Update stats
	RemoveUsedStatsUpdate(pUsed);

	//if block below less than heap bottom, set aboveFree flag
	if (blockBelow < this->pHeap->mStats.heapBottomAddr)
	{
		blockBelow->mAboveBlockFree = true;
	}

	//if block above is used AND block below is used
	if (!blockAbove && !blockBelow)
	{
		//Create free block from used
		Free* pFree = CreateFreeBlock((void*)pUsed, pUsed->mBlockSize);

		//Set block below's aboveBlockFree flag
		if (pUsed > this->pHeap->mStats.heapTopAddr)
		{
			blockBelow->mAboveBlockFree = true;
		}

		//Add free block to list
		AddToFreeList(pFree);
		//Update stats
		AddFreeStatsUpdate(pFree);
	}
	//if block above is used AND block below is free - coalesce down
	else if (!aboveFree && belowFree)
	{
		//create new free from used - convert
		Free* convertedFree = CreateFreeBlock(pUsed, origBlockSize);

		//Store next/prev for block below
		Free* blockBelowNext = blockBelow->pFreeNext;
		Free* blockBelowPrev = blockBelow->pFreePrev;

		//Remove block below from free list
		RemoveFromFreeList(blockBelow);
		//Update stats
		RemoveFreeStatsUpdate(blockBelow);

		//Glue blocks together (coalesce down)
		Free* coalescedBlock = CoalesceDown(convertedFree, blockBelow);

		//Fix pointers to add to free list
		coalescedBlock->pFreeNext = blockBelowNext;

		if (blockBelowNext != nullptr)
		{
			blockBelowNext->pFreePrev = coalescedBlock;
		}

		if (blockBelowPrev != nullptr)
		{
			blockBelowPrev->pFreeNext = coalescedBlock;
		}

		coalescedBlock->pFreePrev = blockBelowPrev;

		//Update next fit
		if (this->pHeap->pNextFit == nullptr)
		{
			this->pHeap->pNextFit = coalescedBlock;
		}

		//Update free head
		if (convertedFree == this->pHeap->mStats.heapTopAddr)
		{
			this->pHeap->pFreeHead = convertedFree;
		}
		if (this->pHeap->pFreeHead == nullptr)
		{
			this->pHeap->pFreeHead = coalescedBlock;
		}

		//Update stats
		CoalesceStatsUpdate(coalescedBlock);
	}
	//if block above is free AND block below is used - coalesce up
	else if(aboveFree && !belowFree)
	{
		//Update stats
		RemoveFreeStatsUpdate(blockAbove);

		//Glue free blocks together (coalesce up)
		Free* coalescedBlock = CoalesceUp(blockAbove, pUsed);

		//Update stats
		CoalesceStatsUpdate(coalescedBlock);
	}
	else if (aboveFree && belowFree)
	{
		//Save block below next ptr
		Free* blockBelowNext = blockBelow->pFreeNext;

		const uint32_t blockAboveOrigSize = blockAbove->mBlockSize;

		//Get new size
		const uint32_t coalescedBlockSize = blockAbove->mBlockSize + sizeof(Free) + origBlockSize + sizeof(Free) + blockBelow->mBlockSize;

		blockAbove->mBlockSize = coalescedBlockSize;

		//Set secret pointer
		SetSecretPtr(blockAbove);

		//Fix pointers
		blockAbove->pFreeNext = blockBelowNext;
		if (blockAbove->pFreeNext != nullptr) 
		{
			blockAbove->pFreeNext->pFreePrev = blockAbove;
		}

		//Update stats
		this->pHeap->mStats.currFreeMem -= blockBelow->mBlockSize;
		this->pHeap->mStats.currFreeMem -= blockAboveOrigSize;
		CoalesceStatsUpdate(blockAbove);
		this->pHeap->mStats.currNumFreeBlocks -= 2;

		//Update free head and next fit
		this->pHeap->pFreeHead = blockAbove;
		this->pHeap->pNextFit = blockAbove;
	}
	else
	{
		//Create free block from used
		Free* pFree = CreateFreeBlock((void*)pUsed, pUsed->mBlockSize);

		//Add free block to list
		AddToFreeList(pFree);
		//Update stats
		AddFreeStatsUpdate(pFree);
	}
}

// Helper Functions

void Mem::SetSecretPtr(Free* pFree) const
{
	//Set secret pointer
	void* blockBottom = (uint8_t*)(pFree + 1) + pFree->mBlockSize;
	void* secretPtrAddr = (uint8_t*)blockBottom - 4;
	placement_new(secretPtrAddr, SecretPtr, pFree);
}


// Method to FIND next free block large enough for allocation
Free* Mem::GetNextFit(uint32_t size) const
{
	Free* free = this->pHeap->pNextFit; //set free node to this.pNextFit
	Free* foundNode = nullptr;

	while (free != nullptr)
	{
		if (free->mBlockSize >= size) //if free node block size is >= size
		{
			foundNode = free;
			break;
		}

		free = free->pFreeNext;

		if (free == nullptr)
		{
			free = this->pHeap->pFreeHead;
		}
		else if (free == this->pHeap->pNextFit)
		{
			break;
		}
	}

	this->pHeap->pNextFit = foundNode;

	return this->pHeap->pNextFit;
}

//Method to create and return free block
Free* Mem::CreateFreeBlock(void* addr, uint32_t size) const
{
	//Create new free header
	Free* pFree = placement_new(addr, Free, size);

	//Set secret pointer
	SetSecretPtr(pFree);

	return pFree;
}

//Method to create and return used block
Used* Mem::CreateUsedBlock(void* addr, uint32_t size) const
{
	Used* pUsed = placement_new(addr, Used, size);

	return pUsed;
}

// Function to add block to free list. INSERTS IN SORTED ORDER***
void Mem::AddToFreeList(Free* freeBlock) const
{
	//Get head of free list to walk
	Free* current = this->GetFreeHead();

	if (this->GetFreeHead() == nullptr) //if list is empty
	{
		this->pHeap->pFreeHead = freeBlock;
		this->pHeap->pNextFit = freeBlock;

		//Fix pointers
		this->pHeap->pFreeHead->pFreeNext = nullptr;
		this->pHeap->pFreeHead->pFreePrev = nullptr;
	}
	else if (freeBlock < current) //add to front of list
	{
		freeBlock->pFreeNext = this->GetFreeHead();
		this->pHeap->pFreeHead->pFreePrev = freeBlock;
		this->pHeap->pFreeHead = freeBlock;
		freeBlock->pFreePrev = nullptr;
	}
	else
	{

		while (current->pFreeNext != nullptr && current->pFreeNext < freeBlock) //while current is not null and freeBlock address is greater than current, go to next node
		{
			current = current->pFreeNext;
		}

		//add to middle
		if (current->pFreeNext != nullptr)
		{
			//current->pFreePrev->pFreeNext = freeBlock;
			current->pFreeNext->pFreePrev = freeBlock;
			freeBlock->pFreePrev = current;
			freeBlock->pFreeNext = current->pFreeNext;
			current->pFreeNext = freeBlock;
		}
		//add to end
		else
		{
			current->pFreeNext = freeBlock;
			freeBlock->pFreeNext = nullptr;
			freeBlock->pFreePrev = current;
		}
	}
}

//Function to remove block from free list
void Mem::RemoveFromFreeList(void* block) const
{
	Free* blockToRemove = (Free*)block;

	// Remove ONLY node in list
	if (blockToRemove->pFreePrev == nullptr && blockToRemove->pFreeNext == nullptr)
	{
		this->pHeap->pFreeHead = nullptr;
		this->pHeap->pNextFit = nullptr;
	}
	// Remove from front of list
	else if (blockToRemove->pFreePrev == nullptr && blockToRemove->pFreeNext != nullptr)
	{
		this->pHeap->pFreeHead = blockToRemove->pFreeNext;
		this->pHeap->pFreeHead->pFreePrev = nullptr;
	}
	// Remove from end of list
	else if (blockToRemove->pFreePrev != nullptr && blockToRemove->pFreeNext == nullptr)
	{
		blockToRemove->pFreePrev->pFreeNext = nullptr;
	}
	// Remove from middle
	else
	{
		blockToRemove->pFreePrev->pFreeNext = blockToRemove->pFreeNext;
		blockToRemove->pFreeNext->pFreePrev = blockToRemove->pFreePrev;
	}

}

// Function to add used block to front of list
void Mem::AddToUsedList(Used* usedBlock) const
{
	if (this->GetUsedHead() == nullptr) //if list is empty
	{
		this->pHeap->pUsedHead = usedBlock;

		//Fix pointers
		this->pHeap->pUsedHead->pUsedNext = nullptr;
		this->pHeap->pUsedHead->pUsedPrev = nullptr;
	}
	else
	{
		usedBlock->pUsedNext = this->GetUsedHead();
		this->pHeap->pUsedHead->pUsedPrev = usedBlock;
		this->pHeap->pUsedHead = usedBlock;
		usedBlock->pUsedPrev = nullptr;
	}
}

//Function to remove block from used list
void Mem::RemoveFromUsedList(Used* blockToRmv) const
{
	if (blockToRmv->pUsedPrev != nullptr)
	{
		blockToRmv->pUsedPrev->pUsedNext = blockToRmv->pUsedNext;
	}
	else
	{
		this->pHeap->pUsedHead = blockToRmv->pUsedNext;
	}

	if (blockToRmv->pUsedNext != nullptr)
	{
		blockToRmv->pUsedNext->pUsedPrev = blockToRmv->pUsedPrev;
	}
}

//Glues block to block below (glue down, coalesce down)
Free* Mem::CoalesceDown(void* pUsed, void* blockBelow) const
{
	Free* upper = (Free*)pUsed;
	Free* lower = (Free*)blockBelow;

	//Calculate new block size for coalesced block
	const uint32_t coalescedBlockSize = (uint32_t)upper->mBlockSize + sizeof Free + (uint32_t)lower->mBlockSize;

	//Fix pointers for coalesced block
	upper->mBlockSize = coalescedBlockSize;

	//Set secret pointer
	SetSecretPtr(upper);

	return upper;
}

//Glues block to block above (glue up, coalesce up)
Free* Mem::CoalesceUp(void* blockAbove, void* pUsed) const
{
	Free* upper = (Free*)blockAbove;
	Free* lower = (Free*)pUsed;

	//Calculate new block size
	const uint32_t coalescedBlockSize = (uint32_t)upper->mBlockSize + sizeof Free + (uint32_t)lower->mBlockSize;

	//Create new coalesced free block
	upper->mBlockSize = coalescedBlockSize;

	//Set secret pointer
	SetSecretPtr(upper);

	return upper;
}

//Get pointer to free block above using secret pointer
Free* Mem::GetBlockAboveFromSecretPtr(Used* pUsed)
{
	SecretPtr* pSecret;
	pSecret = (SecretPtr*)pUsed;
	pSecret = pSecret - 1;

	Free* freeBlockAbove = pSecret->GetSecretPtr();

	return freeBlockAbove;
}

//Get pointer to header pointer from block (data) pointer
void* Mem::GetHeaderPtr(void* blockPtr)
{
	void* headerAddr = (Used*)blockPtr - 1;

	return headerAddr;
}

//Check to see if block above is free
bool Mem::IsAboveFree(Free* blockAbove)
{
	bool aF = false;

	Block bType = blockAbove->mType;

	//if block above is free, set aF to true
	if (bType == Block::Free)
	{
		aF = true;
	}

	return aF;
}

//Check to see if block below is free
bool Mem::IsBelowFree(Free* blockBelow)
{
	bool bF = false;
	Block bType = blockBelow->mType;

	//if block below free, set bF to true
	if (bType == Block::Free)
	{
		bF = true;
	}
	
	return bF;
}

//Get pointer to block below. Returns pointer to header
void* Mem::GetBlockBelow(Used* pUsed, uint32_t size)
{
	void* belowAddr = (uint8_t*)(pUsed + 1) + size;
	Free* blockBelow = (Free*)belowAddr;

	return blockBelow;
}

//Update stats after adding free block
void Mem::AddFreeStatsUpdate(Free* pFree) const
{
	//Update stats
	this->pHeap->mStats.currFreeMem += pFree->mBlockSize;
	this->pHeap->mStats.currNumFreeBlocks += 1;
}

//Update stats after coalescing
void Mem::CoalesceStatsUpdate(Free* coalescedBlock) const
{
	//this->pHeap->mStats.currNumFreeBlocks -= 1;
	this->pHeap->mStats.currNumFreeBlocks += 1;
	//this->pHeap->mStats.currFreeMem += sizeof Free;
	this->pHeap->mStats.currFreeMem += coalescedBlock->mBlockSize;
}

//Update stats after removing free block
void Mem::RemoveFreeStatsUpdate(Free* pFree) const
{
	//Update stats
	this->pHeap->mStats.currFreeMem -= pFree->mBlockSize;
	this->pHeap->mStats.currNumFreeBlocks -= 1;
}

//Update stats after adding used block
void Mem::AddUsedStatsUpdate(Used* pUsed) const
{
	//Update stats
	this->pHeap->mStats.currUsedMem += pUsed->mBlockSize;
	if (this->pHeap->mStats.currUsedMem > this->pHeap->mStats.peakUsedMemory)
	{
		this->pHeap->mStats.peakUsedMemory = this->pHeap->mStats.currUsedMem;
	}

	this->pHeap->mStats.currNumUsedBlocks += 1;
	if (this->pHeap->mStats.currNumUsedBlocks > this->pHeap->mStats.peakNumUsed)
	{
		this->pHeap->mStats.peakNumUsed = this->pHeap->mStats.currNumUsedBlocks;
	}
}

//Update stats after removing used block
void Mem::RemoveUsedStatsUpdate(Used* pUsed) const
{
	//Update stats
	this->pHeap->mStats.currUsedMem -= pUsed->mBlockSize;
	this->pHeap->mStats.currNumUsedBlocks -= 1;
}

//Getters
Used* Mem::GetUsedHead() const
{
	return this->pHeap->pUsedHead;
}

Free* Mem::GetFreeHead() const
{
	return this->pHeap->pFreeHead;
}


// ---  End of File ---------------
