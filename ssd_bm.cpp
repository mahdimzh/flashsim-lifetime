/* Copyright 2011 Matias Bjørling */

/* Block Management
 *
 * This class handle allocation of block pools for the FTL
 * algorithms.
 */

#include <new>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <queue>
#include "ssd.h"

using namespace ssd;


Block_manager::Block_manager(FtlParent *ftl) : ftl(ftl)
{
	/*
	 * Configuration of blocks.
	 * User-space is the number of blocks minus the
	 * requirements for map directory.
	 */

	max_blocks = NUMBER_OF_ADDRESSABLE_BLOCKS;
	max_log_blocks = max_blocks;

	if (FTL_IMPLEMENTATION == IMPL_FAST)
		max_log_blocks = FAST_LOG_PAGE_LIMIT;

	// Block-based map lookup simulation
	max_map_pages = MAP_DIRECTORY_SIZE * BLOCK_SIZE;

	directoryCurrentPage = 0;
	num_insert_events = 0;

	data_active = 0;
	log_active = 0;
	// printf("%d\n", SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE);
	for (int i = 0; i < SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE; ++i)
	{
		 block_erase[i] = 0;
	}
	

	current_writing_block = -2;

	out_of_blocks = false;

	active_cost.reserve(NUMBER_OF_ADDRESSABLE_BLOCKS);
}

Block_manager::~Block_manager(void)
{
	return;
}

void Block_manager::cost_insert(Block *b)
{
	active_cost.push_back(b);
}

void Block_manager::instance_initialize(FtlParent *ftl)
{
	Block_manager::inst = new Block_manager(ftl);
}

Block_manager *Block_manager::instance()
{
	return Block_manager::inst;
}

/*
 * Retrieves a page using either simple approach (when not all
 * pages have been written or the complex that retrieves
 * it from a free page list.
 */
void Block_manager::get_page_block(Address &address, Event &event)
{
	// We need separate queues for each plane? communication channel? communication channel is at the per die level at the moment. i.e. each LUN is a die.

	if (simpleCurrentFree < max_blocks*BLOCK_SIZE)
	{
		address.set_linear_address(simpleCurrentFree, BLOCK);
		current_writing_block = simpleCurrentFree;
		simpleCurrentFree += BLOCK_SIZE;
	}
	else
	{
		if (free_list.size() <= 1 && !out_of_blocks)
		{
			out_of_blocks = true;
			insert_events(event);
		}

		assert(free_list.size() != 0);
		address.set_linear_address(free_list.front()->get_physical_address(), BLOCK);
		current_writing_block = free_list.front()->get_physical_address();
		free_list.erase(free_list.begin());
		out_of_blocks = false;
	}
}


Address Block_manager::get_free_block(Event &event)
{
	return get_free_block(DATA, event);
}

/*
 * Handles block manager statistics when changing a
 * block to a data block from a log block or vice versa.
 */
void Block_manager::promote_block(block_type to_type)
{
	if (to_type == DATA)
	{
		data_active++;
		log_active--;
	}
	else if (to_type == LOG)
	{
		log_active++;
		data_active--;
	}
}

/*
 * Returns true if there are no space left for additional log pages.
 */
bool Block_manager::is_log_full()
{
	return log_active == max_log_blocks;
}

void Block_manager::print_statistics()
{
	printf("-----------------\n");
	printf("Block Statistics:\n");
	printf("-----------------\n");
	printf("Log blocks:  %lu\n", log_active);
	printf("Data blocks: %lu\n", data_active);
	printf("Free blocks: %lu\n", (max_blocks - (simpleCurrentFree/BLOCK_SIZE)) + free_list.size());
	printf("Invalid blocks: %lu\n", invalid_list.size());
	printf("Free2 blocks: %lu\n", (unsigned long int)invalid_list.size() + (unsigned long int)log_active + (unsigned long int)data_active - (unsigned long int)free_list.size());
	printf("-----------------\n");


}

void Block_manager::invalidate(Address address, block_type type)
{
	invalid_list.push_back(ftl->get_block_pointer(address));

	switch (type)
	{
	case DATA:
		data_active--;
		break;
	case LOG:
		log_active--;
		break;
	case LOG_SEQ:
		break;
	}
}

/*
 * Insert erase events into the event stream.
 * The strategy is to clean up all invalid pages instantly.
 */
void Block_manager::insert_events(Event &event)
{	//this function is called when controller writes in new block
	// Calculate if GC should be activated.
	float used = (int)invalid_list.size() + (int)log_active + (int)data_active - (int)free_list.size();
	float total = NUMBER_OF_ADDRESSABLE_BLOCKS;
	float ratio = used/total;
	float ratio1 = (total-((max_blocks - (simpleCurrentFree/BLOCK_SIZE)) + free_list.size())) / total;

	float sum = 0;
	float min = block_erase[0];
	float Max = block_erase[0];

	for (int i = 0; i < SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE; ++i)
	{
		//printf("%d  -  ", block_erase[i]);
		sum += block_erase[i];
		if (block_erase[i] > Max)
			Max = block_erase[i];
		if (block_erase[i] < min)
			min = block_erase[i];
		
	} 
	float partialAverage 			= (Max + min) / 2;
	float partialSubtractAverage 	= (Max - min) / 2;
	float totalAverage = sum / (SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE);
	float UpperB = totalAverage + partialSubtractAverage;
	float LowerB = totalAverage - partialSubtractAverage;
	float criticalLowerB = (totalAverage - partialAverage) > 0 ? (totalAverage - partialAverage) : 0;
	printf("\n\npartialAverage: %f partialSubtractAverage: %f (Max: %f - min: %f)totalAverage %f\n", partialAverage,partialSubtractAverage, Max, min, totalAverage);
	printf("UpperB %f LowerB %f, criticalLowerB %f", UpperB, LowerB, criticalLowerB);


	printf("\ninvalid_list: %li, log_active: %li, full block: %li, free_list: %li\n", (int)invalid_list.size(), (int)log_active, (int)data_active, (int)free_list.size());
	printf("used: %f, total: %f, ratio: %f ratio1: %f\n", used, total, ratio, ratio1);
	
	int free_blocks = (max_blocks - (simpleCurrentFree/BLOCK_SIZE)) + free_list.size();
	float check = (float) free_blocks / NUMBER_OF_ADDRESSABLE_BLOCKS;
	uint num_to_erase = PLANE_SIZE * 0.5; 
	int counter = PLANE_SIZE; 

	ActiveByCost::iterator it = active_cost.get<1>().end();
	--it;

	bool erase = false;
	while (*it) {
		erase = false;
		//printf("%d - %d - %d - %d - %d\n", current_writing_block, (*it)->physical_address, (*it)->get_pages_invalid(), (*it)->get_pages_valid(), block_erase[(*it)->get_physical_address()/BLOCK_SIZE]);
		if (current_writing_block != (*it)->physical_address) {
			if(num_to_erase > 0 && (*it)->get_pages_invalid() == BLOCK_SIZE && (*it)->get_pages_valid() == BLOCK_SIZE && block_erase[(*it)->get_physical_address()/BLOCK_SIZE] <= UpperB  && block_erase[(*it)->get_physical_address()/BLOCK_SIZE] >= LowerB) {
				//All pages is invalid and LowerB < block_erase < UpperB 
				erase = true;
				num_to_erase--;
			} else if((*it)->get_pages_valid() > 0 && block_erase[(*it)->get_physical_address()/BLOCK_SIZE] < LowerB) {
				// block_erase < LowerB && block not empty 
				int be = block_erase[(*it)->get_physical_address()/BLOCK_SIZE];
				float m = ((criticalLowerB - LowerB) != 0) ? (0.5 / (LowerB - criticalLowerB)) : 1;
				float partialRatio = m * (be - criticalLowerB);
				partialRatio = partialRatio < 0 ? 0 : partialRatio;
				// partialRatio = validPages / blockSize
				//printf("\n****\npartialRatio: %f be: %d criticalLowerB: %f LowerB: %f m: %f invalid/blockSize: %f \n", partialRatio, be, criticalLowerB, LowerB, m, ((float)((*it)->get_pages_invalid())/BLOCK_SIZE));
				if(((float)((*it)->get_pages_invalid())/BLOCK_SIZE) >= partialRatio) {
					erase = true;
					num_to_erase--;
				}
			} else if(ratio1 >= 0.75) { //full blocks > 75%
				
				float partialRatio = (-1.7 * ratio1) + 2; // invalidPages / blockSize

				float freeBlocks = total - (ratio1 * total);
				//printf("ratio1: %f partialRatio: %f freeBlocks: %f\n",ratio1, partialRatio,freeBlocks);
				int freePages = (freeBlocks * BLOCK_SIZE) - 1;
				if(freePages > 0 && ((float)(*it)->get_pages_invalid()/BLOCK_SIZE) >= partialRatio && (*it)->get_pages_valid() == BLOCK_SIZE) {
					//free pages of block >= partialRatio && and block is full
					erase = true;
					num_to_erase--;
				}

			}
		}
		if(erase) {
			Block *blockErase = (*it);
			// Let the FTL handle cleanup of the block.
			ftl->cleanup_block(event, blockErase);
			// Create erase event and attach to current event queue.
			Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time());
			erase_event.set_address(Address(blockErase->get_physical_address(), BLOCK));
			// Execute erase
			if (ftl->controller.issue(erase_event) == FAILURE) { assert(false);	}
			block_erase[blockErase->get_physical_address()/BLOCK_SIZE]++;
			free_list.push_back(blockErase);
			event.incr_time_taken(erase_event.get_time_taken());
			ftl->controller.stats.numFTLErase++;

			it = active_cost.get<1>().end();
			--it;
			if (current_writing_block == (*it)->physical_address)
				--it;
		} else {
			--it;
		}		
	}
}

Address Block_manager::get_free_block(block_type type, Event &event)
{
	Address address;
	get_page_block(address, event);
	switch (type)
	{
	case DATA:
		ftl->controller.get_block_pointer(address)->set_block_type(DATA);
		data_active++;
		break;
	case LOG:
		if (log_active > max_log_blocks)
			throw std::bad_alloc();

		ftl->controller.get_block_pointer(address)->set_block_type(LOG);
		log_active++;
		break;
	default:
		break;
	}

	return address;
}

/*void Block_manager::print_cost_status()
{

	ActiveByCost::iterator it = active_cost.get<1>().begin();

	for (uint i=0;i<10;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
		printf("%li %i %i\n", (*it)->physical_address, (*it)->get_pages_valid(), (*it)->get_pages_invalid());
		++it;
	}

	printf("end:::\n");

	it = active_cost.get<1>().end();
	--it;

	for (uint i=0;i<10;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
		printf("%li %i %i\n", (*it)->physical_address, (*it)->get_pages_valid(), (*it)->get_pages_invalid());
		--it;
	}
}*/
void Block_manager::print_cost_status()
{

	ActiveByCost::iterator it = active_cost.get<1>().end();
	--it;
	printf("physical_address\tlast_wirte\t#empty_pages\tInvalid_pages\tValid_Pages\tErases_remaining - Erase #\n");
	for (uint i=0;i<SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
		printf("%li\t\t\t%i\t\t%u\t\t\t%i\t\t%u\t\t%li - %d\n", (*it)->physical_address, (*it)->get_pages_valid(), (BLOCK_SIZE - (*it)->get_pages_valid()), (*it)->get_pages_invalid(), (BLOCK_SIZE - (*it)->get_pages_invalid()), (*it)->get_erases_remaining(), block_erase[(*it)->get_physical_address()/BLOCK_SIZE]);
		--it;
	}


	printf("End.\n");

	/*it = active_cost.get<1>().begin();
	for (uint i=0;i<SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
		printf("\n\n%li\n", (*it)->physical_address);
		for (int i = 0; i < BLOCK_SIZE; ++i)
		{
			int page_type;
			switch((*it)->get_state(i))
			{
				case EMPTY: page_type = 0;
				case VALID: page_type = 1;
				case INVALID: page_type = 2;
				default: page_type = -1;
			}
			printf("logical_address %li : %d\n", i, page_type);
		}
		++it;
	}
	*/
	

	//for (uint i=0;i<10;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
	//	printf("%li %i %i\n", (*it)->physical_address, (*it)->get_pages_valid(), (*it)->get_pages_invalid());
	//	--it;
	}
}

void Block_manager::erase_and_invalidate(Event &event, Address &address, block_type btype)
{
	Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time()+event.get_time_taken());
	erase_event.set_address(address);

	if (ftl->controller.issue(erase_event) == FAILURE) { assert(false);}

	free_list.push_back(ftl->get_block_pointer(address));

	switch (btype)
	{
	case DATA:
		data_active--;
		break;
	case LOG:
		log_active--;
		break;
	case LOG_SEQ:
		break;
	}

	event.incr_time_taken(erase_event.get_time_taken());
	ftl->controller.stats.numFTLErase++;
}

int Block_manager::get_num_free_blocks()
{
	if (simpleCurrentFree < max_blocks*BLOCK_SIZE)
		return (simpleCurrentFree / BLOCK_SIZE) + free_list.size();
	else
		return free_list.size();
}

void Block_manager::update_block(Block * b)
{
	std::size_t pos = (b->physical_address / BLOCK_SIZE);
	active_cost.replace(active_cost.begin()+pos, b);
}
