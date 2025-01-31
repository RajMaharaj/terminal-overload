// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include <stdio.h>
#include "console/consoleInternal.h"
#include "console/stringStack.h"


void ConsoleValueStack::getArgcArgv(StringTableEntry name, U32 *argc, ConsoleValueRef **in_argv, bool popStackFrame /* = false */)
{
   U32 startStack = mStackFrames[mFrame-1];
   U32 argCount   = getMin(mStackPos - startStack, (U32)MaxArgs - 1);

   *in_argv = mArgv;
   mArgv[0] = name;
   
   for(U32 i = 0; i < argCount; i++) {
      ConsoleValueRef *ref = &mArgv[i+1];
      ref->value = &mStack[startStack + i];
      ref->stringStackValue = NULL;
   }
   argCount++;
   
   *argc = argCount;

   if(popStackFrame)
      popFrame();
}

ConsoleValueStack::ConsoleValueStack() : 
mFrame(0),
mStackPos(0)
{
   for (int i=0; i<ConsoleValueStack::MaxStackDepth; i++) {
      mStack[i].init();
      mStack[i].type = ConsoleValue::TypeInternalString;
   }
}

ConsoleValueStack::~ConsoleValueStack()
{
}

void ConsoleValueStack::pushVar(ConsoleValue *variable)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return;
   }

   switch (variable->type)
   {
   case ConsoleValue::TypeInternalInt:
      mStack[mStackPos++].setIntValue((S32)variable->getIntValue());
   case ConsoleValue::TypeInternalFloat:
      mStack[mStackPos++].setFloatValue((F32)variable->getFloatValue());
   default:
      mStack[mStackPos++].setStackStringValue(variable->getStringValue());
   }
}

void ConsoleValueStack::pushValue(ConsoleValue &variable)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return;
   }

   switch (variable.type)
   {
   case ConsoleValue::TypeInternalInt:
      mStack[mStackPos++].setIntValue((S32)variable.getIntValue());
   case ConsoleValue::TypeInternalFloat:
      mStack[mStackPos++].setFloatValue((F32)variable.getFloatValue());
   default:
      mStack[mStackPos++].setStringValue(variable.getStringValue());
   }
}

ConsoleValue *ConsoleValueStack::pushString(const char *value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushString %s", mStackPos, value);

   mStack[mStackPos++].setStringValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushStackString(const char *value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushString %s", mStackPos, value);

   mStack[mStackPos++].setStackStringValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushUINT(U32 value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushUINT %i", mStackPos, value);

   mStack[mStackPos++].setIntValue(value);
   return &mStack[mStackPos-1];
}

ConsoleValue *ConsoleValueStack::pushFLT(float value)
{
   if (mStackPos == ConsoleValueStack::MaxStackDepth) {
      AssertFatal(false, "Console Value Stack is empty");
      return NULL;
   }

   //Con::printf("[%i]CSTK pushFLT %f", mStackPos, value);

   mStack[mStackPos++].setFloatValue(value);
   return &mStack[mStackPos-1];
}

static ConsoleValue gNothing;

ConsoleValue* ConsoleValueStack::pop()
{
   if (mStackPos == 0) {
      AssertFatal(false, "Console Value Stack is empty");
      return &gNothing;
   }

   return &mStack[--mStackPos];
}

void ConsoleValueStack::pushFrame()
{
   //Con::printf("CSTK pushFrame[%i] (%i)", mFrame, mStackPos);
   mStackFrames[mFrame++] = mStackPos;
}

void ConsoleValueStack::resetFrame()
{
   if (mFrame == 0) {
      mStackPos = 0;
      return;
   }

   U32 start = mStackFrames[mFrame-1];
   //for (U32 i=start; i<mStackPos; i++) {
      //mStack[i].clear();
   //}
   mStackPos = start;
   //Con::printf("CSTK resetFrame to %i", mStackPos);
}

void ConsoleValueStack::clearFrames()
{
   mStackPos = 0;
   mFrame = 0;
}

void ConsoleValueStack::popFrame()
{
   //Con::printf("CSTK popFrame");
   if (mFrame == 0) {
      // Go back to start
      mStackPos = 0;
      return;
   }

   U32 start = mStackFrames[mFrame-1];
   //for (U32 i=start; i<mStackPos; i++) {
      //mStack[i].clear();
   //}
   mStackPos = start;
   mFrame--;
}
