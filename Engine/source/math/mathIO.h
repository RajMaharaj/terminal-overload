// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _MATHIO_H_
#define _MATHIO_H_

//Includes
#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _STREAM_H_
#include "core/stream/stream.h"
#endif
#ifndef _MMATH_H_
#include "math/mMath.h"
#endif


//------------------------------------------------------------------------------
//-------------------------------------- READING
//
inline bool mathRead(Stream& stream, Point2I* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   return success;
}

inline bool mathRead(Stream& stream, Point3I* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   success     &= stream.read(&p->z);
   return success;
}

inline bool mathRead(Stream& stream, Point2F* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   return success;
}

inline bool mathRead(Stream& stream, Point3F* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   success     &= stream.read(&p->z);
   return success;
}

inline bool mathRead(Stream& stream, Point4F* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   success     &= stream.read(&p->z);
   success     &= stream.read(&p->w);
   return success;
}

inline bool mathRead(Stream& stream, Point3D* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   success     &= stream.read(&p->z);
   return success;
}

inline bool mathRead(Stream& stream, PlaneF* p)
{
   bool success = stream.read(&p->x);
   success     &= stream.read(&p->y);
   success     &= stream.read(&p->z);
   success     &= stream.read(&p->d);
   return success;
}

inline bool mathRead(Stream& stream, Box3F* b)
{
   bool success = mathRead(stream, &b->minExtents);
   success     &= mathRead(stream, &b->maxExtents);
   return success;
}

inline bool mathRead(Stream& stream, SphereF* s)
{
   bool success = mathRead(stream, &s->center);
   success     &= stream.read(&s->radius);
   return success;
}

inline bool mathRead(Stream& stream, RectI* r)
{
   bool success = mathRead(stream, &r->point);
   success     &= mathRead(stream, &r->extent);
   return success;
}

inline bool mathRead(Stream& stream, RectF* r)
{
   bool success = mathRead(stream, &r->point);
   success     &= mathRead(stream, &r->extent);
   return success;
}

inline bool mathRead(Stream& stream, MatrixF* m)
{
   bool success = true;
   F32* pm    = *m;
   for (U32 i = 0; i < 16; i++)
      success &= stream.read(&pm[i]);
   return success;
}

inline bool mathRead(Stream& stream, QuatF* q)
{
   bool success = stream.read(&q->x);
   success     &= stream.read(&q->y);
   success     &= stream.read(&q->z);
   success     &= stream.read(&q->w);
   return success;
}

inline bool mathRead(Stream& stream, EaseF* e)
{
   bool success = stream.read( &e->dir );
   success     &= stream.read( &e->type );
   success     &= stream.read( &e->param[ 0 ] );
   success     &= stream.read( &e->param[ 1 ] );
   return success;
}

//------------------------------------------------------------------------------
//-------------------------------------- WRITING
//
inline bool mathWrite(Stream& stream, const Point2I& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   return success;
}

inline bool mathWrite(Stream& stream, const Point3I& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   success     &= stream.write(p.z);
   return success;
}

inline bool mathWrite(Stream& stream, const Point2F& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   return success;
}

inline bool mathWrite(Stream& stream, const Point3F& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   success     &= stream.write(p.z);
   return success;
}

inline bool mathWrite(Stream& stream, const Point4F& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   success     &= stream.write(p.z);
   success     &= stream.write(p.w);
   return success;
}

inline bool mathWrite(Stream& stream, const Point3D& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   success     &= stream.write(p.z);
   return success;
}

inline bool mathWrite(Stream& stream, const PlaneF& p)
{
   bool success = stream.write(p.x);
   success     &= stream.write(p.y);
   success     &= stream.write(p.z);
   success     &= stream.write(p.d);
   return success;
}

inline bool mathWrite(Stream& stream, const Box3F& b)
{
   bool success = mathWrite(stream, b.minExtents);
   success     &= mathWrite(stream, b.maxExtents);
   return success;
}

inline bool mathWrite(Stream& stream, const SphereF& s)
{
   bool success = mathWrite(stream, s.center);
   success     &= stream.write(s.radius);
   return success;
}

inline bool mathWrite(Stream& stream, const RectI& r)
{
   bool success = mathWrite(stream, r.point);
   success     &= mathWrite(stream, r.extent);
   return success;
}

inline bool mathWrite(Stream& stream, const RectF& r)
{
   bool success = mathWrite(stream, r.point);
   success     &= mathWrite(stream, r.extent);
   return success;
}

inline bool mathWrite(Stream& stream, const MatrixF& m)
{
   bool success    = true;
   const F32* pm = m;
   for (U32 i = 0; i < 16; i++)
      success &= stream.write(pm[i]);
   return success;
}

inline bool mathWrite(Stream& stream, const QuatF& q)
{
   bool success = stream.write(q.x);
   success     &= stream.write(q.y);
   success     &= stream.write(q.z);
   success     &= stream.write(q.w);
   return success;
}

inline bool mathWrite(Stream& stream, const EaseF& e)
{
   bool success = stream.write(e.dir);
   success     &= stream.write(e.type);
   success     &= stream.write(e.param[0]);
   success     &= stream.write(e.param[1]);
   return success;
}

#endif //_MATHIO_H_

