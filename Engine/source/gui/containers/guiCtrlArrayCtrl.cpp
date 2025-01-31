// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "gui/containers/guiCtrlArrayCtrl.h"

#include "console/engineAPI.h"

IMPLEMENT_CONOBJECT(GuiControlArrayControl);

ConsoleDocClass( GuiControlArrayControl,
   "@brief Brief Desc.\n\n"
   
   "@tsexample\n"
   "// Comment:\n"
   "%okButton = new ClassObject()\n"
   "instantiation\n"
   "@endtsexample\n\n"
   
   "@ingroup GuiContainers"
);

// One call back that the system isnt ready for yet in this class

GuiControlArrayControl::GuiControlArrayControl()
{
   mResizing = false;

   mCols = 0;
   mRowSize = 30;
   mRowSpacing = 2;
   mColSpacing = 0;
   mIsContainer = true;
}

void GuiControlArrayControl::initPersistFields()
{
   addGroup( "Array" );
   
      addField( "colCount",     TypeS32,       Offset(mCols,        GuiControlArrayControl),
         "Number of colums in the array." );
      addField( "colSizes",     TypeS32Vector, Offset(mColumnSizes, GuiControlArrayControl),
         "Size of each individual column." );
      addField( "rowSize",      TypeS32,       Offset(mRowSize,     GuiControlArrayControl),
         "Heigth of a row in the array." );
      addField( "rowSpacing",   TypeS32,       Offset(mRowSpacing,  GuiControlArrayControl),
         "Padding to put between rows." );
      addField( "colSpacing",   TypeS32,       Offset(mColSpacing,  GuiControlArrayControl),
         "Padding to put between columns." );
      
   endGroup( "Array" );

   Parent::initPersistFields();
}

bool GuiControlArrayControl::onWake()
{
   if ( !Parent::onWake() )
      return false;

   return true;
}

void GuiControlArrayControl::onSleep()
{
   Parent::onSleep();
}

void GuiControlArrayControl::inspectPostApply()
{
   Parent::inspectPostApply();

   updateArray();
}

bool GuiControlArrayControl::resize(const Point2I &newPosition, const Point2I &newExtent)
{
   if( !Parent::resize(newPosition, newExtent) )
      return false;

   return updateArray();
}

void GuiControlArrayControl::addObject(SimObject *obj)
{
   Parent::addObject(obj);

   updateArray();
}

void GuiControlArrayControl::removeObject(SimObject *obj)
{
   Parent::removeObject(obj);

   updateArray();
}

bool GuiControlArrayControl::reOrder(SimObject* obj, SimObject* target)
{
   bool ret = Parent::reOrder(obj, target);
   if (ret)
      updateArray();

   return ret;
}

bool GuiControlArrayControl::updateArray()
{
   // Prevent recursion
   if(mResizing) 
      return false;

   // Set Resizing.
   mResizing = true;

   if(mCols < 1 || size() < 1)
   {
      mResizing = false;
      return false;
   }

   S32 *sizes = new S32[mCols];
   S32 *offsets = new S32[mCols];
   S32 totalSize = 0;
   Point2I extent = getExtent();

   // Calculate the column sizes
   for(S32 i=0; i<mCols; i++)
   {
      if( i >= mColumnSizes.size() )
         sizes[ i ] = 0;
      else
         sizes[i] = mColumnSizes[i];
         
      offsets[i] = totalSize;

      // If it's an auto-size one, then... auto-size...
      if(sizes[i] == -1)
      {
         sizes[i] = extent.x - totalSize;
         break;
      }

      totalSize += sizes[i] + mColSpacing;
   }

   // Now iterate through the children and resize them to fit the grid...
   for(S32 i=0; i<size(); i++)
   {
      GuiControl *gc = dynamic_cast<GuiControl*>(operator[](i));

      // Get the current column and row...
      S32 curCol = i % mCols;
      S32 curRow = i / mCols;

      if(gc)
      {
         Point2I newPos(offsets[curCol], curRow * (mRowSize + mRowSpacing));
         Point2I newExtents(sizes[curCol], mRowSize);

         gc->resize(newPos, newExtents);
      }
   }

   // Clear Sizing Flag.
   mResizing = false;

   delete [] sizes;
   delete [] offsets;

   return true;
}
