// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "windowManager/platformCursorController.h"

void PlatformCursorController::pushCursor( S32 cursorID )
{
   // Place the new cursor shape onto the stack
   mCursors.increment();

   Cursor_Shape &shape = mCursors.last();
   shape.mCursorType  = Cursor_Shape::TYPE_RESOURCE;
   shape.mCursorID    = cursorID;

   // Now Change the Cursor Shape.
   setCursorShape( shape.mCursorID );
}

void PlatformCursorController::pushCursor( const UTF8 *fileName )
{
   // Place the new cursor shape onto the stack
   mCursors.increment();

   // Store the Details.
   Cursor_Shape &shape = mCursors.last();
   shape.mCursorType  = Cursor_Shape::TYPE_FILE;
   shape.mCursorFile  = String::ToString( "%s", fileName );

   // Now Change the Cursor Shape.
   setCursorShape( shape.mCursorFile.c_str(), true );
}

void PlatformCursorController::popCursor()
{
   // Before poping the stack, make sure we're not trying to remove the last cursor shape
   if ( mCursors.size() <= 1 )
   {
      return;
   }

   // Clear the Last Cursor.
   mCursors.pop_back();

   // Now Change the Cursor Shape.
   setCursorShape( mCursors.last(), true );
}

void PlatformCursorController::refreshCursor()
{
   // Refresh the Cursor Shape.
   setCursorShape( mCursors.last(), false );
}

void PlatformCursorController::setCursorShape( const Cursor_Shape &shape, bool reload )
{
    switch( shape.mCursorType )
    {
        case Cursor_Shape::TYPE_RESOURCE :
            {

                // Set Resource.
                setCursorShape( shape.mCursorID );

            } break;

        case Cursor_Shape::TYPE_FILE :
            {

                // Set File.
                setCursorShape( shape.mCursorFile.c_str(), reload );

            } break;
    }
}
