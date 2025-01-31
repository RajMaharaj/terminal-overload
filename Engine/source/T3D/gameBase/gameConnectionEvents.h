// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#ifndef _GAMECONNECTIONEVENTS_H_
#define _GAMECONNECTIONEVENTS_H_

#ifndef _SIMBASE_H_
#include "console/simBase.h"
#endif

#ifndef _GAMECONNECTION_H_
#include "T3D/gameBase/gameConnection.h"
#endif

#ifndef _SFXPROFILE_H_
#include "sfx/sfxProfile.h"
#endif

#ifndef _BITSTREAM_H_
#include "core/stream/bitStream.h"
#endif


class QuitEvent : public SimEvent
{
   void process(SimObject *object)
   {
      Platform::postQuitMessage(0);
   }
};

/// Event for sending a datablock over the net from the server to the client.
///
/// Datablock events are GuaranteedOrdered client events.
///
class SimDataBlockEvent : public NetEvent
{
   public:
   
      typedef NetEvent Parent;
      
   protected:
   
      /// Id of the datablock object to be sent.  This must be a datablock ID
      /// (as opposed to a normal object ID).
      SimObjectId id;
      
      ///
      U32 mIndex;
      
      /// Total number of datablocks that are part of this datablock transmission.
      /// Each datablock is transmitted in an independent datablock event.
      U32 mTotal;
      
      /// The mission sequence number to which this datablock transmission
      /// belongs.
      ///
      /// @see GameConnection::getDataBlockSequence
      U32 mMissionSequence;

      /// Datablock object constructed on the client side.
      SimDataBlock *mObj;
      
      ///
      bool mProcess;
  
   public:
   
      SimDataBlockEvent(SimDataBlock* obj = NULL, U32 index = 0, U32 total = 0, U32 missionSequence = 0);
      ~SimDataBlockEvent();
      
      void pack(NetConnection *, BitStream *bstream);
      void write(NetConnection *, BitStream *bstream);
      void unpack(NetConnection *cptr, BitStream *bstream);
      void process(NetConnection*);
      void notifyDelivered(NetConnection *, bool);
      
      #ifdef TORQUE_DEBUG_NET
      const char *getDebugName();
      #endif
      
      DECLARE_CONOBJECT( SimDataBlockEvent );
      DECLARE_CATEGORY( "Game Networking" );
};

class Sim2DAudioEvent: public NetEvent
{
  private:
   SFXProfile *mProfile;

  public:
   typedef NetEvent Parent;
   Sim2DAudioEvent(SFXProfile *profile=NULL);
   void pack(NetConnection *, BitStream *bstream);
   void write(NetConnection *, BitStream *bstream);
   void unpack(NetConnection *, BitStream *bstream);
   void process(NetConnection *);
   DECLARE_CONOBJECT(Sim2DAudioEvent);
};

class Sim3DAudioEvent: public NetEvent
{
  private:
   SFXProfile *mProfile;
   MatrixF mTransform;

  public:
   typedef NetEvent Parent;
   Sim3DAudioEvent(SFXProfile *profile=NULL,const MatrixF* mat=NULL);
   void pack(NetConnection *, BitStream *bstream);
   void write(NetConnection *, BitStream *bstream);
   void unpack(NetConnection *, BitStream *bstream);
   void process(NetConnection *);
   DECLARE_CONOBJECT(Sim3DAudioEvent);
};


//----------------------------------------------------------------------------
// used to set the crc for the current mission (mission lighting)
//----------------------------------------------------------------------------
class SetMissionCRCEvent : public NetEvent
{
   private:
      U32   mCrc;

   public:
      typedef NetEvent Parent;
      SetMissionCRCEvent(U32 crc = 0xffffffff)
         { mCrc = crc; }
      void pack(NetConnection *, BitStream * bstream)
         { bstream->write(mCrc); }
      void write(NetConnection * con, BitStream * bstream)
         { pack(con, bstream); }
      void unpack(NetConnection *, BitStream * bstream)
         { bstream->read(&mCrc); }
      void process(NetConnection * con)
         { static_cast<GameConnection*>(con)->setMissionCRC(mCrc); }

      DECLARE_CONOBJECT(SetMissionCRCEvent);
};

#endif
