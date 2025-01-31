// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _GFXTEXTUREHANDLE_H_
#define _GFXTEXTUREHANDLE_H_

#ifndef _GFXTEXTUREOBJECT_H_
#include "gfx/gfxTextureObject.h"
#endif
#ifndef _MPOINT2_H_
#include "math/mPoint2.h"
#endif


class GFXTextureProfile;


/// A reference counted handle to a texture resource.
class GFXTexHandle : public StrongRefPtr<GFXTextureObject>
{
public:
   GFXTexHandle() {}
   GFXTexHandle( GFXTextureObject* obj );
   GFXTexHandle( const GFXTexHandle &handle, const String &desc );

   // load texture
   GFXTexHandle( const String &texName, GFXTextureProfile *profile, const String &desc );
   bool set( const String &texName, GFXTextureProfile *profile, const String &desc );

   // register texture
   GFXTexHandle( GBitmap *bmp, GFXTextureProfile *profile, bool deleteBmp, const String &desc );
   bool set( GBitmap *bmp, GFXTextureProfile *profile, bool deleteBmp, const String &desc );

   GFXTexHandle( DDSFile *bmp, GFXTextureProfile *profile, bool deleteDDS, const String &desc );
   bool set( DDSFile *bmp, GFXTextureProfile *profile, bool deleteDDS, const String &desc );

   // Sized bitmap
   GFXTexHandle( U32 width, U32 height, GFXFormat format, GFXTextureProfile *profile, const String &desc, U32 numMipLevels = 1, S32 antialiasLevel = 0);
   bool set( U32 width, U32 height, GFXFormat format, GFXTextureProfile *profile, const String &desc, U32 numMipLevels = 1, S32 antialiasLevel = 0);
   bool set( U32 width, U32 height, U32 depth, void *pixels, GFXFormat format, GFXTextureProfile *profile, const String &desc, U32 numMipLevels = 1 );

   /// Returns the width and height as a point.
   Point2I getWidthHeight() const { return getPointer() ? Point2I( getPointer()->getWidth(), getPointer()->getHeight() ) : Point2I::Zero; }
   
   U32 getWidth() const    { return getPointer() ? getPointer()->getWidth()  : 0; }
   U32 getHeight() const   { return getPointer() ? getPointer()->getHeight() : 0; }
   U32 getDepth() const    { return getPointer() ? getPointer()->getDepth()  : 0; }
   GFXFormat getFormat() const { return getPointer() ? getPointer()->getFormat() : GFXFormat_COUNT; }
   
   /// Reloads the texture.
   /// @see GFXTextureManager::reloadTexture
   void refresh();

   /// Releases the texture handle.
   void free() { StrongObjectRef::set( NULL ); }
   
   GFXLockedRect *lock( U32 mipLevel = 0, RectI *inRect = NULL )
   {
      return getPointer()->lock(mipLevel, inRect); 
   }

   void unlock( U32 mipLevel = 0) 
   {
      getPointer()->unlock(mipLevel); 
   }

   // copy to bitmap.  see gfxTetureObject.h for description of what types of textures
   // can be copied into bitmaps.  returns true if successful, false otherwise
   bool copyToBmp(GBitmap* bmp) { return getPointer() ? getPointer()->copyToBmp(bmp) : false; }

   //---------------------------------------------------------------------------
   // Operator overloads
   //---------------------------------------------------------------------------
   GFXTexHandle& operator=(const GFXTexHandle &t)
   {
      StrongObjectRef::set(t.getPointer());
      return *this;
   }
   
   GFXTexHandle& operator=( GFXTextureObject *to)
   {
      StrongObjectRef::set(to);
      return *this;
   }

   bool operator==(const GFXTexHandle &t) const { return t.getPointer() == getPointer(); }
   bool operator!=(const GFXTexHandle &t) const { return t.getPointer() != getPointer(); }

   /// Returns the texture object.
   operator GFXTextureObject*() const { return getPointer(); }

   /// Returns the backing bitmap for this texture.
   GBitmap* getBitmap() { return getPointer() ? getPointer()->getBitmap() : NULL; }
   const GBitmap* getBitmap() const { return getPointer() ? getPointer()->getBitmap() : NULL; }


   /// Helper 2x2 R8G8B8A8 texture filled with 0.
   static GFXTexHandle ZERO;

   /// Helper 2x2 R8G8B8A8 texture filled with 255.
   static GFXTexHandle ONE;

   /// Helper 2x2 R8G8B8A8 normal map texture filled 
   /// with 128, 128, 255.
   static GFXTexHandle ZUP;

};


#endif // _GFXTEXTUREHANDLE_H_
