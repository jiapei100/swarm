## @file plot_tutorial.py Tutorial for extracting and plotting information from the BDB log files.
#  refer to @ref TutorialPythonQueryPlot for the formatted version

# @page TutorialPythonQueryPlot Plotting results from a log file
#
# In this tutorial, we are going to explore how to examine the
# contents of a log file generated by Swarm-NG library. For
# more information on how to generate log files refer to @ref Configuration
#
# Source code for this tutorial can be found at @ref py/plot_tutorial.py
#
# To open and query a log file, we use \ref swarmng.logdb.IndexedLogDB 
# class. You can see the range of all queries that are possible using
# IndexedLogDB class in the API documentation.
#
# To begin, import the module and create the IndexedLogDB with a filename
#
import swarmng.logdb
d = swarmng.logdb.IndexedLogDB("sample_log.db")
#
# In this example, we have already created a `sample_log.db` file
# using the following command line:
# @verbatim bin/swarm integrate nsys=16 nbod=3 integrator=hermite_cpu_log time_step=0.001 log_writer=bdb log_output_db=sample_log.db log_time_interval=1 destination_time=100 @endverbatim
#
# Now we use two methods from IndexedLogDB, namely \ref swarmng.logdb.IndexedLogDB.initial_conditions "initial_conditions" and \ref swarmng.logdb.IndexedLogDB.final_conditions "final_conditions".
# These methods give you list of records for initial conditions and final conditions
# for the systems.
#
# To specify ranges, you need to use a special class swarmng.range_type.Range.
# you can specify ranges in 3 ways:
#
# All of the entries in the range, which basically does not restrict the results at all
import swarmng.range_type
sr1 = swarmng.range_type.Range.universal()
# One system ID (in this example system number 42)
sr2 = swarmng.range_type.Range.single(42)
# A continuous range of systems (including both ends)
sr3 = swarmng.range_type.Range.interval(60,80)
#
# Now we get the initial conditions for all of the systems
# note that all query methods return iterables and not
# the actual data.
ic = d.initial_conditions(sr1)
# To get the data, you should run a for loop over the returned
# object:
from logrecord_tutorial import get_semi_major_axis
initial = []
for i, l in ic:
  initial.append(get_semi_major_axis(l, body=1))
  
# In this for loop, i is the index of the system and l is a
# swarmng.logrecord.LogRecord object that has simple methods such
# as time, event_id, accessors for the planets, etc.
# for a tutorial on LogRecord object refer to @ref TutorialPythonLogRecord
# 
# In this example we want to extract the semi-major axis for the 
# first planet.
#
# The final_conditions method uses very similar semantics, the code
# is almost identical. By the way, you do not need to assign the result
# to an object, you can just call the method from the first line of
# the for loop
final = []
for i, l in d.final_conditions(sr1):
  final.append(get_semi_major_axis(l, body=1))

# Just to make the data more meaninful, we only want to 
# know the difference between final and initial semi-major axis.
# we create the diff array like this:
diff = []
for i, f in zip(initial,final):
  diff.append(i - f)

# Now we have all the data we need in two arrays: initial and final.
# We can use the matplotlib package to visualize this data. for more
# information on how to make customized plots look at matplotlib 
# documentation at http://matplotlib.org/
#

import matplotlib.pyplot as P 
P.scatter(initial,diff)
P.savefig("initial_vs_final_semi-major_axis")
P.show()

# If you are running this script in an environment where X server
# is not accessible, just comment out the last line `P.show()`. The
# generated plot is saved is `initial_vs_final_semi-major_axis.png`.
#
# To learn how to make more complicated plots, try reading the 
# API documentation for following classes:
# * swarmng.logdb.IndexedLogDB : for running other types of queries
# * swarmng.logrecord.LogRecord : to extract other information about planetary systems, e.g. time, eccentricity of planets, etc.
# 
