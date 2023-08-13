/*
 * Buddy Page Allocation Algorithm
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1894401
 */
#include <infos/mm/page-allocator.h>
#include <infos/mm/mm.h>
#include <infos/kernel/kernel.h>
#include <infos/kernel/log.h>
#include <infos/util/math.h>
#include <infos/util/printf.h>

using namespace infos::kernel;
using namespace infos::mm;
using namespace infos::util;

#define MAX_ORDER	17

/**
 * A buddy page allocation algorithm.
 */
class BuddyPageAllocator : public PageAllocatorAlgorithm
{
private:
	
	/**
	 * Returns the number of pages that comprise a 'block', in a given order.
	 * @param order The order to base the calculation off of.
	 * @return Returns the number of pages in a block, in the order.
	 */
	static inline constexpr uint64_t pages_per_block(int order)
	{
		/* The number of pages per block in a given order is simply 1, shifted left by the order number.
		 * For example, in order-2, there are (1 << 2) == 4 pages in each block.
		 */
		return (1 << order);
	}
	
	/**
	 * Returns TRUE if the supplied page descriptor is correctly aligned for the 
	 * given order.  Returns FALSE otherwise.
	 * @param pgd The page descriptor to test alignment for.
	 * @param order The order to use for calculations.
	 */
	static inline bool is_correct_alignment_for_order(const PageDescriptor *pgd, int order)
	{
		// Calculate the page-frame-number for the page descriptor, and return TRUE if
		// it divides evenly into the number pages in a block of the given order.
		return (sys.mm().pgalloc().pgd_to_pfn(pgd) % pages_per_block(order)) == 0;
	}
	
	/** Given a page descriptor, and an order, returns the buddy PGD.  The buddy could either be
	 * to the left or the right of PGD, in the given order.
	 * @param pgd The page descriptor to find the buddy for.
	 * @param order The order in which the page descriptor lives.
	 * @return Returns the buddy of the given page descriptor, in the given order.
	 */
	PageDescriptor *buddy_of(PageDescriptor *pgd, int order)
	{
		// (1) Make sure 'order' is within range
		if (order >= MAX_ORDER) {
			return nullptr;
		}

		// (2) Check to make sure that PGD is correctly aligned in the order
		if (!is_correct_alignment_for_order(pgd, order)) {
			return nullptr;
		}
				
		// (3) Calculate the page-frame-number of the buddy of this page.
		// * If the PFN is aligned to the next order, then the buddy is the next block in THIS order.
		// * If it's not aligned, then the buddy must be the previous block in THIS order.
		uint64_t buddy_pfn = is_correct_alignment_for_order(pgd, order + 1) ?
			sys.mm().pgalloc().pgd_to_pfn(pgd) + pages_per_block(order) : 
			sys.mm().pgalloc().pgd_to_pfn(pgd) - pages_per_block(order);
		
		// (4) Return the page descriptor associated with the buddy page-frame-number.
		return sys.mm().pgalloc().pfn_to_pgd(buddy_pfn);
	}
	
	/**
	 * Inserts a block into the free list of the given order.  The block is inserted in ascending order.
	 * @param pgd The page descriptor of the block to insert.
	 * @param order The order in which to insert the block.
	 * @return Returns the slot (i.e. a pointer to the pointer that points to the block) that the block
	 * was inserted into.
	 */
	PageDescriptor **insert_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, find the slot in which the page descriptor
		// should be inserted.
		PageDescriptor **slot = &_free_areas[order];
		
		// Iterate whilst there is a slot, and whilst the page descriptor pointer is numerically
		// greater than what the slot is pointing to.
		while (*slot && pgd > *slot) {
			slot = &(*slot)->next_free;
		}
		
		// Insert the page descriptor into the linked list.
		pgd->next_free = *slot;
		*slot = pgd;
		
		// Return the insert point (i.e. slot)
		return slot;
	}
	
	/**
	 * Removes a block from the free list of the given order.  The block MUST be present in the free-list, otherwise
	 * the system will panic.
	 * @param pgd The page descriptor of the block to remove.
	 * @param order The order in which to remove the block from.
	 */
	void remove_block(PageDescriptor *pgd, int order)
	{
		// Starting from the _free_area array, iterate until the block has been located in the linked-list.
		PageDescriptor **slot = &_free_areas[order];
		while (*slot && pgd != *slot) {
			slot = &(*slot)->next_free;
		}

		// Make sure the block actually exists.  Panic the system if it does not.
		assert(*slot == pgd);
		
		// Remove the block from the free list.
		*slot = pgd->next_free;
		pgd->next_free = nullptr;
	}
	
	/**
	 * Given a pointer to a block of free memory in the order "source_order", this function will
	 * split the block in half, and insert it into the order below.
	 * @param block_pointer A pointer to a pointer containing the beginning of a block of free memory.
	 * @param source_order The order in which the block of free memory exists.  Naturally,
	 * the split will insert the two new blocks into the order below.
	 * @return Returns the left-hand-side of the new block.
	 */
	PageDescriptor *split_block(PageDescriptor **block_pointer, int source_order)
	{
		// Make sure there is an incoming pointer.
		assert(*block_pointer);
		
		// Make sure the block_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		int target_order = source_order-1;
		uint64_t nr_ppb = pages_per_block(target_order);
		
		// Get the base address of left-hand-side sub-block in source order
		PageDescriptor *pgd_left_block = (*block_pointer);
		// Get the base address of right-hand-side sub-block in source order
		PageDescriptor *pgd_right_block = (*block_pointer) + nr_ppb;
		
		// Remove the block from the free list of its source order
		remove_block(*block_pointer, source_order);
		
		// Insert the left- and right-hand-side sub-blocks into the order below
		PageDescriptor **left_slot = insert_block(pgd_left_block, target_order);
		PageDescriptor **right_slot = insert_block(pgd_right_block, target_order);
		
		// Return the left-hand-side of new block in the order below
		return (*left_slot);		
	}
	
	/**
	 * Takes a block in the given source order, and merges it (and its buddy) into the next order.
	 * This function assumes both the source block and the buddy block are in the free list for the
	 * source order.  If they aren't this function will panic the system.
	 * @param block_pointer A pointer to a pointer containing a block in the pair to merge.
	 * @param source_order The order in which the pair of blocks live.
	 * @return Returns the new slot that points to the merged block.
	 */
	PageDescriptor **merge_block(PageDescriptor **block_pointer, int source_order)
	{
		assert(*block_pointer);
		
		// Make sure the area_pointer is correctly aligned.
		assert(is_correct_alignment_for_order(*block_pointer, source_order));
		
		int target_order = source_order+1;
		
		// Get the buddy of the given source block
		PageDescriptor *buddy = buddy_of(*block_pointer, source_order);
		
		// Get the base address of the source block and its buddy
		PageDescriptor *base_addr = (*block_pointer < buddy) ? *block_pointer : buddy;
		
		// Remove the source block and its buddy from the free list of their source order
		remove_block(*block_pointer, source_order);
		remove_block(buddy, source_order);
		
		// Insert the source block and its buddy into the order above
		PageDescriptor **slot = insert_block(base_addr, target_order);
		
		return slot;		
	}
	
public:
	/**
	 * Constructs a new instance of the Buddy Page Allocator.
	 */
	BuddyPageAllocator() {
		// Iterate over each free area, and clear it.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			_free_areas[i] = nullptr;
		}
		syslog.messagef(LogLevel::DEBUG, "Constructor has been called");
	}
	
	/**
	 * Allocates 2^order number of contiguous pages
	 * @param order The power of two, of the number of contiguous pages to allocate.
	 * @return Returns a pointer to the first page descriptor for the newly allocated page range, or NULL if
	 * allocation failed.
	 */
	PageDescriptor *alloc_pages(int order) override
	{
		PageDescriptor **block_pointer;
		PageDescriptor *left_slot = nullptr;
		
		int i = ARRAY_SIZE(_free_areas) - 1;
		block_pointer = &_free_areas[i];
		while (i > order) {
			// Make sure there is an incoming pointer
			assert(*block_pointer);
			// Get the left-hand-side of the new block in the order below
			left_slot = split_block(block_pointer, i);
			block_pointer = &left_slot;
			i--;
		}
		// By now the desired page descriptor is in the free list of the given source order
		if (*block_pointer) {
			remove_block(*block_pointer, order);
			return *block_pointer;
		}
		return nullptr;
	}
	
	/**
	 * Frees 2^order contiguous pages.
	 * @param pgd A pointer to an array of page descriptors to be freed.
	 * @param order The power of two number of contiguous pages to free.
	 */
	void free_pages(PageDescriptor *pgd, int order) override
	{
		// Make sure that the incoming page descriptor is correctly aligned
		// for the order on which it is being freed, for example, it is
		// illegal to free page 1 in order-1.
		assert(is_correct_alignment_for_order(pgd, order));
		
		// Insert page into the free list of the source order
		insert_block(pgd, order);
		
		// Now, merge blocks and their buddies from the source order
		// all the way to the maximum order
		int i = order;
		PageDescriptor **block_pointer = &pgd;
		PageDescriptor **buddy;
		
		while (i < (MAX_ORDER-1)) {
			// Get the buddy of the block
			PageDescriptor *ptr = buddy_of(*block_pointer, i);
			buddy = &ptr;
			// Make sure the block and its buddy are in the free list of the given order
			bool both_are_free = ((*block_pointer)->next_free == *buddy) || ((*buddy)->next_free == *block_pointer);
			if (*block_pointer && *buddy && both_are_free) {
				block_pointer = merge_block(block_pointer, i);
			}
			else {
				break;
			}
			i++;	
		}
	}
	
	/*
	Used just for order zero. Goes through each block in the free list
	and checks if its address matches that of the given pgd.
	Returns a pointer to the pgd if it's found or a null pointer if it's absent.
	*/
	PageDescriptor** is_page_free(PageDescriptor* pgd, int order)
	{
		PageDescriptor **slot = &_free_areas[order];
		while (*slot != nullptr) {
			if (*slot == pgd) {
				return slot;
			}
			slot = &(*slot)->next_free;
		}
		return nullptr;
	}
	
	/*
	Goes through a block in a given order and verifies whether or not
	the given page is within the block.
	*/
	bool does_block_contain_page(PageDescriptor* block, int order, PageDescriptor* pgd) {
		uint64_t size = pages_per_block(order);
		PageDescriptor* last_page = block + size;
		return (pgd >= block) && (pgd < last_page);
	}

	/**
	 * Reserves a specific page, so that it cannot be allocated.
	 * @param pgd The page descriptor of the page to reserve.
	 * @return Returns TRUE if the reservation was successful, FALSE otherwise.
	 */
	bool reserve_page(PageDescriptor *pgd)
	{
		int order = MAX_ORDER-1;
		PageDescriptor* current_block = nullptr;

		// For each order, starting from largest
		while (order >= 0)
		{
			// If the order is 0, and we have found the block, search through the block
			if (order == 0 && current_block)
			{
				PageDescriptor **slot = is_page_free(pgd, 0);
				if (slot == nullptr) {
					return false;
				}
				// The page at the slot should be equal to the pgd we're finding
				assert(*slot == pgd);

				remove_block(*slot, 0);
				return true;
			}
			// If the block containing the page has been found...
			if (current_block != nullptr) {
				PageDescriptor *left_block = split_block(&current_block, order);
				int i = order-1;
				// If the left-hand-side block contains the page...
				if (does_block_contain_page(left_block, i, pgd)) {
					// update the current block
					current_block = left_block;
				} else {
					PageDescriptor *right_block = buddy_of(left_block, i);
					// The right-hand-side must contain the block
					assert(does_block_contain_page(right_block, i, pgd));
					current_block = right_block;
				}
				// Split further down, on the next loop.
				order = i;
				continue;
			}
			
			// Search through the free areas, starting off with the first free list of this order
			current_block = _free_areas[order];
			while (current_block != nullptr) {
				// If pgd is between (inclusive) the current block and the last page of that block
				if (does_block_contain_page(current_block, order, pgd)) {
					// then we've discovered the block
					break;
				}
				current_block = current_block->next_free;
			}
			// Search lower down if the block was not found.
			if (current_block == nullptr) {
				order--;
			}
		}
		return false;
	}
	
	/**
	 * Initialises the allocation algorithm.
	 * @return Returns TRUE if the algorithm was successfully initialised, FALSE otherwise.
	 */
	bool init(PageDescriptor *page_descriptors, uint64_t nr_page_descriptors) override
	{
		mm_log.messagef(LogLevel::DEBUG, "Buddy Allocator Initialising pd=%p, nr=0x%lx", page_descriptors, nr_page_descriptors);
				
		// Get the number of pages in the maximum order block
		uint64_t block_size = pages_per_block(MAX_ORDER-1);
		uint64_t nr_blocks;
		
		// The number of pages should be a multiple of the number of pages per block
		if ((nr_page_descriptors % block_size) == 0) {
			nr_blocks = nr_page_descriptors >> (MAX_ORDER-1);
		}
		else {
			nr_blocks = (nr_page_descriptors >> (MAX_ORDER-1)) + 1;
		}
		
		for (uint64_t i = 0; i < nr_blocks; i++) {
			insert_block(page_descriptors, MAX_ORDER-1);
			page_descriptors += block_size;
		}
		return true;
	}

	/**
	 * Returns the friendly name of the allocation algorithm, for debugging and selection purposes.
	 */
	const char* name() const override { return "buddy"; }
	
	/**
	 * Dumps out the current state of the buddy system
	 */
	void dump_state() const override
	{
		// Print out a header, so we can find the output in the logs.
		mm_log.messagef(LogLevel::DEBUG, "BUDDY STATE:");
		
		// Iterate over each free area.
		for (unsigned int i = 0; i < ARRAY_SIZE(_free_areas); i++) {
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "[%d] ", i);
						
			// Iterate over each block in the free area.
			PageDescriptor *pg = _free_areas[i];
			while (pg) {
				// Append the PFN of the free block to the output buffer.
				snprintf(buffer, sizeof(buffer), "%s%lx ", buffer, sys.mm().pgalloc().pgd_to_pfn(pg));
				pg = pg->next_free;
			}
			
			mm_log.messagef(LogLevel::DEBUG, "%s", buffer);
		}
	}

private:
	PageDescriptor *_free_areas[MAX_ORDER];
};

/* --- DO NOT CHANGE ANYTHING BELOW THIS LINE --- */

/*
 * Allocation algorithm registration framework
 */
RegisterPageAllocator(BuddyPageAllocator);
