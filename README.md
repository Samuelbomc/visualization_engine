# Vulkan Visualization Engine
Repository testing Vulkan's graphics API. 
The engine is in early development stages, mostly used for personal learning and experimentation with Vulkan features.
As of now, a window will be created and will show some selected 3D geometry rotating.
<img width="926" height="606" alt="image" src="https://github.com/user-attachments/assets/3d958330-6ebd-4a33-92bb-a9f9f3340ae9" />
The main program receives some geometry via ipc and draws it on screen using Vulkan. geometry_writer.cpp can be used to send some predefined objects rotating for the main program to draw.
