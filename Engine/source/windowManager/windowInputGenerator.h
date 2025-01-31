// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _WINDOW_INPUTGENERATOR_H_
#define _WINDOW_INPUTGENERATOR_H_

#ifndef _PLATFORMINPUT_H_
   #include "platform/platformInput.h"
#endif
#ifndef _MPOINT2_H_
   #include "math/mPoint2.h"
#endif


class IProcessInput;
class PlatformWindow;


class WindowInputGenerator
{
      bool mNotifyPosition;
      
   protected:

      PlatformWindow *mWindow;
      IProcessInput  *mInputController;
      Point2I         mLastCursorPos;
      bool            mClampToWindow;
      bool            mFocused; ///< We store this off to avoid polling the OS constantly

      ///  This is the scale factor which relates  mouse movement in pixels
      /// (one unit of mouse movement is a mickey) to units in the GUI.
      F32             mPixelsPerMickey;

      // Event Handlers
      void handleMouseButton(WindowId did, U32 modifier,  U32 action, U16 button);
      void handleMouseWheel (WindowId did, U32 modifier,  S32 wheelDeltaX, S32 wheelDeltaY);
      void handleMouseMove  (WindowId did, U32 modifier,  S32 x,      S32 y, bool isRelative);
      void handleKeyboard   (WindowId did, U32 modifier,  U32 action, U16 key);
      void handleCharInput  (WindowId did, U32 modifier,  U16 key);
      void handleAppEvent   (WindowId did, S32 event);
      void handleInputEvent (U32 deviceInst, F32 fValue, F32 fValue2, F32 fValue3, F32 fValue4, S32 iValue, U16 deviceType, U16 objType, U16 ascii, U16 objInst, U8 action, U8 modifier);

      void generateInputEvent( InputEventInfo &inputEvent );

      /// Accelerator key map
       struct AccKeyMap
       {
          void *hnd;
          String cmd;
          U32 keyCode;
          U32 modifier;
       };
       Vector <AccKeyMap> mAcceleratorMap;
      
   public:
   
      WindowInputGenerator( PlatformWindow *window );
      virtual ~WindowInputGenerator();

      void setInputController( IProcessInput *inputController ) { mInputController = inputController; };
      
      /// Returns true if the given keypress event should be send as a raw keyboard
      /// event even if it maps to a character input event.
      bool wantAsKeyboardEvent( U32 modifiers, U32 key );

    void addAcceleratorKey( void *hnd, const String &cmd, U32 keycode, U32 modifier)
    {
        AccKeyMap acc;
        acc.hnd = hnd;
        acc.cmd = cmd;
        acc.keyCode = keycode;
        acc.modifier = modifier;
        mAcceleratorMap.push_back(acc);
    }

    void removeAcceleratorKeys( void *hnd )
    {
         for( int i = 0; i < mAcceleratorMap.size(); )
         {
            if( mAcceleratorMap[i].hnd == hnd )
            {
                mAcceleratorMap.erase( i, 1 );
                continue;
            }

             ++i;
         }
    }

    void removeAcceleratorKey( void *hnd, U32 keycode, U32 modifier )
    {
         for( int i = 0; i < mAcceleratorMap.size(); ++i )
         {
            if( mAcceleratorMap[i].hnd == hnd && mAcceleratorMap[i].keyCode == keycode && mAcceleratorMap[i].modifier == modifier )
            {
                mAcceleratorMap.erase( i, 1 );
                return;
            }
         }
    }
};

#endif // _WINDOW_INPUTGENERATOR_H_