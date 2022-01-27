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

    defines
    {
        "LUCRE_VERSION=\"0.1.1\"",
    }

    files 
    {
        "application/**.h", 
        "application/**.cpp",
    }

    includedirs 
    {
        "./",
        "application",
        "application/lucre",
        "engine",
        "resources",
        "vendor/sdl/include",
        "vendor/spdlog/include",
        "vendor/yaml-cpp/include",
        "vendor/tinyObjLoader",
        "vendor/box2d/include",
        "vendor/glm",
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
        "glfw3",
        "sdl_mixer",
        "sdl",
        "libvorbis",
        "libogg",
        "yaml-cpp",
        "box2d"
    }

    prebuildcommands
    {
        "scripts/compileShaders.sh"
    }

    filter "system:linux"

        linkoptions { "-fno-pie -no-pie" }

        prebuildcommands
        {
        }

        files 
        { 
        }
        includedirs 
        {
            "vendor/pamanager/libpamanager/src",

            -- resource system: glib-2.0
            -- this should actually be `pkg-config glib-2.0 --cflags`
            "/usr/include/glib-2.0",
            "/usr/lib/x86_64-linux-gnu/glib-2.0/include",
            "/usr/lib/glib-2.0/include/",
            "/usr/lib64/glib-2.0/include/",
            -- end resource system: glib-2.0
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
        }
        libdirs 
        {
        }

    filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"
        kind "ConsoleApp"

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

    include "engine.lua"

