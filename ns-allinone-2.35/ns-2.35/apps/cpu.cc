#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/apps/cpu.cc,v 1.21 2005/08/26 05:05:28 tomh Exp $ (Xerox)";
#endif

#include "agent.h"
#include "trafgen.h"
#include "packet.h"
#include "stdio.h"
#include "string.h"
#include "rtp.h"
#include "random.h"
#include "address.h"
#include "ip.h"
//"rtp timestamp" needs the samplerate
#define SAMPLERATE 8000
#define RTP_M 0x0080 // marker for significant events

class CpuAgent : public Agent {
public:
	CpuAgent();
	CpuAgent(packet_t);

	/*virtual void sendmsg(int nbytes, const char *flags = 0)
	{
		sendmsg(nbytes, NULL, flags);
	}*/
	//virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
	virtual void recv(Packet* pkt, Handler*);
	virtual int command(int argc, const char*const* argv);
	//---------------------------------
	void SendToDisk(Packet* pkt, double current_time, double stay_time);
	//---------------------------------

protected:
	int seqno_;
	//---------------------------------
	double Time_start_;
	double Time_head_;
	double Time_tail_;
	double usage;
	double round_time_mean;
	unsigned long busy_time_;
	unsigned long round_time_sum;
	unsigned int num_pkt;
	//---------------------------------

};


static class CpuAgentClass : public TclClass {
public:
	CpuAgentClass() : TclClass("Agent/CPU") {}
	TclObject* create(int, const char*const*) {
		return (new CpuAgent);
	}
} class_cpu_agent;

CpuAgent::CpuAgent() : Agent(PT_UDP), seqno_(-1)
{
	Time_start_ = Scheduler::instance().clock();
	Time_head_ = 0;
	Time_tail_ = 0;
	usage = 0;
	round_time_mean = 0;
	round_time_sum = 0;
	busy_time_ = 0;
	num_pkt = 0;
}


// put in timestamp and sequence number, even though CPU doesn't usually 
// have one.
/*
void CpuAgent::sendmsg(int nbytes, AppData* data, const char* flags)
{
	Packet *p;
	int n;

	assert (size_ > 0);

	n = nbytes / size_;

	if (nbytes == -1) {
		printf("Error:  sendmsg() for CPU should not be -1\n");
		return;
	}	

	// If they are sending data, then it must fit within a single packet.
	if (data && nbytes > size_) {
		printf("Error: data greater than maximum CPU packet size\n");
		return;
	}

	double local_time = Scheduler::instance().clock();
	while (n-- > 0) {
		p = allocpkt();
		hdr_cmn::access(p)->size() = size_;
		hdr_rtp* rh = hdr_rtp::access(p);
		rh->flags() = 0;
		rh->seqno() = ++seqno_;k
		hdr_cmn::access(p)->timestamp() = 
		    (u_int32_t)(SAMPLERATE*local_time);
		// add "beginning of talkspurt" labels (tcl/ex/test-rcvr.tcl)
		if (flags && (0 ==strcmp(flags, "NEW_BURST")))
			rh->flags() |= RTP_M;
		p->setdata(data);
		target_->recv(p);
	}
	n = nbytes % size_;
	if (n > 0) {
		p = allocpkt();
		hdr_cmn::access(p)->size() = n;
		hdr_rtp* rh = hdr_rtp::access(p);
		rh->flags() = 0;
		rh->seqno() = ++seqno_;
		hdr_cmn::access(p)->timestamp() = 
		    (u_int32_t)(SAMPLERATE*local_time);
		// add "beginning of talkspurt" labels (tcl/ex/test-rcvr.tcl)
		if (flags && (0 == strcmp(flags, "NEW_BURST")))
			rh->flags() |= RTP_M;
		p->setdata(data);
		target_->recv(p);
	}
	idle();
}*/

void CpuAgent::recv(Packet* pkt, Handler*)
{

	if (app_ ) {
		// If an application is attached, pass the data to the app
		hdr_cmn* h = hdr_cmn::access(pkt);
		app_->process_data(h->size(), pkt->userdata());

	} else if (pkt->userdata() && pkt->userdata()->type() == PACKET_DATA) {
		// otherwise if it's just PacketData, pass it to Tcl
		//
		// Note that a Tcl procedure Agent/Cpu recv {from data}
		// needs to be defined.  For example,
		//
		// Agent/Cpu instproc recv {from data} {puts data}

		PacketData* data = (PacketData*)pkt->userdata();

		hdr_ip* iph = hdr_ip::access(pkt);
                Tcl& tcl = Tcl::instance();
		tcl.evalf("%s process_data %d {%s}", name(),
		          iph->src_.addr_ >> Address::instance().NodeShift_[1],
			  data->data());
	}
	//--------------------------------------------------------------------	
	double current_time = Scheduler::instance().clock();
	double pktrest_time = hdr_cmn::access(pkt)->PKT_resttime();
	double stay_time;
	if (pktrest_time >= 0.25){
		stay_time = 0.25;
	}else{
		stay_time = pktrest_time;
	}
	//--------------------------------------------------------------------	
	if (current_time >= Time_tail_){
		/* usage */
		busy_time_ += stay_time;
		usage = busy_time_ / (current_time + stay_time - Time_start_);
		
		/* refresh time */
		Time_head_ = current_time;
		Time_tail_ = current_time + stay_time;

		/* send to Disk */
		SendToDisk(pkt, current_time, stay_time);

	}else if ((current_time+stay_time) > Time_tail_){
		/* usage */
		busy_time_ += (current_time + stay_time - Time_tail_);
		usage = busy_time_ / (current_time + stay_time - Time_start_);
		
		/* refresh time */
		Time_tail_ = current_time + stay_time;

		/* send to Disk */
		SendToDisk(pkt, current_time, stay_time);

	}else{
		/* send to Disk */
		SendToDisk(pkt, current_time, stay_time);
	}
	//--------------------------------------------------------------------	
}


int CpuAgent::command(int argc, const char*const* argv)
{
	if (argc == 4) {
		if (strcmp(argv[1], "send") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data);
			return (TCL_OK);
		}
	} else if (argc == 5) {
		if (strcmp(argv[1], "sendmsg") == 0) {
			PacketData* data = new PacketData(1 + strlen(argv[3]));
			strcpy((char*)data->data(), argv[3]);
			sendmsg(atoi(argv[2]), data, argv[4]);
			return (TCL_OK);
		}
	}
	return (Agent::command(argc, argv));
}

//------------------------------------------------------------------------------
void CpuAgent::SendToDisk(Packet* pkt, double current_time, double stay_time)
{
	if (hdr_cmn::access(pkt)->PKT_resttime() >= 0.25){
		hdr_cmn::access(pkt)->PKT_resttime() -= 0.25;
		(void)Scheduler::instance().schedule(target_, pkt, stay_time);
	}else{
		round_time_sum += ((current_time + stay_time) - (hdr_cmn::access(pkt)->PKT_sendtime()));
		num_pkt++;
		round_time_mean = round_time_sum / num_pkt ;
		Packet::free(pkt);
	}
}
//------------------------------------------------------------------------------