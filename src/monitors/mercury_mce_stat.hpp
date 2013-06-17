/** Implementation of the algorithm for collision detection in Mecurial 
 *	by John E. Chambers which could be found at 
 *  https://github.com/smirik/mercury
 *************************************************************************
 * Copyright (C) 2011 by Thien Nguyen and the Swarm-NG Development Team  *
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
 */
 
#pragma once

#include <limits>

#define PI 3.14159265358979
#define THIRD 0.3333333333333333
namespace swarm {

/*!
 * @brief Namespace for monitors (i.e., stoppers & loggers) and their associated classes
 */
namespace monitors { 
 
  //! Structure for monitor_template_params
struct mce_stat_params {
int nbod;
double rceh, dmin, time_step;
bool deactivate_on, log_on, verbose_on;
/*! \param cfg Configuration Paramaters
  */
      mce_stat_params(const config &cfg)
      {
	      nbod = cfg.require<int>("nbod");
	      time_step = cfg.require("time_step", 0.0);
	      dmin = cfg.optional("close_approach",0.0);
	      rceh = cfg.optional("rceh",0.001);
	      	      
	      deactivate_on = cfg.optional("deactivate_on_close_encounter",false);
	      log_on = cfg.optional("log_on_close_encounter",false);
	      verbose_on = cfg.optional("verbose_on_close_encounter",false);
	      
      } 
  
};

template<int W>
struct CurrentSysStat
{
  static const int CHUNK_SIZE = W;
  
  double _pos[CHUNK_SIZE];
  double _vel[CHUNK_SIZE];
  double _BB_SW[CHUNK_SIZE];
  double _BB_NE[CHUNK_SIZE];
  unsigned long long int _crash[CHUNK_SIZE];
  // Accessors
  GENERIC double& pos() { return _pos[0];  }
  GENERIC double& vel() { return _vel[0];  }
  GENERIC double& bb_sw() { return _BB_SW[0];  }
  GENERIC double& bb_ne() { return _BB_NE[0];  }
  GENERIC unsigned long long int& crash() { return _crash[0];  }
};

/** Empty monitor to use as a template.  
 * Signal is always false.  Does not do any logging.
 * \ingroup monitors
 *
 */

template<class log_t>
class mce_stat {
	public:
	
	  typedef mce_stat_params params;
	  
	  const static int CHUNK_SIZE = SHMEM_CHUNK_SIZE;
	  
	  typedef CurrentSysStat<CHUNK_SIZE>  shared_data[MAX_NBODIES][3];
	private:
	params _params;
	ensemble::SystemRef& _sys;
	log_t& _log;
	
	shared_data &shared;
	
	int nbod,pair_count;
	
	double timestep;
	
	public:

	  //! default monitor_template construct
	GPUAPI mce_stat(const params& p,ensemble::SystemRef& s,log_t& l, shared_data &shared_mem)
	:_params(p),_sys(s),_log(l), shared(shared_mem)
	{
	    nbod = _params.nbod;
	    pair_count = (nbod*(nbod-1))/2;
	    timestep = _params.time_step;   
	}
	
	template<class T>
	static GENERIC int thread_per_system(T compile_time_param){
	  return (T::n*(T::n-1))/2;
	}
		
        //! The amount of memory per system
	template<class T>
	static GENERIC int shmem_per_system(T compile_time_param) {
		 return sizeof(shared_data);
	}
	
        //! Provide these functions, so two monitors can be combined
        GPUAPI bool is_deactivate_on() { return false; }
        GPUAPI bool is_log_on() { return false; }
        GPUAPI bool is_verbose_on() { return false; }
        GPUAPI bool is_any_on() { return is_deactivate_on() || is_log_on() || is_verbose_on() ; }
        GPUAPI bool is_condition_met () { return false; }
        GPUAPI bool need_to_log_system () 
          { return (is_log_on() && is_condition_met() ); }
        GPUAPI bool need_to_deactivate () 
          { return ( is_deactivate_on() && is_condition_met() ); }

    GPUAPI void init(const int thread_in_system)
    {
      if (0< thread_in_system < nbod)
      {
	
	double gm = _sys[0].mass() + _sys[thread_in_system].mass();
	
	double r = sqrt(_sys[thread_in_system][0].pos()*_sys[thread_in_system][0].pos() 
			+ _sys[thread_in_system][1].pos()*_sys[thread_in_system][1].pos()
			+ _sys[thread_in_system][2].pos()*_sys[thread_in_system][2].pos());
	double vi_max = _sys[thread_in_system][0].vel()*_sys[thread_in_system][0].vel()
		    + _sys[thread_in_system][1].vel()*_sys[thread_in_system][1].vel()
		    + _sys[thread_in_system][2].vel()*_sys[thread_in_system][2].vel();
	double a = gm*r/(2*gm - r*vi_max);
	if ( a <=0 ) a = r;
	double hill = a*pow(THIRD*_sys[thread_in_system].mass()/_sys[0].mass(), THIRD);
	
	// assume attribute 0 is the radius of the planet
	// rho is the density of the planet = mass/volume = mass/(4/3piR^3)
	double rho =  _sys[thread_in_system].mass()/(4*THIRD*PI*_sys[thread_in_system].attribute(0)*_sys[thread_in_system].attribute(0)*_sys[thread_in_system].attribute(0));
	
	_sys[thread_in_system].attribute(1) = hill;//*_params.rceh; // rce
	_sys[thread_in_system].attribute(2) = hill/a * pow(2.25 * _sys[0].mass()/(PI*rho),THIRD); // rphysics
	
	
      }
      if (thread_in_system == 0)
      {
	_sys[thread_in_system].attribute(1) = 0.0;
	_sys[thread_in_system].attribute(2) = 0.0;
	
      }
    }
    GPUAPI void storeCurrentStat(const int thread_in_system)
    {
      if (thread_in_system < nbod)
      {
	shared[thread_in_system][0].pos() = _sys[thread_in_system][0].pos();
	shared[thread_in_system][1].pos() = _sys[thread_in_system][1].pos();
	shared[thread_in_system][2].pos() = _sys[thread_in_system][2].pos();
	
	shared[thread_in_system][0].vel() = _sys[thread_in_system][0].vel();
	shared[thread_in_system][1].vel() = _sys[thread_in_system][1].vel();
	shared[thread_in_system][2].vel() = _sys[thread_in_system][2].vel();
      }
    }
    GPUAPI void operator () (const int thread_in_system) 
    { 
    	
	    pass_one(thread_in_system);
	    
	    __syncthreads();
	    
	    pass_two(thread_in_system);
	    
	    __syncthreads();
	    
	    if(need_to_log_system() && (thread_in_system==0) )
	      log::system(_log, _sys);
	      
	       	
    }

	private:
		
        /// This is mce_box() routine in Mercury,
        /// Calculate bounding box of the trajectory of each planet between time steps t0 and t1
	GPUAPI void pass_one (int thread_in_system) 
          {
	      /*************************************************************************
	      * Calculate the bounding box of the trajectory of a planet between time step t0 and t0 + h
	      * **********************************************************************/
          	         	
          	if (0< thread_in_system < nbod)
		{
			
		      for(int c = 0; c<3 ; c++)
		      {
			shared[thread_in_system][c].bb_sw() = min(shared[thread_in_system][c].pos(),_sys[thread_in_system][c].pos()); // component min
			shared[thread_in_system][c].bb_ne() = max(shared[thread_in_system][c].pos(),_sys[thread_in_system][c].pos()); // component max
		      }
			
			
			// if velocity changes sign, do an interpolation
		      for (int c = 0; c < 3; c++)
		      {
			if ( (shared[thread_in_system][c].vel() < 0 && _sys[thread_in_system][c].vel() > 0 ) || 
			      (shared[thread_in_system][c].vel() > 0 && _sys[thread_in_system][c].vel() < 0 ) )
			{
				double temp = shared[thread_in_system][c].vel()*_sys[thread_in_system][c].pos() 
					      - _sys[thread_in_system][c].vel()*shared[thread_in_system][c].pos()
					      - 0.5*timestep*shared[thread_in_system][c].vel()*_sys[thread_in_system][c].vel()/(shared[thread_in_system][c].vel()-_sys[thread_in_system][c].vel());
				shared[thread_in_system][c].bb_sw() = min(shared[thread_in_system][c].bb_sw(),temp);
				shared[thread_in_system][c].bb_ne() = max(shared[thread_in_system][c].bb_ne(),temp);
				
			}
		      }
		      
		      
		      //Then adjust values by the maximum close-encounter radius plus a fudge factor
		      double tmp = _sys[thread_in_system].attribute(1)*1.2; 
		      for (int c = 0; c < 3; c++)
		      {
			shared[thread_in_system][c].bb_sw() -= tmp;
			shared[thread_in_system][c].bb_ne() += tmp;
		      }
		      
		      shared[thread_in_system][0].crash() = 0;
		}
		if (thread_in_system == 0) 
		  shared[thread_in_system][0].crash() = 0;
			
			
          }

        //! set the system state to disabled when three conditions are met
	GPUAPI void pass_two (int thread_in_system) 
      	{
      		if (thread_in_system < pair_count)
      		{
		  int j = nbod - 1 - thread_in_system / (nbod/2);
		  int i = thread_in_system % (nbod/2);
		  if (i >= j)
		  {
		    i = nbod - i - nbod%2-1;
		    j = nbod - j - nbod%2;
		  }
		  
		  //lprintf(_log,"BBox: thread=%d,i=%d, j=%d\n",thread_in_system,i,j);
		      
		      
		  /*************************************************************************
		    * Consider planet - planet first
		    * **********************************************************************/
		  if (i > 0)
		  {

		      /************************************************************************
		       * Check if bounding boxes of the trajectories of the two planet intersect.
		       * If intersect, then calculating minimum distance and resolving collision.
		       * **********************************************************************/
		      
		      if ( shared[i][0].bb_ne() >= shared[j][0].bb_sw() && shared[j][0].bb_ne() >= shared[i][0].bb_sw() &&
			   shared[i][1].bb_ne() >= shared[j][1].bb_sw() && shared[j][1].bb_ne() >= shared[i][1].bb_sw() &&
			   shared[i][2].bb_ne() >= shared[j][2].bb_sw() && shared[j][2].bb_ne() >= shared[i][2].bb_sw() &&
			   _sys[i].mass() > 0 && _sys[j].mass() > 0)
		      {
			      float dx0 = shared[i][0].pos() - shared[j][0].pos();
			      float dy0 = shared[i][1].pos() - shared[j][1].pos();
			      float dz0 = shared[i][2].pos() - shared[j][2].pos();
			      float du0 = shared[i][0].vel() - shared[j][0].vel();
			      float dv0 = shared[i][1].vel() - shared[j][1].vel();
			      float dw0 = shared[i][2].vel() - shared[j][2].vel();
			      float d0t = (dx0*du0 + dy0*dv0 + dz0*dw0)*2.0; 
			      
			      float dx1 = _sys[i][0].pos() - _sys[j][0].pos();
			      float dy1 = _sys[i][1].pos() - _sys[j][1].pos();
			      float dz1 = _sys[i][2].pos() - _sys[j][2].pos();
			      float du1 = _sys[i][0].vel() - _sys[j][0].vel();
			      float dv1 = _sys[i][1].vel() - _sys[j][1].vel();
			      float dw1 = _sys[i][2].vel() - _sys[j][2].vel();
			      float d1t = (dx1*du1 + dy1*dv1 + dz1*dw1)*2.0;
			      
			      float d0 = dx0*dx0 + dy0*dy0 + dz0*dz0;
			      float d1 = dx1*dx1 + dy1*dy1 + dz1*dz1;
			     
			      
			      float d2min, tmin, tau;
			      //Calculating the mininum distance between two trajectories of particle i and j
			      if (d0t > 0 && d1t < 0)
				      if (d0 < d1)
				      {
					      d2min = d0;
					      tmin = -timestep;
				      }
				      else
				      {
					      d2min = d1;
					      tmin = 0.0;
				      }
			      else // using Hermite interpolation to calculate trajectories and the possible intersections
			      {
				      float tmp = 6.0*(d0 - d1);
				      float a = tmp + 3.0*timestep*(d0t + d1t);
				      float b = tmp + 2.0*timestep*(d0t + 2.0*d1t);
				      float c = timestep * d1t;
				      
				      tmp = -0.5*(b + ((b>0)?1.0:(-1.0))*sqrt(max(b*b-4*a*c,0.0)));
				      if (tmp == 0.0) 
					tau = 0.0;
				      else
					tau = c/tmp;
				      tau = min(tau,0.0);
				      tau = max(tau,-1.0);
				      tmin = tau*timestep;
				      tmp = 1.0 + tau;
				      d2min = tau*tau*((3.0+2.0*tau)*d0 + tmp*timestep*d0t)
						      + tmp*tmp*((1.0-2.0*tau)*d1 + tau*timestep*d1t);
				      d2min = max(d2min, 0.0);
				      
			      }
			      
			      //---------------- End of minimum distance calculation --------------------------------
			      //float d2ce = max(_sys[i].attribute(1), _sys[j].attribute(1)); // distant for closed encounter
			      float d2hit = max(_sys[i].attribute(2), _sys[j].attribute(2)); // distant for collision
			      //d2ce = d2ce*d2ce;
			      d2hit = d2hit*d2hit;
			      //float d2near = d2hit*4.0;
			      
			      if (d2min <= d2hit)
			      {
				atomicAdd(&shared[i][0].crash(),1);
				atomicAdd(&shared[j][0].crash(),1);
				
				//i is the index of the heavier planet
				
				if (_sys[i].mass() < _sys[j].mass()) {int k=i; i=j;j=k; }
				
				lprintf(_log,"Hitting detected: i=%d, j=%d, d=%f, T=%f \n", i,j, d2min, _sys.time()+tmin);
				
				float msum = _sys[i].mass() + _sys[j].mass();
				float mredu = _sys[i].mass()*_sys[j].mass()/msum;
				//energy lost
				_sys.attribute(0) = 0.5*mredu*(du1*du1+dv1*dv1+dw1*dw1) - _sys[i].mass()*_sys[j].mass()/sqrt(dx1*dx1+dy1*dy1+dz1*dz1);
				float tmp1 = _sys[i].mass()/msum;
				float tmp2 = _sys[j].mass()/msum;
// 				
				//Does it need to use atomicAdd ?
				for (int c = 0; c < 3;c++)
				{
				  _sys[i][c].pos() = _sys[i][c].pos()*tmp1 + _sys[j][c].pos()*tmp2;
				  _sys[i][c].vel() = _sys[i][c].vel()*tmp1 + _sys[j][c].vel()*tmp2;
				}
				_sys[i].mass() = msum;
				
				//eliminate the planet j
				for (int c = 0; c < 3;c++)
				{
				  _sys[j][c].pos() *= -1.0;
				  _sys[j][c].vel() *= -1.0;
				}
				_sys[j].mass() = 0.0;
// 				   
				
			      }
			      
			      
		      }// endif: two bounding boxes have non-zero intersection*/
		  }//endif: 2 planet
		  
		  /*************************************************************************
		  * Now consider sun - planet
		  * **********************************************************************/
		  if (i==0 && j > 0) //
		  {
		    float rr0 = shared[j][0].pos()*shared[j][0].pos() + shared[j][1].pos()*shared[j][1].pos() + shared[j][2].pos()*shared[j][2].pos();
		    float rr1 = _sys[j][0].pos()*_sys[j][0].pos() + _sys[j][1].pos()*_sys[j][1].pos() + _sys[j][2].pos()*_sys[j][2].pos();
		    float rv0 = shared[j][0].vel()*shared[j][0].pos() + shared[j][1].vel()*shared[j][1].pos() + shared[j][2].vel()*shared[j][2].pos();
		    float rv1 = _sys[j][0].vel()*_sys[j][0].pos() + _sys[j][1].vel()*_sys[j][1].pos() + _sys[j][2].pos()*_sys[j][2].pos();
		    
		    //If inside the central body, or passing through pericentre, use 2-body approx.
		    if ((rv0*timestep <= 0.0 && rv1*timestep >=0.0)||min(rr0,rr1) <= _sys[0].attribute(0)*_sys[0].attribute(0))
		    {
		      atomicAdd(&shared[i][0].crash(),1);
		      atomicAdd(&shared[j][0].crash(),1);
		      //x cross v
		      float hx = shared[j][1].pos()*shared[j][2].vel() - shared[j][2].pos()*shared[j][1].vel();
		      float hy = shared[j][2].pos()*shared[j][0].vel() - shared[j][0].pos()*shared[j][2].vel();
		      float hz = shared[j][0].pos()*shared[j][1].vel() - shared[j][1].pos()*shared[j][0].vel();
		      
		      float v2 = shared[j][0].vel()*shared[j][0].vel() + shared[j][1].vel()*shared[j][1].vel() + shared[j][2].vel()*shared[j][2].vel();
		      float h2 = hx*hx+hy*hy+hz*hz;
		      float p = h2/(_sys[i].mass() + _sys[j].mass());
		      float r0 = sqrt(rr0);
		      float temp = 1.0 + p*(v2/(_sys[i].mass() + _sys[j].mass()) -2.0/r0);
		      float e = sqrt(max(temp,0.0));
		      float q = p/(1.0+e);
		      
		      if (q <= _sys[i].attribute(0)) // less than or equal the radius of the sun
		      {
			// modify the position and velocity of the sun
			float tmp2 = _sys[j].mass()/(_sys[j].mass()+_sys[i].mass());
			for( int c = 0; c<3 ; c++)
			{
			  _sys[i][c].pos() = tmp2*_sys[j][c].pos();
			  _sys[i][c].vel() = tmp2*_sys[j][c].vel();
			}
			_sys[i].mass() = _sys[j].mass()+_sys[i].mass();
			//eliminate the planet j
			for (int c = 0; c < 3;c++)
			{
			  _sys[j][c].pos() *= -1.0;
			  _sys[j][c].vel() *= -1.0;
			}
			_sys[j].mass() = 0.0;
			// TODO: calculate the energy lost
		      }
			
		    }
		    
		  }
		} 
		__syncthreads();
		if (thread_in_system == 0)
		{
		  for(int iter = 0; iter < nbod; iter++)
		    if (shared[iter][0].crash()> 1)
		    {
		      lprintf(_log,"Can not deal with the situation that has larger than 3 planets in collision!!! \n");
		      _sys.set_disabled();
		    }
		}
		__syncthreads();
		/** Modify position of the system if the sun is hitted.
		 *  Just translate the coordinate system by the new position of the sun.
		 */
		if (0 < thread_in_system < nbod)
		{
		  for (int c = 0; c<3 ; c++)
		  {
		    _sys[thread_in_system][c].pos() = _sys[thread_in_system ][c].pos() - _sys[0][c].pos();
		    _sys[thread_in_system][c].vel() = _sys[thread_in_system ][c].vel() - _sys[0][c].vel();
		  }
		  
		}
		
		
		
// 		if(is_condition_met() && is_deactivate_on() && (thread_in_system==0) )
// 		  {  _sys.set_disabled(); }
// 		return _sys.state();
		
	}
	
	
};

} // namespace monitor

} // namespace swarm
 
 
