// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "gui/worldEditor/editor.h"
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/engineAPI.h"
#include "gui/controls/guiTextListCtrl.h"
#include "T3D/shapeBase.h"
#include "T3D/gameBase/gameConnection.h"

#ifndef TORQUE_PLAYER
// See matching #ifdef in app/game.cpp
bool gEditingMission = false;
#endif

//------------------------------------------------------------------------------
// Class EditManager
//------------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(EditManager);

ConsoleDocClass( EditManager,
   "@brief For Editor use only, deprecated\n\n"
   "@internal"
);

EditManager::EditManager()
{
   for(U32 i = 0; i < 10; i++)
      mBookmarks[i] = MatrixF(true);
}

EditManager::~EditManager()
{
}

//------------------------------------------------------------------------------

bool EditManager::onWake()
{
   if(!Parent::onWake())
      return(false);

   return(true);
}

void EditManager::onSleep()
{
   Parent::onSleep();
}

//------------------------------------------------------------------------------

bool EditManager::onAdd()
{
   if(!Parent::onAdd())
      return(false);

   // hook the namespace
   const char * name = getName();
   if(name && name[0] && getClassRep())
   {
      Namespace * parent = getClassRep()->getNameSpace();
      Con::linkNamespaces(parent->mName, name);
      mNameSpace = Con::lookupNamespace(name);
   }

   return(true);
}

//------------------------------------------------------------------------------

// NOTE: since EditManager is not actually used as a gui anymore, onWake/Sleep
// were never called, which broke anyone hooking into onEditorEnable/onEditorDisable 
// and gEditingMission. So, moved these to happen in response to console methods
// which should be called at the appropriate time.
//
// This is a quick fix and this system is still "begging" for a remake.

void EditManager::editorEnabled()
{
   for(SimGroupIterator itr(Sim::getRootGroup());  *itr; ++itr)
      (*itr)->onEditorEnable();

   gEditingMission = true;
}

void EditManager::editorDisabled()
{
   for(SimGroupIterator itr(Sim::getRootGroup());  *itr; ++itr)
   {
      SimObject *so = *itr;
      AssertFatal(so->isProperlyAdded() && !so->isRemoved(), "bad");
      so->onEditorDisable();
   }

   gEditingMission = false;
}

//------------------------------------------------------------------------------

static GameBase * getControlObj()
{
   GameConnection * connection = GameConnection::getLocalClientConnection();
   ShapeBase* control = 0;
   if(connection)
      control = dynamic_cast<ShapeBase*>(connection->getControlObject());
   return(control);
}

DefineConsoleMethod( EditManager, setBookmark, void, (S32 val), , "(int slot)")
{
   if(val < 0 || val > 9)
      return;

   GameBase * control = getControlObj();
   if(control)
      object->mBookmarks[val] = control->getTransform();
}

DefineConsoleMethod( EditManager, gotoBookmark, void, (S32 val), , "(int slot)")
{
   if(val < 0 || val > 9)
      return;

   GameBase * control = getControlObj();
   if(control)
      control->setTransform(object->mBookmarks[val]);
}

DefineConsoleMethod( EditManager, editorEnabled, void, (), , "Perform the onEditorEnabled callback on all SimObjects and set gEditingMission true" )
{
   object->editorEnabled();
}

DefineConsoleMethod( EditManager, editorDisabled, void, (), , "Perform the onEditorDisabled callback on all SimObjects and set gEditingMission false" )
{
   object->editorDisabled();
}

DefineConsoleMethod( EditManager, isEditorEnabled, bool, (), , "Return the value of gEditingMission." )
{
   return gEditingMission;
}