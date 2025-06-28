Shovel Knight Modding Framework: From Investigation to a Working Modding Framework

Welcome, future Shovel Knight modders! This document details the journey of reverse-engineering the Shovel Knight game engine and building a robust, multi-part modding framework from the ground up. This framework aims to provide a powerful yet user-friendly platform for creating your own modifications for the game.

The Journey: A Technical Overview

Phase 1: Initial Investigation – Laying the Groundwork

Our initial exploration into modding Shovel Knight focused on two primary avenues: static asset modification (directly altering game files) and runtime memory modification (changing values in the game's active memory).

Through extensive debugging with tools like Cheat Engine, a crucial insight emerged: attempting to pinpoint a stable, simple memory address for dynamic elements like the "current song ID" proved unreliable. The game's complexity, coupled with noise from the graphics driver, made such an approach unstable. This discovery was pivotal, guiding us towards a more resilient, code-based methodology.

Phase 2: Reverse Engineering with IDA Pro – Unveiling Game Internals

With the initial challenges identified, we delved deeper using IDA Pro, a powerful disassembler, to analyze the game's executable and its imported libraries.

    Audio System: Our analysis confirmed that Shovel Knight utilizes the FMOD audio engine. We successfully pinpointed the exact FMOD::Event::start function responsible for playing sounds, providing a key hook point for audio manipulation.

    Asset Loading: We meticulously traced the game's internal list of .pak archive names and followed the execution flow through multiple layers of function calls. This allowed us to locate the core asset loading routines, revealing how the game accesses its various assets.

    The "Golden Key" – YCG_Hash Algorithm: The most significant breakthrough during this phase was the discovery of the ShovelKnightRE repository. This invaluable resource provided the proprietary YCG_Hash algorithm. This algorithm is the secret sauce for how the game engine converts human-readable string names (e.g., "music_title") into unique integer IDs for all its assets. This discovery unlocked the ability to reliably identify and target specific in-game elements.

    The Relative Virtual Address (RVA): We accurately calculated the Relative Virtual Address (RVA) for the game's primary sound-playing function, which is 0xA580. This RVA is crucial because it provides a stable and consistent offset for hooking this function, ensuring our modifications work reliably every time the game is launched, regardless of where the game's executable loads in memory.

Phase 3: Building the Modding Platform – Bringing It All Together

With the reverse-engineering complete, the focus shifted to constructing a functional modding framework. We successfully built and integrated three powerful components:

    A D3D9 Proxy Loader (d3d9_proxy.dll): This serves as the core of our modding platform. By acting as a proxy for the legitimate d3d9.dll (DirectX 9 Dynamic Link Library), our custom DLL ensures that our code loads before the game's rendering engine. This grants us complete control over the game's environment. The proxy successfully creates a debug console for real-time feedback and automatically loads other mod DLLs.

    A Python-based Tool and Injector: We developed a fully functional GUI application using CustomTkinter. This intuitive tool automates the process of finding the Shovel Knight game installation, launching it correctly through Steam (crucially, preserving save files), and injecting our custom DLLs into the game's process.

    A Functional Mod Library (library.cpp): To prove the entire system's efficacy, we created a C++ mod. The latest iteration of this mod successfully:

        Loads seamlessly via the proxy loader.

        Utilizes MinHook to intercept the game's sound function at the pre-calculated RVA.

        Correctly reads the Sound ID directly from the ECX register, which holds a pointer to the sound event's data structure.

        Prints the identified sounds to the debug console in real-time as they play in-game.

In essence, we now have a working modding framework capable of executing custom code directly within the Shovel Knight game.

The New Roadmap: Building a True Modding Platform

Our vision is to transform this powerful personal tool into a resource for the entire Shovel Knight modding community. The following roadmap outlines the steps to achieve this.

Phase 1: Finalize Reverse Engineering & Create the BGM Mod

Objective: Complete the original Background Music (BGM) mod. This serves as a perfect test case for the framework and will provide the first complete "feature" demonstrating its capabilities.

    Find the Song Hash Offset: With the sound hook successfully in place, the next step involves using IDA's debugger to meticulously examine the data structure pointed to by the ECX register. The goal is to pinpoint the exact offset within this structure where the song's unique hash ID is stored.

    Build the Song Database: We will expand the std::map within library.cpp. This map will store a comprehensive list of song names and their corresponding pre-calculated hashes, generated using the ycg_hash.py script. This database will allow us to easily translate raw hash IDs into recognizable song titles.

    Draw the UI: To provide real-time feedback to the player, we will hook into a rendering function (such as IDirect3DDevice9::Present or EndScene from our d3d9_proxy.dll). Using a D3D9 text-drawing method, we will display the name of the current song directly on the game screen.

Phase 2: Platform Expansion – Making Modding Easy

Objective: Integrate high-level features into our proxy loader, enabling other modders to create content without needing to delve into complex C++ or assembly code.

    Integrate a Scripting Engine (Lua): This is arguably the most significant step towards user-friendliness. We will embed the Lua scripting language directly into our d3d9_proxy.dll. The proxy will be responsible for loading and executing .lua files from a designated scripts folder, allowing for dynamic and easily modifiable game behavior.

    Create a Scripting API: To empower Lua modders, we will expose our core C++ functions to the Lua environment. For example, a C++ function like GetCurrentSongName() could be made accessible in Lua as ShovelKnight.GetSongName(), providing a clear and intuitive way for scripts to interact with the game.

    Integrate a UI Framework (Dear ImGui): To simplify in-game user interface creation, we will integrate the popular Dear ImGui library into our proxy. This will provide all mods (both C++ and Lua) with a powerful, simple, and consistent way to create their own custom in-game menus, windows, debug displays, and other interactive elements.

Phase 3: Community Toolkit Release

Objective: Package and document our tools for easy distribution and use by the wider Shovel Knight modding community.

    Refine the Python Launcher: The Python GUI application will be enhanced with additional features. This includes functionalities such as managing installed Lua scripts, enabling or disabling mods, and providing an interface for editing configuration files.

    Write Documentation: Clear and concise documentation is paramount for community adoption. We will create a simple yet comprehensive README.md file that explains:

        How to install the d3d9_proxy.dll.

        Where to place mod DLLs and Lua script files.

        How to use the basic API functions exposed to Lua.

    Bundle for Release: Finally, we will package all components into a single, easily distributable ZIP file. This bundle will include our d3d9_proxy.dll, the Python launcher executable, and some example mods/scripts to serve as a starting point for new modders.