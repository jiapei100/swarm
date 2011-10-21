/*************************************************************************
 * Copyright (C) 2011 by Eric Ford and the Swarm-NG Development Team     *
 *                                                                       *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 3 of the License.        *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ************************************************************************/
#pragma once

#include <limits>

namespace swarm {
  namespace monitors {

struct stop_on_collision_param {
	double dmin_squared;
	stop_on_collision_param(const config &cfg)
	{
	  dmin_squared = cfg.optional("collision_radius",0.);
	  dmin_squared *= dmin_squared;
	}
};

/** Simple monitor to detect physical collisions.  
 *  Signals and logs if current separation between any two bodies is less than "collision_radius".
 *  WARNING: Does not interpolate between steps
 *  TODO: Need to allow object specific collision radii or collision densities
 *  \ingroup monitors
 */
template<class log_t>
class stop_on_collision {
	public:
	typedef stop_on_collision_param params;

	private:
	params _p;

	ensemble::SystemRef& _sys;
	log_t& _log;

	int _counter;

	public:

#if 0 // still under development
  GPUAPI bool check_close_encounter_possible(const int& i, const int& j, double dt){

    //	  double hill_radius_sq_upper_limit = pow(((_sys.mass(i)+_sys.mass(j))/(3.0*_sys.mass(0))),2.0/3.0)*(std::max(_sys.radius_squared(i),_sys.radius_squared(j)));
	  double target_radius_sq = (NUM_ATTRIBUTES>=1) ? attribute(i)*attribute(i) : _p.dmin_squared;
	  double vesc_sq = 2.0*_sys.mass(0)/std::min(_sys.radius(i),_sys.radius(j));
	  double d_squared = _sys.distance_squared_between(i,j);
	  bool close_encounter = (d_squared < vesc_sq*dt*dt + target_radius_sq);
	  return close_encounter;
	}
#endif
  
#if 0 // still under development
  GPUAPI double min_encounter_distance(const int& i, const int& j){
    const Body& b1 = _body[i], & b2 = _body[j];
    double d = sqrt( square(b1[0].pos()-b2[0].pos())
		     + square(b1[1].pos()-b2[1].pos())
		     + square(b1[2].pos()-b2[2].pos()) );
    return d;
  }
#endif

	GPUAPI bool check_close_encounters(const int& i, const int& j){

		double d_squared = _sys.distance_squared_between(i,j);
		bool close_encounter = d_squared < _p.dmin_squared;

		if( close_encounter )
			lprintf(_log, "Collision detected: "
					"sys=%d, T=%f j=%d i=%d  d=%lg.\n"
				, _sys.number(), _sys.time(), j, i,sqrt(d_squared));

		return close_encounter;
	}

  
  
	GPUAPI void operator () () { 
		bool stopit = false;

		// Chcek for close encounters
		for(int b = 1; b < _sys.nbod(); b++)
			for(int d = 0; d < b; d++)
				stopit = stopit || check_close_encounters(b,d); 

		if(stopit) {
			log::system(_log, _sys);
			_sys.set_disabled();
		}
	}

	GPUAPI stop_on_collision(const params& p,ensemble::SystemRef& s,log_t& l)
	    :_p(p),_sys(s),_log(l),_counter(0){}
	
};

}


}