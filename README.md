# CUDA Setup Injector
This is based in part on nefarius/Injector and lowleveldesign/takedetour

## What?
A DLL injector for the cuda setup that pulls the dependencies to a defined folder. The DLL is embedded in the binary.

## Why?
As a linux career professional I hate windows. Every linux build system on the planet is like drop some shit in a folder. There has to be billions, if not trillions, in lost productivity just trying to troubleshoot the god damn dependencies on windows. Oh your path is too long, cuda toolkit dir is wrong, cmake version is wrong. Just kill me. I want to put some shit in a folder. Why is that hard? I don't think I'm missing anything. There's not a flag to just silently install deps to a folder.
  
Then legally I don't think I can repackage and distribute their headers so here you go. This wouldn't be the first time I've moved mountains because I missed the door. But I think this has just sucked for years. Companies worth TRILLIONS. CEO Worth BILLIONS. Engineers worth MILLIONS. And I can't put some shit in a folder. [Am I wrong](https://media1.tenor.com/images/2df998371c24c21ba3b8bdb06cf1e3b4/tenor.gif?itemid=4575092)? So Jensen can buy a shinier jacket every year for CES. 

Passion has died. Technology is no longer for the everyman. They're starting to pull up the ladder. There is no real dev focus because complete market capitalization means there's no competition. They're weaponizing labor. 

## Caveat
`Injector.exe` will get it's current dir, and pull deps from the cuda installer to the current_dir\CUDA. I wanted to do a full intercept, and still will, but of course it uses fricking ntdll calls so I can't hook those from user space. Right now it copies in real time. I will fix this, but for now I just don't have time. The end goal is no modifications outside the root folder.


