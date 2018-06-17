#Create a simulator object
set ns [new Simulator]
$ns color 1 Blue
  
#Define a 'finish' procedure
proc finish {} {
    exit 0
}

#Create two nodes
set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]

#Create links between the nodes
$ns simplex-link $n0 $n1 100Mb 0ms DropTail
$ns duplex-link $n1 $n2 1000000Mb 0ms DropTail


#Set Queue Size of link (n0-n1) to infinite
#$ns queue-limit $n0 $n1 100000000


#Give node position 
$ns simplex-link-op $n0 $n1 orient up
$ns duplex-link-op $n1 $n2 orient left


#Setup UDP agent on n0
set udp [new Agent/UDP]
$ns attach-agent $n0 $udp
$udp set fid_ 0


#Setup CPU agent on n1
set cpu [new Agent/CPU]
$ns attach-agent $n1 $cpu


#Setup DISK agent on n2
set disk [new Agent/DISK]
$ns attach-agent $n2 $disk


#Setup agent connection
$ns connect $udp $cpu
$ns connect $cpu $disk


#Setup a exponential over UDP connection


set exp [new Application/Traffic/Exponential]
$exp attach-agent $udp
$exp set packetSize_ 131072
#131072 = 128 * 1024


$exp set burst_time_ 0ms
$exp set idle_time_ 100ms
#100ms = 0.1s = 10 tasks/sec
$exp set rate_ 1000000k


#Schedule events for the Exponential agent.
$ns at 0.1 "$exp start"
$ns at 1000 "$exp stop"


#Call the finish procedure after 5 seconds of simulation time
$ns at 1000.1 "finish"


#Run the simulation
$ns run
