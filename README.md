### gpu poisoning; hide the payload inside the gpu memory.

##### after my older [repo](https://gitlab.com/ORCA000/t.d.p), in which i used the thread description to hide the payload, i wanted to find new way, so now im using **nividia** gpu memory using cuda api's to allocate, write, and free when there is no need for the payload to be found in memory.

#### Steps:
1- first, we need to setup and initialize some structs / and run our ve handler.

2- second, we run the **stageless** payload, we **must** use stageless, to know where
the reflection will land (im using cobalt strike and it uses dll reflective loader), we only care about the `2nd stage` cz thats the actual 
payload. i didnt add any tricks to this step, just a virtualalloc and a rwx section

3- now when the payload goes to sleep, we copy the payload to the gpu memory, and clean the payload in the real memory.

4- when the sleep is done, the veh will handle the exception (EXCEPTION_ACCESS_VIOLATION) by re-setting the memory permissions to `PAGE_EXECUTE_READWRITE`, and placing the payload back in place from the gpu.


#### Demo:
![img](https://gitlab.com/ORCA000/gp/-/raw/main/images/demo1.png)
#### Thanks for : [vxunderground papers](https://github.com/vxunderground/VXUG-Papers/blob/main/GpuMemoryAbuse.cpp)


<br>
<br>
<br>



