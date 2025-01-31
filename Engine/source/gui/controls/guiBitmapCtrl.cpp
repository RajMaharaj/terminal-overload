// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "gui/controls/guiBitmapCtrl.h"

#include "console/console.h"
#include "console/consoleTypes.h"
#include "console/engineAPI.h"
#include "gfx/gfxDevice.h"
#include "gfx/gfxDrawUtil.h"


IMPLEMENT_CONOBJECT(GuiBitmapCtrl);

ConsoleDocClass( GuiBitmapCtrl,
   "@brief A gui control that is used to display an image.\n\n"
   
   "The image is stretched to the constraints of the control by default. However, the control can also\n"
   "tile the image as well.\n\n"

   "The image itself is stored inside the GuiBitmapCtrl::bitmap field. The boolean value that decides\n"
   "whether the image is stretched or tiled is stored inside the GuiBitmapCtrl::wrap field.\n"
   
   "@tsexample\n"
   "// Create a tiling GuiBitmapCtrl that displays \"myImage.png\"\n"
   "%bitmapCtrl = new GuiBitmapCtrl()\n"
   "{\n"
   "   bitmap = \"myImage.png\";\n"
   "   wrap = \"true\";\n"
   "};\n"
   "@endtsexample\n\n"
   
   "@ingroup GuiControls"
);

GuiBitmapCtrl::GuiBitmapCtrl(void)
 : mBitmapName(),
   mStartPoint( 0, 0 ),
   mWrap( false )
{	
}

bool GuiBitmapCtrl::setBitmapName( void *object, const char *index, const char *data )
{
   // Prior to this, you couldn't do bitmap.bitmap = "foo.jpg" and have it work.
   // With protected console types you can now call the setBitmap function and
   // make it load the image.
   static_cast<GuiBitmapCtrl *>( object )->setBitmap( data );

   // Return false because the setBitmap method will assign 'mBitmapName' to the
   // argument we are specifying in the call.
   return false;
}

void GuiBitmapCtrl::initPersistFields()
{
   addGroup( "Bitmap" );
   
      addProtectedField( "bitmap", TypeImageFilename, Offset( mBitmapName, GuiBitmapCtrl ),
         &setBitmapName, &defaultProtectedGetFn,
         "The bitmap file to display in the control." );
      addField( "wrap",   TypeBool,     Offset( mWrap, GuiBitmapCtrl ),
         "If true, the bitmap is tiled inside the control rather than stretched to fit." );
      
   endGroup( "Bitmap" );

   Parent::initPersistFields();
}

bool GuiBitmapCtrl::onWake()
{
   if (! Parent::onWake())
      return false;
   setActive(true);
   setBitmap(mBitmapName);
   return true;
}

void GuiBitmapCtrl::onSleep()
{
   if ( !mBitmapName.equal("texhandle", String::NoCase) )
      mTextureObject = NULL;

   Parent::onSleep();
}

//-------------------------------------
void GuiBitmapCtrl::inspectPostApply()
{
   // if the extent is set to (0,0) in the gui editor and appy hit, this control will
   // set it's extent to be exactly the size of the bitmap (if present)
   Parent::inspectPostApply();

   if (!mWrap && (getExtent().x == 0) && (getExtent().y == 0) && mTextureObject)
   {
      setExtent( mTextureObject->getWidth(), mTextureObject->getHeight());
   }
}

void GuiBitmapCtrl::setBitmap( const char *name, bool resize )
{
   mBitmapName = name;
   if ( !isAwake() )
      return;

   if ( mBitmapName.isNotEmpty() )
	{
      if ( !mBitmapName.equal("texhandle", String::NoCase) )
		   mTextureObject.set( mBitmapName, &GFXDefaultGUIProfile, avar("%s() - mTextureObject (line %d)", __FUNCTION__, __LINE__) );

      // Resize the control to fit the bitmap
      if ( mTextureObject && resize )
      {
         setExtent( mTextureObject->getWidth(), mTextureObject->getHeight() );
         updateSizing();
      }
   }
   else
      mTextureObject = NULL;

   setUpdate();
}

void GuiBitmapCtrl::updateSizing()
{
   if(!getParent())
      return;
   // updates our bounds according to our horizSizing and verSizing rules
   RectI fakeBounds( getPosition(), getParent()->getExtent());
   parentResized( fakeBounds, fakeBounds);
}

void GuiBitmapCtrl::setBitmapHandle(GFXTexHandle handle, bool resize)
{
   mTextureObject = handle;

   mBitmapName = String("texhandle");

   // Resize the control to fit the bitmap
   if (resize) 
   {
      setExtent(mTextureObject->getWidth(), mTextureObject->getHeight());
      updateSizing();
   }
}

void GuiBitmapCtrl::onRender(Point2I offset, const RectI &updateRect)
{
   if (mTextureObject)
   {
      GFX->getDrawUtil()->clearBitmapModulation();
		if(mWrap)
		{
         // We manually draw each repeat because non power of two textures will 
         // not tile correctly when rendered with GFX->drawBitmapTile(). The non POT
         // bitmap will be padded by the hardware, and we'll see lots of slack
         // in the texture. So... lets do what we must: draw each repeat by itself:
 			GFXTextureObject* texture = mTextureObject;
			RectI srcRegion;
			RectI dstRegion;
			F32 xdone = ((F32)getExtent().x/(F32)texture->mBitmapSize.x)+1;
			F32 ydone = ((F32)getExtent().y/(F32)texture->mBitmapSize.y)+1;

			S32 xshift = mStartPoint.x%texture->mBitmapSize.x;
			S32 yshift = mStartPoint.y%texture->mBitmapSize.y;
			for(S32 y = 0; y < ydone; ++y)
				for(S32 x = 0; x < xdone; ++x)
				{
		 			srcRegion.set(0,0,texture->mBitmapSize.x,texture->mBitmapSize.y);
  					dstRegion.set( ((texture->mBitmapSize.x*x)+offset.x)-xshift,
								      ((texture->mBitmapSize.y*y)+offset.y)-yshift,
								      texture->mBitmapSize.x,
								      texture->mBitmapSize.y);
               GFX->getDrawUtil()->drawBitmapStretchSR(texture,dstRegion, srcRegion, GFXBitmapFlip_None, GFXTextureFilterLinear);
				}

		}
		else
      {
         RectI rect(offset, getExtent());
         GFX->getDrawUtil()->drawBitmapStretch(mTextureObject, rect, GFXBitmapFlip_None, GFXTextureFilterLinear, false);
      }
   }

   if (mProfile->mBorder || !mTextureObject)
   {
      RectI rect(offset.x, offset.y, getExtent().x, getExtent().y);
      GFX->getDrawUtil()->drawRect(rect, mProfile->mBorderColor);
   }

   renderChildControls(offset, updateRect);
}

void GuiBitmapCtrl::setValue(S32 x, S32 y)
{
   if (mTextureObject)
   {
		x += mTextureObject->getWidth() / 2;
		y += mTextureObject->getHeight() / 2;
  	}
  	while (x < 0)
  		x += 256;
  	mStartPoint.x = x % 256;

  	while (y < 0)
  		y += 256;
  	mStartPoint.y = y % 256;
}

DefineEngineMethod( GuiBitmapCtrl, setValue, void, ( S32 x, S32 y ),,
   "Set the offset of the bitmap within the control.\n"
   "@param x The x-axis offset of the image.\n"
   "@param y The y-axis offset of the image.\n")
{
   object->setValue(x, y);
}


//

static ConsoleDocFragment _sGuiBitmapCtrlSetBitmap1(
   "@brief Assign an image to the control.\n\n"
   "Child controls with resize according to their layout settings.\n"
   "@param filename The filename of the image.\n"
   "@param resize Optional parameter. If true, the GUI will resize to fit the image.",
   "GuiBitmapCtrl", // The class to place the method in; use NULL for functions.
   "void setBitmap( String filename, bool resize );" ); // The definition string.

static ConsoleDocFragment _sGuiBitmapCtrlSetBitmap2(
   "@brief Assign an image to the control.\n\n"
   "Child controls will resize according to their layout settings.\n"
   "@param filename The filename of the image.\n"
   "@param resize A boolean value that decides whether the ctrl refreshes or not.",
   "GuiBitmapCtrl", // The class to place the method in; use NULL for functions.
   "void setBitmap( String filename );" ); // The definition string.


//"Set the bitmap displayed in the control. Note that it is limited in size, to 256x256."
DefineConsoleMethod( GuiBitmapCtrl, setBitmap, void, ( const char * fileRoot, bool resize), ( false),
   "( String filename | String filename, bool resize ) Assign an image to the control.\n\n"
   "@hide" )
{
   char filename[1024];
   Con::expandScriptFilename(filename, sizeof(filename), fileRoot);
   object->setBitmap(filename, resize );
}
