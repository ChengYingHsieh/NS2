#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/apps/cpu.cc,v 1.21 2005/08/26 05:05:28 tomh Exp $ (Xerox)";
#endif

#include <iostream>
#include <stdio.h>
#include "agent.h"
#include "trafgen.h"
#include "packet.h"
#include "stdio.h"
#include "string.h"
#include "rtp.h"
#include "random.h"
#include "ranvar.h"
#include "address.h"
#include "ip.h"
#include "udp.h"
//"rtp timestamp" needs the samplerate
#define SAMPLERATE 8000
#define RTP_M 0x0080 // marker for significant events

class CpuAgent : public Agent {
public:
	CpuAgent();
	CpuAgent(packet_t);

	virtual void sendmsg(int nbytes, const char *flags = 0)
	{
		sendmsg(nbytes, NULL, flags);
	}
	virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
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
	long double busy_time_;
	long double round_time_sum;
	unsigned int num_pkt;
	ExponentialRandomVariable HowLongPktLive;
	//---------------------------------

};


static class CpuAgentClass : public TclClass {
public:
	CpuAgentClass() : TclClass("Agent/CPU") {}
	TclObject* create(int, const char*const*) {
		return (new CpuAgent);
	}
} class_cpu_agent;

CpuAgent::CpuAgent() : Agent(PT_CPU), seqno_(-1)
{
	Time_start_ = Scheduler::instance().clock();
	Time_head_ = 0;
	Time_tail_ = 0;
	usage = 0;
	round_time_mean = 0;
	round_time_sum = 0;
	busy_time_ = 0;
	num_pkt = 0;
	HowLongPktLive.setavg(0.5);
}


// put in timestamp and sequence number, even though CPU doesn't usually 
// have one.

void CpuAgent::sendmsg(int nbytes, AppData* data, const char* flags)
{
	Packet *p;
	int n;


	double local_time = Scheduler::instance().clock();
	p = allocpkt();
	hdr_cmn::access(p)->size() = nbytes;
	hdr_rtp* rh = hdr_rtp::access(p);
	rh->flags() = 0;
	rh->seqno() = ++seqno_;
	hdr_cmn::access(p)->timestamp() = 
	    (u_int32_t)(SAMPLERATE*local_time);
	// add "beginning of talkspurt" labels (tcl/ex/test-rcvr.tcl)
	
	//------------------------------------------------
	hdr_cmn::access(p)->PKT_sendtime() = local_time;
	hdr_cmn::access(p)->PKT_resttime() = HowLongPktLive.value();
	//------------------------------------------------
	
	if (flags && (0 == strcmp(flags, "NEW_BURST")))
		rh->flags() |= RTP_M;
	p->setdata(data);
	target_->recv(p);
	
	idle();
}

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
	Packet* p = allocpkt();
	Packet::free(pkt);
	/*
	hdr_cmn::access(p)->size() = hdr_cmn::access(pkt)->size_;
	
	
	hdr_rtp* rh = hdr_rtp::access(p);
	rh->flags() = 0;
	rh->seqno() = hdr_rtp::access(pkt)->seqno_;//++seqno_;
	*/
	/*
	hdr_cmn::access(p)->timestamp() = hdr_cmn::access(pkt)->ts_;
	    //(u_int32_t)(SAMPLERATE*local_time);
	hdr_cmn::access(p)->PKT_sendtime() = hdr_cmn::access(pkt)->PKT_sendtime_;
	hdr_cmn::access(p)->PKT_resttime() = hdr_cmn::access(pkt)->PKT_resttime_;
	
	*/
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
		busy_time_ = busy_time_ + stay_time;
		usage = busy_time_ / (current_time + stay_time - Time_start_);
		
		/* refresh time */
		Time_head_ = current_time;
		Time_tail_ = current_time + stay_time;

		/* send to Disk */
		SendToDisk(p, current_time, stay_time);

	}else if ((current_time+stay_time) > Time_tail_){
		/* usage */
		busy_time_ = busy_time_ + (current_time + stay_time - Time_tail_);
		usage = busy_time_ / (current_time + stay_time - Time_start_);
		
		/* refresh time */
		Time_tail_ = current_time + stay_time;

		/* send to Disk */
		SendToDisk(p, current_time, stay_time);

	}else{
		/* send to Disk */
		SendToDisk(p, current_time, stay_time);
	}
	//--------------------------------------------------------------------	
	cout << "-----------------------------" << endl;
	cout << "CPU Usage = " << usage << endl;
	cout << "PKT round time = " << round_time_mean << endl;
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
		hdr_cmn::access(pkt)->PKT_resttime() = hdr_cmn::access(pkt)->PKT_resttime() - 0.25;
		
		cout << "target_ = "<< target_ << endl;

		(void)Scheduler::instance().schedule(target_, pkt, stay_time);

	}else{
		round_time_sum = round_time_sum + ((current_time + stay_time) - (hdr_cmn::access(pkt)->PKT_sendtime()));
		num_pkt++;
		round_time_mean = round_time_sum / num_pkt ;
		Packet::free(pkt);
	}
}
//------------------------------------------------------------------------------
