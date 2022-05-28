#include <common.h>
#include <kmt.h>
#include <fat32.h>

int fill_task_ofile(ofile_t* ofile){
	task_t* cur_task = kmt->gettask();
	for(int i = STDERR_FILENO + 1; i < MAX_OPEN_FILE; i++){
		if(!cur_task->ofiles[i]){
			cur_task->ofiles[i] = ofile;
			return i;
		}
	}
	Assert(0, "number of opening files is more than %d", MAX_OPEN_FILE);
}