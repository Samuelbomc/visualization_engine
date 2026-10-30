# Vulkan Visualization Engine
Repository testing Vulkan's graphics API. 
The engine is in early development stages, mostly used for personal learning and experimentation with Vulkan features.
As of now, a window will be created and will show some selected 3D geometry rotating.

https://github.com/user-attachments/assets/51fa7a27-de01-4e59-9fbf-b18953fc77c9

## Functionality
The main program receives some geometry via ipc and draws it on screen using Vulkan. geometry_writer.cpp can be used to send some predefined objects rotating for the main program to draw.
