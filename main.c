#define NEURON

#define START_NEURON 1
#define RESET_NEURON 2
#define SET_SPIKE 3
#define SET_TEST_SPIKE 4
#define CLEAR_BUFFERED_SPIKE 5


//AXI GPIO driver
#include "xgpio.h"

//send data over UART
#include "xil_printf.h"

//information about AXI peripherals
#include "xparameters.h"


#include "xllfifo.h"

#include "xllfifo_hw.h"

#include "xstream_test.h"
#include "xstream_test_hw.h"

#include "xneuron.h"
#include "xneuron_hw.h"

#include "xstatus.h"
#include "xuartlite.h"

#include "xuartlite_l.h"

/*******************************Endianness************************************
* Always use little endian. For example, a hex value x1234, when it is splitted
* into 4 bytes in an array, msb stores in array[3], lsb stores in array[0].
* When transmit through uart, send array[0] first.
* Endianess reference: https://www.embeddedrelated.com/showarticle/174.php
******************************************************************************/

// reference: https://stackoverflow.com/questions/3784263/converting-an-int-into-a-4-byte-char-array-c
// split a integer into bytes e.g.
// msb in byte[3], lsb in byte[0]
// reason is that byte[0] will be send first, to guarantee msb are sent first, store bytes in reverse order
// d'512 = b'1000000000
// split it into 4 bytes, it will be
// byte[3] = b00000000, byte[2] = b00000000, byte[1] = b00000010, byte[0] = b00000000
void uint2byte(u32 value, u8 *bytes)
{
	bytes[3] = (value >> 24) & 0xFF;
	bytes[2] = (value >> 16) & 0xFF;
	bytes[1] = (value >> 8) & 0xFF;
	bytes[0] = value & 0xFF;
}

//convert an array of u8 to u32
//assumes lsb in bytes[0], msb in bytes[3]
u32 byte2uint(u8 *bytes)
{
	u32 value = bytes[0] | (u32)bytes[1] << 8 | (u32)bytes[2] << 16 | (u32)bytes[3] << 24;

	return value;
}

unsigned int send_done()
{
	for (int i = 0; i < 4; i++)
		XUartLite_SendByte(XPAR_AXI_UARTLITE_0_BASEADDR, 255);
}

//field 0 for header, field 1-4 for data.
// allow to specify each byte
unsigned int send_discrete_packet(XUartLite *InstancePtr, u8 field0, u8 field1, u8 field2, u8 field3, u8 field4)
{
	unsigned int SentCount = 0;
	u8 SendBuffer[5] = {field0, field1, field2, field3, field4};
	SentCount = XUartLite_Send(InstancePtr, SendBuffer, 5);

	if (SentCount != 5)
		return XST_FAILURE;

	return SentCount;
}

//provide a u32 value, split it in 4 bytes and send it.
//first byte specify te type, reset 4 bytes are actual data
// uint2byte stores msb of data in data_byte[4], lsb in data_byte[1]
unsigned int send_packet(XUartLite *InstancePtr, u8 packet_type, u32 data)
{
	u8 data_byte [5];
	data_byte[0] = packet_type;
	uint2byte(data, data_byte+1);
	xil_printf("send:%d\n", data);

	for(int i = 0; i != 5; i++)
		xil_printf("data[%d]:%d" , i, data_byte[i]);

	xil_printf("\n");

	unsigned int SentCount = 0;
	SentCount = XUartLite_Send(InstancePtr, data_byte+1, 4);
	xil_printf("SentCount:%d\n" , SentCount);
	if (SentCount != 5)
		return XST_FAILURE;

	return SentCount;
}


int initfifo(XLlFifo *InstancePtr, u16 DeviceId){
  XLlFifo_Config *Config;
  int Status = XST_SUCCESS;

  // Initialize the driver
  Config = XLlFfio_LookupConfig(DeviceId);
  if (!Config) {
    xil_printf("No config found for %d\n", DeviceId);
    return XST_FAILURE;
  }

  Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
  if (Status != XST_SUCCESS) {
    xil_printf("Initialization failed\n");
    return Status;
  }

  // Check reset value
  Status = XLlFifo_Status(InstancePtr);
  XLlFifo_IntClear(InstancePtr,0xffffffff);
  Status = XLlFifo_Status(InstancePtr);
  if(Status != 0x0) {
    xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
               "Expected : 0x0 \n",
               XLlFifo_Status(InstancePtr));
    return XST_FAILURE;
  }
  return Status;
}

int read_fifo(XLlFifo *InstancePtr)
{
	int i;

	static u32 ReceiveLength;
	ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/4;
	for ( i=0; i < ReceiveLength; i++)
	{
		u32 RxWord = XLlFifo_RxGetWord(InstancePtr);

		if(XLlFifo_iRxOccupancy(InstancePtr))
		{
		  RxWord = XLlFifo_RxGetWord(InstancePtr);
		}
		xil_printf("%d,", RxWord);
	}
}

int recv (XLlFifo *InstancePtr){
  int i;

  static u32 ReceiveLength;

  xil_printf(" Receiving data ....\n");

  // Read Recv length
  ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/4;
  xil_printf("Recv length %d \n", ReceiveLength);

  // Start receiving
  for ( i=0; i < ReceiveLength; i++){
    u32 RxWord = XLlFifo_RxGetWord(InstancePtr);

    if(XLlFifo_iRxOccupancy(InstancePtr)){
      RxWord = XLlFifo_RxGetWord(InstancePtr);
    }
    xil_printf("receive %d,", RxWord);

//    int a = RxWord >> 8;
//
//    u32 b = RxWord >> 8;
//
//    xil_printf("shift : %d, %u, %d, ", a, b, b);
  }



  // check the cr bit of isr to see if can generate correct signal
  //
//  while(XLlFifo_IsRxDone(InstancePtr) != TRUE)
//  {
//	  	int len = (XLlFifo_iRxGetLen(InstancePtr))/4;
//	  	xil_printf("length %d\n", len);
//
//	  	u32 RxWord = XLlFifo_RxGetWord(InstancePtr);
//	    if(XLlFifo_iRxOccupancy(InstancePtr)){
//	      RxWord = XLlFifo_RxGetWord(InstancePtr);
//	    }
//	    xil_printf("receive %d,", RxWord);
//  }


// comment out, don't check rc bit(receive complete)
// Although all packets can be received successfully, rc bit is not correct,
// reason is not clear, so ignore it
// Status = XLlFifo_IsRxDone(InstancePtr);
// if(Status != TRUE){
//   xil_printf("Failing in receive complete ... \n");
//   return -1;
// }

  return ReceiveLength;
}


#ifdef NEURON
int main()
{
	int status;

	//xil_printf("################## start of program ######################\n");
	XGpio gpio;
	u32 btn, led;

	//test uart

	//xil_printf("-----------------uart test--------------------\n");

	/********************************init uart*************************************/
	int init_uart_result = 1;
	int uart_selftest_result = 1;
	XUartLite UartLite;
	status = XUartLite_Initialize(&UartLite, XPAR_AXI_UARTLITE_0_DEVICE_ID);
	if (status != XST_SUCCESS)
		init_uart_result = 0;

	xil_printf("uart init:%d\n", init_uart_result);


	status = XUartLite_SelfTest(&UartLite);

	if (status != XST_SUCCESS)
		uart_selftest_result = 0;
	xil_printf("uart selftest:%d\n", uart_selftest_result);

	/************************initialize stream fifo of neuron**********************/
	int init_neuron_test_fifo_result = 1;

	XLlFifo neuron_test_fifo;
	//initialize stream fifo
	status = initfifo(&neuron_test_fifo, XPAR_NEURON_TEST_FIFO_DEVICE_ID);
	if (status != XST_SUCCESS)
		init_neuron_test_fifo_result = 0;

	xil_printf("neuron test fifo init:%d\n", init_neuron_test_fifo_result);

	//init psp fifo
	int init_psp_fifo = 1;
	XLlFifo psp_fifo;
	//initialize stream fifo
	status = initfifo(&psp_fifo, XPAR_PSP_FIFO_DEVICE_ID);
	if (status != XST_SUCCESS)
		init_psp_fifo = 0;
	xil_printf("psp fifo init:%d\n", init_psp_fifo);

	//init voltage fifo
	int init_voltage_fifo = 1;
	XLlFifo voltage_fifo;
	//initialize stream fifo
	status = initfifo(&voltage_fifo, XPAR_VOLTAGE_FIFO_DEVICE_ID);
	if (status != XST_SUCCESS)
		init_voltage_fifo = 0;
	xil_printf("voltage fifo init:%d\n", init_voltage_fifo);

	//initialize neuron
	int init_neuron_result = 1;
	XNeuron neuron_inst;
	int neuron_init_status = XNeuron_Initialize(&neuron_inst, XPAR_NEURON_0_DEVICE_ID);
	if (neuron_init_status != XST_SUCCESS)
		init_neuron_result = 0;
	xil_printf("neuron init: %d\n", init_neuron_result);

	u64 spike_63_0 = 0;
	u64 spike_127_64 = 0;
	u64 one = 1;

	send_done();

	//main loop
	u8 recv_data[4] = {0};

	while(1)
	{
		unsigned int sentcount = 0;

		int action = 0;

		while(1)
		{
			//XUartLite_RecvByte is a blocking function, it blocks until receive a word from uart
			for (int i = 0; i < 4; i++)
				recv_data[i] = XUartLite_RecvByte(XPAR_AXI_UARTLITE_0_BASEADDR);

			//xil_printf("receive:%d,%d,%d,%d\n",recv_data[0],recv_data[1],recv_data[2],recv_data[3]);

			if (recv_data[3] == START_NEURON)
			{
				action = START_NEURON;
				break;
			}
			else if(recv_data[3] == RESET_NEURON)
			{
				action = RESET_NEURON;
				break;
			}
			else if(recv_data[3] == SET_SPIKE)
			{
				action = SET_SPIKE;
				break;
			}
			else if (recv_data[3] == SET_TEST_SPIKE)
			{
				action = SET_TEST_SPIKE;
				break;
			}
			else if(recv_data[3] == CLEAR_BUFFERED_SPIKE)
			{
				action = CLEAR_BUFFERED_SPIKE;
				break;
			}
			else
			{
				xil_printf("loopback\n");
				u8 sendbuf[4];
				int sendcnt = 0;

				for (int i = 0; i < 4; i++)
					sendbuf[i] = i;

				for (int i = 0; i < 4; i++)
					XUartLite_SendByte(XPAR_AXI_UARTLITE_0_BASEADDR, 15);
			}
		}

		if (action == START_NEURON)
		{

			int neuron_done =  XNeuron_IsDone(&neuron_inst);
			int neuron_idle =  XNeuron_IsIdle(&neuron_inst);
			int neuron_ready =  XNeuron_IsReady(&neuron_inst);

//			xil_printf("done: %d,idle:%d,ready:%d\n", neuron_done, neuron_idle, neuron_ready);

			//xil_printf("check input spike before set\n %d \n",spike_63_0);
			XNeuron_Set_input_spike_63_0_V(&neuron_inst, spike_63_0);
			XNeuron_Set_input_spike_127_64_V(&neuron_inst, spike_127_64);

//			spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
			//xil_printf("spike buffer 63-0: %u, %u \n",(u32)(spike_63_0>>32), spike_63_0);
//			spike_127_64 = XNeuron_Get_input_spike_127_64_V(&neuron_inst);
//			xil_printf("spike buffer:%u,%u,%u,%u\n",(u32)(spike_127_64>>32), spike_127_64, (u32)(spike_63_0>>32), spike_63_0);

			//start neuron
//			xil_printf("start\n");
			XNeuron_Start(&neuron_inst);

			//check neuron status
			MB_Sleep(1);
			neuron_done =  XNeuron_IsDone(&neuron_inst);
			neuron_idle =  XNeuron_IsIdle(&neuron_inst);
			neuron_ready =  XNeuron_IsReady(&neuron_inst);

//			xil_printf("done:%d,idle:%d,ready:%d\n", neuron_done, neuron_idle, neuron_ready);

//			xil_printf("psp:");
			//recv(&psp_fifo);
			read_fifo(&psp_fifo);

			xil_printf("\n");
//			xil_printf("voltage:");

			//recv(&voltage_fifo);
			read_fifo(&voltage_fifo);

			//clear input after run one step
			XNeuron_Set_input_spike_63_0_V(&neuron_inst, 0);
			XNeuron_Set_input_spike_127_64_V(&neuron_inst, 0);
			spike_63_0 = 0;
			spike_127_64 = 0;

			send_done();

		}
		else if (action == RESET_NEURON)
		{
//			xil_printf("reset\n");
			XNeuron_Set_reset_neuron(&neuron_inst, 1);
			XNeuron_Start(&neuron_inst);
			XNeuron_Set_reset_neuron(&neuron_inst, 0);
		}
		else if(action == SET_TEST_SPIKE)
		{
			XNeuron_Set_input_spike_63_0_V(&neuron_inst, 65535);
//			xil_printf("set test spike\n");
			spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
//			xil_printf("check input spike:%d\n",spike_63_0);
		}
		else if(action == SET_SPIKE)
		{
			//reference: https://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit

			if(recv_data[2] < 64)
				spike_63_0 |= one << recv_data[2];

			else
				spike_127_64 |= one << (recv_data[2] - 64);

//			xil_printf("input spike 63-0:%u,%u\n",(u32)(spike_63_0>>32), spike_63_0);
//			xil_printf("input spike 127-64:%u,%u\n",(u32)(spike_127_64>>32), spike_127_64);

		}
		else if (action == CLEAR_BUFFERED_SPIKE)
		{
			spike_63_0 = 0;
			spike_127_64 = 0;
		}
		//memset(recv_data, 0, sizeof(recv_data));
	}

}

#else

int main()
{
	int status;

	xil_printf("################## start of program ######################\n");
	XGpio gpio;
	u32 btn, led;

	//test uart

	xil_printf("-----------------uart test--------------------\n");

	//init uart
	XUartLite UartLite;
	status = XUartLite_Initialize(&UartLite, XPAR_AXI_UARTLITE_0_DEVICE_ID);
	if (status != XST_SUCCESS)
		xil_printf("failed to init uart \n");

	status = XUartLite_SelfTest(&UartLite);

	if (status != XST_SUCCESS)
		xil_printf("uart self test failed \n");

	u32 testvalue = 18760;
	//test send_packet function
	unsigned int sentcount = send_packet(&UartLite, 1, testvalue);

	xil_printf("\n");
	// initialize stream fifo of stream_test
	// stream fifo instance
	XLlFifo FifoInstance;
	//initialize stream fifo
	xil_printf("----------initialize stream_fifo of stream_test module---------\n");
	status = initfifo(&FifoInstance, XPAR_STREAM_TEST_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
		xil_printf("initialize stream_test fifo: successful\n");

	xil_printf("\n");

	// verify stream_test functionality
	xil_printf("-------------------verify stream_test----------------------\n");
	XStream_test stream_test;
	int stream_test_init_status = XStream_test_Initialize(&stream_test,XPAR_STREAM_TEST_0_DEVICE_ID);
	XStream_test_Config* stream_test_cfg = XStream_test_LookupConfig(XPAR_STREAM_TEST_0_DEVICE_ID);
	int stream_test_cfginit_status = XStream_test_CfgInitialize(&stream_test, stream_test_cfg);

	if (stream_test_cfginit_status == XST_SUCCESS)
		xil_printf("initialize stream_test cfg: successful\n");
	else
		xil_printf("initialize stream_test cfg: failed\n");

	if (stream_test_init_status == XST_SUCCESS)
		xil_printf("initialize stream_test: successful\n");
	else
		xil_printf("initialize stream_test: failed\n");

	//disable restart
	xil_printf("disable stream_test autostart\n");
	XStream_test_DisableAutoRestart(&stream_test);

	//set input value of stream_test
	int stream_test_a = 5;
	xil_printf("set input a: %d\n", stream_test_a);
	XStream_test_Set_a_V(&stream_test, stream_test_a);

	int stream_test_b = 3;
	xil_printf("set input b: %d\n", stream_test_b);
	XStream_test_Set_b_V(&stream_test, stream_test_b);

	//check if input is set correctly
	xil_printf("check if a and b are set correctly \n");
	u32 val_a =  XStream_test_Get_a_V(&stream_test);
	u32 val_b = XStream_test_Get_b_V(&stream_test);
	xil_printf("value a: %d, value b: %d \n", val_a, val_b);

	//start stream_test
	xil_printf("start stream_test\n", val_a, val_b);
	XStream_test_Start(&stream_test);

	//get result from stream_test
	u32 ret = XStream_test_Get_return(&stream_test);
	xil_printf("get return value of stream_test\n");
	xil_printf("return %d \n", ret);

	//check the status of stream_test
	xil_printf("check the status of stream_test \n");
	u32 stream_test_done = XStream_test_IsDone(&stream_test);
	u32 stream_test_idle = XStream_test_IsIdle(&stream_test);
	u32 stream_test_ready = XStream_test_IsReady(&stream_test);

	xil_printf("stream test done: %d, idle: %d, ready: %d\n", stream_test_done, stream_test_idle, stream_test_ready);

	//read stream fifo of stream_test

	xil_printf("read stream_test fifo\n");
	recv(&FifoInstance);

	xil_printf("\n\n");


	xil_printf("-----------------------verify neuron functionality---------------------\n");
	//initialize stream fifo of neuron
	XLlFifo neuron_test_fifo;
	//initialize stream fifo
	status = initfifo(&neuron_test_fifo, XPAR_NEURON_TEST_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize neuron_test fifo: successful\n");

	//init psp fifo
	XLlFifo psp_fifo;
	//initialize stream fifo
	status = initfifo(&psp_fifo, XPAR_PSP_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize psp fifo: successful\n");


	//init voltage fifo
	XLlFifo voltage_fifo;
	//initialize stream fifo
	status = initfifo(&voltage_fifo, XPAR_VOLTAGE_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize psp fifo: successful\n");

	// test neuron module
	xil_printf("initialize neuron\n");

	//initialize neuron
	XNeuron neuron_inst;
	int neuron_init_status = XNeuron_Initialize(&neuron_inst, XPAR_NEURON_0_DEVICE_ID);
	if (neuron_init_status != XST_SUCCESS)
		xil_printf("initialize neuron: failed \n");
	else
		xil_printf("initialize neuron: successful \n");

	//initialize neuron cfg
//	XNeuron_Config* neuron_cfg_pointer = XNeuron_LookupConfig(XPAR_NEURON_0_DEVICE_ID);
//	int neuron_cfg_init_status = XNeuron_CfgInitialize(&neuron_inst, neuron_cfg_pointer);
//	if (neuron_cfg_init_status != XST_SUCCESS)
//		xil_printf("initialize neuron cfg failed \n");
//	else
//		xil_printf("initialize neuron cfg successfully \n");

	XNeuron_DisableAutoRestart(&neuron_inst);

	int neuron_done =  XNeuron_IsDone(&neuron_inst);
	int neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	int neuron_ready =  XNeuron_IsReady(&neuron_inst);
	xil_printf("check neuron status\n");
	xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);

	//test neuron
	//set input
	XNeuron_Set_test_var(&neuron_inst, 1);

	//check if input set successfully
	int test_var =  XNeuron_Get_test_var(&neuron_inst);
	xil_printf("test_var: %d \n", test_var);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 65535);

	u64 spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
	xil_printf("check input spike\n %d \n",spike_63_0);

	//start neuron
	xil_printf("start neuron \n");
	XNeuron_Start(&neuron_inst);

	//check neuron status
	sleep(1);
	neuron_done =  XNeuron_IsDone(&neuron_inst);
	neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	neuron_ready =  XNeuron_IsReady(&neuron_inst);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 0);

	xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);
	//sleep

	//xil_printf("read neuron test fifo \n");
	//read neuron test fifo
	//recv(&neuron_test_fifo);

	xil_printf("\n\n");

	xil_printf("read psp\n");
	recv(&psp_fifo);

	xil_printf("\n");
	xil_printf("read voltage\n");

	recv(&voltage_fifo);

	for (int i = 0; i != 5; i++)
	{
		xil_printf("round %d\n", i);

		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);

		XNeuron_Set_test_var(&neuron_inst, 5);
		test_var =  XNeuron_Get_test_var(&neuron_inst);
		xil_printf("test_var: %d \n", test_var);
		xil_printf("start neuron \n");
		XNeuron_Start(&neuron_inst);

		sleep(1);
		xil_printf("\n");
		//xil_printf("read test\n");
		//recv(&neuron_test_fifo);

		xil_printf("\n");
		xil_printf("read psp\n");
		recv(&psp_fifo);

		xil_printf("\n");
		xil_printf("read voltage\n");
		recv(&voltage_fifo);

		xil_printf("\n");
		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);
	}

	xil_printf("\n-----------------------test reset psp---------------------\n");
	XNeuron_Set_reset_neuron(&neuron_inst, 1);
	XNeuron_Start(&neuron_inst);

	XNeuron_Set_reset_neuron(&neuron_inst, 0);

	XNeuron_Set_test_var(&neuron_inst, 1);

	//check if input set successfully
	test_var =  XNeuron_Get_test_var(&neuron_inst);
	xil_printf("test_var: %d \n", test_var);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 65535);

	spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
	xil_printf("check input spike\n %d \n",spike_63_0);

	//start neuron
	xil_printf("start neuron \n");
	XNeuron_Start(&neuron_inst);

	//check neuron status
	sleep(1);
	neuron_done =  XNeuron_IsDone(&neuron_inst);
	neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	neuron_ready =  XNeuron_IsReady(&neuron_inst);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 0);

	xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);
	//sleep

	//xil_printf("read neuron test fifo \n");
	//read neuron test fifo
	//recv(&neuron_test_fifo);

	xil_printf("\n\n");

	xil_printf("read psp\n");
	recv(&psp_fifo);

	xil_printf("\n");
	xil_printf("read voltage\n");

	recv(&voltage_fifo);


	for (int i = 0; i != 5; i++)
	{
		xil_printf("round %d\n", i);

		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);

		XNeuron_Set_test_var(&neuron_inst, 5);
		test_var =  XNeuron_Get_test_var(&neuron_inst);
		xil_printf("test_var: %d \n", test_var);
		xil_printf("start neuron \n");
		XNeuron_Start(&neuron_inst);

		sleep(2);
		xil_printf("\n");
		//xil_printf("read test\n");
		//recv(&neuron_test_fifo);

		xil_printf("\n");
		xil_printf("read psp\n");
		recv(&psp_fifo);

		xil_printf("\n");
		xil_printf("read voltage\n");
		recv(&voltage_fifo);

		xil_printf("\n");
		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \n", neuron_done, neuron_idle, neuron_ready);
	}


	xil_printf("end program \n");
}
#endif
