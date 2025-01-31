// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "core/strings/stringFunctions.h"
#include "core/tagDictionary.h"
#include "core/stream/stream.h"

namespace {

const char TAG_ASCII_ID[] = "[TAG]";
const char TAG_ASCII_END[] = "[END]";
const char TAG_ASCII_HEADER[] = "// Auto-Generated by TagDictionary class";

const S32 sg_tagDictAsciiUser   = 1;

} // namespace

TagDictionary tagDictionary;

TagDictionary::TagDictionary()
{
   numBuckets = 29;
   defineHashBuckets = (TagEntry **) dMalloc(numBuckets * sizeof(TagEntry *));
   idHashBuckets = (TagEntry **) dMalloc(numBuckets * sizeof(TagEntry *));

   S32 i;
   for(i = 0; i < numBuckets; i++)
   {
      defineHashBuckets[i] = NULL;
      idHashBuckets[i] = NULL;
   }
   numEntries = 0;
   entryChain = NULL;
}

TagDictionary::~TagDictionary()
{
   dFree(defineHashBuckets);
   dFree(idHashBuckets);
}

//------------------------------------------------------------------------------

static inline S32 hashId(S32 id, S32 tsize)
{
   return id % tsize;
}

static inline S32 hashDefine(StringTableEntry define, S32 tsize)
{
   return ((S32)((dsize_t)define) >> 2) % tsize;
}

//------------------------------------------------------------------------------

bool TagDictionary::addEntry(S32 value, StringTableEntry define, StringTableEntry string)
{
   if(!value)
      return false;
//#pragma message "put console prints back"
   if(idToDefine(value))
   {
      AssertWarn(false, avar("Error: id %d already defined to a tag.", value));
      //Con::printf("Error: id %d already defined to a tag.", value);
      return false;
   }
   S32 tempTag;
   if((tempTag = defineToId(define)) != 0)
   {
      AssertWarn(false, avar("Error: define %s already defined to tag %d.", define, tempTag));
      //Con::printf("Error: define %s already defined to tag %d.", define, tempTag);
      return false;
   }
   TagEntry *newEntry = (TagEntry *) mempool.alloc(sizeof(TagEntry));
   
   newEntry->id = value;
   newEntry->define = define;
   newEntry->string = string;

   numEntries++;
   if(numEntries > numBuckets)
   {
      numBuckets = numBuckets * 2 + 1;
      defineHashBuckets = (TagEntry **) dRealloc(defineHashBuckets, numBuckets * sizeof(TagEntry *));
      idHashBuckets = (TagEntry **) dRealloc(idHashBuckets, numBuckets * sizeof(TagEntry *));
      S32 i;
      for(i = 0; i < numBuckets; i++)
      {
         defineHashBuckets[i] = NULL;
         idHashBuckets[i] = NULL;
      }
      TagEntry *walk = entryChain;
   
      while(walk)
      {
         S32 index = hashId(walk->id, numBuckets);
         walk->idHashLink = idHashBuckets[index];
         idHashBuckets[index] = walk;

         index = hashDefine(walk->define, numBuckets);
         walk->defineHashLink = defineHashBuckets[index];
         defineHashBuckets[index] = walk;

         walk = walk->chain;
      }
   }
   newEntry->chain = entryChain;
   entryChain = newEntry;

   S32 index = hashId(newEntry->id, numBuckets);
   newEntry->idHashLink = idHashBuckets[index];
   idHashBuckets[index] = newEntry;

   index = hashDefine(newEntry->define, numBuckets);
   newEntry->defineHashLink = defineHashBuckets[index];
   defineHashBuckets[index] = newEntry;
   return true;
}

//------------------------------------------------------------------------------

bool TagDictionary::writeHeader(Stream& io_sio)
{
   char buff[15000];
   Vector<S32> v;

   TagEntry *walk = entryChain;
   while(walk)
   {
      v.push_back(walk->id);
      walk = walk->chain;
   }

   sortIdVector(v);

   io_sio.write( sizeof(TAG_ASCII_HEADER)-1, TAG_ASCII_HEADER);
   io_sio.write( 4, "\r\n\r\n");
   
   char exclude[256];
   char tempBuf[256];
   dSprintf(exclude, sizeof(exclude), "_TD%10.10u_H_", Platform::getVirtualMilliseconds() / 4);
   
   dSprintf(tempBuf, sizeof(tempBuf), "#ifndef %s\r\n", exclude);
   io_sio.write(dStrlen(tempBuf), tempBuf);
   dSprintf(tempBuf, sizeof(tempBuf), "#define %s\r\n\r\n", exclude);
   io_sio.write(dStrlen(tempBuf), tempBuf);
   
   for (U32 i = 0; i < v.size(); i++)
   {
      dSprintf(buff, sizeof(buff), "#define %s (%d)\r\n", idToDefine(v[i]), v[i]);
      io_sio.write(dStrlen(buff), buff);
   }

   dSprintf(tempBuf, sizeof(tempBuf), "\r\n#endif // %s\r\n", exclude);
   io_sio.write(dStrlen(tempBuf), tempBuf);

   return (io_sio.getStatus() == Stream::Ok);
}

//------------------------------------------------------------------------------

StringTableEntry TagDictionary::defineToString(StringTableEntry tag)
{
   S32 index = hashDefine(tag, numBuckets);
   if (index < 0) return NULL;
   TagEntry *walk = defineHashBuckets[index];
   while(walk)
   {
      if(walk->define == tag)
         return walk->string;
      walk = walk->defineHashLink;
   }
   return NULL;
}

S32 TagDictionary::defineToId(StringTableEntry tag)
{
   S32 index = hashDefine(tag, numBuckets);
   if (index < 0) return 0;
   TagEntry *walk = defineHashBuckets[index];
   while(walk)
   {
      if(walk->define == tag)
         return walk->id;
      walk = walk->defineHashLink;
   }
   return 0;
}

StringTableEntry TagDictionary::idToString(S32 id)
{
   S32 index = hashId(id, numBuckets);
   if (index < 0) return NULL;
   TagEntry *walk = idHashBuckets[index];
   while(walk)
   {
      if(walk->id == id)
         return walk->string;
      walk = walk->idHashLink;
   }
   return NULL;
}

StringTableEntry TagDictionary::idToDefine(S32 id)
{
   S32 index = hashId(id, numBuckets);
   if (index < 0) return NULL;
   TagEntry *walk = idHashBuckets[index];
   while(walk)
   {
      if(walk->id == id)
         return walk->define;
      walk = walk->idHashLink;
   }
   return NULL;
}

//------------------------------------------------------------------------------

void TagDictionary::findIDs(Vector<S32>& out_v,
                       const S32    in_minID,
                       const S32    in_maxID )
{
   //locate all IDs that lie in between minID and maxID

   TagEntry *walk = entryChain;
   while(walk)
   {
      if(walk->id > in_minID && walk->id < in_maxID)
         out_v.push_back(walk->id);
      walk = walk->chain;
   }
   sortIdVector(out_v);
}


//------------------------------------------------------------------------------
void TagDictionary::findStrings(Vector<S32>& out_v, const char*  in_pPattern)
{
   //locate all strings that match the pattern
   //
   TagEntry *walk = entryChain;
   while(walk)
   {
      if (match(in_pPattern, walk->string))
         out_v.push_back(walk->id);
      walk = walk->chain;
   }
   sortIdVector(out_v);
}


//------------------------------------------------------------------------------
void TagDictionary::findDefines(Vector<S32>& out_v, const char*  in_pPattern)
{
   //locate all define strings that match the pattern and add their ID 
   //to the given vector
   //
   TagEntry *walk = entryChain;
   while(walk)
   {
      if (match(in_pPattern, walk->define))
         out_v.push_back(walk->id);
      walk = walk->chain;
   }
   sortIdVector(out_v);
}

//------------------------------------------------------------------------------

bool TagDictionary::match(const char* pattern, const char* str)
{
   //quick and dirty recursive DOS-style wild-card string matcher
   //
   switch (*pattern) {
     case '\0':
      return !*str;

     case '*':
      return match(pattern+1, str) || *str && match(pattern, str+1);

     case '?':
      return *str && match(pattern+1, str+1);

     default:
      return (*pattern == *str) && match(pattern+1, str+1);
   }
}

//------------------------------------------------------------------------------

static S32 QSORT_CALLBACK idCompare(const void *in_p1, const void *in_p2)
{
   return *((S32 *) in_p1) - *((S32 *) in_p2);
}

void TagDictionary::sortIdVector(Vector<S32>& out_v)
{
   dQsort(out_v.address(), out_v.size(), sizeof(S32), idCompare);
}

