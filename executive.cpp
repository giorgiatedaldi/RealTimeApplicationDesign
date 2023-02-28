/**
 * @file executive.cpp
 * @author AMEDEO BERTUZZI 340922
 * @author GIORGIA TEDALDI 339642
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
	assert(task_id < p_tasks.size()); // Fallisce in caso di task_id non corretto (fuori range)
	
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
		assert(id < p_tasks.size()); // Fallisce in caso di task_id non corretto (fuori range)
	
	frames.push_back(frame);

	unsigned int tot_wcet = 0;
	for (size_t i = 0; i < frame.size(); i++)
	{
		tot_wcet += p_tasks[frame[i]].wcet;
	}

	slack_times.push_back(frame_length-tot_wcet); //costruzione del vettore contenente i vari slack time precalcolati
}

//RUN DI START
void Executive::run()
{
	//INIZIALIZZAZIONE EXECUTIVE THREAD
	rt::priority exec_prio(rt::priority::rt_max);
	rt::affinity aff("1");


	//INIZIALIZZAZIONE PERIODIC TASK THREAD
	for (size_t id = 0; id < p_tasks.size(); ++id)
	{
		assert(p_tasks[id].function); // Fallisce se set_periodic_task() non e' stato invocato per questo id
		
		p_tasks[id].thread = std::thread(&Executive::task_function, std::ref(p_tasks[id]), std::ref(state_mutex));

	}
	

	//INIZIALIZZAZIONE APERIODIC TASK THREAD
	assert(ap_task.function); // Fallisce se set_aperiodic_task() non e' stato invocato
	
	ap_task.thread = std::thread(&Executive::task_function, std::ref(ap_task), std::ref(state_mutex));
	rt::priority a_prio(rt::priority::rt_min);

	set_thread_priority(ap_task.thread, a_prio);
	rt::set_affinity(ap_task.thread, aff);
	
	std::thread exec_thread(&Executive::exec_function, this);

	set_thread_priority(exec_thread, exec_prio);
	rt::set_affinity(exec_thread, aff);
	
	
	//JOIN FINALI
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

//FUNZIONE DEL THREAD ESECUTIVO
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
		

		//Controllo di ap_request
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
		//La priorità dei task viene settata dinamicamente ogni frame.
		/**
		 * GESTIONE DELLE PRIORITA
		 * 
		 * L'executive ha sempre priorità massima.
		 * Se un ap_running viene utilizzata la politica di slack stealing: durante lo slack time gli eventuali
		 * thread periodici in deadline miss hanno tutti priorità pari a (MAX - 1), mentre l'apriodico (MAX - 2), mentre 
		 * i periodici da schedulare nel frame hanno priorità inferiore.
		 * Alla fine dello slack time, al risveglio dell'executive, la priorità dell'apriodico viene settata a MIN,
		 * quella di eventuali periodici in deadline miss tutti a (MIN + 1), mentre quella degli altri task periodici sarà superiore
		 * e definita in base all'ordine di esecuzione.
		 * 
		 * In particolare: se un task è in deadline miss e presenta un esecuzione anche nel frame successivo la
		 * sua esecuzione viene saltata. Il task in questione esegue nell'eventuale tempo rimasto alla fine del frame (e/o nello slack time), 
		 * in modo da evitare anche il ritardo degli altri task periodici.
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

			//Executive dorme per slack_time
			next_frame += std::chrono::milliseconds(slack_times[frame_id]*unit_time);
			std::this_thread::sleep_until(next_frame);
			
			auto next = std::chrono::steady_clock::now();
				
			std::chrono::duration<double, std::milli> elapsed(next - last);

			std::ostringstream debug1;
			debug1 << "-----Exec: end slack time" << elapsed.count() << "-----"<< std::endl;
			std::cout << debug1.str();

			//si sveglia e cambia la priorità all'aperiodico e ai periodici in deadline miss
			prio = rt::priority::rt_min;
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
		

		//AVANZAMENTO FRAME
		if (++frame_id == frames.size())
		{
			frame_id = 0;
		}
	}
}


