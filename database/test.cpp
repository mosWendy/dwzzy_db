/*
 * test.cpp
 *
 *  Created on: Oct 23, 2017
 *      Author: chelsea
 */
#include "storage.h"
#include <iostream>
#include <fstream>
int main() {
	StorageManagement *storage_manager = new StorageManagement();
	char* name="tmp.txt";
	int ret = storage_manager->InitStorage(name);
	if (ret == -1) return ret;
	fstream out;
	char buffer[MAX_TUPLE_SIZE+1];
	out.open("test.txt",ios::in);

	while(!out.eof())
	{
	   out.getline(buffer,MAX_TUPLE_SIZE,'\n');
	   for (int i = strlen(buffer); i < MAX_TUPLE_SIZE; i++)
		   buffer[i] = '*';
	   buffer[MAX_TUPLE_SIZE] = '\0';
	   storage_manager->Write(buffer, MAX_TUPLE_SIZE);
	}
	out.close();
	char* buf;
	storage_manager->ReadBuffer(8120, buf, PAGE_SIZE);
	cout<<(char*)buf<<endl;
	delete storage_manager;
	cout<<"end"<<endl;
	return 0;
}


