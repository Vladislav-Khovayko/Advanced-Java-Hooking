# Advanced Java Hooking for the HotSpot JVM
This repository contains a detailed write-up and implementation example for placing detour hooks on Java functions in the HotSpot JVM, this implementation does not use pattern scanning or JVMTI.

Features:
 - Intercept calls made to the Java function of your choice.
 - View and modify the values of the registers and stack of intercepted calls.
 - View and modify the values of the arguments passed in to the Java function that was hooked.
 - Run your own code before and after the hooked Java function runs.
 - Intercept the return value of the hooked Java function.
 - Prevent the hooked Java function from running, run your own code instead.
 - Replace the hooked Java function with a completely different Java function.
 - Safe JNI usage within the detour.
 - Low performance impact, can handle 100k+ calls per second easily.

Due to the large amount of different JVMs, this code will most likely not work out of the box, you will need to make adjustments to it, please see the write-up that explains everything in detail so you know what to do.

Not all the discoveries/code in here were made by me, this project would not have been possible without these people:
 - [Lefraudeur](https://github.com/Lefraudeur/)
 - [Sebastien Spirit](https://github.com/SystematicSkid)
 - [ctsmax](https://github.com/ctsmax/)
 - Members of the [unknowncheats minecraft community](https://www.unknowncheats.me/forum/minecraft/) and [this post](https://www.unknowncheats.me/forum/minecraft/517132-dope-hooking-javas-interpreted-methods.html)

License:
 - Different parts of the code in this repository are covered by different licenses, the majority of it is GPLv3.

Example of hook being placed on Minecraft's Render.renderLivingLabel method to edit nametags:
<img width="1732" height="953" alt="nametags_example" src="https://github.com/user-attachments/assets/577f4499-6e1a-4a4d-883d-78c69de2a593" />
