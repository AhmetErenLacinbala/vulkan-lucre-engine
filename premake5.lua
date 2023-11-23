-- premake5.lua
workspace "vulkanRenderEngine"
    architecture "x86_64"
    startproject "lucre"
    configurations 
    {
        "Debug",
        "Release",
        "Dist"
    }

project "lucre"
    language "C++"
    cppdialect "C++17"
    targetdir "bin/%{cfg.buildcfg}"
    objdir ("bin-int/%{cfg.buildcfg}")

    defines
    {
        "LUCRE_VERSION=\"0.4.2\"",
        "PROFILING"
    }

    files 
    {
        "application/**.h", 
        "application/**.cpp"
    }

    includedirs 
    {
        "./",
        "application",
        "application/lucre",
        "engine",
        "vendor",
        "vendor/imgui",
        "resources",
        "vendor/spdlog/include",
        "vendor/yaml-cpp/include",
        "vendor/tinyObjLoader",
        "vendor/box2d/include",
        "vendor/entt/include",
        "vendor/simdjson",
        "vendor/json",
        "vendor/glm",
        "vendor/stb",
    }

    libdirs
    {
    }

    flags
    {
        "MultiProcessorCompile"
    }

    links
    {
        "engine",
        "yaml-cpp",
        "box2d",
        "shaderc",
        "shaderc_util",
        "SPIRV-Tools-opt",
        "SPIRV-Tools",
        "MachineIndependent",
        "OSDependent",
        "GenericCodeGen",
        "OGLCompiler",
        "SPIRV"
    }

    prebuildcommands
    {
    }

    filter "system:linux"

        linkoptions { "-fno-pie -no-pie" }

        files 
        { 
        }
        includedirs 
        {
            "vendor/pamanager/libpamanager/src",
            "/usr/include/glib-2.0",
            "/usr/lib/x86_64-linux-gnu/glib-2.0/include",
            "/usr/lib/glib-2.0/include/",
            "/usr/lib64/glib-2.0/include/",
            "vendor/sdl/include"
        }
        links
        {
            "m",
            "dl", 
            "vulkan",
            "pthread",
            "X11",
            "Xrandr",
            "Xi",
            "libpamanager",
            "pulse",
            "glib-2.0",
            "gio-2.0",
            "sdl_mixer",
            "sdl",
            "libvorbis",
            "libogg",
            "glfw3",
            "resource-system-gnu"
        }
        libdirs
        {
        }
        defines
        {
            "LINUX",
        }

    filter "system:windows"
        defines
        {
        }
        files 
        {
            "resources/windowsEmbeddedResources.rc",
        }
        includedirs 
        {
            "vendor/VulkanSDK/Include",
            "vendor/sdl/include"
        }
        links
        {
            "imagehlp", 
            "dinput8", 
            "dxguid", 
            "user32", 
            "gdi32", 
            "imm32", 
            "ole32",
            "oleaut32",
            "shell32",
            "version",
            "uuid",
            "Setupapi",
            "vulkan-1",
            "sdl_mixer",
            "sdl",
            "libvorbis",
            "libogg",
            "glfw3"
        }
        libdirs 
        {
            "vendor/VulkanSDK/Lib",
        }

    filter "system:macosx"

        files 
        { 
        }
        includedirs 
        {
            "/opt/homebrew/include",
            "/opt/homebrew/Cellar/glib/2.78.1/include/glib-2.0/",
            "/opt/homebrew/Cellar/glib/2.78.1/lib/glib-2.0/include/",
            "/opt/homebrew/include/SDL2/"
        }
        links
        {
            "m",
            "dl", 
            "vulkan",
            "pthread",
            "glib-2.0",
            "gio-2.0",
            "SDL2",
            "SDL2_mixer",
            "vorbis",
            "ogg",
            "glfw",
            "resource-system-gnu"
        }
        libdirs
        {
            "/opt/homebrew/lib",
            "/usr/local/lib/"
        }
        defines
        {
            "MACOSX",
        }

    filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"
        kind "ConsoleApp"

    filter { "action:gmake*", "configurations:Debug"}
        buildoptions { "-ggdb -Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter -Wno-reorder -Wno-expansion-to-defined" }

    filter { "action:gmake*", "configurations:Release"}
        buildoptions { "-Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter -Wno-reorder -Wno-expansion-to-defined" }

    filter { "action:gmake*", "configurations:Dist"}
        buildoptions { "-Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter -Wno-reorder -Wno-expansion-to-defined" }

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "On"
        kind "ConsoleApp"

    filter { "configurations:Dist" }
        defines {
            "NDEBUG",
            "DISTRIBUTION_BUILD"
        }
        optimize "On"
        kind "WindowedApp"

    if _ACTION == 'clean' then
        print("clean the build...")
        os.rmdir("./bin")
        os.rmdir("./bin-int")
        os.remove("./engine.cfg")
        os.remove("./imgui.ini")
        os.remove("./*.scene")
        os.remove("./resources/gnuEmbeddedResources.cpp")
        os.remove("./resources/gnuEmbeddedResources.h")
        os.remove("./**.make")
        os.remove("./Makefile")
        os.remove("./vendor/pamanager/libpamanager/Makefile")
        os.remove("./vendor/atlas/Makefile")
        os.remove("./vendor/box2d/Makefile")
        os.remove("./profiling (open with chrome tracing).json")
        os.rmdir("./vendor/atlas/bin/")
        os.remove("./vendor/atlas/MakefileMakefile")
        os.rmdir("./vendor/box2d/bin")
        os.rmdir("./vendor/box2d/bin-int")
        os.rmdir("./vendor/glfw/build")
        os.rmdir("./vendor/pamanager/libpamanager/bin")
        os.rmdir("./vendor/pamanager/libpamanager/obj")
        os.rmdir("./vendor/sdl/build")
        os.rmdir("./vendor/sdl_mixer/build")
        os.rmdir("./vendor/sfml/build")
        os.rmdir("./vendor/shaderc/bin")
        os.rmdir("./vendor/shaderc/bin-int")
        os.rmdir("./vendor/yaml-cpp/build")
        print("done.")
    end

    include "engine.lua"

