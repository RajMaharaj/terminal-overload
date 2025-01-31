// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

#include "platform/platform.h"
#include "core/module.h"
#include "core/util/tVector.h"
#include "core/strings/stringFunctions.h"


//#define DEBUG_SPEW
#define DEBUG_SPEW_LEVEL 2


Module* Module::smFirst;


//-----------------------------------------------------------------------------

bool Module::_constrainedToComeBefore( Module* module, Mode mode )
{
   if( module == this )
      return false;
      
   for( Dependency* dependency = _getDependencies( mode );
        dependency != NULL; dependency = dependency->mNext )
   {
      Module* depModule = dependency->mModule;
      if( !depModule )
      {
         depModule = ModuleManager::findModule( dependency->mModuleName );
         if( !depModule )
         {
            // Module does not exist.  Only emit a warning here so that modules
            // can be omitted from a link without requiring the module definitions
            // to be adapted.
            
            Platform::outputDebugString( "[ModuleManager] Module %s of '%s' depends on module '%s' which does not exist",
               mode == Module::ModeInitialize ? "init" : "shutdown",
               module->getName(), dependency->mModuleName );
            continue;
         }
         
         dependency->mModule = depModule;
      }
      
      if( dependency->mType == DependencyBefore )
      {
         if(    depModule == module
             || depModule->_constrainedToComeBefore( module, mode ) )
            return true;
      }
   }
   
   return false;
}

//-----------------------------------------------------------------------------

bool Module::_constrainedToComeAfter( Module* module, Mode mode )
{
   if( module == this )
      return false;

   for( Dependency* dependency = _getDependencies( mode );
        dependency != NULL; dependency = dependency->mNext )
   {
      Module* depModule = dependency->mModule;
      if( !depModule )
      {
         depModule = ModuleManager::findModule( dependency->mModuleName );
         if( !depModule )
         {
            // Module does not exist.  Only emit a warning here so that modules
            // can be omitted from a link without requiring the module definitions
            // to be adapted.
            
            Platform::outputDebugString( "[ModuleManager] Module %s of '%s' depends on module '%s' which does not exist",
               mode == Module::ModeInitialize ? "init" : "shutdown",
               module->getName(), dependency->mModuleName );
            continue;
         }
         
         dependency->mModule = depModule;
      }
      
      if( dependency->mType == DependencyAfter )
      {
         if(    depModule == module
             || depModule->_constrainedToComeAfter( module, mode ) )
            return true;
      }
   }
   
   return false;
}

//-----------------------------------------------------------------------------

String ModuleManager::_moduleListToString( Vector< Module* >& moduleList )
{
   StringBuilder str;
   
   const U32 numModules = moduleList.size();
   bool isFirst = true;
   for( U32 i = 0; i < numModules; ++ i )
   {
      if( !isFirst )
         str.append( " -> " );
         
      str.append( moduleList[ i ]->getName() );
      
      isFirst = false;
   }
   
   return str.end();
}

//-----------------------------------------------------------------------------

void ModuleManager::_printModuleList( Vector< Module* >& moduleList )
{
   Platform::outputDebugString( _moduleListToString( moduleList ) );
}

//-----------------------------------------------------------------------------

void ModuleManager::_insertIntoModuleList( Module::Mode mode, Vector< Module* >& moduleList, Module* module )
{
   // If this module is being overridden, switch over to
   // the module overriding it.
      
   Module* override;
   do
   {
      override = _findOverrideFor( module );
      if( override )
         module = override;
   }
   while( override != NULL );

   // If we are already on the list, return.
   
   if( _getIndexOfModuleInList( moduleList, module ) != -1 )
      return;
      
   // If we don't have dependencies, just push the module
   // to the back of the list.
   
   if( !module->_getDependencies( mode ) )
   {
      #if defined( DEBUG_SPEW ) && DEBUG_SPEW_LEVEL > 1
      Platform::outputDebugString( "[ModuleManager] Appending '%s' to '%s'",
         module->getName(), _moduleListToString( moduleList ).c_str() );
      #endif

      moduleList.push_back( module );         
      return;
   }
   
   // First make sure that all 'after' dependencies are in the list.
      
   #if defined( DEBUG_SPEW ) && DEBUG_SPEW_LEVEL > 1
   Platform::outputDebugString( "[ModuleManager] Resolving %s dependencies of '%s'",
      mode == Module::ModeInitialize ? "init" : "shutdown",
      module->getName() );
   #endif
   
   for( Module::Dependency* dependency = module->_getDependencies( mode );
        dependency != NULL; dependency = dependency->mNext )
   {
      if( dependency->mType != Module::DependencyAfter )
         continue;
         
      dependency->mModule = findModule( dependency->mModuleName );
      if( !dependency->mModule )
         continue; // Allow modules to not exist.
         
      if( _getIndexOfModuleInList( moduleList, dependency->mModule ) == -1 )
         _insertIntoModuleList( mode, moduleList, dependency->mModule );
   }
   
   AssertFatal( _getIndexOfModuleInList( moduleList, module ) == -1,
      avar( "ModuleManager::_insertModuleIntoList - Cycle in 'after' %s dependency chain of '%s'",
         mode == Module::ModeInitialize ? "init" : "shutdown",
         module->getName() ) );
      
   // Now add the module itself.
   
   const U32 numModules = moduleList.size();
   for( U32 i = 0; i < numModules; ++ i )
   {
      const bool thisBeforeCurrent  = module->_constrainedToComeBefore( moduleList[ i ], mode );
      const bool currentAfterThis   = moduleList[ i ]->_constrainedToComeAfter( module, mode );
      
      AssertFatal( !( thisBeforeCurrent && currentAfterThis ),
         avar( "ModuleManager::_insertModuleIntoList - Ambiguous %s placement of module '%s' relative to '%s'",
            mode == Module::ModeInitialize ? "init" : "shutdown",
            module->getName(), moduleList[ i ]->getName() ) );
      
      // If no contraints relate us to this module,
      // push us one more position back in the line.
      
      if( !thisBeforeCurrent && !currentAfterThis )
         continue;
         
      // If this module is contrained to come before the
      // module at our current position but that module does
      // not actually have dependencies of its own, make sure
      // that module is at the back of the module list so that
      // if we have more dependencies, it will not prevent us
      // from correctly positioning us in relation to them.
      
      if( thisBeforeCurrent && !moduleList[ i ]->_getDependencies( mode ) && i != numModules - 1 )
      {
         #if defined( DEBUG_SPEW ) && DEBUG_SPEW_LEVEL > 1
         Platform::outputDebugString( "[ModuleManager] Pushing '%s' to back end of chain for resolving '%s'",
            moduleList[ i ]->getName(), module->getName() );
         #endif
         
         Module* depModule = moduleList[ i ];
         moduleList.erase( i );
         -- i;
         
         moduleList.push_back( depModule );
         continue;
      }
      
      // Try the reverse constraint with all remaining modules in the list.
      // If there is one for which we have one, then the placement of this
      // module is ambiguous.
      
      for( U32 n = i + 1; n < numModules; ++ n )
         AssertFatal( !(    moduleList[ n ]->_constrainedToComeBefore( module, mode )
                         || module->_constrainedToComeAfter( moduleList[ n ], mode ) ),
            avar( "ModuleManager::_insertModuleIntoList - Ambiguous %s constraint on module '%s' to come before '%s' yet after '%s'",
               mode == Module::ModeInitialize ? "init" : "shutdown",
               module->getName(),
               moduleList[ i ]->getName(),
               moduleList[ n ]->getName() ) );
      
      // Add the module at this position.
   
      #if defined( DEBUG_SPEW ) && DEBUG_SPEW_LEVEL > 1
      Platform::outputDebugString( "[ModuleManager] Inserting '%s' at index %i into '%s'",
         module->getName(), i, _moduleListToString( moduleList ).c_str() );
      #endif

      moduleList.insert( i, module );
      return;
   }
   
   // No constraint-based position.  Just append.

   #if defined( DEBUG_SPEW ) && DEBUG_SPEW_LEVEL > 1
   Platform::outputDebugString( "[ModuleManager] Appending '%s' to '%s'",
      module->getName(), _moduleListToString( moduleList ).c_str() );
   #endif

   moduleList.push_back( module );
}

//-----------------------------------------------------------------------------

Module* ModuleManager::_findOverrideFor( Module* module )
{
   const char* name = module->getName();
   
   for( Module* ptr = Module::smFirst; ptr != NULL; ptr = ptr->mNext )
      for( Module::Override* override = ptr->mOverrides; override != NULL; override = override->mNext )
         if( dStricmp( override->mModuleName, name ) == 0 )
            return ptr;
   
   return NULL;
}

//-----------------------------------------------------------------------------

S32 ModuleManager::_getIndexOfModuleInList( Vector< Module* >& moduleList, Module* module )
{
   const U32 numModules = moduleList.size();
   for( U32 i = 0; i < numModules; ++ i )
      if( moduleList[ i ] == module )
         return i;
         
   return -1;
}

//-----------------------------------------------------------------------------

S32 ModuleManager::_getIndexOfModuleInList( Vector< Module* >& moduleList, const char* moduleName )
{
   const U32 numModules = moduleList.size();
   for( U32 i = 0; i < numModules; ++ i )
      if( dStricmp( moduleList[ i ]->getName(), moduleName ) == 0 )
         return i;
         
   return -1;
}

//-----------------------------------------------------------------------------

void ModuleManager::_createModuleList( Module::Mode mode, Vector< Module* >& moduleList )
{
   for( Module* module = Module::smFirst; module != NULL; module = module->mNext )
      _insertIntoModuleList( mode, moduleList, module );
}

//-----------------------------------------------------------------------------

void ModuleManager::initializeSystem()
{
   Vector< Module* > modules;
   
   _createModuleList( Module::ModeInitialize, modules );
   
   const U32 numModules = modules.size();
   for( U32 i = 0; i < numModules; ++ i )
   {
      Module* module = modules[ i ];      
      if( !module->mIsInitialized )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[ModuleManager] Initializing %s",
            module->getName() );
         #endif
         
         module->initialize();
         module->mIsInitialized = true;
      }
   }
}

//-----------------------------------------------------------------------------

void ModuleManager::shutdownSystem()
{
   Vector< Module* > modules;
   
   _createModuleList( Module::ModeShutdown, modules );

   const U32 numModules = modules.size();
   for( U32 i = 0; i < numModules; ++ i )
   {      
      if( modules[ i ]->mIsInitialized )
      {
         #ifdef DEBUG_SPEW
         Platform::outputDebugString( "[ModuleManager] Shutting down %s",
            modules[ i ]->getName() );
         #endif
         
         modules[ i ]->shutdown();
         modules[ i ]->mIsInitialized = false;
      }
   }
}

//-----------------------------------------------------------------------------

Module* ModuleManager::findModule( const char* name )
{
   for( Module* module = Module::smFirst; module != NULL; module = module->mNext )
      if( dStricmp( module->getName(), name ) == 0 )
         return module;
   
   return NULL;
}
