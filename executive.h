/**
 * @file executive.h
 * @author AMEDEO BERTUZZI
 * @author GIORGIA TEDALDI
 */

#ifndef EXECUTIVE_H
#define EXECUTIVE_H

#include <vector>
#include <functional>
#include <chrono>
#include <thread>
#include <sstream>
#include <mutex>
#include <condition_variable>
#include "rt/priority.h"
#include "rt/affinity.h"

class Executive
{
	public:
		/* 
			Executive initialization and parameters set up:
			num_tasks: total number of tasks in the schedule;
			frame_length: frame's lenght;
			unit_duration: unit time duration, in milliseconds (default 10ms).
		*/
		Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration = 10);

		/* 
			Function to set the periodic task with index "task_id" (to be called during the schedule's creation):
			task_id: progressive index of the task, in range [0, num_tasks);
			periodic_task: function to executed when task is realised;
			wcet: worst case execution time.
		*/
		void set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int wcet);
		
		/* 
			Function to set the aperiodic task (to call during the schedule's creation):
			aperiodic_task: function to execute when the task is released;
			wcet: worst case execution time.
		*/
		void set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet);
		
		/* 
			List of tasks to execute in a specific frame (to call during the schedule's creation)
			frame: ordered list of task's id to execute in the frame.
		*/
		void add_frame(std::vector<size_t> frame);

		/* Function to execute the application */
		void run();
		
		/* 
			Function to request aperiodic task release (to call during the execution).
		*/
		void ap_task_request();

	private:
		enum thread_type {PERIODIC, APERIODIC}; //used to print debug info
		enum thread_state {PENDING, IDLE, RUNNING};

		std::mutex state_mutex;
		std::mutex ap_request_mutex;

		struct task_data
		{
			std::function<void()> function;
			unsigned int wcet;
			std::thread thread;
			thread_type type;
			thread_state state;
			std::condition_variable cond;
			int id;
			bool miss;
		};
		
		std::vector<task_data> p_tasks;
		task_data ap_task;
		
		std::vector< std::vector<size_t> > frames;
	
		std::vector<size_t> slack_times; //vector that contains slack times of the different frames
		
		const unsigned int frame_length; // frames' length
		const std::chrono::milliseconds unit_time; // unit time duration		

		bool ap_request; //flag to request the activation of the aperiodic task
	
		/**
		 * Function to set the thread's priority.
		 */
		void set_thread_priority(std::thread &th, rt::priority &p); 

		static void task_function(task_data & task, std::mutex &state_mutex);
		
		void exec_function();
		
		
};

#endif
