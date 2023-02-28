### Design of a Real Time application
# Task Objective
The purpose of this project is the construction of a real time scheduling system that allows to execute a set of periodic tasks following clock driven approach, using a static scheduling algorithm that provides the system with a feasible schedule, if any, before the system runs. The system must also be able to execute aperiodic tasks.

# Executive
The executive is able to manage the execution of tasks by activating himself only at the beginning of each frame to perform the following actions:
- Execute the periodic tasks of the next frame, in the correct order;
- Check that the periodic tasks of the previous frame have finished;
- Manage the execution of aperiodic tasks avoiding interference with the schedule; 
- Detect and report any missing deadlines.
To achive this goals synchronization mechanisms such as condition variables and mutexes are used.

# Aperiodic Task
The execution of the aperiodic takes place in the slack time present in the frames immediately following the release one, without interfering with periodic tasks deadlines. 
The execution of the aperiodic task is considered correct when it ends within the number of frames specified in the release request.

# Authors
GIORGIA TEDALDI: giorgia.tedaldi@studenti.unipr.it
AMEDEO BERTUZZI: amedeo.bertuzzi@studenti.unipr.it



