// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include <stdarg.h>

#include "core/strings/stringFunctions.h"
#include "console/console.h"


//-------------------------------------- STATIC Declaration
PlatformAssert *PlatformAssert::platformAssert = NULL;

//--------------------------------------
PlatformAssert::PlatformAssert()
{
   processing = false;
   ignoreAll = false;
}

//--------------------------------------
PlatformAssert::~PlatformAssert()
{
}

//--------------------------------------
void PlatformAssert::create( PlatformAssert* newAssertClass )
{
   if (!platformAssert)
      platformAssert = newAssertClass ? newAssertClass : new PlatformAssert;
}


//--------------------------------------
void PlatformAssert::destroy()
{
   if (platformAssert)
      delete platformAssert;
   platformAssert = NULL;
}


//--------------------------------------
bool PlatformAssert::displayMessageBox(const char *title, const char *message, bool retry)
{
   if (retry)
      return Platform::AlertRetry(title, message);

   Platform::AlertOK(title, message);
   return false;
}

static const char *typeName[] = { "Unknown", "Fatal-ISV", "Fatal", "Warning" };
//------------------------------------------------------------------------------
static bool askToEnterDebugger(const char* message )
{
   static bool haveAsked = false;
   static bool useDebugger = true;
   if(!haveAsked )
   {
      static char tempBuff[1024];
      dSprintf( tempBuff, 1024, "Torque has encountered an assertion with message\n\n"
         "%s\n\n"
         "Would you like to use the debugger? If you cancel, you won't be asked"
         " again until you restart Torque.", message);

      useDebugger = Platform::AlertOKCancel("Use debugger?", tempBuff );
      haveAsked = true;
   }
   return useDebugger;
}

//--------------------------------------

bool PlatformAssert::process(Type         assertType,
                             const char  *filename,
                             U32          lineNumber,
                             const char  *message)
{
    // If we're somehow recursing, just die.
    if(processing)
        Platform::debugBreak();
    
    processing = true;
    bool ret = false;
    
    // always dump to the Assert to the Console
    if (Con::isActive())
    {
        if (assertType == Warning)
            Con::warnf(ConsoleLogEntry::Assert, "%s(%ld,0): {%s} - %s", filename, lineNumber, typeName[assertType], message);
        else
            Con::errorf(ConsoleLogEntry::Assert, "%s(%ld,0): {%s} - %s", filename, lineNumber, typeName[assertType], message);
    }
    
    // if not a WARNING pop-up a dialog box
    if (assertType != Warning)
    {
        // used for processing navGraphs (an assert won't botch the whole build)
        if(Con::getBoolVariable("$FP::DisableAsserts", false) == true)
            Platform::forceShutdown(1);
        
        char buffer[2048];
        dSprintf(buffer, 2048, "%s: (%s @ %ld)", typeName[assertType], filename, lineNumber);
        if( !ignoreAll )
        {
            // Display message box with Debug, Ignore, Ignore All, and Exit options
            switch( Platform::AlertAssert(buffer, message) )
            {
                case Platform::ALERT_ASSERT_DEBUG:				
                    ret = true;
                    break;
                case Platform::ALERT_ASSERT_IGNORE:
                    ret = false;
                    break;
                case Platform::ALERT_ASSERT_IGNORE_ALL:
                    ignoreAll = true;
                    ret = false;
                    break;
                default:
                case Platform::ALERT_ASSERT_EXIT:
                    Platform::forceShutdown(1);
                    break;
            }
        }
    }
    
    processing = false;
    
    return ret;
}

bool PlatformAssert::processingAssert()
{
   return platformAssert ? platformAssert->processing : false;
}

//--------------------------------------
bool PlatformAssert::processAssert(Type        assertType,
                                   const char  *filename,
                                   U32         lineNumber,
                                   const char  *message)
{
   if (platformAssert)
      return platformAssert->process(assertType, filename, lineNumber, message);
   else // when platAssert NULL (during _start/_exit) try direct output...
      dPrintf("\n%s: (%s @ %ld) %s\n", typeName[assertType], filename, lineNumber, message);

   // this could also be platform-specific: OutputDebugString on PC, DebugStr on Mac.
   // Will raw printfs do the job?  In the worst case, it's a break-pointable line of code.
   // would have preferred Con but due to race conditions, it might not be around...
   // Con::errorf(ConsoleLogEntry::Assert, "%s: (%s @ %ld) %s", typeName[assertType], filename, lineNumber, message);

   return true;
}

//--------------------------------------
const char* avar(const char *message, ...)
{
   static char buffer[4096];
   va_list args;
   va_start(args, message);
   dVsprintf(buffer, sizeof(buffer), message, args);
   return( buffer );
}

//-----------------------------------------------------------------------------

ConsoleFunction( Assert, void, 3, 3, "(condition, message) - Fatal Script Assertion" )
{
    // Process Assertion.
    AssertISV( dAtob(argv[1]), argv[2] );
}
