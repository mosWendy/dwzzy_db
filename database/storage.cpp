/*
 * storage.cpp
 *
 *  Created on: Oct 15, 2017
 *      Author: wcw
 */
#include "storage.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

BufferTableItem ::BufferTableItem(int index):   index(index), virtual_addr(-1), in_buffer(false), is_written(false), U(false), M(false) {}

StorageManagement::StorageManagement() {
	this->last_used = 0;
	this->fd = 0;
	this->InitBufferTable();
	this->segment_table = SegmentTable();
	this->frame_table = FrameTable();
}

StorageManagement::~StorageManagement() {

}
/*
 * initial storage, pass file_path
 */
void StorageManagement::InitStorage(char *path){
	this->fd = open(path,O_RDWR);
}

void StorageManagement::InitBufferTable() {
	for (int i = 0; i < BUFFER_SIZE; i ++) {
		this->buffer_table[i] = BufferTableItem(i);
	}
}

int StorageManagement::HitBuffer(ADDR virtual_addr) {
	int index = -1;
	int i = 0;
	while (i < BUFFER_SIZE && buffer_table[i].in_buffer == true) {
		if (buffer_table[i].virtual_addr == virtual_addr) {
			return i;
		}
		i++;
	}
	return index;
}

BufferTableItem* StorageManagement::NRU(BufferTableItem* buffer_table) {
	// 如果有空闲空间，返回第一个空闲空间位置
	for (int i = 0; i < BUFFER_SIZE; i ++) {
		if (buffer_table[i].in_buffer == false) {
			this->last_used = i;
			return &buffer_table[i];
		}
	}
	int i = this->last_used + 1;
	while (true) {
		int index = i % BUFFER_SIZE;
		// 第一轮扫描
		while (index <= last_used) {
			if (buffer_table[index].M == false && buffer_table[index].U == false) {
				// 既未被访问过，也未被修改过
				this->last_used = index;
				return &buffer_table[index];
			}
			i++;
			index = i % BUFFER_SIZE;
		}
		// 第二轮扫描
		while (index <= last_used) {
			if ( buffer_table[index].U == false && buffer_table[index].M == true) {
				// 被修改过未被访问过
				if (buffer_table[index].is_written == true) {
					// 换出的块需要写回硬盘
					// WriteFile将原位置变成空闲空间，在文件后面追加？
					this->WriteFile(buffer_table[index].virtual_addr, buffer[index].frame_content, PAGE_SIZE);
				}
				buffer_table[index].in_buffer = false;
				this->last_used = index;
				return &buffer_table[index];
			}
			buffer_table[index].U = false;
			i++;
			index = i % BUFFER_SIZE;
		}
		// 重复一二轮扫描
	}
}

int StorageManagement::ReadBuffer(ADDR virtual_addr, void *buf, unsigned int length) {
	int index = this->HitBuffer(virtual_addr);
	BufferTableItem* item = NULL;
	if (index < 0) {
		// 未命中
		item = this->NRU(this->buffer_table);
		unsigned int swap_index = item->index;
		// 将要读的内容写入缓冲区
		// length实际上就是PAGE_SIZE，LoadPage需要根据virtual_addr找到seg_id, page_id和offset，判断是否valid
		// 在文件中读入offset偏移量后的length长度的内容
		int ret = this->LoadPage(virtual_addr, buffer[swap_index].frame_content);
		if (ret < 0) {
			cout << "Content does not exist in file" << endl;
			return ret;
		}
		// 更改BufferTable表项
		item->virtual_addr = virtual_addr;
		item->in_buffer = true;
		item->U = true;
		item->M = true;
		return this->ReadBuffer(virtual_addr, buf, length);
	}
	else {
		//命中
		memcpy(buf, buffer[index].frame_content, length);
		this->buffer_table[index].U = true;
		return length;
	}
}

int StorageManagement::WriteBuffer(ADDR virtual_addr, const void *buf, unsigned int length) {
	int index = this->HitBuffer(virtual_addr);
	BufferTableItem* item = NULL;
	if (index < 0) {
		// 未命中，更改要换出的BufferTable表项
		item = this->NRU(this->buffer_table);
		item->virtual_addr = virtual_addr;
		item->in_buffer = true;
		return this->WriteBuffer(virtual_addr, buf, length);
	}
	else {
		// 命中，修改缓冲区
		memcpy(buffer[index].frame_content, buf, length);
		// 更改BufferTable表项
		this->buffer_table[index].is_written = true;
		this->buffer_table[index].in_buffer = true;
		this->buffer_table[index].U = true;
		this->buffer_table[index].M = true;
		return length;
	}
}

void StorageManagement::FlushBuffer() {
	for (int i = 0; i < BUFFER_SIZE; i++) {
		if (this->buffer_table[i].is_written == true) {
			this->WriteFile(buffer_table[i].virtual_addr, buffer[i].frame_content, PAGE_SIZE);
			this->buffer_table[i] = BufferTableItem(i);
		}
	}
}

ADDR StorageManagement::GetSegId(ADDR virtual_addr){
	ADDR seg_id;
	seg_id = (virtual_addr >> (PAGE_BIT + OFFSET_BIT)) & SEG_MASK;
	return seg_id;
}
ADDR StorageManagement::GetPageId(ADDR virtual_addr){
	ADDR page_id;
	page_id = (virtual_addr >> OFFSET_BIT) & PAGE_MASK;
	return page_id;
}
ADDR StorageManagement::GetOffset(ADDR virtual_addr){
	ADDR offset;
	offset = virtual_addr & OFFSET_MASK;
	return offset;
}

void* Frame::GetFrameContent(int fd){
	off_t offset = this->frame_id * PAGE_SIZE;
	if (this->is_valid == false){
		printf("ERROR: Invalid Frame! %u\n ",this->frame_id);
		return NULL;
	}
	if(lseek(fd,offset,SEEK_SET) != -1){
			read(fd,this->frame_content,PAGE_SIZE);
			return this->frame_content;
	}
	return NULL; //lseek failed.
}

ADDR FrameTable::AllocFrame(){
	unsigned int count = this->count;
	Frame new_frame = Frame(count,true);
	this->frame_table[count] = new_frame;
	count++;
	return new_frame.frame_id;
}

int Frame::FlushFrame(const void *buf, int fd){
	off_t offset = this->frame_id * PAGE_SIZE;
	if(lseek(fd,offset,SEEK_SET) != -1){
		write(fd,buf,PAGE_SIZE);
		return 0;
	}
	return -1;
}
ADDR Segment::AllocPage(){
	unsigned int count = this->count;
	Page new_page = Page(count,true);
	this->page_table[count] = new_page;
	count++;
	return new_page.page_id;
}
/*
 *1. Get seg_id, look up segment table to get start address of the segment
 *2. Get page_id
 *2.
 */
int StorageManagement::LoadPage(ADDR virtual_addr, void *buf){
	ADDR seg_id = this->GetSegId(virtual_addr);
	ADDR start_id = this->segment_table.seg_table[seg_id].start;
	map<ADDR,ADDR> addr_map = this->segment_table.seg_table[seg_id].addr_map;
	ADDR page_id = this->GetPageId(virtual_addr);
	/*
	 * look up map to get frame_id
	 */
	map<ADDR,ADDR>::iterator iter;
	iter = addr_map.find(page_id);
	if(iter != addr_map.end())
	{
		Frame frame = this->frame_table.frame_table[iter->second];
		if((buf = frame.GetFrameContent(this->fd)) != NULL)
			return 0;
		else
			return -1;
	}
	printf("ERROR:cannot find page %u in map!",page_id);
	return -1;

	/*
	off_t offset = (start_id + page_id) * PAGE_SIZE;
	int fd = this->fd;
	if(fd == NULL){
		printf("ERROR, file not exists");
		return -1;
	}
	if(lseek(fd,offset,SEEK_SET) != -1){
		read(fd,buf,PAGE_SIZE);
		return 0;
	}
	else{
		printf("ERROR, page not exists");
		return -1;
	}*/
}

/*
 * write *buf to file, size of buf no more than one page
 */
int StorageManagement::WriteOnePage(ADDR virtual_addr,const void *buf, unsigned int length){
	if(length > PAGE_SIZE){
		printf("ERROR: Write more than one page!");
		return -1;
	}
	ADDR seg_id = this->GetSegId(virtual_addr);
	ADDR start_id = this->segment_table.seg_table[seg_id].start;
	map<ADDR,ADDR> addr_map = this->segment_table.seg_table[seg_id].addr_map;
	ADDR page_id = this->GetPageId(virtual_addr);

	map<ADDR,ADDR>::iterator iter;
	iter = addr_map.find(page_id);
	/*
	 *the address already have been mapped,
	 *which means the page has contents
	 *invalid old page, and assign new pages
	 */
	if(iter == addr_map.end()){
		Frame frame = this->frame_table.frame_table[iter->second];
		frame.is_valid = false;
		Page page = this->segment_table.seg_table[seg_id].page_table[page_id];
		page.is_valid = false;
	}
	/*
	 * allocate new page
	 */
	ADDR new_page = this->segment_table.seg_table[seg_id].AllocPage();
	ADDR new_frame = this->frame_table.AllocFrame();
	addr_map.insert(map<ADDR,ADDR>::value_type (new_page,new_frame));
	int ret = this->frame_table.frame_table[new_frame].FlushFrame(buf,this->fd);
	return ret;
}

int main()
{
	return 1;
}




