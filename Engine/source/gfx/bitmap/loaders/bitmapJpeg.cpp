// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "ljpeg/jpeglib.h"

#include "core/stream/stream.h"

#include "gfx/bitmap/gBitmap.h"


static bool sReadJPG(Stream &stream, GBitmap *bitmap);
static bool sWriteJPG(GBitmap *bitmap, Stream &stream, U32 compressionLevel);

static struct _privateRegisterJPG
{
   _privateRegisterJPG()
   {
      GBitmap::Registration reg;

      reg.priority = 50;
      reg.extensions.push_back( "jpeg" );
      reg.extensions.push_back( "jpg" );

      reg.readFunc = sReadJPG;
      reg.writeFunc = sWriteJPG;

      GBitmap::sRegisterFormat( reg );
   }
} sStaticRegisterJPG;

//-------------------------------------- Replacement I/O for standard LIBjpeg
//                                        functions.  we don't wanna use
//                                        FILE*'s...
static S32 jpegReadDataFn(void *client_data, U8 *data, S32 length)
{
   Stream *stream = (Stream*)client_data;
   AssertFatal(stream != NULL, "jpegReadDataFn::No stream.");
   S32 pos = stream->getPosition();
   if (stream->read(length, data))
      return length;

   if (stream->getStatus() == Stream::EOS)
      return (stream->getPosition()-pos);
   else
      return 0;
}


//--------------------------------------
static S32 jpegWriteDataFn(void *client_data, U8 *data, S32 length)
{
   Stream *stream = (Stream*)client_data;
   AssertFatal(stream != NULL, "jpegWriteDataFn::No stream.");
   if (stream->write(length, data))
      return length;
   else
      return 0;
}


//--------------------------------------
static S32 jpegFlushDataFn(void *)
{
   // do nothing since we can't flush the stream object
   return 0;
}


//--------------------------------------
static S32 jpegErrorFn(void *client_data)
{
   Stream *stream = (Stream*)client_data;
   AssertFatal(stream != NULL, "jpegErrorFn::No stream.");
   return (stream->getStatus() != Stream::Ok);
}


//--------------------------------------
static bool sReadJPG(Stream &stream, GBitmap *bitmap)
{
   JFREAD  = jpegReadDataFn;
   JFERROR = jpegErrorFn;

   jpeg_decompress_struct cinfo;
   jpeg_error_mgr jerr;

   // We set up the normal JPEG error routines, then override error_exit.
   //cinfo.err = jpeg_std_error(&jerr.pub);
   //jerr.pub.error_exit = my_error_exit;

   // if (setjmp(jerr.setjmp_buffer))
   // {
   //    // If we get here, the JPEG code has signaled an error.
   //    // We need to clean up the JPEG object, close the input file, and return.
   //    jpeg_destroy_decompress(&cinfo);
   //    return false;
   // }


   cinfo.err = jpeg_std_error(&jerr);    // set up the normal JPEG error routines.
   cinfo.client_data = (void*)&stream;       // set the stream into the client_data

   // Now we can initialize the JPEG decompression object.
   jpeg_create_decompress(&cinfo);

   jpeg_stdio_src(&cinfo);

   // Read file header, set default decompression parameters
   jpeg_read_header(&cinfo, true);

   GFXFormat format;
   switch (cinfo.out_color_space)
   {
      case JCS_GRAYSCALE:  format = GFXFormatA8; break;
      case JCS_RGB:        format = GFXFormatR8G8B8;   break;
      default:
         jpeg_destroy_decompress(&cinfo);
         return false;
   }

   // Start decompressor
   jpeg_start_decompress(&cinfo);

   // allocate the bitmap space and init internal variables...
   bitmap->allocateBitmap(cinfo.output_width, cinfo.output_height, false, format);

   // Set up the row pointers...
   U32 rowBytes = cinfo.output_width * cinfo.output_components;

   U8* pBase = (U8*)bitmap->getBits();
   for (U32 i = 0; i < bitmap->getHeight(); i++)
   {
      JSAMPROW rowPointer = pBase + (i * rowBytes);
      jpeg_read_scanlines(&cinfo, &rowPointer, 1);
   }

   // Finish decompression
   jpeg_finish_decompress(&cinfo);

   // Release JPEG decompression object
   // This is an important step since it will release a good deal of memory.
   jpeg_destroy_decompress(&cinfo);

   // We know JPEG's don't have any transparency
   bitmap->setHasTransparency(false);

   return true;
}


//--------------------------------------------------------------------------
static bool sWriteJPG(GBitmap *bitmap, Stream &stream, U32 compressionLevel)
{
   TORQUE_UNUSED(compressionLevel); // compression level not currently hooked up

   GFXFormat   format = bitmap->getFormat();

   // JPEG format does not support transparency so any image
   // in Alpha format should be saved as a grayscale which coincides
   // with how the readJPEG function will read-in a JPEG. So the
   // only formats supported are RGB and Alpha, not RGBA.
   AssertFatal(format == GFXFormatR8G8B8 || format == GFXFormatA8,
	            "GBitmap::writeJPEG: ONLY RGB bitmap writing supported at this time.");
   if (format != GFXFormatR8G8B8 && format != GFXFormatA8)
      return false;

   // maximum image size allowed
   #define MAX_HEIGHT 4096
   if (bitmap->getHeight() > MAX_HEIGHT)
      return false;

   // Bind our own stream writing, error, and memory flush functions
   // to the jpeg library interface
   JFWRITE = jpegWriteDataFn;
   JFFLUSH = jpegFlushDataFn;
   JFERROR = jpegErrorFn;

   // Allocate and initialize our jpeg compression structure and error manager
   jpeg_compress_struct cinfo;
   jpeg_error_mgr jerr;

   cinfo.err = jpeg_std_error(&jerr);    // set up the normal JPEG error routines.
   cinfo.client_data = (void*)&stream;   // set the stream into the client_data
   jpeg_create_compress(&cinfo);         // allocates a small amount of memory

   // specify the destination for the compressed data(our stream)
   jpeg_stdio_dest(&cinfo);

   // set the image properties
   cinfo.image_width = bitmap->getWidth();           // image width
   cinfo.image_height = bitmap->getHeight();         // image height
   cinfo.input_components = bitmap->getBytesPerPixel();   // samples per pixel(RGB:3, Alpha:1)
   
   switch (format)
   {
      case GFXFormatA8:  // no alpha support in JPEG format, so turn it into a grayscale
         cinfo.in_color_space = JCS_GRAYSCALE;
         break;
      case GFXFormatR8G8B8:    // otherwise we are writing in RGB format
         cinfo.in_color_space = JCS_RGB;
         break;
      default:
         AssertFatal( false, "Format not handled in GBitmap::writeJPEG() switch" );
         break;
   }
   // use default compression params(75% compression)
   jpeg_set_defaults(&cinfo);

   // begin JPEG compression cycle
   jpeg_start_compress(&cinfo, true);

   // Set up the row pointers...
   U32 rowBytes = cinfo.image_width * cinfo.input_components;

   U8* pBase = (U8*)bitmap->getBits();
   for (U32 i = 0; i < bitmap->getHeight(); i++)
   {
      // write the image data
      JSAMPROW rowPointer = pBase + (i * rowBytes);
	   jpeg_write_scanlines(&cinfo, &rowPointer, 1);
   }

   // complete the compression cycle
   jpeg_finish_compress(&cinfo);

   // release the JPEG compression object
   jpeg_destroy_compress(&cinfo);

   // return success
   return true;
}
