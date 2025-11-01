# MemoryManipulationTest
My first program in terms of practicing memory management, memory editing and reverse engineering I decided to use roblox in a baseplate singleplayer world as its a game I've played, so it would be a good point to learn without violating any agreements 
Had to decompile a static version of the exe of roblox, through the built in studio application, I reconstructed the games structure based off how they were formed there, after finding the offsets of data structures by viewing how the memory changed in repsonse to stimuli, I made this program, which allows to externally change the values as well as the clientside checks for tampering

This has been updated a few times as well as a new version which involves DLL injection to run within the process's memory space to execute its own functions, though those will remain private for now as they're being worked on
