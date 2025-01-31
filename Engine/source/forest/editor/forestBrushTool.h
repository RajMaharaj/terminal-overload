// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _FOREST_EDITOR_BRUSH_H_
#define _FOREST_EDITOR_BRUSH_H_

#ifndef _FOREST_EDITOR_TOOL_H_
#include "forest/editor/forestTool.h"
#endif
#ifndef _FORESTITEM_H_
#include "forest/forestItem.h"
#endif
#ifndef _FOREST_EDITOR_BRUSHELEMENT_H_
#include "forest/editor/forestBrushElement.h"
#endif
#ifndef _COLOR_H_
#include "core/color.h"
#endif



class Forest;
class ForestUndoAction;

class ForestBrushTool : public ForestTool
{
   typedef ForestTool Parent;

   friend class ForestBrushToolEvent;

public:

   enum BrushMode
   {
      Paint = 0,
      Erase,
      EraseSelected
   };

   ForestBrushTool();
   virtual ~ForestBrushTool();

   // SimObject
   DECLARE_CONOBJECT( ForestBrushTool );
   static void initPersistFields();
   virtual bool onAdd();
   virtual void onRemove();

   // ForestTool
   virtual void on3DMouseDown( const Gui3DMouseEvent &evt );
   virtual void on3DMouseUp( const Gui3DMouseEvent &evt );
   virtual void on3DMouseMove( const Gui3DMouseEvent &evt );
   virtual void on3DMouseDragged( const Gui3DMouseEvent &evt );
   virtual bool onMouseWheel(const GuiEvent &evt );   
   virtual void onRender3D();
   virtual void onRender2D();
   virtual void onActivated( const Gui3DMouseEvent &lastEvent );
   virtual void onDeactivated();
   virtual bool updateGuiInfo();

   // ForestBrushTool
   void setSize( F32 val );
   void setPressure( F32 val );
   void setHardness( F32 val );
   void collectElements() { _collectElements(); }
   bool getGroundAt( const Point3F &worldPt, F32 *zValueOut, VectorF *normalOut );

protected:      

   void _onStroke();
   virtual void _action( const Point3F &point );
   virtual void _paint( const Point3F &point );
   virtual void _erase( const Point3F &point );
      
   bool _updateBrushPoint( const Gui3DMouseEvent &event_ );   
   virtual void _collectElements();

   static bool protectedSetSize( void *object, const char *index, const char *data );
   static bool protectedSetPressure( void *object, const char *index, const char *data );
   static bool protectedSetHardness( void *object, const char *index, const char *data );

protected:

   MRandom mRandom;

   /// We treat this as a radius.
   F32 mSize;
   F32 mPressure;
	F32 mHardness;

   U32 mMode;

   ColorI mColor;

   Vector<ForestBrushElement*> mElements;
   Vector<ForestItemData*> mDatablocks;

   ///
   bool mBrushDown;

   ///
   bool mDrawBrush;

   ///
   U32 mStrokeEvent;

   ///
   Point3F mLastBrushPoint;
   Point3F mLastBrushNormal;

   /// The creation action we're actively filling.
   ForestUndoAction *mCurrAction;  
};

typedef ForestBrushTool::BrushMode ForestBrushMode;
DefineEnumType( ForestBrushMode );


class ForestBrushToolEvent : public SimEvent
{
public:

   void process( SimObject *object )
   {
      ((ForestBrushTool*)object)->_onStroke();
   }
};


#endif // _FOREST_EDITOR_BRUSH_H_



