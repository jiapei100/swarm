/*************************************************************************
 * Copyright (C) 2011 by Saleh Dindar and the Swarm-NG Development Team  *
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

#include <cuda_runtime_api.h>

namespace swarm {
namespace hp {

template<class N>
GPUAPI N sqr(const N& x) { return x*x; }

template<int i>
struct params_t {
	const static int n = i;
};

template<class implementation,class T>
__global__ void generic_kernel(implementation* integ,T a) {
	integ->kernel(a);
}


template< class implementation, class T>
void launch_template(implementation* integ, implementation* gpu_integ, T a)
{
	if(integ->get_ensemble().nbod() == T::n) 
		generic_kernel<<<integ->gridDim(), integ->threadDim(), integ->shmemSize() >>>(gpu_integ,a);

}

template<class implementation>
void launch_templatized_integrator(implementation* integ){

	if(integ->get_ensemble().nbod() <= 3){
		implementation* gpu_integ;
		cudaMalloc(&gpu_integ,sizeof(implementation));
		cudaMemcpy(gpu_integ,integ,sizeof(implementation),cudaMemcpyHostToDevice);

		launch_template(integ,gpu_integ,params_t<3>());

		cudaFree(integ);
	} else {
		// How do we get an error message out of here?
		ERROR("Invalid number of bodies. (only up to 10 bodies per system)");
	}

}


	
}
}
