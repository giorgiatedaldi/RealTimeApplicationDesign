/**
 * @file application-err_p.cpp
 * @author AMEDEO BERTUZZI 
 * @author GIORGIA TEDALDI 
 */

#include "executive.h"
#include "busy_wait.h"
#include <iostream>

Executive exec(5, 4);
int count = 0;

void task0()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*0.7);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;	
	debug << "Task 0 executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
}

void task1()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*1.7);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;
	debug << "Task 1 executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
}

void task2()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*2.0);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;
	debug << "Task 2 executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
}

void task3()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*2.7);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;
	debug << "Task 3 executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
	if (count % 3 == 0)
	{
		//exec.ap_task_request();
	}
	count++;
	
}

void task4()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*0.7);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;
	debug << "Task 4 executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
}

/* Note: in the code of one or more periodic task it is allowed to call Executive::ap_task_request() */

void ap_task()
{
	auto start = std::chrono::steady_clock::now();
	busy_wait(10*10);
	auto end = std::chrono::steady_clock::now();	
	std::chrono::duration<double, std::milli> elapsed(end - start);
	
	std::ostringstream debug;
	debug << "Task Aperiodic executing for " << elapsed.count() << "ms" << std::endl;
	std::cout << debug.str();
}

int main()
{
	busy_wait_init();

	exec.set_periodic_task(0, task0, 1); // tau_1
	exec.set_periodic_task(1, task1, 2); // tau_2
	exec.set_periodic_task(2, task2, 1); // tau_3,1
	exec.set_periodic_task(3, task3, 3); // tau_3,2
	exec.set_periodic_task(4, task4, 1); // tau_3,3
	/* ... */
	
	exec.set_aperiodic_task(ap_task, 2);
	
	exec.add_frame({0,1,2});
	exec.add_frame({0,3});
	exec.add_frame({0,1});
	exec.add_frame({0,1});
	exec.add_frame({0,1,4});
	/* ... */
	
	exec.run();
	
	return 0;
}
