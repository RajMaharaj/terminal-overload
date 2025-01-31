// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _TFIXEDSIZEDEQUE_H_
#define _TFIXEDSIZEDEQUE_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif


/// A double-ended queue with a fixed number of entries set at
/// construction time.  Implemented as an array.
///
/// @param T Element type of the queue.
template< typename T >
class FixedSizeDeque
{
   public:

      /// Type for elements stored in the deque.
      typedef T ValueType;

   protected:

      ///
      U32 mBufferSize;

      ///
      U32 mSize;

      /// Index
      U32 mStart;

      ///
      U32 mEnd;

      ///
      ValueType* mBuffer;

   public:

      ///
      FixedSizeDeque( U32 bufferSize )
         : mBufferSize( bufferSize ), mSize( 0 ), mStart( 0 ), mEnd( 0 )
      {
         // Don't use new() so we don't get a buffer full of instances.
         mBuffer = ( ValueType* ) dMalloc( sizeof( ValueType ) * bufferSize );
      }
      
      ~FixedSizeDeque()
      {
         // Destruct remaining instances.

         while( mSize )
         {
            destructInPlace( &mBuffer[ mStart ] );
            mStart ++;
            if( mStart == mBufferSize )
               mStart = 0;
            mSize --;
         }

         // Free buffer.
         dFree( mBuffer );
      }

      ///
      bool isEmpty() const { return !size(); }

      ///
      U32 size() const
      {
         return mSize;
      }

      ///
      U32 capacity() const
      {
         return ( mBufferSize - size() );
      }
      
      ///
      void clear()
      {
         while( size() )
            popFront();
      }

      /// Return the leftmost value in the queue.
      ValueType front() const
      {
         AssertFatal( !isEmpty(), "FixedSizeDeque::front() - queue is empty" );
         return mBuffer[ mStart ];
      }

      /// Return the rightmost value in the queue.
      ValueType back() const
      {
         AssertFatal( !isEmpty(), "FixedSizeDeque::back() - queue is empty" );
         if( !mEnd )
            return mBuffer[ mBufferSize - 1 ];
         else
            return mBuffer[ mEnd - 1 ];
      }

      /// Prepend "value" to the left end of the queue.
      void pushFront( const ValueType& value )
      {
         AssertFatal( capacity() != 0, "FixedSizeDeque::pushFront() - queue is full" );
         if( mStart == 0 )
            mStart = mBufferSize - 1;
         else
            mStart --;
         mBuffer[ mStart ] = value;
         mSize ++;
      }

      /// Append "value" to the right end of the queue.
      void pushBack( const ValueType& value )
      {
         AssertFatal( capacity() != 0, "FixedSizeDeque::pushBack() - queue is full" );
         mBuffer[ mEnd ] = value;
         mEnd ++;
         if( mEnd == mBufferSize )
            mEnd = 0;
         mSize ++;
      }

      /// Remove and return the leftmost value in the queue.
      ValueType popFront()
      {
         AssertFatal( !isEmpty(), "FixedSizeDeque::popFront() - queue is empty" );
         ValueType value = mBuffer[ mStart ];
         destructInPlace( &mBuffer[ mStart ] );
         
         mStart ++;
         if( mStart == mBufferSize )
            mStart = 0;

         mSize --;
         return value;
      }

      /// Remove and return the rightmost value in the queue.
      ValueType popBack()
      {
         AssertFatal( !isEmpty(), "FixedSizeDeque::popBack() - queue is empty" );
         
         U32 idx;
         if( !mEnd )
            idx = mBufferSize - 1;
         else
            idx = mEnd - 1;

         ValueType value = mBuffer[ idx ];
         destructInPlace( &mBuffer[ idx ] );

         mEnd = idx;

         mSize --;
         return value;
      }
};

#endif // _TFIXEDSIZEDEQUE_H_
