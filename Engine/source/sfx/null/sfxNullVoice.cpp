// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "sfx/null/sfxNullVoice.h"
#include "sfx/null/sfxNullBuffer.h"
#include "sfx/sfxInternal.h"


SFXNullVoice::SFXNullVoice( SFXNullBuffer* buffer )
   : Parent( buffer ),
     mIsLooping( false )
{
}

SFXNullVoice::~SFXNullVoice()
{
}

SFXStatus SFXNullVoice::_status() const
{
   if( !mIsLooping
       && mPlayTimer.isStarted()
       && !mPlayTimer.isPaused()
       && mPlayTimer.getPosition() >= mBuffer->getDuration() )
      mPlayTimer.stop();

   if( mPlayTimer.isPaused() )
      return SFXStatusPaused;
   else if( mPlayTimer.isStarted() )
      return SFXStatusPlaying;
   else
      return SFXStatusStopped;
}

void SFXNullVoice::_play()
{
   mPlayTimer.start();
}

void SFXNullVoice::_pause()
{
   mPlayTimer.pause();
}

void SFXNullVoice::_stop()
{
   mPlayTimer.stop();
}

void SFXNullVoice::_seek( U32 sample )
{
   const U32 sampleTime = mBuffer->getFormat().getDuration( sample );
   mPlayTimer.setPosition( sampleTime );
}

void SFXNullVoice::play( bool looping )
{
   mIsLooping = looping;
   mPlayTimer.start();
}

U32 SFXNullVoice::_tell() const
{
   U32 ms = _getPlayTime();
   
   const SFXFormat& format = mBuffer->getFormat();
   return ( format.getDataLength( ms ) / format.getBytesPerSample() );
}

SFXStatus SFXNullVoice::getStatus() const
{
   return _status();
}

void SFXNullVoice::setPosition( U32 sample )
{
   _seek( sample );
}

void SFXNullVoice::setMinMaxDistance( F32 min, F32 max )
{
}

void SFXNullVoice::setVelocity( const VectorF& velocity )
{
}

void SFXNullVoice::setTransform( const MatrixF& transform )
{
}

void SFXNullVoice::setVolume( F32 volume )
{
}

void SFXNullVoice::setPitch( F32 pitch )
{ 
}

void SFXNullVoice::setCone( F32 innerAngle, F32 outerAngle, F32 outerVolume )
{
}
