<?php
// Copyright information can be found in the file named COPYING
// located in the root directory of this distribution.

// This class wraps up some project generator specific stuff for the Torque 3D project itself
// simplifying the per project setup, while still allowing the engine to have modules plugged in

class Torque3D
{
    static $sharedConfig = true;
    static $projectName = "";
    
    static function includeDefaultLibs()
    {
        // Libs
        includeLib( 'mng' );
        includeLib( 'png' );
        includeLib( 'ungif' );
        includeLib( 'zlib' );
        includeLib( 'jpeg' );
        includeLib( 'tinyxml' );
        includeLib( 'opcode' );
        includeLib( 'squish' );
        includeLib( 'collada_dom' );
        includeLib( 'pcre' ); 
        includeLib( 'convexDecomp' ); 

        // Use FMOD on consoles
        if ( TorqueGenerator::$platform != "360" && TorqueGenerator::$platform != "ps3" )
        {
           includeLib( 'libvorbis' );
           includeLib( 'libogg' );
           includeLib( 'libtheora' );
        }    
    }
    
    static function beginConfig( $platform, $projectName )
    {
		//linux: error when debuging on shared config
		if ( $platform == "linux")
			self::$sharedConfig = false;
        
        setPlatform( $platform );

        beginProject( $projectName, self::$sharedConfig );
        
        self::includeDefaultLibs();
        
        $ext = "DLL";
        if ( TorqueGenerator::$platform == "mac" )
            $ext = "Bundle";
   

        //some platforms will not want a shared config        
        if ( TorqueGenerator::$platform == "360" || TorqueGenerator::$platform == "ps3")
            self::$sharedConfig = false;

        //begin either a shared lib config, or a static app config
        if ( self::$sharedConfig )
            beginSharedLibConfig( getGameProjectName().'_'.$ext, '{C0FCDFF9-E125-412E-87BC-2D89DB971CAB}', 'game', getGameProjectName() );
        else
            beginAppConfig( getGameProjectName(), '{C0FCDFF9-E125-412E-87BC-2D89DB971CAB}', 'game', getGameProjectName() );
        
        /// Prefs
        addProjectDefine( 'TORQUE_SHADERGEN' );
        addProjectDefine( 'TORQUE_UNICODE' );
        
        if ( self::$sharedConfig )
           addProjectDefine( 'TORQUE_SHARED' );    

        /// For OPCODE
        addProjectDefine( 'BAN_OPCODE_AUTOLINK' );
        addProjectDefine( 'ICE_NO_DLL' );
        addProjectDefine( 'TORQUE_OPCODE' );

        // For libTomCrypt
        addProjectDefine( 'LTC_NO_PROTOTYPES' );
        
        // Additional includes
        addIncludePath( "../../game/shaders" );
        
        addLibIncludePath( "lmng" );
        addLibIncludePath( "lpng" );
        addLibIncludePath( "ljpeg" );
        addLibIncludePath( "lungif" );
        addLibIncludePath( "zlib" );
        addLibIncludePath( "tinyxml" );
        addLibIncludePath( "opcode" );
        addLibIncludePath( "squish" );
        addLibIncludePath( 'convexDecomp' ); 
        
        if ( TorqueGenerator::$platform != "360" && TorqueGenerator::$platform != "ps3" )
        {
          addLibIncludePath( "libvorbis/include" );
          addLibIncludePath( "libogg/include" );
          addLibIncludePath( "libtheora/include" );
        }
        
        // Modules
        includeModule( 'core' );
        includeModule( 'dsound' );
        includeModule( 'fmod');
        includeModule( 'T3D' );
        includeModule( 'advancedLighting' );
        includeModule( 'basicLighting' );
        includeModule( 'collada' );
        
        if ( TorqueGenerator::$platform != "360" && TorqueGenerator::$platform != "ps3" )
        {
          includeModule( 'vorbis' );
          includeModule( 'theora' );
        }
       
        if(TorqueGenerator::$platform == "mac" || TorqueGenerator::$platform == "win32" || TorqueGenerator::$platform == "linux")
           includeModule( 'openal' );

   
        // Dependencies
        
        addProjectDependency( 'lmng' );
        addProjectDependency( 'lpng' );
        addProjectDependency( 'lungif' );
        addProjectDependency( 'ljpeg' );
        addProjectDependency( 'zlib' );
        addProjectDependency( 'tinyxml' );
        addProjectDependency( 'opcode' );
        addProjectDependency( 'squish' );
        addProjectDependency( 'collada_dom' );
        addProjectDependency( 'pcre' );
        addProjectDependency( 'convexDecomp' ); 
        
        if ( TorqueGenerator::$platform != "360" && TorqueGenerator::$platform != "ps3" )
        {
          addProjectDependency( 'libvorbis' );
          addProjectDependency( 'libogg' );
          addProjectDependency( 'libtheora' );
        }
        
        if ( TorqueGenerator::$platform == "mac" )
        {    
            addProjectDefine( '__MACOSX__' );
            addProjectDefine( 'LTM_DESC' );
        }

        if (TorqueGenerator::$platform == "win32")
        {
            setProjectModuleDefinitionFile('../../' . getLibSrcDir() . 'Torque3D/msvc/torque3d.def');

            addProjectDefine( 'UNICODE' );
            addProjectDefine( 'INITGUID' );
            addProjectDefine( '_CRT_SECURE_NO_DEPRECATE' );
            

            addProjectLibInput('COMCTL32.LIB');
            addProjectLibInput('COMDLG32.LIB');
            addProjectLibInput('USER32.LIB');
            addProjectLibInput('ADVAPI32.LIB');
            addProjectLibInput('GDI32.LIB');
            addProjectLibInput('WINMM.LIB');
            addProjectLibInput('WSOCK32.LIB');
            addProjectLibInput('vfw32.lib');
            addProjectLibInput('Imm32.lib');
            addProjectLibInput('d3d9.lib');
            addProjectLibInput('d3dx9.lib');
            addProjectLibInput('DxErr.lib');
            addProjectLibInput('ole32.lib');
            addProjectLibInput('shell32.lib');
            addProjectLibInput('oleaut32.lib');
            addProjectLibInput('version.lib');
			
			addProjectLibInput( 'OpenGL32.lib' );
        }
		
		if (TorqueGenerator::$platform == "linux")
        {			
			addProjectDefine( 'LINUX' );			
			//-ldl -lXxf86vm -lXext -lX11 -lXft -lSDL -lstdc++ -lpthread -lGL
			addProjectLibInput('dl');
			addProjectLibInput('Xxf86vm');
			addProjectLibInput('Xext');
			addProjectLibInput('X11');
			addProjectLibInput('Xft');
			addProjectLibInput('SDL');
			addProjectLibInput('stdc++');
			addProjectLibInput('pthread');
			addProjectLibInput('GL');
			
			if ( !self::$sharedConfig )
				addEngineSrcDir( 'main' );
	    }
        
        // Include project specific sources in the project/buildFiles/config/projectCode.conf
        $projectCode = realpath($argv[1])."/buildFiles/config/projectCode.conf";
        echo( "\n   - Loading project code configuration from ".$projectCode."\n");
        include $projectCode;
    }
    
    static function endConfig()
    {
        //end shared/static config
        if ( self::$sharedConfig )
           endSharedLibConfig();
        else
           endAppConfig();

        //add the shared application only if this is a shared config
        if ( self::$sharedConfig )
        {
            /////// Application Config
            beginSharedAppConfig( getGameProjectName(), '{CDECDFF9-E125-523F-87BC-2D89DB971CAB}' );

                addProjectDefine( 'TORQUE_SHARED' );

                addEngineSrcDir( 'main' );
                
                if (TorqueGenerator::$platform == "win32")
                {
                    addProjectDefine( 'WIN32' );
                    addProjectDependency( getGameProjectName() . '_DLL' );
                }
				
				if (TorqueGenerator::$platform == "linux")
                {
                    addProjectDefine( 'LINUX' );
                    addProjectDependency( getGameProjectName() . '_DLL' );
                }

                if (TorqueGenerator::$platform == "mac")
                {
                    addProjectDefine( '__MACOSX__' );
                    addProjectDependency( getGameProjectName() . '_Bundle' );
                    addProjectDependency( getGameProjectName() . '_Plugin' );
                }

            endSharedAppConfig();
        }
        
        // Add solution references for Visual Studio projects
        if (TorqueGenerator::$platform == "win32" || TorqueGenerator::$platform == "linux" ||TorqueGenerator::$platform == "360" || TorqueGenerator::$platform == "ps3")
        {
           if ( !self::$sharedConfig )
              beginSolutionConfig( getGameProjectName(), '{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}' );
              
              addSolutionProjectRef( getGameProjectName() );
              if ( self::$sharedConfig )
                 addSolutionProjectRef( getGameProjectName() . '_DLL' );
                 
              addSolutionProjectRef( 'collada_dom' );
              addSolutionProjectRef( 'ljpeg' );
              addSolutionProjectRef( 'lmng' );
              addSolutionProjectRef( 'lpng' );
              addSolutionProjectRef( 'lungif' );
              addSolutionProjectRef( 'opcode' );
              addSolutionProjectRef( 'pcre' );
              addSolutionProjectRef( 'squish' );
              addSolutionProjectRef( 'tinyxml' );
              addSolutionProjectRef( 'zlib' );
              addSolutionProjectRef( 'convexDecomp' ); 
              
              if (TorqueGenerator::$platform == "win32" || TorqueGenerator::$platform == "linux")
              {
				 addSolutionProjectRef( 'libvorbis' ); 
                 addSolutionProjectRef( 'libogg' );                 
                 addSolutionProjectRef( 'libtheora' );
              }
              
           if ( !self::$sharedConfig )
              endSolutionConfig();
        }
        
        endProject(self::$sharedConfig);
        
    }
        
}

?>