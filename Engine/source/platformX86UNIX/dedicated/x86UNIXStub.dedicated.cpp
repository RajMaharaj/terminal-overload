// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platformInput.h"
#include "platformX86UNIX/x86UNIXFont.h"
#include "platform/nativeDialogs/fileDialog.h"

#if 0
// declare stub functions
#define GL_FUNCTION(fn_return, fn_name, fn_args, fn_value) fn_return stub_##fn_name fn_args{ fn_value }
#include "platform/GLCoreFunc.h"
#include "platform/GLExtFunc.h"
#include "platform/GLUFunc.h"
#undef GL_FUNCTION

// point gl function pointers at stub functions
#define GL_FUNCTION(fn_return,fn_name,fn_args, fn_value) fn_return (*fn_name)fn_args = stub_##fn_name;
#include "platform/GLCoreFunc.h"
#include "platform/GLExtFunc.h"
#include "platform/GLUFunc.h"
#undef GL_FUNCTION

GLState gGLState;
bool  gOpenGLDisablePT                   = false;
bool  gOpenGLDisableCVA                  = false;
bool  gOpenGLDisableTEC                  = false;
bool  gOpenGLDisableARBMT                = false;
bool  gOpenGLDisableFC                   = false;
bool  gOpenGLDisableTCompress            = false;
bool  gOpenGLNoEnvColor                  = false;
float gOpenGLGammaCorrection             = 0.5;
bool  gOpenGLNoDrawArraysAlpha           = false;

// AL stubs
//#include <al/altypes.h>
//#include <al/alctypes.h>
//#define INITGUID
//#include <al/eaxtypes.h>

// Define the OpenAL and Extension Stub functions
#define AL_FUNCTION(fn_return, fn_name, fn_args, fn_value) fn_return stub_##fn_name fn_args{ fn_value }
#include <al/al_func.h>
#include <al/alc_func.h>
#include <al/eax_func.h>
#undef AL_FUNCTION
#include "platform/platformInput.h"

// Declare the OpenAL and Extension Function pointers
// And initialize them to the stub functions
#define AL_FUNCTION(fn_return,fn_name,fn_args, fn_value) fn_return (*fn_name)fn_args = stub_##fn_name;
#include <al/al_func.h>
#include <al/alc_func.h>
#include <al/eax_func.h>
#undef AL_FUNCTION

namespace Audio
{
bool OpenALDLLInit() { return false; }
void OpenALDLLShutdown() {}
}
#endif

// Platform Stubs

bool Platform::excludeOtherInstances(const char*) { return true; }

// clipboard
const char* Platform::getClipboard() { return ""; }
bool Platform::setClipboard(const char *text) { return false; }

// fs
void Platform::openFolder(const char *path) { }
void Platform::openFile(const char *path) { }

// window
bool Platform::displaySplashWindow(String path) { return false; }
void Platform::closeSplashWindow() { }


// font
PlatformFont *createPlatformFont(const char *name, U32 size, U32 charset) { return NULL; }
bool x86UNIXFont::create(const char *name, U32 size, U32 charset) { return false; }

// web
bool Platform::openWebBrowser(const char *) { return false; }

// messagebox
void Platform::AlertOK(const char *, const char *) {}
bool Platform::AlertOKCancel(const char *, const char *) { return false; }
S32  Platform::messageBox(char const*, char const*, MBButtons, MBIcons) { return 0; }
bool Platform::AlertRetry(char const*, char const*) { return false ; }
Platform::ALERT_ASSERT_RESULT Platform::AlertAssert(const char *windowTitle, const char *message)
{
    {
        if (Con::isActive() /*&& StdConsole::isEnabled()*/)
            Con::printf("AlertAssert: %s %s", windowTitle, message);
        else
            dPrintf("AlertAssert: %s %s\n", windowTitle, message);

        return ALERT_ASSERT_DEBUG;
   }
}
class PlatformWindowManager;
PlatformWindowManager * CreatePlatformWindowManager()
{
   AssertFatal(0, "");
   return NULL;
}


// file dialog
IMPLEMENT_CONOBJECT(FileDialog);
FileDialog::FileDialog() {}
FileDialog::~FileDialog() {}
bool FileDialog::Execute() { return false; }
void FileDialog::initPersistFields() { Parent::initPersistFields(); }

class FileDialogOpaqueData {};

FileDialogData::FileDialogData() {}
FileDialogData::~FileDialogData() {}

IMPLEMENT_CONOBJECT(OpenFileDialog);
OpenFileDialog::OpenFileDialog() {}
OpenFileDialog::~OpenFileDialog() {}
void OpenFileDialog::initPersistFields() { Parent::initPersistFields(); }

// Input stubs
void Input::init() {}
void Input::destroy() {}
bool Input::enable() { return false; }
void Input::disable() {}
void Input::activate() {}
void Input::deactivate() {}
U16 Input::getAscii( U16 keyCode, KEY_STATE keyState ) { return 0; }
U16 Input::getKeyCode( U16 asciiCode ) { return 0; }
bool Input::isEnabled() { return false; }
bool Input::isActive() { return false; }
void Input::process() {}
InputManager* Input::getManager() { return NULL; }
InputEvent Input::smInputEvent;
U8 Input::smModifierKeys;

bool OpenGLInit() { return false; }
