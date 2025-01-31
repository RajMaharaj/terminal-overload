// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _SIM_H_
#define _SIM_H_

#ifndef _TORQUE_TYPES_H_
#include "platform/types.h"
#endif
#ifndef _TORQUE_STRING_H_
#include "core/util/str.h"
#endif
#ifndef _MODULE_H_
#include "core/module.h"
#endif
#ifndef _CONSOLE_H_
#include "console/console.h"
#endif

// Forward Refs
class SimSet;
class SimGroup;
class SimDataBlockGroup;
class SimObject;
class SimEvent;
class Stream;

// Sim Types
typedef U32 SimTime;
typedef U32 SimObjectId;

/// Definition of some basic Sim system constants.
///
/// These constants define the range of ids assigned to datablocks
/// (DataBlockObjectIdFirst - DataBlockObjectIdLast), and the number
/// of bits used to store datablock IDs.
///
/// Normal Sim objects are given the range of IDs starting at
/// DynamicObjectIdFirst and going to infinity. Sim objects use
/// a SimObjectId to represent their ID; this is currently a U32.
///
/// The RootGroupId is assigned to gRootGroup, in which most SimObjects
/// are addded as child members. See simManager.cc for details, particularly
/// Sim::initRoot() and following.
enum SimObjectsConstants
{
   DataBlockObjectIdFirst = 3,
   DataBlockObjectIdBitSize = 10,
   DataBlockObjectIdLast = DataBlockObjectIdFirst + (1 << DataBlockObjectIdBitSize) - 1,

   MessageObjectIdFirst = DataBlockObjectIdLast + 1,
   MessageObjectIdBitSize = 6,
   MessageObjectIdLast = MessageObjectIdFirst + (1 << MessageObjectIdBitSize) - 1,

   DynamicObjectIdFirst = MessageObjectIdLast + 1,
   InvalidEventId = 0,
   RootGroupId = 0xFFFFFFFF,
};

//---------------------------------------------------------------------------

/// @defgroup simbase_helpermacros Helper Macros
///
/// These are used for named sets and groups in the manager.
/// @{
#define DeclareNamedSet(set) extern SimSet *g##set;inline SimSet *get##set() { return g##set; }
#define DeclareNamedGroup(set) extern SimGroup *g##set;inline SimGroup *get##set() { return g##set; }
#define ImplementNamedSet(set) SimSet *g##set;
#define ImplementNamedGroup(set) SimGroup *g##set;
/// @}

//---------------------------------------------------------------------------

namespace Sim
{
   DeclareNamedSet(ActiveActionMapSet)
   DeclareNamedSet(GhostAlwaysSet)
   DeclareNamedSet(WayPointSet)
   DeclareNamedSet(fxReplicatorSet)
   DeclareNamedSet(fxFoliageSet)
   DeclareNamedSet(BehaviorSet)
   DeclareNamedSet(MaterialSet)
   DeclareNamedSet(SFXSourceSet);
   DeclareNamedSet(SFXDescriptionSet);
   DeclareNamedSet(SFXTrackSet);
   DeclareNamedSet(SFXEnvironmentSet);
   DeclareNamedSet(SFXStateSet);
   DeclareNamedSet(SFXAmbienceSet);
   DeclareNamedSet(TerrainMaterialSet);
   DeclareNamedSet(DataBlockSet);
   DeclareNamedSet(ServerHudInfoSet)
   DeclareNamedSet(ClientHudInfoSet)
   DeclareNamedGroup(ActionMapGroup)
   DeclareNamedGroup(ClientGroup)
   DeclareNamedGroup(GuiGroup)
   DeclareNamedGroup(GuiDataGroup)
   DeclareNamedGroup(TCPGroup)
   DeclareNamedGroup(ClientConnectionGroup)
   DeclareNamedGroup(SFXParameterGroup);

   DeclareNamedSet(sgMissionLightingFilterSet);
   
   void init();
   void shutdown();
   
   bool isShuttingDown();

   SimDataBlockGroup *getDataBlockGroup();
   SimGroup* getRootGroup();

   SimObject* findObject(ConsoleValueRef&);
   SimObject* findObject(SimObjectId);
   SimObject* findObject(const char* name);
   SimObject* findObject(const char* fileName, S32 declarationLine);
   template<class T> inline bool findObject(ConsoleValueRef &ref,T*&t)
   {
      t = dynamic_cast<T*>(findObject(ref));
      return t != NULL;
   }
   template<class T> inline bool findObject(SimObjectId iD,T*&t)
   {
      t = dynamic_cast<T*>(findObject(iD));
      return t != NULL;
   }
   template<class T> inline bool findObject(const char *objectName,T*&t)
   {
      t = dynamic_cast<T*>(findObject(objectName));
      return t != NULL;
   }

   SimObject *spawnObject(String spawnClass,
                          String spawnDataBlock = String::EmptyString,
                          String spawnName = String::EmptyString,
                          String spawnProperties = String::EmptyString,
                          String spawnScript = String::EmptyString);

   void advanceToTime(SimTime time);
   void advanceTime(SimTime delta);
   SimTime getCurrentTime();
   SimTime getTargetTime();

   /// a target time of 0 on an event means current event
   U32 postEvent(SimObject*, SimEvent*, U32 targetTime);

   inline U32 postEvent(SimObjectId iD,SimEvent*evt, U32 targetTime)
   {
      return postEvent(findObject(iD), evt, targetTime);
   }
   inline U32 postEvent(const char *objectName,SimEvent*evt, U32 targetTime)
   {
      return postEvent(findObject(objectName), evt, targetTime);
   }
   inline U32 postCurrentEvent(SimObject*obj, SimEvent*evt)
   {
      return postEvent(obj,evt,getCurrentTime());
   }
   inline U32 postCurrentEvent(SimObjectId obj,SimEvent*evt)
   {
      return postEvent(obj,evt,getCurrentTime());
   }
   inline U32 postCurrentEvent(const char *obj,SimEvent*evt)
   {
      return postEvent(obj,evt,getCurrentTime());
   }

   void cancelEvent(U32 eventId);
   void cancelPendingEvents(SimObject *obj);
   bool isEventPending(U32 eventId);
   U32  getEventTimeLeft(U32 eventId);
   U32  getTimeSinceStart(U32 eventId);
   U32  getScheduleDuration(U32 eventId);

   /// Appends numbers to inName until an unused SimObject name is created
   String getUniqueName( const char *inName );
   /// Appends numbers to inName until an internal name not taken in the inSet is found.
   String getUniqueInternalName( const char *inName, SimSet *inSet, bool searchChildren );
   
   /// Return true if the given name string makes for a valid object name.
   /// Empty strings and NULL are also treated as valid names (anonymous objects).
   bool isValidObjectName( const char* name );

   bool saveObject(SimObject *obj, Stream *stream);
   SimObject *loadObjectStream(Stream *stream);

   bool saveObject(SimObject *obj, const char *filename);
   SimObject *loadObjectStream(const char *filename);
}

#endif // _SIM_H_
