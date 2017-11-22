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
	printf("\n\n***\n\ninvalid_list: %li, log_active: %li, full block: %li, free_list: %li\n", (int)invalid_list.size(), (int)log_active, (int)data_active, (int)free_list.size());
	printf("used: %f, total: %f, ratio: %f\n****\n", used, total, ratio);
	
	int free_blocks = (max_blocks - (simpleCurrentFree/BLOCK_SIZE)) + free_list.size();
	float check = (float) free_blocks / NUMBER_OF_ADDRESSABLE_BLOCKS;
	uint num_to_erase = 5; 

	if(ratio >= 0.75) {
		if (FTL_IMPLEMENTATION == IMPL_DFTL || FTL_IMPLEMENTATION == IMPL_BIMODAL)
		{
			ActiveByCost::iterator it = active_cost.get<1>().end();
			--it;

			//while ((*it)->get_pages_invalid() > 0 && (*it)->get_pages_valid() == BLOCK_SIZE)
			while(((float)(*it)->get_pages_invalid()/max_blocks) >= 0.75 && (total - used) <= 1) {
				if (current_writing_block != (*it)->physical_address)
				{
					//printf("erase p: %p phy: %li ratio: %i num: %i\n", (*it), (*it)->physical_address, (*it)->get_pages_invalid(), num_to_erase);
					Block *blockErase = (*it);

					// Let the FTL handle cleanup of the block.
					ftl->cleanup_block(event, blockErase);

					// Create erase event and attach to current event queue.
					Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time());
					erase_event.set_address(Address(blockErase->get_physical_address(), BLOCK));

					// Execute erase
					if (ftl->controller.issue(erase_event) == FAILURE) { assert(false);	}
	
					free_list.push_back(blockErase);
	
					event.incr_time_taken(erase_event.get_time_taken());

					ftl->controller.stats.numFTLErase++;
				}

				it = active_cost.get<1>().end();
				--it;

				if (current_writing_block == (*it)->physical_address)
			 		--it;

				//num_to_erase--;
				print_cost_status();
			}
		}
	}


	/*if(ratio >= 1.75) {
		//printf("ration: %f\n", (float)(*it)->get_pages_invalid()/max_blocks);
		//printf("%f\n", (total - used));
		while(((float)(*it)->get_pages_invalid()/max_blocks) >= 0.75 && (total - used) <= 1){
			if (current_writing_block != (*it)->physical_address){
			Block *blockErase = (*it);
			for (uint i = 0; i < BLOCK_SIZE; i++) {
				if ((*it)->get_state(i) == VALID) {//move valid pages
					
					//block address: (*it)->get_physical_address() page adresss: i
					printf("page readed: %li\n", (*it)->get_physical_address() + i);
					Event readEvent = Event(READ, event.get_logical_address(), 1, event.get_start_time()); 
					readEvent.set_address(Address((*it)->get_physical_address() + i, PAGE));
					if (ftl->controller.issue(readEvent) == FAILURE) printf("Data block copy failed.");
					//copied

					// Get new address to write to and invalidate previous
					long free_page = get_free_block(DATA, event).get_linear_address();
					printf("page to copy: %d\n", free_page);
					Event writeEvent = Event(WRITE, (*it)->get_physical_address()+i, 1, event.get_start_time()+readEvent.get_time_taken());
					//Address dataBlockAddress = Address(free_page, PAGE);
					//writeEvent.set_address(dataBlockAddress);
					//writeEvent.set_replace_address(Address((*it)->get_physical_address()+i, PAGE));
					//writeEvent.set_payload((char*)page_data + ((*it)->get_physical_address()+i) * PAGE_SIZE);
					//print_cost_status();
					//Controller::instance()->event_arrive(*writeEvent);
					if (ftl->controller.event_arrive(writeEvent) == FAILURE) printf("Data block copy failed.");
					//pasted
					//long dataPpn = dataBlockAddress.get_linear_address();
					//invalidated_translation[reverse_trans_map[(*it)->get_physical_address()+i]] = dataPpn;
					Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time() + writeEvent.get_start_time()+readEvent.get_time_taken());
					erase_event.set_address(Address((*it)->get_physical_address(), BLOCK));
					if (ftl->controller.issue(erase_event) == FAILURE) {	assert(false);}// Execute erase
					event.incr_time_taken(erase_event.get_time_taken());
					ftl->controller.stats.numFTLErase++;
					free_list.push_back(blockErase);
					//cleaned
				}
			}
		} 
			//it = active_cost.get<1>().end();
			--it;

			if (current_writing_block == (*it)->physical_address)
				--it;
			//ftl->controller.print_ftl_cost_status();
			print_cost_status();

		}
		
		//exit(1);
	}*/

	/*if(check < 0.75) {
		printf("%f\n", check);
		Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time());
		erase_event.set_address(Address(invalid_list.back()->get_physical_address(), BLOCK));
		if (ftl->controller.issue(erase_event) == FAILURE) {	assert(false);}// Execute erase
		event.incr_time_taken(erase_event.get_time_taken());

		free_list.push_back(invalid_list.back());
		return;
	}*/


	/*if (ratio < 1.90) // Magic number
		return;

	 // More Magic! #of block to erase

	//printf("%i %i %i\n", invalid_list.size(), log_active, data_active);

	// First step and least expensive is to go though invalid list. (Only used by FAST)
	while (num_to_erase != 0 && invalid_list.size() != 0)
	{
		Event erase_event = Event(ERASE, event.get_logical_address(), 1, event.get_start_time());
		erase_event.set_address(Address(invalid_list.back()->get_physical_address(), BLOCK));
		if (ftl->controller.issue(erase_event) == FAILURE) {	assert(false);}// Execute erase
		event.incr_time_taken(erase_event.get_time_taken());

		free_list.push_back(invalid_list.back());
		invalid_list.pop_back();

		num_to_erase--;
		ftl->controller.stats.numFTLErase++;
	}

	num_insert_events++;*/

	
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
	printf("physical_address\tlast_wirte\t#empty_pages\tInvalid_pages\tValid_Pages\tErases_remaining\n");
	for (uint i=0;i<SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
		printf("%li\t\t\t%i\t\t%u\t\t\t%i\t\t%u\t\t%li\n", (*it)->physical_address, (*it)->get_pages_valid(), (BLOCK_SIZE - (*it)->get_pages_valid()), (*it)->get_pages_invalid(), (BLOCK_SIZE - (*it)->get_pages_invalid()), (*it)->get_erases_remaining());
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
	

	for (uint i=0;i<10;i++) //SSD_SIZE*PACKAGE_SIZE*DIE_SIZE*PLANE_SIZE
	{
	//	printf("%li %i %i\n", (*it)->physical_address, (*it)->get_pages_valid(), (*it)->get_pages_invalid());
		--it;
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
