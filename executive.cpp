/**
 * @file executive.cpp
 * @author AMEDEO BERTUZZI 
 * @author GIORGIA TEDALDI 
 */

#include <cassert>
#include <iostream>
#include <sstream>

#include "executive.h"

#include "rt/priority.h"
#include "rt/affinity.h"

Executive::Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration)
	: p_tasks(num_tasks), frame_length(frame_length), unit_time(unit_duration), ap_request(false)
{
}

void Executive::set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int wcet)
{
	assert(task_id < p_tasks.size()); //It fails if task_id is not correct (out of range)
	
	p_tasks[task_id].function = periodic_task;
	p_tasks[task_id].wcet = wcet;
	p_tasks[task_id].miss = false;
	p_tasks[task_id].state = IDLE;
	p_tasks[task_id].type = PERIODIC;
	p_tasks[task_id].id = task_id;
}

void Executive::set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet)
{
 	ap_task.function = aperiodic_task;
 	ap_task.wcet = wcet;
	ap_task.state = IDLE;
	ap_task.type = APERIODIC;
}
		
void Executive::add_frame(std::vector<size_t> frame)
{
	for (auto & id: frame)
		assert(id < p_tasks.size()); //It fails if task_id is not correct (out of range)
	
	frames.push_back(frame);

	unsigned int tot_wcet = 0;
	for (size_t i = 0; i < frame.size(); i++)
	{
		tot_wcet += p_tasks[frame[i]].wcet;
	}

	slack_times.push_back(frame_length-tot_wcet); //Vector's construction that contains pre-computed slack-times
}

//START RUN
void Executive::run()
{
	//EXECUTIVE THREAD INITIALIZATION
	rt::priority exec_prio(rt::priority::rt_max);
	rt::affinity aff("1");


	//PERIODIC TASK THREAD INITIALIZATION
	for (size_t id = 0; id < p_tasks.size(); ++id)
	{
		
		assert(p_tasks[id].function); //It fails if set_periodic_task() has not been invoked for this id
		
		p_tasks[id].thread = std::thread(&Executive::task_function, std::ref(p_tasks[id]), std::ref(state_mutex));

	}
	

	//APERIODIC TASK THREAD INITIALIZATION
	assert(ap_task.function); // It fails if set_aperiodic_task() has not been invoked
	
	ap_task.thread = std::thread(&Executive::task_function, std::ref(ap_task), std::ref(state_mutex));
	rt::priority a_prio(rt::priority::rt_min);

	set_thread_priority(ap_task.thread, a_prio);
	rt::set_affinity(ap_task.thread, aff);
	
	std::thread exec_thread(&Executive::exec_function, this);

	set_thread_priority(exec_thread, exec_prio);
	rt::set_affinity(exec_thread, aff);
	
	
	//FINAL JOIN
	exec_thread.join();
	
	ap_task.thread.join();
	
	for (auto & pt: p_tasks)
		pt.thread.join();
}

void Executive::ap_task_request() 
{
	std::unique_lock<std::mutex> lock(ap_request_mutex);
    ap_request = true;
}

void Executive::set_thread_priority(std::thread &th, rt::priority &p)
{
	try
		{
			rt::set_priority(th,p);
		}
		catch(rt::permission_error & e)
		{
			std::cerr << "Error setting priorities" << e.what()<<  std::endl;
			th.detach();
			return;
		}
}

void Executive::task_function(Executive::task_data & task,  std::mutex &state_mutex)
{
	while (true)
	{
		{
			std::unique_lock<std::mutex> lock(state_mutex);
			while (task.state!= PENDING)
			{
				task.cond.wait(lock);
			}
			task.state=RUNNING;
			
			//debug
			if (task.type == PERIODIC)
			{
				std::ostringstream debug;
				debug << "Task " << task.id << " RUNNING"<< std::endl;
				std::cout << debug.str();
			}
			else
			{
				std::ostringstream debug;
				debug << "Task Aperiodico RUNNING"<< std::endl;
				std::cout << debug.str();
			}
			
		} 

		task.function();

		{
			std::unique_lock<std::mutex> lock(state_mutex);
			task.state = IDLE;
			
			//debug
			if (task.type == PERIODIC)
			{
				std::ostringstream debug;
				debug << "Task " << task.id << " IDLE"<< std::endl;
				std::cout << debug.str();
			}
			else
			{
				std::ostringstream debug;
				debug << "Task Aperiodico IDLE"<< std::endl;
				std::cout << debug.str();
			}
		}

	}
}

//EXECUTIVE THREAD
void Executive::exec_function()
{
	unsigned long frame_id = 0;

	auto last = std::chrono::steady_clock::now();
	auto next_frame = std::chrono::steady_clock::now();	

	bool ap_running = false;

	while (true)
	{
		std::ostringstream debug;
		debug << "-----Executive: frame_id " << frame_id <<  " starting-----" << std::endl;			
		std::cout << debug.str();
		

		//ap_request check
		{
			std::unique_lock<std::mutex> lock(ap_request_mutex);
			if(ap_request)
			{
				if(ap_running)
				{
					std::cout << "Deadline miss task aperiodico"<< std::endl;
				}
				else
				{
					ap_running = true;
				}
				ap_request = false;
			}
		}



		//SET PRIORITY & WAKE-UP TASK
		//Tasks' priority is set dinamiccaly in each frame.
		/**
		 * PRIORITY MANAGMENT
		 * 
		 * Executive has always the maximum priority.
		 * If in ap_running the policy used is 'slack stealing': during the slack time any periodic task in deadline miss has
		 * priority equal to (MAX - 1), the aperiodic task has priority (MAX - 2), while the periodic tasks to schedule in the frame
		 * have lower priority.
		 * At the end of slack time, when the executive wakes up, the aperiodic task's priority is set to MIN,
		 * the one of any periodic task in deadline miss to (MIN + 1), while the one of the other periodic task will be higher
		 * and defined according to the established order execution.
		 * 
		 * In particular: if a task is in deadline miss and it has an execution even in the next fram, then its execution is skipped.
		 * The task in question executes in any remaining time at the end of the frame (and / or in the slack time) in order to avoid
		 * delaying periodic tasks.
		 * 
		 */
		rt::priority thread_prio(rt::priority::rt_max);
		thread_prio -= 3;
		rt::affinity aff("1");
		{
			std::unique_lock<std::mutex> lock(state_mutex);
			for (size_t i = 0; i < frames[frame_id].size(); i++)
			{
				if (p_tasks[frames[frame_id][i]].state == IDLE)
				{
					set_thread_priority(p_tasks[frames[frame_id][i]].thread, thread_prio);
					rt::set_affinity(p_tasks[frames[frame_id][i]].thread, aff);
					--thread_prio;

					p_tasks[frames[frame_id][i]].state = PENDING;
					
					std::ostringstream debug;
					debug << "Task " << p_tasks[frames[frame_id][i]].id << " PENDING"<< std::endl;
					std::cout<< debug.str();
					
					p_tasks[frames[frame_id][i]].cond.notify_one();
				}
			}
		}

		//WAKE-UP APERIODIC
		if (ap_running)
		{
			rt::priority prio(rt::priority::rt_max);
			--prio;

			for(size_t i = 0; i < p_tasks.size(); i++)
			{
				if(p_tasks[i].miss)
				{
					set_thread_priority(p_tasks[i].thread, prio);
				}
			}

			--prio;
			set_thread_priority(ap_task.thread, prio);

			{
				std::unique_lock<std::mutex> lock(state_mutex);
				if (ap_task.state == IDLE)
				{
					ap_task.state = PENDING;
					
					std::ostringstream debug;
					debug << "Task Aperiodico PENDING"<< std::endl;
					std::cout<< debug.str();
					
					ap_task.cond.notify_one();
				}
			}

			std::ostringstream debug;
			debug << "-----Exec Sleeping for SLACK TIME-----" << std::endl;
			std::cout << debug.str();

			//Executive sleeps for slack_time
			next_frame += std::chrono::milliseconds(slack_times[frame_id]*unit_time);
			std::this_thread::sleep_until(next_frame);
			
			auto next = std::chrono::steady_clock::now();
				
			std::chrono::duration<double, std::milli> elapsed(next - last);

			std::ostringstream debug1;
			debug1 << "-----Exec: end slack time" << elapsed.count() << "-----"<< std::endl;
			std::cout << debug1.str();

			//Executive wakes up and updates priority to the aperiodic task and to any periodic task in deadline miss.
			set_thread_priority(ap_task.thread, prio);
			
			++prio;
			for(size_t i = 0; i < p_tasks.size(); i++)
			{
				if(p_tasks[i].miss)
				{
					set_thread_priority(p_tasks[i].thread, prio);
				}
			}

			next_frame += std::chrono::milliseconds((frame_length*unit_time)-slack_times[frame_id]*unit_time);
		}
		else
		{
			std::ostringstream debug;
			debug << "-----Exec sleeping for FRAME TIME-----" << std::endl;
			std::cout << debug.str();
			
			next_frame += std::chrono::milliseconds(frame_length*unit_time);
		}

		std::this_thread::sleep_until(next_frame);
		auto next = std::chrono::steady_clock::now();
		std::chrono::duration<double, std::milli> elapsed(next - last);
		
		std::ostringstream debug2;
		debug2 << "------Exec: end frame " << elapsed.count() << "-----"<< std::endl << std::endl << std::endl;
		std::cout << debug2.str();
		last = next;
		
		//CHECK DEADLINE MISS
		rt::priority miss_prio(rt::priority::rt_min);
		++miss_prio;
		
		{
			std::unique_lock<std::mutex> lock(state_mutex);
			for(size_t i = 0; i < p_tasks.size(); i++)
			{
				if((p_tasks[i].miss) && (p_tasks[i].state == IDLE))
				{
					p_tasks[i].miss = false;
				}
			}

			if(ap_task.state == IDLE && ap_running)
			{
				ap_running = false;
			}

			for (size_t i = 0; i < frames[frame_id].size(); i++)
			{				
				std::ostringstream debug;

				if (p_tasks[frames[frame_id][i]].state != IDLE)
				{
					p_tasks[frames[frame_id][i]].miss = true;
					set_thread_priority(p_tasks[frames[frame_id][i]].thread, miss_prio);
					
					debug << "Deadline miss task periodico di ID "<< p_tasks[frames[frame_id][i]].id << std::endl;
					std::cout << debug.str();
				}
				else
				{
					debug << "Check miss superato. Task periodico: stato IDLE, ID " << p_tasks[frames[frame_id][i]].id << std::endl;
					std::cout << debug.str();
				}
				
			}
		}

		std::ostringstream debug3;
		debug3 << std::endl << std::endl;
		std::cout << debug3.str();
		

		//FRAME ADVANCE
		if (++frame_id == frames.size())
		{
			frame_id = 0;
		}
	}
}


