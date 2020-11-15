//-----------------------------------------------------------------------------
// Copyright Ed Keenan 2019
// Optimized C++
//----------------------------------------------------------------------------- 

#ifndef MEM_H
#define MEM_H

#include "Heap.h"

class Mem
{
public:
	static const unsigned int HEAP_SIZE = (50 * 1024);

	//Constructors
	Mem();
	Mem(const Mem &) = delete;
	Mem & operator = (const Mem &) = delete;
	~Mem();

	Heap *getHeap();
	void dump();

	// implement these functions
	void free(void * const data);
	void *malloc(const uint32_t size) const;
	void initialize() const;

private:
	//Getters
	Used * GetUsedHead() const;
	Free* GetFreeHead() const;

	// Helper Functions

	//Set secret pointer
	void SetSecretPtr(Free* pFree) const;

	// get next fit for allocation
	Free* GetNextFit(uint32_t size) const;

	//Create Free Headers
	Free* CreateFreeBlock(void* addr, uint32_t size) const;

	//Create Used Headers
	Used* CreateUsedBlock(void* addr, uint32_t size) const;

	//Add / Remove - Free List
	void AddToFreeList(Free* freeBlock) const;
	void RemoveFromFreeList(void* block) const;

	//Add / Remove - Used List
	void AddToUsedList(Used* usedBlock) const;
	void RemoveFromUsedList(Used* blockToRmv) const;

	//Coalesce free blocks
	Free* CoalesceDown(void* pUsed, void* blockBelow) const;
	Free* CoalesceUp(void* blockAbove, void* pUsed) const;
	//Free* CoalesceBoth(void* top, void* middle, void* bottom);

	//Get Header Pointer from block ptr (data)
	void* GetHeaderPtr(void* blockPtr);

	//Get free header pointer to block above - using secret pointer
	Free* GetBlockAboveFromSecretPtr(Used* pUsed);

	//Check if block above free
	bool IsAboveFree(Free* blockAbove);

	//Check if below block free
	bool IsBelowFree(Free* blockBelow);

	//Get ptr to block below
	void* GetBlockBelow(Used* pUsed, uint32_t size);

	//Update free stats
	void AddFreeStatsUpdate(Free* pFree) const;
	void RemoveFreeStatsUpdate(Free* pFree) const;

	//Update coalesce stats
	void CoalesceStatsUpdate(Free* coalescedBlock) const;

	//Update used stats
	void AddUsedStatsUpdate(Used* pUsed) const;
	void RemoveUsedStatsUpdate(Used* pUsed) const;

private:
	//Data
	Heap * pHeap;
	void	*pRawMem;
};

#endif 

// ---  End of File ---------------
