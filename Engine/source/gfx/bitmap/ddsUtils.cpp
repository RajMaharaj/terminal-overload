// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "squish/squish.h"
#include "gfx/bitmap/ddsFile.h"
#include "gfx/bitmap/ddsUtils.h"

//------------------------------------------------------------------------------

// If false is returned, from this method, the source DDS is not modified
bool DDSUtil::squishDDS( DDSFile *srcDDS, const GFXFormat dxtFormat )
{
   // Sanity check
   if( srcDDS->mBytesPerPixel != 4 )
   {
      AssertFatal( false, "Squish wants 32-bit source data" );
      return false;
   }

   // Build flags, start with fast compress
   U32 squishFlags = squish::kColourRangeFit;

   // Flag which format we are using
   switch( dxtFormat )
   {
      case GFXFormatDXT1:
         squishFlags |= squish::kDxt1;
         break;

      case GFXFormatDXT2:
      case GFXFormatDXT3:
         squishFlags |= squish::kDxt3;
         break;

      case GFXFormatDXT4:
      case GFXFormatDXT5:
         squishFlags |= squish::kDxt5;
         break;

      default:
         AssertFatal( false, "Assumption failed" );
         return false;
         break;
   }

   // We got this far, so assume we can finish (gosh I hope so)
   srcDDS->mFormat = dxtFormat;
   srcDDS->mFlags.set( DDSFile::CompressedData );

   // If this has alpha, set the flag
   if( srcDDS->mFormat == GFXFormatR8G8B8A8 )
      squishFlags |= squish::kWeightColourByAlpha;

   // The source surface is the original surface of the file
   DDSFile::SurfaceData *srcSurface = srcDDS->mSurfaces.last();

   // Create a new surface, this will be the DXT compressed surface. Once we
   // are done, we can discard the old surface, and replace it with this one.
   DDSFile::SurfaceData *newSurface = new DDSFile::SurfaceData();

   for( S32 i = 0; i < srcDDS->mMipMapCount; i++ )
   {
      const U8 *srcBits = srcSurface->mMips[i];

      const U32 mipSz = srcDDS->getSurfaceSize(i);
      U8 *dstBits = new U8[mipSz];
      newSurface->mMips.push_back( dstBits );

      PROFILE_START(SQUISH_DXT_COMPRESS);

      // Compress with Squish
      squish::CompressImage( srcBits, srcDDS->getWidth(i), srcDDS->getHeight(i), 
         dstBits, squishFlags );

      PROFILE_END();
   }

   // Now delete the source surface, and return.
   srcDDS->mSurfaces.pop_back();
   delete srcSurface;
   srcDDS->mSurfaces.push_back( newSurface );

   return true;
}

//------------------------------------------------------------------------------

void DDSUtil::swizzleDDS( DDSFile *srcDDS, const Swizzle<U8, 4> &swizzle )
{
   for( S32 i = 0; i < srcDDS->mMipMapCount; i++ )
   {
      swizzle.InPlace( srcDDS->mSurfaces.last()->mMips[i], srcDDS->getSurfaceSize( i ) );
   }
}