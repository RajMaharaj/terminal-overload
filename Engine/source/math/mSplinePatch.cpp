// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "math/mSplinePatch.h"


//******************************************************************************
// Spline control points
//******************************************************************************
SplCtrlPts::SplCtrlPts()
{
}

//------------------------------------------------------------------------------
// Destructor
//------------------------------------------------------------------------------
SplCtrlPts::~SplCtrlPts()
{
}

//------------------------------------------------------------------------------
// Get point
//------------------------------------------------------------------------------
const Point3F * SplCtrlPts::getPoint( U32 pointNum )
{
   return &mPoints[pointNum];
}

//------------------------------------------------------------------------------
// Set point
//------------------------------------------------------------------------------
void SplCtrlPts::setPoint( Point3F &point, U32 pointNum )
{
   mPoints[pointNum] = point;
}

//------------------------------------------------------------------------------
// Add point
//------------------------------------------------------------------------------
void SplCtrlPts::addPoint( Point3F &point )
{
   mPoints.push_back( point );
}

//------------------------------------------------------------------------------
// Submit control points
//------------------------------------------------------------------------------
void SplCtrlPts::submitPoints( Point3F *pts, U32 num )
{
   mPoints.clear();

   for( S32 i=0; i<num; i++ )
   {
      mPoints.push_back( pts[i] );
   }
}


//******************************************************************************
// Spline patch  - base class
//******************************************************************************
SplinePatch::SplinePatch()
{
   mNumReqControlPoints = 0;
}

//------------------------------------------------------------------------------
// This method is the only way to modify control points once they have been
// submitted to the patch.  This was done to make sure the patch has a chance
// to re-calc any pre-calculated data it may be using from the submitted control
// points.
//------------------------------------------------------------------------------
void SplinePatch::setControlPoint( Point3F &point, S32 index )
{
   mControlPoints.setPoint( point, index );
}

//------------------------------------------------------------------------------
// Calc point on spline using already submitted points
//------------------------------------------------------------------------------
void SplinePatch::calc( F32 /*t*/, Point3F &result )
{
   result.x = result.y = result.z = 0.0f;
}

//------------------------------------------------------------------------------
// Calc point on spline using passed-in points
//------------------------------------------------------------------------------
void SplinePatch::calc( Point3F * /*points*/, F32 /*t*/, Point3F &result )
{
   result.x = result.y = result.z = 0.0f;
}
