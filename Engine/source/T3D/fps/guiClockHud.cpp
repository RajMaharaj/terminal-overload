// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"

#include "gui/core/guiControl.h"
#include "console/consoleTypes.h"
#include "T3D/shapeBase.h"
#include "gfx/gfxDrawUtil.h"
#include "console/engineAPI.h"

//-----------------------------------------------------------------------------

/// Vary basic HUD clock.
/// Displays the current simulation time offset from some base. The base time
/// is usually synchronized with the server as mission start time.  This hud
/// currently only displays minutes:seconds.
class GuiClockHud : public GuiControl
{
   typedef GuiControl Parent;

   bool     mShowFrame;
   bool     mShowFill;
   bool     mTimeReversed;

   ColorF   mFillColor;
   ColorF   mFrameColor;
   ColorF   mTextColor;

   S32      mTimeOffset;

public:
   GuiClockHud();

   void setTime(F32 newTime);
   void setReverseTime(F32 reverseTime);
   F32  getTime();

   void onRender( Point2I, const RectI &);
   static void initPersistFields();
   DECLARE_CONOBJECT( GuiClockHud );
   DECLARE_CATEGORY( "Gui Game" );
   DECLARE_DESCRIPTION( "Basic HUD clock. Displays the current simulation time offset from some base." );
};


//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT( GuiClockHud );

ConsoleDocClass( GuiClockHud,
   "@brief Basic HUD clock. Displays the current simulation time offset from some base.\n"

   "@tsexample\n"
		"\n new GuiClockHud()"
		"{\n"
		"	fillColor = \"0.0 1.0 0.0 1.0\"; // Fills with a solid green color\n"
		"	frameColor = \"1.0 1.0 1.0 1.0\"; // Solid white frame color\n"
		"	textColor = \"1.0 1.0 1.0 1.0\"; // Solid white text Color\n"
		"	showFill = \"true\";\n"
		"	showFrame = \"true\";\n"
		"};\n"
   "@endtsexample\n\n"

   "@ingroup GuiGame\n"
);

GuiClockHud::GuiClockHud()
{
   mShowFrame = mShowFill = true;
   mFillColor.set(0, 0, 0, 0.5);
   mFrameColor.set(0, 1, 0, 1);
   mTextColor.set( 0, 1, 0, 1 );

   mTimeOffset = 0;
}

void GuiClockHud::initPersistFields()
{
   addGroup("Misc");		
   addField( "showFill", TypeBool, Offset( mShowFill, GuiClockHud ), "If true, draws a background color behind the control.");
   addField( "showFrame", TypeBool, Offset( mShowFrame, GuiClockHud ), "If true, draws a frame around the control." );
   addField( "fillColor", TypeColorF, Offset( mFillColor, GuiClockHud ), "Standard color for the background of the control." );
   addField( "frameColor", TypeColorF, Offset( mFrameColor, GuiClockHud ), "Color for the control's frame." );
   addField( "textColor", TypeColorF, Offset( mTextColor, GuiClockHud ), "Color for the text on this control." );
   endGroup("Misc");

   Parent::initPersistFields();
}


//-----------------------------------------------------------------------------

void GuiClockHud::onRender(Point2I offset, const RectI &updateRect)
{
   // Background first
   if (mShowFill)
      GFX->getDrawUtil()->drawRectFill(updateRect, mFillColor);

   // Convert ms time into hours, minutes and seconds.
   S32 time = S32(getTime());
   S32 secs = time % 60;
   S32 mins = (time % 3600) / 60;

   // Currently only displays min/sec
   char buf[256];
   dSprintf(buf,sizeof(buf), "%02d:%02d",mins,secs);

   // Center the text
   offset.x += (getWidth() - mProfile->mFont->getStrWidth((const UTF8 *)buf)) / 2;
   offset.y += (getHeight() - mProfile->mFont->getHeight()) / 2;
   GFX->getDrawUtil()->setBitmapModulation(mTextColor);
   GFX->getDrawUtil()->drawText(mProfile->mFont, offset, buf);
   GFX->getDrawUtil()->clearBitmapModulation();

   // Border last
   if (mShowFrame)
      GFX->getDrawUtil()->drawRect(updateRect, mFrameColor);
}


//-----------------------------------------------------------------------------

void GuiClockHud::setReverseTime(F32 time)  
{  
   // Set the current time in seconds.  
   mTimeReversed = true;  
   mTimeOffset = S32(time * 1000) + Platform::getVirtualMilliseconds();  
}

void GuiClockHud::setTime(F32 time)
{
   // Set the current time in seconds.
   mTimeReversed = false;
   mTimeOffset = S32(time * 1000) - Platform::getVirtualMilliseconds();
}

F32 GuiClockHud::getTime()
{
   // Return elapsed time in seconds.
   if(mTimeReversed)
      return F32(mTimeOffset - Platform::getVirtualMilliseconds()) / 1000;  
   else
      return F32(mTimeOffset + Platform::getVirtualMilliseconds()) / 1000;
}

DefineEngineMethod(GuiClockHud, setTime, void, (F32 timeInSeconds),(60), "Sets the current base time for the clock.\n"
													"@param timeInSeconds Time to set the clock, in seconds (IE: 00:02 would be 120)\n"
													"@tsexample\n"
														"// Define the time, in seconds\n"
														"%timeInSeconds = 120;\n\n"
														"// Change the time on the GuiClockHud control\n"
														"%guiClockHud.setTime(%timeInSeconds);\n"
													"@endtsexample\n"
				  )
{
   object->setTime(timeInSeconds);
}

DefineEngineMethod(GuiClockHud, setReverseTime, void, (F32 timeInSeconds),(60), "@brief Sets a time for a countdown clock.\n\n"
   												"Setting the time like this will cause the clock to count backwards from the specified time.\n\n"
													"@param timeInSeconds Time to set the clock, in seconds (IE: 00:02 would be 120)\n\n"
													"@see setTime\n"
				  )
{
   object->setReverseTime(timeInSeconds);
}

DefineEngineMethod(GuiClockHud, getTime, F32, (),, "Returns the current time, in seconds.\n"
													"@return timeInseconds Current time, in seconds\n"
													"@tsexample\n"
														"// Get the current time from the GuiClockHud control\n"
														"%timeInSeconds = %guiClockHud.getTime();\n"
													"@endtsexample\n"
				  )
{
   return object->getTime();
}
