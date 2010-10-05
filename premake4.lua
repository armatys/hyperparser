solution "hyperparser"
    configurations { "release" }
    flags { "ExtraWarnings", "NoFramePointer", "OptimizeSpeed" }
    buildoptions { "-ansi" }
    platforms { "native", "x32", "x64" }

project "hyperparser"
    kind "SharedLib"
    language "c"
    location "build"
    files { "src/*.c" }
    targetprefix ""
    targetname "hyperparser"
    
    
    configuration { "macosx" }
        targetdir "build/macosx"
        targetextension ".so"
        linkoptions { "-single_module", "-undefined dynamic_lookup" }
    
    configuration { "linux" }
        includedirs { "/usr/include/lua5.1" }
        targetdir "build/linux"
