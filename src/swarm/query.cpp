//
//
// Author: Mario Juric <mjuric@cfa.harvard.edu>, (C) 2010
//
// Copyright: See COPYING file that comes with this distribution
//

/*! \file query.cpp
 *    \brief routines for extracting text information from binary files generated by swarm GPU logging subsystem.
 * @authors Mario Juric
 *
 * 
 * A range has the format xx..yy.  
 * Since system id's are integers, the range 
 * for the system id, can also be a single integer.
 * 
 *
*/

#include "query.hpp"
#include "kepler.h"


namespace swarm { namespace query {



void get_Tsys(gpulog::logrecord &lr, double &T, int &sys)
{
	//std::cerr << "msgid=" << lr.msgid() << "\n";
	if(lr.msgid() < 0)
	{
		// system-defined events that have no (T,sys) heading
		T = -1; sys = -1;
	}
	else
	{
		lr >> T >> sys;
	}
}

// Default output, if no handler is registered
std::ostream& record_output_default(std::ostream &out, gpulog::logrecord &lr)
{
	double T; int sys;
	get_Tsys(lr, T, sys);
	out << lr.msgid() << "\t" << T << "\t" << sys;
	return out;
}

bool keplerian_output = false;

planets_coordinate_system_t planets_coordinate_system ;
void set_keplerian_output(const planets_coordinate_system_t& coordinate_system) {
	keplerian_output = true;
	planets_coordinate_system = coordinate_system;
}

struct keplerian_t {
	double a, e, i, O, w , M;
};

keplerian_t keplerian_for_cartesian ( const swarm::body& b, const swarm::body& center ) { 
	keplerian_t k;
	double x = b.x - center.x;
	double y = b.y - center.y;
	double z = b.z - center.z;
	double vx = b.vx - center.vx;
	double vy = b.vy - center.vy;
	double vz = b.vz - center.vz;
	double mass = b.mass + center.mass;
	calc_keplerian_for_cartesian(k.a, k.e, k.i, k.O, k.w, k.M, x, y,z, vx, vy, vz, mass );
	return k;
}

body center_of_mass(const body* bodies, const int nbod ){ 
	body center;
	center.x = center.y = center.z = center.vx = center.vy = center.vz = center.mass;
	for(int i = 0; i < nbod; i++ )
		center.x += bodies[i].x,
		center.y += bodies[i].y,
		center.z += bodies[i].z,
		center.vx += bodies[i].vx,
		center.vy += bodies[i].vy,
		center.vz += bodies[i].vz,
		center.mass += bodies[i].mass;

	center.x /=   center.mass;
	center.y /=   center.mass;
	center.z /=   center.mass;
	center.vx /=  center.mass;
	center.vy /=  center.mass;
	center.vz /= center.mass;

	return center;
}

// EVT_SNAPSHOT
std::ostream& record_output_1(std::ostream &out, gpulog::logrecord &lr)
{
	double T;
	int nbod, sys, flags;
	const body *bodies;
	lr >> T >> sys >> flags >> nbod >> bodies;

	body center;
	const body& star = bodies[0];

	if( keplerian_output ) 
		switch(planets_coordinate_system){
			case astrocentric:
				center = star;
				break;
			case barycentric:
				center = center_of_mass( bodies, nbod );
				break;
			case jacobi:
				center = star;
				break;
		};


	size_t bufsize = 1000;
	char buf[bufsize];
	for(int bod = 0; bod != nbod; bod++)
	{
		const swarm::body &b = bodies[bod];
		if(bod != 0) { out << "\n"; }

		if( keplerian_output  && bod > 0) {

			if( planets_coordinate_system == jacobi ){
				center = center_of_mass ( bodies, bod + 1);
			}

			keplerian_t k = keplerian_for_cartesian( b, center );

			const double rad2deg = 180./M_PI;
			snprintf(buf, bufsize, "%10d %lg  %5d %5d  %lg  % 9.5lg % 9.5lg % 9.5lg  % 9.5lg % 9.5lg % 9.5lg  %d", lr.msgid(), T, sys, bod, b.mass, k.a, k.e , k.i*rad2deg, k.O*rad2deg, k.w *rad2deg, k.M*rad2deg, flags);

		}
		if(!keplerian_output)
		  snprintf(buf, bufsize, "%10d %lg  %5d %5d  %lg  %9.5lg %9.5lg %9.5lg  %9.5lg %9.5lg %9.5lg  %d", lr.msgid(), T, sys, bod, b.mass, b.x, b.y, b.z, b.vx, b.vy, b.vz, flags);
		out << buf;
	}
	return out;
}




// EVT_EJECTION
std::ostream& record_output_2(std::ostream &out, gpulog::logrecord &lr)
{
	double T;
	int sys;
	swarm::body b;
	lr >> T >> sys >> b;

        size_t bufsize = 1000;
        char buf[bufsize];
     snprintf(buf, bufsize, "%10d %lg  %5d  %lg  % 9.5lf % 9.5lf % 9.5lf  % 9.5lf % 9.5lf % 9.5lf", lr.msgid(), T, sys, b.mass, b.x, b.y, b.z, b.vx, b.vy, b.vz);
	out << buf;

	return out;
}

std::ostream &output_record(std::ostream &out, gpulog::logrecord &lr)
{
	int evtid = lr.msgid();

	switch(evtid){
	case 1:
		return record_output_1(out,lr);
	case 2:
		return record_output_2(out,lr);
	default:
		return record_output_default(out,lr);
	}
}


void execute(const std::string &datafile, swarm::time_range_t T, swarm::sys_range_t sys)
{
	swarmdb db(datafile);
	swarmdb::result r = db.query(sys, T);
	gpulog::logrecord lr;
	while(lr = r.next())
	{
		output_record(std::cout, lr);
		std::cout << "\n";
	}
}

} }
