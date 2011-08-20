#include <iostream>
#include <boost/program_options.hpp>
#include <boost/program_options/positional_options.hpp>
#include "swarm.h"
#include "snapshot.hpp"
#include "stopwatch.h"

int DEBUG_LEVEL  = 0;

using namespace swarm;
using namespace std;
namespace po = boost::program_options;
using boost::bind;
using boost::shared_ptr;

typedef shared_ptr<integrator> Pintegrator;


// Runtime variables
config cfg = default_config();
config base_cfg;
defaultEnsemble initial_ens;
defaultEnsemble current_ens;
defaultEnsemble reference_ens;
po::variables_map argvars_map;
Pintegrator integ;

void stability_test() {

	defaultEnsemble &ens = current_ens;

	double begin_time = ens.time_ranges().average;
	double destination_time = cfg.optional("destination time", begin_time + 10 * M_PI );
	double interval = cfg.optional("interval", (destination_time-begin_time) ) ; 
	double logarithmic = cfg.optional("logarithmic", 0 ) ; 

	if(destination_time < begin_time ) ERROR("Destination time should be larger than begin time");
	if(interval < 0) ERROR("Interval cannot be negative");
	if(interval < 0.001) ERROR("Interval too small");
	if(logarithmic != 0 && logarithmic <= 1) ERROR("logarithm base should be greater than 1");


	std::cout << "Time, Energy Conservation Error " << std::endl;
	for(double time = begin_time; time < destination_time; ) {

		if(logarithmic > 1)
			time = (time > 0) ? time * logarithmic : interval;
		else
			time += interval;

		double effective_time = min(time,destination_time);
		integ->set_destination_time ( effective_time );

		DEBUG_OUTPUT(2, "Integrator ensemble" );
		integ->integrate();

		SYNC;
		DEBUG_OUTPUT(2, "Check energy conservation" );
		double max_deltaE = find_max_energy_conservation_error(ens, initial_ens );
		std::cout << effective_time << ", " << max_deltaE << std::endl;

	}

}

void load_generate_ensemble(){
	// Load/Generate the ensemble
	if(cfg.valid("input") ) {
		cout << "Lading initial conditions from " << cfg["input"];
		initial_ens = swarm::snapshot::load(cfg["input"]);	
		cout << ", time = " << initial_ens.time_ranges() << endl;
		
	}else{
		cout << "Generating new ensemble:  " << cfg["nsys"] << ", " << cfg["nbod"] << endl;
		initial_ens = generate_ensemble(cfg);
	}
}

void save_ensemble(){ 
	// Save the ensemble
	if(cfg.valid("output")) {
		cout << "Saving to " << cfg["output"];
		cout << ", time = " << current_ens.time_ranges() << endl;
		swarm::snapshot::save(current_ens,cfg["output"]);	
	}
}

void prepare_integrator () {
	// Initialize Integrator
	DEBUG_OUTPUT(2, "Initializing integrator" );
	double begin_time = initial_ens.time_ranges().average;
	double destination_time = cfg.optional("destination time", begin_time + 10 * M_PI );
	integ.reset(integrator::create(cfg));
	integ->set_ensemble(current_ens);
	integ->set_destination_time ( destination_time );
	SYNC;
}

void run_integration(){
	if(!validate_configuration(cfg) ) ERROR( "Invalid configuration" );

	load_generate_ensemble();

	DEBUG_OUTPUT(2, "Make a copy of ensemble for energy conservation test" );
	current_ens = initial_ens.clone();

	prepare_integrator();

	double integration_time = watch_time ( stability_test );

	save_ensemble();

	std::cout << "\n# Integration time: " << integration_time << " ms " << std::endl;
}

void reference_integration() {
	DEBUG_OUTPUT(2, "Make a copy of ensemble for energy conservation test" );
	reference_ens = initial_ens.clone();
	prepare_integrator();
	integ->integrate();
}

void benchmark_item(const string& param, const string& value) {
	if(!validate_configuration(cfg) ) ERROR( "Invalid configuration" );

	if(param == "input" || param == "nsys" || param == "nbod" || cfg.count("reinitialize"))
		load_generate_ensemble();

	DEBUG_OUTPUT(2, "Make a copy of ensemble for energy conservation test" );
	current_ens = initial_ens.clone();

	double init_time = watch_time( prepare_integrator );

	DEBUG_OUTPUT(2, "Integrator ensemble" );
	double integration_time = watch_time( bind(&integrator::integrate,integ.get())) ;

	double max_deltaE = find_max_energy_conservation_error(current_ens, initial_ens );
	
	// TODO: compare with reference ensemble
	/// CSV output for use in spreadsheet software 
	std::cout << param << ", "
	          << value << ",  "   
	          << max_deltaE << ",    " 
	          << integration_time << ",    "
	          << init_time 
	          << std::endl;
}

void benchmark(){ 
	// Reference settings
	if(!validate_configuration(cfg) ) ERROR( "Invalid configuration" );
	load_generate_ensemble();
	reference_integration();

	// Go through the loop
	std::cout << "Parameter, Value, Energy Conservation Error"
			     ", Integration (ms), Integrator initialize (ms) \n";

	po::variables_map &vm = argvars_map;
	if((vm.count("parameter") > 0) && (vm.count("value") > 0)) {
		
		// Iterate over values
		string param = vm["parameter"].as< vector<string> >().front();
		vector<string> values = vm["value"].as< vector<string> >();

		for(vector<string>::const_iterator i = values.begin();  i != values.end(); i++){
			string value = *i;

			if(param == "config")
				cfg = config::load(value,base_cfg);
			else
				cfg[param] = *i;

			DEBUG_OUTPUT(1, "=========================================");
			benchmark_item(param,value);
		}

	} else if ((vm.count("parameter") > 0) &&(vm.count("from") > 0) && (vm.count("to") > 0)){

		// Numeric range
		int from = vm["from"].as<int>(), to = vm["to"].as<int>();
		int increment = vm.count("inc") > 0 ? vm["inc"].as<int>() : 1;
		string param = vm["parameter"].as< vector<string> >().front();

		for(int i = from; i <= to ; i+= increment ){
			std::ostringstream stream;
			stream <<  i;
			string value = stream.str();

			cfg[param] = value;

			DEBUG_OUTPUT(1, "=========================================");
			benchmark_item(param,value);
		}

	} else {
		std::cerr << "Invalid combination of parameters and values " << std::endl;
	}
}

void parse_commandline_and_config(int argc, char* argv[]){
	po::positional_options_description pos;
	po::options_description desc("Usage: swarm [options] COMMAND [PARAMETER VALUES] \nOptions");

	pos.add("command", 1);
	pos.add("parameter", 1);
	pos.add("value", -1);

	desc.add_options()
		("command" , po::value<string>() , "Swarm command: stability, integrate, benchmark ")
		("parameter" , po::value<vector<string> >() , "Parameteres to benchmark: config, integrator, nsys, nbod, blocksize, ... ")
		("value" , po::value<vector<string> >() , "Values to iterate over ")
		("from", po::value<int>() , "from integer value")
		("to", po::value<int>() , "to integer value")
		("inc", po::value<int>() , "increment integer value")
		("destination time,d", po::value<std::string>() , "Destination time to achieve")
		("logarithmic,l", po::value<std::string>() , "Produce times in logarithmic scale" )
		("interval,n", po::value<std::string>() , "Energy test intervals")
		("input,i", po::value<std::string>(), "Input file")
		("output,o", po::value<std::string>(), "Output file")
		("cfg,c", po::value<std::string>(), "Integrator configuration file")
		("help,h", "produce help message")
		("plugins,p", "list all of the plugins")
		;

	po::variables_map &vm = argvars_map;
	po::store(po::command_line_parser(argc, argv).
			options(desc).positional(pos).run(), vm);
	po::notify(vm);

	//// Respond to switches 
	//
	if (vm.count("help")) { std::cout << desc << "\n"; exit(1); }
	if (vm.count("verbose") ) DEBUG_LEVEL = vm["verbose"].as<int>();

	if (vm.count("plugins")) {
		cout << swarm::plugin::help;
		exit(0);
	}

	if(vm.count("cfg")){
		std::string icfgfn =  vm["cfg"].as<std::string>();
		cfg = config::load(icfgfn, cfg);
	}

	if(vm.count("command") == 0){
		std::cout << desc << "\n"; exit(1); 
	}

	const int cmd_to_config_len = 5;
	const char* cmd_to_config[cmd_to_config_len] = { "input", "output", "destination time", "logarithmic", "interval" };
	for(int i = 0; i < cmd_to_config_len; i++)
		if(vm.count(cmd_to_config[i]))
			cfg[cmd_to_config[i]] = vm[cmd_to_config[i]].as<std::string>();

}


int main(int argc, char* argv[]){

	parse_commandline_and_config(argc,argv);

	outputConfigSummary(std::cout,cfg);

	base_cfg = cfg;

	// Initialize Swarm
	swarm::init(cfg);

	string command = argvars_map["command"].as< string >();
	if(command == "integrate" || command == "stability")
		run_integration();
	else if(command == "benchmark" || command == "verify" )
		benchmark();
	else
		std::cerr << "Valid commands are: integrate, stability, benchmark, verify " << std::endl;

	return 0;
}