// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifdef TORQUE_SHARED

#ifdef WIN32

#include <windows.h>
#include <string>

extern "C"
{
   int (*torque_winmain)( HINSTANCE hInstance, HINSTANCE h, LPSTR lpszCmdLine, int nShow) = NULL;
};

bool getDllName(std::wstring& dllName, const std::wstring suffix)
{
   wchar_t filenameBuf[MAX_PATH];
   DWORD length = GetModuleFileNameW( NULL, filenameBuf, MAX_PATH );
   if(length == 0) return false;
   dllName = std::wstring(filenameBuf);
   size_t dotPos = dllName.find_last_of(L".");
   if(dotPos == std::wstring::npos)
   {
      dllName.clear();
      return false;
   }
   dllName.erase(dotPos);
   dllName += suffix + L".dll";
   return true;
}

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCommandShow)
{
   // Try to find the game DLL, which may have one of several file names.
   HMODULE hGame = NULL;
   std::wstring dllName = std::wstring();
   // The file name is the same as this executable's name, plus a suffix.
   const std::wstring dllSuffices[] = {L" DLL", L""};
   const unsigned int numSuffices = sizeof(dllSuffices) / sizeof(std::wstring);

   for (unsigned int i = 0; i < numSuffices; i++)
   {
      // Attempt to glue the suffix onto the current filename.
      if(!getDllName(dllName, dllSuffices[i]))
         continue;
      // Load the DLL at that address.
      hGame = LoadLibraryW(dllName.c_str());
      if (hGame)
         break;
   }

   if(!dllName.length())
   {
      MessageBoxW(NULL, L"Unable to find game dll", L"Error",  MB_OK|MB_ICONWARNING);
      return -1;
   }

   enum { errorSize = 4096 };
   if (!hGame)
   {
      wchar_t error[errorSize];
      _swprintf_l(error, errorSize, L"Unable to load game library: %s.  Please make sure it exists and the latest DirectX is installed.", _get_current_locale(), dllName.c_str());
      MessageBoxW(NULL, error, L"Error",  MB_OK|MB_ICONWARNING);
      return -1;
   }

   torque_winmain = (int (*)(HINSTANCE hInstance, HINSTANCE h, LPSTR lpszCmdLine, int nShow))GetProcAddress(hGame, "torque_winmain");
   if (!torque_winmain)
   {
      wchar_t error[errorSize];
      _swprintf_l(error, errorSize, L"Missing torque_winmain export in game library: %s.  Please make sure that it exists and the latest DirectX is installed.", _get_current_locale(), dllName.c_str());
      MessageBoxW(NULL, error, L"Error",  MB_OK|MB_ICONWARNING);
      return -1;
   }

   int ret = torque_winmain(hInstance, hPrevInstance, lpszCmdLine, nCommandShow);

   FreeLibrary(hGame);
   return ret;
}
#endif // WIN32


#ifdef __MACOSX__

#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <Carbon/Carbon.h>

extern "C" {

   int (*torque_macmain)(int argc, const char **argv) = 0;

}

void GetBasePath(const char** cpath, const char** cname)
{
   static char path[2049];
   static char name[2049];

   ProcessSerialNumber PSN;
   ProcessInfoRec pinfo;
   FSSpec pspec;
   FSRef fsr;
   OSStatus err;

   path[0] = 0;
   name[0] = 0;

   *cpath = path;
   *cname = name;

   // set up process serial number
   PSN.highLongOfPSN = 0;
   PSN.lowLongOfPSN = kCurrentProcess;

   // set up info block
   pinfo.processInfoLength = sizeof(pinfo);
   pinfo.processName = NULL;
   pinfo.processAppSpec = &pspec;

   // grab the vrefnum and directory
   err = GetProcessInformation(&PSN, &pinfo);
   if (! err ) {

      FSSpec fss2;

      strcpy(name, &pspec.name[1]);

      err = FSMakeFSSpec(pspec.vRefNum, pspec.parID, 0, &fss2);

      if ( ! err ) {
         err = FSpMakeFSRef(&fss2, &fsr);
         if ( ! err ) {
            err = (OSErr)FSRefMakePath(&fsr, (UInt8*)path, 2048);
         }
      }
   }
}

int main(int argc, const char **argv)
{
   void *gameBundle = 0;
   char gameBundleFilename[2049];

   const char* basePath;
   const char* appName;

   // Get the path to our app binary and the app name

   GetBasePath(&basePath, &appName);

   if (!basePath[0] || !appName[0])
      return;

   char appNameNoDebug[2049];

   strcpy(appNameNoDebug, appName);

   int i = strlen(appName);
   while (i > 0)
   {
      if (!strcmp(&appName[i], "_DEBUG"))
      {
         appNameNoDebug[i] = 0;
         break;
      }

      i--;
   }

   sprintf(gameBundleFilename, "%s.app/Contents/Frameworks/%s Bundle.bundle/Contents/MacOS/%s Bundle", appName, appNameNoDebug, appNameNoDebug);

   // first see if the current directory is set properly
   gameBundle = dlopen(gameBundleFilename, RTLD_LAZY | RTLD_LOCAL);

   if (!gameBundle)
   {
      // Couldn't load the game bundle... so, using the path to the bundle binary fix up the cwd

      if (basePath[0]) {
         chdir( basePath );
         chdir( "../../../" );
      }

      // and try again
      gameBundle = dlopen( gameBundleFilename, RTLD_LAZY | RTLD_LOCAL);
   }

   if (!gameBundle)
      return -1;

   torque_macmain = (int (*)(int argc, const char **argv)) dlsym(gameBundle, "torque_macmain");

   if (!torque_macmain)
      return -1;

   return torque_macmain(argc, argv);
}

#endif // __MACOSX

#ifdef __linux__

#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

extern "C"
{
   int (*torque_unixmain)(int argc, const char **argv) = NULL;
   void(*setExePathName)(const char *exePathName) = NULL;
}

int main(int argc, const char **argv)
{
   // assume bin name is in argv[0]
   int len = strlen(argv[0]);
   char *libName = new char[len+4]; // len + .so + NUL

   strcpy(libName, argv[0]);
   strcat(libName, ".so");

   // try to load the game lib
   void *gameLib = dlopen(libName, RTLD_LAZY | RTLD_LOCAL);
   delete [] libName;

   if(gameLib == NULL)
   {
      printf("%s\n", dlerror());
      return -1;
   }

   // set the filename of the exe image
   setExePathName = (void(*)(const char *)) dlsym(gameLib, "setExePathName");
   if(setExePathName == NULL)
   {
      printf("%s\n", dlerror());
      return -1;
   }
   setExePathName(argv[0]);

   // try to load the lib entry point
   torque_unixmain = (int(*)(int argc, const char **argv)) dlsym(gameLib, "torque_unixmain");

   if(torque_unixmain == NULL)
   {
      printf("%s\n", dlerror());
      return -1;
   }

   // Go!
   return torque_unixmain(argc, argv);
}
#endif // __linux__


#else //static exe build

#include "platform/platform.h"
#include "app/mainLoop.h"
#include "T3D/gameFunctions.h"
#include "core/strings/stringFunctions.h"

bool OpenGLCompat = false;

// Entry point for your game.
//
// This is build by default using the "StandardMainLoop" toolkit. Feel free
// to bring code over directly as you need to modify or extend things. You
// will need to merge against future changes to the SML code if you do this.
S32 TorqueMain(S32 argc, const char **argv)
{
   // Some handy debugging code:
   //   if (argc == 1) {
   //      static const char* argvFake[] = { "dtest.exe", "-jload", "test.jrn" };
   //      argc = 3;
   //      argv = argvFake;
   //   }

   //   Memory::enableLogging("testMem.log");
   //   Memory::setBreakAlloc(104717);

   // hack hack hack:
   // -OpenGLCompat arg must be processed
   // before OpenGL subsystem init.
   for(U32 i = 0; i < argc; i++)
   {
      if(dStricmp(argv[i], "-OpenGLCompat") == 0)
      {
         OpenGLCompat = true;
         break;
      }
   }

   // Initialize the subsystems.
   StandardMainLoop::init();

   // Handle any command line args.
   if(!StandardMainLoop::handleCommandLine(argc, argv))
   {
      Platform::AlertOK("Error", "Failed to initialize game, shutting down.");

      return 1;
   }

   // Main loop
   while(StandardMainLoop::doMainLoop());

   // Clean everything up.
   StandardMainLoop::shutdown();

   // Do we need to restart?
   if( StandardMainLoop::requiresRestart() )
      Platform::restartInstance();

   // Return.
   return StandardMainLoop::getReturnStatus();
}

#endif //TORQUE_SHARED
