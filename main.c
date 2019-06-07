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


int initfifo(XLlFifo *InstancePtr, u16 DeviceId){
  XLlFifo_Config *Config;
  int Status = XST_SUCCESS;

  // Initialize the driver
  Config = XLlFfio_LookupConfig(DeviceId);
  if (!Config) {
    xil_printf("No config found for %d \r\n", DeviceId);
    return XST_FAILURE;
  }

  Status = XLlFifo_CfgInitialize(InstancePtr, Config, Config->BaseAddress);
  if (Status != XST_SUCCESS) {
    xil_printf("Initialization failed \r\n");
    return Status;
  }

  // Check reset value
  Status = XLlFifo_Status(InstancePtr);
  XLlFifo_IntClear(InstancePtr,0xffffffff);
  Status = XLlFifo_Status(InstancePtr);
  if(Status != 0x0) {
    xil_printf("\n ERROR : Reset value of ISR0 : 0x%x\t"
               "Expected : 0x0 \r\n",
               XLlFifo_Status(InstancePtr));
    return XST_FAILURE;
  }
  return Status;
}

int recv (XLlFifo *InstancePtr){
  int i;
  int Status;
  static u32 ReceiveLength;

  xil_printf(" Receiving data ....\r\n");

  // Read Recv length
  ReceiveLength = (XLlFifo_iRxGetLen(InstancePtr))/4;
  xil_printf("Recv length %d \r\n", ReceiveLength);

  // Start receiving
  for ( i=0; i < ReceiveLength; i++){
    u32 RxWord = XLlFifo_RxGetWord(InstancePtr);

    if(XLlFifo_iRxOccupancy(InstancePtr)){
      RxWord = XLlFifo_RxGetWord(InstancePtr);
    }
    xil_printf("receive %d,", RxWord);
  }



  // check the cr bit of isr to see if can generate correct signal
  //
//  while(XLlFifo_IsRxDone(InstancePtr) != TRUE)
//  {
//	  	int len = (XLlFifo_iRxGetLen(InstancePtr))/4;
//	  	xil_printf("length %d\r\n", len);
//
//	  	u32 RxWord = XLlFifo_RxGetWord(InstancePtr);
//	    if(XLlFifo_iRxOccupancy(InstancePtr)){
//	      RxWord = XLlFifo_RxGetWord(InstancePtr);
//	    }
//	    xil_printf("receive %d,", RxWord);
//  }


  //comment out, don't check rc bit(receive complete)
  //althoug all packet can be received successfully, rc bit is not correct,
  // reason is not clear, so ignore it
//  Status = XLlFifo_IsRxDone(InstancePtr);
//  if(Status != TRUE){
//    xil_printf("Failing in receive complete ... \r\n");
//    return -1;
//  }

  return ReceiveLength;
}

//int initmymodule()

int main()
{
	int status;

	xil_printf("################## start of program ######################\r\n");
	XGpio gpio;
	u32 btn, led;

	// initialize stream fifo of stream_test
	// stream fifo instance
	XLlFifo FifoInstance;
	//initialize stream fifo
	xil_printf("----------initialize stream_fifo of stream_test module---------\r\n");
	status = initfifo(&FifoInstance, XPAR_STREAM_TEST_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
		xil_printf("initialize stream_test fifo: successful\r\n");

	xil_printf("\r\n");

	// verify stream_test functionality
	xil_printf("-------------------verify stream_test----------------------\r\n");
	XStream_test stream_test;
	int stream_test_init_status = XStream_test_Initialize(&stream_test,XPAR_STREAM_TEST_0_DEVICE_ID);
	XStream_test_Config* stream_test_cfg = XStream_test_LookupConfig(XPAR_STREAM_TEST_0_DEVICE_ID);
	int stream_test_cfginit_status = XStream_test_CfgInitialize(&stream_test, stream_test_cfg);

	if (stream_test_cfginit_status == XST_SUCCESS)
		xil_printf("initialize stream_test cfg: successful\r\n");
	else
		xil_printf("initialize stream_test cfg: failed\r\n");

	if (stream_test_init_status == XST_SUCCESS)
		xil_printf("initialize stream_test: successful\r\n");
	else
		xil_printf("initialize stream_test: failed\r\n");

	//disable restart
	xil_printf("disable stream_test autostart\r\n");
	XStream_test_DisableAutoRestart(&stream_test);

	//set input value of stream_test
	int stream_test_a = 5;
	xil_printf("set input a: %d\r\n", stream_test_a);
	XStream_test_Set_a_V(&stream_test, stream_test_a);

	int stream_test_b = 3;
	xil_printf("set input b: %d\r\n", stream_test_b);
	XStream_test_Set_b_V(&stream_test, stream_test_b);

	//check if input is set correctly
	xil_printf("check if a and b are set correctly \r\n");
	u32 val_a =  XStream_test_Get_a_V(&stream_test);
	u32 val_b = XStream_test_Get_b_V(&stream_test);
	xil_printf("value a: %d, value b: %d \r\n", val_a, val_b);

	//start stream_test
	xil_printf("start stream_test\r\n", val_a, val_b);
	XStream_test_Start(&stream_test);

	//get result from stream_test
	u32 ret = XStream_test_Get_return(&stream_test);
	xil_printf("get return value of stream_test\r\n");
	xil_printf("return %d \r\n", ret);

	//check the status of stream_test
	xil_printf("check the status of stream_test \r\n");
	u32 stream_test_done = XStream_test_IsDone(&stream_test);
	u32 stream_test_idle = XStream_test_IsIdle(&stream_test);
	u32 stream_test_ready = XStream_test_IsReady(&stream_test);

	xil_printf("stream test done: %d, idle: %d, ready: %d\r\n", stream_test_done, stream_test_idle, stream_test_ready);

	//read stream fifo of stream_test

	xil_printf("read stream_test fifo\r\n");
	recv(&FifoInstance);

	xil_printf("\r\n\r\n");


	xil_printf("-----------------------verify neuron functionality---------------------\r\n");
	//initialize stream fifo of neuron
	XLlFifo neuron_test_fifo;
	//initialize stream fifo
	status = initfifo(&neuron_test_fifo, XPAR_NEURON_TEST_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize neuron_test fifo: successful\r\n");

	//init psp fifo
	XLlFifo psp_fifo;
	//initialize stream fifo
	status = initfifo(&psp_fifo, XPAR_PSP_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize psp fifo: successful\r\n");


	//init voltage fifo
	XLlFifo voltage_fifo;
	//initialize stream fifo
	status = initfifo(&voltage_fifo, XPAR_VOLTAGE_FIFO_DEVICE_ID);
	if (status == XST_SUCCESS)
	xil_printf("initialize psp fifo: successful\r\n");

	// test neuron module
	xil_printf("initialize neuron\r\n");

	//initialize neuron
	XNeuron neuron_inst;
	int neuron_init_status = XNeuron_Initialize(&neuron_inst, XPAR_NEURON_0_DEVICE_ID);
	if (neuron_init_status != XST_SUCCESS)
		xil_printf("initialize neuron: failed \r\n");
	else
		xil_printf("initialize neuron: successful \r\n");

	//initialize neuron cfg
//	XNeuron_Config* neuron_cfg_pointer = XNeuron_LookupConfig(XPAR_NEURON_0_DEVICE_ID);
//	int neuron_cfg_init_status = XNeuron_CfgInitialize(&neuron_inst, neuron_cfg_pointer);
//	if (neuron_cfg_init_status != XST_SUCCESS)
//		xil_printf("initialize neuron cfg failed \r\n");
//	else
//		xil_printf("initialize neuron cfg successfully \r\n");

	XNeuron_DisableAutoRestart(&neuron_inst);

	int neuron_done =  XNeuron_IsDone(&neuron_inst);
	int neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	int neuron_ready =  XNeuron_IsReady(&neuron_inst);
	xil_printf("check neuron status\r\n");
	xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);

	//test neuron
	//set input
	XNeuron_Set_test_var(&neuron_inst, 1);

	//check if input set successfully
	int test_var =  XNeuron_Get_test_var(&neuron_inst);
	xil_printf("test_var: %d \r\n", test_var);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 65535);

	u64 spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
	xil_printf("check input spike\r\n %d \r\n",spike_63_0);

	//start neuron
	xil_printf("start neuron \r\n");
	XNeuron_Start(&neuron_inst);

	//check neuron status
	sleep(1);
	neuron_done =  XNeuron_IsDone(&neuron_inst);
	neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	neuron_ready =  XNeuron_IsReady(&neuron_inst);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 0);

	xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);
	//sleep

	xil_printf("read neuron test fifo \r\n");
	//read neuron test fifo
	recv(&neuron_test_fifo);

	xil_printf("\r\n\r\n");

	xil_printf("read psp\r\n");
	recv(&psp_fifo);

	xil_printf("\r\n");
	xil_printf("read voltage\r\n");

	recv(&voltage_fifo);

	for (int i = 0; i != 5; i++)
	{
		xil_printf("round %d\r\n", i);

		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);

		XNeuron_Set_test_var(&neuron_inst, 5);
		test_var =  XNeuron_Get_test_var(&neuron_inst);
		xil_printf("test_var: %d \r\n", test_var);
		xil_printf("start neuron \r\n");
		XNeuron_Start(&neuron_inst);

		sleep(2);
		xil_printf("\r\n");
		xil_printf("read test\r\n");
		recv(&neuron_test_fifo);

		xil_printf("\r\n");
		xil_printf("read psp\r\n");
		recv(&psp_fifo);

		xil_printf("\r\n");
		xil_printf("read voltage\r\n");
		recv(&voltage_fifo);

		xil_printf("\r\n");
		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);
	}

	xil_printf("\r\n-----------------------test reset psp---------------------\r\n");
	XNeuron_Set_reset_neuron(&neuron_inst, 1);
	XNeuron_Start(&neuron_inst);

	XNeuron_Set_reset_neuron(&neuron_inst, 0);

	XNeuron_Set_test_var(&neuron_inst, 1);

	//check if input set successfully
	test_var =  XNeuron_Get_test_var(&neuron_inst);
	xil_printf("test_var: %d \r\n", test_var);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 65535);

	spike_63_0 = XNeuron_Get_input_spike_63_0_V(&neuron_inst);
	xil_printf("check input spike\r\n %d \r\n",spike_63_0);

	//start neuron
	xil_printf("start neuron \r\n");
	XNeuron_Start(&neuron_inst);

	//check neuron status
	sleep(1);
	neuron_done =  XNeuron_IsDone(&neuron_inst);
	neuron_idle =  XNeuron_IsIdle(&neuron_inst);
	neuron_ready =  XNeuron_IsReady(&neuron_inst);

	XNeuron_Set_input_spike_63_0_V(&neuron_inst, 0);

	xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);
	//sleep

	xil_printf("read neuron test fifo \r\n");
	//read neuron test fifo
	recv(&neuron_test_fifo);

	xil_printf("\r\n\r\n");

	xil_printf("read psp\r\n");
	recv(&psp_fifo);

	xil_printf("\r\n");
	xil_printf("read voltage\r\n");

	recv(&voltage_fifo);


	for (int i = 0; i != 5; i++)
	{
		xil_printf("round %d\r\n", i);

		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);

		XNeuron_Set_test_var(&neuron_inst, 5);
		test_var =  XNeuron_Get_test_var(&neuron_inst);
		xil_printf("test_var: %d \r\n", test_var);
		xil_printf("start neuron \r\n");
		XNeuron_Start(&neuron_inst);

		sleep(2);
		xil_printf("\r\n");
		xil_printf("read test\r\n");
		recv(&neuron_test_fifo);

		xil_printf("\r\n");
		xil_printf("read psp\r\n");
		recv(&psp_fifo);

		xil_printf("\r\n");
		xil_printf("read voltage\r\n");
		recv(&voltage_fifo);

		xil_printf("\r\n");
		neuron_done =  XNeuron_IsDone(&neuron_inst);
		neuron_idle =  XNeuron_IsIdle(&neuron_inst);
		neuron_ready =  XNeuron_IsReady(&neuron_inst);
		xil_printf("done: %d, idle: %d, ready: %d \r\n", neuron_done, neuron_idle, neuron_ready);
	}

	xil_printf("end program \r\n");

/*
	XGpio_Initialize(&gpio, 0);

	XGpio_SetDataDirection(&gpio, 2, 0x00000000); // set LED GPIO channel tristates to All Output
	XGpio_SetDataDirection(&gpio, 1, 0xFFFFFFFF); // set BTN GPIO channel tristates to All Input
*/

//	while (1)
//	{
//		btn = XGpio_DiscreteRead(&gpio, 1);
//
//		if (btn != 0) // turn all LEDs on when any button is pressed
//			led = 0xFFFFFFFF;
//		else
//			led = 0x00000000;
//
//		XGpio_DiscreteWrite(&gpio, 2, led);
//
//		xil_printf("\rbutton state: %08x", btn);
//	}
}
