#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvsroot/nsnam/ns-2/apps/disk.cc,v 1.21 2005/08/26 05:05:28 tomh Exp $ (Xerox)";
#endif

#include "udp.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdio.h>
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
	ofstream fd;
};


class DiskAgent : public Agent {
public:
	DiskAgent();
	virtual void recv(Packet* pkt, Handler*);
	virtual int command(int argc, const char*const* argv);
	int PickNum();

protected:
	int seqno_;
	int pick_num_;
	double taskPsec_;
	bool cut_;
	ExponentialRandomVariable HowLongPktStay;
	Disk_struct Disk[4];
};


static class DiskAgentClass : public TclClass {
public:
	DiskAgentClass() : TclClass("Agent/DISK") {}
	TclObject* create(int, const char*const*) {
		return (new DiskAgent);
	}
} class_disk_agent;


DiskAgent::DiskAgent() : Agent(PT_DISK), seqno_(-1)
{
	double Time_now_ = Scheduler::instance().clock();
	double pick_num_ = 0;
	
	HowLongPktStay.setavg(0.2);
	
	for (int i=0; i<4; i++) {
		Disk[i].Time_start_ = Time_now_;
		Disk[i].Time_head_ = 0;
		Disk[i].Time_tail_ = 0;
		Disk[i].usage = 0;
		Disk[i].busy_time = 0;
	}
	
	bind("taskPsec_", &taskPsec_);
	cut_ = false;
	
}


//----------------------------------------------------------------------
int DiskAgent::PickNum()
{
	
	pick_num_++;

	if (pick_num_ >= 10) {pick_num_ = 0;}
	
	int array[10] = {0,1,1,2,2,2,3,3,3,3};

	return array[pick_num_];
}
//----------------------------------------------------------------------

void DiskAgent::recv(Packet* pkt, Handler*)
{
	//-------------------------------------------------------------------
	Packet* p = allocpkt();
	
	hdr_cmn::access(p)->size() = hdr_cmn::access(pkt)->size_;
	hdr_cmn::access(p)->timestamp() = hdr_cmn::access(pkt)->ts_;
	hdr_cmn::access(p)->PKT_sendtime() = hdr_cmn::access(pkt)->PKT_sendtime_;
	hdr_cmn::access(p)->PKT_resttime() = hdr_cmn::access(pkt)->PKT_resttime_;
	
	Packet::free(pkt);
	//-------------------------------------------------------------------
	int D_num = PickNum();
	double current_time = Scheduler::instance().clock();
	double stay_time = HowLongPktStay.value();
	//-------------------------------------------------------------------
	
	cout << "-----------------------------" << endl;
	cout << "stay_time = " << stay_time << endl;

	if (current_time >= Disk[D_num].Time_tail_) {
		/* usage */
		Disk[D_num].busy_time = Disk[D_num].busy_time + stay_time;
		Disk[D_num].usage = Disk[D_num].busy_time / (current_time + stay_time - Disk[D_num].Time_start_);
		
		/* refresh time */
		Disk[D_num].Time_head_ = current_time;
		Disk[D_num].Time_tail_ = current_time + stay_time;
		
		/* send to CPU */
		cout << "case1" << endl;
		(void)Scheduler::instance().schedule(target_, p, stay_time);

	}else if ( (current_time+stay_time) > Disk[D_num].Time_tail_ ) {
		/* usage */
		Disk[D_num].busy_time = Disk[D_num].busy_time + (current_time + stay_time - Disk[D_num].Time_tail_);
		Disk[D_num].usage = Disk[D_num].busy_time / (current_time + stay_time - Disk[D_num].Time_start_);
		
		/* refresh time */
		Disk[D_num].Time_tail_ = current_time + stay_time;
		
		/* send to CPU */
		cout << "case2" << endl;
		(void)Scheduler::instance().schedule(target_, p, stay_time);

	}else {
		/* send to CPU */
		cout << "case3" << endl;
		(void)Scheduler::instance().schedule(target_, p, stay_time);

	}

	//-------------------------------------------------------------------
	for (int i=0; i<4; i++){
		cout << "Disk[" << i << "].usage = " << Disk[i].usage << endl;
	}
	cout << "taskPsec_ = " << taskPsec_ << endl;
	//-------------------------------------------------------------------
	if ((current_time>=9999) && (cut_==false)){

		char address[100];
		
		for (int i=0; i<4; i++) {
			
			sprintf(address, "/home/wirelab/NS2/ns-allinone-2.35/disk%d.txt", i+1);

			Disk[i].fd.open(address,ios::app);
			Disk[i].fd << (taskPsec_) << " " << Disk[i].usage << endl;
			Disk[i].fd.close();
			cut_ = true;
		}
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
