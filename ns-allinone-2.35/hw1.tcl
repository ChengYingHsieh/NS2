#Create a simulator object
set ns [new Simulator]
$ns color 1 Blue
  
#Open the NAM trace file
#set nf [open hw1.nam w]
#$ns namtrace-all $nf

#Define a 'finish' procedure
proc finish {} {
	
	#global ns nf
	#$ns flush-trace
	
	#Close the NAM trace file
	#close $nf
	
	#Execute NAM on the trace file
	#exec nam hw1.nam &
	exit 0
}

#Create two nodes
set n0 [$ns node]
set n1 [$ns node]

#Create links between the nodes
$ns simplex-link $n0 $n1 100Mb 0ms DropTail

#Set Queue Size of link (n0-n1) to 10
$ns queue-limit $n0 $n1 100000000

#Give node position 
$ns simplex-link-op $n0 $n1 orient right

#Setup a UDP attach
set udp [new Agent/UDP]
$ns attach-agent $n0 $udp
$udp set fid_ 0

#Set lossmonitor on n1
set lm [new Agent/LossMonitor]
$ns attach-agent $n1 $lm

#Setup agent connection
$ns connect $udp $lm

#Setup a exponential over UDP connection
set round 2
$lm set agent_num_ $round

for {set i 1} {$i < $round} {incr i 1} {
	set exp($i) [new Application/Traffic/Exponential]
	$exp($i) attach-agent $udp
	$exp($i) set packetSize_ 131072
	#131072 = 128 * 1024
	
	$exp($i) set burst_time_ 0ms
	$exp($i) set idle_time_ 100ms
	$exp($i) set rate_ 1000000k
	
	#Schedule events for the Exponential agent.
	$ns at 0.1 "$exp($i) start"
	$ns at 1000 "$exp($i) stop"
}


#Call the finish procedure after 5 seconds of simulation time
$ns at 1000.1 "finish"

#Run the simulation
$ns run
