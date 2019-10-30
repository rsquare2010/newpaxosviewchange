The project should consist of 
-Dockerfile
-/src

The src directory should consist of
Hostfile.txt
messages.h
port.h
udp.cpp



The hostfile .txt consists of the names of the servers. these names should match the name of the docker container.
Each server needs to be launched in a separate terminal.


The servers can be run after building a docker image from the docker file in the proj directory
We also need to create a docker network and provide that as a parameter when running a docker image.

port.h contains the port number which all the servers will be using
messages.h contains the struct definitions of the different message types and the methods to serialize and deserialize them.
udp.cpp should be the main source file which actually implements pacts.

Running an instance of the docker image takes multiple optional parameters
-h path for the hostfile(this is kinda useless as I read from hardcoded path)
-t integer representing the test case to be run between 1 and 5, its 1 by default

#######NOTE:
Each process sleeps for 60 seconds after a container is run before becoming alive.(sorry about this).
Each process also prints its ip address and Id It picks.



sample run commands
"docker run -it --name server0 --network mynetwork send" // run on defaults, here server0 is the container name, mynetwork is the docker network created, and send is the docker image name.

"docker run -it --name server1 --network mynetwork send -t 3" runs the same instance as before on the same network, the container name here is server1.
The test case to be tested is test case 3.

