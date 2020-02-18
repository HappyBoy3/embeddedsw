/******************************************************************************
*
* Copyright (C) 2019 - 2020 Xilinx, Inc. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*
* 
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file xplmi_task.c
*
* This file contains code Task handling.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00  kc   02/06/2019 Initial release
* 1.01  ma   02/03/2020 Change XPlmi_MeasurePerfTime to retrieve Performance
*                       time and print
*
* </pre>
*
* @note
*
******************************************************************************/

/***************************** Include Files *********************************/
#include "xplmi_task.h"
#include "xplmi_debug.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
static struct metal_list TaskQueue[XPLMI_TASK_PRIORITIES];
static XPlmi_TaskNode Tasks[XPLMI_TASK_MAX];

/*****************************************************************************/

/*****************************************************************************/
/**
 * @brief This function creates the task and initializes its fields with
 * the user parameters
 *
 * @param	Priority Priority of the task
 * @param	handler function pointer to the task handler
 * @param	PrivData Private Data to be passed with task handler
 *
 * @return	Pointer to the task node structure
 *****************************************************************************/
XPlmi_TaskNode * XPlmi_TaskCreate(u32 Priority,
	int (*Handler)(void * PrivData), void * PrivData)
{
    XPlmi_TaskNode * Task=NULL;
    u32 Index;
    /** Assign free task node */
    for (Index = 0; Index < XPLMI_TASK_MAX; Index++) {
        Task = &Tasks[Index];
        if (Task->Handler == NULL) {break;}
    }
    if (Index >= XPLMI_TASK_MAX) {
	XPlmi_Printf(DEBUG_GENERAL, "Task create failed \n\r");
	Task = NULL;
	goto END;
    }
    if (Task == NULL) {
	goto END;
    }
    Task->Priority = Priority;
    Task->Delay = 0;
    metal_list_init(&Task->TaskNode);
    Task->Handler = Handler;
    Task->PrivData = PrivData;
END:
    return Task;
}

/*****************************************************************************/
/**
 * @brief This function deletes the task from the task queue
 *
 * @param	Task Pointer to the task node
 *
 * @return	None
 *****************************************************************************/
void XPlmi_TaskDelete(XPlmi_TaskNode * Task)
{
    if (!metal_list_is_empty(&Task->TaskNode)) {
        metal_list_del(&Task->TaskNode);
    }
    Task->Delay = 0;
    Task->Handler = NULL;
    Task->PrivData = NULL;
}

/*****************************************************************************/
/**
 * @brief This function adds the task to the task queue so that it
 * can be triggered based on its priority
 *
 * @param	Task Pointer to the task node
 *
 * @return	None
 *****************************************************************************/
void XPlmi_TaskTriggerNow(XPlmi_TaskNode * Task)
{
    Xil_AssertVoid(Task->Handler != NULL);
    if (metal_list_is_empty(&Task->TaskNode)) {
        metal_list_add_tail(&TaskQueue[Task->Priority], &Task->TaskNode);
    }
}

/*****************************************************************************/
/**
 * @brief This function initializes the task queues list
 *
 * @param	None
 *
 * @return	None
 *****************************************************************************/
void XPlmi_TaskInit(void )
{
	u32 Index;

	XPlmi_Printf(DEBUG_DETAILED, "%s\n\r", __func__);

	/* Initialize the list pointers */
	for (Index = 0; Index < XPLMI_TASK_PRIORITIES; Index++)
	{
		metal_list_init(&TaskQueue[Index]);
	}
}

/*****************************************************************************/
/**
 * @brief This function will be checking for tasks in the queue based on the
 * priority. After calling every task handlers, next high priority task will
 * be called.
 *
 * @param	None
 *
 * @return	None
 *****************************************************************************/
void XPlmi_TaskDispatchLoop(void )
{
	int Status = XST_FAILURE;
	struct metal_list *Node[XPLMI_TASK_PRIORITIES];
	XPlmi_TaskNode *Task;
	u32 Index;
#ifdef PLM_DEBUG_INFO
	u64 TaskStartTime;
	XPlmi_PerfTime tPerfTime = {0U};
#endif

	XPlmi_Printf(DEBUG_DETAILED, "%s\n\r", __func__);
	for (Index=0U; Index<XPLMI_TASK_PRIORITIES; Index++)
	{
		Node[Index] = &TaskQueue[Index];
	}

	while (1)
	{
		Task = NULL;
		/** Priority based task handling */
		for (Index=0U; Index<XPLMI_TASK_PRIORITIES; Index++)
		{
			/** if no pending tasks are present, go to sleep */
			if (metal_list_is_empty(&TaskQueue[Index]))
			{
				XPlmi_Printf(DEBUG_DETAILED,
				     "No pending tasks in Priority%d Queue\n\r",
				     Index);
				continue;
			} else {
				/** Skip the first element as it
				 * is not proper task */
				if ((Node[Index] == &TaskQueue[Index])) {
					Node[Index] = (TaskQueue[Index].next);
				}
				/* Get the next task in round robin */
				Task = metal_container_of(Node[Index],
					XPlmi_TaskNode, TaskNode);
				Node[Index] = Node[Index]->next;
				break;
			}
		}

		if (Task != NULL)
		{
#ifdef PLM_DEBUG_INFO
			/** Call the task handler */
			TaskStartTime = XPlmi_GetTimerValue();
#endif
			Xil_AssertVoid(Task->Handler != NULL);
			Status = Task->Handler(Task->PrivData);
#ifdef PLM_DEBUG_INFO
			XPlmi_MeasurePerfTime(TaskStartTime, &tPerfTime);
			XPlmi_Printf(DEBUG_PRINT_PERF, "%u.%u ms: Task Time\n\r",
					(u32)tPerfTime.tPerfMs, (u32)tPerfTime.tPerfMsFrac);
#endif
			if (Status != XPLMI_TASK_INPROGRESS)
			{
				/** delete the task that is handled */
				XPlmi_TaskDelete(Task);
			}
			if ((Status != XST_SUCCESS) &&
			    (Status != XPLMI_TASK_INPROGRESS))
			{
				/** TODO Call the error manager */
				XPlmi_ErrMgr(Status);
			}
			continue;
		}
		/**
		 * Goto sleep when all queues are empty
		 */
		XPlmi_Printf(DEBUG_DETAILED,
			"No pending tasks..Going to sleep\n\r");
		mb_sleep();
	}
}
