// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _GUITEXTCTRL_H_
#define _GUITEXTCTRL_H_

#ifndef _GFONT_H_
#include "gfx/gFont.h"
#endif
#ifndef _GUITYPES_H_
#include "gui/core/guiTypes.h"
#endif
#ifndef _GUICONTAINER_H_
#include "gui/containers/guiContainer.h"
#endif

class GuiTextCtrl : public GuiContainer
{
private:
   typedef GuiContainer Parent;

public:
   enum Constants { MAX_STRING_LENGTH = 1024 };


protected:
   StringTableEntry mInitialText;
   StringTableEntry mInitialTextID;
   UTF8 mText[MAX_STRING_LENGTH + 1];
   S32 mMaxStrLen;   // max string len, must be less then or equal to 255

public:

   //creation methods
   DECLARE_CONOBJECT(GuiTextCtrl);
   DECLARE_CATEGORY( "Gui Text" );
   DECLARE_DESCRIPTION( "A control that displays a single line of text." );
   
   GuiTextCtrl();
   static void initPersistFields();

   //Parental methods
   bool onAdd();
   virtual bool onWake();

   //text methods
   virtual void setText(const char *txt = NULL);
   virtual void setTextID(S32 id);
   virtual void setTextID(const char *id);
   const char *getText() { return (const char*)mText; }

   // Text Property Accessors
   static bool setText(void *object, const char *index, const char *data) 
      { static_cast<GuiTextCtrl*>(object)->setText(data); return true; }
   static const char* getTextProperty(void* obj, const char* data) 
      { return static_cast<GuiTextCtrl*>(obj)->getText(); }


   void inspectPostApply();
   //rendering methods
   void onPreRender();
   void onRender(Point2I offset, const RectI &updateRect);
   void displayText( S32 xOffset, S32 yOffset );

   // resizing
   void autoResize();

   //Console methods
   const char *getScriptValue();
   void setScriptValue(const char *value);
};

#endif //_GUI_TEXT_CONTROL_H_
