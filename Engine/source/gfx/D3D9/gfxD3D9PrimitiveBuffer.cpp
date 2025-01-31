// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "gfx/D3D9/gfxD3D9Device.h"
#include "gfx/D3D9/gfxD3D9EnumTranslate.h"
#include "gfx/D3D9/gfxD3D9PrimitiveBuffer.h"
#include "core/util/safeRelease.h"

void GFXD3D9PrimitiveBuffer::prepare()
{
	static_cast<GFXD3D9Device *>( mDevice )->_setPrimitiveBuffer(this);
}

void GFXD3D9PrimitiveBuffer::unlock()
{
   #ifdef TORQUE_DEBUG
   
      if ( mDebugGuardBuffer )
      {
         const U32 guardSize = sizeof( _PBGuardString );

         // First check the guard areas for overwrites.
         AssertFatal( dMemcmp( mDebugGuardBuffer, _PBGuardString, guardSize ) == 0,
            "GFXD3D9PrimitiveBuffer::unlock - Caught lock memory underrun!" );
         AssertFatal( dMemcmp( mDebugGuardBuffer + mLockedSize + guardSize, _PBGuardString, guardSize ) == 0,
            "GFXD3D9PrimitiveBuffer::unlock - Caught lock memory overrun!" );

         // Copy the debug content down to the real PB.
         dMemcpy( mLockedBuffer, mDebugGuardBuffer + guardSize, mLockedSize );

         // Cleanup.
         delete [] mDebugGuardBuffer;
         mDebugGuardBuffer = NULL;
         mLockedBuffer = NULL;
         mLockedSize = 0;
      }

   #endif // TORQUE_DEBUG

   ib->Unlock();
   mLocked = false;
   mIsFirstLock = false;
   mVolatileBuffer = NULL;
}

GFXD3D9PrimitiveBuffer::~GFXD3D9PrimitiveBuffer() 
{
   if( mBufferType != GFXBufferTypeVolatile )
   {
#if defined(TORQUE_OS_XENON)
      if(ib->IsSet(reinterpret_cast<GFXD3D9Device *>(mDevice)->mD3DDevice))
      {
         reinterpret_cast<GFXD3D9Device *>(mDevice)->mD3DDevice->SetIndices(NULL);
      }
#endif
      SAFE_RELEASE( ib );
   }
}

void GFXD3D9PrimitiveBuffer::zombify()
{
   if(mBufferType == GFXBufferTypeStatic)
      return;
            
   AssertFatal(!mLocked, "GFXD3D9PrimitiveBuffer::zombify - Cannot zombify a locked buffer!");

   if (mBufferType == GFXBufferTypeVolatile)
   {
      // We must null the volatile buffer else we're holding
      // a dead pointer which can be set on the device.      
      ib = NULL;
      return;
   }

   // Dynamic buffers get released.
   SAFE_RELEASE(ib);
}

void GFXD3D9PrimitiveBuffer::resurrect()
{
   if ( mBufferType != GFXBufferTypeDynamic )
      return;
      
   U32 usage = D3DUSAGE_WRITEONLY;

#ifndef TORQUE_OS_XENON
   usage |= D3DUSAGE_DYNAMIC;
#endif

   D3DPOOL pool = D3DPOOL_DEFAULT;

   D3D9Assert(static_cast<GFXD3D9Device*>(mDevice)->mD3DDevice->CreateIndexBuffer( sizeof(U16) * mIndexCount , 
        usage , GFXD3D9IndexFormat[GFXIndexFormat16], pool, &ib, 0),
        "GFXD3D9PrimitiveBuffer::resurrect - Failed to allocate an index buffer.");
}
