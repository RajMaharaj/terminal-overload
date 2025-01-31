// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "core/bitVector.h"


void BitVector::_resize( U32 sizeInBits, bool copyBits )
{
   if ( sizeInBits != 0 ) 
   {
      U32 newSize = calcByteSize( sizeInBits );
      if ( mByteSize < newSize ) 
      {
         U8 *newBits = new U8[newSize];
         if( copyBits )
            dMemcpy( newBits, mBits, mByteSize );

         delete [] mBits;
         mBits = newBits;
         mByteSize = newSize;
      }
   } 
   else 
   {
      delete [] mBits;
      mBits     = NULL;
      mByteSize = 0;
   }

   mSize = sizeInBits;
}

void BitVector::combineOR( const BitVector &other )
{
   AssertFatal( mSize == other.mSize, "BitVector::combineOR - Vectors differ in size!" );

   for ( U32 i=0; i < mSize; i++ )
   {
      bool b = test(i) | other.test(i);
      set( i, b );
   }
}

bool BitVector::_test( const BitVector& vector, bool all ) const
{
   AssertFatal( mByteSize == vector.mByteSize, "BitVector::_test - Vectors differ in size!" );
   AssertFatal( mByteSize % 4 == 0, "BitVector::_test - Vector not DWORD aligned!" );

   const U32 numDWORDS = mByteSize / 4;
   const U32* bits1 = reinterpret_cast< const U32* >( mBits );
   const U32* bits2 = reinterpret_cast< const U32* >( vector.mBits );

   for( U32 i = 0; i < numDWORDS; ++ i )
   {
      if( !( bits1[ i ] & bits2[ i ] ) )
         continue;
      else if( bits2[ i ] && all )
         return false;
      else
         return true;
   }

   return false;
}

bool BitVector::testAll() const
{
   const U32 remaider = mSize % 8;
   const U32 testBytes = mSize / 8;

   for ( U32 i=0; i < testBytes; i++ )
      if ( mBits[i] != 0xFF )
         return false;

   if ( remaider == 0 )
      return true;

   const U8 mask = (U8)0xFF >> ( 8 - remaider );
   return ( mBits[testBytes] & mask ) == mask; 
}

bool BitVector::testAllClear() const
{
   const U32 remaider = mSize % 8;
   const U32 testBytes = mSize / 8;

   for ( U32 i=0; i < testBytes; i++ )
      if ( mBits[i] != 0 )
         return false;

   if ( remaider == 0 )
      return true;

   const U8 mask = (U8)0xFF >> ( 8 - remaider );
   return ( mBits[testBytes] & mask ) == 0;
}
