// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9PrimitiveBuffer.h"
#include "core/util/safeRelease.h"

void GFXD3D9PrimitiveBuffer::lock(U32 indexStart, U32 indexEnd, void **indexPtr)
{
   AssertFatal(!mLocked, "GFXD3D9PrimitiveBuffer::lock - Can't lock a primitive buffer more than once!");
   mLocked = true;
   U32 flags=0;
   switch(mBufferType)
   {
   case GFXBufferTypeStatic:
      // flags |= D3DLOCK_DISCARD;
      break;

   case GFXBufferTypeDynamic:
      // Always discard the content within a locked region.
      flags |= D3DLOCK_DISCARD;
      break;

   case GFXBufferTypeVolatile:
      // Get our range now...
      AssertFatal(indexStart == 0,                "Cannot get a subrange on a volatile buffer.");
      AssertFatal(indexEnd < MAX_DYNAMIC_INDICES, "Cannot get more than MAX_DYNAMIC_INDICES in a volatile buffer. Up the constant!");

      // Get the primtive buffer
      mVolatileBuffer = ((GFXD3D9Device*)mDevice)->mDynamicPB;

      AssertFatal( mVolatileBuffer, "GFXD3D9PrimitiveBuffer::lock - No dynamic primitive buffer was available!");

      // We created the pool when we requested this volatile buffer, so assume it exists...
      if( mVolatileBuffer->mIndexCount + indexEnd > MAX_DYNAMIC_INDICES ) 
      {
         flags |= D3DLOCK_DISCARD;
         mVolatileStart = indexStart  = 0;
         indexEnd       = indexEnd;
      }
      else 
      {
         flags |= D3DLOCK_NOOVERWRITE;
         mVolatileStart = indexStart  = mVolatileBuffer->mIndexCount;
         indexEnd                    += mVolatileBuffer->mIndexCount;
      }

      mVolatileBuffer->mIndexCount = indexEnd + 1;
      ib = mVolatileBuffer->ib;

      break;
   }

   D3D9Assert( ib->Lock(indexStart * sizeof(U16), (indexEnd - indexStart) * sizeof(U16), indexPtr, flags),
      "GFXD3D9PrimitiveBuffer::lock - Could not lock primitive buffer.");

   #ifdef TORQUE_DEBUG
   
      // Allocate a debug buffer large enough for the lock
      // plus space for over and under run guard strings.
      mLockedSize = (indexEnd - indexStart) * sizeof(U16);
      const U32 guardSize = sizeof( _PBGuardString );
      mDebugGuardBuffer = new U8[mLockedSize+(guardSize*2)];

      // Setup the guard strings.
      dMemcpy( mDebugGuardBuffer, _PBGuardString, guardSize ); 
      dMemcpy( mDebugGuardBuffer + mLockedSize + guardSize, _PBGuardString, guardSize ); 

      // Store the real lock pointer and return our debug pointer.
      mLockedBuffer = *indexPtr;
      *indexPtr = (U16*)( mDebugGuardBuffer + guardSize );

   #endif // TORQUE_DEBUG
}

