// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _GFXGLVERTEXBUFFER_H_
#define _GFXGLVERTEXBUFFER_H_

#ifndef _GFXVERTEXBUFFER_H_
#include "gfx/gfxVertexBuffer.h"
#endif
#include "gfx/gl/tGL/tGL.h"
#include "gfx/gl/util/glFrameAllocatorLockableHelper.h"

/// This is a vertex buffer which uses GL_ARB_vertex_buffer_object.
class GFXGLVertexBuffer : public GFXVertexBuffer 
{
public:

	GFXGLVertexBuffer(   GFXDevice *device, 
                        U32 numVerts, 
                        const GFXVertexFormat *vertexFormat, 
                        U32 vertexSize, 
                        GFXBufferType bufferType );

	~GFXGLVertexBuffer();

	virtual void lock(U32 vertexStart, U32 vertexEnd, void **vertexPtr); ///< Only write lock are supported. 
	virtual void unlock(); ///<
	virtual void prepare(); ///< Do nothing. Use void prepare(U32 stream, U32 divisor).
   virtual void finish(); ///< Do nothing.

   void prepare(U32 stream, U32 divisor);

	GLvoid* getBuffer(); ///< returns NULL

   // GFXResource interface
   virtual void zombify();
   virtual void resurrect();

private:
   friend class GFXGLDevice;
	/// GL buffer handle
	GLuint mBuffer;

    /// bytes offset in buffer 
	U32 mBufferOffset;

	/// start vertex offset in buffer
	U32 mBufferVertexOffset;
   U8* mZombieCache;

   FrameAllocatorLockableHelper mFrameAllocator;
};

#endif
