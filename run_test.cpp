/* Copyright 2009, 2010 Brendan Tauras */

/* run_test.cpp is part of FlashSim. */

/* FlashSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. */

/* FlashSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License
 * along with FlashSim.  If not, see <http://www.gnu.org/licenses/>. */

/****************************************************************************/

/* Basic test driver
 * Brendan Tauras 2009-11-02
 *
 * driver to create and run a very basic test of writes then reads */

#include "ssd.h"
#include <time.h>
#include <stdlib.h>
#include <ctime> 
#include <fstream>
 #include <iostream>
#include <sstream>
#include <string>
#define SIZE 130

using namespace ssd; 

int main()
{
	load_config();
	//print_config(NULL);
   //printf("Press ENTER to continue...");
   //getchar();
   printf("\n");

	Ssd *ssd = new Ssd();

	double result;

//	// Test one write to some blocks.
//	for (int i = 0; i < SIZE; i++)
//	{
//		/* event_arrive(event_type, logical_address, size, start_time) */
//		result = ssd -> event_arrive(WRITE, i*100000, 1, (double) 1+(250*i));
//
//		printf("Write time: %.20lf\n", result);
////		result = ssd -> event_arrive(WRITE, i+10240, 1, (double) 1);
////
//	}
//	for (int i = 0; i < SIZE; i++)
//	{
//		/* event_arrive(event_type, logical_address, size, start_time) */
//		result = ssd -> event_arrive(READ, i*100000, 1, (double) 1+(500*i));
//		printf("Read time : %.20lf\n", result);
////		result = ssd -> event_arrive(READ, i, 1, (double) 1);
////		printf("Read time : %.20lf\n", result);
//	}

//	// Test writes and read to same block.
//	for (int i = 0; i < SIZE; i++)
//	{
//		result = ssd -> event_arrive(WRITE, i%64, 1, (double) 1+(250*i));
//
//		printf("Write time: %.20lf\n", result);
//	}
//	for (int i = 0; i < SIZE; i++)
//	{
//		result = ssd -> event_arrive(READ, i%64, 1, (double) 1+(500*i));
//		printf("Read time : %.20lf\n", result);
//	}

	// Test random writes to a block
	/*result = ssd -> event_arrive(WRITE, 5, 1, (double) 0.0);
	printf("Write time: %.20lf\n", result);
	result = ssd -> event_arrive(WRITE, 4, 1, (double) 300.0);
	printf("Write time: %.20lf\n", result);
	result = ssd -> event_arrive(WRITE, 3, 1, (double) 600.0);
	printf("Write time: %.20lf\n", result);
	result = ssd -> event_arrive(WRITE, 2, 1, (double) 900.0);
	printf("Write time: %.20lf\n", result);
	result = ssd -> event_arrive(WRITE, 1, 1, (double) 1200.0);
	printf("Write time: %.20lf\n", result);
	result = ssd -> event_arrive(WRITE, 0, 1, (double) 1500.0);
	printf("Write time: %.20lf\n", result);
*/
//	for (int i = 0; i < SIZE-6; i++)
	{
		/* event_arrive(event_type, logical_address, size, start_time) */
//		result = ssd -> event_arrive(WRITE, 6+i, 1, (double) 1800+(300*i));
//		printf("Write time: %.20lf\n", result);
	}

	// Force Merge
//	result = ssd -> event_arrive(WRITE, 10 , 1, (double) 0.0);
//	printf("Write time: %.20lf\n", result);
//	for (int i = 0; i < SIZE; i++)
//	{
//		/* event_arrive(event_type, logical_address, size, start_time) */
//		result = ssd -> event_arrive(READ, i%64, 1, (double) 1+(500*i));
//		printf("Read time : %.20lf\n", result);
//	}
	printf("Number Of blocks: %d\n", SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE);

	printf("SSD_SIZE: %d MB\n", SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE*PAGE_SIZE/1024/1024);
	
	int count[(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)/2];
	for (int i = 0; i < (SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)/2; ++i)
	{
		count[i] = 0;
	}
	FILE *logFile = NULL;
	if ((logFile = fopen("output1.csv", "w")) == NULL)
	{
		printf("Output file cannot be written to.\n");
		exit(-1);
	}


	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    fp = fopen("./Trace/Traces/Exchange.csv", "r");
    if (fp == NULL) {
    	printf("fail open trace file.\n");
        exit(EXIT_FAILURE);
    }
    int total = 0;
    int writeNumber = 0;
    int readNumber = 0;

    while ((read = getline(&line, &len, fp)) != -1) {
	    std::stringstream   linestream(line);
    	std::string         value;
    	int countLine = 1;
    	std::string _type = "";
    	unsigned long int _time = 0;
    	unsigned long int _address = 0;
    	unsigned long int _size = 0; 
        while(getline(linestream,value,','))
    	{	
    		if(countLine == 1) {
    			_type = value;

    		} else if (countLine == 2) {
    			//_time = std::stoi( value );
    			_time = std::stoul(value, nullptr, 10);
    			//_address = (std::stoul(value, nullptr, 10))%(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE);
    		} else if(countLine == 4) {
    			unsigned long int x = std::stoul(value, nullptr, 16);
    			//std::cout << value << " : " << x << "\n";
    			_address = x%(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE);
    		
    			//_size = std::stoul(value, nullptr, 10) / PAGE_SIZE;
    		} else if(countLine == 5) {
    			unsigned long int x = std::stoul(value, nullptr, 16);
    			//std::cout << value << " : " << x/PAGE_SIZE << "\n";
    			_size = x / PAGE_SIZE;
    			//_type = value;
    		} else if (countLine == 6) {
    			//_time = std::stoi( value ) * 100;
    		}
          	countLine++;
    	}

    	for(int i = 0; i < _size; i++){
    		total++;
    		if(_type.compare("              DiskWrite") == 0 ) {
    			writeNumber++;
    			result = ssd -> event_arrive(WRITE, _address, 1, _time);
    			fprintf(logFile, "%lu\n", _address);
    			//printf("Write to logical_address: %lu\n", _address);
    			Block_manager::instance()->writeToFile();
	//ssd->print_ftl_statistics();
    			//ssd->writeToFile();

    		} else if (_type.compare("R ") == 0) {
    			readNumber++;
    			result = ssd -> event_arrive(READ, _address, 1, _time);
    			printf("Read from logical_address: %lu\n", _address);
    		}
    		_address++;
    		_address = _address % (SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE);
    	}
 
    }

    fclose(fp);
    if (line)
        free(line);

    printf("\n\n total: %d readNumber: %d writeNumber: %d", total, readNumber, writeNumber);
    printf("\n***Finished***\n");






	/*
	srand((unsigned)time(0));
	while(1){
		printf("\n\n\n\nEnter firstHalf(1) secondHalf(2):");
		int number = 0;
		scanf("%d", &number);
		if(number == -1)
			break;
		int  i = 0;
		if(number == 1) {
			printf("\n\n\n\nEnter number:");
			int number1 = 0;
			scanf("%d", &number1);
			for (i = 0; i < number1; i++)
			{	 
				int r = rand()%(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)/2;
				count[r]++;
				fprintf(logFile, "%d\n", r);
				
				printf("\nlogical_address: %d\n", r);
			// event_arrive(event_type, logical_address, size, start_time) 
				result = ssd -> event_arrive(WRITE, r, 1, (double) 1800+(300*i));
			//printf("Write time: %.20lf\n", result);
				//i++;
			}

		} else if(number == 2) {
			printf("\n\n\n\nEnter number:");
			int number1 = 0;
			scanf("%d", &number1);
			for (i = 0; i < number1; i++)
			{	
				//srand((unsigned)time(0)); 
				int r = rand()%(SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)/2 + (SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)/2;
				fprintf(logFile, "%d\n", r);
				count[r]++;
			printf("\nlogical_address: %d\n", r);
			// event_arrive(event_type, logical_address, size, start_time) 
				result = ssd -> event_arrive(WRITE, r, 1, (double) 1800+(300*i));
			//printf("Write time: %.20lf\n", result);
				//i++;
			}
		}
		
	
		
	
	//result = ssd -> event_arrive(WRITE, 1, 1, (double) 1800+(300*62));
	//result = ssd -> event_arrive(WRITE, 1, 1, (double) 1800+(300*62));
	//ssd->print_ftl_statistics();
	//ssd->print_statistics();
	//printf("\nprint ftl cost status\n");
	//ssd->print_ftl_cost_status();
	Block_manager::instance()->print_cost_status();

	getchar();
	}*/
	fclose(logFile);

	delete ssd;
	return 0;
}
