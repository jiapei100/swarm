#include "swarmlog.h"
#include <iostream>
#include <astro/binarystream.h>
#include <fstream>
#include <sstream>
#include <memory>

extern "C" void debug_hook()
{
	std::cerr << "";
}

const ieventstream::body* ieventstream::get_bodies(int &nbod, int &ndropped) const
{
	nbod = std::min(log.ctr->nbod, log.bcap);
	ndropped = std::max(log.ctr->nbod - log.bcap, 0);
	return log.bodies;
}

ieventstream::ieventstream(cpu_eventlog &log_)
	: log(log_)
{
	init(log.events, log.ctr->nevt, log.ecap);
}

// advance to next message
int ieventstream::next()
{
	// already at end?
	if(atevt == nevt) { return EVT_EOF; }

	// move and exit if we're at the end
	atevt++;
	if(atevt != 0)
	{
		data += eventlog_base::MAX_MSG_LEN;
	}

	// learn the basics about the current message
	if(atevt != nevt)
	{
		hdr = *(eventlog_base::evt_hdr*)(data);
		at = sizeof(eventlog_base::evt_hdr);
		m_eom = false;
	}
	else
	{
		hdr.evtid = EVT_EOF;
	}

	// return current message event ID
	return evtid();
}

void ieventstream::init(const char *data_, int nevt_, int ecap_)
{
	data = data_;
	nevt = std::min(nevt_, ecap_);
	m_ndropped = std::max(nevt_ - ecap_, 0);

	// position us _ahead_ of the first message
	// (expecting a call to next() will move us to first message)
	atevt = -1;
	m_eom = true;
}

bool ieventstream::check_end()
{
	if(atevt == -1) // first call, auto-advance
	{
		next();
	}
	if(hdr.len == at) { m_eom = true; }
	return *this;
}

ieventstream &ieventstream::operator >>(raw_evt &v)
{
	if(!check_end()) { return *this; }

	align_to_header(at);
	v.size = *(int*)(data + at);
	at += sizeof(int);

	if(v.size > 0) { align_to_payload(at, v.size); } else { v.size *= -1; }
	v.data = data+at;
	at += v.size;
	return *this;
}

ieventstream &ieventstream::operator >>(std::string &s)
{
	if(!check_end()) { return *this; }

	align_to_header(at);
	int size = *(int*)(data + at);
	assert(size < 0);
	size *= -1;
	at += sizeof(int);

	s = (data+at);
	int strlen = s.size();
	if(strlen != size-1-sizeof(int))
		ERROR("Programmer error: data size != string size. Contact the authors.");
	at += size;
	return *this;
}

ieventstream &ieventstream::operator >>(char *s)
{
	if(!check_end()) { return *this; }

	align_to_header(at);
	int size = *(int*)(data + at);
	assert(size < 0);
	size *= -1;
	at += sizeof(int);

	strcpy(s, data+at);
	int strl = strlen(s);
	if(strl != size-1)
		ERROR("Programmer error: data size != string size. Contact the authors.");
	at += size;
	return *this;
}

ieventstream &ieventstream::operator >>(eventlog_base::event &evt)
{
	if(!check_end()) { return *this; }
	assert(sizeof(eventlog_base::event) == eventlog_base::MAX_MSG_LEN);
	memcpy(&evt, data, sizeof(eventlog_base::event));
	at = hdr.len;

	return *this;
}

cpu_eventlog clog;

bool cpu_eventlog::need_gpu_flush()
{
	// download GPU counters
	eventlog_base::counters gctr;
	memcpyToHost(&gctr, glog.ctr);

	return gctr.nevt >= glog.ecap || gctr.nbod >= glog.bcap;
}

void cpu_eventlog::flush_if_needed(bool cpuonly)
{
	bool cpuneedflush = ctr->nevt >= ecap || ctr->nbod >= bcap;
	bool gpuneedflush = !cpuonly && need_gpu_flush();

	if(cpuneedflush || gpuneedflush)
	{
		flush();
	}
}

// download the GPU log data, overwriting the CPU log
// called _ONLY_ from flush()
void cpu_eventlog::copyFromGPU()
{
	// download GPU log data
	if(ctr == NULL)
		ERROR("Programmer error: cpu_eventlog must be initialized before use. Contact the authors.");

	// download GPU counters
	memcpyToHost(ctr, glog.ctr);

	// download the data
	events = hostAlloc(events, ecap * MAX_MSG_LEN);
	memset(events, 0, ecap * MAX_MSG_LEN);
	memcpyToHost(events, glog.events, ecap * MAX_MSG_LEN);
	bodies = hostAlloc(bodies, bcap);
	memcpyToHost(bodies, glog.bodies, bcap);
}

void cpu_eventlog::prepare_for_gpu()
{
	if(lastongpu) { return; }

	// last write was on the CPU, and may have moved the
	// refbases. Upload them to the GPU. Because of how
	// we do things, the GPU buffers should be empty at
	// this point (it's a bug if they're not)

#if __DEVICE_EMULATION__
	assert(glog.ctr->nbod == 0 && glog.ctr->nevt == 0);
#endif

	// update the GPU copy with the current ref bases
	glog.evtref_base = evtref_base + ctr->nevt;
	glog.bodref_base = bodref_base + ctr->nbod;
	cuxUploadConst("glog", this->glog);

	lastongpu = true;
}

void cpu_eventlog::prepare_for_cpu()
{
	if(!lastongpu) { return; }

	// last write was on the GPU, which moved the ref base.
	// before that, we may have had writes on the GPU
	// so now trigger a flush, CPU first, followed by the GPU
	// After the flush is done, download the GPU refbases
	// and store it here

	flush();
}

void cpu_eventlog::attach_sink(writer *w_)
{
	w = w_;
}

void cpu_eventlog::flush()
{
	if(lastongpu)
	{
		// GPU was written-to last

		// flush CPU buffers
		ieventstream es(*this);
		w->process(es);
		evtref_base += ctr->nevt;
		bodref_base += ctr->nbod;
			
		// flush GPU buffers
		copyFromGPU();
		ieventstream es2(*this);
		w->process(es2);
	}
	else
	{
		// CPU was written-to last -- by construction
		// of this class, there's nothing on the GPU side

		// flush CPU buffers
		ieventstream es(*this);
		w->process(es);
	}

	// update ref bases before we clear the counters
	evtref_base += ctr->nevt;
	bodref_base += ctr->nbod;

	// clear CPU & GPU counters
	ctr->reset();
	memcpyToGPU(glog.ctr, ctr);

	// cpu is now the master
	lastongpu = false;
}

cpu_eventlog::cpu_eventlog()
{
	// set everything to NULL
	memset(this, 0, sizeof(this));
}

//
// Initialize the GPU eventlog buffer, and prepare the CPU
// eventlog object
//
void cpu_eventlog::initialize(int ecap_, int bcap_, int scap_)
{
	if(ctr != NULL) { return; }

	// buffer sizes
	bcap = bcap_;
	ecap = ecap_;
	btresh = (int)(0.9 * bcap);
	etresh = (int)(0.9 * ecap);

	// CPU side
	ctr = new eventlog_base::counters;
	ctr->reset();
	events = hostAlloc(events, ecap * MAX_MSG_LEN);
	bodies = hostAlloc(bodies, bcap);

	// GPU side
	glog = *this;
	glog.ctr = cuxNew<eventlog_base::counters>(1);
	memcpyToGPU(glog.ctr, ctr);

	// initialize GPU buffers & eventlog structure
	glog.events = cuxNew<char>(ecap * MAX_MSG_LEN);
	glog.bodies = cuxNew<body>(bcap);

	// upload to const
	cuxUploadConst("glog", this->glog);
	lastongpu = true;
}

cpu_eventlog::~cpu_eventlog()
{
	// Host
	hostFree(events);
	hostFree(bodies);
	delete ctr;
	
	// GPU
	cudaFree(glog.events);
	cudaFree(glog.bodies);
	cudaFree(glog.ctr);
}

//
// gpuPrintf: CPU side
//

bool get_message(ieventstream &evt, std::string &res)
{
	// format of data packets:
	// ([hdr][size|...data...][size|...data...])

	int evtid2;
	if(evt.evtid() == EVT_MSGLOST  && evt >> evtid2 && evtid2 == EVT_PRINTF)
	{
		evt.next();
		res = "Message lost (too big).\n";
		return true;
	}
	if(evt.evtid() != EVT_PRINTF)
	{
		return false;
	}

	// slurp up the format string
	char fmtbuf[cpu_eventlog::MAX_MSG_LEN], *fmt = fmtbuf;
	evt >> fmt;

	// Now run through it, printing everything we can. We must
	// run to every % character, extract only that, and use printf
	// to format it.
	char buf[1024];
	std::ostringstream out;
	char *p = strchr ( fmt, '%' );
	while ( p != NULL )
	{
		// Print up to the % character
		*p = '\0';
		out << fmt;
		*p = '%';           // Put back the %

		// Now handle the format specifier
		char *format = p++;         // Points to the '%'
		p += strcspn ( p, "%cdiouxXeEfgGaAnps" );
		if ( *p == '\0' )           // If no format specifier, print the whole thing
		{
			fmt = format;
			break;
		}

		ieventstream::raw_evt m;
		evt >> m;

		char specifier = *p++;
		char c = *p;        // Store for later
		*p = '\0';
		switch ( specifier )
		{
				// These all take integer arguments
			case 'c':
			case 'd':
			case 'i':
			case 'o':
			case 'u':
			case 'x':
			case 'X':
			case 'p':
				sprintf(buf, format, *(int*)m.data);
				out << buf;
				break;

				// These all take double arguments
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
			case 'a':
			case 'A':
				if ( m.size == 4 )  // Float vs. Double thing
					sprintf(buf, format, * ( ( float * ) m.data ));
				else
					sprintf(buf, format, * ( ( double * ) m.data ));
				out << buf;
				break;

				// Strings are handled in a special way
			case 's':
				sprintf(buf, format, ( char * ) m.data);
				out << buf;
				break;

				// % is special
			case '%':
				out << "%%";
				break;

				// Everything else is just printed out as-is
			default:
				out << format;
				break;
		}
		*p = c;                     // Restore what we removed
		fmt = p;                    // Adjust fmt string to be past the specifier
		p = strchr(fmt,'%');    // and get the next specifier
	}

	// Print out the last of the string
	out << fmt;
	res = out.str();
	return true;
}

bool next_message(ieventstream &evt, std::string &res)
{
	while(!get_message(evt, res) && evt.next());
	return evt;
}

//
// Default binary writer
//

class binary_writer : public writer
{
protected:
	std::auto_ptr<std::ostream> eout_strm, bout_strm;
	std::auto_ptr<obstream> eout, bout;
	int ctr;

public:
	binary_writer(const std::string &cfg);
	virtual void process(ieventstream &es);
};

extern "C" writer *create_writer_binary(const std::string &cfg)
{
	return new binary_writer(cfg);
}

binary_writer::binary_writer(const std::string &cfg)
{
	ctr = 0;

	std::string event_fn, bodies_fn;
	std::istringstream ss(cfg);
	if(!(ss >> event_fn >> bodies_fn))
		ERROR("Expected 'binary <events_fn> <bodies_fn>' form of configuration for writer.")

	// TODO: check error return codes
	eout_strm.reset(new std::ofstream(event_fn.c_str()));
	bout_strm.reset(new std::ofstream(bodies_fn.c_str()));
	eout.reset(new obstream(*eout_strm));
	bout.reset(new obstream(*bout_strm));
}

// store the accumulated events and bodies into a file, while
// printing out any printf() events to stdout
BLESS_POD(eventlog_base::body);
BLESS_POD(eventlog_base::event);
void binary_writer::process(ieventstream &es)
{
	// download and dump the log
	// write out events
	while(es.next())
	{
		std::string evt;
		if(get_message(es, evt))
		{
			std::cout << "[Event #" << es.evtref() << " from thread " << es.threadId() << "]: " << evt << "\n";
		}
		else
		{
			// Write it to file
			eventlog_base::event evt;
			es >> evt;
			*eout << evt;

			std::cout << "[Event #" << evt.hdr.evtref << " from thread " << evt.hdr.threadId << "] EVT=" << evt.hdr.evtid << ", with " << evt.hdr.nargs << " data elements (" << evt.hdr.len << " bytes for entire event)\n";
		}
	}
	if(es.nevents_dropped())
	{
		std::cerr << "==== Ran out of event GPU output buffer space (" << es.nevents_dropped() << " event records dropped).\n";
	}

	// write out bodies
	int nbod, ndropped;
	const eventlog_base::body *bodies = es.get_bodies(nbod, ndropped);
	for(int bod = 0; bod != nbod; bod++)
	{
		*bout << bodies[bod];
	}
	if(ndropped)
	{
		std::cerr << "==== Ran out of body GPU output buffer space (" << ndropped << " body records dropped).\n";
	}
}