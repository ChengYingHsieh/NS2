#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/apps/disk.cc,v 1.21 2005/08/26 05:05:28 tomh Exp $ (Xerox)";
#endif

#include <iostream>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
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
//"rtp timestamp" needs the samplerate
#define SAMPLERATE 8000
#define RTP_M 0x0080 // marker for significant events

struct Disk_struct {
	double Time_start_;
	double Time_head_;
	double Time_tail_;
	double usage;
	long double busy_time;
	ExponentialRandomVariable HowLongPktStay;
};


class DiskAgent : public Agent {
public:
	DiskAgent();
	/*virtual void sendmsg(int nbytes, const char *flags = 0)
	{
		sendmsg(nbytes, NULL, flags);
	}*/
	//virtual void sendmsg(int nbytes, AppData* data, const char *flags = 0);
	virtual void recv(Packet* pkt, Handler*);
	virtual int command(int argc, const char*const* argv);
	int PickNum();

protected:
	int seqno_;
	Disk_struct Disk[4];
};


static class DiskAgentClass : public TclClass {
public:
	DiskAgentClass() : TclClass("Agent/DISK") {}
	TclObject* create(int, const char*const*) {
		return (new DiskAgent());
	}
} class_disk_agent;


DiskAgent::DiskAgent() : Agent(PT_UDP), seqno_(-1)
{
	double Time_now_ = Scheduler::instance().clock();
	
	cout << "Here is THE DISK " << endl;


	for (int i=0; i<4; i++) {
		Disk[i].Time_start_ = Time_now_;
		Disk[i].Time_head_ = 0;
		Disk[i].Time_tail_ = 0;
		Disk[i].usage = 0;
		Disk[i].busy_time = 0;
		Disk[i].HowLongPktStay.setavg(0.2);
	}
}


//----------------------------------------------------------------------
int DiskAgent::PickNum()
{
	int array[10] = {1,2,2,3,3,3,4,4,4,4};

	srand(time(NULL));
	int pick = (rand() % (9-0+1))+0;

	return array[pick];
}
//----------------------------------------------------------------------


// put in timestamp and sequence number, even though CPU doesn't usually 
// have one.
/*
void DiskAgent::sendmsg(int nbytes, AppData* data, const char* flags)
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
		rh->seqno() = ++seqno_;
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

void DiskAgent::recv(Packet* pkt, Handler*)
{

	if (app_ ) {
		// If an application is attached, pass the data to the app
		hdr_cmn* h = hdr_cmn::access(pkt);
		app_->process_data(h->size(), pkt->userdata());

	} else if (pkt->userdata() && pkt->userdata()->type() == PACKET_DATA) {
		// otherwise if it's just PacketData, pass it to Tcl
		//
		// Note that a Tcl procedure Agent/Disk recv {from data}
		// needs to be defined.  For example,
		//
		// Agent/Disk instproc recv {from data} {puts data}

		PacketData* data = (PacketData*)pkt->userdata();

		hdr_ip* iph = hdr_ip::access(pkt);
                Tcl& tcl = Tcl::instance();
		tcl.evalf("%s process_data %d {%s}", name(),
		          iph->src_.addr_ >> Address::instance().NodeShift_[1],
			  data->data());
	}
	
	//-------------------------------------------------------------------
	int D_num = PickNum();
	double current_time = Scheduler::instance().clock();
	Disk[D_num].HowLongPktStay.setavg(0.2);
	double stay_time = Disk[D_num].HowLongPktStay.value();

	Packet* p = allocpkt();
	hdr_cmn::access(p)->size() = hdr_cmn::access(pkt)->size_;
	hdr_rtp* rh = hdr_rtp::access(p);
	rh->flags() = 0;
	rh->seqno() = hdr_rtp::access(pkt)->seqno_ + 1;
	hdr_cmn::access(p)->timestamp() = hdr_cmn::access(pkt)->timestamp();
	hdr_cmn::access(p)->PKT_sendtime() = hdr_cmn::access(pkt)->PKT_sendtime();
	hdr_cmn::access(p)->PKT_resttime() = hdr_cmn::access(pkt)->PKT_resttime();

	Packet::free(pkt);
	//-------------------------------------------------------------------
	
	cout << "-----------------------------" << endl;
	
	
	if (current_time >= Disk[D_num].Time_tail_) {
		/* usage */
		Disk[D_num].busy_time = Disk[D_num].busy_time + stay_time;
		Disk[D_num].usage = Disk[D_num].busy_time / (current_time + stay_time - Disk[D_num].Time_start_);
		
		/* refresh time */
		Disk[D_num].Time_head_ = current_time;
		Disk[D_num].Time_tail_ = current_time + stay_time;
		
		/* send to CPU */
		cout << "case1" << endl;
		(void)Scheduler::instance().schedule(target_, pkt, stay_time);
		cout << "target_ = " << target_ << endl;
		//target_->recv(p);

	}else if ( (current_time+stay_time) > Disk[D_num].Time_tail_ ) {
		/* usage */
		Disk[D_num].busy_time = Disk[D_num].busy_time + (current_time + stay_time - Disk[D_num].Time_tail_);
		Disk[D_num].usage = Disk[D_num].busy_time / (current_time + stay_time - Disk[D_num].Time_start_);
		
		cout << "case2" << endl;
		cout << "(current_time + stay_time - Disk[D_num].Time_start_)" << endl;
		cout << "current_time = " << current_time << endl;
		cout << "stay_time = " << stay_time << endl;
		cout << "Disk[D_num].Time_start_ = " << Disk[D_num].Time_start_ << endl;
		
		/* refresh time */
		Disk[D_num].Time_tail_ = current_time + stay_time;
		
		/* send to CPU */
		(void)Scheduler::instance().schedule(target_, pkt, stay_time);
		cout << "target_ = " << target_ << endl;
		//target_->recv(p);

	}else {
		/* send to CPU */
		cout << "case3" << endl;
		(void)Scheduler::instance().schedule(target_, pkt, stay_time);
		cout << "target_ = " << target_ << endl;
		//target_->recv(p);

	}

	//-------------------------------------------------------------------
	for (int i=0; i<4; i++){
		cout << "Disk[" << i << "].usage = " << Disk[i].usage << endl;
	}
	//-------------------------------------------------------------------

}


int DiskAgent::command(int argc, const char*const* argv)
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
