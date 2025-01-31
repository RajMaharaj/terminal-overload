// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _DXT5nm_SWIZZLE_H_
#define _DXT5nm_SWIZZLE_H_

#include "core/util/swizzle.h"
#include "core/util/byteswap.h"

class DXT5nmSwizzle : public Swizzle<U8, 4>
{
public:
   DXT5nmSwizzle() : Swizzle<U8, 4>( NULL ) {};

   virtual void InPlace( void *memory, const dsize_t size ) const
   {
      AssertFatal( size % 4 == 0, "Bad buffer size for DXT5nm Swizzle" );

      volatile U8 *u8Mem = reinterpret_cast<U8 *>( memory );

      for( S32 i = 0; i < size >> 2; i++ )
      {
         // g = garbage byte
         // Input: [X|Y|Z|g]     (rgba)
         // Output: [g|Y|0xFF|X] (bgra)
         BYTESWAP( u8Mem[0], u8Mem[3] ); // Store X in Alpha
         *u8Mem ^= *u8Mem;               // 0 the garbage bit
         u8Mem[2] |= 0xFF;               // Set Red to 1.0

         u8Mem += 4;
      }
   }

   virtual void ToBuffer( void *destination, const void *source, const dsize_t size ) const
   {
      AssertFatal( size % 4 == 0, "Bad buffer size for DXT5nm Swizzle" );

      volatile const U8 *srcU8 = reinterpret_cast<const U8 *>( source );
      volatile U8 *dstU8 = reinterpret_cast<U8 *>( destination );

      for( S32 i = 0; i < size >> 2; i++ )
      {
         // g = garbage byte
         // Input: [X|Y|Z|g]     (rgba)
         // Output: [g|Y|0xFF|X] (bgra)
         *dstU8++ = 0;                   // 0 garbage bit
         *dstU8++ = srcU8[1];            // Copy Y into G
         *dstU8++ |= 0xFF;               // Set Red to 1.0
         *dstU8++ = srcU8[0];            // Copy X into Alpha

         srcU8 += 4;
      }
   }
};

class DXT5nmSwizzleUp24t32 : public Swizzle<U8, 3>
{
public:
   DXT5nmSwizzleUp24t32() : Swizzle<U8, 3>( NULL ) {};

   virtual void InPlace( void *memory, const dsize_t size ) const
   {
      AssertISV( false, "Cannot swizzle in place a 24->32 bit swizzle." );
   }

   virtual void ToBuffer( void *destination, const void *source, const dsize_t size ) const
   {
      AssertFatal( size % 3 == 0, "Bad buffer size for DXT5nm Swizzle" );
      const S32 pixels = size / 3;

      volatile const U8 *srcU8 = reinterpret_cast<const U8 *>( source );
      volatile U8 *dstU8 = reinterpret_cast<U8 *>( destination );

      // destination better damn well be the right size
      for( S32 i = 0; i < pixels; i++ )
      {
         // g = garbage byte
         // Input: [X|Y|Z|g]     (rgba)
         // Output: [g|Y|0xFF|X] (bgra)
         *dstU8++ = 0;                   // 0 garbage bit
         *dstU8++ = srcU8[1];            // Copy Y into G
         *dstU8++ |= 0xFF;               // Set Red to 1.0
         *dstU8++ = srcU8[0];            // Copy X into Alpha

         srcU8 += 3;
      }
   }
};
#endif
