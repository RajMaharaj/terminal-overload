// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _T3DSCENECLIENT_H_
#define _T3DSCENECLIENT_H_

#include "component/simComponent.h"
#include "T3DSceneComponent.h"

class T3DSceneClient : public SimComponent
{
   typedef SimComponent Parent;

public:
   T3DSceneClient()
   {
      _nextClient = NULL;
      _sceneGroup = NULL;
      _sceneGroupName = NULL;
      _sceneClientName = NULL;
   }

   T3DSceneClient * getNextSceneClient() { return _nextClient; }
   // TODO: internal
   void setNextSceneClient(T3DSceneClient * client) { _nextClient = client; }

   T3DSceneComponent * getSceneGroup() { return _sceneGroup; }

   StringTableEntry getSceneGroupName() { return _sceneGroupName; }
   void setSceneGroupName(const char * name);

   StringTableEntry getSceneClientName() { return _sceneClientName; }
   void setSceneClientName(const char * name) { _sceneClientName = StringTable->insert(name); }

protected:

   bool onComponentRegister(SimComponent * owner);
   void registerInterfaces(SimComponent * owner);

   T3DSceneClient * _nextClient;
   T3DSceneComponent * _sceneGroup;
   StringTableEntry _sceneGroupName;
   StringTableEntry _sceneClientName;
};

class T3DSolidSceneClient : public T3DSceneClient, public ISolid3D, public Transform3D::IDirtyListener
{
public:

   T3DSolidSceneClient()
   {
      _transform = NULL;
      _objectBox = new ValueWrapperInterface<Box3F>();
   }

   Box3F getObjectBox() { return _objectBox->get(); }
   void setObjectBox(const Box3F & box) { _objectBox->set(box); }
   Box3F getWorldBox();
   const MatrixF & getTransform();
   Transform3D * getTransform3D() { return _transform; }

   void OnTransformDirty();

protected:


   // TODO: internal
   void setTransform3D(Transform3D *  transform);

   Transform3D * _transform;
   ValueWrapperInterface<Box3F> * _objectBox;
};

#endif // #ifndef _T3DSCENECLIENT_H_
