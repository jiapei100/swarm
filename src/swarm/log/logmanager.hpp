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
#include "../common.hpp"
#include "log.hpp"
#include "writer.h"


namespace swarm { namespace log {

	/*! Manage CPU/GPU logs and writing them to appropriate output.
	 *  
	 *  Routines to coordinate three objects:
	 *   - host_log   : log data structure on host memory
	 *   - device_log : log data structure on device memory.
	 *   - writer     : provider class to write to desired output device
	 *  
	 *  Purpose:
	 *   - Stream log messages from device_log to host_log to writer
	 *   - Allocating memory for device_log and host_log
	 *   - Select and configure plugin for writer
	 *   - Stream lprintf messages to standard output
	 *
	 *
	 */
	class manager {
		//! Host log used by CPU integrators and used as temp for device_log
		gpulog::host_log hlog;
		//! Device log used by GPU integrators
		gpulog::device_log* pdlog;
		//! Writer plugin to output to a file
		Pwriter log_writer;
		public:

		enum { memory = 0x01, if_full = 0x02 };

		/*! Initialize logging system
		 * - Allocates memory for host_log
		 * - Allocates memory for device_log
		 * - Select plugin for writer
		 * - Configure writer plugin
		 */
		void init(const config&, int host_buffer_size = 50*1024*1024, int device_buffer_size = 50*1024*1024);

		/*! Stream the log from host_log and device_log to the output.
		 * - Replay lprintf
		 * - Download device_log to host_log
		 * - Output host_log to writer
		 */
		void flush(int flags = memory);

		//! Unitialize logging system
		void shutdown();

		gpulog::device_log* get_gpulog() { return pdlog; }
		gpulog::host_log* get_hostlog() { return &hlog; }

		//! Default log that is initialized in swarm::init(cfg)
		//! It is automatically used in \ref integrator.
		static shared_ptr<manager>& default_log();

	};

	typedef shared_ptr<manager> Pmanager;

}}
