# CUDA Setup Injector
This is based in part on nefarius/Injector and lowleveldesign/takedetour


## What?
A DLL injector for the cuda setup that pulls the dependencies to a defined folder. The DLL is embedded in the binary


## Why?
As a linux career professional I fucking hate windows. Every linux build system on the planet is like drop some shit in a folder. There has to be billions, if not trillions, in lost productivity just trying to troubleshoot the god damn dependencies on windows. Oh your path is too long, cuda toolkit dir is wrong, cmake version is wrong. Just kill me. I want to put some shit in a folder. Why is that hard? I don't think I'm missing anything. There's not a flag to just silently install deps to a folder.
  
Then legally I don't think I can repackage and distribute their headers so here you go.

## Caveat
`Injector.exe` will get it's current dir, and pull deps from the cuda installer to the current_dir\CUDA. I wanted to do a full intercept, and still will, but of course it uses fricking ntdll calls so I can't hook those from user space. Right now it copies in real time. I will fix this, but for now I just don't have time. The end goal is no modifications outside the root folder.







