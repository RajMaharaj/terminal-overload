// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _DATACHUNKER_H_
#define _DATACHUNKER_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif

//----------------------------------------------------------------------------
/// Implements a chunked data allocator.
///
/// Calling new/malloc all the time is a time consuming operation. Therefore,
/// we provide the DataChunker, which allocates memory in blocks of
/// chunkSize (by default 16k, see ChunkSize, though it can be set in
/// the constructor), then doles it out as requested, in chunks of up to
/// chunkSize in size.
///
/// It will assert if you try to get more than ChunkSize bytes at a time,
/// and it deals with the logic of allocating new blocks and giving out
/// word-aligned chunks.
///
/// Note that new/free/realloc WILL NOT WORK on memory gotten from the
/// DataChunker. This also only grows (you can call freeBlocks to deallocate
/// and reset things).
class DataChunker
{
public:
   enum {
      ChunkSize = 16376 ///< Default size of each DataBlock page in the DataChunker
   };

   /// Return a pointer to a chunk of memory from a pre-allocated block.
   ///
   /// This memory goes away when you call freeBlocks.
   ///
   /// This memory is word-aligned.
   /// @param   size    Size of chunk to return. This must be less than chunkSize or else
   ///                  an assertion will occur.
   void *alloc(S32 size);

   /// Free all allocated memory blocks.
   ///
   /// This invalidates all pointers returned from alloc().
   void freeBlocks(bool keepOne = false);

   /// Initialize using blocks of a given size.
   ///
   /// One new block is allocated at constructor-time.
   ///
   /// @param   size    Size in bytes of the space to allocate for each block.
   DataChunker(S32 size=ChunkSize);
   ~DataChunker();

   /// Swaps the memory allocated in one data chunker for another.  This can be used to implement
   /// packing of memory stored in a DataChunker.
   void swap(DataChunker &d)
   {
      DataBlock *temp = d.mCurBlock;
      d.mCurBlock = mCurBlock;
      mCurBlock = temp;
   }
   
private:
   /// Block of allocated memory.
   ///
   /// <b>This has nothing to do with datablocks as used in the rest of Torque.</b>
   struct DataBlock
   {
      DataBlock* prev;
      DataBlock* next;        ///< linked list pointer to the next DataBlock for this chunker
      U8 *data;               ///< allocated pointer for the base of this page
      S32 curIndex;           ///< current allocation point within this DataBlock
      DataBlock(S32 size);
      ~DataBlock();
   };

   DataBlock*  mFirstBlock;
   DataBlock   *mCurBlock;    ///< current page we're allocating data from.  If the
                              ///< data size request is greater than the memory space currently
                              ///< available in the current page, a new page will be allocated.
   S32         mChunkSize;    ///< The size allocated for each page in the DataChunker
};

//----------------------------------------------------------------------------

template<class T>
class Chunker: private DataChunker
{
public:
   Chunker(S32 size = DataChunker::ChunkSize) : DataChunker(size) {};
   T* alloc()  { return reinterpret_cast<T*>(DataChunker::alloc(S32(sizeof(T)))); }
   void clear()  { freeBlocks(); }
};

//----------------------------------------------------------------------------
/// This class is similar to the Chunker<> class above.  But it allows for multiple
/// types of structs to be stored.  
/// CodeReview:  This could potentially go into DataChunker directly, but I wasn't sure if 
/// CodeReview:  That would be polluting it.  BTR
class MultiTypedChunker : private DataChunker
{
public:
   MultiTypedChunker(S32 size = DataChunker::ChunkSize) : DataChunker(size) {};

   /// Use like so:  MyType* t = chunker.alloc<MyType>();
   template<typename T>
   T* alloc()  { return reinterpret_cast<T*>(DataChunker::alloc(S32(sizeof(T)))); }
   void clear()  { freeBlocks(true); }
};

//----------------------------------------------------------------------------

/// Templatized data chunker class with proper construction and destruction of its elements.
///
/// DataChunker just allocates space. This subclass actually constructs/destructs the
/// elements. This class is appropriate for more complex classes.
template<class T>
class ClassChunker: private DataChunker
{
public:
   ClassChunker(S32 size = DataChunker::ChunkSize) : DataChunker(size)
   {
      mElementSize = getMax(U32(sizeof(T)), U32(sizeof(T *)));
      mFreeListHead = NULL;
   }

   /// Allocates and properly constructs in place a new element.
   T *alloc()
   {
      if(mFreeListHead == NULL)
         return constructInPlace(reinterpret_cast<T*>(DataChunker::alloc(mElementSize)));
      T* ret = mFreeListHead;
      mFreeListHead = *(reinterpret_cast<T**>(mFreeListHead));
      return constructInPlace(ret);
   }

   /// Properly destructs and frees an element allocated with the alloc method.
   void free(T* elem)
   {
      destructInPlace(elem);
      *(reinterpret_cast<T**>(elem)) = mFreeListHead;
      mFreeListHead = elem;
   }

   void freeBlocks( bool keepOne = false ) 
   { 
      DataChunker::freeBlocks( keepOne ); 
      mFreeListHead = NULL;
   }

private:
   S32   mElementSize;     ///< the size of each element, or the size of a pointer, whichever is greater
   T     *mFreeListHead;   ///< a pointer to a linked list of freed elements for reuse
};

//----------------------------------------------------------------------------

template<class T>
class FreeListChunker
{
public:
   FreeListChunker(DataChunker *inChunker)
      :  mChunker( inChunker ),
         mOwnChunker( false ),
         mFreeListHead( NULL )
   {
      mElementSize = getMax(U32(sizeof(T)), U32(sizeof(T *)));
   }

   FreeListChunker(S32 size = DataChunker::ChunkSize)
      :  mFreeListHead( NULL )
   {
      mChunker = new DataChunker( size );
      mOwnChunker = true;

      mElementSize = getMax(U32(sizeof(T)), U32(sizeof(T *)));
   }

   ~FreeListChunker()
   {
      if ( mOwnChunker )
         delete mChunker;
   }

   T *alloc()
   {
      if(mFreeListHead == NULL)
         return reinterpret_cast<T*>(mChunker->alloc(mElementSize));
      T* ret = mFreeListHead;
      mFreeListHead = *(reinterpret_cast<T**>(mFreeListHead));
      return ret;
   }

   void free(T* elem)
   {
      *(reinterpret_cast<T**>(elem)) = mFreeListHead;
      mFreeListHead = elem;
   }

   /// Allow people to free all their memory if they want.
   void freeBlocks( bool keepOne = false )
   {
      mChunker->freeBlocks( keepOne );
      mFreeListHead = NULL;
   }
   
private:
   DataChunker *mChunker;
   bool        mOwnChunker;

   S32   mElementSize;
   T     *mFreeListHead;
};


class FreeListChunkerUntyped
{
public:
   FreeListChunkerUntyped(U32 inElementSize, DataChunker *inChunker)
      :  mChunker( inChunker ),
         mOwnChunker( false ),
         mElementSize( inElementSize ),
         mFreeListHead( NULL )
   {
   }

   FreeListChunkerUntyped(U32 inElementSize, S32 size = DataChunker::ChunkSize)
      :  mElementSize( inElementSize ),
         mFreeListHead( NULL )
   {
      mChunker = new DataChunker( size );
      mOwnChunker = true;
   }

   ~FreeListChunkerUntyped()
   {
      if ( mOwnChunker )
         delete mChunker; 
   }

   void *alloc()
   {
      if(mFreeListHead == NULL)
         return mChunker->alloc(mElementSize);

      void  *ret = mFreeListHead;
      mFreeListHead = *(reinterpret_cast<void**>(mFreeListHead));
      return ret;
   }

   void free(void* elem)
   {
      *(reinterpret_cast<void**>(elem)) = mFreeListHead;
      mFreeListHead = elem;
   }

   // Allow people to free all their memory if they want.
   void freeBlocks()
   {
      mChunker->freeBlocks();

      // We have to terminate the freelist as well or else we'll run
      // into crazy unused memory.
      mFreeListHead = NULL;
   }

   U32   getElementSize() const { return mElementSize; }

private:
   DataChunker *mChunker;
   bool        mOwnChunker;

   const U32   mElementSize;
   void        *mFreeListHead;
};
#endif
