/*
**  Command & Conquer Generals Zero Hour(tm)
**  Simplified OS-backed memory manager replacement.
*/

#include "PreRTS.h"

#include "Common/GameMemory.h"
#include "Common/CriticalSection.h"
#include "Common/Errors.h"
#include "Common/GlobalData.h"
#include "Common/PerfTimer.h"

#include <new>
#include <malloc.h>
#include <string.h>

#ifdef MEMORYPOOL_DEBUG
DECLARE_PERF_TIMER(MemoryPoolDebugging)
DECLARE_PERF_TIMER(MemoryPoolInitFilling)
#endif

// ----------------------------------------------------------------------------
// PRIVATE DATA / HELPERS
// ----------------------------------------------------------------------------

#ifndef MEM_BOUND_ALIGNMENT
#define MEM_BOUND_ALIGNMENT 8
#endif

static Bool thePreMainInitFlag = false;
static Bool theMainInitFlag = false;
static Int  theLinkTester = 0;

#ifdef MEMORYPOOL_DEBUG
static Int theTotalSystemAllocationInBytes = 0;
static Int thePeakSystemAllocationInBytes = 0;
static Int theTotalLargeBlocks = 0;
static Int thePeakLargeBlocks = 0;
Int theTotalDMA = 0;
Int thePeakDMA = 0;
Int theWastedDMA = 0;
Int thePeakWastedDMA = 0;

static const char* FREE_SINGLEBLOCK_TAG_STRING = "FREE_SINGLEBLOCK_TAG_STRING";
const Short SINGLEBLOCK_MAGIC_COOKIE = 12345;
const Int   GARBAGE_FILL_VALUE = 0xdeadbeef;

enum
{
  IGNORE_LEAKS = 0x0001
};

#define USE_FILLER_VALUE
const Int MAX_INIT_FILLER_COUNT = 8;
#ifdef USE_FILLER_VALUE
static UnsignedInt s_initFillerValue = 0xf00dcafe;
static void calcFillerValue(Int index)
{
  s_initFillerValue = (index & 3) << 1;
  s_initFillerValue |= 0x01;
  s_initFillerValue |= (~(s_initFillerValue << 4)) & 0xf0;
  s_initFillerValue |= (s_initFillerValue << 8);
  s_initFillerValue |= (s_initFillerValue << 16);
}
#endif
#endif

static void preMainInitMemoryManager();

static Int roundUpMemBound(Int i)
{
  return (i + (MEM_BOUND_ALIGNMENT - 1)) & ~(MEM_BOUND_ALIGNMENT - 1);
}

static void memset32(void* ptr, Int value, Int bytesToFill)
{
  Int wordsToFill = bytesToFill >> 2;
  bytesToFill -= (wordsToFill << 2);

  Int* p = (Int*)ptr;
  for (++wordsToFill; --wordsToFill; )
    *p++ = value;

  Byte* b = (Byte*)p;
  for (++bytesToFill; --bytesToFill; )
    *b++ = (Byte)value;
}

// ----------------------------------------------------------------------------
// RAW OS ALLOCATION HEADER
// ----------------------------------------------------------------------------

struct OsAllocHeader
{
  size_t      size;
#ifdef MEMORYPOOL_DEBUG
  const char* tag;
#endif
};

static void* sysAllocateDoNotZero(Int numBytes)
{
  if (numBytes <= 0)
    numBytes = 1;

  const size_t total = sizeof(OsAllocHeader) + (size_t)numBytes;
  OsAllocHeader* h = (OsAllocHeader*)::malloc(total);
  if (!h)
    throw ERROR_OUT_OF_MEMORY;

  ::memset(h, 0, total);

  h->size = (size_t)numBytes;
#ifdef MEMORYPOOL_DEBUG
  h->tag = FREE_SINGLEBLOCK_TAG_STRING;

  // Do NOT fill returned memory with debug filler here, because caller-visible
  // memory must always be zeroed when handed out.
  theTotalSystemAllocationInBytes += (Int)total;
  if (thePeakSystemAllocationInBytes < theTotalSystemAllocationInBytes)
    thePeakSystemAllocationInBytes = theTotalSystemAllocationInBytes;
#endif

  return (void*)(h + 1);
}

static void sysFree(void* p)
{
  if (!p)
    return;

  OsAllocHeader* h = ((OsAllocHeader*)p) - 1;
#ifdef MEMORYPOOL_DEBUG
  theTotalSystemAllocationInBytes -= (Int)(sizeof(OsAllocHeader) + h->size);
#endif
  ::free(h);
}

static size_t sysAllocationSize(void* p)
{
  if (!p)
    return 0;
  const OsAllocHeader* h = ((const OsAllocHeader*)p) - 1;
  return h->size;
}

#ifdef MEMORYPOOL_DEBUG
static void sysSetTag(void* p, const char* tag)
{
  if (!p)
    return;
  OsAllocHeader* h = ((OsAllocHeader*)p) - 1;
  h->tag = tag ? tag : FREE_SINGLEBLOCK_TAG_STRING;
}

static const char* sysGetTag(void* p)
{
  if (!p)
    return FREE_SINGLEBLOCK_TAG_STRING;
  const OsAllocHeader* h = ((const OsAllocHeader*)p) - 1;
  return h->tag ? h->tag : FREE_SINGLEBLOCK_TAG_STRING;
}
#endif

// ----------------------------------------------------------------------------
// PUBLIC DATA
// ----------------------------------------------------------------------------

MemoryPoolFactory* TheMemoryPoolFactory = nullptr;
DynamicMemoryAllocator* TheDynamicMemoryAllocator = nullptr;

// ----------------------------------------------------------------------------
// CHECKPOINTABLE
// ----------------------------------------------------------------------------

#ifdef MEMORYPOOL_CHECKPOINTING
Checkpointable::Checkpointable() :
  m_firstCheckpointInfo(nullptr),
  m_cpiEverFailed(false)
{
}

Checkpointable::~Checkpointable()
{
  m_firstCheckpointInfo = nullptr;
  m_cpiEverFailed = false;
}

BlockCheckpointInfo* Checkpointable::debugAddCheckpointInfo(
  const char* /*debugLiteralTagString*/,
  Int /*allocCheckpoint*/,
  Int /*blockSize*/)
{
  m_cpiEverFailed = false;
  return nullptr;
}

void Checkpointable::debugCheckpointReport(Int /*flags*/, Int /*startCheckpoint*/, Int /*endCheckpoint*/, const char* /*poolName*/)
{
}

void Checkpointable::debugResetCheckpoints()
{
  m_firstCheckpointInfo = nullptr;
}
#endif

// ----------------------------------------------------------------------------
// MEMORYPOOL
// ----------------------------------------------------------------------------

MemoryPool::MemoryPool() :
  m_factory(nullptr),
  m_nextPoolInFactory(nullptr),
  m_poolName(""),
  m_allocationSize(0),
  m_initialAllocationCount(0),
  m_overflowAllocationCount(0),
  m_usedBlocksInPool(0),
  m_totalBlocksInPool(0),
  m_peakUsedBlocksInPool(0),
  m_firstBlob(nullptr),
  m_lastBlob(nullptr),
  m_firstBlobWithFreeBlocks(nullptr)
{
}

MemoryPool::~MemoryPool()
{
}

void MemoryPool::init(MemoryPoolFactory* factory, const char* poolName, Int allocationSize, Int initialAllocationCount, Int overflowAllocationCount)
{
  m_factory = factory;
  m_poolName = poolName ? poolName : "";
  m_allocationSize = roundUpMemBound(allocationSize);
  m_initialAllocationCount = initialAllocationCount;
  m_overflowAllocationCount = overflowAllocationCount;
  m_usedBlocksInPool = 0;
  m_totalBlocksInPool = 0;
  m_peakUsedBlocksInPool = 0;
  m_firstBlob = nullptr;
  m_lastBlob = nullptr;
  m_firstBlobWithFreeBlocks = nullptr;
}

void MemoryPool::addToList(MemoryPool** pHead)
{
  m_nextPoolInFactory = *pHead;
  *pHead = this;
}

void MemoryPool::removeFromList(MemoryPool** pHead)
{
  if (!pHead)
    return;

  if (*pHead == this)
  {
    *pHead = m_nextPoolInFactory;
    m_nextPoolInFactory = nullptr;
    return;
  }

  for (MemoryPool* p = *pHead; p; p = p->m_nextPoolInFactory)
  {
    if (p->m_nextPoolInFactory == this)
    {
      p->m_nextPoolInFactory = m_nextPoolInFactory;
      m_nextPoolInFactory = nullptr;
      return;
    }
  }
}

void* MemoryPool::allocateBlockDoNotZeroImplementation(DECLARE_LITERALSTRING_ARG1)
{
  ScopedCriticalSection scopedCriticalSection(TheMemoryPoolCriticalSection);

  void* p = sysAllocateDoNotZero(m_allocationSize);

#ifdef MEMORYPOOL_DEBUG
  sysSetTag(p, debugLiteralTagString);
  if (m_factory)
    m_factory->adjustTotals(debugLiteralTagString, m_allocationSize, m_allocationSize);
#endif

  ++m_usedBlocksInPool;
  ++m_totalBlocksInPool;
  if (m_peakUsedBlocksInPool < m_usedBlocksInPool)
    m_peakUsedBlocksInPool = m_usedBlocksInPool;

  return p;
}

void* MemoryPool::allocateBlockImplementation(DECLARE_LITERALSTRING_ARG1)
{
  void* p = allocateBlockDoNotZeroImplementation(PASS_LITERALSTRING_ARG1);
  ::memset(p, 0, m_allocationSize);
  return p;
}

void MemoryPool::freeBlock(void* pBlockPtr)
{
  if (!pBlockPtr)
    return;

  ScopedCriticalSection scopedCriticalSection(TheMemoryPoolCriticalSection);

#ifdef MEMORYPOOL_DEBUG
  const char* tagString = sysGetTag(pBlockPtr);
  if (m_factory)
    m_factory->adjustTotals(tagString, -m_allocationSize, -m_allocationSize);
#endif

  sysFree(pBlockPtr);

  if (m_usedBlocksInPool > 0)
    --m_usedBlocksInPool;
  if (m_totalBlocksInPool > 0)
    --m_totalBlocksInPool;
}

Int MemoryPool::countBlobsInPool()
{
  return 0;
}

Int MemoryPool::releaseEmpties()
{
  return 0;
}

void MemoryPool::reset()
{
  // No retained blobs anymore.
  m_usedBlocksInPool = 0;
  m_totalBlocksInPool = 0;
  m_firstBlob = nullptr;
  m_lastBlob = nullptr;
  m_firstBlobWithFreeBlocks = nullptr;
}

#ifdef MEMORYPOOL_DEBUG
void MemoryPool::debugMemoryVerifyPool()
{
}

Bool MemoryPool::debugIsBlockInPool(void* /*pBlock*/)
{
  return false;
}


#endif

#ifdef MEMORYPOOL_CHECKPOINTING
void MemoryPool::debugCheckpointReport(Int /*flags*/, Int /*startCheckpoint*/, Int /*endCheckpoint*/)
{
}

void MemoryPool::debugResetCheckpoints()
{
}
#endif

// ----------------------------------------------------------------------------
// DYNAMICMEMORYALLOCATOR
// ----------------------------------------------------------------------------

DynamicMemoryAllocator::DynamicMemoryAllocator() :
  m_factory(nullptr),
  m_nextDmaInFactory(nullptr),
  m_numPools(0),
  m_usedBlocksInDma(0),
  m_rawBlocks(nullptr)
{
  for (Int i = 0; i < MAX_DYNAMICMEMORYALLOCATOR_SUBPOOLS; ++i)
    m_pools[i] = nullptr;
}

void DynamicMemoryAllocator::init(MemoryPoolFactory* factory, Int /*numSubPools*/, const PoolInitRec /*pParms*/[])
{
  m_factory = factory;
  m_numPools = 0;
  m_usedBlocksInDma = 0;
  m_rawBlocks = nullptr;

  for (Int i = 0; i < MAX_DYNAMICMEMORYALLOCATOR_SUBPOOLS; ++i)
    m_pools[i] = nullptr;
}

DynamicMemoryAllocator::~DynamicMemoryAllocator()
{
  DEBUG_ASSERTCRASH(m_usedBlocksInDma == 0, ("destroying a nonempty dma"));
}

MemoryPool* DynamicMemoryAllocator::findPoolForSize(Int /*allocSize*/)
{
  return nullptr;
}

void DynamicMemoryAllocator::addToList(DynamicMemoryAllocator** pHead)
{
  m_nextDmaInFactory = *pHead;
  *pHead = this;
}

void DynamicMemoryAllocator::removeFromList(DynamicMemoryAllocator** pHead)
{
  if (!pHead)
    return;

  if (*pHead == this)
  {
    *pHead = m_nextDmaInFactory;
    m_nextDmaInFactory = nullptr;
    return;
  }

  for (DynamicMemoryAllocator* d = *pHead; d; d = d->m_nextDmaInFactory)
  {
    if (d->m_nextDmaInFactory == this)
    {
      d->m_nextDmaInFactory = m_nextDmaInFactory;
      m_nextDmaInFactory = nullptr;
      return;
    }
  }
}


void* DynamicMemoryAllocator::allocateBytesDoNotZero(Int numBytes, const char* debugLiteralTagString)
{
  ScopedCriticalSection scopedCriticalSection(TheMemoryPoolCriticalSection);

  void* p = sysAllocateDoNotZero(numBytes);

#ifdef MEMORYPOOL_DEBUG
  sysSetTag(p, debugLiteralTagString);
  if (m_factory)
    m_factory->adjustTotals(debugLiteralTagString, numBytes, numBytes);
  theTotalDMA += numBytes;
  if (thePeakDMA < theTotalDMA)
    thePeakDMA = theTotalDMA;
#endif

  ++m_usedBlocksInDma;

  return p;
}

void* DynamicMemoryAllocator::allocateBytes(Int numBytes, const char* debugLiteralTagString)
{
  void* p = allocateBytesDoNotZero(numBytes, debugLiteralTagString);
  return p;
}

void DynamicMemoryAllocator::freeBytes(void* p)
{
  if (!p)
    return;

  ScopedCriticalSection scopedCriticalSection(TheMemoryPoolCriticalSection);

  const Int size = (Int)sysAllocationSize(p);

#ifdef MEMORYPOOL_DEBUG
  const char* tagString = sysGetTag(p);
  if (m_factory)
    m_factory->adjustTotals(tagString, -size, -size);
  theTotalDMA -= size;
#endif

  sysFree(p);

  if (m_usedBlocksInDma > 0)
    --m_usedBlocksInDma;
}

Int DynamicMemoryAllocator::getActualAllocationSize(Int numBytes)
{
  return numBytes;
}

void DynamicMemoryAllocator::reset()
{
  m_usedBlocksInDma = 0;
}

#ifdef MEMORYPOOL_DEBUG
Bool DynamicMemoryAllocator::debugIsPoolInDma(MemoryPool* /*pool*/)
{
  return false;
}

Bool DynamicMemoryAllocator::debugIsBlockInDma(void* /*pBlockPtr*/)
{
  return false;
}

const char* DynamicMemoryAllocator::debugGetBlockTagString(void* pBlockPtr)
{
  return sysGetTag(pBlockPtr);
}

void DynamicMemoryAllocator::debugMemoryVerifyDma()
{
}

Int DynamicMemoryAllocator::debugCalcRawBlockBytes(Int* pNumRawBlocks)
{
  if (pNumRawBlocks)
    *pNumRawBlocks = 0;
  return 0;
}

Int DynamicMemoryAllocator::debugDmaReportLeaks()
{
  return 0;
}
#endif

#ifdef MEMORYPOOL_CHECKPOINTING
void DynamicMemoryAllocator::debugCheckpointReport(Int /*flags*/, Int /*startCheckpoint*/, Int /*endCheckpoint*/)
{
}

void DynamicMemoryAllocator::debugResetCheckpoints()
{
}
#endif

// ----------------------------------------------------------------------------
// MEMORYPOOLFACTORY
// ----------------------------------------------------------------------------

MemoryPoolFactory::MemoryPoolFactory() :
  m_firstPoolInFactory(nullptr),
  m_firstDmaInFactory(nullptr)
#ifdef MEMORYPOOL_CHECKPOINTING
  , m_curCheckpoint(0)
#endif
#ifdef MEMORYPOOL_DEBUG
  , m_usedBytes(0)
  , m_physBytes(0)
  , m_peakUsedBytes(0)
  , m_peakPhysBytes(0)
#endif
{
#ifdef MEMORYPOOL_DEBUG
  for (int i = 0; i < MAX_SPECIAL_USED; ++i)
  {
    m_usedBytesSpecial[i] = 0;
    m_usedBytesSpecialPeak[i] = 0;
    m_physBytesSpecial[i] = 0;
    m_physBytesSpecialPeak[i] = 0;
  }
#endif
}

MemoryPoolFactory::~MemoryPoolFactory()
{
  while (m_firstDmaInFactory)
  {
    destroyDynamicMemoryAllocator(m_firstDmaInFactory);
  }

  while (m_firstPoolInFactory)
  {
    destroyMemoryPool(m_firstPoolInFactory);
  }
}

void MemoryPoolFactory::init()
{
#ifdef MEMORYPOOL_DEBUG
  m_usedBytes = 0;
  m_physBytes = 0;
  m_peakUsedBytes = 0;
  m_peakPhysBytes = 0;
#   ifdef USE_FILLER_VALUE
  calcFillerValue(0);
#   endif
#endif
#ifdef MEMORYPOOL_CHECKPOINTING
  m_curCheckpoint = 0;
#endif
}

MemoryPool* MemoryPoolFactory::createMemoryPool(const PoolInitRec* rec)
{
  DEBUG_ASSERTCRASH(rec != nullptr, ("null pool init rec"));
  return createMemoryPool(rec->poolName, rec->allocationSize, rec->initialAllocationCount, rec->overflowAllocationCount);
}

MemoryPool* MemoryPoolFactory::createMemoryPool(const char* poolName, Int allocationSize, Int initialAllocationCount, Int overflowAllocationCount)
{
  MemoryPool* pool = findMemoryPool(poolName);
  if (pool)
    return pool;

  userMemoryAdjustPoolSize(poolName, initialAllocationCount, overflowAllocationCount);

  void* raw = ::malloc(sizeof(MemoryPool));
  if (!raw)
    throw ERROR_OUT_OF_MEMORY;

  ::memset(raw, 0, sizeof(MemoryPool));
  pool = new (raw) MemoryPool;

  pool->init(this, poolName, allocationSize, initialAllocationCount, overflowAllocationCount);
  pool->addToList(&m_firstPoolInFactory);
  return pool;
}

MemoryPool* MemoryPoolFactory::findMemoryPool(const char* poolName)
{
  for (MemoryPool* pool = m_firstPoolInFactory; pool; pool = pool->getNextPoolInList())
  {
    if (strcmp(poolName, pool->getPoolName()) == 0)
      return pool;
  }
  return nullptr;
}

void MemoryPoolFactory::destroyMemoryPool(MemoryPool* pMemoryPool)
{
  if (!pMemoryPool)
    return;

  pMemoryPool->removeFromList(&m_firstPoolInFactory);
  pMemoryPool->~MemoryPool();
  ::free(pMemoryPool);
}

DynamicMemoryAllocator* MemoryPoolFactory::createDynamicMemoryAllocator(Int numSubPools, const PoolInitRec pParms[])
{
  void* raw = ::malloc(sizeof(DynamicMemoryAllocator));
  if (!raw)
    throw ERROR_OUT_OF_MEMORY;

  ::memset(raw, 0, sizeof(DynamicMemoryAllocator));
  DynamicMemoryAllocator* dma = new (raw) DynamicMemoryAllocator;

  dma->init(this, numSubPools, pParms);
  dma->addToList(&m_firstDmaInFactory);
  return dma;
}

void MemoryPoolFactory::destroyDynamicMemoryAllocator(DynamicMemoryAllocator* dma)
{
  if (!dma)
    return;

  dma->removeFromList(&m_firstDmaInFactory);
  dma->~DynamicMemoryAllocator();
  ::free(dma);
}

void MemoryPoolFactory::reset()
{
#ifdef MEMORYPOOL_CHECKPOINTING
  debugResetCheckpoints();
#endif

  for (MemoryPool* pool = m_firstPoolInFactory; pool; pool = pool->getNextPoolInList())
    pool->reset();

  for (DynamicMemoryAllocator* dma = m_firstDmaInFactory; dma; dma = dma->getNextDmaInList())
    dma->reset();

#ifdef MEMORYPOOL_DEBUG
  m_usedBytes = 0;
  m_physBytes = 0;
  m_peakUsedBytes = 0;
  m_peakPhysBytes = 0;
  for (int i = 0; i < MAX_SPECIAL_USED; ++i)
  {
    m_usedBytesSpecial[i] = 0;
    m_usedBytesSpecialPeak[i] = 0;
    m_physBytesSpecial[i] = 0;
    m_physBytesSpecialPeak[i] = 0;
  }
#endif
}

#ifdef MEMORYPOOL_DEBUG
static const char* s_specialPrefixes[MAX_SPECIAL_USED] =
{
    "Misc",
    "W3D_",
    "W3A_",
    "STL_",
    "STR_",
    nullptr
};

void MemoryPoolFactory::adjustTotals(const char* tagString, Int usedDelta, Int physDelta)
{
  m_usedBytes += usedDelta;
  m_physBytes += physDelta;

  if (m_peakUsedBytes < m_usedBytes)
    m_peakUsedBytes = m_usedBytes;
  if (m_peakPhysBytes < m_physBytes)
    m_peakPhysBytes = m_physBytes;

  int found = 0;
  if (tagString)
  {
    for (int i = 1; s_specialPrefixes[i] != nullptr; ++i)
    {
      if (strncmp(tagString, s_specialPrefixes[i], strlen(s_specialPrefixes[i])) == 0)
      {
        found = i;
        break;
      }
    }
  }

  m_usedBytesSpecial[found] += usedDelta;
  m_physBytesSpecial[found] += physDelta;
  if (m_usedBytesSpecialPeak[found] < m_usedBytesSpecial[found])
    m_usedBytesSpecialPeak[found] = m_usedBytesSpecial[found];
  if (m_physBytesSpecialPeak[found] < m_physBytesSpecial[found])
    m_physBytesSpecialPeak[found] = m_physBytesSpecial[found];
}

void MemoryPoolFactory::debugSetInitFillerIndex(Int index)
{
#ifdef USE_FILLER_VALUE
  if (index < 0 || index >= MAX_INIT_FILLER_COUNT)
    index = 0;
  calcFillerValue(index);
#endif
}

void MemoryPoolFactory::debugMemoryVerify()
{
}

Bool MemoryPoolFactory::debugIsBlockInAnyPool(void* /*pBlock*/)
{
  return false;
}

const char* MemoryPoolFactory::debugGetBlockTagString(void* pBlockPtr)
{
  return sysGetTag(pBlockPtr);
}
#endif

#ifdef MEMORYPOOL_CHECKPOINTING
Int MemoryPoolFactory::debugSetCheckpoint()
{
  return ++m_curCheckpoint;
}

void MemoryPoolFactory::debugResetCheckpoints()
{
  m_curCheckpoint = 0;
}
#endif

void MemoryPoolFactory::memoryPoolUsageReport(const char* filename, FILE* appendToFileInstead)
{
#ifdef MEMORYPOOL_DEBUG
  FILE* f = appendToFileInstead;
  if (!f)
  {
    char tmp[256];
    strlcpy(tmp, filename, ARRAY_SIZE(tmp));
    strlcat(tmp, ".csv", ARRAY_SIZE(tmp));
    f = fopen(tmp, "w");
  }

  if (!f)
    return;

  fprintf(f, "usedBytes,physBytes,peakUsedBytes,peakPhysBytes\n");
  fprintf(f, "%d,%d,%d,%d\n", m_usedBytes, m_physBytes, m_peakUsedBytes, m_peakPhysBytes);

  if (!appendToFileInstead)
    fclose(f);
#endif
}

// ----------------------------------------------------------------------------
// GLOBAL NEW / DELETE
// ----------------------------------------------------------------------------

void* operator new(size_t s)
{
  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before calling global operator new"));
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, "operator new");
}

void* operator new[](size_t s)
{
  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before calling global operator new[]"));
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, "operator new[]");
}

void operator delete(void* p) noexcept
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void operator delete[](void* p) noexcept
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void* operator new(size_t s, const char* tag)
{
  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before calling tagged operator new"));
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, tag ? tag : "operator new(tag)");
}

void* operator new[](size_t s, const char* tag)
{
  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before calling tagged operator new[]"));
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, tag ? tag : "operator new[](tag)");
}

void operator delete(void* p, const char* /*tag*/)
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void operator delete[](void* p, const char* /*tag*/)
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void operator delete(void* p, const char*, int)
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void operator delete[](void* p, const char*, int)
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

#ifdef MEMORYPOOL_OVERRIDE_MALLOC
void* calloc(size_t a, size_t b)
{
  ++theLinkTester;
  preMainInitMemoryManager();
  return TheDynamicMemoryAllocator->allocateBytes((Int)(a * b), "calloc");
}

void free(void* p)
{
  ++theLinkTester;
  if (!TheDynamicMemoryAllocator)
    return;
  TheDynamicMemoryAllocator->freeBytes(p);
}

void* malloc(size_t a)
{
  ++theLinkTester;
  preMainInitMemoryManager();

  // malloc must now also hand back zeroed memory.
  return TheDynamicMemoryAllocator->allocateBytes((Int)a, "malloc");
}

void* realloc(void* p, size_t s)
{
  if (!p)
    return malloc(s);

  if (s == 0)
  {
    free(p);
    return nullptr;
  }

  const size_t oldSize = sysAllocationSize(p);
  void* newPtr = malloc(s);
  if (!newPtr)
    throw ERROR_OUT_OF_MEMORY;

  const size_t copySize = oldSize < s ? oldSize : s;
  memcpy(newPtr, p, copySize);

  // Ensure any newly-grown tail is zero.
  if (s > copySize)
    ::memset((Byte*)newPtr + copySize, 0, s - copySize);

  free(p);
  return newPtr;
}
#endif

// ----------------------------------------------------------------------------
// MEMORY MANAGER LIFETIME
// ----------------------------------------------------------------------------

void initMemoryManager()
{
  if (TheMemoryPoolFactory == nullptr)
  {
    Int numSubPools = 0;
    const PoolInitRec* pParms = nullptr;
    userMemoryManagerGetDmaParms(&numSubPools, &pParms);

    void* raw = ::malloc(sizeof(MemoryPoolFactory));
    if (!raw)
      throw ERROR_OUT_OF_MEMORY;

    ::memset(raw, 0, sizeof(MemoryPoolFactory));
    TheMemoryPoolFactory = new (raw) MemoryPoolFactory;

    TheMemoryPoolFactory->init();
    TheDynamicMemoryAllocator = TheMemoryPoolFactory->createDynamicMemoryAllocator(numSubPools, pParms);
    userMemoryManagerInitPools();
    thePreMainInitFlag = false;

    DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
    DEBUG_LOG(("*** Initialized the Memory Manager"));
  }
  else
  {
    if (!thePreMainInitFlag)
      DEBUG_CRASH(("Memory Manager is already initialized"));
  }

  theMainInitFlag = true;
}

Bool isMemoryManagerOfficiallyInited()
{
  return theMainInitFlag;
}

static void preMainInitMemoryManager()
{
  if (TheMemoryPoolFactory == nullptr)
  {
    Int numSubPools = 0;
    const PoolInitRec* pParms = nullptr;
    userMemoryManagerGetDmaParms(&numSubPools, &pParms);

    TheMemoryPoolFactory = new (::malloc(sizeof(MemoryPoolFactory))) MemoryPoolFactory;
    if (!TheMemoryPoolFactory)
      throw ERROR_OUT_OF_MEMORY;

    TheMemoryPoolFactory->init();
    TheDynamicMemoryAllocator = TheMemoryPoolFactory->createDynamicMemoryAllocator(numSubPools, pParms);
    userMemoryManagerInitPools();
    thePreMainInitFlag = true;

    DEBUG_INIT(DEBUG_FLAGS_DEFAULT);
    DEBUG_LOG(("*** Initialized the Memory Manager prior to main!"));
  }
}

void shutdownMemoryManager()
{
  if (thePreMainInitFlag)
  {
#ifdef MEMORYPOOL_DEBUG
    DEBUG_LOG(("*** Memory Manager was inited prior to main -- skipping shutdown!"));
#endif
    return;
  }

  if (TheDynamicMemoryAllocator)
  {
    if (TheMemoryPoolFactory)
      TheMemoryPoolFactory->destroyDynamicMemoryAllocator(TheDynamicMemoryAllocator);
    TheDynamicMemoryAllocator = nullptr;
  }

  if (TheMemoryPoolFactory)
  {
    TheMemoryPoolFactory->~MemoryPoolFactory();
    ::free(TheMemoryPoolFactory);
    TheMemoryPoolFactory = nullptr;
  }

#ifdef MEMORYPOOL_DEBUG
  DEBUG_LOG(("Peak system allocation was %d bytes", thePeakSystemAllocationInBytes));
#endif
}

// ============================================================================
// Missing compatibility shims
// ============================================================================

struct W3DMemPoolShim
{
  const char* name;
  Int defaultSize;
};

#ifdef MEMORYPOOL_DEBUG
void DynamicMemoryAllocator::debugIgnoreLeaksForThisBlock(void* p)
{
  // In the simplified OS-backed allocator we do not retain per-block metadata
  // beyond the allocation header tag, so this is a no-op compatibility shim.
  // Intentionally ignore p.
  (void)p;
}
#endif

void* createW3DMemPool(const char* name, int size)
{
  W3DMemPoolShim* pool = (W3DMemPoolShim*)::malloc(sizeof(W3DMemPoolShim));
  if (!pool)
    throw ERROR_OUT_OF_MEMORY;

  pool->name = name ? name : "W3DMemPool";
  pool->defaultSize = size;
  return pool;
}

void* allocateFromW3DMemPool(void* poolPtr, int size, const char* file, int line)
{
  (void)line;

  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("memory manager not initialized"));

  const W3DMemPoolShim* pool = (const W3DMemPoolShim*)poolPtr;

  const char* tag = nullptr;
  if (file && *file)
  {
    tag = file;
  }
  else if (pool && pool->name)
  {
    tag = pool->name;
  }
  else
  {
    tag = "allocateFromW3DMemPool";
  }

  return TheDynamicMemoryAllocator->allocateBytesDoNotZero(size, tag);
}

void* allocateFromW3DMemPool(void* poolPtr, int size)
{
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("memory manager not initialized"));

  const W3DMemPoolShim* pool = (const W3DMemPoolShim*)poolPtr;
  const char* tag = (pool && pool->name) ? pool->name : "allocateFromW3DMemPool";

  return TheDynamicMemoryAllocator->allocateBytesDoNotZero(size, tag);
}

#ifdef MEMORYPOOL_DEBUG
void MemoryPoolFactory::debugMemoryReport(int dumpAllLeaks, int /*unused1*/, int /*unused2*/, FILE* f)
{
  FILE* out = f ? f : stderr;

  fprintf(out, "================ Memory Report ================\n");
  fprintf(out, "Used Bytes      : %d\n", m_usedBytes);
  fprintf(out, "Physical Bytes  : %d\n", m_physBytes);
  fprintf(out, "Peak Used Bytes : %d\n", m_peakUsedBytes);
  fprintf(out, "Peak Phys Bytes : %d\n", m_peakPhysBytes);
  fprintf(out, "System Peak     : %d\n", thePeakSystemAllocationInBytes);
  fprintf(out, "DMA Current     : %d\n", theTotalDMA);
  fprintf(out, "DMA Peak        : %d\n", thePeakDMA);
  fprintf(out, "Large Blocks    : %d\n", theTotalLargeBlocks);
  fprintf(out, "Peak Large Blks : %d\n", thePeakLargeBlocks);
  fprintf(out, "===============================================\n");

  if (dumpAllLeaks)
  {
    fprintf(out, "Leak reporting is limited in the simplified OS-backed allocator.\n");
    fprintf(out, "Per-allocation historical tracking from the original pool/blob system is not retained.\n");
  }
}
#else
void MemoryPoolFactory::debugMemoryReport(int /*dumpAllLeaks*/, int /*unused1*/, int /*unused2*/, FILE* /*f*/)
{
}
#endif
void freeFromW3DMemPool(void* poolPtr, void* p)
{
  (void)poolPtr;

  if (!p)
    return;

  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("memory manager not initialized"));
  TheDynamicMemoryAllocator->freeBytes(p);
}

void* __cdecl operator new(size_t s, const char* file, int line)
{
  (void)line;

  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before file/line operator new"));

  const char* tag = (file && *file) ? file : "operator new(file,line)";
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, tag);
}

void* __cdecl operator new[](size_t s, const char* file, int line)
{
  (void)line;

  ++theLinkTester;
  preMainInitMemoryManager();
  DEBUG_ASSERTCRASH(TheDynamicMemoryAllocator != nullptr, ("must init memory manager before file/line operator new[]"));

  const char* tag = (file && *file) ? file : "operator new[](file,line)";
  return TheDynamicMemoryAllocator->allocateBytesDoNotZero((Int)s, tag);
}



