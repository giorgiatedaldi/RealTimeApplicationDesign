/**
 * @file executive.h
 * @author AMEDEO BERTUZZI 340922
 * @author GIORGIA TEDALDI 339642
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
			Inizializza l'executive, impostando i parametri di scheduling:
			num_tasks: numero totale di task presenti nello schedule;
			frame_length: lunghezza del frame (in quanti temporali);
			unit_duration: durata dell'unita di tempo, in millisecondi (default 10ms).
		*/
		Executive(size_t num_tasks, unsigned int frame_length, unsigned int unit_duration = 10);

		/* 
			Imposta il task periodico di indice "task_id" (da invocare durante la creazione dello schedule):
			task_id: indice progressivo del task, nel range [0, num_tasks);
			periodic_task: funzione da eseguire al rilascio del task;
			wcet: tempo di esecuzione di caso peggiore (in quanti temporali).
		*/
		void set_periodic_task(size_t task_id, std::function<void()> periodic_task, unsigned int wcet);
		
		/* 
			Imposta il task aperiodico (da invocare durante la creazione dello schedule):
			aperiodic_task: funzione da eseguire al rilascio del task;
			wcet: tempo di esecuzione di caso peggiore (in quanti temporali).
		*/
		void set_aperiodic_task(std::function<void()> aperiodic_task, unsigned int wcet);
		
		/* 
			Lista di task da eseguire in un dato frame (da invocare durante la creazione dello schedule):
			frame: lista degli id corrispondenti ai task da eseguire nel frame, in sequenza.
		*/
		void add_frame(std::vector<size_t> frame);

		/* Esegue l'applicazione */
		void run();
		
		/* 
			Richiede il rilascio del task aperiodico (da invocare durante l'esecuzione).
		*/
		void ap_task_request();

	private:
		enum thread_type {PERIODIC, APERIODIC}; //utilizzato solo per la stampa di debug
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
		std::vector<size_t> slack_times; //vettore che contiene i vari slack time dei diversi frame
		
		const unsigned int frame_length; // lunghezza del frame (in quanti temporali)
		const std::chrono::milliseconds unit_time; // durata dell'unita di tempo (quanto temporale)		

		bool ap_request; //flag per la richiesta di attivazione dell'aperiodico		

		/**
		 * Funzione per settare la priorità del thread.
		 */
		void set_thread_priority(std::thread &th, rt::priority &p); 

		static void task_function(task_data & task, std::mutex &state_mutex);
		
		void exec_function();
		
		
};

#endif
