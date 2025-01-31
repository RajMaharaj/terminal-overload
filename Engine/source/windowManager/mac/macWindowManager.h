// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _MACWINDOWMANAGER_H_
#define _MACWINDOWMANAGER_H_

#include "windowManager/platformWindowMgr.h"
#include "core/util/tVector.h"

class MacWindow;

class MacWindowManager : public PlatformWindowManager
{
private:
   typedef VectorPtr<MacWindow*> WindowList;
   WindowList mWindowList;
   U32 mFadeToken;
   Delegate<bool(void)> mNotifyShutdownDelegate;
   
public:
   MacWindowManager();
   ~MacWindowManager();

   virtual void setParentWindow(void* newParent) {
   }
   
   /// Get the parent window
   virtual void* getParentWindow()
   {
      return NULL;
   }
   
   virtual PlatformWindow *createWindow(GFXDevice *device, const GFXVideoMode &mode);
   
   /// @name Desktop Queries
   /// @{
   
   /// Return the extents in window coordinates of the primary desktop
   /// area. On a single monitor system this is just the display extents.
   /// On a multi-monitor system this is the primary monitor (which Torque should
   /// launch on).
   virtual RectI getPrimaryDesktopArea();
   
   /// Populate a vector with all monitors and their extents in window space.
   virtual void getMonitorRegions(Vector<RectI> &regions);
   
   /// Retrieve the currently set desktop bit depth
   /// @return The current desktop bit depth, or -1 if an error occurred
   virtual S32 getDesktopBitDepth();
   
   /// Retrieve the currently set desktop resolution
   /// @return The current desktop bit depth, or Point2I(-1,-1) if an error occurred
   virtual Point2I getDesktopResolution();
   
   /// @}
   
   /// @name Window Lookup
   /// @{
   
   /// Get the number of Window's in this system
   virtual S32 getWindowCount();
   
   /// Populate a list with references to all the windows created from this manager.
   virtual void getWindows(VectorPtr<PlatformWindow*> &windows);
   
   /// Get a window from a device ID.
   ///
   /// @return The window associated with the specified ID, or NULL if no
   ///         match was found.
   virtual PlatformWindow *getWindowById(WindowId id);

   virtual PlatformWindow *getFirstWindow();
   virtual PlatformWindow* getFocusedWindow();
   
   /// During full-screen toggles we want to suppress ugly transition states,
   /// which we do (on Win32) by showing and hiding a full-monitor black window.
   ///
   /// This method cues the appearance of that window ("lowering the curtain").
   virtual void lowerCurtain();
   
   /// @see lowerCurtain
   ///
   /// This method removes the curtain window.
   virtual void raiseCurtain();
   
   /// @}
   /// @name Command Line Usage
   /// @{
   
   /// Process command line arguments sent to this window manager 
   /// to manipulate it's windows.  
   virtual void _processCmdLineArgs(const S32 argc, const char **argv);
   
   /// @}
   
   // static MacWindowManager* get() { return (MacWindowManager*)PlatformWindowManager::get(); }
   void _addWindow(MacWindow* window);
   void _removeWindow(MacWindow* window);
   
   void _onAppSignal(WindowId wnd, S32 event);
   
   bool onShutdown();
   bool canWindowGainFocus(MacWindow* window);
   
   
private:
   bool mIsShuttingDown;
};

#endif
