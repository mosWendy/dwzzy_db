/*
 * storage.h
 *
 *  Created on: Oct 15, 2017
 *      Author: wcw
 */

#include<iostream>
#include<cstring>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include<stack>
#include <unistd.h>
#include <sys/types.h>
#include <map>

using namespace std;


#define BUFFER_SIZE		16
#define PAGE_SIZE		1024 * 4
#define MAX_SEG_SIZE	256 * 1024 * 4
/*
 * address space 8M
 * per tuple 512B
 * MAX_TUPLE_SIZE 512 Bytes
 * seg_id | page_id | offset
 * store 4GB data
 */
#define MAX_TUPLE_SIZE 512
#define SEG_BIT 4
#define PAGE_BIT 16
#define OFFSET_BIT 12
#define OFFSET_MASK 0XFFF
#define PAGE_MASK	0XFFFF
#define	SEG_MASK	0XF

# define ADDR unsigned long

class Frame {
public:
	/*
	 * relate to offset in file
	 */
	ADDR frame_id;
	bool is_valid;
	/*
	 * when load or write page, store content.
	 */
	unsigned char  frame_content[PAGE_SIZE];

	Frame(ADDR id, bool is_valid)
	{
		this->is_valid = is_valid;
		this->frame_id = id;
		this->frame_content = NULL; //? pointer
	}
	/*
	 * get one frame with content,
	 * a.k.a. read from file fd according to frame_id
	 */
	void* GetFrameContent(int fd);
	/*
	 * flush PAGE_SIZE to file fd
	 */
	int FlushFrame(const void *buf, int fd);
};

class FrameTable{
public:
	Frame *frame_table;
	Frame *free_frame_list;
	/*
	 * count frames
	 */
	unsigned int count;
	FrameTable(){
		this->frame_table = NULL;
		this->free_frame_list  = NULL;
		this->count = 0;
	}
	/*
	 * assign a free frame, add it to frame_table
	 * return a free frame_id
	 */
	ADDR AllocFrame();
};

class Page {
public:
	ADDR page_id;
	bool is_valid;
	unsigned int free_tuples;//bit map
	//Frame frame;
	Page(){
		this->page_id = 0;
		this->is_valid = true;
		this->free_tuples = 0;
	}
	Page(ADDR page_id,bool is_valid){
		this->page_id = page_id;
		this->is_valid = is_valid;
		this->free_tuples = 0;
	}
};

class Segment {
public:
	ADDR seg_id;
	ADDR start; //start from which frame id
	map<ADDR,ADDR> addr_map; //<page_id,frame_id>
	Page *page_table;
	unsigned int count;
	stack<unsigned int> free_pages;

	Segment(){
		this->seg_id = 0;
		this->start = 0;
		this->count = 0;
		this->page_table = NULL;
	}
	Segment(ADDR seg_id, ADDR start, unsigned int count){
		this->count = count;
		this->seg_id = seg_id;
		this->start = start;
		this->page_table = NULL;

	}
	/*
	 * assign caller a new free page;
	 * call AllocFrame() to get a free frame_id;
	 * insert map<page_id, frame_id>;
	 * return page_id;
	 */
	ADDR AllocPage();

	int SetFreePage(unsigned int page_id);
	unsigned int GetFreePage();
};

/*
 * organize segments array;
 */
class SegmentTable {
public:
	Segment *seg_table;
	unsigned int size;
	SegmentTable(){
		this->size = 1;
		this->seg_table = Segment();
	}
	SegmentTable(unsigned int size, Segment *seg_table) //
	{
		this->size = size;
		this->seg_table = seg_table;
	}
	/*
	 * add one seg
	 */
};

class BufferTableItem {
public:
	int index; //在Buffer_table中的index
	ADDR virtual_addr;
	bool in_buffer;
	// 换出时是否需要写回硬盘
	bool is_written;
	// NRU标志位
	bool U;
	bool M;
	BufferTableItem();
	BufferTableItem(int index);
};

class StorageManagement {
private:
	int last_used; //置换策略中上一次访问的位置

	void InitBufferTable();
	int HitBuffer(ADDR virtual_addr);
	BufferTableItem* NRU(BufferTableItem* buffer_table);

/*
 * parse address
 */
	ADDR GetPageId(ADDR virtual_addr);
	ADDR GetSegId(ADDR virtual_addr);
	ADDR GetOffset(ADDR virtual_addr);

	int LoadPage(ADDR virtual_addr, void *buf); //read a page into buf

	Page* AllocPage();

	int Write(const void *buf, unsigned int length);

	int WriteFile(ADDR virtual_addr, const void *buf, unsigned int length);//virtual_addr <- alloc
	int WriteOnePage(ADDR virtual_addr,const void *buf, unsigned int length);
public:
	int fd; //for file operations
	BufferTableItem  buffer_table[BUFFER_SIZE];
	Frame buffer[BUFFER_SIZE];
	SegmentTable segment_table; //user define the number of segs, 1 seg by default
	FrameTable frame_table;

	StorageManagement();
	~StorageManagement();

	void InitStorage(char *path);
	int ReadBuffer(ADDR virtual_addr, void *buf, unsigned int length);
	int WriteBuffer(ADDR virtual_addr, const void *buf, unsigned int length);
	void FlushBuffer();
};
